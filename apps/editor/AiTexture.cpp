#include "Editor.hpp"

#include <SceneEdits.hpp>
#include <TextureTools.hpp>

#include <imgui.h>

#include <cstdio>

// AI texture window (roadmap P8C.4). The creator describes a surface; a local ComfyUI makes a few
// variations; each is made seamless and shown on the selected object in the scene view, repeating
// at its real size. "Apply" writes the PNG and material file, then sets the object's material as
// one undoable edit. Rules in Editor.md.

using umbra::authoring::Change;

namespace
{
    void InputText(const char *label, std::string &value)
    {
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer)))
            value = buffer;
    }
}

void Editor::FindComfyUi()
{
    // Asks the usual local ports (or the typed address) which models ComfyUI has.
    textureProblems_.clear();
    if (textureHost_.empty())
        textureHost_ = umbra::localai::FindComfyUi(textureModels_);
    else
        textureModels_ = umbra::localai::ListCheckpoints(textureHost_, textureProblems_);
    if (textureHost_.empty())
        textureProblems_.push_back({umbra::core::Severity::Error, "ai.connect", "comfyui", "",
                                    "ComfyUI is not running on port 8000 (Desktop) or 8188 (command line). Start it, then press Find."});
    textureModel_ = umbra::localai::PreferredCheckpoint(textureModels_, textureModel_);
    textureSearched_ = true;
}

void Editor::ClearTexturePreviews()
{
    for (const umbra::core::TextureHandle handle : texturePreviews_)
        models_.DestroyPreviewTexture(handle);
    texturePreviews_.clear();
    textureCandidates_.clear();
    texturePick_ = -1;
}

void Editor::ApplyTexturePreview(umbra::core::DrawList &drawList) const
{
    // Draws the picked variation on the selected object instead of its own texture.
    if (texturePick_ < 0 || !world_ || selected_.empty())
        return;
    const umbra::world::Entity entity = world_->Find(selected_);
    const umbra::world::Renderable *renderable = world_->GetRenderable(entity);
    if (renderable == nullptr)
        return;
    for (umbra::core::DrawItem &item : drawList.items)
        if (item.mesh == renderable->meshHandle && item.model == *world_->GetWorldMatrix(entity))
        {
            item.texture = texturePreviews_[static_cast<std::size_t>(texturePick_)];
            item.metersPerRepeat = textureMeters_;
            item.color = renderable->color;
        }
}

void Editor::ApplyTexture()
{
    // 1. New files first: the image, then the material that points at it.
    std::vector<umbra::core::Diagnostic> diagnostics;
    const auto &candidate = textureCandidates_[static_cast<std::size_t>(texturePick_)];
    const std::string material = umbra::localai::SaveTextureMaterial(assetRoot_, textureName_.empty() ? textureIdea_ : textureName_, candidate,
                                                                     textureMeters_, diagnostics);
    if (material.empty())
    {
        Report(diagnostics, "Texture not saved");
        return;
    }
    ScanAssets();
    // 2. Then the object's material, as one undoable edit (undo keeps the files, so redo works).
    const std::optional<std::size_t> index = SelectedIndex();
    if (index && scene_->Data()["entities"][*index].contains("model"))
        SetField("/entities/" + std::to_string(*index) + "/material", material, "AI texture " + selected_);
    message_ = "Saved " + material + (index ? "" : " (select an object to use it)") + ".";
    messageSeconds_ = 6.0F;
    ClearTexturePreviews();
}

