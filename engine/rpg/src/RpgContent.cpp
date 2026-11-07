#include <RpgContent.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <functional>
#include <iterator>
#include <set>

namespace umbra::rpg
{
    using nlohmann::json;

    namespace
    {
        constexpr int maxValue = 9999; // stats, costs and powers stay small and readable
        constexpr const char *statNames[] = {"maxHealth", "maxEnergy", "attack", "defense", "speed"};

        // Collects diagnostics for one file and reads typed fields with bounds.
        struct Reader final
        {
            std::string file;
            std::vector<core::Diagnostic> &out;

            void Error(const std::string &code, const std::string &field, const std::string &message)
            {
                out.push_back({core::Severity::Error, code, file, field, message});
            }
            void Warning(const std::string &code, const std::string &field, const std::string &message)
            {
                out.push_back({core::Severity::Warning, code, file, field, message});
            }

            // Warns about keys this format doesn't know (usually typos).
            void Known(const json &object, const std::string &where, std::initializer_list<const char *> keys)
            {
                const std::set<std::string> known(keys.begin(), keys.end());
                for (const auto &[key, unused] : object.items())
                {
                    if (!known.contains(key))
                        Warning("field.unknown", where + "." + key, "unknown field is ignored (typo?)");
                }
            }

            int Int(const json &object, const char *key, int fallback, int min, int max, const std::string &where)
            {
                const auto it = object.find(key);
                if (it == object.end())
                    return fallback;
                if (!it->is_number_integer() || it->get<long long>() < min || it->get<long long>() > max)
                {
                    Error("field.range", where + "." + key, "expected a whole number from " + std::to_string(min) + " to " + std::to_string(max));
                    return fallback;
                }
                return it->get<int>();
            }

            float Float(const json &object, const char *key, float fallback, float min, float max, const std::string &where)
            {
                const auto it = object.find(key);
                if (it == object.end())
                    return fallback;
                if (!it->is_number() || it->get<double>() < min || it->get<double>() > max)
                {
                    Error("field.range", where + "." + key, "expected a number from " + std::to_string(min) + " to " + std::to_string(max));
                    return fallback;
                }
                return it->get<float>();
            }

            std::string String(const json &object, const char *key, const std::string &where)
            {
                const auto it = object.find(key);
                if (it == object.end())
                    return {};
                if (!it->is_string())
                {
                    Error("field.type", where + "." + key, "expected a string");
                    return {};
                }
                return it->get<std::string>();
            }

            bool Bool(const json &object, const char *key, const std::string &where)
            {
                const auto it = object.find(key);
                if (it == object.end())
                    return false;
                if (!it->is_boolean())
                {
                    Error("field.type", where + "." + key, "expected true or false");
                    return false;
                }
                return it->get<bool>();
            }

            std::vector<std::string> Strings(const json &object, const char *key, const std::string &where)
            {
                std::vector<std::string> values;
                const auto it = object.find(key);
                if (it == object.end())
                    return values;
                if (!it->is_array())
                {
                    Error("field.type", where + "." + key, "expected a list of strings");
                    return values;
                }
                for (std::size_t i = 0; i < it->size(); ++i)
                {
                    if ((*it)[i].is_string())
                        values.push_back((*it)[i].get<std::string>());
                    else
                        Error("field.type", where + "." + key + "[" + std::to_string(i) + "]", "expected a string");
                }
                return values;
            }

            // Calls `each(object, where)` for every object in the list `key`.
            template <typename Each>
            void Objects(const json &object, const char *key, const std::string &where, Each each)
            {
                const auto it = object.find(key);
                if (it == object.end())
                    return;
                if (!it->is_array())
                {
                    Error("field.type", where + key, "expected a list");
                    return;
                }
                for (std::size_t i = 0; i < it->size(); ++i)
                {
                    const std::string at = where + key + "[" + std::to_string(i) + "]";
                    if ((*it)[i].is_object())
                        each((*it)[i], at);
                    else
                        Error("field.type", at, "expected an object");
                }
            }
        };

        std::optional<Stat> ParseStat(const std::string &name)
        {
            for (std::size_t i = 0; i < std::size(statNames); ++i)
            {
                if (name == statNames[i])
                    return static_cast<Stat>(i);
            }
            return std::nullopt;
        }

