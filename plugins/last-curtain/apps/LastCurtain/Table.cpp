#include "Table.hpp"

#include "Art.hpp"

#include <AnimGraph.hpp>
#include <GltfImporter.hpp>
#include <Lighting.hpp>
#include <Log.hpp>
#include <MeshData.hpp>
#include <TextureTools.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

using namespace umbra::night;

namespace
{
    constexpr float tile = 2.2F, tileThick = 0.15F;

    glm::vec4 Hex(std::uint32_t rgb)
    {
        return {static_cast<float>((rgb >> 16) & 255) / 255.0F, static_cast<float>((rgb >> 8) & 255) / 255.0F,
                static_cast<float>(rgb & 255) / 255.0F, 1.0F};
    }

    glm::vec4 RoomColor(RoomKind kind)
    {
        switch (kind)
        {
        case RoomKind::Foyer: return Hex(0xC9A227);
        case RoomKind::UnderStage: return Hex(0x7A1020);
        case RoomKind::Plain: return Hex(0x6B7280);
        case RoomKind::Item: return Hex(0x3B6FB6);
        case RoomKind::Event: return Hex(0x7C4DBF);
        case RoomKind::Trap: return Hex(0xB3261E);
        case RoomKind::Transport: return Hex(0x1F8A8A);
        case RoomKind::Bonus: return Hex(0x2F8F5B);
        }
        return Hex(0x6B7280);
    }


    // A 1 m cube: four vertices per face so each face has its own normal.
    umbra::core::MeshData Cube()
    {
        umbra::core::MeshData mesh;
        const glm::vec3 normals[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const glm::vec3 &n : normals)
        {
            // 1. Two axes across the face, then its four corners.
            const glm::vec3 u = std::abs(n.y) > 0.5F ? glm::vec3{1, 0, 0} : glm::vec3{0, 1, 0};
            const glm::vec3 v = glm::cross(n, u);
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            for (const glm::vec2 c : {glm::vec2{-1, -1}, glm::vec2{1, -1}, glm::vec2{1, 1}, glm::vec2{-1, 1}})
                mesh.vertices.push_back({0.5F * (n + c.x * u + c.y * v), n, (c + 1.0F) * 0.5F});
            // 2. Two triangles, wound so the face points along n.
            mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        return mesh;
    }

    // The turn arrow: a glowing gold diamond pointing down at `top`, bobbing with the clock.
    // A 1 m square facing +Z: u across, v down the picture; mirrored: u runs the other way.
    umbra::core::MeshData Quad(bool mirrored = false)
    {
        umbra::core::MeshData mesh;
        for (const glm::vec2 c : {glm::vec2{-0.5F, -0.5F}, glm::vec2{0.5F, -0.5F}, glm::vec2{0.5F, 0.5F}, glm::vec2{-0.5F, 0.5F}})
            mesh.vertices.push_back({{c.x, c.y, 0.0F}, {0.0F, 0.0F, 1.0F}, {mirrored ? 0.5F - c.x : c.x + 0.5F, 0.5F - c.y}});
        mesh.indices = {0, 1, 2, 0, 2, 3};
        return mesh;
    }

    void Arrow(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 top, float size, float clock)
    {
        const glm::vec3 at = top + glm::vec3{0.0F, size * (0.9F + 0.25F * std::sin(clock * 3.0F)), 0.0F};
        const glm::mat4 model = glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4{1.0F}, at), clock * 1.5F, {0, 1, 0}), 0.7854F, {0, 0, 1}),
                                           glm::vec3{size * 0.5F, size * 0.5F, size * 0.2F});
        out.items.push_back({mesh, {}, model, glm::vec4{0.95F, 0.8F, 0.3F, 1.0F}, 0.0F, 1.0F});
    }

    void Box(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 centre, glm::vec3 size, glm::vec4 color, float glow = 0.0F)
    {
        out.items.push_back({mesh, {}, glm::scale(glm::translate(glm::mat4{1.0F}, centre), size), color, 0.0F, glow});
    }

    // A room the player can step into: a thin glowing gold frame inside its edges, raised clear of the floor
    // (a flat plate on the floor flickered against it, the user).
    void Outline(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 centre, float size, float bar, float glow)
    {
        const float h = size * 0.5F - bar * 0.5F;
        for (const glm::vec3 side : {glm::vec3{0, 0, h}, glm::vec3{0, 0, -h}})
            Box(out, mesh, centre + side, {size, bar * 0.5F, bar}, Hex(0xD4AF37), glow);
        for (const glm::vec3 side : {glm::vec3{h, 0, 0}, glm::vec3{-h, 0, 0}})
            Box(out, mesh, centre + side, {bar, bar * 0.5F, size}, Hex(0xD4AF37), glow);
    }

    // A room's marker floating over it (Table.md "Markers"), each its own shape and colour, glowing:
    // an item a gold tablet, an event a violet diamond, a trap a red spike, a bonus a green plus.
    void Marker(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 at, float size, RoomMark mark, float clock)
    {
        if (mark == RoomMark::None)
            return;
        at.y += size * 0.15F * std::sin(clock * 2.0F);
        const glm::mat4 spin = glm::rotate(glm::translate(glm::mat4{1.0F}, at), clock * 0.8F, {0, 1, 0});
        const auto put = [&](const glm::mat4 &m, std::uint32_t rgb) { out.items.push_back({mesh, {}, m, Hex(rgb), 0.0F, 0.9F}); };
        switch (mark)
        {
        case RoomMark::Item: put(glm::scale(spin, glm::vec3{size * 0.6F, size * 0.6F, size * 0.12F}), 0xF2D58A); break;
        case RoomMark::Event: put(glm::scale(glm::rotate(spin, 0.7854F, {0, 0, 1}), glm::vec3{size * 0.5F, size * 0.5F, size * 0.15F}), 0xB28DFF); break;
        case RoomMark::Trap: put(glm::scale(spin, glm::vec3{size * 0.18F, size * 0.9F, size * 0.18F}), 0xD62828); break;
        case RoomMark::Bonus:
            put(glm::scale(spin, glm::vec3{size * 0.6F, size * 0.18F, size * 0.18F}), 0x5FC9A0);
            put(glm::scale(spin, glm::vec3{size * 0.18F, size * 0.6F, size * 0.18F}), 0x5FC9A0);
            break;
        default: break;
        }
    }
}

Table::Table(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets)
    : renderer_(renderer), box_(renderer.CreateMesh(Cube())), quad_(renderer.CreateMesh(Quad())), mirrorQuad_(renderer.CreateMesh(Quad(true))), guest_(Load(assets / "models/guest.glb", assets / "animations/guest.anim.json", 0.55F)),
      taken_(Load(assets / "models/taken.glb", assets / "animations/taken.anim.json", 0.65F))
{
}

