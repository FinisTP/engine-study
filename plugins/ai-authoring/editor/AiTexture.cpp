#include "ComfySettings.hpp"

#include <EditorPanel.hpp>
#include <TextureAssistant.hpp>
#include <TextureTools.hpp>

#include <imgui.h>

#include <cstdio>

// AI texture window (roadmap P8C.4). The creator describes a surface; a local ComfyUI makes a few
// variations; each is made seamless and shown on the selected object in the scene view, repeating
// at its real size. "Apply" writes the PNG and material file, then sets the object's material as
// one undoable edit. Rules in apps/editor/Editor.md.

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

    class AiTexturePanel final : public umbra::editor::EditorPanel
    {
    public:
        ~AiTexturePanel() override { ClearPreviews(); }
        const char *Title() const override { return "AI texture"; }
        void Draw(EditorHost &host) override;
        void DecorateView(EditorHost &host, umbra::core::DrawList &drawList) const override;

    private:
        void ClearPreviews();
        void Apply(EditorHost &host);

        ModelLibrary *models_ = nullptr; // for the preview textures (set by Draw)
        std::unique_ptr<umbra::localai::ImageJob> job_ = std::make_unique<umbra::localai::ImageJob>();
        std::string idea_, name_;
        float meters_ = 2.0F;
        int count_ = 4;
        std::vector<umbra::localai::ImageRequest> requests_;
        std::vector<umbra::localai::TextureCandidate> candidates_;
        std::vector<umbra::core::TextureHandle> previews_; // GPU copies of the candidates
        int pick_ = -1;
        std::vector<umbra::core::Diagnostic> problems_;
    };
}

UMBRA_EDITOR_PANEL(AiTexturePanel);

void AiTexturePanel::ClearPreviews()
{
    for (const umbra::core::TextureHandle handle : previews_)
        models_->DestroyPreviewTexture(handle);
    previews_.clear();
    candidates_.clear();
    pick_ = -1;
}

void AiTexturePanel::DecorateView(EditorHost &host, umbra::core::DrawList &drawList) const
{
    // Draws the picked variation on the selected object instead of its own texture.
    const umbra::world::World *world = host.PreviewWorld();
    if (pick_ < 0 || world == nullptr || host.Selected().empty())
        return;
    const umbra::world::Entity entity = world->Find(host.Selected());
    const umbra::world::Renderable *renderable = world->GetRenderable(entity);
    if (renderable == nullptr)
        return;
    for (umbra::core::DrawItem &item : drawList.items)
        if (item.mesh == renderable->meshHandle && item.model == *world->GetWorldMatrix(entity))
        {
            item.texture = previews_[static_cast<std::size_t>(pick_)];
            item.metersPerRepeat = meters_;
            item.color = renderable->color;
        }
}

void AiTexturePanel::Apply(EditorHost &host)
{
    // 1. New files first: the image, then the material that points at it.
    std::vector<umbra::core::Diagnostic> diagnostics;
    const auto &candidate = candidates_[static_cast<std::size_t>(pick_)];
    const std::string material = umbra::localai::SaveTextureMaterial(host.AssetRoot().Project(), name_.empty() ? idea_ : name_, candidate,
                                                                     meters_, diagnostics);
    if (material.empty())
    {
        host.Report(diagnostics, "Texture not saved");
        return;
    }
    host.RescanAssets();
    // 2. Then the object's material, as one undoable edit (undo keeps the files, so redo works).
    const std::optional<std::size_t> index = host.SelectedIndex();
    umbra::authoring::Document *scene = host.Scene();
    if (index && scene && scene->Data()["entities"][*index].contains("model"))
        host.Edit(*scene, {{umbra::authoring::Change::Op::Set, "/entities/" + std::to_string(*index) + "/material", material}}, "AI texture " + host.Selected());
    host.Message("Saved " + material + (index ? "" : " (select an object to use it)") + ".", 6.0F);
    ClearPreviews();
}

