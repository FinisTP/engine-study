#include <QuestDraft.hpp>

#include <GameSession.hpp>
#include <Inventory.hpp>
#include <Progression.hpp>
#include <RpgContent.hpp>

#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <regex>

namespace umbra::authoring
{
    namespace
    {
        using core::Diagnostic;
        using core::Severity;

        void Err(std::vector<Diagnostic> &d, const std::string &code, const std::string &field, const std::string &message)
        {
            d.push_back({Severity::Error, code, "draft", field, message});
        }

        void Warn(std::vector<Diagnostic> &d, const std::string &code, const std::string &field, const std::string &message)
        {
            d.push_back({Severity::Warning, code, "draft", field, message});
        }

        // Model output is untrusted: every field is checked for type and size, nothing is cut silently.
        std::string Str(const Json &o, const char *key, const std::string &at, std::size_t max, std::vector<Diagnostic> &d,
                        bool required = true)
        {
            const std::string field = at + "." + key;
            if (!o.is_object() || !o.contains(key) || !o[key].is_string())
            {
                if (required)
                    Err(d, "draft.field", field, "missing text");
                return "";
            }
            std::string value = o[key].get<std::string>();
            if (value.size() > max)
                Err(d, "draft.limit", field, "longer than " + std::to_string(max) + " characters");
            if (required && value.empty())
                Err(d, "draft.field", field, "empty");
            return value;
        }

        int Int(const Json &o, const char *key, const std::string &at, int low, int high, int fallback, std::vector<Diagnostic> &d)
        {
            if (!o.is_object() || !o.contains(key))
                return fallback;
            if (!o[key].is_number_integer() || o[key].get<int>() < low || o[key].get<int>() > high)
            {
                Err(d, "draft.limit", at + "." + key, "must be a whole number from " + std::to_string(low) + " to " + std::to_string(high));
                return fallback;
            }
            return o[key].get<int>();
        }

        const Json &Array(const Json &o, const char *key, const std::string &at, std::size_t max, std::vector<Diagnostic> &d)
        {
            static const Json empty = Json::array();
            if (!o.is_object() || !o.contains(key) || !o[key].is_array())
            {
                Err(d, "draft.field", at + "." + key, "missing list");
                return empty;
            }
            if (o[key].size() > max)
                Err(d, "draft.limit", at + "." + key, "more than " + std::to_string(max) + " entries");
            return o[key];
        }

        // "Wolves at the Well!" -> "wolves_at_the_well"
        std::string MakeId(const std::string &text)
        {
            std::string id;
            for (char c : text)
            {
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                    id += c;
                else if (!id.empty() && id.back() != '_')
                    id += '_';
            }
            while (!id.empty() && id.back() == '_')
                id.pop_back();
            return id.substr(0, 32);
        }

        bool HasId(const Json &rpg, const char *section, const std::string &id)
        {
            if (!rpg.contains(section))
                return false;
            for (const Json &entry : rpg[section])
                if (entry.value("id", "") == id)
                    return true;
            return false;
        }

        std::string FreeId(const Json &rpg, const char *section, const std::string &wanted)
        {
            std::string id = wanted;
            for (int n = 2; HasId(rpg, section, id); ++n)
                id = wanted + "_" + std::to_string(n);
            return id;
        }

        std::string NameOf(const Json &rpg, const char *section, const std::string &id)
        {
            if (rpg.contains(section))
                for (const Json &entry : rpg[section])
                    if (entry.value("id", "") == id)
                        return entry.value("name", id);
            return id;
        }

        // --- Play-through: the real GameSession, replayed from a fresh game for every attempt ---

        struct Step final
        {
            bool fight = false;               // win a fight against actor `id`
            std::string id;                   // actor, or conversation to talk to
            std::vector<std::size_t> choices; // picked in order (talk)
        };

