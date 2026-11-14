#include "Table.hpp"

#include <Lighting.hpp>
#include <MeshData.hpp>

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
    constexpr float tile = 2.2F, tileThick = 0.15F, tilt = 50.0F;

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
        case RoomKind::Omen: return Hex(0x1F1F24);
        case RoomKind::Transport: return Hex(0x1F8A8A);
        case RoomKind::Bonus: return Hex(0x2F8F5B);
        }
        return Hex(0x6B7280);
    }

    constexpr std::array<std::uint32_t, 7> guestColors{0xE07A5F, 0xF2CC8F, 0x81B29A, 0x5DA9E9, 0xC27BA0, 0xF4A261, 0xEDEDE9};

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

    void Box(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 centre, glm::vec3 size, glm::vec4 color)
    {
        out.items.push_back({mesh, {}, glm::scale(glm::translate(glm::mat4{1.0F}, centre), size), color});
    }
}

Table::Table(umbra::render::Renderer &renderer) : renderer_(renderer), box_(renderer.CreateMesh(Cube())) {}

Table::~Table()
{
    if (box_)
        renderer_.DestroyMesh(box_);
}

void Table::Draw(const NightDef &def, const BoardView &view, umbra::core::DrawList &out) const
{
    // 1. Light: indoors, dim ambient (red in the hunt), one warm lamp over the middle of the table.
    umbra::core::EnvironmentSettings settings;
    settings.indoor = true;
    settings.ambient = 0.8F;
    settings.fog = 0.0F;
    settings.shadows = false;
    settings.tint = hunt_ ? glm::vec3{1.0F, 0.45F, 0.4F} : glm::vec3{1.0F};
    out.environment = umbra::core::EvaluateEnvironment(settings, 20.0F);
    out.lights.push_back({{0.0F, 7.0F, 0.0F}, {1.0F, 0.82F, 0.6F}, 30.0F, 40.0F, 0.0F});
    out.camera = Camera();

    // 2. Rooms: a floor tile, wall stubs where a side has no door, a faint bar at each doorway.
    const int cells = def.rules.board.width * def.rules.board.height;
    const glm::vec3 side[] = {{0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}}; // N E S W
    for (int c = 0; c < cells; ++c)
    {
        const CellView &cell = view.cells[static_cast<std::size_t>(c)];
        if (!cell.opened)
            continue;
        const glm::vec3 at = CellCenter(def, c);
        Box(out, box_, at - glm::vec3{0, tileThick * 0.5F, 0}, {tile, tileThick, tile}, cell.lit ? RoomColor(cell.kind) : Hex(0x2A2A33));
        if (cell.step >= 0)
            Box(out, box_, at + glm::vec3{0, 0.01F, 0}, {tile * 0.6F, 0.02F, tile * 0.6F}, Hex(0xF2D58A));
        for (int d = 0; d < 4; ++d)
        {
            const bool across = d == 1 || d == 3;
            const glm::vec3 edge = at + side[d] * (tile * 0.5F);
            if (!(cell.doors & (1u << d)))
                Box(out, box_, edge + glm::vec3{0, 0.17F, 0}, across ? glm::vec3{0.12F, 0.35F, tile} : glm::vec3{tile, 0.35F, 0.12F}, Hex(0x3A3A44));
            else if (cell.doorways & (1u << d))
                Box(out, box_, edge + side[d] * 0.2F, across ? glm::vec3{0.3F, 0.03F, 0.6F} : glm::vec3{0.6F, 0.03F, 0.3F}, Hex(0xC9C1AD));
        }
    }

    // 3. Pawns, spread around their cell; a ring under the player and allies, a red one under the Taken.
    std::map<int, int> inCell;
    const auto spot = [&](int cell) {
        const int i = inCell[cell]++;
        const float angle = static_cast<float>(i) * 2.399F; // golden angle: no two land together
        return CellCenter(def, cell) + (i == 0 ? glm::vec3{0} : 0.6F * glm::vec3{std::cos(angle), 0, std::sin(angle)});
    };
    for (const PawnView &pawn : view.pawns)
    {
        const glm::vec3 at = spot(pawn.cell);
        Box(out, box_, at + glm::vec3{0, 0.45F, 0}, {0.5F, 0.9F, 0.5F}, Hex(guestColors[static_cast<std::size_t>(pawn.guest) % guestColors.size()]));
        if (pawn.player || pawn.ally || pawn.taken)
            Box(out, box_, at + glm::vec3{0, 0.02F, 0}, {0.75F, 0.04F, 0.75F}, pawn.taken ? Hex(0xD62828) : pawn.player ? Hex(0xF2D58A) : Hex(0x8FD48A));
    }
    for (const MonsterView &monster : view.monsters)
        Box(out, box_, spot(monster.cell) + glm::vec3{0, 0.35F, 0}, {0.6F, 0.7F, 0.6F}, monster.minion ? Hex(0x5B2A86) : Hex(0x8B5A2B));
}

void Table::Aim(glm::vec3 target, float seconds)
{
    from_ = Camera().target;
    to_ = target;
    seconds_ = seconds;
    elapsed_ = 0.0F;
}

void Table::Update(float dt) { elapsed_ = std::min(elapsed_ + dt, seconds_); }

void Table::Zoom(float steps) { distance_ = std::clamp(distance_ - steps, 5.0F, 22.0F); }

umbra::core::Camera Table::Camera() const
{
    // Ease in and out (smoothstep) from the old target to the new; the camera sits up and back at `tilt`.
    const float t = seconds_ > 0.0F ? elapsed_ / seconds_ : 1.0F;
    umbra::core::Camera camera;
    camera.target = from_ + (to_ - from_) * (t * t * (3.0F - 2.0F * t));
    camera.position = camera.target + distance_ * glm::vec3{0.0F, std::sin(glm::radians(tilt)), std::cos(glm::radians(tilt))};
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