        std::vector<Modifier> ReadModifiers(Reader &r, const json &object, const std::string &where)
        {
            std::vector<Modifier> modifiers;
            r.Objects(object, "modifiers", where + ".", [&](const json &m, const std::string &at) {
                r.Known(m, at, {"stat", "add", "percent"});
                const std::optional<Stat> stat = ParseStat(r.String(m, "stat", at));
                if (!stat)
                    r.Error("modifier.stat", at + ".stat", "expected maxHealth, maxEnergy, attack, defense or speed");
                modifiers.push_back({stat.value_or(Stat::Attack), r.Int(m, "add", 0, -maxValue, maxValue, at),
                                     r.Int(m, "percent", 0, -90, 1000, at)});
            });
            return modifiers;
        }

        std::vector<EffectDef> ReadEffects(Reader &r, const json &object, const char *key, const std::string &where)
        {
            std::vector<EffectDef> effects;
            r.Objects(object, key, where + ".", [&](const json &e, const std::string &at) {
                r.Known(e, at, {"kind", "power", "status"});
                const std::string kind = r.String(e, "kind", at);
                EffectDef effect;
                if (kind == "damage")
                    effect.kind = EffectKind::Damage;
                else if (kind == "heal")
                    effect.kind = EffectKind::Heal;
                else if (kind == "energy")
                    effect.kind = EffectKind::RestoreEnergy;
                else if (kind == "status")
                    effect.kind = EffectKind::ApplyStatus;
                else
                    r.Error("effect.kind", at + ".kind", "expected damage, heal, energy or status");
                effect.power = r.Int(e, "power", 0, 0, maxValue, at);
                effect.status = r.String(e, "status", at);
                if (effect.kind == EffectKind::ApplyStatus && effect.status.empty())
                    r.Error("effect.status", at + ".status", "a status effect needs the status id");
                effects.push_back(effect);
            });
            return effects;
        }

        // Adds `def` under its id unless the id is empty or taken.
        template <typename Def>
        void Insert(Reader &r, std::map<std::string, Def> &defs, Def def, const std::string &where)
        {
            if (def.id.empty())
                r.Error("id.missing", where + ".id", "needs a non-empty id");
            else if (!defs.emplace(def.id, def).second)
                r.Error("id.duplicate", where + ".id", "'" + def.id + "' is defined twice");
        }