        bool Run(game::GameSession &s, const rpg::Content &c, const Step &step)
        {
            if (step.fight)
            {
                // 1. Assume the player wins (difficulty is not this check's job).
                std::optional<rpg::Character> enemy = rpg::MakeCharacter(c, step.id, step.id + "_draft");
                if (!enemy)
                    return false;
                // 2. Loot and quest events exactly as GameSession::CheckFightEnd does them.
                rpg::AwardVictory(c, s.Hero(), {&*enemy});
                s.Quests().Notify({story::GameEvent::Kind::Defeated, step.id}, s.Hero());
                s.Quests().Notify({story::GameEvent::Kind::ItemsChanged, ""}, s.Hero());
                return true;
            }
            if (!s.Talk(step.id))
                return false;
            for (std::size_t choice : step.choices)
                if (s.Choose(choice) != story::ChoiceResult::Done)
                    return false;
            s.EndTalk();
            return true;
        }

        std::unique_ptr<game::GameSession> Replay(const rpg::Content &c, const std::vector<Step> &script)
        {
            auto s = std::make_unique<game::GameSession>(c, 1);
            s->NewGame();
            for (const Step &step : script)
                if (!Run(*s, c, step))
                    return nullptr;
            return s;
        }

        using Goal = std::function<bool(game::GameSession &)>;

        // Breadth-first over the player's choices in one conversation, up to 8 deep.
        std::optional<std::vector<std::size_t>> FindPath(const rpg::Content &c, const std::vector<Step> &script,
                                                         const std::string &conversation, const Goal &goal)
        {
            std::deque<std::vector<std::size_t>> open{{}};
            for (int tries = 0; !open.empty() && tries < 4000; ++tries)
            {
                std::vector<std::size_t> path = open.front();
                open.pop_front();
                std::unique_ptr<game::GameSession> s = Replay(c, script);
                if (!s || !s->Talk(conversation))
                    return std::nullopt;
                bool ok = true;
                for (std::size_t choice : path)
                    ok = ok && s->CurrentConversation() && s->Choose(choice) == story::ChoiceResult::Done;
                if (!ok)
                    continue;
                if (goal(*s))
                    return path;
                story::Conversation *talk = s->CurrentConversation();
                if (talk == nullptr || path.size() >= 8)
                    continue;
                for (std::size_t choice : talk->VisibleChoices(s->Quests(), s->Hero()))
                {
                    open.push_back(path);
                    open.back().push_back(choice);
                }
            }
            return std::nullopt;
        }

