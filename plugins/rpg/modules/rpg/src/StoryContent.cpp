// Quests and conversations in the umbra.rpg content file (format in Story.md).
#include "ContentReader.hpp"

namespace umbra::rpg::detail
{
    namespace
    {
        std::optional<QuestState> ParseQuestState(const std::string &name)
        {
            if (name == "notStarted") return QuestState::NotStarted;
            if (name == "active") return QuestState::Active;
            if (name == "completed") return QuestState::Completed;
            if (name == "failed") return QuestState::Failed;
            return std::nullopt;
        }

        // {"quest": "relic", "state": "active"} or {"hasItem": "relic", "count": 1}
        std::vector<Condition> ReadConditions(Reader &r, const json &choice, const std::string &where)
        {
            std::vector<Condition> conditions;
            r.Objects(choice, "if", where + ".", [&](const json &c, const std::string &at) {
                r.Known(c, at, {"quest", "state", "hasItem", "count"});
                if (c.contains("quest"))
                {
                    const std::optional<QuestState> state = ParseQuestState(r.String(c, "state", at));
                    if (!state)
                        r.Error("condition.state", at + ".state", "expected notStarted, active, completed or failed");
                    conditions.push_back({Condition::Kind::QuestIs, r.String(c, "quest", at), state.value_or(QuestState::NotStarted)});
                }
                else if (c.contains("hasItem"))
                    conditions.push_back({Condition::Kind::HasItem, r.String(c, "hasItem", at), {}, r.Int(c, "count", 1, 1, 999, at)});
                else
                    r.Error("condition.kind", at, "expected \"quest\" + \"state\" or \"hasItem\"");
            });
            return conditions;
        }

        // {"startQuest": id} | {"talk": npc} | {"give": item, "count": n} | {"take": item, "count": n}
        std::vector<StoryAction> ReadActions(Reader &r, const json &choice, const std::string &where)
        {
            std::vector<StoryAction> actions;
            r.Objects(choice, "do", where + ".", [&](const json &a, const std::string &at) {
                r.Known(a, at, {"startQuest", "talk", "give", "take", "count"});
                const int count = r.Int(a, "count", 1, 1, 999, at);
                if (a.contains("startQuest"))
                    actions.push_back({StoryAction::Kind::StartQuest, r.String(a, "startQuest", at)});
                else if (a.contains("talk"))
                    actions.push_back({StoryAction::Kind::Talk, r.String(a, "talk", at)});
                else if (a.contains("give"))
                    actions.push_back({StoryAction::Kind::Give, r.String(a, "give", at), count});
                else if (a.contains("take"))
                    actions.push_back({StoryAction::Kind::Take, r.String(a, "take", at), count});
                else
                    r.Error("action.kind", at, "expected startQuest, talk, give or take");
            });
            return actions;
        }

        template <typename Def>
        void Insert(Reader &r, std::map<std::string, Def> &defs, Def def, const std::string &where)
        {
            if (def.id.empty())
                r.Error("id.missing", where + ".id", "needs a non-empty id");
            else if (!defs.emplace(def.id, def).second)
                r.Error("id.duplicate", where + ".id", "'" + def.id + "' is defined twice");
        }
    }

