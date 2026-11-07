#include "Editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

// RPG content forms (roadmap P7.2). One tab per section: a list of entries on the left, the
// selected entry's fields on the right. Every field change is one validated, undoable edit of
// the RPG document; the field names are those of engine/rpg/include/Rpg.md and Story.md.

using umbra::authoring::Change;
using umbra::authoring::Json;

namespace
{
    const char *stats[] = {"maxHealth", "maxEnergy", "attack", "defense", "speed"};

    // A new entry that passes validation, for each section.
    Json Template(const std::string &section, const std::string &id, const std::string &npc)
    {
        if (section == "statuses")
            return {{"id", id}, {"name", "New status"}, {"turns", 2}};
        if (section == "skills")
            return {{"id", id}, {"name", "New skill"}, {"effects", Json::array({{{"kind", "damage"}, {"power", 5}}})}, {"modes", Json::array({"turn"})}};
        if (section == "items")
            return {{"id", id}, {"name", "New item"}};
        if (section == "actors")
            return {{"id", id}, {"name", "New actor"}, {"stats", {{"maxHealth", 20}}}};
        if (section == "quests")
            return {{"id", id}, {"name", "New quest"}, {"description", ""}, {"objectives", Json::array({{{"talk", npc}, {"text", "Talk to " + npc}}})}};
        return {{"id", id}, {"npc", npc}, {"start", "start"}, {"nodes", Json::array({{{"id", "start"}, {"speaker", "Someone"}, {"text", "Hello."}}})}};
    }

    // Text input whose change is reported once, when editing ends. `multiline` for long text.
    bool TextField(const char *label, const std::string &current, std::string &result, bool multiline = false)
    {
        char buffer[2048];
        std::snprintf(buffer, sizeof(buffer), "%s", current.c_str());
        if (multiline)
            ImGui::InputTextMultiline(label, buffer, sizeof(buffer), ImVec2(-1, 60));
        else
            ImGui::InputText(label, buffer, sizeof(buffer));
        if (!ImGui::IsItemDeactivatedAfterEdit())
            return false;
        result = buffer;
        return true;
    }

    bool Combo(const char *label, const std::string &current, const std::vector<std::string> &options, std::string &result)
    {
        bool picked = false;
        if (ImGui::BeginCombo(label, current.empty() ? "(none)" : current.c_str()))
        {
            for (const std::string &option : options)
                if (ImGui::Selectable(option.empty() ? "(none)" : option.c_str(), option == current))
                    result = option, picked = true;
            ImGui::EndCombo();
        }
        return picked;
    }
}

bool Editor::RpgEdit(const std::vector<Change> &changes, const std::string &label, bool merge)
{
    return Edit(*rpg_, changes, label, merge);
}