        void Play(const Json &result, DraftPlan &plan, const std::string &npc, const WorldFacts &world, std::vector<Diagnostic> &d)
        {
            rpg::Content c;
            std::vector<Diagnostic> ignored;
            if (!rpg::ParseContent(result.dump(), "draft", c, ignored))
                return; // already reported by the validator
            if (c.FindActor("hero") == nullptr)
                return Err(d, "draft.play.hero", "", "the content has no \"hero\" actor to play the quest with");
            const rpg::QuestDef &quest = *c.FindQuest(plan.quest);

            auto fail = [&](const std::string &code, const std::string &line) {
                plan.play.push_back("FAIL " + line);
                Err(d, code, "", line);
            };
            auto state = [&](game::GameSession &s) { return s.Quests().GetState(plan.quest); };
            std::vector<Step> script;

            // 1. Some choice in the NPC's conversation starts the quest.
            const auto start = FindPath(c, script, plan.conversation, [&](game::GameSession &s) { return state(s) != rpg::QuestState::NotStarted; });
            if (!start)
                return fail("draft.play.start", "No choice in conversation '" + plan.conversation +
                                                    "' starts the quest (needs requires questNotStarted + action startQuest).");
            script.push_back({false, plan.conversation, *start});
            plan.play.push_back("ok   Talking to " + npc + " can start the quest.");

            // 2. Every objective, in order, can be completed.
            for (std::size_t i = 0; i < quest.objectives.size(); ++i)
            {
                const rpg::ObjectiveDef &objective = quest.objectives[i];
                const std::string step = "Objective " + std::to_string(i + 1) + " (" + objective.target + ")";
                const Goal done = [&, i](game::GameSession &s) {
                    const story::QuestProgress *p = s.Quests().Get(plan.quest);
                    return state(s) == rpg::QuestState::Completed || (p != nullptr && p->objective > i);
                };
                auto finished = [&] {
                    std::unique_ptr<game::GameSession> s = Replay(c, script);
                    return s && done(*s);
                };
                auto talkPath = [&](const std::string &onlyNpc) {
                    for (const auto &[id, conversation] : c.conversations)
                        if (onlyNpc.empty() || conversation.npc == onlyNpc)
                            if (const auto path = FindPath(c, script, id, done))
                            {
                                script.push_back({false, id, *path});
                                return true;
                            }
                    return false;
                };

                if (finished())
                {
                    plan.play.push_back("ok   " + step + " is already done at that point.");
                    continue;
                }
                if (objective.kind == rpg::ObjectiveKind::Defeat)
                {
                    if (!world.spawned.contains(objective.target))
                        Warn(d, "draft.play.spawn", "", "No scene spawns '" + objective.target + "' yet: add it to a spawner pool.");
                    for (int n = 0; n < objective.count; ++n)
                        script.push_back({true, objective.target, {}});
                }
                else if (objective.kind == rpg::ObjectiveKind::Obtain)
                {
                    // Loot from an enemy that carries it, else a conversation that gives it.
                    for (const auto &[id, actor] : c.actors)
                        for (const rpg::ItemCount &carried : actor.inventory)
                            if (carried.item == objective.target && id != "hero" && !finished())
                            {
                                if (!world.spawned.contains(id))
                                    Warn(d, "draft.play.spawn", "", "'" + objective.target + "' is carried by '" + id + "', which no scene spawns yet.");
                                for (int n = 0; n < 20 && !finished(); ++n)
                                    script.push_back({true, id, {}});
                            }
                    if (!finished())
                        talkPath("");
                }
                else
                    talkPath(objective.target);

                if (!finished())
                    return fail("draft.play.objective", step + " can't be completed: " +
                                                            (objective.kind == rpg::ObjectiveKind::Talk
                                                                 ? "no choice with action talk is shown while the quest is active."
                                                                 : objective.kind == rpg::ObjectiveKind::Obtain
                                                                       ? "nothing in the game gives this item."
                                                                       : "the enemy can't be defeated."));
                plan.play.push_back("ok   " + step + " can be completed.");
            }

            // 3. The quest ends completed and pays its reward once.
            std::unique_ptr<game::GameSession> s = Replay(c, script);
            const story::QuestProgress *p = s ? s->Quests().Get(plan.quest) : nullptr;
            if (p == nullptr || p->state != rpg::QuestState::Completed || !p->rewarded)
                return fail("draft.play.complete", "The quest does not end completed.");
            plan.play.push_back("ok   The quest completes and pays its reward.");
        }

        // --- Draft -> content JSON ---

        Json Requirement(const Json &r, const std::string &at, const std::string &quest, std::vector<Diagnostic> &d)
        {
            const std::string kind = Str(r, "kind", at, 32, d);
            if (kind == "questNotStarted" || kind == "questActive" || kind == "questCompleted")
            {
                const std::string state = kind == "questNotStarted" ? "notStarted" : kind == "questActive" ? "active" : "completed";
                return {{"quest", quest}, {"state", state}};
            }
            if (kind == "hasItem")
                return {{"hasItem", Str(r, "item", at, 64, d)}, {"count", Int(r, "count", at, 1, 20, 1, d)}};
            Err(d, "draft.kind", at + ".kind", "unknown requirement '" + kind + "'");
            return nullptr;
        }

