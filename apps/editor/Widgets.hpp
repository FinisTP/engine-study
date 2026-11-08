#pragma once

#include <Document.hpp>

#include <glm/vec3.hpp>
#include <imgui.h>

#include <cstdio>
#include <string>
#include <vector>

// Small ImGui helpers shared by the editor's panels.
namespace widgets
{
    using umbra::authoring::Json;

    // Reads [x, y, z] (or [x, y, z, w]) from an entity field, with a default.
    inline glm::vec3 Vec3(const Json &entity, const char *key, glm::vec3 fallback)
    {
        const auto it = entity.find(key);
        if (it == entity.end() || !it->is_array() || it->size() < 3)
            return fallback;
        return {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()};
    }

    // A text field that reports a change once, when the user finishes editing it.
    inline bool EditText(const char *label, const std::string &current, std::string &result)
    {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), "%s", current.c_str());
        ImGui::InputText(label, buffer, sizeof(buffer));
        if (!ImGui::IsItemDeactivatedAfterEdit())
            return false;
        result = buffer;
        return true;
    }

    // A combo over `options`; returns true and sets `result` when the user picks one.
    inline bool Choose(const char *label, const std::string &current, const std::vector<std::string> &options, std::string &result)
    {
        bool picked = false;
        if (ImGui::BeginCombo(label, current.c_str()))
        {
            for (const std::string &option : options)
                if (ImGui::Selectable(option.c_str(), option == current))
                {
                    result = option;
                    picked = true;
                }
            ImGui::EndCombo();
        }
        return picked;
    }
}