Table::~Table()
{
    for (const auto *art : {&roomArt_, &cardArt_, &itemArt_, &plaqueArt_})
        for (const umbra::core::TextureHandle texture : *art)
            if (texture)
                renderer_.DestroyTexture(texture);
    for (const umbra::core::TextureHandle texture : skyArt_)
        if (texture)
            renderer_.DestroyTexture(texture);
    for (const Figure *figure : {&guest_, &taken_})
    {
        if (figure->mesh)
            renderer_.DestroyMesh(figure->mesh);
        if (figure->texture)
            renderer_.DestroyTexture(figure->texture);
    }
    for (const auto &[face, texture] : spriteArt_)
        if (texture)
            renderer_.DestroyTexture(texture);
    for (const umbra::core::MeshHandle mesh : {box_, quad_, mirrorQuad_})
        if (mesh)
            renderer_.DestroyMesh(mesh);
}

Table::Figure Table::Load(const std::filesystem::path &model, const std::filesystem::path &graph, float scale)
{
    // 1. The rigged model and its graph; a problem is logged and the pawn stays a box.
    umbra::assets::ImportResult imported = umbra::assets::ImportGltf(model);
    std::vector<umbra::core::Diagnostic> diagnostics;
    Figure figure;
    figure.graph = umbra::scene::LoadAnimGraph(graph, graph.filename().string(), diagnostics);
    for (const umbra::core::Diagnostic &d : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(d));
    if (!imported.Ok() || !imported.model.skeleton || imported.model.meshes.empty() || !figure.graph)
    {
        umbra::diagnostics::Error("Pawn figure " + model.string() + " could not be loaded; drawing a box");
        return {};
    }
    // 2. Its one skinned mesh and texture on the GPU (pack_animated.py makes one of each).
    figure.model = std::make_shared<const umbra::assets::Model>(std::move(imported.model));
    const umbra::assets::ModelMesh &mesh = figure.model->meshes.front();
    figure.mesh = renderer_.CreateMesh(mesh.data);
    if (mesh.texture >= 0)
        figure.texture = renderer_.CreateTexture(figure.model->textures[static_cast<std::size_t>(mesh.texture)]);
    figure.scale = scale;
    return figure;
}

void Table::LoadArt(const NightDef &def, const umbra::assets::AssetFolders &assets)
{
    int missing = 0;
    const auto load = [&](const char *set, const std::string &id) {
        umbra::core::TextureData data;
        std::string error;
        const bool ok = umbra::assets::LoadImageFile(assets / ArtPath(set, id), data, error);
        missing += ok ? 0 : 1;
        return ok ? renderer_.CreateTexture(data) : umbra::core::TextureHandle{};
    };
    for (const RoomDef &room : def.rooms)
        roomArt_.push_back(load("rooms", room.id)), plaqueArt_.push_back(load("plaques", room.id));
    skyArt_ = {load("sky", "before"), load("sky", "after")};
    for (const MonsterDef &monster : def.monsters)
        cardArt_.push_back(load("cards", monster.id));
    for (const ItemDef &item : def.items)
        itemArt_.push_back(load("items", item.id));
    // The guests' sprite (the template, Huy, for every guest until each has its own) and its motions.
    std::string error;
    motions_ = Motions::Load(assets / "motions.json", error);
    if (!error.empty())
        umbra::diagnostics::Error(error + "; the sprites stand still");
    for (const char *face : {"calm", "angry", "neardead", "happy", "scheming", "afraid", "talk"})
    {
        umbra::core::TextureData data;
        if (umbra::assets::LoadImageFile(assets / ("art/sprites/huy/body_" + std::string(face) + ".png"), data, error))
        {
            spriteAspect_ = static_cast<float>(data.width) / static_cast<float>(std::max(1U, data.height));
            spriteArt_[face] = renderer_.CreateTexture(data);
        }
        else
            ++missing;
    }
    if (missing > 0)
        umbra::diagnostics::Error(std::to_string(missing) + " table pictures are missing; those are boxes (Art.md)");
}

umbra::core::Animator Table::MakeAnimator(const Figure &figure)
{
    const std::shared_ptr<const umbra::assets::Model> model = figure.model;
    return {std::shared_ptr<const umbra::core::Skeleton>(model, &*model->skeleton), figure.graph, [model](const std::string &name) {
                for (const umbra::core::Clip &clip : model->clips)
                    if (clip.name == name)
                        return std::shared_ptr<const umbra::core::Clip>(model, &clip);
                return std::shared_ptr<const umbra::core::Clip>{};
            }};
}

void Table::Animate(const NightDef &def, const BoardView &view, const Slide &slide, float dt)
{
    for (const PawnView &pawn : view.pawns)
    {
        if (pawn.guest < 0)
            continue;
        if (static_cast<std::size_t>(pawn.guest) >= pawns_.size())
            pawns_.resize(static_cast<std::size_t>(pawn.guest) + 1);
        Pawn &p = pawns_[static_cast<std::size_t>(pawn.guest)];
        // 1. A new animator when the guest first shows, and again when it turns.
        const Figure &figure = pawn.taken ? taken_ : guest_;
        if (!figure.mesh)
            continue;
        if (!p.animator || p.taken != pawn.taken)
            p.animator.emplace(MakeAnimator(figure)), p.taken = pawn.taken;
        // 2. Sliding: face the step (the model faces -Z, so yaw = atan2(-x, -z)) and walk.
        const bool walking = pawn.guest == slide.guest && slide.from >= 0 && slide.to >= 0 && slide.from != slide.to;
        if (walking)
        {
            const glm::vec3 step = CellCenter(def, slide.to) - CellCenter(def, slide.from);
            p.yaw = std::atan2(-step.x, -step.z);
        }
        p.animator->SetNumber("speed", walking ? 1.0F : 0.0F);
        p.animator->Update(dt);
        // 3. The sprite: a ghost floats, a walker walks (facing its step on screen), near the killer it
        //    trembles, otherwise it stands idle (Motions.md).
        if (walking)
        {
            const float side = glm::dot(CellCenter(def, slide.to) - CellCenter(def, slide.from), CameraRight());
            p.sprite.facing = std::abs(side) > 0.01F ? (side > 0.0F ? 1.0F : -1.0F) : p.sprite.facing;
        }
        p.sprite.Base(pawn.ghost ? "ghost" : walking ? "walk" : view.killer >= 0 && view.killer == pawn.cell ? "fear" : "idle");
        p.sprite.Update(dt, motions_);
    }
    // The killer stands in the Taken's figure, idle.
    if (view.killer >= 0 && taken_.mesh)
    {
        if (!killer_.animator)
            killer_.animator.emplace(MakeAnimator(taken_));
        killer_.animator->SetNumber("speed", 0.0F);
        killer_.animator->Update(dt);
    }
}