        Json Action(const Json &a, const std::string &at, const std::string &quest, const std::string &npc, std::vector<Diagnostic> &d)
        {
            const std::string kind = Str(a, "kind", at, 32, d);
            if (kind == "startQuest")
                return {{"startQuest", quest}};
            if (kind == "talk")
                return {{"talk", npc}};
            if (kind == "give" || kind == "take")
                return {{kind, Str(a, "item", at, 64, d)}, {"count", Int(a, "count", at, 1, 20, 1, d)}};
            Err(d, "draft.kind", at + ".kind", "unknown action '" + kind + "'");
            return nullptr;
        }

        Json Quest(const Json &q, const std::string &id, std::vector<Diagnostic> &d)
        {
            Json quest = {{"id", id}, {"name", Str(q, "name", "quest", 60, d)}, {"description", Str(q, "description", "quest", 400, d)}};
            Json objectives = Json::array();
            const Json &list = Array(q, "objectives", "quest", 5, d);
            if (list.empty())
                Err(d, "draft.field", "quest.objectives", "a quest needs at least one objective");
            for (std::size_t i = 0; i < list.size() && i < 5; ++i)
            {
                const std::string at = "quest.objectives[" + std::to_string(i) + "]";
                const std::string kind = Str(list[i], "kind", at, 16, d);
                if (kind != "defeat" && kind != "obtain" && kind != "talk")
                {
                    Err(d, "draft.kind", at + ".kind", "must be defeat, obtain or talk");
                    continue;
                }
                Json objective = {{kind, Str(list[i], "target", at, 64, d)}};
                const int count = Int(list[i], "count", at, 1, 20, 1, d);
                if (kind != "talk" && count > 1)
                    objective["count"] = count;
                objective["text"] = Str(list[i], "text", at, 120, d);
                objectives.push_back(objective);
            }
            quest["objectives"] = objectives;

            const Json &r = q.value("reward", Json::object());
            Json reward = {{"xp", Int(r, "xp", "quest.reward", 0, 1000, 0, d)}, {"skillPoints", Int(r, "skillPoints", "quest.reward", 0, 3, 0, d)}};
            Json items = Json::array();
            if (r.contains("items"))
            {
                const Json &list2 = Array(r, "items", "quest.reward", 3, d);
                for (std::size_t i = 0; i < list2.size() && i < 3; ++i)
                {
                    const std::string at = "quest.reward.items[" + std::to_string(i) + "]";
                    items.push_back({{"item", Str(list2[i], "item", at, 64, d)}, {"count", Int(list2[i], "count", at, 1, 5, 1, d)}});
                }
            }
            if (!items.empty())
                reward["items"] = items;
            quest["reward"] = reward;
            return quest;
        }

        // Nodes with ids made unique by the quest id prefix ("talk" -> "wolves_talk").
        Json Nodes(const Json &draft, const std::string &quest, const std::string &npc, std::string &start, std::vector<Diagnostic> &d)
        {
            const Json &list = Array(draft, "nodes", "draft", 20, d);
            std::set<std::string> ids;
            for (const Json &node : list)
                ids.insert(MakeId(node.value("id", "")));
            auto local = [&](const std::string &raw, const std::string &at) {
                const std::string id = MakeId(raw);
                if (!ids.contains(id))
                    Err(d, "draft.next", at, "names no node: '" + raw + "'");
                return quest + "_" + id;
            };
            start = local(Str(draft, "start", "draft", 64, d), "draft.start");

            Json nodes = Json::array();
            std::set<std::string> seen;
            for (std::size_t i = 0; i < list.size() && i < 20; ++i)
            {
                const std::string at = "nodes[" + std::to_string(i) + "]";
                const std::string id = MakeId(Str(list[i], "id", at, 64, d));
                if (!seen.insert(id).second)
                    Err(d, "draft.node.duplicate", at + ".id", "two nodes are called '" + id + "'");
                Json node = {{"id", quest + "_" + id}, {"speaker", Str(list[i], "speaker", at, 60, d)}, {"text", Str(list[i], "text", at, 300, d)}};
                Json choices = Json::array();
                const Json &options = list[i].contains("choices") ? Array(list[i], "choices", at, 4, d) : Json::array();
                for (std::size_t j = 0; j < options.size() && j < 4; ++j)
                {
                    const std::string cat = at + ".choices[" + std::to_string(j) + "]";
                    Json choice = {{"text", Str(options[j], "text", cat, 120, d)}};
                    const std::string next = Str(options[j], "next", cat, 64, d, false);
                    if (!next.empty())
                        choice["next"] = local(next, cat + ".next");
                    Json conditions = Json::array();
                    Json actions = Json::array();
                    if (options[j].contains("requires"))
                        for (const Json &r : Array(options[j], "requires", cat, 3, d))
                            if (Json c = Requirement(r, cat + ".requires", quest, d); !c.is_null())
                                conditions.push_back(c);
                    if (options[j].contains("actions"))
                        for (const Json &a : Array(options[j], "actions", cat, 3, d))
                            if (Json c = Action(a, cat + ".actions", quest, npc, d); !c.is_null())
                                actions.push_back(c);
                    if (!conditions.empty())
                        choice["if"] = conditions;
                    if (!actions.empty())
                        choice["do"] = actions;
                    choices.push_back(choice);
                }
                if (!choices.empty())
                    node["choices"] = choices;
                nodes.push_back(node);
            }
            if (nodes.empty())
                Err(d, "draft.field", "draft.nodes", "the conversation needs at least one node");
            return nodes;
        }

