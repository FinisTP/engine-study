#include "ComfySettings.hpp"

#include <EditorPanel.hpp>
#include <ModelAssistant.hpp>
#include <Platform.hpp>
#include <TextureTools.hpp>

#include <SDL3/SDL_misc.h>
#include <imgui.h>

#include <cstdio>
#include <fstream>

// AI model window (roadmap P8D). Text -> a few concept pictures (SDXL, object style) -> pick one, or
// use your own picture -> the creator's exported image-to-3D workflow (TRELLIS.2) -> a new GLB in
// assets/models -> "Place in scene" adds it as one undoable edit. Rules in apps/editor/Editor.md.
// Exported workflows live in the project: projects/<name>/tools/comfyui.

using umbra::authoring::Change;
using umbra::editor::EditorHost;

namespace
{
    void InputText(const char *label, std::string &value)
    {
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer)))
            value = buffer;
    }

    std::filesystem::path Prefs(const char *name)
    {
        return umbra::platform::Platform::PreferencesFolder() / name;
    }

    // Opens a file with the program Windows uses for it (the image viewer for PNGs).
    void OpenFile(const std::filesystem::path &file)
    {
        std::string url = "file:///" + file.generic_string();
        SDL_OpenURL(url.c_str());
    }

    class AiModelPanel final : public umbra::editor::EditorPanel
    {
    public:
        const char *Title() const override { return "AI model"; }
        std::string ButtonLabel() const override { return job_->Running() ? "AI model (working)" : "AI model"; }
        void Draw(EditorHost &host) override;
        void Tick(EditorHost &host) override; // collects a finished model, window open or not
        void Leave() override { job_->Leave(); } // ComfyUI finishes the model; the next start fetches it

    private:
        void LoadSettings(EditorHost &host);
        void SaveSettings();
        void Resume(); // a job left running in ComfyUI when the editor closed
        void Start();
        void Place(EditorHost &host);

        bool started_ = false;
        int source_ = 0; // 0 = describe, 1 = picture file
        std::string idea_, picturePath_, name_, workflow_, sourceText_, result_, saveName_;
        std::vector<std::filesystem::path> workflows_; // usable exports in the project's tools/comfyui
        bool workflowsScanned_ = false;
        int conceptCount_ = 4;
        int faces_ = 10000; // user decision 2026-11-07
        float height_ = 1.0F;
        long long seed_ = 0;
        std::unique_ptr<umbra::localai::ImageJob> conceptJob_ = std::make_unique<umbra::localai::ImageJob>();
        std::unique_ptr<umbra::localai::ModelJob> job_ = std::make_unique<umbra::localai::ModelJob>();
        std::vector<umbra::localai::ImageRequest> conceptRequests_;
        std::vector<umbra::localai::TextureCandidate> concepts_;
        std::vector<std::filesystem::path> conceptFiles_;
        int conceptPick_ = -1;
        umbra::core::TextureData picture_;
        umbra::authoring::ModelInfo info_;
        std::vector<umbra::core::Diagnostic> problems_;
        std::string resumeMessage_;
    };
}

UMBRA_EDITOR_PANEL(AiModelPanel);

void AiModelPanel::LoadSettings(EditorHost &host)
{
    // The workflow chosen last time, if it still exists; else whatever export tools/comfyui holds.
    std::ifstream in(Prefs("ai-model.json"));
    const umbra::authoring::Json settings = umbra::authoring::Json::parse(in, nullptr, false);
    if (settings.is_object())
        workflow_ = settings.value("workflow", "");
    if (workflow_.empty() || !std::filesystem::exists(workflow_))
        workflow_ = umbra::localai::DefaultModelWorkflow(host.AssetRoot().Project()).string();
}

void AiModelPanel::SaveSettings()
{
    std::filesystem::create_directories(umbra::platform::Platform::PreferencesFolder());
    std::ofstream(Prefs("ai-model.json")) << umbra::authoring::Json{{"workflow", workflow_}}.dump(2);
}

void AiModelPanel::Resume()
{
    // 1. A job ComfyUI kept working on after the editor closed (written by StartModel).
    std::ifstream in(Prefs("ai-model-pending.json"));
    const umbra::authoring::Json pending = umbra::authoring::Json::parse(in, nullptr, false);
    if (!pending.is_object() || !pending.contains("job") || job_->Running())
        return;
    umbra::localai::ModelRequest request;
    if (!umbra::localai::LoadModelWorkflow(pending.value("workflow", ""), request.workflow, problems_))
        return;
    // 2. Wait for it again; the result is saved like any other.
    request.host = pending.value("host", "");
    request.resumeId = pending["job"].get<std::string>();
    request.seed = seed_ = pending.value("seed", 0LL);
    request.faces = faces_ = pending.value("faces", faces_);
    saveName_ = pending.value("name", "model");
    sourceText_ = pending.value("source", "");
    workflow_ = pending.value("workflow", "");
    std::string ignored;
    umbra::assets::LoadImageFile(Prefs("ai-model-pending.png"), picture_, ignored);
    job_->Start({request});
    resumeMessage_ = "Fetching the 3D model ComfyUI made after the editor closed (" + saveName_ + ")...";
}