void Table::Draw(const NightDef &def, const BoardView &view, const std::vector<int> &path, int affordable, const Slide &slide,
                 const std::vector<std::pair<int, int>> &rings, umbra::core::DrawList &out) const
{
    if (view_.RoomT() > 0.0F)
        return DrawRooms(def, view, path, affordable, slide, rings, out);
    // 1. Light: indoors, dim ambient (red in the hunt), one warm lamp over the middle of the table.
    umbra::core::EnvironmentSettings settings;
    settings.indoor = true;
    settings.ambient = 0.8F;
    settings.fog = 0.0F;
    settings.shadows = false;
    settings.tint = HuntTint();
    out.environment = umbra::core::EvaluateEnvironment(settings, 20.0F);
    out.lights.push_back({{0.0F, 7.0F, 0.0F}, {1.0F, 0.82F, 0.6F}, 30.0F * Lamp(), 40.0F, 0.0F});
    out.camera = Camera();
    Sky(out);

    // 2. Rooms: a floor tile, a wall stub on every side a guest cannot pass, a faint bar at each doorway.
    const int cells = def.rules.board.width * def.rules.board.height;
    const glm::vec3 side[] = {{0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}}; // N E S W
    for (int c = 0; c < cells; ++c)
    {
        const CellView &cell = view.cells[static_cast<std::size_t>(c)];
        if (!cell.opened)
            continue;
        const glm::vec3 at = CellCenter(def, c);
        const umbra::core::TextureHandle art = cell.room >= 0 && static_cast<std::size_t>(cell.room) < roomArt_.size() ? roomArt_[static_cast<std::size_t>(cell.room)] : umbra::core::TextureHandle{};
        if (!art)
            Box(out, box_, at - glm::vec3{0, tileThick * 0.5F, 0}, {tile, tileThick, tile}, cell.lit ? RoomColor(cell.kind) : Hex(0x2A2A33));
        else
        {
            // The frame (gold for a featured room, else the kind's colour), the painted tile on it; dim out of the light.
            const glm::vec4 shade = cell.lit ? glm::vec4{1.0F} : glm::vec4{0.32F, 0.32F, 0.36F, 1.0F};
            const bool featured = static_cast<std::size_t>(c) < featured_.size() && featured_[static_cast<std::size_t>(c)];
            const bool goal = std::find(goalCells_.begin(), goalCells_.end(), c) != goalCells_.end();
            Box(out, box_, at - glm::vec3{0, tileThick * 0.5F, 0}, {tile, tileThick, tile}, (featured || goal ? Hex(0xD4AF37) : RoomColor(cell.kind)) * shade,
                goal ? 0.6F + 0.4F * std::sin(clock_ * 3.0F) : 0.0F);
            const float inner = featured ? 0.84F : 0.9F;
            Floor(out, art, glm::scale(glm::translate(glm::mat4{1.0F}, at - glm::vec3{0, tileThick * 0.5F - 0.004F, 0}), {tile * inner, tileThick, tile * inner}), shade);
        }
        Plaque(out, cell.room, at + glm::vec3{0.0F, 0.008F, tile * 0.36F}, tile * 0.72F, cell.lit ? 1.0F : 0.4F);
        if (cell.step >= 0)
            Outline(out, box_, at + glm::vec3{0, 0.05F, 0}, tile * 0.8F, 0.07F, 0.5F + 0.3F * std::sin(clock_ * 3.0F));
        Marker(out, box_, at + glm::vec3{0.0F, 1.0F, 0.0F}, 0.35F, cell.mark, clock_);
        for (int d = 0; d < 4; ++d)
        {
            const bool across = d == 1 || d == 3;
            const glm::vec3 edge = at + side[d] * (tile * 0.5F);
            if (cell.walls & (1u << d)) // pale and glowing a little, so they read under the mood
                Box(out, box_, edge + glm::vec3{0, 0.17F, 0}, across ? glm::vec3{0.12F, 0.35F, tile} : glm::vec3{tile, 0.35F, 0.12F}, Hex(0x8A8272), readable_);
            else if (cell.doorways & (1u << d))
                Box(out, box_, edge + side[d] * 0.2F, across ? glm::vec3{0.3F, 0.03F, 0.6F} : glm::vec3{0.6F, 0.03F, 0.3F}, Hex(0xC9C1AD));
        }
    }

    // 3. The walk being picked: a gold dot per affordable step, red beyond.
    for (std::size_t i = 0; i < path.size(); ++i)
        Box(out, box_, CellCenter(def, path[i]) + glm::vec3{0, 0.03F, 0}, {0.35F, 0.04F, 0.35F},
            static_cast<int>(i) < affordable ? Hex(0xF2D58A) : Hex(0xD62828));

    // 4. Pawns where Place puts them; a ring under the player (gold), each group (its number's colour) and the Taken (red).
    constexpr std::array<std::uint32_t, 4> groupColors{0xF2D58A, 0xF2D58A, 0x5FC9C9, 0xB28DFF};
    for (const PawnView &pawn : view.pawns)
    {
        const glm::vec3 at = Place(def, view, pawn.guest, slide);
        // A ghost is a pale, glowing blue-white version of its guest.
        const glm::vec4 color = pawn.ghost ? glm::mix(Hex(def.guests[static_cast<std::size_t>(pawn.guest)].color), Hex(0xBFD8FF), 0.7F)
                                           : Hex(def.guests[static_cast<std::size_t>(pawn.guest)].color);
        const Figure &figure = pawn.taken ? taken_ : guest_;
        const Pawn *p = static_cast<std::size_t>(pawn.guest) < pawns_.size() ? &pawns_[static_cast<std::size_t>(pawn.guest)] : nullptr;
        if (!pawn.taken && !spriteArt_.empty() && p != nullptr)
            Sprite(def, pawn.guest, at, 1.1F, glm::vec3{0.0F}, out);
        else if (figure.mesh && p != nullptr && p->animator && p->taken == pawn.taken)
        {
            // The figure stands on the table: turned by yaw, scaled, feet (y = -1 in the packed model) at y = 0.
            const glm::mat4 model = glm::scale(glm::rotate(glm::translate(glm::mat4{1.0F}, at), p->yaw, {0, 1, 0}), glm::vec3{figure.scale}) *
                                    glm::translate(glm::mat4{1.0F}, {0, 1, 0});
            const std::vector<glm::mat4> &palette = p->animator->Palette();
            // Lightened a quarter toward white and a faint glow, so dark tints still read in the dim room.
            out.items.push_back({figure.mesh, figure.texture, model, glm::mix(color, glm::vec4{1.0F}, 0.25F), 0.0F, pawn.ghost ? 0.6F : std::max(0.12F, readable_)});
            out.items.back().jointOffset = static_cast<std::uint32_t>(out.joints.size());
            out.items.back().jointCount = static_cast<std::uint32_t>(palette.size());
            out.joints.insert(out.joints.end(), palette.begin(), palette.end());
        }
        else
            Box(out, box_, at + glm::vec3{0, 0.45F, 0}, {0.5F, 0.9F, 0.5F}, color);
        if (pawn.guest == acting_) // above the portrait label (Above), so the two never overlap
            Arrow(out, box_, at + glm::vec3{0.0F, 2.2F, 0.0F}, 0.6F, clock_);
        if (pawn.guest == bannerGuest_) // the turn opening: a ring grows and fades
            Outline(out, box_, at + glm::vec3{0.0F, 0.06F, 0.0F}, 0.6F + 1.4F * bannerT_, 0.08F, 1.5F * (1.0F - bannerT_));
        int group = 0;
        for (const auto &[guest, number] : rings)
            group = guest == pawn.guest ? number : group;
        if (pawn.taken)
            Box(out, box_, at + glm::vec3{0, 0.02F, 0}, {0.75F, 0.04F, 0.75F}, Hex(0xD62828));
        else if (pawn.player || group > 0)
            Box(out, box_, at + glm::vec3{0, 0.02F, 0}, {0.75F, 0.04F, 0.75F}, Hex(groupColors[static_cast<std::size_t>(std::clamp(group, 0, 3))]));
    }
    std::map<int, int> inCell;
    const auto spot = [&](int cell) {
        const int i = inCell[cell]++ + Crowd(view, cell);
        const float angle = static_cast<float>(i) * 2.399F;
        return CellCenter(def, cell) + (i == 0 ? glm::vec3{0} : 0.6F * glm::vec3{std::cos(angle), 0, std::sin(angle)});
    };
    // The killer: the Taken's figure, dark red, on a red ring.
    if (view.killer >= 0 && taken_.mesh && killer_.animator)
    {
        const glm::vec3 at = spot(view.killer);
        const glm::mat4 model = glm::scale(glm::translate(glm::mat4{1.0F}, at), glm::vec3{taken_.scale}) * glm::translate(glm::mat4{1.0F}, {0, 1, 0});
        const std::vector<glm::mat4> &palette = killer_.animator->Palette();
        out.items.push_back({taken_.mesh, taken_.texture, model, Hex(0x8A1C1C), 0.0F, 0.3F});
        out.items.back().jointOffset = static_cast<std::uint32_t>(out.joints.size());
        out.items.back().jointCount = static_cast<std::uint32_t>(palette.size());
        out.joints.insert(out.joints.end(), palette.begin(), palette.end());
        Box(out, box_, at + glm::vec3{0, 0.02F, 0}, {0.9F, 0.04F, 0.9F}, Hex(0xD62828));
    }
    else if (view.killer >= 0)
        Box(out, box_, spot(view.killer) + glm::vec3{0, 0.6F, 0}, {0.6F, 1.2F, 0.6F}, Hex(0x8A1C1C));
    // 5. Monsters: a painted card standing in a dark frame, facing the camera (+Z); a box without its picture.
    const auto art = [](const std::vector<umbra::core::TextureHandle> &set, int row) {
        return row >= 0 && static_cast<std::size_t>(row) < set.size() ? set[static_cast<std::size_t>(row)] : umbra::core::TextureHandle{};
    };
    for (const MonsterView &monster : view.monsters)
    {
        const glm::vec3 at = spot(monster.cell);
        const glm::vec4 frame = monster.minion ? Hex(0x5B2A86) : Hex(0x2A1E16);
        if (const umbra::core::TextureHandle card = art(cardArt_, monster.kind))
        {
            Box(out, box_, at + glm::vec3{0, 0.45F, 0}, {0.68F, 0.9F, 0.04F}, frame);
            out.items.push_back({box_, card, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.46F, 0.005F}), {0.6F, 0.8F, 0.04F}), glm::vec4{1.0F}, 0.0F, readable_});
            Box(out, box_, at + glm::vec3{0, 0.02F, 0}, {0.5F, 0.04F, 0.25F}, frame); // its stand
        }
        else
            Box(out, box_, at + glm::vec3{0, 0.35F, 0}, {0.6F, 0.7F, 0.6F}, monster.minion ? Hex(0x5B2A86) : Hex(0x8B5A2B));
    }
    // 6. Items on the floor: small painted tokens along the cell's near edge.
    std::map<int, int> tokens;
    for (const FloorView &item : view.items)
    {
        const int i = tokens[item.cell]++;
        const glm::vec3 at = CellCenter(def, item.cell) + glm::vec3{-tile * 0.3F + static_cast<float>(i) * 0.45F, 0.0F, tile * 0.32F};
        Box(out, box_, at + glm::vec3{0, 0.015F, 0}, {0.42F, 0.03F, 0.42F}, Hex(0x2A1E16));
        if (const umbra::core::TextureHandle token = art(itemArt_, item.item))
            out.items.push_back({box_, token, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.02F, 0}), {0.36F, 0.03F, 0.36F}), glm::vec4{1.0F}, 0.0F, readable_});
        else
            Box(out, box_, at + glm::vec3{0, 0.02F, 0}, {0.3F, 0.03F, 0.3F}, Hex(0xF2D58A));
    }
}

