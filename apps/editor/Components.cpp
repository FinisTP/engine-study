#include "Editor.hpp"
#include "Widgets.hpp"

#include <SceneEdits.hpp>

#include <imgui.h>

#include <algorithm>

// Inspector components (Components.md in engine/scene/include): the entity's "components" object
// drawn one header each, a remove button, and an "Add component" menu listing the built-in
// components and the project's own (assets/components/*.component.json).

using umbra::authoring::Change;
using umbra::authoring::Json;
using widgets::Choose;
using widgets::EditText;
using widgets::Vec3;

const Json *Editor::ComponentOf(const Json &entity, const std::string &type)
{
    if (const auto c = entity.find("components"); c != entity.end() && c->is_object() && c->contains(type))
        return &(*c)[type];
    const auto legacy = entity.find(type); // files from before components
    return legacy != entity.end() ? &*legacy : nullptr;
}

void Editor::MoveLegacyParts()
{
    // Older scenes keep collider/interactable/portal/spawner on the entity: move them into
    // "components" as one undoable step (the file changes when you save).
    std::vector<Change> changes;
    const Json &entities = scene_->Data()["entities"];
    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        Json components = entities[i].value("components", Json::object());
        std::vector<Change> remove;
        for (const std::string &type : umbra::scene::builtInComponents)
            if (entities[i].contains(type))
            {
                components[type] = entities[i][type];
                remove.push_back({Change::Op::Remove, umbra::authoring::EntityPath(i) + "/" + type, {}});
            }
        if (remove.empty())
            continue;
        changes.push_back({Change::Op::Set, umbra::authoring::EntityPath(i) + "/components", components});
        changes.insert(changes.end(), remove.begin(), remove.end());
    }
    if (!changes.empty() && Edit(*scene_, changes, "Move parts into components"))
    {
        message_ = "Collider, interactable, portal and spawner are now components; save to update the file.";
        messageSeconds_ = 5.0F;
    }
}

void Editor::ComponentsPanel(const Json &entity, const std::string &at)
{
    const Json none = Json::object();
    const Json &components = entity.contains("components") && entity["components"].is_object() ? entity["components"] : none;
    const std::string path = at + "/components";

    // 1. One header per component; "x" removes it.
    std::string removed;
    for (const auto &[type, value] : components.items())
    {
        ImGui::PushID(type.c_str());
        const auto schema = schemas_.find(type);
        const std::string label = schema != schemas_.end() ? schema->second.label : type;
        const bool open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 20.0F);
        if (ImGui::SmallButton("x"))
            removed = type;
        if (open)
            ComponentFields(type, value, path + "/" + type, entity);
        ImGui::PopID();
    }
    if (!removed.empty())
        Edit(*scene_, {{Change::Op::Remove, path + "/" + removed, {}}}, "Remove " + removed + " " + selected_);

    // 2. Add: built-in components first, then the project's own. Each type once per entity.
    ImGui::Spacing();
    if (ImGui::Button("Add component"))
        ImGui::OpenPopup("add-component");
    if (!ImGui::BeginPopup("add-component"))
        return;
    const auto add = [&](const std::string &type, const Json &defaults) {
        Json next = components;
        next[type] = defaults;
        Edit(*scene_, {{Change::Op::Set, path, next}}, "Add " + type + " " + selected_);
    };
    const std::vector<std::string> actors = RpgIds("actors");
    const Json builtIn[] = {FitCollider(entity).value_or(Json::object()), Json{{"prompt", "Use"}},
                            Json{{"scene", "scenes/settlement.scene.json"}, {"spawn", "player"}},
                            Json{{"pool", Json::array({actors.empty() ? "wolf" : actors.back()})}, {"count", 1}}};
    for (std::size_t i = 0; i < umbra::scene::builtInComponents.size(); ++i)
    {
        const std::string &type = umbra::scene::builtInComponents[i];
        if (ImGui::MenuItem(type.c_str(), nullptr, false, !components.contains(type)))
            add(type, builtIn[i]);
    }
    ImGui::Separator();
    if (schemas_.empty())
        ImGui::TextDisabled("No project components (assets/components)");
    for (const auto &[type, schema] : schemas_)
    {
        if (ImGui::MenuItem(schema.label.c_str(), nullptr, false, !components.contains(type)))
            add(type, schema.Defaults());
        if (!schema.description.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", schema.description.c_str());
    }
    ImGui::EndPopup();
}

