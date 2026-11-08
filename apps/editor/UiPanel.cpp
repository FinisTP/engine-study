#include "EditorPanel.hpp"
#include "Widgets.hpp"

#include <Log.hpp>
#include <TextureTools.hpp>
#include <Ui.hpp>

#include <imgui.h>

#include <fstream>
#include <map>

// Game UI window (roadmap S3.6): pick a screen file (ui/*.ui.json), see it drawn over the scene
// view with the sample data (ui/sample.json), select a widget in its tree and change its fields.
// Every change is checked like the game checks it; Save writes the file into the project, so a
// plugin's screen becomes a project copy that wins over the plugin's. Rules in Editor.md.

using umbra::editor::EditorHost;
using Json = nlohmann::json;

namespace
{
    // Fields a widget may have (engine/ui/include/UI.md); some have a fixed set of values.
    const std::vector<std::string> fieldNames{"id", "anchor", "offset", "size", "style", "direction", "align", "gap", "columns", "text",
                                              "textAlign", "wrap", "visible", "disabled", "selected", "value", "max", "image",
                                              "action", "arg", "items", "as"};
    const std::map<std::string, std::vector<std::string>> fieldValues{
        {"type", {"panel", "stack", "text", "bar", "image", "button", "list", "spacer"}},
        {"anchor", {"top-left", "top", "top-right", "left", "center", "right", "bottom-left", "bottom", "bottom-right", "fill"}},
        {"direction", {"vertical", "horizontal"}},
        {"align", {"start", "center", "end", "stretch"}},
        {"textAlign", {"left", "center", "right"}}};
    // A new field's first value.
    Json NewValue(const std::string &field)
    {
        if (field == "offset" || field == "size")
            return Json::array({0, 0});
        if (field == "gap" || field == "columns")
            return field == "gap" ? 4 : 2;
        if (field == "wrap")
            return true;
        if (fieldValues.contains(field))
            return fieldValues.at(field).front();
        return "";
    }

    class UiPanel final : public umbra::editor::EditorPanel
    {
    public:
        const char *Title() const override { return "Game UI"; }
        void Draw(EditorHost &host) override;
        void Tick(EditorHost &host) override;
        void DecorateView(EditorHost &, umbra::core::DrawList &drawList) const override
        {
            if (open && showInView_)
                drawList.ui = preview_;
        }
        void Leave() override { FreeTextures(); }

    private:
        void Load(EditorHost &host);
        void FreeTextures();
        void Pick(const std::string &screen);
        void Check(); // the edited file -> the preview, or the reasons it can't be shown
        void Change(const std::function<void()> &edit);
        void Tree(Json &node, const std::string &pointer);
        void Fields(Json &node);
        void Save(EditorHost &host);

        ModelLibrary *models_ = nullptr;
        umbra::assets::AssetFolders root_;
        bool loaded_ = false, showInView_ = true, withHud_ = true;
        umbra::ui::UiSystem ui_;
        std::vector<umbra::core::Diagnostic> loadProblems_, problems_;
        umbra::core::TextureHandle atlas_;
        std::map<std::string, umbra::ui::ImageRef> images_;
        Json sample_ = Json::object();
        std::string screen_, file_;          // the screen being edited and its asset path
        Json json_;                          // its file, as edited
        std::vector<Json> undo_;
        std::string selected_ = "/root";     // JSON pointer of the selected widget
        bool changed_ = false;
        umbra::core::UiDrawList preview_;
    };

    void UiPanel::FreeTextures()
    {
        if (models_ == nullptr)
            return;
        if (atlas_)
            models_->DestroyPreviewTexture(atlas_);
        for (const auto &[path, image] : images_)
            if (image.texture)
                models_->DestroyPreviewTexture(image.texture);
        atlas_ = {};
        images_.clear();
    }