int Table::Crowd(const BoardView &view, int cell)
{
    int n = 0;
    for (const PawnView &pawn : view.pawns)
        n += pawn.cell == cell ? 1 : 0;
    return n;
}

glm::vec3 Table::Place(const NightDef &def, const BoardView &view, int guest, const Slide &slide)
{
    // 1. A sliding pawn: between its two cells, eased (Table.md).
    if (guest == slide.guest && slide.from >= 0 && slide.to >= 0)
    {
        const float t = std::clamp(slide.t, 0.0F, 1.0F), eased = t * t * (3.0F - 2.0F * t);
        return CellCenter(def, slide.from) + (CellCenter(def, slide.to) - CellCenter(def, slide.from)) * eased;
    }
    // 2. Else spread round its cell by the golden angle (no two land together), in the view's order.
    int cell = -1;
    for (const PawnView &pawn : view.pawns)
        cell = pawn.guest == guest ? pawn.cell : cell;
    if (cell < 0)
        return {};
    int before = 0;
    for (const PawnView &pawn : view.pawns)
    {
        if (pawn.guest == guest)
            break;
        before += pawn.cell == cell ? 1 : 0;
    }
    const float angle = static_cast<float>(before) * 2.399F;
    return CellCenter(def, cell) + (before == 0 ? glm::vec3{0} : 0.6F * glm::vec3{std::cos(angle), 0, std::sin(angle)});
}

