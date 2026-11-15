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
    void Arrow(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 top, float size, float clock)
    {
        const glm::vec3 at = top + glm::vec3{0.0F, size * (0.9F + 0.25F * std::sin(clock * 3.0F)), 0.0F};
        const glm::mat4 model = glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4{1.0F}, at), clock * 1.5F, {0, 1, 0}), 0.7854F, {0, 0, 1}),
                                           glm::vec3{size * 0.5F, size * 0.5F, size * 0.2F});
        out.items.push_back({mesh, {}, model, glm::vec4{0.95F, 0.8F, 0.3F, 1.0F}, 0.0F, 1.0F});
    }

    void Box(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 centre, glm::vec3 size, glm::vec4 color)
    {
        out.items.push_back({mesh, {}, glm::scale(glm::translate(glm::mat4{1.0F}, centre), size), color});
    }
}

Table::Table(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets)
    : renderer_(renderer), box_(renderer.CreateMesh(Cube())), guest_(Load(assets / "models/guest.glb", assets / "animations/guest.anim.json", 0.55F)),
      taken_(Load(assets / "models/taken.glb", assets / "animations/taken.anim.json", 0.65F))
{
}

Table::~Table()
{
    for (const auto *art : {&roomArt_, &cardArt_, &itemArt_})
        for (const umbra::core::TextureHandle texture : *art)
            if (texture)
                renderer_.DestroyTexture(texture);
    for (const Figure *figure : {&guest_, &taken_})
    {
        if (figure->mesh)
            renderer_.DestroyMesh(figure->mesh);
        if (figure->texture)
            renderer_.DestroyTexture(figure->texture);
    }
    if (box_)
        renderer_.DestroyMesh(box_);
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
        roomArt_.push_back(load("rooms", room.id));
    for (const MonsterDef &monster : def.monsters)
        cardArt_.push_back(load("cards", monster.id));
    for (const ItemDef &item : def.items)
        itemArt_.push_back(load("items", item.id));
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
        bool walking = pawn.guest == slide.guest && slide.from >= 0 && slide.to >= 0 && slide.from != slide.to;
        if (walking)
        {
            const glm::vec3 step = CellCenter(def, slide.to) - CellCenter(def, slide.from);
            p.yaw = std::atan2(-step.x, -step.z);
        }
        // 3. The stage walker heads for its target at 1.6 m/s.
        if (pawn.guest == stageGuest_)
        {
            const glm::vec3 left = walkTarget_ - walkAt_;
            const float distance = glm::length(left);
            if (distance > 0.05F)
            {
                walkAt_ += left * std::min(1.0F, 1.6F * dt / distance);
                p.yaw = std::atan2(-left.x, -left.z), walking = true;
            }
        }
        p.animator->SetNumber("speed", walking ? 1.0F : 0.0F);
        p.animator->Update(dt);
    }
}