    void ReadStorySections(Reader &r, const json &root, Content &c)
    {
        r.Objects(root, "quests", "", [&](const json &q, const std::string &at) {
            r.Known(q, at, {"id", "name", "description", "objectives", "failOn", "reward"});
            QuestDef def{r.String(q, "id", at), r.String(q, "name", at), r.String(q, "description", at)};
            r.Objects(q, "objectives", at + ".", [&](const json &o, const std::string &oat) {
                r.Known(o, oat, {"defeat", "obtain", "talk", "count", "text"});
                ObjectiveDef objective{ObjectiveKind::Talk, "", r.Int(o, "count", 1, 1, 99, oat), r.String(o, "text", oat)};
                if (o.contains("defeat"))
                    objective = {ObjectiveKind::Defeat, r.String(o, "defeat", oat), objective.count, objective.text};
                else if (o.contains("obtain"))
                    objective = {ObjectiveKind::Obtain, r.String(o, "obtain", oat), objective.count, objective.text};
                else if (o.contains("talk"))
                    objective.target = r.String(o, "talk", oat);
                else
                    r.Error("objective.kind", oat, "expected defeat, obtain or talk");
                def.objectives.push_back(objective);
            });
            if (def.objectives.empty())
                r.Error("quest.objectives", at + ".objectives", "a quest needs at least one objective");
            for (const std::string &reason : r.Strings(q, "failOn", at))
            {
                if (reason == "death")
                    def.failOnDeath = true;
                else
                    r.Error("quest.failOn", at + ".failOn", "the only fail condition so far is \"death\"");
            }
            if (const auto reward = q.find("reward"); reward != q.end() && reward->is_object())
            {
                const std::string rat = at + ".reward";
                r.Known(*reward, rat, {"xp", "skillPoints", "items"});
                def.xp = r.Int(*reward, "xp", 0, 0, 100000, rat);
                def.skillPoints = r.Int(*reward, "skillPoints", 0, 0, 100, rat);
                r.Objects(*reward, "items", rat + ".", [&](const json &i, const std::string &iat) {
                    r.Known(i, iat, {"item", "count"});
                    def.items.push_back({r.String(i, "item", iat), r.Int(i, "count", 1, 1, 999, iat)});
                });
            }
            Insert(r, c.quests, def, at);
        });

        r.Objects(root, "conversations", "", [&](const json &conv, const std::string &at) {
            r.Known(conv, at, {"id", "npc", "start", "nodes"});
            ConversationDef def{r.String(conv, "id", at), r.String(conv, "npc", at), r.String(conv, "start", at)};
            r.Objects(conv, "nodes", at + ".", [&](const json &n, const std::string &nat) {
                r.Known(n, nat, {"id", "speaker", "text", "choices"});
                DialogueNode node{r.String(n, "speaker", nat), r.String(n, "text", nat)};
                r.Objects(n, "choices", nat + ".", [&](const json &ch, const std::string &cat) {
                    r.Known(ch, cat, {"text", "next", "if", "do"});
                    node.choices.push_back({r.String(ch, "text", cat), r.String(ch, "next", cat), ReadConditions(r, ch, cat), ReadActions(r, ch, cat)});
                });
                const std::string id = r.String(n, "id", nat);
                if (id.empty())
                    r.Error("id.missing", nat + ".id", "needs a non-empty id");
                else if (!def.nodes.emplace(id, node).second)
                    r.Error("id.duplicate", nat + ".id", "node '" + id + "' is defined twice");
            });
            if (def.npc.empty())
                r.Error("conversation.npc", at + ".npc", "needs the npc it belongs to");
            Insert(r, c.conversations, def, at);
        });
    }

    void CheckStoryReferences(Reader &r, const Content &c)
    {
        const auto checkItem = [&](const std::string &item, const std::string &at) {
            if (!c.FindItem(item))
                r.Error("ref.item", at, "unknown item '" + item + "'");
        };
        for (const auto &[id, quest] : c.quests)
        {
            const std::string at = "quests." + id;
            for (const ObjectiveDef &o : quest.objectives)
            {
                if (o.target.empty())
                    r.Error("objective.target", at + ".objectives", "an objective needs a target");
                else if (o.kind == ObjectiveKind::Defeat && !c.FindActor(o.target))
                    r.Error("ref.actor", at + ".objectives", "unknown actor '" + o.target + "'");
                else if (o.kind == ObjectiveKind::Obtain)
                    checkItem(o.target, at + ".objectives");
            }
            for (const ItemCount &reward : quest.items)
                checkItem(reward.item, at + ".reward");
        }
        for (const auto &[id, conv] : c.conversations)
        {
            const std::string at = "conversations." + id;
            // Every link must land on a node: a dangling "next" would strand the player mid-talk.
            if (!conv.nodes.contains(conv.start))
                r.Error("conversation.start", at + ".start", "start node '" + conv.start + "' does not exist");
            for (const auto &[nodeId, node] : conv.nodes)
            {
                for (const DialogueChoice &choice : node.choices)
                {
                    const std::string cat = at + ".nodes." + nodeId;
                    if (!choice.next.empty() && !conv.nodes.contains(choice.next))
                        r.Error("conversation.next", cat, "choice leads to missing node '" + choice.next + "'");
                    for (const Condition &cond : choice.conditions)
                    {
                        if (cond.kind == Condition::Kind::QuestIs && !c.FindQuest(cond.id))
                            r.Error("ref.quest", cat, "unknown quest '" + cond.id + "'");
                        if (cond.kind == Condition::Kind::HasItem)
                            checkItem(cond.id, cat);
                    }
                    for (const StoryAction &action : choice.actions)
                    {
                        if (action.kind == StoryAction::Kind::StartQuest && !c.FindQuest(action.id))
                            r.Error("ref.quest", cat, "unknown quest '" + action.id + "'");
                        if (action.kind == StoryAction::Kind::Give || action.kind == StoryAction::Kind::Take)
                            checkItem(action.id, cat);
                    }
                }
            }
        }
    }
}
