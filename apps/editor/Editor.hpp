#pragma once

#include "../umbra/ModelLibrary.hpp"

#include <Document.hpp>
#include <QuestAssistant.hpp>
#include <DrawList.hpp>
#include <World.hpp>

#include <glm/mat4x4.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Orbit camera for the scene view: right drag turns, middle drag pans, wheel zooms.
struct OrbitCamera final
{
    glm::vec3 target{0.0F, 0.0F, 0.0F};
    float yaw = 0.6F;    // around +Y
    float pitch = -0.5F; // looking down
    float distance = 14.0F;

    [[nodiscard]] umbra::core::Camera ToCamera() const;
};

// The Umbra editor: one scene document and the RPG content document, edited only through
// umbra::authoring (validated, undoable). Panels and rules in Editor.md.
class Editor final
{
public:
    Editor(ModelLibrary &models, std::filesystem::path assetRoot, std::filesystem::path gameExecutable);

    // Opens a scene by its path under the asset folder ("scenes/dungeon.scene.json").
    bool OpenScene(const std::string &scenePath);
    // Before rendering: rebuilds the preview world after an edit and fills the draw list.
    void Update(float deltaSeconds, umbra::core::DrawList &drawList);
    // Inside the Dear ImGui frame: every window, viewport mouse input and the gizmo.
    void DrawUi();

    // Closing the window: asks about unsaved changes first.
    void RequestQuit();
    [[nodiscard]] bool WantsQuit() const noexcept { return quit_; }

private:
    // --- Edits (all go through the documents) ---
    bool Edit(umbra::authoring::Document &doc, const std::vector<umbra::authoring::Change> &changes, const std::string &label,
              bool merge = false);
    bool SetField(const std::string &path, const umbra::authoring::Json &value, const std::string &label, bool merge = false);
    void Report(const std::vector<umbra::core::Diagnostic> &diagnostics, const std::string &what);
    void Save();
    void Undo();
    void Redo();
    void Play();

    // --- Scene view ---
    void RebuildWorld();
    void ViewportInput();
    void DrawGizmo();
    [[nodiscard]] std::optional<std::string> Pick(float mouseX, float mouseY) const;
    [[nodiscard]] glm::mat4 View() const;
    [[nodiscard]] glm::mat4 GlProjection() const; // OpenGL-style, for ImGuizmo and picking
    void AddSelectionBox(umbra::core::DrawList &drawList) const;

    // --- Panels ---
    void MenuBar();
    void Hierarchy();
    void HierarchyNode(const std::string &id);
    void Inspector();
    void AssetBrowser();
    void DiagnosticsPanel();
    void StatusBar();
    void Popups();
    void Shortcuts();
    void RpgForms(); // RpgForms.cpp
    void RpgEntryForm(const std::string &at, const umbra::authoring::Json &entry, const std::vector<std::string> &npcs);
    bool RpgEdit(const std::vector<umbra::authoring::Change> &changes, const std::string &label, bool merge = false);
    void AiDraft(); // AiDraft.cpp
    void LoadDraftSettings();
    void StartDraft(bool fix);
    void PlanCurrentDraft();
    void ApproveDraft();
    void RefreshDraftModels();

    void ScanAssets();
    void AddEntity(const std::optional<std::string> &parent);
    void DuplicateSelected();
    void DeleteSelected();
    [[nodiscard]] std::optional<std::size_t> SelectedIndex() const;
    [[nodiscard]] std::vector<std::string> RpgIds(const char *section) const;

    ModelLibrary &models_;
    std::filesystem::path assetRoot_;
    std::filesystem::path gameExecutable_;

    std::unique_ptr<umbra::authoring::Document> scene_;
    std::string scenePath_; // relative to the asset folder
    std::unique_ptr<umbra::authoring::Document> rpg_;
    std::unique_ptr<umbra::world::World> world_; // preview, rebuilt from scene_ after each change
    std::uint64_t builtRevision_ = 0;

    std::string selected_; // entity id
    OrbitCamera camera_;
    int gizmoOperation_ = 0; // 0 move, 1 rotate, 2 scale
    bool gizmoWasUsing_ = false;
    bool newStep_ = true; // the next drag edit starts a new undo step
    glm::vec2 displaySize_{1280.0F, 720.0F};
    umbra::authoring::Document *lastEdited_ = nullptr; // undo follows the last edit
    bool openNewScene_ = false;

    std::map<std::string, std::vector<std::string>> assets_; // "Scenes", "Models", ... -> paths
    std::vector<umbra::core::Diagnostic> problems_;
    std::string message_;
    float messageSeconds_ = 0.0F;
    float diskCheckSeconds_ = 0.0F;
    bool changedOnDisk_ = false;
    std::string pendingOpen_; // scene waiting for the "unsaved changes" answer
    bool askQuit_ = false;
    bool quit_ = false;
    bool showRpg_ = false;
    std::string rpgSection_ = "items";
    int rpgSelected_ = 0;

    // AI quest draft (P8B): the request runs on a worker; the plan is checked against draftBase_.
    bool showDraft_ = false;
    std::unique_ptr<umbra::localai::ChatJob> draftJob_ = std::make_unique<umbra::localai::ChatJob>();
    std::string draftIdea_, draftNpc_, draftModel_ = "qwen2.5:7b", draftHost_ = "http://localhost:11434";
    umbra::localai::ChatRequest draftRequest_;
    std::string draftAnswer_;
    umbra::authoring::DraftPlan draftPlan_;
    std::vector<umbra::core::Diagnostic> draftProblems_;
    std::uint64_t draftBase_ = 0; // RPG revision the plan was checked against
    std::vector<std::string> draftModels_; // installed in Ollama
    std::vector<umbra::core::Diagnostic> draftModelProblems_;
    bool draftModelsChecked_ = false;
};
