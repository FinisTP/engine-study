#include <SaveGame.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <set>
#include <cstdint>
#include <fstream>
#include <iterator>

namespace umbra::game
{
    using nlohmann::json;

    namespace
    {
        constexpr int version = saveFormatVersion;

        // FNV-1a 64-bit over the payload text: catches truncation and accidental edits.
        std::string Checksum(const std::string &text)
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const unsigned char c : text)
            {
                hash ^= c;
                hash *= 1099511628211ull;
            }
            char out[17];
            std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(hash));
            return out;
        }

        const char *SlotName(rpg::Slot slot) { return slot == rpg::Slot::Weapon ? "weapon" : "armor"; }

        json Stacks(const std::vector<rpg::ItemStack> &stacks)
        {
            json out = json::array();
            for (const rpg::ItemStack &s : stacks)
                out.push_back({{"item", s.item}, {"count", s.count}});
            return out;
        }

        json ToJson(const SaveData &d)
        {
            const rpg::Character &h = d.hero;
            json equipment = json::object();
            for (const auto &[slot, item] : h.equipment)
                equipment[SlotName(slot)] = item;
            json quests = json::object();
            for (const auto &[id, q] : d.quests)
                quests[id] = {{"state", rpg::ToString(q.state)}, {"objective", q.objective}, {"count", q.count}, {"rewarded", q.rewarded}};
            json extras = json::object();
            for (const auto &[name, text] : d.extras)
                extras[name] = json::parse(text, nullptr, false);
            return {
                {"extras", extras},
                {"scene", d.scene},
                {"feet", {d.feet.x, d.feet.y, d.feet.z}},
                {"yaw", d.yaw},
                {"mode", d.mode == CombatMode::Turn ? "turn" : "realtime"},
                {"hero",
                 {{"id", h.id}, {"actor", h.actor}, {"health", h.health}, {"energy", h.energy}, {"level", h.level}, {"xp", h.xp},
                  {"skillPoints", h.skillPoints}, {"skills", h.skills}, {"inventorySlots", h.inventorySlots},
                  {"inventory", Stacks(h.inventory)}, {"pending", Stacks(h.pending)}, {"equipment", equipment}}},
                {"quests", quests},
                {"hotkeys", d.hotkeys},
                {"hour", d.hour},
            };
        }

        // Reads the payload; any missing or wrong field is an error (the checksum already passed,
        // so this mostly catches saves from older/other builds and missing content).
        bool FromJson(const json &p, const rpg::Content &content, SaveData &d, std::vector<core::Diagnostic> &out,
                      const std::string &file)
        {
            const auto fail = [&](const std::string &code, const std::string &message) {
                out.push_back({core::Severity::Error, code, file, "", message});
                return false;
            };
            try
            {
                // nlohmann throws on a wrong type; the try turns that into one clear error.
                d.scene = p.at("scene").get<std::string>();
                d.feet = {p.at("feet").at(0).get<float>(), p.at("feet").at(1).get<float>(), p.at("feet").at(2).get<float>()};
                d.yaw = p.at("yaw").get<float>();
                d.mode = AvailableMode(p.at("mode").get<std::string>() == "realtime" ? CombatMode::Realtime : CombatMode::Turn);
                const json &h = p.at("hero");
                rpg::Character &c = d.hero;
                c.id = h.at("id").get<std::string>();
                c.actor = h.at("actor").get<std::string>();
                c.health = h.at("health").get<int>();
                c.energy = h.at("energy").get<int>();
                c.level = h.at("level").get<int>();
                c.xp = h.at("xp").get<int>();
                c.skillPoints = h.at("skillPoints").get<int>();
                c.skills = h.at("skills").get<std::set<std::string>>();
                c.inventorySlots = h.at("inventorySlots").get<int>();
                for (const json &s : h.at("inventory"))
                    c.inventory.push_back({s.at("item").get<std::string>(), s.at("count").get<int>()});
                for (const json &s : h.at("pending"))
                    c.pending.push_back({s.at("item").get<std::string>(), s.at("count").get<int>()});
                for (const auto &[slot, item] : h.at("equipment").items())
                    c.equipment[slot == "weapon" ? rpg::Slot::Weapon : rpg::Slot::Armor] = item.get<std::string>();
                for (const auto &[id, q] : p.at("quests").items())
                {
                    const std::string state = q.at("state").get<std::string>();
                    story::QuestProgress progress;
                    progress.state = state == "active"      ? rpg::QuestState::Active
                                     : state == "completed" ? rpg::QuestState::Completed
                                     : state == "failed"    ? rpg::QuestState::Failed
                                                            : rpg::QuestState::NotStarted;
                    progress.objective = q.at("objective").get<std::size_t>();
                    progress.count = q.at("count").get<int>();
                    progress.rewarded = q.at("rewarded").get<bool>();
                    d.quests[id] = progress;
                }
            }
            catch (const json::exception &e)
            {
                return fail("save.field", std::string("missing or wrong field: ") + e.what());
            }
            // Hotkeys are a preference: older saves have none, and a key whose skill or item is gone is
            // cleared instead of refusing the save.
            if (const auto it = p.find("hotkeys"); it != p.end() && it->is_array())
                for (const json &key : *it)
                {
                    const std::string text = key.is_string() ? key.get<std::string>() : "";
                    const bool known = (text.starts_with("skill:") && content.FindSkill(text.substr(6))) || (text.starts_with("item:") && content.FindItem(text.substr(5)));
                    d.hotkeys.push_back(known ? text : "");
                }

            if (const auto it = p.find("extras"); it != p.end() && it->is_object())
                for (const auto &[name, part] : it->items())
                    d.extras[name] = part.dump();

            // The time of day is optional too (older saves have none); a bad value is just dropped.
            if (const auto it = p.find("hour"); it != p.end() && it->is_number())
                if (const float hour = it->get<float>(); hour >= 0.0F && hour < 24.0F)
                    d.hour = hour;

            // Every id must still exist in the content: a save from content that changed since
            // (renamed item, removed quest) is refused instead of loading half a world.
            const auto missing = [&](const std::string &what) { return fail("save.content.missing", what + " no longer exists in the game content"); };
            if (!content.FindActor(d.hero.actor))
                return missing("actor '" + d.hero.actor + "'");
            for (const std::string &skill : d.hero.skills)
                if (!content.FindSkill(skill))
                    return missing("skill '" + skill + "'");
            for (const auto *list : {&d.hero.inventory, &d.hero.pending})
                for (const rpg::ItemStack &s : *list)
                    if (!content.FindItem(s.item) || s.count < 1)
                        return missing("item '" + s.item + "'");
            for (const auto &[slot, item] : d.hero.equipment)
                if (!content.FindItem(item))
                    return missing("item '" + item + "'");
            for (const auto &[id, q] : d.quests)
                if (!content.FindQuest(id) || q.objective > content.FindQuest(id)->objectives.size())
                    return missing("quest '" + id + "'");
            if (!std::isfinite(d.feet.x) || !std::isfinite(d.feet.y) || !std::isfinite(d.feet.z) || d.hero.level < 1)
                return fail("save.field", "position or level out of range");
            return true;
        }

        bool ReadText(const std::filesystem::path &file, std::string &text)
        {
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return false;
            text.assign(std::istreambuf_iterator<char>(in), {});
            return true;
        }
    }

    bool ReadSave(const std::filesystem::path &file, const rpg::Content &content, SaveData &data,
                  std::vector<core::Diagnostic> &diagnostics)
    {
        const std::string name = file.string();
        const auto fail = [&](const std::string &code, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, code, name, "", message});
            return false;
        };

        // 1. Envelope: {"format":"umbra.save","version":1,"checksum":"...","payload":"<json text>"}.
        std::string text;
        if (!ReadText(file, text))
            return fail("save.missing", "no save file");
        const json root = json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object() || root.value("format", "") != "umbra.save")
            return fail("save.corrupt", "not an Umbra save (damaged or another file)");
        const int fileVersion = root.value("version", 0);
        if (fileVersion > version)
            return fail("save.newer", "made by a newer version of the game (version " + std::to_string(fileVersion) + ")");
        if (fileVersion < 1)
            return fail("save.corrupt", "unknown save version");
        // (Older versions would be migrated here; version 1 is the first.)

        // 2. The payload must match its checksum exactly.
        if (!root.contains("payload") || !root["payload"].is_string() || !root.contains("checksum"))
            return fail("save.corrupt", "save is incomplete");
        const std::string payload = root["payload"].get<std::string>();
        if (root.value("checksum", "") != Checksum(payload))
            return fail("save.corrupt", "checksum mismatch: the file was damaged or edited");
        const json p = json::parse(payload, nullptr, false);
        if (p.is_discarded() || !p.is_object())
            return fail("save.corrupt", "payload is not valid JSON");

        // 3. Fields and content references, into a fresh object.
        SaveData loaded;
        if (!FromJson(p, content, loaded, diagnostics, name))
            return false;
        data = std::move(loaded);
        return true;
    }

    bool WriteSave(const std::filesystem::path &file, const SaveData &data, std::vector<core::Diagnostic> &diagnostics)
    {
        const auto fail = [&](const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "save.write", file.string(), "", message});
            return false;
        };
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);

        // 1. Write the whole file next to the real one.
        const std::string payload = ToJson(data).dump();
        const json root{{"format", "umbra.save"}, {"version", version}, {"checksum", Checksum(payload)}, {"payload", payload}};
        std::filesystem::path temp = file;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out)
                return fail("cannot create " + temp.string() + " (read-only folder or disk full?)");
            out << root.dump(1) << '\n';
            if (!out.flush())
                return fail("writing " + temp.string() + " failed (disk full?)");
        }

        // 2. Read it back before trusting it (catches a disk that silently dropped data).
        std::string check;
        if (!ReadText(temp, check) || json::parse(check, nullptr, false) != root)
        {
            std::filesystem::remove(temp, ec);
            return fail("the written save did not read back correctly");
        }

        // 3. Keep the previous save as the backup, then put the new one in place.
        if (std::filesystem::exists(file))
        {
            std::filesystem::path backup = file;
            backup += ".bak";
            std::filesystem::copy_file(file, backup, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
                return fail("cannot keep a backup: " + ec.message());
        }
        std::filesystem::rename(temp, file, ec); // replaces the old file
        if (ec)
            return fail("cannot replace the save: " + ec.message());
        return true;
    }

    bool ReadSaveOrBackup(const std::filesystem::path &file, const rpg::Content &content, SaveData &data,
                          std::vector<core::Diagnostic> &diagnostics)
    {
        std::vector<core::Diagnostic> first;
        if (ReadSave(file, content, data, first))
            return true;
        std::filesystem::path backup = file;
        backup += ".bak";
        std::vector<core::Diagnostic> second;
        if (std::filesystem::exists(backup) && ReadSave(backup, content, data, second))
        {
            diagnostics.insert(diagnostics.end(), first.begin(), first.end());
            for (core::Diagnostic &d : diagnostics)
                d.severity = core::Severity::Warning; // recovered
            diagnostics.push_back({core::Severity::Warning, "save.backup.used", file.string(), "", "the save was damaged; loaded the previous one"});
            return true;
        }
        diagnostics.insert(diagnostics.end(), first.begin(), first.end());
        return false;
    }
}