void Editor::RpgForms()
{
    if (!showRpg_)
        return;
    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("RPG content", &showRpg_) || !rpg_->IsOpen())
    {
        if (!rpg_->IsOpen())
            ImGui::TextWrapped("assets/rpg/reference.rpg.json did not open; see Diagnostics.");
        ImGui::End();
        return;
    }

    // 1. Section tabs.
    if (ImGui::BeginTabBar("sections"))
    {
        for (const char *section : {"items", "skills", "statuses", "actors", "quests", "conversations"})
            if (ImGui::BeginTabItem(section))
            {
                if (rpgSection_ != section)
                    rpgSection_ = section, rpgSelected_ = 0;
                ImGui::EndTabItem();
            }
        ImGui::EndTabBar();
    }
    const Json entries = rpg_->Data().value(rpgSection_, Json::array()); // copy: edits replace the document
    const std::vector<std::string> npcs = [&] {
        std::vector<std::string> list;
        for (const Json &c : rpg_->Data().value("conversations", Json::array()))
            list.push_back(c.value("npc", ""));
        return list;
    }();

    // 2. Entry list with add / duplicate / delete. Deleting something still referenced is refused.
    ImGui::BeginChild("list", ImVec2(200, 0), ImGuiChildFlags_Borders);
    const std::string base = rpgSection_.substr(0, rpgSection_.size() - (rpgSection_ == "statuses" ? 2 : 1));
    const auto uniqueId = [&](std::string id) {
        const std::vector<std::string> used = RpgIds(rpgSection_.c_str());
        for (int n = 2; std::find(used.begin(), used.end(), id) != used.end(); ++n)
            id = "new_" + base + "_" + std::to_string(n);
        return id;
    };
    if (ImGui::SmallButton("Add"))
    {
        if (RpgEdit({{Change::Op::Insert, "/" + rpgSection_ + "/-", Template(rpgSection_, uniqueId("new_" + base), npcs.empty() ? "npc" : npcs[0])}}, "Add " + base))
            rpgSelected_ = static_cast<int>(entries.size());
    }
    const bool valid = rpgSelected_ >= 0 && rpgSelected_ < static_cast<int>(entries.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Duplicate") && valid)
    {
        Json copy = entries[static_cast<std::size_t>(rpgSelected_)];
        copy["id"] = uniqueId(copy.value("id", base) + "_copy");
        if (RpgEdit({{Change::Op::Insert, "/" + rpgSection_ + "/-", copy}}, "Duplicate " + base))
            rpgSelected_ = static_cast<int>(entries.size());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete") && valid)
        RpgEdit({{Change::Op::Remove, "/" + rpgSection_ + "/" + std::to_string(rpgSelected_), {}}}, "Delete " + base);
    ImGui::Separator();
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (ImGui::Selectable(entries[i].value("id", "?").c_str(), static_cast<int>(i) == rpgSelected_))
            rpgSelected_ = static_cast<int>(i);
    ImGui::EndChild();
    ImGui::SameLine();

    // 3. The selected entry's form.
    ImGui::BeginChild("form", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (valid)
        RpgEntryForm("/" + rpgSection_ + "/" + std::to_string(rpgSelected_), entries[static_cast<std::size_t>(rpgSelected_)], npcs);
    ImGui::EndChild();
    ImGui::End();
}

void Editor::RpgEntryForm(const std::string &at, const Json &entry, const std::vector<std::string> &npcs)
{
    const std::string label = "Edit " + entry.value("id", "");
    std::string text;

    // --- Small field helpers bound to this entry (paths are relative to `at`) ---
    const auto set = [&](const std::string &path, const Json &value, bool merge = false) {
        return RpgEdit({{Change::Op::Set, at + path, value}}, label, merge);
    };
    const auto remove = [&](const std::string &path) { return RpgEdit({{Change::Op::Remove, at + path, {}}}, label); };
    // Numbers: holding +/- or typing digits is one undo step until the field is left.
    const auto number = [&](bool changed, const std::string &path, const Json &value) {
        if (ImGui::IsItemActivated())
            newStep_ = true;
        if (changed)
            set(path, value, !newStep_), newStep_ = false;
    };
    const auto textField = [&](const char *name, const std::string &key, bool multiline = false) {
        if (TextField(name, entry.value(key, ""), text, multiline))
            set("/" + key, text);
    };
    const auto intField = [&](const char *name, const Json &object, const std::string &path, int fallback) {
        int value = object.is_number() ? object.get<int>() : fallback;
        const bool changed = ImGui::InputInt(name, &value);
        number(changed, path, value);
    };
    const auto floatField = [&](const char *name, const Json &object, const std::string &path, float fallback) {
        float value = object.is_number() ? object.get<float>() : fallback;
        const bool changed = ImGui::InputFloat(name, &value, 0.1F, 1.0F, "%.2f");
        number(changed, path, value);
    };
    // A list of ids from another section, as checkboxes (skills, equipment, requires).
    const auto idSet = [&](const char *name, const std::string &key, const char *section) {
        if (!ImGui::TreeNode(name))
            return;
        const Json current = entry.value(key, Json::array());
        for (const std::string &id : RpgIds(section))
        {
            bool in = std::find(current.begin(), current.end(), id) != current.end();
            if (ImGui::Checkbox(id.c_str(), &in))
            {
                Json next = Json::array();
                for (const std::string &other : RpgIds(section))
                    if (other == id ? in : std::find(current.begin(), current.end(), other) != current.end())
                        next.push_back(other);
                if (next.empty())
                    remove("/" + key);
                else
                    set("/" + key, next);
            }
        }
        ImGui::TreePop();
    };
    // Rows of an array of objects: each row drawn by `row`, an "x" removes it, "Add" appends `fresh`.
    const auto rows = [&](const char *name, const std::string &key, const Json &fresh, auto row) {
        if (!ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen))
            return;
        const Json list = entry.value(key, Json::array());
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            row("/" + key + "/" + std::to_string(i), list[i]);
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                remove("/" + key + "/" + std::to_string(i));
            ImGui::PopID();
        }
        if (ImGui::SmallButton("Add"))
            set("/" + key, [&] { Json next = list; next.push_back(fresh); return next; }());
        ImGui::TreePop();
    };
    // Raw JSON for rarely edited nested values (dialogue conditions and actions).
    const auto jsonField = [&](const char *name, const std::string &path, const Json &value) {
        if (!TextField(name, value.is_null() ? "" : value.dump(), text))
            return;
        if (text.empty())
            return void(remove(path));
        const Json parsed = Json::parse(text, nullptr, false);
        if (parsed.is_discarded())
            Report({{umbra::core::Severity::Error, "json.parse", rpg_->File().string(), at + path, "not valid JSON"}}, "Edit refused");
        else
            set(path, parsed);
    };
    // One effect / modifier / item stack as a row of widgets.
    const auto effectRow = [&](const std::string &path, const Json &e) {
        ImGui::SetNextItemWidth(90);
        if (Combo("##kind", e.value("kind", "damage"), {"damage", "heal", "energy", "status"}, text))
            set(path + "/kind", text);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        intField("power", e.value("power", Json()), path + "/power", 0);
        if (e.value("kind", "") == "status")
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110);
            if (Combo("##status", e.value("status", ""), RpgIds("statuses"), text))
                set(path + "/status", text);
        }
    };
    const auto modifierRow = [&](const std::string &path, const Json &m) {
        const bool percent = m.contains("percent");
        ImGui::SetNextItemWidth(100);
        if (Combo("##stat", m.value("stat", "attack"), {std::begin(stats), std::end(stats)}, text))
            set(path + "/stat", text);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (Combo("##how", percent ? "percent" : "add", {"add", "percent"}, text) && (text == "percent") != percent)
            set(path, Json{{"stat", m.value("stat", "attack")}, {text, m.value(percent ? "percent" : "add", 0)}});
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        intField("##amount", m.value(percent ? "percent" : "add", Json()), path + (percent ? "/percent" : "/add"), 0);
    };
    const auto stackRow = [&](const std::string &path, const Json &s) {
        ImGui::SetNextItemWidth(140);
        if (Combo("##item", s.value("item", ""), RpgIds("items"), text))
            set(path + "/item", text);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        intField("count", s.value("count", Json()), path + "/count", 1);
    };

    // --- Common: id and name ---
    if (TextField("Id", entry.value("id", ""), text))
        set("/id", text); // refused while other content still refers to the old id
    if (rpgSection_ != "conversations")
        textField("Name", "name");

    if (rpgSection_ == "items")
    {
        if (Combo("Slot", entry.value("slot", ""), {"", "weapon", "armor"}, text))
            text.empty() ? remove("/slot") : set("/slot", text);
        intField("Stack", entry.value("stack", Json()), "/stack", 1);
        rows("Modifiers (while equipped)", "modifiers", Json{{"stat", "attack"}, {"add", 1}}, modifierRow);
        rows("Use effects (consumes one)", "use", Json{{"kind", "heal"}, {"power", 10}}, effectRow);
    }
    else if (rpgSection_ == "skills")
    {
        intField("Energy cost", entry.value("cost", Json()), "/cost", 0);
        if (Combo("Target", entry.value("target", "enemy"), {"enemy", "self"}, text))
            set("/target", text);
        const Json modes = entry.value("modes", Json::array());
        for (const char *mode : {"turn", "realtime"})
        {
            bool on = std::find(modes.begin(), modes.end(), mode) != modes.end();
            ImGui::SameLine();
            if (ImGui::Checkbox(mode, &on))
            {
                Json next = Json::array();
                for (const char *m : {"turn", "realtime"})
                    if (std::string{m} == mode ? on : std::find(modes.begin(), modes.end(), m) != modes.end())
                        next.push_back(m);
                // Real-time use needs timing: add sensible defaults with the mode.
                std::vector<Change> changes{{Change::Op::Set, at + "/modes", next}};
                if (on && std::string{mode} == "realtime" && !entry.contains("realtime"))
                    changes.push_back({Change::Op::Set, at + "/realtime",
                                       Json{{"windup", 0.3}, {"active", 0.2}, {"recovery", 0.3}, {"cooldown", 1.0}, {"range", 1.8}, {"arc", 90}}});
                RpgEdit(changes, label);
            }
        }
        rows("Effects", "effects", Json{{"kind", "damage"}, {"power", 5}}, effectRow);
        if (entry.contains("realtime") && ImGui::TreeNode("Real-time timing (seconds, meters, degrees)"))
        {
            const Json timing = entry["realtime"];
            for (const char *key : {"windup", "active", "recovery", "cooldown", "range", "arc"})
                floatField(key, timing.value(key, Json()), std::string{"/realtime/"} + key, 0.0F);
            ImGui::TreePop();
        }
        intField("Skill points", entry.value("points", Json()), "/points", 0);
        idSet("Requires", "requires", "skills");
    }
    else if (rpgSection_ == "statuses")
    {
        intField("Turns", entry.value("turns", Json()), "/turns", 0);
        floatField("Seconds", entry.value("seconds", Json()), "/seconds", 0.0F);
        intField("Damage per tick", entry.value("damagePerTick", Json()), "/damagePerTick", 0);
        rows("Modifiers", "modifiers", Json{{"stat", "defense"}, {"percent", 50}}, modifierRow);
    }
    else if (rpgSection_ == "actors")
    {
        for (const char *group : {"stats", "growth"})
            if (ImGui::TreeNodeEx(group, ImGuiTreeNodeFlags_DefaultOpen))
            {
                const Json values = entry.value(group, Json::object());
                for (const char *stat : stats)
                    intField(stat, values.value(stat, Json()), std::string{"/"} + group + "/" + stat, 0);
                ImGui::TreePop();
            }
        idSet("Skills at start", "skills", "skills");
        idSet("Equipment at start", "equipment", "items");
        rows("Inventory at start", "inventory", Json{{"item", RpgIds("items").empty() ? "" : RpgIds("items")[0]}, {"count", 1}}, stackRow);
        intField("Inventory slots", entry.value("inventorySlots", Json()), "/inventorySlots", 12);
        intField("Skill points", entry.value("skillPoints", Json()), "/skillPoints", 0);
        intField("XP when defeated", entry.value("xp", Json()), "/xp", 0);
        // Look in the world: model, tint and half-height (the game draws enemies with these).
        if (ImGui::TreeNodeEx("Look", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::vector<std::string> models = assets_["Models"];
            models.insert(models.begin(), "");
            if (Combo("Model", entry.value("model", ""), models, text))
                text.empty() ? remove("/model") : set("/model", text);
            floatField("Size (half-height, m)", entry.value("size", Json()), "/size", 0.8F);
            float color[4] = {1, 1, 1, 1};
            if (const Json c = entry.value("color", Json()); c.is_array() && c.size() == 4)
                for (int i = 0; i < 4; ++i)
                    color[i] = c[static_cast<std::size_t>(i)].get<float>();
            const bool changed = ImGui::ColorEdit4("Tint", color);
            number(changed, "/color", Json::array({color[0], color[1], color[2], color[3]}));
            ImGui::TreePop();
        }
    }
    else if (rpgSection_ == "quests")
    {
        textField("Description", "description", true);
        rows("Objectives (in order)", "objectives", Json{{"talk", npcs.empty() ? "npc" : npcs[0]}, {"text", "Talk"}}, [&](const std::string &path, const Json &o) {
            // An objective is {"defeat"|"obtain"|"talk": target, "count", "text"}.
            const std::string kind = o.contains("defeat") ? "defeat" : o.contains("obtain") ? "obtain" : "talk";
            ImGui::SetNextItemWidth(80);
            if (Combo("##kind", kind, {"defeat", "obtain", "talk"}, text) && text != kind)
            {
                // A new kind needs a target of that kind: start with the first one.
                const std::vector<std::string> first = text == "defeat" ? RpgIds("actors") : text == "obtain" ? RpgIds("items") : npcs;
                set(path, Json{{text, first.empty() ? "" : first[0]}, {"text", o.value("text", "")}});
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130);
            const std::vector<std::string> targets = kind == "defeat" ? RpgIds("actors") : kind == "obtain" ? RpgIds("items") : npcs;
            if (Combo("##target", o.value(kind, ""), targets, text))
                set(path + "/" + kind, text);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            intField("##count", o.value("count", Json()), path + "/count", 1);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220);
            if (TextField("##text", o.value("text", ""), text))
                set(path + "/text", text);
        });
        const Json failOn = entry.value("failOn", Json::array());
        bool onDeath = std::find(failOn.begin(), failOn.end(), "death") != failOn.end();
        if (ImGui::Checkbox("Fails when the player dies", &onDeath))
            onDeath ? set("/failOn", Json::array({"death"})) : remove("/failOn");
        if (ImGui::TreeNodeEx("Reward", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const Json reward = entry.value("reward", Json::object());
            intField("XP", reward.value("xp", Json()), "/reward/xp", 0);
            intField("Skill points##reward", reward.value("skillPoints", Json()), "/reward/skillPoints", 0);
            Json items = reward.value("items", Json::array());
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                stackRow("/reward/items/" + std::to_string(i), items[i]);
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                    remove("/reward/items/" + std::to_string(i));
                ImGui::PopID();
            }
            if (ImGui::SmallButton("Add reward item") && !RpgIds("items").empty())
            {
                items.push_back({{"item", RpgIds("items")[0]}, {"count", 1}});
                set("/reward", [&] { Json r = reward; r["items"] = items; return r; }());
            }
            ImGui::TreePop();
        }
    }
    else // conversations
    {
        textField("NPC id", "npc");
        const Json nodes = entry.value("nodes", Json::array());
        std::vector<std::string> nodeIds{""};
        for (const Json &n : nodes)
            nodeIds.push_back(n.value("id", ""));
        if (Combo("Start node", entry.value("start", ""), {nodeIds.begin() + 1, nodeIds.end()}, text))
            set("/start", text);
        ImGui::TextDisabled("Choices: 'next' empty ends the talk. 'if' and 'do' are JSON lists (Story.md).");
        for (std::size_t n = 0; n < nodes.size(); ++n)
        {
            const Json &node = nodes[n];
            const std::string np = "/nodes/" + std::to_string(n);
            ImGui::PushID(static_cast<int>(n));
            if (ImGui::CollapsingHeader((node.value("id", "?") + " - " + node.value("speaker", "")).c_str()))
            {
                if (TextField("Node id", node.value("id", ""), text))
                    set(np + "/id", text);
                if (TextField("Speaker", node.value("speaker", ""), text))
                    set(np + "/speaker", text);
                if (TextField("Line", node.value("text", ""), text, true))
                    set(np + "/text", text);
                const Json choices = node.value("choices", Json::array());
                for (std::size_t c = 0; c < choices.size(); ++c)
                {
                    const std::string cp = np + "/choices/" + std::to_string(c);
                    ImGui::PushID(static_cast<int>(c));
                    ImGui::Separator();
                    if (TextField("Choice", choices[c].value("text", ""), text))
                        set(cp + "/text", text);
                    if (Combo("Next", choices[c].value("next", ""), nodeIds, text))
                        set(cp + "/next", text);
                    jsonField("If", cp + "/if", choices[c].value("if", Json()));
                    jsonField("Do", cp + "/do", choices[c].value("do", Json()));
                    if (ImGui::SmallButton("Remove choice"))
                        remove(cp);
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("Add choice"))
                    set(np + "/choices", [&] { Json next = choices; next.push_back({{"text", "..."}, {"next", ""}}); return next; }());
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove node"))
                    remove(np);
            }
            ImGui::PopID();
        }
        if (ImGui::SmallButton("Add node"))
        {
            std::string id = "node";
            for (int i = 2; std::find(nodeIds.begin(), nodeIds.end(), id) != nodeIds.end(); ++i)
                id = "node_" + std::to_string(i);
            set("/nodes", [&] { Json next = nodes; next.push_back({{"id", id}, {"speaker", ""}, {"text", "..."}}); return next; }());
        }
    }
}