        void Preview(const Json &quest, const Json &nodes, const std::string &host, const std::string &offer, const Json &rpg, DraftPlan &plan)
        {
            std::vector<std::string> &out = plan.preview;
            out.push_back("New quest \"" + quest["name"].get<std::string>() + "\" (" + plan.quest + ")");
            out.push_back("  " + quest["description"].get<std::string>());
            int n = 1;
            for (const Json &o : quest["objectives"])
            {
                const std::string kind = o.contains("defeat") ? "defeat" : o.contains("obtain") ? "obtain" : "talk";
                const std::string target = o[kind].get<std::string>();
                const std::string name = NameOf(rpg, kind == "obtain" ? "items" : "actors", target);
                const std::string what = kind == "talk" ? "talk to " + target : kind + " " + name + " x" + std::to_string(o.value("count", 1));
                out.push_back("  " + std::to_string(n++) + ". " + what + ": " + o["text"].get<std::string>());
            }
            std::string reward = "  Reward: " + std::to_string(quest["reward"]["xp"].get<int>()) + " XP, " +
                                 std::to_string(quest["reward"]["skillPoints"].get<int>()) + " skill points";
            for (const Json &i : quest["reward"].value("items", Json::array()))
                reward += ", " + NameOf(rpg, "items", i["item"]) + " x" + std::to_string(i["count"].get<int>());
            out.push_back(reward);
            out.push_back(host.empty() ? "New conversation '" + plan.conversation + "':"
                                       : "Added to conversation '" + plan.conversation + "' as the choice \"" + offer + "\":");
            for (const Json &node : nodes)
            {
                out.push_back("  [" + node["id"].get<std::string>() + "] " + node["speaker"].get<std::string>() + ": " + node["text"].get<std::string>());
                for (const Json &c : node.value("choices", Json::array()))
                {
                    std::string line = "      > \"" + c["text"].get<std::string>() + "\" -> " + c.value("next", std::string("(end)"));
                    for (const Json &r : c.value("if", Json::array()))
                        line += r.contains("quest") ? "  [if quest " + r["state"].get<std::string>() + "]"
                                                    : "  [if holding " + NameOf(rpg, "items", r["hasItem"]) + " x" + std::to_string(r["count"].get<int>()) + "]";
                    for (const Json &a : c.value("do", Json::array()))
                    {
                        if (a.contains("startQuest"))
                            line += "  [starts the quest]";
                        else if (a.contains("talk"))
                            line += "  [reports to " + a["talk"].get<std::string>() + "]";
                        else
                            for (const char *kind : {"give", "take"})
                                if (a.contains(kind))
                                    line += std::string("  [") + (kind[0] == 'g' ? "gives " : "takes ") + NameOf(rpg, "items", a[kind]) + " x" +
                                            std::to_string(a["count"].get<int>()) + "]";
                    }
                    out.push_back(line);
                }
            }
        }

