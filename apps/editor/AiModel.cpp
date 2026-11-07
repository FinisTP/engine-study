#include "Editor.hpp"

#include <Platform.hpp>
#include <TextureTools.hpp>

#include <SDL3/SDL_misc.h>
#include <imgui.h>

#include <cstdio>
#include <fstream>

// AI model window (roadmap P8D). Text -> a few concept pictures (SDXL, object style) -> pick one, or
// use your own picture -> the creator's exported image-to-3D workflow (TRELLIS.2) -> a new GLB in
// assets/models -> "Place in scene" adds it as one undoable edit. Rules in Editor.md.

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

    // Opens a file with the program Windows uses for it (the image viewer for PNGs).
    void OpenFile(const std::filesystem::path &file)
    {
        std::string url = "file:///" + file.generic_string();
        SDL_OpenURL(url.c_str());
    }
}

void Editor::StartModel()
{
    // 1. The workflow and the picture.
    modelProblems_.clear();
    umbra::localai::ModelRequest request;
    if (!umbra::localai::LoadModelWorkflow(modelWorkflow_, request.workflow, modelProblems_))
        return;
    std::string error;
    if (modelSource_ == 1)
    {
        if (!umbra::assets::LoadImageFile(modelPicturePath_, request.picture, error))
        {
            modelProblems_.push_back({umbra::core::Severity::Error, "file.read", modelPicturePath_, "", error});
            return;
        }
        modelSourceText_ = "file: " + std::filesystem::path(modelPicturePath_).filename().string();
        modelSeed_ = umbra::localai::TextureSeeds(1)[0];
    }
    else
    {
        const auto &picked = concepts_[static_cast<std::size_t>(conceptPick_)];
        request.picture = picked.image;
        modelSourceText_ = conceptRequests_[static_cast<std::size_t>(conceptPick_)].prompt;
        modelSeed_ = conceptRequests_[static_cast<std::size_t>(conceptPick_)].seed;
    }
    // 2. Run it in the background.
    request.host = textureHost_;
    request.seed = modelSeed_;
    request.faces = modelFaces_;
    modelPicture_ = request.picture;
    modelWorkflowData_ = request.workflow;
    modelJob_->Start({request});
}

void Editor::PlaceModel()
{
    // One undoable edit: a new entity standing on the ground at the camera's focus point.
    if (!scene_)
        return;
    const auto entity = umbra::localai::PlacedModelEntity(scene_->Data(), modelName_.empty() ? modelResult_ : modelName_, modelResult_, modelInfo_,
                                                          {camera_.target.x, 0.0F, camera_.target.z}, modelHeight_);
    if (Edit(*scene_, {{Change::Op::Insert, "/entities/-", entity}}, "AI model " + entity["id"].get<std::string>()))
        selected_ = entity["id"].get<std::string>();
}