void Table::Draw(const NightDef &def, const BoardView &view, const std::vector<int> &path, int affordable, const Slide &slide,
                 const std::vector<std::pair<int, int>> &rings, umbra::core::DrawList &out) const
{
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
            Box(out, box_, at - glm::vec3{0, tileThick * 0.5F, 0}, {tile, tileThick, tile}, (featured ? Hex(0xD4AF37) : RoomColor(cell.kind)) * shade);
            const float inner = featured ? 0.84F : 0.9F;
            Floor(out, art, glm::scale(glm::translate(glm::mat4{1.0F}, at - glm::vec3{0, tileThick * 0.5F - 0.004F, 0}), {tile * inner, tileThick, tile * inner}), shade);
        }
        if (cell.step >= 0)
            Box(out, box_, at + glm::vec3{0, 0.01F, 0}, {tile * 0.6F, 0.02F, tile * 0.6F}, Hex(0xF2D58A));
        for (int d = 0; d < 4; ++d)
        {
            const bool across = d == 1 || d == 3;
            const glm::vec3 edge = at + side[d] * (tile * 0.5F);
            if (cell.walls & (1u << d))
                Box(out, box_, edge + glm::vec3{0, 0.17F, 0}, across ? glm::vec3{0.12F, 0.35F, tile} : glm::vec3{tile, 0.35F, 0.12F}, Hex(0x3A3A44));
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
        const glm::vec4 color = Hex(def.guests[static_cast<std::size_t>(pawn.guest)].color);
        const Figure &figure = pawn.taken ? taken_ : guest_;
        const Pawn *p = static_cast<std::size_t>(pawn.guest) < pawns_.size() ? &pawns_[static_cast<std::size_t>(pawn.guest)] : nullptr;
        if (figure.mesh && p != nullptr && p->animator && p->taken == pawn.taken)
        {
            // The figure stands on the table: turned by yaw, scaled, feet (y = -1 in the packed model) at y = 0.
            const glm::mat4 model = glm::scale(glm::rotate(glm::translate(glm::mat4{1.0F}, at), p->yaw, {0, 1, 0}), glm::vec3{figure.scale}) *
                                    glm::translate(glm::mat4{1.0F}, {0, 1, 0});
            const std::vector<glm::mat4> &palette = p->animator->Palette();
            // Lightened a quarter toward white and a faint glow, so dark tints still read in the dim room.
            out.items.push_back({figure.mesh, figure.texture, model, glm::mix(color, glm::vec4{1.0F}, 0.25F), 0.0F, 0.12F});
            out.items.back().jointOffset = static_cast<std::uint32_t>(out.joints.size());
            out.items.back().jointCount = static_cast<std::uint32_t>(palette.size());
            out.joints.insert(out.joints.end(), palette.begin(), palette.end());
        }
        else
            Box(out, box_, at + glm::vec3{0, 0.45F, 0}, {0.5F, 0.9F, 0.5F}, color);
        if (pawn.guest == acting_)
            Arrow(out, box_, at + glm::vec3{0.0F, 1.2F, 0.0F}, 0.6F, clock_);
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
            out.items.push_back({box_, card, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.46F, 0.005F}), {0.6F, 0.8F, 0.04F}), glm::vec4{1.0F}});
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
            out.items.push_back({box_, token, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.02F, 0}), {0.36F, 0.03F, 0.36F}), glm::vec4{1.0F}});
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

void Table::Aim(glm::vec3 target, float seconds)
{
    from_ = Camera().target;
    to_ = target;
    seconds_ = seconds;
    elapsed_ = 0.0F;
}

void Table::Update(float dt) { elapsed_ = std::min(elapsed_ + dt, seconds_), clock_ += dt; }

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

void Table::Pan(glm::vec2 pixels)
{
    // A pixel covers more of the table the farther the camera is: about distance / 700 metres.
    const glm::vec3 target = Camera().target - glm::vec3{pixels.x, 0.0F, pixels.y} * (distance_ / 700.0F);
    from_ = to_ = target;
    elapsed_ = seconds_;
}

void Table::Zoom(float steps) { distance_ = std::clamp(distance_ - steps, minDistance_, maxDistance_); }

umbra::core::Camera Table::Camera() const
{
    // Ease in and out (smoothstep) from the old target to the new; the camera sits up and back at `tilt`.
    const float t = seconds_ > 0.0F ? elapsed_ / seconds_ : 1.0F;
    umbra::core::Camera camera;
    camera.target = from_ + (to_ - from_) * (t * t * (3.0F - 2.0F * t));
    camera.position = camera.target + distance_ * glm::vec3{0.0F, std::sin(glm::radians(tilt_)), std::cos(glm::radians(tilt_))};
    return camera;
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
    return CellUnderRay(def, origin, glm::vec3(farPoint) / farPoint.w - origin);
}

// --- The stage: one room at full size (Table.md "The stage") ---

namespace
{
    constexpr float stageHalf = 4.0F;   // an 8 m room
    constexpr float doorHalf = 0.9F;    // a 1.8 m doorway
    constexpr float stageScale = 1.55F; // figures from 1.1 m on the table to 1.7 m
}

umbra::core::Camera Table::StageCamera() const
{
    // A seat high in the front corner (user): the back walls (N, W) face it.
    umbra::core::Camera camera;
    camera.target = {0.0F, 0.7F, -0.5F};
    camera.position = {5.2F, 5.8F, 7.0F};
    return camera;
}

void Table::StageStart(int guest)
{
    stageGuest_ = guest;
    walkAt_ = walkTarget_ = {0.0F, 0.0F, 1.5F};
}