        // Walks any JSON and collects spawner pools and conversation references.
        void Collect(const nlohmann::json &j, WorldFacts &facts)
        {
            if (j.is_object())
            {
                for (auto it = j.begin(); it != j.end(); ++it)
                {
                    if (it.key() == "pool" && it->is_array())
                        for (const auto &actor : *it)
                            if (actor.is_string())
                                facts.spawned.insert(actor.get<std::string>());
                    if (it.key() == "conversation" && it->is_string())
                        facts.conversations.insert(it->get<std::string>());
                    Collect(*it, facts);
                }
            }
            else if (j.is_array())
                for (const auto &item : j)
                    Collect(item, facts);
        }
    }

    WorldFacts ScanScenes(const std::filesystem::path &sceneFolder)
    {
        WorldFacts facts;
        std::error_code error;
        for (const auto &entry : std::filesystem::directory_iterator(sceneFolder, error))
        {
            if (!entry.path().string().ends_with(".scene.json"))
                continue;
            std::ifstream in(entry.path());
            Collect(nlohmann::json::parse(in, nullptr, false), facts);
        }
        return facts;
    }

    std::vector<std::string> DraftNpcs(const Json &rpg)
    {
        std::set<std::string> npcs;
        for (const Json &c : rpg.value("conversations", Json::array()))
            npcs.insert(c.value("npc", ""));
        npcs.erase("");
        return {npcs.begin(), npcs.end()};
    }

    Json DraftSchema()
    {
        const Json text = {{"type", "string"}};
        const Json count = {{"type", "integer"}, {"minimum", 1}, {"maximum", 20}};
        auto object = [](Json properties) {
            Json required = Json::array();
            for (auto it = properties.begin(); it != properties.end(); ++it)
                required.push_back(it.key());
            return Json{{"type", "object"}, {"properties", properties}, {"required", required}};
        };
        auto list = [](Json item) { return Json{{"type", "array"}, {"items", item}}; };
        auto oneOf = [](std::initializer_list<const char *> values) {
            Json e = Json::array();
            for (const char *v : values)
                e.push_back(v);
            return Json{{"type", "string"}, {"enum", e}};
        };
        const Json requirement = object({{"kind", oneOf({"questNotStarted", "questActive", "questCompleted", "hasItem"})}, {"item", text}, {"count", count}});
        const Json action = object({{"kind", oneOf({"startQuest", "talk", "give", "take"})}, {"item", text}, {"count", count}});
        const Json choice = object({{"text", text}, {"next", text}, {"requires", list(requirement)}, {"actions", list(action)}});
        const Json node = object({{"id", text}, {"speaker", text}, {"text", text}, {"choices", list(choice)}});
        const Json objective = object({{"kind", oneOf({"defeat", "obtain", "talk"})}, {"target", text}, {"count", count}, {"text", text}});
        const Json item = object({{"item", text}, {"count", count}});
        const Json reward = object({{"xp", {{"type", "integer"}}}, {"skillPoints", {{"type", "integer"}}}, {"items", list(item)}});
        const Json quest = object({{"name", text}, {"description", text}, {"objectives", list(objective)}, {"reward", reward}});
        return object({{"quest", quest}, {"offer", text}, {"start", text}, {"nodes", list(node)}});
    }

