#include "Editor.hpp"

#include <Log.hpp>
#include <ModelInstance.hpp>
#include <SceneEdits.hpp>
#include <SceneFile.hpp>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <ImGuizmo.h> // needs imgui.h first

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

using umbra::authoring::Change;
using umbra::authoring::Json;
namespace authoring = umbra::authoring;

namespace
{
    // Paths as UTF-8 text and back. std::string paths would use the Windows code page (roadmap T37).
    std::string Utf8(const std::filesystem::path &path)
    {
        const std::u8string text = path.generic_u8string();
        return {text.begin(), text.end()};
    }
    std::filesystem::path FromUtf8(const std::string &text) { return std::u8string(text.begin(), text.end()); }

    // Reads [x, y, z] (or [x, y, z, w]) from an entity field, with a default.
    glm::vec3 Vec3(const Json &entity, const char *key, glm::vec3 fallback)
    {
        const auto it = entity.find(key);
        if (it == entity.end() || !it->is_array() || it->size() < 3)
            return fallback;
        return {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()};
    }

    glm::quat Rotation(const Json &entity)
    {
        const auto it = entity.find("rotation");
        if (it == entity.end() || !it->is_array() || it->size() < 4)
            return glm::identity<glm::quat>();
        return glm::normalize(glm::quat((*it)[3].get<float>(), (*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()));
    }

    // Matrix -> translation, rotation, scale (no shear: what the scene format can store).
    void Decompose(const glm::mat4 &m, glm::vec3 &position, glm::quat &rotation, glm::vec3 &scale)
    {
        position = glm::vec3(m[3]);
        scale = {glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))};
        const glm::mat3 r{glm::vec3(m[0]) / scale.x, glm::vec3(m[1]) / scale.y, glm::vec3(m[2]) / scale.z};
        rotation = glm::normalize(glm::quat_cast(r));
    }

    // A text field that reports a change once, when the user finishes editing it.
    bool EditText(const char *label, const std::string &current, std::string &result)
    {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), "%s", current.c_str());
        ImGui::InputText(label, buffer, sizeof(buffer));
        if (!ImGui::IsItemDeactivatedAfterEdit())
            return false;
        result = buffer;
        return true;
    }

    // A combo over `options`; returns true and sets `result` when the user picks one.
    bool Choose(const char *label, const std::string &current, const std::vector<std::string> &options, std::string &result)
    {
        bool picked = false;
        if (ImGui::BeginCombo(label, current.c_str()))
        {
            for (const std::string &option : options)
                if (ImGui::Selectable(option.c_str(), option == current))
                {
                    result = option;
                    picked = true;
                }
            ImGui::EndCombo();
        }
        return picked;
    }

    // Slab test: the ray hits the box [min, max] at distance t (in ray-parameter units).
    std::optional<float> RayBox(glm::vec3 origin, glm::vec3 direction, glm::vec3 min, glm::vec3 max)
    {
        float tNear = -1e30F, tFar = 1e30F;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::abs(direction[axis]) < 1e-8F)
            {
                if (origin[axis] < min[axis] || origin[axis] > max[axis])
                    return std::nullopt;
                continue;
            }
            float t1 = (min[axis] - origin[axis]) / direction[axis];
            float t2 = (max[axis] - origin[axis]) / direction[axis];
            if (t1 > t2)
                std::swap(t1, t2);
            tNear = std::max(tNear, t1);
            tFar = std::min(tFar, t2);
            if (tNear > tFar)
                return std::nullopt;
        }
        if (tFar < 0.0F)
            return std::nullopt;
        return std::max(tNear, 0.0F);
    }
}

umbra::core::Camera OrbitCamera::ToCamera() const
{
    // Spherical coordinates around the target.
    const glm::vec3 offset{std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
    umbra::core::Camera camera;
    camera.target = target;
    camera.position = target + offset * distance;
    camera.farPlane = 500.0F;
    return camera;
}

Editor::Editor(ModelLibrary &models, std::filesystem::path assetRoot, std::filesystem::path gameExecutable)
    : models_(models), assetRoot_(std::move(assetRoot)), gameExecutable_(std::move(gameExecutable))
{
    // The RPG content is open the whole time: forms edit it, scene pickers read its ids.
    rpg_ = std::make_unique<authoring::Document>(assetRoot_ / "rpg/reference.rpg.json", authoring::RpgValidator());
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!rpg_->Open(diagnostics))
        Report(diagnostics, "RPG content did not open");
    ScanAssets();
    LoadDraftSettings();
    LoadModelSettings();
    ResumeModel();
}