    void UiPanel::Load(EditorHost &host)
    {
        // 1. The skin, fonts and screens, the same way the game reads them.
        FreeTextures();
        models_ = &host.Models();
        ui_ = umbra::ui::UiSystem{};
        loadProblems_.clear();
        loaded_ = ui_.Load(host.AssetRoot(), loadProblems_);
        loaded_ = loaded_ || !ui_.Screens().empty(); // a broken screen file still lets the others load
        if (ui_.Atlas().width > 0)
        {
            atlas_ = models_->CreatePreviewTexture(ui_.Atlas());
            ui_.SetAtlasTexture(atlas_);
        }
        root_ = host.AssetRoot();
        ui_.SetImageLoader([this](const std::string &path) {
            if (const auto it = images_.find(path); it != images_.end())
                return it->second;
            umbra::core::TextureData image;
            std::string error;
            umbra::ui::ImageRef ref;
            if (umbra::assets::LoadImageFile(root_ / path, image, error))
                ref = {models_->CreatePreviewTexture(image), {static_cast<float>(image.width), static_cast<float>(image.height)}};
            images_[path] = ref;
            return ref;
        });
        // 2. The sample data the preview binds to.
        std::ifstream in(root_ / "ui/sample.json");
        sample_ = Json::parse(in, nullptr, false);
        if (!sample_.is_object())
            sample_ = Json::object();
        Pick(!screen_.empty() && ui_.Has(screen_) ? screen_ : ui_.Has("hud") ? "hud" : ui_.Screens().empty() ? "" : ui_.Screens().begin()->first);
    }

    void UiPanel::Pick(const std::string &screen)
    {
        screen_ = screen;
        undo_.clear();
        selected_ = "/root";
        changed_ = false;
        problems_.clear();
        if (screen.empty())
            return;
        file_ = ui_.Screens().at(screen).file;
        std::ifstream in(root_ / file_);
        json_ = Json::parse(in, nullptr, false);
        Check();
    }

    void UiPanel::Check()
    {
        problems_.clear();
        umbra::ui::Screen screen;
        if (umbra::ui::ParseScreen(json_, screen_, file_, screen, problems_))
            ui_.SetScreen(std::move(screen));
    }

    void UiPanel::Change(const std::function<void()> &edit)
    {
        undo_.push_back(json_);
        if (undo_.size() > 100)
            undo_.erase(undo_.begin());
        edit();
        changed_ = true;
        Check();
    }

    void UiPanel::Tick(EditorHost &)
    {
        // The preview: the HUD under the screen (unless it is the HUD), laid out for the window.
        preview_.Clear();
        if (!open || !showInView_ || !loaded_ || screen_.empty())
            return;
        const std::vector<std::string> shown = ui_.OpenScreens(); // a copy: Close changes the list
        for (const std::string &name : shown)
            ui_.Close(name);
        if (withHud_ && screen_ != "hud" && ui_.Has("hud"))
            ui_.Open("hud");
        ui_.Open(screen_);
        const ImGuiIO &io = ImGui::GetIO();
        (void)ui_.Update(sample_, {io.DisplaySize.x * io.DisplayFramebufferScale.x, io.DisplaySize.y * io.DisplayFramebufferScale.y}, {}, preview_);
    }

    void UiPanel::Tree(Json &node, const std::string &pointer)
    {
        // One row per widget: its type and id or text; children and a list's entry below it.
        if (!node.is_object())
            return;
        std::string label = node.value("type", "?");
        if (const std::string id = node.value("id", ""); !id.empty())
            label += "  #" + id;
        else if (const std::string text = node.value("text", ""); !text.empty())
            label += "  \"" + text.substr(0, 28) + (text.size() > 28 ? "...\"" : "\"");
        const bool leaf = !node.contains("children") && !node.contains("entry");
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (leaf)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (pointer == selected_)
            flags |= ImGuiTreeNodeFlags_Selected;
        const bool expanded = ImGui::TreeNodeEx(pointer.c_str(), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked())
            selected_ = pointer;
        if (leaf || !expanded)
            return;
        if (node.contains("children") && node["children"].is_array())
            for (std::size_t i = 0; i < node["children"].size(); ++i)
                Tree(node["children"][i], pointer + "/children/" + std::to_string(i));
        if (node.contains("entry"))
            Tree(node["entry"], pointer + "/entry");
        ImGui::TreePop();
    }