void AiModelPanel::Tick(EditorHost &host)
{
    // 0. Once: the remembered workflow, and a job left running when the editor last closed.
    if (!started_)
    {
        started_ = true;
        LoadSettings(host);
        Resume();
        if (!resumeMessage_.empty())
            host.Message(resumeMessage_, 8.0F);
    }
    if (auto results = job_->Take())
    {
        const auto &result = (*results)[0];
        problems_.insert(problems_.end(), result.diagnostics.begin(), result.diagnostics.end());
        // 1. Forget the pending job, unless ComfyUI only stopped answering (try again next start).
        const bool tryLater = !result.diagnostics.empty() && result.diagnostics[0].code == "ai.connect";
        std::error_code ignored;
        if (!tryLater)
            for (const char *name : {"ai-model-pending.json", "ai-model-pending.png"})
                std::filesystem::remove(Prefs(name), ignored);
        // 2. Save and check the model, then show it in the AI model window.
        if (result.ok)
        {
            result_ = umbra::localai::SaveGeneratedModel(host.AssetRoot().Project(), saveName_, result.glb, picture_,
                                                              {sourceText_, std::filesystem::path(workflow_).filename().string(), seed_, faces_},
                                                              result.graph, problems_);
            if (!result_.empty() && umbra::authoring::InspectModel(host.AssetRoot() / result_, info_, problems_))
                host.RescanAssets();
            else
                result_.clear();
        }
        host.Message(result_.empty() ? "AI model failed: see the AI model window" : "AI model ready: " + result_, 8.0F);
        open = true;
    }
}