Editor::~Editor()
{
    modelJob_->Leave(); // ComfyUI finishes the model; the next start fetches it (ResumeModel)
    ClearTexturePreviews();
}

// ---------------- Edits ----------------

bool Editor::Edit(authoring::Document &doc, const std::vector<Change> &changes, const std::string &label, bool merge)
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (doc.Apply(changes, label, diagnostics, std::nullopt, merge))
    {
        lastEdited_ = &doc;
        return true;
    }
    Report(diagnostics, "Edit refused");
    return false;
}

bool Editor::SetField(const std::string &path, const Json &value, const std::string &label, bool merge)
{
    return scene_ && Edit(*scene_, {{Change::Op::Set, path, value}}, label, merge);
}

void Editor::Report(const std::vector<umbra::core::Diagnostic> &diagnostics, const std::string &what)
{
    problems_ = diagnostics;
    message_ = what + (diagnostics.empty() ? "" : ": " + diagnostics.front().message);
    messageSeconds_ = 5.0F;
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Info(umbra::core::Format(d));
}

void Editor::Save()
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    bool ok = true;
    if (scene_ && scene_->IsDirty())
        ok &= scene_->Save(diagnostics);
    if (rpg_ && rpg_->IsOpen() && rpg_->IsDirty())
        ok &= rpg_->Save(diagnostics);
    if (ok)
    {
        message_ = "Saved";
        messageSeconds_ = 2.0F;
        changedOnDisk_ = false;
    }
    else
        Report(diagnostics, "Save failed");
}

void Editor::Undo()
{
    // Undo follows the document edited last (scene or RPG content), then the other one.
    authoring::Document *other = lastEdited_ == rpg_.get() ? scene_.get() : rpg_.get();
    for (authoring::Document *doc : {lastEdited_, other})
        if (doc && doc->CanUndo())
        {
            message_ = "Undo " + doc->UndoLabel();
            messageSeconds_ = 1.5F;
            doc->Undo();
            return;
        }
}

void Editor::Redo()
{
    authoring::Document *other = lastEdited_ == rpg_.get() ? scene_.get() : rpg_.get();
    for (authoring::Document *doc : {lastEdited_, other})
        if (doc && doc->CanRedo())
        {
            message_ = "Redo " + doc->RedoLabel();
            messageSeconds_ = 1.5F;
            doc->Redo();
            return;
        }
}

void Editor::Play()
{
    // The game plays the scene as it is on disk, so save first.
    if (!scene_)
        return;
    if (scene_->IsDirty() || rpg_->IsDirty())
        Save();
    if (scene_->IsDirty())
        return;
    const std::string game = Utf8(gameExecutable_);
    const char *arguments[] = {game.c_str(), scenePath_.c_str(), nullptr};
    SDL_Process *process = SDL_CreateProcess(arguments, false);
    if (process == nullptr)
    {
        message_ = std::string{"Cannot start the game: "} + SDL_GetError();
        messageSeconds_ = 5.0F;
        return;
    }
    SDL_DestroyProcess(process); // only our handle; the game keeps running
    message_ = "Playing " + scenePath_;
    messageSeconds_ = 3.0F;
}

// ---------------- Scene and preview world ----------------

bool Editor::OpenScene(const std::string &scenePath)
{
    // Scenes can place whole models ("instance"), so the check needs to load them like the game does.
    umbra::scene::SceneLoadOptions options;
    options.assetRoot = assetRoot_;
    options.models = [this](const std::string &path) -> const umbra::assets::Model * {
        const LoadedModel *loaded = models_.Load(path);
        return loaded ? &loaded->model : nullptr;
    };
    auto doc = std::make_unique<authoring::Document>(assetRoot_ / FromUtf8(scenePath), authoring::SceneValidator(options));
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!doc->Open(diagnostics))
    {
        Report(diagnostics, "Cannot open " + scenePath);
        return false;
    }
    scene_ = std::move(doc);
    scenePath_ = scenePath;
    selected_.clear();
    builtRevision_ = 0;
    changedOnDisk_ = false;
    problems_.clear();
    RebuildWorld();
    return true;
}

void Editor::RebuildWorld()
{
    // The preview is rebuilt from the document text, with the game's own loader.
    umbra::scene::SceneLoadOptions options;
    options.assetRoot = assetRoot_;
    options.models = [&](const std::string &path) -> const umbra::assets::Model * {
        const LoadedModel *loaded = models_.Load(path);
        return loaded ? &loaded->model : nullptr;
    };
    auto fresh = std::make_unique<umbra::world::World>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!umbra::scene::LoadSceneText(scene_->Text(), scene_->File(), options, *fresh, diagnostics))
    {
        Report(diagnostics, "Preview failed");
        return;
    }
    fresh->UpdateTransforms();
    umbra::scene::BindRenderables(*fresh, [&](const std::string &model, std::uint32_t mesh) { return models_.Resolve(model, mesh); },
                                  [&](const std::string &material) { return models_.ResolveMaterial(material); });
    world_ = std::move(fresh);
    builtRevision_ = scene_->Revision();
}