    void UiPanel::Fields(Json &node)
    {
        // Each field with a widget that fits its value; changes apply when editing ends.
        std::string removed;
        for (auto &[key, value] : node.items())
        {
            if (key == "children" || key == "entry")
                continue;
            ImGui::PushID(key.c_str());
            std::string text;
            const auto options = key == "style" ? ui_.GetSkin().Names() : fieldValues.contains(key) ? fieldValues.at(key) : std::vector<std::string>{};
            if (value.is_string() && !options.empty())
            {
                if (widgets::Choose(key.c_str(), value.get<std::string>(), options, text))
                    Change([&] { value = text; });
            }
            else if (value.is_string())
            {
                if (widgets::EditText(key.c_str(), value.get<std::string>(), text))
                    Change([&] { value = text; });
            }
            else if (value.is_boolean())
            {
                bool on = value.get<bool>();
                if (ImGui::Checkbox(key.c_str(), &on))
                    Change([&] { value = on; });
            }
            else if (value.is_number())
            {
                float number = value.get<float>();
                ImGui::DragFloat(key.c_str(), &number, 1.0F);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    Change([&] { value = key == "columns" ? Json(static_cast<int>(number)) : Json(number); });
            }
            else if (value.is_array() && value.size() == 2 && value[0].is_number() && value[1].is_number())
            {
                float pair[2] = {value[0].get<float>(), value[1].get<float>()};
                ImGui::DragFloat2(key.c_str(), pair, 1.0F);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    Change([&] { value = Json::array({pair[0], pair[1]}); });
            }
            else if (widgets::EditText(key.c_str(), value.dump(), text)) // anything else as JSON
            {
                const Json parsed = Json::parse(text, nullptr, false);
                if (!parsed.is_discarded())
                    Change([&] { value = parsed; });
            }
            if (key != "type")
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                    removed = key;
            }
            ImGui::PopID();
        }
        if (!removed.empty())
            Change([&] { node.erase(removed); });
        // Fields it doesn't have yet.
        std::vector<std::string> missing;
        for (const std::string &field : fieldNames)
            if (!node.contains(field))
                missing.push_back(field);
        if (std::string field; widgets::Choose("Add field", "", missing, field))
            Change([&] { node[field] = NewValue(field); });
    }

    void UiPanel::Save(EditorHost &host)
    {
        // Into the project: a plugin's screen becomes a project copy that the game uses instead.
        const std::filesystem::path target = host.AssetRoot().Project() / file_;
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream out(target);
        if (!(out << json_.dump(2) << "\n"))
        {
            host.Message("Could not write " + target.generic_string());
            return;
        }
        changed_ = false;
        host.Message("Saved " + file_ + " in the project");
        host.RescanAssets();
    }

    void UiPanel::Draw(EditorHost &host)
    {
        if (!loaded_ && models_ == nullptr)
            Load(host);
        ImGui::SetNextWindowSize(ImVec2(440, 620), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(Title(), &open))
        {
            ImGui::End();
            return;
        }
        // 1. Which screen, how it is shown.
        std::vector<std::string> names;
        for (const auto &[name, screen] : ui_.Screens())
            names.push_back(name);
        if (std::string name; widgets::Choose("Screen", screen_, names, name) && name != screen_)
            Pick(name);
        ImGui::SameLine();
        if (ImGui::Button("Reload"))
            Load(host);
        ImGui::Checkbox("Show in the scene view", &showInView_);
        ImGui::SameLine();
        ImGui::Checkbox("HUD underneath", &withHud_);
        ImGui::TextDisabled("Data: ui/sample.json. Saving writes %s into the project.", file_.c_str());
        for (const umbra::core::Diagnostic &d : loadProblems_)
            ImGui::TextColored(ImVec4(1.0F, 0.5F, 0.4F, 1.0F), "%s", umbra::core::Format(d).c_str());
        if (screen_.empty() || !json_.is_object())
        {
            ImGui::TextDisabled(screen_.empty() ? "No ui/*.ui.json screens in the project or its plugins." : "This screen file is not valid JSON.");
            ImGui::End();
            return;
        }

        // 2. Save, undo, revert; problems with the edited file (the preview keeps the last good one).
        ImGui::BeginDisabled(!changed_);
        if (ImGui::Button("Save"))
            Save(host);
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
            Pick(screen_);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(undo_.empty());
        if (ImGui::Button("Undo"))
        {
            json_ = undo_.back();
            undo_.pop_back();
            changed_ = true;
            Check();
        }
        ImGui::EndDisabled();
        for (const umbra::core::Diagnostic &d : problems_)
            ImGui::TextColored(ImVec4(1.0F, 0.5F, 0.4F, 1.0F), "%s", umbra::core::Format(d).c_str());

        // 3. The screen's own settings, the widget tree and the selected widget's fields.
        if (ImGui::CollapsingHeader("Screen"))
        {
            for (const char *flag : {"modal", "dim"})
            {
                bool on = json_.value(flag, false);
                if (ImGui::Checkbox(flag, &on))
                    Change([&] { json_[flag] = on; });
            }
            if (std::string back; widgets::EditText("back (Esc action)", json_.value("back", ""), back))
                Change([&] { json_["back"] = back; });
        }
        ImGui::SeparatorText("Widgets");
        ImGui::BeginChild("tree", ImVec2(0, 200), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
        if (json_.contains("root"))
            Tree(json_["root"], "/root");
        ImGui::EndChild();

        if (!json_.contains(Json::json_pointer(selected_)))
            selected_ = "/root";
        Json &node = json_[Json::json_pointer(selected_)];
        ImGui::SeparatorText("Selected widget");
        // Structure: add a child (panel, stack), delete or move this one among its siblings.
        const std::string type = node.value("type", "");
        if (type == "panel" || type == "stack")
            if (std::string child; widgets::Choose("Add child", "", fieldValues.at("type"), child))
                Change([&] { node["children"].push_back({{"type", child}, {"text", child == "text" || child == "button" ? "New" : ""}}); });
        bool moved = false; // the selected widget moved or went away: the fields wait a frame
        if (selected_ != "/root")
        {
            const std::string parentPath = selected_.substr(0, selected_.rfind('/'));
            const bool inChildren = parentPath.ends_with("/children");
            if (ImGui::Button("Delete"))
            {
                const std::string key = selected_.substr(selected_.rfind('/') + 1);
                Change([&] {
                    Json &parent = json_[Json::json_pointer(parentPath)];
                    if (inChildren)
                        parent.erase(std::stoul(key));
                    else
                        parent.erase(key);
                });
                selected_ = inChildren ? parentPath.substr(0, parentPath.rfind('/')) : parentPath;
                moved = true;
            }
            if (inChildren)
            {
                const std::size_t index = std::stoul(selected_.substr(selected_.rfind('/') + 1));
                const std::size_t count = json_[Json::json_pointer(parentPath)].size();
                for (const auto &[label, to] : {std::pair{"Up", index - 1}, std::pair{"Down", index + 1}})
                {
                    if (moved)
                        break;
                    ImGui::SameLine();
                    ImGui::BeginDisabled(to >= count); // index 0 - 1 wraps to a huge number
                    if (ImGui::Button(label))
                    {
                        Change([&] { std::swap(json_[Json::json_pointer(parentPath)][index], json_[Json::json_pointer(parentPath)][to]); });
                        selected_ = parentPath + "/" + std::to_string(to);
                        moved = true;
                    }
                    ImGui::EndDisabled();
                }
            }
        }
        if (moved)
        {
            ImGui::End();
            return;
        }
        if (type == "list" && !node.contains("entry"))
            if (ImGui::Button("Add entry widget"))
                Change([&] { node["entry"] = {{"type", "button"}, {"text", "{item}"}}; });
        Fields(json_[Json::json_pointer(selected_)]);
        ImGui::End();
    }
}

UMBRA_EDITOR_PANEL(UiPanel);
