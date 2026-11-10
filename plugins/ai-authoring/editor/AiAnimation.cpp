#include "ComfySettings.hpp"

#include <AnimationGen.hpp>
#include <EditorPanel.hpp>
#include <Generate.hpp>

#include <imgui.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <thread>

// AI animation window (S8). Rig: pick a model, a local ComfyUI runs the creator's exported UniRig
// workflow and models/<name>_rigged.glb is saved. Motion: describe a movement, the exported
// HY-Motion workflow makes animations/<name>.glb. Both run in the background; rules in
// localai/include/AnimationGen.md.

using umbra::editor::EditorHost;
using umbra::localai::AnimKind;

namespace
{
    void InputText(const char *label, std::string &value)
    {
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer)))
            value = buffer;
    }

    // A combo over a list of names (the first entry may be "" = none).
    void Choose(const char *label, std::string &value, const std::vector<std::string> &items)
    {
        if (!ImGui::BeginCombo(label, value.empty() ? "(none)" : value.c_str()))
            return;
        for (const std::string &item : items)
            if (ImGui::Selectable(item.empty() ? "(none)" : item.c_str(), item == value))
                value = item;
        ImGui::EndCombo();
    }

    class AiAnimationPanel final : public umbra::editor::EditorPanel
    {
    public:
        ~AiAnimationPanel() override { Stop(); }
        const char *Title() const override { return "AI animation"; }
        void Draw(EditorHost &host) override;
        void Leave() override { Stop(); }

    private:
        void Start(EditorHost &host);
        void Stop()
        {
            stop_ = true;
            if (thread_.joinable())
                thread_.join();
        }

        AnimKind kind_ = AnimKind::Rig;
        std::string model_, idea_, character_, name_, workflow_;
        float seconds_ = 3.0F;
        std::thread thread_;
        std::atomic<bool> running_ = false, stop_ = false;
        std::mutex mutex_;
        std::string progress_;
        std::optional<umbra::localai::GenerateResult> result_; // set by the worker, taken by Draw
        std::string made_;                                     // the last saved file
        std::string madeInfo_;
    };
}

UMBRA_EDITOR_PANEL(AiAnimationPanel);

void AiAnimationPanel::Start(EditorHost &host)
{
    // The worker gets its own copy of everything it needs; Draw picks the result up.
    Stop();
    stop_ = false;
    running_ = true;
    umbra::localai::GenerateOptions options;
    options.assets = host.AssetRoot();
    options.host = ComfySettings::Shared().host;
    options.idea = idea_;
    options.name = name_;
    options.seconds = seconds_;
    options.source = kind_ == AnimKind::Rig ? model_ : character_;
    options.workflow = workflow_;
    options.stop = &stop_;
    options.progress = [this](const std::string &what, std::size_t, std::size_t, float seconds) {
        std::lock_guard lock(mutex_);
        progress_ = what + "... " + std::to_string(static_cast<int>(seconds)) + " s";
    };
    thread_ = std::thread([this, options, kind = kind_] {
        auto result = kind == AnimKind::Rig ? umbra::localai::CreateRig(options) : umbra::localai::CreateMotion(options);
        std::lock_guard lock(mutex_);
        result_ = std::move(result);
        running_ = false;
    });
}

void AiAnimationPanel::Draw(EditorHost &host)
{
    ComfySettings &comfy = ComfySettings::Shared();
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title(), &open))
    {
        ImGui::End();
        return;
    }
    if (!comfy.searched)
        comfy.Find();

    // 1. A finished run: report problems, remember what was made.
    {
        std::lock_guard lock(mutex_);
        if (result_)
        {
            if (!result_->diagnostics.empty())
                host.Report(result_->diagnostics, kind_ == AnimKind::Rig ? "Rigging" : "Motion");
            if (!result_->made.empty())
            {
                made_ = result_->made[0].path;
                madeInfo_ = result_->made[0].info.dump();
                host.RescanAssets();
                host.Message("Saved " + made_ + ".", 6.0F);
            }
            result_.reset();
        }
    }

    // 2. The request.
    const bool running = running_;
    ImGui::BeginDisabled(running);
    if (ImGui::RadioButton("Rig a model (UniRig)", kind_ == AnimKind::Rig))
        kind_ = AnimKind::Rig, workflow_.clear();
    ImGui::SameLine();
    if (ImGui::RadioButton("Motion from text (HY-Motion)", kind_ == AnimKind::Motion))
        kind_ = AnimKind::Motion, workflow_.clear();
    std::vector<std::string> models = host.AssetList("Models");
    if (kind_ == AnimKind::Rig)
        Choose("Model", model_, models);
    else
    {
        InputText("What it does", idea_);
        ImGui::SliderFloat("Seconds", &seconds_, 0.5F, 12.0F, "%.1f");
        models.insert(models.begin(), "");
        Choose("Character (retarget)", character_, models);
    }
    InputText("Name (optional)", name_);
    // Workflows exported from ComfyUI into <project>/tools/comfyui (AnimationGen.md).
    std::vector<std::string> workflows;
    for (const auto &file : umbra::localai::FindAnimWorkflows(host.AssetRoot().Project().parent_path() / "tools/comfyui", kind_))
        workflows.push_back(file.string());
    if (workflow_.empty() && !workflows.empty())
        workflow_ = umbra::localai::DefaultAnimWorkflow(host.AssetRoot().Project(), kind_).string();
    Choose("Workflow", workflow_, workflows);
    InputText("ComfyUI", comfy.host);
    const bool ready = !comfy.host.empty() && !workflow_.empty() && (kind_ == AnimKind::Rig ? !model_.empty() : !idea_.empty());
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button(kind_ == AnimKind::Rig ? "Rig" : "Make motion"))
        Start(host);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (workflows.empty())
        ImGui::TextWrapped("No %s workflow yet: export one from ComfyUI with Workflow -> Export (API) into tools/comfyui (see AnimationGen.md).",
                           kind_ == AnimKind::Rig ? "rigging" : "motion");

    // 3. Progress, then the result and how to use it.
    if (running)
    {
        std::lock_guard lock(mutex_);
        ImGui::TextUnformatted(progress_.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            stop_ = true;
    }
    if (!made_.empty())
    {
        ImGui::Separator();
        ImGui::Text("Made %s", made_.c_str());
        ImGui::TextWrapped("%s", madeInfo_.c_str());
        const std::string use = made_.starts_with("animations/")
                                    ? "Use a clip in a graph: \"clip\": \"" + made_ + "#<clip name>\" (same joint names as the character)."
                                    : "Give an entity this model and an animation component (animations/*.anim.json).";
        ImGui::TextWrapped("%s", use.c_str());
    }
    ImGui::End();
}