glm::vec3 Table::Above(const NightDef &def, const BoardView &view, int guest, const Slide &slide) const
{
    // The Taken's figure stands taller.
    bool taken = false;
    for (const PawnView &pawn : view.pawns)
        taken = pawn.guest == guest ? pawn.taken : taken;
    if (view_.RoomT() > 0.0F)
        return Place(def, view, guest, slide) * roomScale + glm::vec3{0.0F, taken ? 2.3F : 1.9F, 0.0F};
    return Place(def, view, guest, slide) + glm::vec3{0.0F, taken ? 1.45F : 1.2F, 0.0F};
}

std::optional<glm::vec2> Table::ScreenPoint(glm::vec3 world, glm::vec2 pixels) const
{
    // 1. World -> clip space with the table's camera; behind the eye: none.
    const umbra::core::Camera camera = Camera();
    const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / std::max(pixels.y, 1.0F), camera.nearPlane, camera.farPlane);
    const glm::vec4 clip = projection * view * glm::vec4(world, 1.0F);
    if (clip.w <= 0.0F)
        return std::nullopt;
    // 2. Divide by w (-1..1), then to pixels; screen y grows downward.
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return glm::vec2{(ndc.x * 0.5F + 0.5F) * pixels.x, (0.5F - ndc.y * 0.5F) * pixels.y};
}

void Table::Update(float dt)
{
    clock_ += dt;
    view_.Update(dt);
}

float Table::Lamp() const
{
    // Two waves beat against each other; only their peaks dim the lamp, so it stutters now and then.
    const float stutter = std::max(0.0F, std::sin(clock_ * 7.3F) * std::sin(clock_ * 2.1F + 1.0F));
    return 1.0F - flicker_ * stutter;
}

glm::vec3 Table::HuntTint() const
{
    // Red after the Turn, breathing slowly; plain before.
    if (!hunt_)
        return glm::vec3{1.0F};
    const float breath = 0.5F + 0.5F * std::sin(clock_ * 1.6F);
    return glm::vec3{1.0F, 0.45F, 0.4F} * (0.85F + 0.25F * breath);
}

void Table::Floor(umbra::core::DrawList &out, umbra::core::TextureHandle art, const glm::mat4 &model, glm::vec4 color) const
{
    // A painted floor with the stain shader: the tint's alpha says how far the blood has spread.
    color.a = blood_;
    out.items.push_back({box_, art, model, color});
    out.items.back().surface = out.SurfaceIndex("dread");
}

int Table::Pick(const NightDef &def, glm::vec2 mouse, glm::vec2 pixels) const
{
    // 1. The mouse in normalized device coordinates (screen y grows downward).
    if (pixels.x <= 0.0F || pixels.y <= 0.0F)
        return -1;
    const glm::vec2 ndc{mouse.x / pixels.x * 2.0F - 1.0F, 1.0F - mouse.y / pixels.y * 2.0F};
    // 2. Undo the projection and view at the near and far planes: the ray between them.
    const umbra::core::Camera camera = Camera();
    const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / pixels.y, camera.nearPlane, camera.farPlane);
    const glm::mat4 inverse = glm::inverse(projection * view);
    const glm::vec4 nearPoint = inverse * glm::vec4{ndc, -1.0F, 1.0F}, farPoint = inverse * glm::vec4{ndc, 1.0F, 1.0F};
    const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
    // In the room view the world is roomScale times larger: scale the ray back to the table.
    const float scale = view_.RoomT() > 0.0F ? roomScale : 1.0F;
    return CellUnderRay(def, origin / scale, glm::vec3(farPoint) / farPoint.w - origin);
}

// --- The stage: one room at full size (Table.md "The stage") ---

namespace
{
    constexpr float stageHalf = 4.0F;   // an 8 m room
    constexpr float doorHalf = 0.9F;    // a 1.8 m doorway
    constexpr float stageScale = 1.55F; // figures from 1.1 m on the table to 1.7 m
}