void Editor::Update(float deltaSeconds, umbra::core::DrawList &drawList)
{
    messageSeconds_ = std::max(0.0F, messageSeconds_ - deltaSeconds);

    // 1. Outside changes to the file are noticed once a second.
    diskCheckSeconds_ -= deltaSeconds;
    if (scene_ && diskCheckSeconds_ <= 0.0F)
    {
        diskCheckSeconds_ = 1.0F;
        changedOnDisk_ = scene_->ChangedOnDisk();
    }

    // 2. A new document state (edit, undo, redo) means a new preview.
    if (scene_ && scene_->Revision() != builtRevision_)
        RebuildWorld();

    // 3. Draw: the preview world, a ground grid, the selection box.
    drawList.Clear();
    drawList.camera = camera_.ToCamera();
    drawList.hud.diagnosticsWindow = false;
    if (world_)
        world_->ExtractDrawList(drawList);
    ApplyTexturePreview(drawList);
    const glm::vec4 gridColor{0.35F, 0.35F, 0.4F, 1.0F};
    for (int i = -20; i <= 20; ++i)
    {
        const float f = static_cast<float>(i);
        drawList.lines.push_back({{f, 0.0F, -20.0F}, {f, 0.0F, 20.0F}, gridColor});
        drawList.lines.push_back({{-20.0F, 0.0F, f}, {20.0F, 0.0F, f}, gridColor});
    }
    AddSelectionBox(drawList);
}

glm::mat4 Editor::View() const
{
    const umbra::core::Camera camera = camera_.ToCamera();
    return glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
}

glm::mat4 Editor::GlProjection() const
{
    const umbra::core::Camera camera = camera_.ToCamera();
    return glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), displaySize_.x / std::max(displaySize_.y, 1.0F),
                                 camera.nearPlane, camera.farPlane);
}

std::optional<std::string> Editor::Pick(float mouseX, float mouseY) const
{
    if (!world_ || !scene_)
        return std::nullopt;
    // 1. Mouse -> a ray in world space (unproject the near and far points). Picking.md has the math.
    const glm::vec2 ndc{2.0F * mouseX / displaySize_.x - 1.0F, 1.0F - 2.0F * mouseY / displaySize_.y};
    const glm::mat4 inverse = glm::inverse(GlProjection() * View());
    glm::vec4 nearPoint = inverse * glm::vec4(ndc, -1.0F, 1.0F);
    glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0F, 1.0F);
    const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
    const glm::vec3 direction = glm::vec3(farPoint) / farPoint.w - origin;

    // 2. Test each drawn entity's mesh box in its own space; keep the closest hit.
    std::optional<float> best;
    umbra::world::Entity hit;
    world_->ForEach([&](umbra::world::Entity entity) {
        const umbra::world::Renderable *look = world_->GetRenderable(entity);
        const glm::mat4 *matrix = world_->GetWorldMatrix(entity);
        if (look == nullptr || matrix == nullptr)
            return;
        glm::vec3 min{-1.0F}, max{1.0F}; // the sample cube; real bounds below when the mesh is loaded
        if (const LoadedModel *loaded = models_.Load(look->model); loaded && look->mesh < loaded->model.meshes.size())
        {
            const auto &vertices = loaded->model.meshes[look->mesh].data.vertices;
            if (!vertices.empty())
            {
                min = max = vertices[0].position;
                for (const auto &v : vertices)
                    min = glm::min(min, v.position), max = glm::max(max, v.position);
            }
        }
        const glm::mat4 toLocal = glm::inverse(*matrix);
        const std::optional<float> t = RayBox(glm::vec3(toLocal * glm::vec4(origin, 1.0F)), glm::vec3(toLocal * glm::vec4(direction, 0.0F)), min, max);
        if (t && (!best || *t < *best))
            best = t, hit = entity;
    });
    if (!best)
        return std::nullopt;

    // 3. Instances create entities that aren't in the file; select the nearest ancestor that is.
    for (umbra::world::Entity e = hit; e; e = world_->GetParent(e))
        if (authoring::FindEntity(scene_->Data(), *world_->GetId(e)))
            return *world_->GetId(e);
    return std::nullopt;
}