void Table::DrawStage(const NightDef &def, const BoardView &view, int cell, const std::vector<StageActor> &actors, umbra::core::DrawList &out) const
{
    const CellView &room = view.cells[static_cast<std::size_t>(cell)];
    // 1. Light: the table's own, and a lamp over the room.
    umbra::core::EnvironmentSettings settings;
    settings.indoor = true, settings.ambient = 0.85F, settings.fog = 0.0F, settings.shadows = false;
    settings.tint = HuntTint();
    out.environment = umbra::core::EvaluateEnvironment(settings, 20.0F);
    out.lights.push_back({{0.0F, 6.0F, 0.0F}, {1.0F, 0.82F, 0.6F}, 25.0F * Lamp(), 30.0F, 0.0F});
    out.camera = StageCamera();
    // 2. The floor: the room's own painting, 8 m across.
    const umbra::core::TextureHandle art = room.room >= 0 && static_cast<std::size_t>(room.room) < roomArt_.size() ? roomArt_[static_cast<std::size_t>(room.room)] : umbra::core::TextureHandle{};
    Floor(out, art, glm::scale(glm::translate(glm::mat4{1.0F}, {0.0F, -0.05F, 0.0F}), {2 * stageHalf, 0.1F, 2 * stageHalf}),
          art ? glm::vec4{1.0F} : RoomColor(room.kind));
    // 3. Walls: tall painted flats at the back (N, W), low ones at the front (E, S); a gap at each door.
    const glm::vec3 side[] = {{0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}};
    for (int d = 0; d < 4; ++d)
    {
        const bool back = d == 0 || d == 3, across = d == 1 || d == 3, door = (room.doors & (1u << d)) != 0;
        const float height = back ? 3.0F : 0.4F;
        const glm::vec3 edge = side[d] * stageHalf;
        const glm::vec4 paint = RoomColor(room.kind) * 0.55F + glm::vec4{0.0F, 0.0F, 0.0F, 0.45F};
        // Two pieces either side of the doorway (or one whole wall).
        for (const float sign : {-1.0F, 1.0F})
        {
            const float from = door ? doorHalf : 0.0F, length = stageHalf - from;
            const float mid = sign * (from + length * 0.5F);
            const glm::vec3 at = edge + (across ? glm::vec3{0, 0, mid} : glm::vec3{mid, 0, 0});
            const glm::vec3 size = across ? glm::vec3{0.15F, height, length} : glm::vec3{length, height, 0.15F};
            Box(out, box_, at + glm::vec3{0, height * 0.5F, 0}, size, paint);
            Box(out, box_, at + glm::vec3{0, height + 0.04F, 0}, size * glm::vec3{1.0F, 0.0F, 1.0F} + glm::vec3{0.0F, 0.08F, 0.0F} + (across ? glm::vec3{0.06F, 0, 0} : glm::vec3{0, 0, 0.06F}), Hex(0xD4AF37));
            if (!door)
                break;
        }
    }
    // 4. The actors: a battle's sides face each other; in a Look they stand about the room.
    const bool battle = std::any_of(actors.begin(), actors.end(), [](const StageActor &a) { return a.side == 1; }) &&
                        std::any_of(actors.begin(), actors.end(), [](const StageActor &a) { return a.guest >= 0 && a.side == 0; }) && stageGuest_ < 0;
    std::array<int, 2> count{};
    for (const StageActor &actor : actors)
    {
        const int i = count[static_cast<std::size_t>(std::clamp(actor.side, 0, 1))]++;
        const float lane = (static_cast<float>(i) - 1.0F) * 1.3F;
        glm::vec3 at = battle ? glm::vec3{actor.side == 0 ? -1.7F : 1.7F, 0.0F, lane} : glm::vec3{-1.8F + static_cast<float>(i) * 1.4F, 0.0F, actor.side == 0 ? 0.4F : -1.8F};
        float yaw = battle ? (actor.side == 0 ? -1.5708F : 1.5708F) : 3.14159265F;
        // 4a. A monster: a framed card on a stand.
        if (actor.guest < 0)
        {
            const umbra::core::TextureHandle card = actor.monsterKind >= 0 && static_cast<std::size_t>(actor.monsterKind) < cardArt_.size() ? cardArt_[static_cast<std::size_t>(actor.monsterKind)] : umbra::core::TextureHandle{};
            Box(out, box_, at + glm::vec3{0, 0.85F, 0}, {1.25F, 1.7F, 0.06F}, Hex(0x2A1E16));
            out.items.push_back({box_, card, glm::scale(glm::translate(glm::mat4{1.0F}, at + glm::vec3{0, 0.87F, 0.01F}), {1.1F, 1.5F, 0.06F}), card ? glm::vec4{1.0F} : Hex(0x8B5A2B)});
            continue;
        }
        // 4b. A guest: its figure at full size, as on the table.
        const std::size_t g = static_cast<std::size_t>(actor.guest);
        if (actor.guest == stageGuest_)
            at = walkAt_;
        const Figure &figure = actor.taken ? taken_ : guest_;
        const Pawn *p = g < pawns_.size() ? &pawns_[g] : nullptr;
        const glm::vec4 color = glm::mix(Hex(def.guests[g].color), glm::vec4{1.0F}, 0.25F);
        if (!figure.mesh || p == nullptr || !p->animator)
        {
            Box(out, box_, at + glm::vec3{0, 0.85F, 0}, {0.6F, 1.7F, 0.6F}, color);
            continue;
        }
        if (actor.guest == stageGuest_ || !battle)
            yaw = actor.guest == stageGuest_ ? p->yaw : yaw;
        const glm::mat4 model = glm::scale(glm::rotate(glm::translate(glm::mat4{1.0F}, at), yaw, {0, 1, 0}), glm::vec3{figure.scale * stageScale}) *
                                glm::translate(glm::mat4{1.0F}, {0, 1, 0});
        if (actor.guest == acting_)
            Arrow(out, box_, at + glm::vec3{0.0F, 2.0F * figure.scale * stageScale, 0.0F}, 0.6F, clock_);
        const std::vector<glm::mat4> &palette = p->animator->Palette();
        out.items.push_back({figure.mesh, figure.texture, model, color, 0.0F, 0.12F});
        out.items.back().jointOffset = static_cast<std::uint32_t>(out.joints.size());
        out.items.back().jointCount = static_cast<std::uint32_t>(palette.size());
        out.joints.insert(out.joints.end(), palette.begin(), palette.end());
    }
}

