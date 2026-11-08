#include "ComfySettings.hpp"

#include <EditorPanel.hpp>
#include <SoundAssistant.hpp>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <memory>

// AI sound window (roadmap S4). Describe music or a sound effect; a local ComfyUI makes a few
// variations (ACE-Step 1.5 for music, Stable Audio 3 for effects); listen, keep one: music becomes a
// looping .ogg and can be this scene's music, an effect a .wav that can be a skill's or item's sound.
// Each use is one undoable edit. Rules in apps/editor/Editor.md and localai/include/SoundGen.md.

using umbra::authoring::Change;
using umbra::editor::EditorHost;
using umbra::localai::SoundKind;

namespace
{
    void InputText(const char *label, std::string &value)
    {
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer)))
            value = buffer;
    }

    struct Candidate final
    {
        umbra::localai::SoundCandidate sound;
        std::shared_ptr<const umbra::audio::AudioClip> clip; // for the preview
    };

    class AiSoundPanel final : public umbra::editor::EditorPanel
    {
    public:
        const char *Title() const override { return "AI sound"; }
        void Draw(EditorHost &host) override;
        void Leave() override { job_->Leave(); }

    private:
        void Keep(EditorHost &host);
        void UseAsMusic(EditorHost &host, const std::string &track);
        void UseForEntry(EditorHost &host, const std::string &sound);

        std::unique_ptr<umbra::localai::AudioJob> job_ = std::make_unique<umbra::localai::AudioJob>();
        SoundKind kind_ = SoundKind::Music;
        std::string idea_, name_, model_[2];
        float seconds_[2] = {60.0F, 2.0F};
        int count_ = 2;
        std::vector<umbra::localai::AudioRequest> requests_;
        std::vector<Candidate> candidates_;
        int pick_ = -1;
        std::string saved_;  // the last kept file, ready to be used
        std::string target_; // "skills/<id>" or "items/<id>" for an effect
        std::vector<umbra::core::Diagnostic> problems_;
    };
}

UMBRA_EDITOR_PANEL(AiSoundPanel);

void AiSoundPanel::Keep(EditorHost &host)
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    const std::string name = umbra::localai::TextureName(name_.empty() ? idea_ : name_);
    saved_ = umbra::localai::SaveSound(host.AssetRoot().Project(), name, kind_, candidates_[static_cast<std::size_t>(pick_)].sound, diagnostics);
    if (saved_.empty())
    {
        host.Report(diagnostics, "Sound not saved");
        return;
    }
    host.RescanAssets();
    host.Message("Saved " + saved_ + ".", 6.0F);
}

void AiSoundPanel::UseAsMusic(EditorHost &host, const std::string &track)
{
    // The scene's "music" component: changed where it is, else on a new entity named "music".
    umbra::authoring::Document *scene = host.Scene();
    if (scene == nullptr)
        return;
    const umbra::authoring::Json &entities = scene->Data()["entities"];
    for (std::size_t i = 0; i < entities.size(); ++i)
        if (entities[i].contains("components") && entities[i]["components"].contains("music"))
        {
            host.Edit(*scene, {{Change::Op::Set, "/entities/" + std::to_string(i) + "/components/music/track", track}}, "Scene music");
            return;
        }
    std::string id = "music";
    for (int n = 2; std::any_of(entities.begin(), entities.end(), [&](const auto &e) { return e.value("id", "") == id; }); ++n)
        id = "music_" + std::to_string(n);
    const umbra::authoring::Json entity{{"id", id}, {"name", "Music"}, {"components", {{"music", {{"track", track}, {"volume", 0.7}}}}}};
    host.Edit(*scene, {{Change::Op::Insert, "/entities/-", entity}}, "Scene music");
}

void AiSoundPanel::UseForEntry(EditorHost &host, const std::string &sound)
{
    // "skills/heavy_blow" -> /skills/<index of heavy_blow>/sound in the RPG content.
    umbra::authoring::Document &rpg = host.Rpg();
    const std::string section = target_.substr(0, target_.find('/')), id = target_.substr(target_.find('/') + 1);
    const umbra::authoring::Json &list = rpg.Data()[section];
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].value("id", "") == id)
            host.Edit(rpg, {{Change::Op::Set, "/" + section + "/" + std::to_string(i) + "/sound", sound}}, "Sound of " + id);
}