void Editor::AddSelectionBox(umbra::core::DrawList &drawList) const
{
    if (!world_ || selected_.empty())
        return;
    const umbra::world::Entity entity = world_->Find(selected_);
    const glm::mat4 *matrix = world_->GetWorldMatrix(entity);
    if (matrix == nullptr)
        return;
    // The -1..1 box through the entity's transform: 12 edges.
    const glm::vec4 color{1.0F, 0.75F, 0.1F, 1.0F};
    glm::vec3 corner[8];
    for (int i = 0; i < 8; ++i)
        corner[i] = glm::vec3(*matrix * glm::vec4(i & 1 ? 1.0F : -1.0F, i & 2 ? 1.0F : -1.0F, i & 4 ? 1.0F : -1.0F, 1.0F));
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto &edge : edges)
        drawList.lines.push_back({corner[edge[0]], corner[edge[1]], color});
}

void Editor::ViewportInput()
{
    ImGuiIO &io = ImGui::GetIO();
    displaySize_ = {io.DisplaySize.x, io.DisplaySize.y};
    if (io.WantCaptureMouse)
        return; // the mouse is over a window
    // Right drag turns, middle drag pans, wheel zooms; a left click (not on the gizmo) selects.
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        camera_.yaw -= io.MouseDelta.x * 0.005F;
        camera_.pitch = std::clamp(camera_.pitch - io.MouseDelta.y * 0.005F, -1.5F, 1.5F);
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const glm::mat4 view = View();
        const glm::vec3 right{view[0][0], view[1][0], view[2][0]};
        const glm::vec3 up{view[0][1], view[1][1], view[2][1]};
        const float speed = camera_.distance * 0.0015F;
        camera_.target += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * speed;
    }
    if (io.MouseWheel != 0.0F)
        camera_.distance = std::clamp(camera_.distance * std::pow(0.88F, io.MouseWheel), 0.5F, 300.0F);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
        selected_ = Pick(io.MousePos.x, io.MousePos.y).value_or("");
}

void Editor::DrawGizmo()
{
    const std::optional<std::size_t> index = SelectedIndex();
    if (!index || !world_)
        return;
    const umbra::world::Entity entity = world_->Find(selected_);
    const glm::mat4 *matrix = world_->GetWorldMatrix(entity);
    if (matrix == nullptr)
        return;

    // 1. Manipulate the world matrix with ImGuizmo (OpenGL-style matrices, full window).
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0.0F, 0.0F, displaySize_.x, displaySize_.y);
    const glm::mat4 view = View();
    const glm::mat4 projection = GlProjection();
    glm::mat4 world = *matrix;
    const ImGuizmo::OPERATION operations[] = {ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE};
    const bool changed = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operations[gizmoOperation_],
                                              ImGuizmo::LOCAL, glm::value_ptr(world));
    const bool using_ = ImGuizmo::IsUsing();
    if (using_ && !gizmoWasUsing_)
        newStep_ = true; // a new drag: a new undo step
    gizmoWasUsing_ = using_;
    if (!changed)
        return;

    // 2. Back to the local transform (relative to the parent), then one merged edit per drag.
    const umbra::world::Entity parent = world_->GetParent(entity);
    const glm::mat4 *parentMatrix = parent ? world_->GetWorldMatrix(parent) : nullptr;
    const glm::mat4 local = parentMatrix ? glm::inverse(*parentMatrix) * world : world;
    glm::vec3 position, scale;
    glm::quat rotation;
    Decompose(local, position, rotation, scale);
    Edit(*scene_, authoring::SetTransform(*index, position, rotation, scale), "Transform " + selected_, !newStep_);
    newStep_ = false;
}

// ---------------- Windows ----------------

void Editor::DrawUi()
{
    ImGuizmo::BeginFrame();
    ViewportInput();
    Shortcuts();
    MenuBar();
    Hierarchy();
    Inspector();
    AssetBrowser();
    DiagnosticsPanel();
    RpgForms();
    AiDraft();
    AiTexture();
    CollectModelResults();
    AiModel();
    StatusBar();
    Popups();
    DrawGizmo();
}

void Editor::Shortcuts()
{
    if (ImGui::GetIO().WantTextInput)
        return;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        Undo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
        Redo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        Save();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D))
        DuplicateSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        DeleteSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_F5))
        Play();
    if (ImGui::GetIO().KeyCtrl)
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_W))
        gizmoOperation_ = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        gizmoOperation_ = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        gizmoOperation_ = 2;
    if (ImGui::IsKeyPressed(ImGuiKey_F) && world_ && !selected_.empty())
        if (const glm::mat4 *m = world_->GetWorldMatrix(world_->Find(selected_)))
            camera_.target = glm::vec3((*m)[3]); // focus the selection
}