    std::string DraftInstructions(const Json &rpg, const std::string &npc)
    {
        // 1. The ids the draft may use.
        std::string items, enemies, speaker = npc;
        for (const Json &i : rpg.value("items", Json::array()))
            items += "  " + i.value("id", "") + " (" + i.value("name", "") + ")\n";
        for (const Json &a : rpg.value("actors", Json::array()))
            if (a.value("id", "") != "hero")
                enemies += "  " + a.value("id", "") + " (" + a.value("name", "") + ")\n";
        for (const Json &c : rpg.value("conversations", Json::array()))
            if (c.value("npc", "") == npc && c.contains("nodes") && !c["nodes"].empty())
                speaker = c["nodes"][0].value("speaker", npc);

        // 2. The rules, then one small example.
        return "You write one side quest for a small 3D fantasy RPG. Answer with JSON only, following the schema.\n"
               "Use only these ids.\nItems:\n" + items + "Enemies:\n" + enemies +
               "The quest giver is the NPC '" + npc + "', who speaks as \"" + speaker + "\".\n\n"
               "Rules:\n"
               "- quest.objectives are done in order. defeat: target is an enemy id, count how many. obtain: target is an item id "
               "the player must hold. talk: target is '" + npc + "'; it is done when the player picks a choice with action talk.\n"
               "- The player reaches the node 'start' by saying the line 'offer' to the NPC. nodes are the NPC's lines; each choice is "
               "a player answer. next is another node id, or \"\" to end the conversation.\n"
               "- requires decides when a choice is shown: questNotStarted, questActive, questCompleted, or hasItem (item, count).\n"
               "- actions: startQuest starts this quest; talk reports back to the NPC; give/take move items (item, count).\n"
               "- To be playable: one choice requires questNotStarted and has action startQuest. If the last objective is talk, one "
               "choice requires questActive (plus hasItem if the player brings something) and has action talk (plus take).\n"
               "- 3 to 8 nodes, lines under 200 characters, reward xp 20 to 200. Unused item is \"\", unused count is 1.\n"
               "Example shape: {\"quest\":{\"name\":\"...\",\"description\":\"...\",\"objectives\":[{\"kind\":\"defeat\",\"target\":\"<enemy>\","
               "\"count\":2,\"text\":\"...\"},{\"kind\":\"talk\",\"target\":\"" + npc + "\",\"count\":1,\"text\":\"...\"}],\"reward\":{\"xp\":60,"
               "\"skillPoints\":0,\"items\":[]}},\"offer\":\"Do you need help?\",\"start\":\"ask\",\"nodes\":[{\"id\":\"ask\",\"speaker\":\"" + speaker +
               "\",\"text\":\"...\",\"choices\":[{\"text\":\"I'll do it.\",\"next\":\"thanks\",\"requires\":[{\"kind\":\"questNotStarted\",\"item\":\"\","
               "\"count\":1}],\"actions\":[{\"kind\":\"startQuest\",\"item\":\"\",\"count\":1}]},{\"text\":\"It's done.\",\"next\":\"thanks\",\"requires\":"
               "[{\"kind\":\"questActive\",\"item\":\"\",\"count\":1}],\"actions\":[{\"kind\":\"talk\",\"item\":\"\",\"count\":1}]}]},{\"id\":\"thanks\","
               "\"speaker\":\"" + speaker + "\",\"text\":\"...\",\"choices\":[]}]}\n";
    }

    bool CheckDraftScope(const Json &rpg, const std::vector<Change> &changes, std::vector<Diagnostic> &diagnostics)
    {
        bool ok = true;
        for (const Change &change : changes)
        {
            const Json::json_pointer pointer(change.path);
            const bool section = change.path == "/quests/-" || change.path == "/conversations/-";
            // A new node, or a new choice in a node (not a field of an existing choice).
            static const std::regex conversationPart(R"(^/conversations/\d+/nodes/(-|\d+/choices(/\d+|/-)?)$)");
            const bool inConversation = std::regex_match(change.path, conversationPart);
            // Insert always adds; Set only counts as adding when nothing is there yet.
            const bool adds = change.op == Change::Op::Insert || (change.op == Change::Op::Set && !rpg.contains(pointer));
            if (!adds || !(section || inConversation))
            {
                Err(diagnostics, "draft.scope", change.path, "a draft may only add quests, conversations, nodes and choices");
                ok = false;
            }
        }
        return ok;
    }