void AiTexturePanel::Draw(EditorHost &host)
{
    models_ = &host.Models();
    ComfySettings &comfy = ComfySettings::Shared();
    ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title(), &open))
    {
        ImGui::End();
        return;
    }
    if (!comfy.searched)
        comfy.Find();

    // 1. Finished pictures: make each seamless and upload it for the preview.
    if (auto results = job_->Take())
    {
        for (const umbra::localai::ImageResult &result : *results)
        {
            problems_.insert(problems_.end(), result.diagnostics.begin(), result.diagnostics.end());
            if (!result.ok)
                continue;
            const auto &request = requests_[candidates_.size()];
            candidates_.push_back(umbra::localai::FinishTexture(result.image, request));
            previews_.push_back(models_->CreatePreviewTexture(candidates_.back().image));
        }
        if (!candidates_.empty())
            pick_ = 0;
    }

    // 2. The request.
    ImGui::TextWrapped("Describe a surface. A local image model (ComfyUI) makes it; it repeats without seams.");
    ImGui::TextUnformatted("Looks like");
    InputText("##idea", idea_);
    InputText("File name (optional)", name_);
    ImGui::SliderFloat("Meters per repeat", &meters_, 0.25F, 10.0F, "%.2f");
    ImGui::SliderInt("Variations", &count_, 1, 4);
    if (ImGui::BeginCombo("Model", comfy.checkpoint.c_str()))
    {
        for (const std::string &model : comfy.checkpoints)
            if (ImGui::Selectable(model.c_str(), model == comfy.checkpoint))
                comfy.checkpoint = model;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Find"))
        comfy.Find();
    if (ImGui::CollapsingHeader("ComfyUI settings"))
    {
        InputText("Address", comfy.host);
        ImGui::TextWrapped("First time: install ComfyUI Desktop, put sd_xl_base_1.0.safetensors in its models/checkpoints folder, start it, press Find.");
        ImGui::TextWrapped("Every prompt gets the project style from materials/style.json (the project's, else the plugin's default).");
    }

    const bool running = job_->Running();
    if (running)
    {
        ImGui::Text("Generating %zu/%zu... %d s", job_->Finished(), requests_.size(), static_cast<int>(job_->Seconds()));
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            job_->Cancel();
    }
    else
    {
        ImGui::BeginDisabled(idea_.empty() || comfy.checkpoint.empty() || comfy.host.empty());
        if (ImGui::Button("Generate"))
        {
            ClearPreviews();
            problems_.clear();
            requests_ = umbra::localai::MakeTextureRequests(idea_, umbra::localai::LoadTextureStyle(host.AssetRoot()), comfy.checkpoint, comfy.host,
                                                                    umbra::localai::TextureSeeds(count_));
            job_->Start(requests_);
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();

    // 3. Variations: pick one to see it on the selected object.
    for (std::size_t i = 0; i < candidates_.size(); ++i)
    {
        const auto &candidate = candidates_[i];
        char label[96];
        std::snprintf(label, sizeof(label), "Variation %zu  (seed %lld, seam %.1f%s)", i + 1, candidate.request.seed, candidate.seam,
                      candidate.seam > umbra::assets::visibleSeam ? " - may show" : "");
        if (ImGui::RadioButton(label, pick_ == static_cast<int>(i)))
            pick_ = static_cast<int>(i);
    }
    if (pick_ >= 0)
    {
        ImGui::TextWrapped(host.Selected().empty() ? "Select an object to see it there." : "Shown on '%s' in the scene view.", host.Selected().c_str());
        const auto license = umbra::assets::ModelLicense(comfy.checkpoint);
        if (!license.commercial)
            ImGui::TextColored(ImVec4(1, 0.8F, 0.3F, 1), "Note: %s", license.known ? (license.name + ": not for a game you sell.").c_str()
                                                                                 : "this model's license is unknown; check it before shipping.");
        if (ImGui::Button(host.Selected().empty() ? "Save material" : "Apply to object"))
            Apply(host);
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
            ClearPreviews();
    }
    for (const umbra::core::Diagnostic &d : comfy.problems)
        ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", d.message.c_str());
    for (const umbra::core::Diagnostic &d : problems_)
        ImGui::TextColored(d.severity == umbra::core::Severity::Error ? ImVec4(1, 0.4F, 0.4F, 1) : ImVec4(1, 0.8F, 0.3F, 1), "%s", d.message.c_str());
    ImGui::End();
}