void Editor::MenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New scene..."))
            openNewScene_ = true; // the popup lives in Popups()
        if (ImGui::MenuItem("Save", "Ctrl+S", false, scene_ != nullptr))
            Save();
        if (ImGui::MenuItem("Reload from disk", nullptr, false, scene_ != nullptr))
            pendingOpen_ = scenePath_;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit"))
            RequestQuit();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z"))
            Undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y"))
            Redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Add entity", nullptr, false, scene_ != nullptr))
            AddEntity(std::nullopt);
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !selected_.empty()))
            DuplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, !selected_.empty()))
            DeleteSelected();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("RPG content", nullptr, &showRpg_);
        ImGui::MenuItem("AI quest draft", nullptr, &showDraft_);
        ImGui::MenuItem("AI texture", nullptr, &showTexture_);
        ImGui::MenuItem("AI model", nullptr, &showModel_);
        ImGui::EndMenu();
    }
    const char *names[] = {"Move (W)", "Rotate (E)", "Scale (R)"};
    for (int i = 0; i < 3; ++i)
        if (ImGui::RadioButton(names[i], gizmoOperation_ == i))
            gizmoOperation_ = i;
    ImGui::Separator();
    if (ImGui::Button("Play (F5)"))
        Play();
    if (ImGui::Button("AI quest draft"))
        showDraft_ = true;
    if (ImGui::Button("AI texture"))
        showTexture_ = true;
    if (ImGui::Button(modelJob_->Running() ? "AI model (working)###AiModel" : "AI model###AiModel"))
        showModel_ = true;
    ImGui::EndMainMenuBar();
}

