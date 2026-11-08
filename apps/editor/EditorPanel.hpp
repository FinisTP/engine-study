#pragma once

#include "../umbra/ModelLibrary.hpp"

#include <AssetFolders.hpp>
#include <Document.hpp>
#include <DrawList.hpp>
#include <Wav.hpp>
#include <World.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Editor panels from plugins (docs/adr/0005-plugins.md). A plugin's editor/*.cpp files are
// compiled into the editor; each registers its window with UMBRA_EDITOR_PANEL. The editor lists
// them in the View menu and the toolbar and calls them every frame. Panels reach the editor
// only through EditorHost.
namespace umbra::editor
{
    // What a panel may use. Every edit goes through the documents, so it is validated and undoable.
    class EditorHost
    {
    public:
        virtual ~EditorHost() = default;
        [[nodiscard]] virtual const assets::AssetFolders &AssetRoot() const = 0; // read: `AssetRoot() / "x"`; write: AssetRoot().Project()
        [[nodiscard]] virtual ModelLibrary &Models() = 0;
        [[nodiscard]] virtual authoring::Document &Rpg() = 0;      // the project's RPG content
        [[nodiscard]] virtual authoring::Document *Scene() = 0;    // the open scene, or nullptr
        [[nodiscard]] virtual world::World *PreviewWorld() = 0;    // what the scene view draws
        [[nodiscard]] virtual const std::string &Selected() const = 0; // selected entity id ("" = none)
        [[nodiscard]] virtual std::optional<std::size_t> SelectedIndex() const = 0;
        [[nodiscard]] virtual glm::vec3 ViewTarget() const = 0;    // the point the camera orbits
        virtual bool Edit(authoring::Document &doc, const std::vector<authoring::Change> &changes, const std::string &label, bool merge = false) = 0;
        virtual void Edited(authoring::Document &doc) = 0;         // after doc.Apply() done by the panel: Ctrl+Z undoes it
        virtual void Report(const std::vector<core::Diagnostic> &diagnostics, const std::string &what) = 0;
        virtual void Message(const std::string &text, float seconds = 3.0F) = 0;
        virtual void RescanAssets() = 0;
        [[nodiscard]] virtual std::vector<std::string> AssetList(const std::string &group) const = 0; // "Models", "Sounds", "Learned AI"...
        virtual void Reveal(const std::string &what, int index) = 0; // asks every panel to show this (e.g. "quests", 3)
        virtual void PlaySound(std::shared_ptr<const audio::AudioClip> clip) = 0; // a preview on the editor's speakers (stops the last one)
        virtual void StopSounds() = 0;
    };

    class EditorPanel
    {
    public:
        virtual ~EditorPanel() = default;
        [[nodiscard]] virtual const char *Title() const = 0;          // window and menu name
        [[nodiscard]] virtual std::string ButtonLabel() const { return Title(); } // toolbar ("" = no button)
        virtual void Draw(EditorHost &host) = 0;                       // only while open; calls ImGui::Begin itself
        virtual void Tick(EditorHost &) {}                             // every frame, open or not
        virtual void DecorateView(EditorHost &, core::DrawList &) const {} // change what the scene view draws
        virtual void Leave() {}                                        // the editor is closing
        virtual bool Reveal(const std::string &, int) { return false; } // opens itself at this entry, if it knows it
        bool open = false;
    };

    using PanelFactory = std::function<std::unique_ptr<EditorPanel>()>;
    bool RegisterPanel(PanelFactory factory);
    [[nodiscard]] std::vector<std::unique_ptr<EditorPanel>> CreatePanels(); // in registration order
}

// In a plugin's editor/*.cpp: UMBRA_EDITOR_PANEL(MyPanel);
#define UMBRA_EDITOR_PANEL(CLASS) \
    static const bool umbraPanel##CLASS = ::umbra::editor::RegisterPanel([] { return std::unique_ptr<::umbra::editor::EditorPanel>(new CLASS()); })