void Table::DrawRooms(const NightDef &def, const BoardView &view, const std::vector<int> &path, int affordable, const Slide &slide,
                      const std::vector<std::pair<int, int>> &rings, umbra::core::DrawList &out) const
{
    // 1. Light: the table's own, and a lamp over where the camera looks.
    hits_.clear();
    umbra::core::EnvironmentSettings settings;
    settings.indoor = true, settings.ambient = 0.85F, settings.fog = 0.0F, settings.shadows = false;
    settings.tint = HuntTint();
    out.environment = umbra::core::EvaluateEnvironment(settings, 20.0F);
    out.camera = Camera();
    out.lights.push_back({out.camera.target + glm::vec3{0.0F, 7.0F, 0.0F}, {1.0F, 0.82F, 0.6F}, 30.0F * Lamp(), 45.0F, 0.0F});
    Sky(out);
    // 2. Every opened room at full size where its tile lies; out of the light, dim.
    const int cells = def.rules.board.width * def.rules.board.height;
    for (int c = 0; c < cells; ++c)
    {
        const CellView &cell = view.cells[static_cast<std::size_t>(c)];
        if (!cell.opened)
            continue;
        if (std::find(goalCells_.begin(), goalCells_.end(), c) != goalCells_.end()) // the goal room: a glowing rim
            Box(out, box_, CellCenter(def, c) * roomScale - glm::vec3{0.0F, 0.06F, 0.0F}, {8.6F, 0.1F, 8.6F}, Hex(0xD4AF37), 0.6F + 0.4F * std::sin(clock_ * 3.0F));
        StageRoom(cell, CellCenter(def, c) * roomScale, cell.lit ? 1.0F : 0.35F,
                  static_cast<std::size_t>(c) < featured_.size() && featured_[static_cast<std::size_t>(c)], out);
        Plaque(out, cell.room, CellCenter(def, c) * roomScale + glm::vec3{0.0F, 0.01F, 2.7F}, 3.4F, cell.lit ? 1.0F : 0.4F);
        Marker(out, box_, CellCenter(def, c) * roomScale + glm::vec3{2.6F, 2.8F, -2.6F}, 1.0F, cell.mark, clock_);
    }
    // 3. The rooms one step away (a pale plate), and the walk being picked: gold per affordable step, red beyond.
    for (int c = 0; c < cells; ++c)
        if (const CellView &v = view.cells[static_cast<std::size_t>(c)]; v.step >= 0) // smaller over a room not yet opened
            Outline(out, box_, CellCenter(def, c) * roomScale + glm::vec3{0, 0.12F, 0}, v.opened ? 6.4F : 3.0F, 0.18F, 0.5F + 0.3F * std::sin(clock_ * 3.0F));
    for (std::size_t i = 0; i < path.size(); ++i)
        Box(out, box_, CellCenter(def, path[i]) * roomScale + glm::vec3{0, 0.05F, 0}, {1.4F, 0.05F, 1.4F},
            static_cast<int>(i) < affordable ? Hex(0xF2D58A) : Hex(0xD62828));
    // 4. A battle in sight: its sides face each other in its room.
    const glm::vec3 arena = battleCell_ >= 0 ? CellCenter(def, battleCell_) * roomScale : glm::vec3{0.0F};
    std::array<int, 2> count{};
    for (const StageActor &actor : battleActors_)
    {
        const int i = count[static_cast<std::size_t>(std::clamp(actor.side, 0, 1))]++;
        const glm::vec3 at = arena + glm::vec3{actor.side == 0 ? -1.7F : 1.7F, 0.0F, (static_cast<float>(i) - 1.0F) * 1.3F};
        if (actor.guest < 0)
            StageCard(actor.monsterKind, at, out);
        else
            StageFigure(def, actor.guest, actor.taken, at, actor.side == 0 ? -1.5708F : 1.5708F, out), hits_.push_back({at, 0.8F, {ThingKind::Guest, actor.guest}});
    }
    // 5. Guests where their pawns stand (scaled up), facing their way; the player's and groups' rings.
    constexpr std::array<std::uint32_t, 4> groupColors{0xF2D58A, 0xF2D58A, 0x5FC9C9, 0xB28DFF};
    for (const PawnView &pawn : view.pawns)
    {
        if (battleCell_ >= 0 && pawn.cell == battleCell_)
            continue; // drawn with the battle
        const glm::vec3 at = Place(def, view, pawn.guest, slide) * roomScale;
        const Pawn *p = static_cast<std::size_t>(pawn.guest) < pawns_.size() ? &pawns_[static_cast<std::size_t>(pawn.guest)] : nullptr;
        StageFigure(def, pawn.guest, pawn.taken, at, p != nullptr ? p->yaw : 3.14159265F, out);
        hits_.push_back({at, 0.8F, {ThingKind::Guest, pawn.guest}});
        int group = 0;
        for (const auto &[guest, number] : rings)
            group = guest == pawn.guest ? number : group;
        if (pawn.taken)
            Box(out, box_, at + glm::vec3{0, 0.03F, 0}, {1.4F, 0.05F, 1.4F}, Hex(0xD62828));
        else if (pawn.player || group > 0)
            Box(out, box_, at + glm::vec3{0, 0.03F, 0}, {1.4F, 0.05F, 1.4F}, Hex(groupColors[static_cast<std::size_t>(std::clamp(group, 0, 3))]));
    }
    // 6. Monsters as cards toward the back of their room; floor items as painted tokens toward the front.
    std::map<int, int> inCell, tokens;
    for (const MonsterView &monster : view.monsters)
    {
        if (battleCell_ >= 0 && monster.cell == battleCell_)
            continue;
        const glm::vec3 at = CellCenter(def, monster.cell) * roomScale + glm::vec3{-1.5F + 1.6F * static_cast<float>(inCell[monster.cell]++), 0.0F, -2.0F};
        StageCard(monster.kind, at, out);
        hits_.push_back({at, 0.9F, {ThingKind::Monster, monster.monster}});
    }
    for (const FloorView &item : view.items)
    {
        const glm::vec3 at = CellCenter(def, item.cell) * roomScale + glm::vec3{-2.0F + 1.3F * static_cast<float>(tokens[item.cell]++), 0.0F, 2.4F};
        Box(out, box_, at + glm::vec3{0, 0.03F, 0}, {1.1F, 0.06F, 1.1F}, Hex(0x2A1E16));
        const umbra::core::TextureHandle token = item.item >= 0 && static_cast<std::size_t>(item.item) < itemArt_.size() ? itemArt_[static_cast<std::size_t>(item.item)] : umbra::core::TextureHandle{};
        out.items.push_back({box_, token, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.07F, 0}), {0.95F, 0.03F, 0.95F}),
                             token ? glm::vec4{1.0F} : Hex(0xF2D58A), 0.0F, std::max(0.15F, readable_)});
        hits_.push_back({at, 0.7F, {ThingKind::Item, item.floor}});
    }
}

glm::vec3 Table::Ray(glm::vec2 mouse, glm::vec2 pixels, glm::vec3 &origin) const
{
    // The mouse back through the projection and view at the near and far planes (as Pick).
    const glm::vec2 ndc{mouse.x / pixels.x * 2.0F - 1.0F, 1.0F - mouse.y / pixels.y * 2.0F};
    const umbra::core::Camera camera = Camera();
    const glm::mat4 inverse = glm::inverse(glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / pixels.y, camera.nearPlane, camera.farPlane) *
                                           glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F}));
    const glm::vec4 nearPoint = inverse * glm::vec4{ndc, -1.0F, 1.0F}, farPoint = inverse * glm::vec4{ndc, 1.0F, 1.0F};
    origin = glm::vec3(nearPoint) / nearPoint.w;
    return glm::vec3(farPoint) / farPoint.w - origin;
}

std::optional<Table::Thing> Table::PickThing(glm::vec2 mouse, glm::vec2 pixels) const
{
    // 1. In the room view only: the ray at knee height (0.6 m) and at the floor.
    if (view_.RoomT() < 1.0F || pixels.x <= 0.0F || pixels.y <= 0.0F)
        return std::nullopt;
    glm::vec3 origin{0.0F};
    const glm::vec3 direction = Ray(mouse, pixels, origin);
    if (std::abs(direction.y) < 1e-6F)
        return std::nullopt;
    // 2. The nearest thing drawn last frame whose circle holds either point (figures stand, items lie).
    std::optional<Thing> best;
    float bestDistance = 1e9F;
    for (const float height : {0.6F, 0.0F})
    {
        const glm::vec3 point = origin + direction * ((height - origin.y) / direction.y);
        for (const HitBox &hit : hits_)
            if (const float d = glm::length(glm::vec2{point.x - hit.at.x, point.z - hit.at.z}); d < hit.radius && d < bestDistance)
                best = hit.thing, bestDistance = d;
    }
    return best;
}

