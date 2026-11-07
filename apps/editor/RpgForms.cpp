#include "Editor.hpp"

#include <imgui.h>

// RPG content forms: filled in by P7.2. For now a read-only list of what the content holds.
void Editor::RpgForms()
{
    if (!showRpg_)
        return;
    if (ImGui::Begin("RPG content", &showRpg_))
        for (const char *section : {"items", "skills", "actors", "quests", "conversations"})
            ImGui::Text("%s: %zu", section, RpgIds(section).size());
    ImGui::End();
}