        void ReadSections(Reader &r, const json &root, Content &c)
        {
            r.Objects(root, "statuses", "", [&](const json &s, const std::string &at) {
                r.Known(s, at, {"id", "name", "turns", "seconds", "damagePerTick", "modifiers"});
                StatusDef def{r.String(s, "id", at), r.String(s, "name", at), {}, {}, 0, ReadModifiers(r, s, at)};
                if (s.contains("turns"))
                    def.turns = r.Int(s, "turns", 1, 1, 100, at);
                if (s.contains("seconds"))
                    def.seconds = r.Float(s, "seconds", 1.0F, 0.1F, 600.0F, at);
                def.damagePerTick = r.Int(s, "damagePerTick", 0, -maxValue, maxValue, at);
                if (!def.turns && !def.seconds)
                    r.Error("status.clock", at, "needs \"turns\" and/or \"seconds\"");
                Insert(r, c.statuses, def, at);
            });

            r.Objects(root, "skills", "", [&](const json &s, const std::string &at) {
                r.Known(s, at, {"id", "name", "cost", "target", "effects", "modes", "realtime", "points", "requires"});
                SkillDef def{r.String(s, "id", at), r.String(s, "name", at)};
                def.energyCost = r.Int(s, "cost", 0, 0, maxValue, at);
                const std::string target = r.String(s, "target", at);
                if (target == "self")
                    def.target = TargetRule::Self;
                else if (!target.empty() && target != "enemy")
                    r.Error("skill.target", at + ".target", "expected enemy or self");
                def.effects = ReadEffects(r, s, "effects", at);
                for (const std::string &mode : r.Strings(s, "modes", at))
                {
                    if (mode == "turn")
                        def.turn = true;
                    else if (mode == "realtime")
                        def.realtime = true;
                    else
                        r.Error("skill.mode", at + ".modes", "expected turn and/or realtime");
                }
                if (!def.turn && !def.realtime)
                    r.Error("skill.mode", at + ".modes", "needs at least one mode: turn, realtime");
                if (def.realtime)
                {
                    const auto t = s.find("realtime");
                    if (t == s.end() || !t->is_object())
                        r.Error("skill.timing", at + ".realtime", "real-time skills need a \"realtime\" timing object");
                    else
                    {
                        const std::string tat = at + ".realtime";
                        r.Known(*t, tat, {"windup", "active", "recovery", "cooldown", "range", "arc"});
                        def.timing = {r.Float(*t, "windup", 0.0F, 0.0F, 10.0F, tat), r.Float(*t, "active", 0.1F, 0.01F, 10.0F, tat),
                                      r.Float(*t, "recovery", 0.0F, 0.0F, 10.0F, tat), r.Float(*t, "cooldown", 0.0F, 0.0F, 600.0F, tat),
                                      r.Float(*t, "range", 1.5F, 0.1F, 100.0F, tat), r.Float(*t, "arc", 90.0F, 1.0F, 360.0F, tat)};
                    }
                }
                def.pointCost = r.Int(s, "points", 0, 0, 100, at);
                def.prerequisites = r.Strings(s, "requires", at);
                Insert(r, c.skills, def, at);
            });

            r.Objects(root, "items", "", [&](const json &s, const std::string &at) {
                r.Known(s, at, {"id", "name", "slot", "stack", "modifiers", "use"});
                ItemDef def{r.String(s, "id", at), r.String(s, "name", at)};
                const std::string slot = r.String(s, "slot", at);
                if (slot == "weapon")
                    def.slot = Slot::Weapon;
                else if (slot == "armor")
                    def.slot = Slot::Armor;
                else if (!slot.empty())
                    r.Error("item.slot", at + ".slot", "expected weapon or armor");
                def.maxStack = r.Int(s, "stack", 1, 1, 999, at);
                def.modifiers = ReadModifiers(r, s, at);
                def.use = ReadEffects(r, s, "use", at);
                if (def.slot != Slot::None && def.maxStack != 1)
                    r.Error("item.stack", at + ".stack", "equipment does not stack");
                if (def.slot == Slot::None && !def.modifiers.empty())
                    r.Warning("item.modifiers", at + ".modifiers", "modifiers only apply to equipment; they are ignored");
                Insert(r, c.items, def, at);
            });

            r.Objects(root, "actors", "", [&](const json &s, const std::string &at) {
                r.Known(s, at, {"id", "name", "stats", "skills", "equipment", "inventory", "inventorySlots", "skillPoints"});
                ActorDef def{r.String(s, "id", at), r.String(s, "name", at)};
                const auto stats = s.find("stats");
                if (stats == s.end() || !stats->is_object())
                    r.Error("actor.stats", at + ".stats", "needs a \"stats\" object");
                else
                {
                    r.Known(*stats, at + ".stats", {"maxHealth", "maxEnergy", "attack", "defense", "speed"});
                    for (std::size_t i = 0; i < std::size(statNames); ++i)
                        def.base[i] = r.Int(*stats, statNames[i], 0, i == 0 ? 1 : 0, maxValue, at + ".stats");
                    if (!stats->contains("maxHealth"))
                        r.Error("actor.stats", at + ".stats.maxHealth", "maxHealth is required");
                }
                def.skills = r.Strings(s, "skills", at);
                def.equipment = r.Strings(s, "equipment", at);
                r.Objects(s, "inventory", at + ".", [&](const json &i, const std::string &iat) {
                    r.Known(i, iat, {"item", "count"});
                    def.inventory.push_back({r.String(i, "item", iat), r.Int(i, "count", 1, 1, 999, iat)});
                });
                def.inventorySlots = r.Int(s, "inventorySlots", 12, 0, 100, at);
                def.skillPoints = r.Int(s, "skillPoints", 0, 0, 100, at);
                Insert(r, c.actors, def, at);
            });
        }

        // A skill that (indirectly) requires itself could never be unlocked. Depth-first search:
        // reaching a skill that is still on the current path means a cycle.
        void CheckCycles(Reader &r, const Content &c)
        {
            enum class Mark { None, OnPath, Done };
            std::map<std::string, Mark> marks;
            std::function<bool(const std::string &)> visit = [&](const std::string &id) {
                Mark &mark = marks[id];
                if (mark == Mark::OnPath)
                    return true;
                if (mark == Mark::Done)
                    return false;
                mark = Mark::OnPath;
                if (const SkillDef *skill = c.FindSkill(id))
                {
                    for (const std::string &required : skill->prerequisites)
                    {
                        if (visit(required))
                            return true;
                    }
                }
                marks[id] = Mark::Done;
                return false;
            };
            for (const auto &[id, skill] : c.skills)
            {
                if (marks[id] == Mark::None && visit(id))
                {
                    r.Error("skill.requires.cycle", "skills." + id + ".requires", "skill requirements form a loop through '" + id + "'");
                    return; // one report is enough; the loop has to be fixed by hand
                }
            }
        }