void AiModelPanel::Start()
{
    const ComfySettings &comfy = ComfySettings::Shared();
    // 1. The workflow and the picture.
    problems_.clear();
    umbra::localai::ModelRequest request;
    if (!umbra::localai::LoadModelWorkflow(workflow_, request.workflow, problems_))
        return;
    std::string error;
    if (source_ == 1)
    {
        if (!umbra::assets::LoadImageFile(picturePath_, request.picture, error))
        {
            problems_.push_back({umbra::core::Severity::Error, "file.read", picturePath_, "", error});
            return;
        }
        sourceText_ = "file: " + std::filesystem::path(picturePath_).filename().string();
        seed_ = umbra::localai::TextureSeeds(1)[0];
    }
    else
    {
        const auto &picked = concepts_[static_cast<std::size_t>(conceptPick_)];
        request.picture = picked.image;
        sourceText_ = conceptRequests_[static_cast<std::size_t>(conceptPick_)].prompt;
        seed_ = conceptRequests_[static_cast<std::size_t>(conceptPick_)].seed;
    }
    // 2. Run it in the background. Once ComfyUI has the job, it is written down: if the editor closes,
    //    ComfyUI keeps working and the next start fetches the model (ResumeModel).
    request.host = comfy.host;
    request.seed = seed_;
    request.faces = faces_;
    picture_ = request.picture;
    saveName_ = !name_.empty() ? name_ : source_ == 0 ? idea_ : std::filesystem::path(picturePath_).stem().string();
    result_.clear();
    SaveSettings();
    std::vector<std::uint8_t> png;
    umbra::assets::EncodePng(picture_, png);
    std::ofstream(Prefs("ai-model-pending.png"), std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
    umbra::authoring::Json pending{{"host", request.host}, {"workflow", workflow_}, {"name", saveName_},
                                   {"source", sourceText_}, {"seed", seed_}, {"faces", faces_}};
    request.queued = [pending](const std::string &job) mutable {
        pending["job"] = job;
        std::ofstream(Prefs("ai-model-pending.json")) << pending.dump(2);
    };
    job_->Start({request});
}

void AiModelPanel::Place(EditorHost &host)
{
    // One undoable edit: a new entity standing on the ground at the camera's focus point.
    umbra::authoring::Document *scene = host.Scene();
    if (scene == nullptr)
        return;
    const glm::vec3 target = host.ViewTarget();
    const auto entity = umbra::localai::PlacedModelEntity(scene->Data(), name_.empty() ? result_ : name_, result_, info_,
                                                          {target.x, 0.0F, target.z}, height_);
    host.Edit(*scene, {{Change::Op::Insert, "/entities/-", entity}}, "AI model " + entity["id"].get<std::string>());
}

void AiModelPanel::Draw(EditorHost &host)
{
    ComfySettings &comfy = ComfySettings::Shared();
    ImGui::SetNextWindowSize(ImVec2(540, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title(), &open))
    {
        ImGui::End();
        return;
    }
    if (!comfy.searched)
        comfy.Find();
    if (!workflowsScanned_)
    {
        workflows_ = umbra::localai::FindModelWorkflows(host.AssetRoot().Project().parent_path() / "tools/comfyui");
        workflowsScanned_ = true;
    }

    // 1. Finished concept pictures, saved for the image viewer (models: CollectModelResults).
    if (auto results = conceptJob_->Take())
    {
        const std::filesystem::path folder = umbra::platform::Platform::PreferencesFolder() / "ai-concepts";
        std::filesystem::create_directories(folder);
        for (const auto &result : *results)
        {
            problems_.insert(problems_.end(), result.diagnostics.begin(), result.diagnostics.end());
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

    // 2. Where the picture comes from.
    ImGui::TextWrapped("Make a prop from a description or a picture. ComfyUI on this computer turns the picture into a 3D model.");
    ImGui::RadioButton("Describe it", &source_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Use my picture", &source_, 1);
    const bool busy = conceptJob_->Running() || job_->Running();
    if (source_ == 0)
    {
        InputText("Looks like", idea_);
        ImGui::SliderInt("Concept pictures", &conceptCount_, 1, 4);
        ImGui::BeginDisabled(busy || idea_.empty() || comfy.checkpoint.empty() || comfy.host.empty());
        if (ImGui::Button("Make concept pictures"))
        {
            concepts_.clear();
            conceptFiles_.clear();
            conceptPick_ = -1;
            problems_.clear();
            conceptRequests_ = umbra::localai::MakeTextureRequests(idea_, umbra::localai::LoadObjectStyle(host.AssetRoot()), comfy.checkpoint, comfy.host,
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
        InputText("Picture file (PNG/JPEG)", picturePath_);

    // 3. The 3D step.
    InputText("Name (optional)", name_);
    ImGui::SliderInt("Triangles", &faces_, 1000, 50000);
    const bool havePicture = source_ == 1 ? !picturePath_.empty() : conceptPick_ >= 0;
    ImGui::BeginDisabled(busy || !havePicture || comfy.host.empty());
    if (ImGui::Button("Make 3D model"))
        Start();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("workflow: %s", std::filesystem::path(workflow_).filename().string().c_str());
    if (conceptJob_->Running() || job_->Running())
    {
        const bool concepts = conceptJob_->Running();
        ImGui::Text("%s... %d s", concepts ? "Making concept pictures" : "Making the 3D model (minutes; keeps going if the editor closes)",
                    static_cast<int>(concepts ? conceptJob_->Seconds() : job_->Seconds()));
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            concepts ? conceptJob_->Cancel() : job_->Cancel();
    }

    // 4. The result.
    if (!result_.empty())
    {
        ImGui::Separator();
        ImGui::Text("%s: %zu triangles, texture %u px", result_.c_str(), info_.triangles, info_.textureSize);
        ImGui::SliderFloat("Height in the scene (m)", &height_, 0.1F, 10.0F, "%.2f");
        ImGui::BeginDisabled(host.Scene() == nullptr);
        if (ImGui::Button("Place in scene"))
            Place(host);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(at the view's centre; Ctrl+Z removes it)");
    }

    if (ImGui::CollapsingHeader("ComfyUI 3D workflow", workflows_.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
        // Any exported workflow in tools/comfyui, whatever its name; the choice is remembered.
        const std::string current = std::filesystem::path(workflow_).filename().string();
        if (ImGui::BeginCombo("Exported workflow", current.c_str()))
        {
            for (const auto &file : workflows_)
                if (ImGui::Selectable(file.filename().string().c_str(), file.string() == workflow_))
                {
                    workflow_ = file.string();
                    SaveSettings();
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh"))
            workflowsScanned_ = false;
        InputText("Workflow file", workflow_);
        ImGui::TextWrapped("Once: in ComfyUI open Templates > \"Pixal3D & TRELLIS.2: Image to Model\", switch it to TRELLIS.2, "
                           "then Workflow > Export (API) into the project's tools/comfyui folder (projects/<name>/tools/comfyui, any name).");
    }
    for (const umbra::core::Diagnostic &d : comfy.problems)
        ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", d.message.c_str());
    for (const umbra::core::Diagnostic &d : problems_)
        ImGui::TextColored(d.severity == umbra::core::Severity::Error ? ImVec4(1, 0.4F, 0.4F, 1) : ImVec4(1, 0.8F, 0.3F, 1), "%s", d.message.c_str());
    ImGui::End();
}