bool Table::StagePick(const BoardView &view, int cell, glm::vec2 mouse, glm::vec2 pixels, glm::vec3 &floor, int &door) const
{
    // 1. The mouse ray (as Pick) meets the floor plane y = 0.
    if (pixels.x <= 0.0F || pixels.y <= 0.0F)
        return false;
    const glm::vec2 ndc{mouse.x / pixels.x * 2.0F - 1.0F, 1.0F - mouse.y / pixels.y * 2.0F};
    const umbra::core::Camera camera = StageCamera();
    const glm::mat4 inverse = glm::inverse(glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / pixels.y, camera.nearPlane, camera.farPlane) *
                                           glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F}));
    const glm::vec4 nearPoint = inverse * glm::vec4{ndc, -1.0F, 1.0F}, farPoint = inverse * glm::vec4{ndc, 1.0F, 1.0F};
    const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w, direction = glm::vec3(farPoint) / farPoint.w - origin;
    if (std::abs(direction.y) < 1e-6F || origin.y / -direction.y < 0.0F)
        return false;
    floor = origin + direction * (origin.y / -direction.y);
    if (std::abs(floor.x) > stageHalf + 0.5F || std::abs(floor.z) > stageHalf + 0.5F)
        return false;
    // 2. Near a wall, inside a doorway's gap: that door.
    door = -1;
    const std::uint8_t doors = view.cells[static_cast<std::size_t>(cell)].doors;
    const float edge = stageHalf - 0.6F;
    const int side = floor.z < -edge ? 0 : floor.x > edge ? 1 : floor.z > edge ? 2 : floor.x < -edge ? 3 : -1;
    if (side >= 0 && (doors & (1u << side)) && std::abs(side % 2 == 0 ? floor.x : floor.z) < doorHalf)
        door = side;
    floor = glm::clamp(floor, glm::vec3{-stageHalf + 0.4F, 0.0F, -stageHalf + 0.4F}, glm::vec3{stageHalf - 0.4F, 0.0F, stageHalf - 0.4F});
    return true;
}

void Table::React(const std::vector<LogEntry> &entries)
{
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