void Editor::AiTexture()
{
    if (!showTexture_)
        return;
    ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AI texture", &showTexture_))
    {
        ImGui::End();
        return;
    }
    if (!textureSearched_)
        FindComfyUi();

    // 1. Finished pictures: make each seamless and upload it for the preview.
    if (auto results = imageJob_->Take())
    {
        for (const umbra::localai::ImageResult &result : *results)
        {
            textureProblems_.insert(textureProblems_.end(), result.diagnostics.begin(), result.diagnostics.end());
            if (!result.ok)
                continue;
            const auto &request = textureRequests_[textureCandidates_.size()];
            textureCandidates_.push_back(umbra::localai::FinishTexture(result.image, request));
            texturePreviews_.push_back(models_.CreatePreviewTexture(textureCandidates_.back().image));
        }
        if (!textureCandidates_.empty())
            texturePick_ = 0;
    }

    // 2. The request.
    ImGui::TextWrapped("Describe a surface. A local image model (ComfyUI) makes it; it repeats without seams.");
    ImGui::TextUnformatted("Looks like");
    InputText("##idea", textureIdea_);
    InputText("File name (optional)", textureName_);
    ImGui::SliderFloat("Meters per repeat", &textureMeters_, 0.25F, 10.0F, "%.2f");
    ImGui::SliderInt("Variations", &textureCount_, 1, 4);
    if (ImGui::BeginCombo("Model", textureModel_.c_str()))
    {
        for (const std::string &model : textureModels_)
            if (ImGui::Selectable(model.c_str(), model == textureModel_))
                textureModel_ = model;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Find"))
        FindComfyUi();
    if (ImGui::CollapsingHeader("ComfyUI settings"))
    {
        InputText("Address", textureHost_);
        ImGui::TextWrapped("First time: install ComfyUI Desktop, put sd_xl_base_1.0.safetensors in its models/checkpoints folder, start it, press Find.");
        ImGui::TextWrapped("Every prompt gets the project style from assets/materials/style.json.");
    }

    const bool running = imageJob_->Running();
    if (running)
    {
        ImGui::Text("Generating %zu/%zu... %d s", imageJob_->Finished(), textureRequests_.size(), static_cast<int>(imageJob_->Seconds()));
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            imageJob_->Cancel();
    }
    else
    {
        ImGui::BeginDisabled(textureIdea_.empty() || textureModel_.empty() || textureHost_.empty());
        if (ImGui::Button("Generate"))
        {
            ClearTexturePreviews();
            textureProblems_.clear();
            textureRequests_ = umbra::localai::MakeTextureRequests(textureIdea_, umbra::localai::LoadTextureStyle(assetRoot_), textureModel_, textureHost_,
                                                                    umbra::localai::TextureSeeds(textureCount_));
            imageJob_->Start(textureRequests_);
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();

    // 3. Variations: pick one to see it on the selected object.
    for (std::size_t i = 0; i < textureCandidates_.size(); ++i)
    {
        const auto &candidate = textureCandidates_[i];
        char label[96];
        std::snprintf(label, sizeof(label), "Variation %zu  (seed %lld, seam %.1f%s)", i + 1, candidate.request.seed, candidate.seam,
                      candidate.seam > umbra::assets::visibleSeam ? " - may show" : "");
        if (ImGui::RadioButton(label, texturePick_ == static_cast<int>(i)))
            texturePick_ = static_cast<int>(i);
    }
    if (texturePick_ >= 0)
    {
        ImGui::TextWrapped(selected_.empty() ? "Select an object to see it there." : "Shown on '%s' in the scene view.", selected_.c_str());
        const auto license = umbra::assets::ModelLicense(textureModel_);
        if (!license.commercial)
            ImGui::TextColored(ImVec4(1, 0.8F, 0.3F, 1), "Note: %s", license.known ? (license.name + ": not for a game you sell.").c_str()
                                                                                 : "this model's license is unknown; check it before shipping.");
        if (ImGui::Button(selected_.empty() ? "Save material" : "Apply to object"))
            ApplyTexture();
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
            ClearTexturePreviews();
    }
    for (const umbra::core::Diagnostic &d : textureProblems_)
        ImGui::TextColored(d.severity == umbra::core::Severity::Error ? ImVec4(1, 0.4F, 0.4F, 1) : ImVec4(1, 0.8F, 0.3F, 1), "%s", d.message.c_str());
    ImGui::End();
}