void Editor::ComponentFields(const std::string &type, const Json &c, const std::string &at, const Json &entity)
{
    std::string text;
    const std::string label = type + " " + selected_;
    // One undo step per drag (drags send many edits).
    const auto drag = [&](const std::string &key, const Json &value) {
        if (ImGui::IsItemActivated())
            newStep_ = true;
        SetField(at + "/" + key, value, label, !newStep_);
        newStep_ = false;
    };

    if (type == "collider")
    {
        if (Choose("Shape", c.value("shape", "box"), {"box", "sphere", "capsule"}, text))
            SetField(at + "/shape", text, label);
        if (Choose("Motion", c.value("motion", "static"), {"static", "kinematic", "dynamic"}, text))
            SetField(at + "/motion", text, label);
        // Sizes in meters (green outline); 0 = from the entity's scale.
        const std::string shape = c.value("shape", "box");
        if (shape == "box")
        {
            glm::vec3 half = Vec3(c, "halfExtents", glm::vec3{0.0F});
            if (ImGui::DragFloat3("Half size", &half.x, 0.01F, 0.0F, 1000.0F))
                drag("halfExtents", Json::array({half.x, half.y, half.z}));
        }
        float radius = c.value("radius", 0.0F), halfHeight = c.value("halfHeight", 0.0F);
        if (shape != "box" && ImGui::DragFloat("Radius", &radius, 0.01F, 0.0F, 1000.0F))
            drag("radius", radius);
        if (shape == "capsule" && ImGui::DragFloat("Half height", &halfHeight, 0.01F, 0.0F, 1000.0F))
            drag("halfHeight", halfHeight);
        if (const auto fitted = FitCollider(entity); fitted && ImGui::Button("Fit to model"))
            SetField(at, *fitted, "Fit collider " + selected_);
        ImGui::SameLine();
        ImGui::TextDisabled("(fit again after scaling)");
    }
    else if (type == "interactable")
    {
        if (EditText("Prompt", c.value("prompt", ""), text))
            SetField(at + "/prompt", text, label);
        if (EditText("Message", c.value("message", ""), text))
            SetField(at + "/message", text, label);
        if (Choose("Sound", c.value("sound", ""), assets_["Sounds"], text))
            SetField(at + "/sound", text, label);
        std::vector<std::string> talks = RpgIds("conversations");
        talks.insert(talks.begin(), "");
        if (Choose("Conversation", c.value("conversation", ""), talks, text))
        {
            if (text.empty())
                Edit(*scene_, {{Change::Op::Remove, at + "/conversation", {}}}, label);
            else
                SetField(at + "/conversation", text, label);
        }
        bool save = c.value("save", false);
        if (ImGui::Checkbox("Save stone", &save))
            SetField(at + "/save", save, label);
    }
    else if (type == "portal")
    {
        if (Choose("Scene", c.value("scene", ""), assets_["Scenes"], text))
            SetField(at + "/scene", text, label);
        if (EditText("Spawn", c.value("spawn", ""), text))
            SetField(at + "/spawn", text, label);
    }
    else if (type == "spawner")
    {
        // The pool: one checkbox per actor in the RPG content.
        const std::vector<std::string> actors = RpgIds("actors");
        const Json pool = c.value("pool", Json::array());
        for (const std::string &actor : actors)
        {
            bool in = std::find(pool.begin(), pool.end(), actor) != pool.end();
            if (ImGui::Checkbox(actor.c_str(), &in))
            {
                Json next = Json::array();
                for (const std::string &a : actors)
                    if (a == actor ? in : std::find(pool.begin(), pool.end(), a) != pool.end())
                        next.push_back(a);
                SetField(at + "/pool", next, label);
            }
        }
        int count = c.value("count", 1), group = c.value("group", 3);
        float radius = c.value("radius", 3.0F);
        bool chase = c.value("chase", false);
        if (ImGui::InputInt("Count", &count))
            SetField(at + "/count", count, label);
        if (ImGui::InputInt("Group", &group))
            SetField(at + "/group", group, label);
        if (ImGui::InputFloat("Radius", &radius, 0.5F))
            SetField(at + "/radius", radius, label);
        if (ImGui::Checkbox("Chases on sight", &chase))
            SetField(at + "/chase", chase, label);
    }
    else if (const auto schema = schemas_.find(type); schema != schemas_.end())
    {
        // A project component: one widget per field kind in its schema.
        if (!schema->second.description.empty())
            ImGui::TextDisabled("%s", schema->second.description.c_str());
        for (const umbra::scene::ComponentField &f : schema->second.fields)
        {
            const Json value = c.contains(f.name) ? c[f.name] : Json(f.defaultValue);
            const char *name = f.label.c_str();
            if (f.kind == "number")
            {
                float v = value.is_number() ? value.get<float>() : 0.0F;
                if (ImGui::DragFloat(name, &v, 0.05F, static_cast<float>(f.min), static_cast<float>(f.max)))
                    drag(f.name, v);
            }
            else if (f.kind == "int")
            {
                int v = value.is_number() ? value.get<int>() : 0;
                if (ImGui::InputInt(name, &v))
                    SetField(at + "/" + f.name, std::clamp(v, static_cast<int>(f.min), static_cast<int>(f.max)), label);
            }
            else if (f.kind == "bool")
            {
                bool v = value.is_boolean() && value.get<bool>();
                if (ImGui::Checkbox(name, &v))
                    SetField(at + "/" + f.name, v, label);
            }
            else
            {
                // Text, or a pick from the project: assets (optionally under one folder), actors, scenes.
                const std::string current = value.is_string() ? value.get<std::string>() : "";
                std::vector<std::string> options;
                if (f.kind == "actor")
                    options = RpgIds("actors");
                else if (f.kind == "scene")
                    options = assets_["Scenes"];
                else if (f.kind == "asset")
                    for (const auto &[group, files] : assets_)
                        for (const std::string &file : files)
                            if (file.starts_with(f.folder))
                                options.push_back(file);
                if (f.kind == "string" ? EditText(name, current, text) : Choose(name, current, options, text))
                    SetField(at + "/" + f.name, text, label);
            }
        }
    }
    else
        ImGui::TextDisabled("No components/%s.component.json: fields can't be shown.", type.c_str());
}
