#pragma once

#include "EditorPanel.hpp"

#include <ComponentSchema.hpp>
#include <Document.hpp>
#include <ModelTools.hpp>
#include <DrawList.hpp>
#include <AudioDevice.hpp>
#include <Mixer.hpp>
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
class Editor final : public umbra::editor::EditorHost
{
public:
    Editor(ModelLibrary &models, umbra::assets::AssetFolders assetRoot, std::filesystem::path gameExecutable);
    ~Editor();
    Editor(const Editor &) = delete;
    Editor &operator=(const Editor &) = delete;

    // Opens a scene by its path under the asset folder ("scenes/dungeon.scene.json").
    bool OpenScene(const std::string &scenePath);
    // Before rendering: rebuilds the preview world after an edit and fills the draw list.
    void Update(float deltaSeconds, umbra::core::DrawList &drawList);
    // Inside the Dear ImGui frame: every window, viewport mouse input and the gizmo.
    void DrawUi();

    // Closing the window: asks about unsaved changes first.
    void RequestQuit();
    [[nodiscard]] bool WantsQuit() const noexcept { return quit_; }

    // EditorHost: what plugin panels may use (EditorPanel.hpp).
    const umbra::assets::AssetFolders &AssetRoot() const override { return assetRoot_; }
    ModelLibrary &Models() override { return models_; }
    umbra::authoring::Document &Rpg() override { return *rpg_; }
    umbra::authoring::Document *Scene() override { return scene_.get(); }
    umbra::world::World *PreviewWorld() override { return world_.get(); }
    const std::string &Selected() const override { return selected_; }
    std::optional<std::size_t> SelectedIndex() const override;
    glm::vec3 ViewTarget() const override { return camera_.target; }
    bool Edit(umbra::authoring::Document &doc, const std::vector<umbra::authoring::Change> &changes, const std::string &label, bool merge = false) override;
    void Edited(umbra::authoring::Document &doc) override { lastEdited_ = &doc; }
    void Report(const std::vector<umbra::core::Diagnostic> &diagnostics, const std::string &what) override;
    void Message(const std::string &text, float seconds = 3.0F) override { message_ = text, messageSeconds_ = seconds; }
    void RescanAssets() override { ScanAssets(); }
    std::vector<std::string> AssetList(const std::string &group) const override;
    void Reveal(const std::string &what, int index) override;
    void PlaySound(std::shared_ptr<const umbra::audio::AudioClip> clip) override;
    void StopSounds() override { mixer_.StopAll(); }

private:
    // --- Edits (all go through the documents) ---
    bool SetField(const std::string &path, const umbra::authoring::Json &value, const std::string &label, bool merge = false);
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
    static umbra::core::MeshData GridMesh();
    void AddColliderLines(umbra::core::DrawList &drawList) const; // the selected entity's collider, green
    bool MeshBounds(const umbra::world::Renderable &look, glm::vec3 &min, glm::vec3 &max) const;
    std::optional<umbra::authoring::Json> FitCollider(const umbra::authoring::Json &entity) const; // box around its model

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
    void PluginPanels(); // registered by plugins (EditorPanel.hpp)

    // CodePanel.cpp: the C++ files of the four layers, opened in Visual Studio.
    struct CodeLayer final
    {
        std::string name;
        std::filesystem::path folder;
        std::vector<std::string> files; // relative to folder
    };
    void CodePanel();
    void ScanCode();
    void OpenCode(const std::filesystem::path &file); // empty = the whole source folder
    void NewComponent(const std::string &name);

    // Components.cpp
    [[nodiscard]] static const umbra::authoring::Json *ComponentOf(const umbra::authoring::Json &entity, const std::string &type);
    void MoveLegacyParts();
    void ComponentsPanel(const umbra::authoring::Json &entity, const std::string &at);
    void ComponentFields(const std::string &type, const umbra::authoring::Json &component, const std::string &at, const umbra::authoring::Json &entity);

    void ScanAssets();
    void AddEntity(const std::optional<std::string> &parent);
    void DuplicateSelected();
    void DeleteSelected();
    [[nodiscard]] std::vector<std::string> RpgIds(const char *section) const;

    ModelLibrary &models_;
    umbra::assets::AssetFolders assetRoot_; // the project's assets, then its plugins'
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
    umbra::core::ParticleSystem particles_; // the view's "vfx" components (S9)
    std::map<std::string, std::shared_ptr<const umbra::core::VfxEffect>> effects_;
    std::map<std::string, umbra::scene::ComponentSchema> schemas_; // project components (assets/components)
    std::vector<umbra::core::Diagnostic> problems_;
    std::string message_;
    float messageSeconds_ = 0.0F;
    float diskCheckSeconds_ = 0.0F;
    bool changedOnDisk_ = false;
    std::string pendingOpen_; // scene waiting for the "unsaved changes" answer
    bool askQuit_ = false;
    bool quit_ = false;
    bool showCode_ = false;
    std::vector<CodeLayer> code_;
    mutable std::map<std::string, std::optional<umbra::authoring::ModelInfo>> modelBounds_; // FitCollider cache
    umbra::core::MeshHandle gridMesh_; // the ground grid (depth-tested)
    std::vector<std::unique_ptr<umbra::editor::EditorPanel>> panels_; // from plugins
    umbra::audio::Mixer mixer_;             // sound previews (AI sound window); declared before the device
    umbra::platform::AudioDevice audio_;    // opened on the first preview
};