        // Every id a definition mentions must exist, and statuses must have the clock of
        // every mode that can apply them (no silent turns <-> seconds reinterpretation).
        void CheckReferences(Reader &r, const Content &c)
        {
            const auto checkEffects = [&](const std::vector<EffectDef> &effects, bool turn, bool realtime, const std::string &at) {
                for (const EffectDef &effect : effects)
                {
                    if (effect.kind != EffectKind::ApplyStatus)
                        continue;
                    const StatusDef *status = c.FindStatus(effect.status);
                    if (status == nullptr)
                        r.Error("ref.status", at, "unknown status '" + effect.status + "'");
                    else if ((turn && !status->turns) || (realtime && !status->seconds))
                        r.Error("status.clock.unsupported", at, "status '" + effect.status + "' has no " +
                                                                    (turn && !status->turns ? "\"turns\"" : "\"seconds\"") +
                                                                    " duration but is used in that mode");
                }
            };
            for (const auto &[id, skill] : c.skills)
            {
                checkEffects(skill.effects, skill.turn, skill.realtime, "skills." + id + ".effects");
                for (const std::string &required : skill.prerequisites)
                {
                    if (!c.FindSkill(required))
                        r.Error("ref.skill", "skills." + id + ".requires", "unknown skill '" + required + "'");
                }
            }
            CheckCycles(r, c);
            for (const auto &[id, item] : c.items)
                checkEffects(item.use, true, true, "items." + id + ".use"); // items can be used in both modes
            for (const auto &[id, actor] : c.actors)
            {
                const std::string at = "actors." + id;
                for (const std::string &skill : actor.skills)
                {
                    if (!c.FindSkill(skill))
                        r.Error("ref.skill", at + ".skills", "unknown skill '" + skill + "'");
                }
                std::set<Slot> used;
                for (const std::string &itemId : actor.equipment)
                {
                    const ItemDef *item = c.FindItem(itemId);
                    if (item == nullptr)
                        r.Error("ref.item", at + ".equipment", "unknown item '" + itemId + "'");
                    else if (item->slot == Slot::None)
                        r.Error("actor.equipment", at + ".equipment", "'" + itemId + "' cannot be equipped");
                    else if (!used.insert(item->slot).second)
                        r.Error("actor.equipment", at + ".equipment", "two items for the same slot");
                }
                for (const ItemCount &entry : actor.inventory)
                {
                    if (!c.FindItem(entry.item))
                        r.Error("ref.item", at + ".inventory", "unknown item '" + entry.item + "'");
                }
            }
        }
    }

    const StatusDef *Content::FindStatus(const std::string &id) const
    {
        const auto it = statuses.find(id);
        return it == statuses.end() ? nullptr : &it->second;
    }
    const SkillDef *Content::FindSkill(const std::string &id) const
    {
        const auto it = skills.find(id);
        return it == skills.end() ? nullptr : &it->second;
    }
    const ItemDef *Content::FindItem(const std::string &id) const
    {
        const auto it = items.find(id);
        return it == items.end() ? nullptr : &it->second;
    }
    const ActorDef *Content::FindActor(const std::string &id) const
    {
        const auto it = actors.find(id);
        return it == actors.end() ? nullptr : &it->second;
    }

    const char *StatName(Stat stat) { return statNames[static_cast<std::size_t>(stat)]; }

    bool ParseContent(const std::string &text, const std::string &fileName, Content &content,
                      std::vector<core::Diagnostic> &diagnostics)
    {
        Reader r{fileName, diagnostics};
        const std::size_t firstNew = diagnostics.size();

        // 1. Header.
        const json root = json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            r.Error("json.parse", "", "not valid JSON");
            return false;
        }
        if (root.value("format", "") != "umbra.rpg" || root.value("version", 0) != 1)
        {
            r.Error("format.version", "", "expected \"format\": \"umbra.rpg\" and \"version\": 1");
            return false;
        }
        r.Known(root, "", {"format", "version", "statuses", "skills", "items", "actors"});

        // 2. Sections, then cross-references. Nothing is handed back on any error.
        Content parsed;
        ReadSections(r, root, parsed);
        CheckReferences(r, parsed);
        const std::vector<core::Diagnostic> found(diagnostics.begin() + static_cast<std::ptrdiff_t>(firstNew), diagnostics.end());
        if (core::HasErrors(found))
            return false;
        content = std::move(parsed);
        return true;
    }

    bool LoadContent(const std::filesystem::path &file, Content &content, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            diagnostics.push_back({core::Severity::Error, "file.read", file.string(), "", "cannot open file"});
            return false;
        }
        const std::string text{std::istreambuf_iterator<char>(in), {}};
        return ParseContent(text, file.string(), content, diagnostics);
    }
}