    bool PlanDraft(const Json &rpg, const Json &draft, const std::string &npc, const WorldFacts &world, DraftPlan &plan,
                   std::vector<Diagnostic> &diagnostics)
    {
        plan = {};
        std::vector<Diagnostic> d;
        if (!draft.is_object())
        {
            Err(diagnostics, "draft.json", "", "the answer is not a JSON object");
            return false;
        }
        if (npc.empty())
        {
            Err(diagnostics, "draft.npc", "", "choose the NPC who gives the quest");
            return false;
        }

        // 1. Read the draft into content entries (ids made safe and unique).
        const Json &q = draft.value("quest", Json::object());
        std::string base = MakeId(q.value("name", ""));
        plan.quest = FreeId(rpg, "quests", base.empty() ? "quest" : base);
        const Json quest = Quest(q, plan.quest, d);
        std::string start;
        const Json nodes = Nodes(draft, plan.quest, npc, start, d);
        const std::string offer = Str(draft, "offer", "draft", 120, d);
        plan.label = "AI draft: " + q.value("name", plan.quest);
        if (core::HasErrors(d))
        {
            diagnostics.insert(diagnostics.end(), d.begin(), d.end());
            return false;
        }

        // 2. Changes: the quest, then the lines in the NPC's conversation (the one a scene uses,
        //    if several) or in a new conversation.
        plan.changes.push_back({Change::Op::Insert, "/quests/-", quest});
        int host = -1;
        const Json &conversations = rpg.value("conversations", Json::array());
        for (int i = 0; i < static_cast<int>(conversations.size()); ++i)
            if (conversations[i].value("npc", "") == npc &&
                (host < 0 || (world.conversations.contains(conversations[i].value("id", "")) &&
                              !world.conversations.contains(conversations[host].value("id", "")))))
                host = i;
        if (host >= 0)
        {
            const Json &c = conversations[host];
            plan.conversation = c.value("id", "");
            const std::string at = "/conversations/" + std::to_string(host) + "/nodes/";
            for (const Json &node : nodes)
                plan.changes.push_back({Change::Op::Insert, at + "-", node});
            // The offer goes before a final "goodbye" choice, if there is one.
            const Json offerChoice = {{"text", offer}, {"next", start}};
            for (std::size_t si = 0; si < c["nodes"].size(); ++si)
            {
                const Json &node = c["nodes"][si];
                if (node.value("id", "") != c.value("start", ""))
                    continue;
                if (!node.contains("choices"))
                {
                    plan.changes.push_back({Change::Op::Set, at + std::to_string(si) + "/choices", Json::array({offerChoice})});
                    break;
                }
                std::size_t position = node["choices"].size();
                if (position > 0 && node["choices"].back().value("next", "").empty())
                    --position;
                plan.changes.push_back({Change::Op::Insert, at + std::to_string(si) + "/choices/" + std::to_string(position), offerChoice});
                break;
            }
        }
        else
        {
            plan.conversation = FreeId(rpg, "conversations", plan.quest + "_talk");
            plan.changes.push_back({Change::Op::Insert, "/conversations/-", {{"id", plan.conversation}, {"npc", npc}, {"start", start}, {"nodes", nodes}}});
        }
        if (!world.conversations.contains(plan.conversation))
            Warn(d, "draft.play.npc", "", "No NPC in a scene uses conversation '" + plan.conversation +
                                              "' yet: select the NPC in the editor and set its interactable conversation.");
        Preview(quest, nodes, host >= 0 ? plan.conversation : "", offer, rpg, plan);

        // 3. Same checks as any manual edit, then the play-through.
        const bool valid = CheckDraftScope(rpg, plan.changes, d) && ApplyChanges(rpg, plan.changes, plan.result, d, "rpg content") &&
                           RpgValidator()(plan.result, "rpg content", d);
        if (valid)
            Play(plan.result, plan, npc, world, d);
        diagnostics.insert(diagnostics.end(), d.begin(), d.end());
        return !core::HasErrors(d);
    }
}