void Editor::AiModel()
{
    if (!showModel_)
        return;
    ImGui::SetNextWindowSize(ImVec2(540, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AI model", &showModel_))
    {
        ImGui::End();
        return;
    }
    if (!textureSearched_)
        FindComfyUi();
    if (modelWorkflow_.empty())
        modelWorkflow_ = umbra::localai::DefaultModelWorkflow(assetRoot_).string();

    // 1. Finished jobs: concept pictures (saved for the image viewer) and models (saved as assets).
    if (auto results = conceptJob_->Take())
    {
        const std::filesystem::path folder = umbra::platform::Platform::PreferencesFolder() / "ai-concepts";
        std::filesystem::create_directories(folder);
        for (const auto &result : *results)
        {
            modelProblems_.insert(modelProblems_.end(), result.diagnostics.begin(), result.diagnostics.end());
            if (!result.ok)
                continue;
            const auto &request = conceptRequests_[concepts_.size()];
            concepts_.push_back({result.image, 0, 0, request});
            std::vector<std::uint8_t> png;
            umbra::assets::EncodePng(result.image, png);
            const std::filesystem::path file = folder / ("concept_" + std::to_string(request.seed) + ".png");
            std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
            conceptFiles_.push_back(file);
        }
        if (!concepts_.empty())
            conceptPick_ = 0;
    }
    if (auto results = modelJob_->Take())
    {
        const auto &result = (*results)[0];
        modelProblems_.insert(modelProblems_.end(), result.diagnostics.begin(), result.diagnostics.end());
        if (result.ok)
        {
            const std::string name = !modelName_.empty() ? modelName_ : modelSource_ == 0 ? modelIdea_ : std::filesystem::path(modelPicturePath_).stem().string();
            modelResult_ = umbra::localai::SaveGeneratedModel(assetRoot_, name, result.glb, modelPicture_,
                                                              {modelSourceText_, std::filesystem::path(modelWorkflow_).filename().string(), modelSeed_, modelFaces_},
                                                              modelWorkflowData_, modelProblems_);
            if (!modelResult_.empty() && umbra::localai::InspectModel(assetRoot_ / modelResult_, modelInfo_, modelProblems_))
                ScanAssets();
            else
                modelResult_.clear();
        }
    }

    // 2. Where the picture comes from.
    ImGui::TextWrapped("Make a prop from a description or a picture. ComfyUI on this computer turns the picture into a 3D model.");
    ImGui::RadioButton("Describe it", &modelSource_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Use my picture", &modelSource_, 1);
    const bool busy = conceptJob_->Running() || modelJob_->Running();
    if (modelSource_ == 0)
    {
        InputText("Looks like", modelIdea_);
        ImGui::SliderInt("Concept pictures", &conceptCount_, 1, 4);
        ImGui::BeginDisabled(busy || modelIdea_.empty() || textureModel_.empty() || textureHost_.empty());
        if (ImGui::Button("Make concept pictures"))
        {
            concepts_.clear();
            conceptFiles_.clear();
            conceptPick_ = -1;
            modelProblems_.clear();
            conceptRequests_ = umbra::localai::MakeTextureRequests(modelIdea_, umbra::localai::LoadObjectStyle(assetRoot_), textureModel_, textureHost_,
                                                                   umbra::localai::TextureSeeds(conceptCount_));
            conceptJob_->Start(conceptRequests_);
        }
        ImGui::EndDisabled();
        for (std::size_t i = 0; i < concepts_.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            char label[64];
            std::snprintf(label, sizeof(label), "Concept %zu (seed %lld)", i + 1, conceptRequests_[i].seed);
            if (ImGui::RadioButton(label, conceptPick_ == static_cast<int>(i)))
                conceptPick_ = static_cast<int>(i);
            ImGui::SameLine();
            if (ImGui::SmallButton("View"))
                OpenFile(conceptFiles_[i]);
            ImGui::PopID();
        }
    }
    else
        InputText("Picture file (PNG/JPEG)", modelPicturePath_);

    // 3. The 3D step.
    InputText("Name (optional)", modelName_);
    ImGui::SliderInt("Triangles", &modelFaces_, 1000, 50000);
    const bool havePicture = modelSource_ == 1 ? !modelPicturePath_.empty() : conceptPick_ >= 0;
    ImGui::BeginDisabled(busy || !havePicture || textureHost_.empty());
    if (ImGui::Button("Make 3D model"))
        StartModel();
    ImGui::EndDisabled();
    if (conceptJob_->Running() || modelJob_->Running())
    {
        const bool concepts = conceptJob_->Running();
        ImGui::Text("%s... %d s", concepts ? "Making concept pictures" : "Making the 3D model (minutes)",
                    static_cast<int>(concepts ? conceptJob_->Seconds() : modelJob_->Seconds()));
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            concepts ? conceptJob_->Cancel() : modelJob_->Cancel();
    }

    // 4. The result.
    if (!modelResult_.empty())
    {
        ImGui::Separator();
        ImGui::Text("%s: %zu triangles, texture %u px", modelResult_.c_str(), modelInfo_.triangles, modelInfo_.textureSize);
        ImGui::SliderFloat("Height in the scene (m)", &modelHeight_, 0.1F, 10.0F, "%.2f");
        ImGui::BeginDisabled(!scene_);
        if (ImGui::Button("Place in scene"))
            PlaceModel();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(at the view's centre; Ctrl+Z removes it)");
    }

    if (ImGui::CollapsingHeader("ComfyUI 3D workflow"))
    {
        InputText("Workflow file", modelWorkflow_);
        ImGui::TextWrapped("Once: in ComfyUI open Templates > \"Pixal3D & TRELLIS.2: Image to Model\", switch it to TRELLIS.2, run it once, "
                           "then Workflow > Export (API) and save it as the file above.");
    }
    for (const umbra::core::Diagnostic &d : modelProblems_)
        ImGui::TextColored(d.severity == umbra::core::Severity::Error ? ImVec4(1, 0.4F, 0.4F, 1) : ImVec4(1, 0.8F, 0.3F, 1), "%s", d.message.c_str());
    ImGui::End();
}