void Table::StageRoom(const CellView &room, glm::vec3 offset, float shade, bool featured, umbra::core::DrawList &out) const
{
    // 1. The floor: the room's own painting, 8 m across.
    const umbra::core::TextureHandle art = room.room >= 0 && static_cast<std::size_t>(room.room) < roomArt_.size() ? roomArt_[static_cast<std::size_t>(room.room)] : umbra::core::TextureHandle{};
    Floor(out, art, glm::scale(glm::translate(glm::mat4{1.0F}, offset + glm::vec3{0.0F, -0.05F, 0.0F}), {2 * stageHalf, 0.1F, 2 * stageHalf}),
          (art ? glm::vec4{1.0F} : RoomColor(room.kind)) * glm::vec4{shade, shade, shade, 1.0F});
    // 2. Walls: 1 m on every side; a gap where a guest can pass.
    const glm::vec3 side[] = {{0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}};
    for (int d = 0; d < 4; ++d)
    {
        // A gap where a guest can pass (a passage or a doorway); a whole wall elsewhere (the user: no
        // pieces missing where the plan has a door to nowhere).
        const bool across = d == 1 || d == 3, door = (room.walls & (1u << d)) == 0;
        const float height = 1.0F;
        const glm::vec3 edge = offset + side[d] * (stageHalf - 0.1F); // inset: two rooms' walls stand side by side
        const glm::vec4 paint = ((featured ? Hex(0xD4AF37) : RoomColor(room.kind)) * 0.55F + glm::vec4{0.0F, 0.0F, 0.0F, 0.45F}) * glm::vec4{shade, shade, shade, 1.0F};
        // Two pieces either side of the doorway (or one whole wall).
        for (const float sign : {-1.0F, 1.0F})
        {
            const float from = door ? doorHalf : 0.0F, length = stageHalf - from;
            const float mid = sign * (from + length * 0.5F);
            const glm::vec3 at = edge + (across ? glm::vec3{0, 0, mid} : glm::vec3{mid, 0, 0});
            const glm::vec3 size = across ? glm::vec3{0.15F, height, length} : glm::vec3{length, height, 0.15F};
            Box(out, box_, at + glm::vec3{0, height * 0.5F, 0}, size, paint, readable_ * shade);
            Box(out, box_, at + glm::vec3{0, height + 0.04F, 0}, size * glm::vec3{1.0F, 0.0F, 1.0F} + glm::vec3{0.0F, 0.08F, 0.0F} + (across ? glm::vec3{0.06F, 0, 0} : glm::vec3{0, 0, 0.06F}),
                Hex(0xD4AF37) * glm::vec4{shade, shade, shade, 1.0F}, 0.3F + readable_ * shade);
            if (!door)
                break;
        }
    }
}

void Table::StageCard(int kind, glm::vec3 at, umbra::core::DrawList &out) const
{
    // A monster: a framed card on a stand.
    const umbra::core::TextureHandle card = kind >= 0 && static_cast<std::size_t>(kind) < cardArt_.size() ? cardArt_[static_cast<std::size_t>(kind)] : umbra::core::TextureHandle{};
    Box(out, box_, at + glm::vec3{0, 0.85F, 0}, {1.25F, 1.7F, 0.06F}, Hex(0x2A1E16));
    out.items.push_back({box_, card, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.87F, 0.01F}), {1.1F, 1.5F, 0.06F}), card ? glm::vec4{1.0F} : Hex(0x8B5A2B), 0.0F, readable_});
}

void Table::StageFigure(const NightDef &def, int guest, bool taken, glm::vec3 at, float yaw, umbra::core::DrawList &out) const
{
    // A guest at full size, as on the table; the walking figure stands where it walked, facing its way.
    const std::size_t g = static_cast<std::size_t>(guest);
    const Figure &figure = taken ? taken_ : guest_;
    const Pawn *p = g < pawns_.size() ? &pawns_[g] : nullptr;
    const glm::vec4 color = glm::mix(Hex(def.guests[g].color), glm::vec4{1.0F}, 0.25F);
    if (guest == acting_)
        Arrow(out, box_, at + glm::vec3{0.0F, 2.9F * figure.scale * stageScale, 0.0F}, 0.6F, clock_);
    if (guest == bannerGuest_)
        Outline(out, box_, at + glm::vec3{0.0F, 0.1F, 0.0F}, 1.2F + 3.0F * bannerT_, 0.15F, 1.5F * (1.0F - bannerT_));
    if (!taken && !spriteArt_.empty() && p != nullptr)
        return Sprite(def, guest, at, 1.1F * stageScale, glm::vec3{-std::sin(yaw), 0.0F, -std::cos(yaw)}, out); // the model faces -Z
    if (!figure.mesh || p == nullptr || !p->animator)
    {
        Box(out, box_, at + glm::vec3{0, 0.85F, 0}, {0.6F, 1.7F, 0.6F}, color);
        return;
    }
    const glm::mat4 model = glm::scale(glm::rotate(glm::translate(glm::mat4{1.0F}, at), yaw, {0, 1, 0}), glm::vec3{figure.scale * stageScale}) *
                            glm::translate(glm::mat4{1.0F}, {0, 1, 0});
    const std::vector<glm::mat4> &palette = p->animator->Palette();
    out.items.push_back({figure.mesh, figure.texture, model, color, 0.0F, std::max(0.12F, readable_)});
    out.items.back().jointOffset = static_cast<std::uint32_t>(out.joints.size());
    out.items.back().jointCount = static_cast<std::uint32_t>(palette.size());
    out.joints.insert(out.joints.end(), palette.begin(), palette.end());
}

glm::vec3 Table::CameraRight() const
{
    const umbra::core::Camera camera = view_.Camera();
    glm::vec3 forward = camera.target - camera.position;
    forward.y = 0.0F;
    return glm::length(forward) > 1e-4F ? glm::normalize(glm::cross(forward, glm::vec3{0.0F, 1.0F, 0.0F})) : glm::vec3{1.0F, 0.0F, 0.0F};
}