void Editor::Hierarchy()
{
    ImGui::SetNextWindowPos(ImVec2(0, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy") || !scene_)
    {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Add"))
        AddEntity(std::nullopt);
    ImGui::SameLine();
    if (ImGui::Button("Add child") && !selected_.empty())
        AddEntity(selected_);
    ImGui::Separator();
    // Roots first; children nest under their parent.
    for (const Json &entity : scene_->Data()["entities"])
        if (!entity.contains("parent"))
            HierarchyNode(entity.value("id", ""));
    ImGui::End();
}

void Editor::HierarchyNode(const std::string &id)
{
    const std::vector<std::string> children = authoring::ChildrenOf(scene_->Data(), id);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (id == selected_)
        flags |= ImGuiTreeNodeFlags_Selected;
    const bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", id.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        selected_ = id;
    if (open)
    {
        for (const std::string &child : children)
            HierarchyNode(child);
        ImGui::TreePop();
    }
}

void Editor::Inspector()
{
    ImGui::SetNextWindowPos(ImVec2(displaySize_.x - 340, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 560), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Inspector");
    const std::optional<std::size_t> index = SelectedIndex();
    if (!visible || !index)
    {
        if (visible)
            ImGui::TextDisabled("Select an entity (click it or pick it in the hierarchy).");
        ImGui::End();
        return;
    }
    const Json entity = scene_->Data()["entities"][*index]; // copy: edits replace the document
    const std::string at = authoring::EntityPath(*index);
    std::string text;

    // 1. Identity.
    if (EditText("Id", selected_, text) && text != selected_)
    {
        if (Edit(*scene_, authoring::RenameEntity(scene_->Data(), selected_, text), "Rename " + selected_))
            selected_ = text;
    }
    if (EditText("Name", entity.value("name", ""), text))
        SetField(at + "/name", text, "Name " + selected_);

    // 2. Transform: drags make one undo step each.
    glm::vec3 position = Vec3(entity, "position", glm::vec3{0.0F});
    glm::vec3 euler = glm::degrees(glm::eulerAngles(Rotation(entity)));
    glm::vec3 scale = Vec3(entity, "scale", glm::vec3{1.0F});
    bool moved = ImGui::DragFloat3("Position", &position.x, 0.05F);
    if (ImGui::IsItemActivated())
        newStep_ = true;
    moved |= ImGui::DragFloat3("Rotation", &euler.x, 0.5F);
    if (ImGui::IsItemActivated())
        newStep_ = true;
    moved |= ImGui::DragFloat3("Scale", &scale.x, 0.02F);
    if (ImGui::IsItemActivated())
        newStep_ = true;
    if (moved)
    {
        Edit(*scene_, authoring::SetTransform(*index, position, glm::quat(glm::radians(euler)), scale), "Transform " + selected_, !newStep_);
        newStep_ = false;
    }

    // 3. Look.
    ImGui::SeparatorText("Model");
    bool hasModel = entity.contains("model");
    if (ImGui::Checkbox("Visible", &hasModel))
    {
        if (hasModel)
            Edit(*scene_, {{Change::Op::Set, at + "/model", "scenes/phase2.gltf"}, {Change::Op::Set, at + "/mesh", 0}}, "Model " + selected_);
        else
        {
            std::vector<Change> changes{{Change::Op::Remove, at + "/model", {}}};
            for (const char *key : {"mesh", "color", "material"})
                if (entity.contains(key))
                    changes.push_back({Change::Op::Remove, at + "/" + key, {}});
            Edit(*scene_, changes, "Model " + selected_);
        }
    }
    if (hasModel && entity.contains("model"))
    {
        if (Choose("File", entity.value("model", ""), assets_["Models"], text))
            SetField(at + "/model", text, "Model " + selected_);
        int mesh = entity.value("mesh", 0);
        if (ImGui::InputInt("Mesh", &mesh) && mesh >= 0)
            SetField(at + "/mesh", mesh, "Mesh " + selected_);
        glm::vec4 color{1.0F};
        if (const auto it = entity.find("color"); it != entity.end() && it->size() == 4)
            color = {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), (*it)[3].get<float>()};
        if (ImGui::ColorEdit4("Color", &color.x))
        {
            if (ImGui::IsItemActivated())
                newStep_ = true;
            SetField(at + "/color", Json::array({color.r, color.g, color.b, color.a}), "Color " + selected_, !newStep_);
            newStep_ = false;
        }
        // Material: "" = the model's own texture.
        std::vector<std::string> materials{""};
        materials.insert(materials.end(), assets_["Materials"].begin(), assets_["Materials"].end());
        if (Choose("Material", entity.value("material", ""), materials, text))
        {
            if (text.empty() && entity.contains("material"))
                Edit(*scene_, {{Change::Op::Remove, at + "/material", {}}}, "Material " + selected_);
            else if (!text.empty())
                SetField(at + "/material", text, "Material " + selected_);
        }
    }

    // 4. Optional parts: a checkbox adds the default, the fields edit it.
    const auto part = [&](const char *key, const char *label, const Json &defaults) {
        bool has = entity.contains(key);
        ImGui::SeparatorText(label);
        ImGui::PushID(key);
        if (ImGui::Checkbox("Enabled", &has))
        {
            if (has)
                SetField(at + "/" + key, defaults, std::string{label} + " " + selected_);
            else
                Edit(*scene_, {{Change::Op::Remove, at + "/" + key, {}}}, std::string{label} + " " + selected_);
        }
        ImGui::PopID();
        return entity.contains(key) && has;
    };
    if (part("collider", "Collider", Json::object()))
    {
        const Json &c = entity["collider"];
        if (Choose("Shape", c.value("shape", "box"), {"box", "sphere", "capsule"}, text))
            SetField(at + "/collider/shape", text, "Collider " + selected_);
        if (Choose("Motion", c.value("motion", "static"), {"static", "kinematic", "dynamic"}, text))
            SetField(at + "/collider/motion", text, "Collider " + selected_);
    }
    if (part("interactable", "Interactable", Json{{"prompt", "Use"}}))
    {
        const Json &i = entity["interactable"];
        if (EditText("Prompt", i.value("prompt", ""), text))
            SetField(at + "/interactable/prompt", text, "Interactable " + selected_);
        if (EditText("Message", i.value("message", ""), text))
            SetField(at + "/interactable/message", text, "Interactable " + selected_);
        std::vector<std::string> sounds = assets_["Sounds"];
        if (Choose("Sound", i.value("sound", ""), sounds, text))
            SetField(at + "/interactable/sound", text, "Interactable " + selected_);
        std::vector<std::string> talks = RpgIds("conversations");
        talks.insert(talks.begin(), "");
        if (Choose("Conversation", i.value("conversation", ""), talks, text))
        {
            if (text.empty())
                Edit(*scene_, {{Change::Op::Remove, at + "/interactable/conversation", {}}}, "Interactable " + selected_);
            else
                SetField(at + "/interactable/conversation", text, "Interactable " + selected_);
        }
        bool save = i.value("save", false);
        if (ImGui::Checkbox("Save stone", &save))
            SetField(at + "/interactable/save", save, "Interactable " + selected_);
    }
    if (part("portal", "Portal", Json{{"scene", "scenes/settlement.scene.json"}, {"spawn", "player"}}))
    {
        const Json &p = entity["portal"];
        if (Choose("Scene", p.value("scene", ""), assets_["Scenes"], text))
            SetField(at + "/portal/scene", text, "Portal " + selected_);
        if (EditText("Spawn", p.value("spawn", ""), text))
            SetField(at + "/portal/spawn", text, "Portal " + selected_);
    }
    const std::vector<std::string> actors = RpgIds("actors");
    if (part("spawner", "Enemy spawner", Json{{"pool", Json::array({actors.empty() ? "wolf" : actors.back()})}, {"count", 1}}))
    {
        const Json &s = entity["spawner"];
        // The pool: one checkbox per actor in the RPG content.
        Json pool = s.value("pool", Json::array());
        for (const std::string &actor : actors)
        {
            bool in = std::find(pool.begin(), pool.end(), actor) != pool.end();
            if (ImGui::Checkbox(actor.c_str(), &in))
            {
                Json next = Json::array();
                for (const std::string &a : actors)
                    if (a == actor ? in : std::find(pool.begin(), pool.end(), a) != pool.end())
                        next.push_back(a);
                SetField(at + "/spawner/pool", next, "Spawner " + selected_);
            }
        }
        int count = s.value("count", 1), group = s.value("group", 3);
        float radius = s.value("radius", 3.0F);
        bool chase = s.value("chase", false);
        if (ImGui::InputInt("Count", &count))
            SetField(at + "/spawner/count", count, "Spawner " + selected_);
        if (ImGui::InputInt("Group", &group))
            SetField(at + "/spawner/group", group, "Spawner " + selected_);
        if (ImGui::InputFloat("Radius", &radius, 0.5F))
            SetField(at + "/spawner/radius", radius, "Spawner " + selected_);
        if (ImGui::Checkbox("Chases on sight", &chase))
            SetField(at + "/spawner/chase", chase, "Spawner " + selected_);
    }
    ImGui::End();
}

void Editor::AssetBrowser()
{
    ImGui::SetNextWindowPos(ImVec2(0, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Assets"))
    {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Refresh"))
        ScanAssets();
    ImGui::TextDisabled("Double-click a scene to open it.");
    for (const auto &[group, files] : assets_)
    {
        if (!ImGui::TreeNodeEx(group.c_str(), group == "Scenes" ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            continue;
        for (const std::string &file : files)
        {
            ImGui::Selectable(file.c_str(), group == "Scenes" && file == scenePath_);
            if (group == "Scenes" && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                pendingOpen_ = file;
            if (group == "RPG content" && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                showRpg_ = true;
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

void Editor::DiagnosticsPanel()
{
    if (problems_.empty())
        return;
    ImGui::SetNextWindowPos(ImVec2(280, displaySize_.y - 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Diagnostics");
    if (ImGui::Button("Clear"))
        problems_.clear();
    for (const umbra::core::Diagnostic &d : problems_)
    {
        const ImVec4 color = d.severity == umbra::core::Severity::Error ? ImVec4(1.0F, 0.45F, 0.35F, 1.0F) : ImVec4(1.0F, 0.85F, 0.4F, 1.0F);
        ImGui::TextColored(color, "[%s] %s: %s", d.code.c_str(), d.field.c_str(), d.message.c_str());
    }
    ImGui::End();
}

void Editor::StatusBar()
{
    // A thin bar along the bottom: file, unsaved mark, last message.
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 26));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 26));
    ImGui::Begin("##status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (scene_)
        ImGui::Text("%s%s%s", scenePath_.c_str(), scene_->IsDirty() ? " *" : "", rpg_->IsDirty() ? "  | RPG content *" : "");
    else
        ImGui::TextDisabled("No scene open. Double-click one in Assets.");
    if (changedOnDisk_)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.6F, 0.2F, 1), "  The file changed outside the editor.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload"))
            pendingOpen_ = scenePath_;
    }
    if (messageSeconds_ > 0.0F)
    {
        ImGui::SameLine();
        ImGui::Text("   %s", message_.c_str());
    }
    ImGui::End();
}

void Editor::Popups()
{
    // 1. Opening another scene (or reloading) with unsaved changes asks first.
    if (!pendingOpen_.empty())
    {
        if (!scene_ || !scene_->IsDirty())
        {
            OpenScene(std::exchange(pendingOpen_, {}));
        }
        else if (!ImGui::IsPopupOpen("Unsaved changes"))
            ImGui::OpenPopup("Unsaved changes");
    }
    if (askQuit_ && !ImGui::IsPopupOpen("Unsaved changes"))
        ImGui::OpenPopup("Unsaved changes");
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("There are unsaved changes (%s%s).", scenePath_.c_str(), rpg_->IsDirty() ? ", RPG content" : "");
        const auto proceed = [&] {
            if (askQuit_)
                quit_ = true;
            else
                OpenScene(std::exchange(pendingOpen_, {}));
            askQuit_ = false;
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::Button("Save"))
        {
            Save();
            if (!scene_->IsDirty())
                proceed();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            scene_.reset(); // the edits are dropped; nothing is written
            proceed();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            pendingOpen_.clear();
            askQuit_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. New scene: a file name under scenes/, created with just a player spawn.
    if (std::exchange(openNewScene_, false))
        ImGui::OpenPopup("New scene");
    if (ImGui::BeginPopupModal("New scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char name[128] = "scenes/new.scene.json";
        ImGui::InputText("File", name, sizeof(name));
        if (ImGui::Button("Create"))
        {
            const std::filesystem::path file = assetRoot_ / FromUtf8(name);
            if (std::filesystem::exists(file))
                message_ = std::string{name} + " already exists", messageSeconds_ = 4.0F;
            else
            {
                std::filesystem::create_directories(file.parent_path());
                std::ofstream(file, std::ios::binary)
                    << R"({"format": "umbra.scene", "version": 1, "entities": [{"id": "player", "name": "Player spawn"}]})" << "\n";
                ScanAssets();
                pendingOpen_ = name;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void Editor::RequestQuit()
{
    if ((scene_ && scene_->IsDirty()) || (rpg_ && rpg_->IsDirty()))
        askQuit_ = true;
    else
        quit_ = true;
}

// ---------------- Helpers ----------------

void Editor::ScanAssets()
{
    // Every content file under the asset folder, grouped by kind, as forward-slash paths.
    assets_.clear();
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(assetRoot_, ec); it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec || !it->is_regular_file())
            continue;
        const std::string path = Utf8(std::filesystem::relative(it->path(), assetRoot_));
        const std::string name = Utf8(it->path().filename());
        if (name.ends_with(".scene.json"))
            assets_["Scenes"].push_back(path);
        else if (name.ends_with(".rpg.json"))
            assets_["RPG content"].push_back(path);
        else if (name.ends_with(".gltf") || name.ends_with(".glb"))
            assets_["Models"].push_back(path);
        else if (name.ends_with(".wav"))
            assets_["Sounds"].push_back(path);
        else if (name.ends_with(".policy.json"))
            assets_["Learned AI"].push_back(path);
        else if (name.ends_with(".material.json"))
            assets_["Materials"].push_back(path);
        else if (name.ends_with(".png") || name.ends_with(".jpg"))
            assets_["Textures"].push_back(path);
    }
    for (auto &[group, files] : assets_)
        std::sort(files.begin(), files.end());
}

std::optional<std::size_t> Editor::SelectedIndex() const
{
    if (!scene_ || selected_.empty())
        return std::nullopt;
    return authoring::FindEntity(scene_->Data(), selected_);
}

std::vector<std::string> Editor::RpgIds(const char *section) const
{
    std::vector<std::string> ids;
    if (rpg_ && rpg_->IsOpen())
        for (const Json &entry : rpg_->Data().value(section, Json::array()))
            ids.push_back(entry.value("id", ""));
    return ids;
}

void Editor::AddEntity(const std::optional<std::string> &parent)
{
    if (!scene_)
        return;
    // A visible cube at the camera's focus point (or at the parent's origin).
    const std::string id = authoring::UniqueEntityId(scene_->Data(), "entity");
    Json entity{{"id", id}, {"name", "New entity"}};
    if (parent)
        entity["parent"] = *parent;
    else
        entity["position"] = Json::array({camera_.target.x, std::max(camera_.target.y, 0.5F), camera_.target.z});
    entity["scale"] = Json::array({0.5, 0.5, 0.5});
    entity["model"] = "scenes/phase2.gltf";
    entity["mesh"] = 0;
    if (Edit(*scene_, authoring::AddEntity(entity), "Add " + id))
        selected_ = id;
}

void Editor::DuplicateSelected()
{
    const std::optional<std::size_t> index = SelectedIndex();
    if (!index)
        return;
    // A copy of the entity (not its children), one meter to the side.
    Json copy = scene_->Data()["entities"][*index];
    const std::string id = authoring::UniqueEntityId(scene_->Data(), selected_);
    copy["id"] = id;
    glm::vec3 position = Vec3(copy, "position", glm::vec3{0.0F});
    copy["position"] = Json::array({position.x + 1.0F, position.y, position.z});
    if (Edit(*scene_, authoring::AddEntity(copy), "Duplicate " + selected_))
        selected_ = id;
}

void Editor::DeleteSelected()
{
    if (!SelectedIndex())
        return;
    if (Edit(*scene_, authoring::RemoveEntity(scene_->Data(), selected_), "Delete " + selected_))
        selected_.clear();
}