void AiSoundPanel::Draw(EditorHost &host)
{
    ComfySettings &comfy = ComfySettings::Shared();
    ImGui::SetNextWindowSize(ImVec2(520, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title(), &open))
    {
        ImGui::End();
        return;
    }
    if (!comfy.searched)
        comfy.Find();
    const int k = kind_ == SoundKind::Music ? 0 : 1;
    model_[k] = umbra::localai::PreferredSoundModel(comfy.checkpoints, kind_, model_[k]);

    // 1. Finished runs: decode, loop or trim, normalize.
    if (auto results = job_->Take())
        for (const umbra::localai::AudioResult &result : *results)
        {
            problems_.insert(problems_.end(), result.diagnostics.begin(), result.diagnostics.end());
            umbra::localai::Pcm pcm;
            std::string error;
            if (!result.ok)
                continue;
            if (!umbra::localai::DecodeFlac(result.file, pcm, error))
            {
                problems_.push_back({umbra::core::Severity::Error, "ai.answer", "comfyui", "", "Couldn't read ComfyUI's sound: " + error});
                continue;
            }
            Candidate c{umbra::localai::FinishSound(pcm, kind_, result.request), nullptr};
            c.clip = std::make_shared<const umbra::audio::AudioClip>(c.sound.clip);
            candidates_.push_back(std::move(c));
            if (pick_ < 0)
                pick_ = 0;
        }

    // 2. The request.
    const bool running = job_->Running();
    ImGui::BeginDisabled(running);
    if (ImGui::RadioButton("Music (looping)", kind_ == SoundKind::Music) && kind_ != SoundKind::Music)
        kind_ = SoundKind::Music, candidates_.clear(), pick_ = -1, saved_.clear();
    ImGui::SameLine();
    if (ImGui::RadioButton("Sound effect", kind_ == SoundKind::Effect) && kind_ != SoundKind::Effect)
        kind_ = SoundKind::Effect, candidates_.clear(), pick_ = -1, saved_.clear();
    ImGui::EndDisabled();
    ImGui::TextUnformatted(kind_ == SoundKind::Music ? "Sounds like (style, instruments, mood)" : "Sounds like (what makes the sound)");
    InputText("##idea", idea_);
    InputText("File name (optional)", name_);
    if (kind_ == SoundKind::Music)
        ImGui::SliderFloat("Seconds", &seconds_[0], 20.0F, 180.0F, "%.0f");
    else
        ImGui::SliderFloat("Seconds", &seconds_[1], 0.5F, 6.0F, "%.1f");
    ImGui::SliderInt("Variations", &count_, 1, 4);
    if (ImGui::BeginCombo("Model", model_[k].c_str()))
    {
        for (const std::string &model : comfy.checkpoints)
            if ((model.find("ace") != std::string::npos || model.find("stable_audio") != std::string::npos) &&
                ImGui::Selectable(model.c_str(), model == model_[k]))
                model_[k] = model;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Find"))
        comfy.Find();
    if (model_[k].empty())
        ImGui::TextWrapped("ComfyUI has no audio model yet: put ace_step_1.5_turbo_aio.safetensors (music) or stable_audio_3_medium_base.safetensors "
                           "plus t5gemma_b_b_ul2.safetensors (effects) in its models folders, then press Find (SoundGen.md).");
    else
    {
        const auto license = umbra::assets::ModelLicense(model_[k]);
        ImGui::TextDisabled("License: %s", license.known ? license.name.c_str() : "unknown - check it before shipping");
    }

    if (running)
    {
        ImGui::Text("Generating %zu/%zu... %d s", job_->Finished(), requests_.size(), static_cast<int>(job_->Seconds()));
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            job_->Cancel();
    }
    else
    {
        ImGui::BeginDisabled(idea_.empty() || model_[k].empty() || comfy.host.empty());
        if (ImGui::Button("Generate"))
        {
            host.StopSounds();
            candidates_.clear();
            problems_.clear();
            pick_ = -1;
            saved_.clear();
            requests_ = umbra::localai::MakeSoundRequests(idea_, kind_, umbra::localai::LoadSoundStyle(host.AssetRoot()), model_[k], comfy.host,
                                                          seconds_[k], umbra::localai::TextureSeeds(count_));
            job_->Start(requests_);
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();

    // 3. Variations: listen, pick one, keep it.
    for (std::size_t i = 0; i < candidates_.size(); ++i)
    {
        const auto &c = candidates_[i];
        ImGui::PushID(static_cast<int>(i));
        char label[96];
        std::snprintf(label, sizeof(label), "Variation %zu  (%.1f s, seed %lld%s)", i + 1, c.sound.seconds, c.sound.request.seed,
                      c.sound.silent ? ", almost silent" : "");
        if (ImGui::RadioButton(label, pick_ == static_cast<int>(i)))
            pick_ = static_cast<int>(i);
        ImGui::SameLine();
        if (ImGui::SmallButton("Play"))
            host.PlaySound(c.clip);
        ImGui::PopID();
    }
    if (!candidates_.empty() && ImGui::SmallButton("Stop"))
        host.StopSounds();
    if (pick_ >= 0 && saved_.empty() && ImGui::Button(kind_ == SoundKind::Music ? "Keep (saves audio/music/*.ogg)" : "Keep (saves audio/sfx/*.wav)"))
        Keep(host);

    // 4. Use what was kept: scene music, or a skill's / item's sound. Each is one undoable edit.
    if (!saved_.empty())
    {
        ImGui::Text("Kept: %s", saved_.c_str());
        if (kind_ == SoundKind::Music)
        {
            ImGui::BeginDisabled(host.Scene() == nullptr);
            if (ImGui::Button("Use as this scene's music"))
                UseAsMusic(host, saved_);
            ImGui::EndDisabled();
            ImGui::TextDisabled("Fights happen in the arena scene: open it and use a track there for battle music.");
        }
        else
        {
            if (ImGui::BeginCombo("For", target_.c_str()))
            {
                for (const char *section : {"skills", "items"})
                    for (const auto &entry : host.Rpg().Data()[section])
                    {
                        const std::string option = std::string(section) + "/" + entry.value("id", "");
                        if (ImGui::Selectable(option.c_str(), option == target_))
                            target_ = option;
                    }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(target_.empty());
            if (ImGui::Button("Use"))
                UseForEntry(host, saved_);
            ImGui::EndDisabled();
        }
    }
    for (const umbra::core::Diagnostic &d : comfy.problems)
        ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", d.message.c_str());
    for (const umbra::core::Diagnostic &d : problems_)
        ImGui::TextColored(d.severity == umbra::core::Severity::Error ? ImVec4(1, 0.4F, 0.4F, 1) : ImVec4(1, 0.8F, 0.3F, 1), "%s", d.message.c_str());
    ImGui::End();
}