void Table::Sprite(const NightDef &def, int guest, glm::vec3 feet, float height, glm::vec3 toward, umbra::core::DrawList &out) const
{
    const Pawn &p = pawns_[static_cast<std::size_t>(guest)];
    // 1. Which way it faces on screen: its world direction against the camera's right, else as it last faced.
    const glm::vec3 right = CameraRight();
    const float side = glm::dot(toward, right);
    float facing = std::abs(side) > 0.2F ? (side > 0.0F ? 1.0F : -1.0F) : p.sprite.facing;
    const MotionPose pose = p.sprite.Pose(motions_);
    facing = pose.flip ? -facing : facing;
    // 2. Its colour packs the tint and opacity, its emissive the burn and tint amount (sprite.surface.hlsl);
    //    with no tint of its own the template takes a little of the guest's colour, so guests tell apart.
    const glm::vec4 guestColor = Hex(def.guests[static_cast<std::size_t>(guest)].color);
    const bool tinted = pose.tint.a > 0.001F;
    const glm::vec3 tint = tinted ? glm::vec3(pose.tint) : glm::vec3(guestColor);
    const float amount = tinted ? pose.tint.a : 0.18F;
    const auto draw = [&](const MotionPose &q, float alpha) {
        const auto art = spriteArt_.find(q.face);
        const umbra::core::TextureHandle texture = art != spriteArt_.end() ? art->second : spriteArt_.begin()->second;
        out.items.push_back({facing < 0.0F ? mirrorQuad_ : quad_, texture, SpriteModel(q, feet, right, height, spriteAspect_, facing, clock_),
                             glm::vec4(tint, alpha), 0.0F, std::floor(q.dissolve * 99.0F) + std::clamp(amount, 0.0F, 1.0F) * 0.99F});
        out.items.back().surface = out.SurfaceIndex("sprite");
    };
    // 3. A trail: three faded copies a little behind in time.
    if (pose.trail)
        for (int g = 3; g >= 1; --g)
            draw(p.sprite.Pose(motions_, 0.05F * static_cast<float>(g)), pose.alpha * 0.12F * static_cast<float>(4 - g));
    draw(pose, pose.alpha);
}

void Table::Gesture(int guest, const std::string &clip)
{
    if (guest >= 0 && static_cast<std::size_t>(guest) < pawns_.size())
        pawns_[static_cast<std::size_t>(guest)].sprite.Play(clip); // talk, attack, power, hit: motions too
    if (guest >= 0 && static_cast<std::size_t>(guest) < pawns_.size() && pawns_[static_cast<std::size_t>(guest)].animator)
        pawns_[static_cast<std::size_t>(guest)].animator->Trigger(clip);
}

void Table::SetBanner(int guest, float t)
{
    // A new banner: the figure raises a hand (the talk clip); then the ring follows t.
    if (guest >= 0 && guest != bannerGuest_ && static_cast<std::size_t>(guest) < pawns_.size())
    {
        pawns_[static_cast<std::size_t>(guest)].sprite.Play("nod");
        if (pawns_[static_cast<std::size_t>(guest)].animator)
            pawns_[static_cast<std::size_t>(guest)].animator->Trigger("talk");
    }
    bannerGuest_ = guest, bannerT_ = t;
}

void Table::React(const std::vector<LogEntry> &entries)
{
    // 1. The sprites (Motions.md "Which moment plays which motion"): down is held until woken.
    for (const LogEntry &e : entries)
        for (const auto &[guest, motion] : MotionsFor(e))
        {
            if (guest < 0 || static_cast<std::size_t>(guest) >= pawns_.size())
                continue;
            SpriteState &s = pawns_[static_cast<std::size_t>(guest)].sprite;
            if (motion == "down")
                s.Hold("down");
            else
                s.Release(), s.Play(motion);
        }
    // 2. The Taken's figure plays its clips.
    const auto play = [&](int guest, const char *trigger) {
        if (guest >= 0 && static_cast<std::size_t>(guest) < pawns_.size() && pawns_[static_cast<std::size_t>(guest)].animator)
            pawns_[static_cast<std::size_t>(guest)].animator->Trigger(trigger);
    };
    const auto lie = [&](int guest, bool down) {
        if (guest >= 0 && static_cast<std::size_t>(guest) < pawns_.size() && pawns_[static_cast<std::size_t>(guest)].animator)
            pawns_[static_cast<std::size_t>(guest)].animator->SetFlag("dead", down);
    };
    for (const LogEntry &e : entries)
        if (e.kind == LogKind::Hit)
            play(e.who, "attack"), play(e.other, "hit");
        else if (e.kind == LogKind::Power || e.kind == LogKind::Cleared || (e.kind == LogKind::Goal && e.a == 1))
            play(e.who, "attack"); // the graph has no power clip: a strong gesture
        else if (e.kind == LogKind::Item || e.kind == LogKind::Check)
            play(e.who, "talk"); // reaches out: picks it up, braces for the roll
        else if (e.kind == LogKind::Effect && e.b < 0)
            play(e.who, "hit"); // a trap or an event hurts
        else if (e.kind == LogKind::Persuade)
            play(e.who, "talk"), play(e.other, "talk");
        else if (e.kind == LogKind::Down || e.kind == LogKind::Died)
            lie(e.who, true);
        else if (e.kind == LogKind::Woke)
            lie(e.who, false);
}

void Table::Sky(umbra::core::DrawList &out) const
{
    // 1. A painted backdrop far behind the board, square to the camera, filling the view (Table.md "Sky").
    const umbra::core::TextureHandle art = skyArt_[hunt_ ? 1 : 0];
    if (!art)
        return;
    const umbra::core::Camera &camera = out.camera;
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0.0F, 1.0F, 0.0F}));
    const glm::vec3 up = glm::cross(right, forward);
    // 2. Its height covers the view; its width keeps the picture's 2.4:1 (wide screens up to that).
    const float distance = camera.farPlane * 0.8F;
    const float height = 2.0F * distance * std::tan(glm::radians(camera.fovYDegrees) * 0.5F) * 1.02F, width = height * 2.4F;
    glm::mat4 model{1.0F};
    model[0] = glm::vec4{right * width, 0.0F}, model[1] = glm::vec4{up * height, 0.0F}, model[2] = glm::vec4{-forward, 0.0F};
    model[3] = glm::vec4{camera.position + forward * distance, 1.0F};
    // 3. Lit by itself; the hunt's red ambient still tints it a little.
    out.items.push_back({quad_, art, model, glm::vec4{0.85F, 0.85F, 0.85F, 1.0F}, 0.0F, 0.9F});
}

void Table::Plaque(umbra::core::DrawList &out, int room, glm::vec3 at, float width, float shade) const
{
    // A thin plate lying on the floor, its lettering facing up and reading from the south.
    const umbra::core::TextureHandle art = room >= 0 && static_cast<std::size_t>(room) < plaqueArt_.size() ? plaqueArt_[static_cast<std::size_t>(room)] : umbra::core::TextureHandle{};
    if (!art)
        return;
    const glm::mat4 model = glm::scale(glm::rotate(glm::translate(glm::mat4{1.0F}, at), -1.5708F, {1, 0, 0}), glm::vec3{width, width * 0.1875F, 1.0F});
    out.items.push_back({quad_, art, model, glm::vec4{shade, shade, shade, 1.0F}, 0.0F, 0.35F * shade + readable_ * 0.5F});
}
