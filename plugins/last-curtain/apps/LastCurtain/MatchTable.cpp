#include "MatchTable.hpp"

#include <Lighting.hpp>
#include <MeshData.hpp>
#include <TextureTools.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

using namespace umbra::match;

namespace
{
    constexpr float tile = 1.6F; // metres per square (MatchTable.md)
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    glm::vec4 Hex(std::uint32_t rgb, float a = 1.0F)
    {
        return {static_cast<float>((rgb >> 16) & 255) / 255.0F, static_cast<float>((rgb >> 8) & 255) / 255.0F, static_cast<float>(rgb & 255) / 255.0F, a};
    }

    umbra::core::MeshData Cube()
    {
        umbra::core::MeshData mesh;
        for (const glm::vec3 n : {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{0, -1, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}})
        {
            const glm::vec3 u = std::abs(n.y) > 0.5F ? glm::vec3{1, 0, 0} : glm::vec3{0, 1, 0}, v = glm::cross(n, u);
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            for (const glm::vec2 c : {glm::vec2{-1, -1}, glm::vec2{1, -1}, glm::vec2{1, 1}, glm::vec2{-1, 1}})
                mesh.vertices.push_back({0.5F * (n + c.x * u + c.y * v), n, (c + 1.0F) * 0.5F});
            mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        return mesh;
    }

    umbra::core::MeshData Quad(bool mirrored)
    {
        umbra::core::MeshData mesh;
        for (const glm::vec2 c : {glm::vec2{-0.5F, -0.5F}, glm::vec2{0.5F, -0.5F}, glm::vec2{0.5F, 0.5F}, glm::vec2{-0.5F, 0.5F}})
            mesh.vertices.push_back({{c.x, c.y, 0.0F}, {0.0F, 0.0F, 1.0F}, {mirrored ? 0.5F - c.x : c.x + 0.5F, 0.5F - c.y}});
        mesh.indices = {0, 1, 2, 0, 2, 3};
        return mesh;
    }

    void Box(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 centre, glm::vec3 size, glm::vec4 color, float glow = 0.0F)
    {
        out.items.push_back({mesh, {}, glm::scale(glm::translate(glm::mat4{1.0F}, centre), size), color, 0.0F, glow});
    }

    // Four thin bars inside a square's edges.
    void Outline(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 c, float size, glm::vec4 color, float glow)
    {
        const float h = size * 0.5F, bar = 0.08F;
        for (const float s : {-1.0F, 1.0F})
        {
            Box(out, mesh, c + glm::vec3{0, 0.02F, s * (h - bar * 0.5F)}, {size, 0.04F, bar}, color, glow);
            Box(out, mesh, c + glm::vec3{s * (h - bar * 0.5F), 0.02F, 0}, {bar, 0.04F, size}, color, glow);
        }
    }

    std::vector<int> Cells(const nlohmann::json &list)
    {
        std::vector<int> out;
        if (list.is_array())
            for (const auto &c : list)
                out.push_back(c.get<int>());
        return out;
    }
}

MatchTable::MatchTable(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets, const MatchDef &def)
    : def_(def), box_(renderer.CreateMesh(Cube())), quad_(renderer.CreateMesh(Quad(false))), mirrorQuad_(renderer.CreateMesh(Quad(true)))
{
    // 1. Each sprite folder's standing picture, once; a missing one draws a box.
    auto load = [&](const std::string &sprite) {
        umbra::core::TextureData data;
        std::string error;
        if (!sprites_.contains(sprite) && umbra::assets::LoadImageFile(assets / ("art/sprites/" + sprite + "/body_calm.png"), data, error))
            sprites_[sprite] = renderer.CreateTexture(data), aspects_[sprite] = static_cast<float>(data.width) / static_cast<float>(std::max(1u, data.height));
    };
    for (const CharacterDef &c : def.characters)
        load(c.sprite);
    // 2. The shadow: a flat disc (24 sides), drawn twice, a dark core in a softer rim.
    umbra::core::MeshData disc;
    disc.vertices.push_back({{0, 0, 0}, {0, 1, 0}, {0.5F, 0.5F}});
    for (int k = 0; k <= 24; ++k)
    {
        const float a = static_cast<float>(k) / 24.0F * 6.2831853F;
        disc.vertices.push_back({{0.5F * std::cos(a), 0, 0.5F * std::sin(a)}, {0, 1, 0}, {0.5F, 0.5F}});
        if (k > 0)
            disc.indices.insert(disc.indices.end(), {0u, static_cast<std::uint32_t>(k + 1), static_cast<std::uint32_t>(k)});
    }
    disc_ = renderer.CreateMesh(disc);
    for (const MonsterDef &m : def.monsters)
        load(m.sprite);
}

glm::vec3 MatchTable::Centre(const Board &b, int cell) const
{
    // Sideways, as the prototype: the board's rows run left to right, side 0's hub (the bottom row) on the left.
    return {(static_cast<float>(b.height - 1) * 0.5F - static_cast<float>(cell / b.width)) * tile, 0.0F,
            (static_cast<float>(cell % b.width) - static_cast<float>(b.width - 1) * 0.5F) * tile};
}

glm::vec3 MatchTable::Feet(const Board &b, const Unit &u) const
{
    // The middle of a size x size footprint: half a square right and down per extra square.
    // A board step right is +z on the table, a step down is -x.
    return Centre(b, u.cell) + glm::vec3{-1.0F, 0.0F, 1.0F} * (static_cast<float>(u.size - 1) * tile * 0.5F) + glm::vec3{0.0F, 0.12F, 0.0F};
}

float MatchTable::Height(const Unit &u) const
{
    if (u.kind == 0)
        return 3.0F;
    static const float byTier[] = {1.5F, 2.2F, 2.6F, 2.6F}; // weak, strong, great, guardian
    return byTier[static_cast<int>(def_.monsters[U(u.def)].tier)] * static_cast<float>(u.size);
}

void MatchTable::Sprite(const Unit &u, glm::vec3 feet, umbra::core::DrawList &out) const
{
    // 1. Facing the camera square on, upright on screen (a 2D board's figures); side 1's characters face left.
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0})), up = glm::cross(right, forward);
    const std::string &name = u.kind == 0 ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
    const float h = Height(u), w = h * (aspects_.contains(name) ? aspects_.at(name) : 0.3F);
    glm::mat4 model{1.0F};
    model[0] = glm::vec4(right * w, 0.0F);
    model[1] = glm::vec4(up * h, 0.0F);
    model[2] = glm::vec4(-forward, 0.0F);
    model[3] = glm::vec4(feet + up * (h * 0.5F), 1.0F);
    const std::string &sprite = u.kind == 0 ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
    const auto art = sprites_.find(sprite);
    if (art == sprites_.end())
        return Box(out, box_, feet + glm::vec3{0, h * 0.5F, 0}, {w, h, w}, Hex(0x8a8178));
    // 2. The sprite surface: colour = tint and opacity, emissive = how much tint (a monster's colour).
    const std::uint32_t tint = u.kind == 0 ? 0xFFFFFF : def_.monsters[U(u.def)].tint;
    out.items.push_back({u.kind == 0 && u.faction == 1 ? mirrorQuad_ : quad_, art->second, model, Hex(tint), 0.0F, u.kind == 0 ? 0.0F : 0.45F});
    out.items.back().surface = out.SurfaceIndex("sprite");
}

float MatchTable::Look(const char *group, const char *key, float t, float fallback) const
{
    // A value from the look: start and end mixed by t, or one number for both.
    const auto g = look_.find(group);
    if (g == look_.end())
        return fallback;
    if (g->contains(key))
        return (*g)[key].get<float>();
    const float a = g->contains("start") ? (*g)["start"].value(key, fallback) : fallback;
    const float b = g->contains("end") ? (*g)["end"].value(key, a) : a;
    return a + (b - a) * t;
}

void MatchTable::Shadow(glm::vec3 feet, float width, umbra::core::DrawList &out) const
{
    // A wide rim and a smaller dark core, flat on the floor, a little wider than deep.
    const float dark = Look("shadow", "opacity", 0.0F, 0.6F);
    for (const auto [scale, shade, lift] : {std::tuple{1.0F, 0.45F, 0.025F}, std::tuple{0.6F, 0.75F, 0.03F}})
    {
        const float v = 0.2F * (1.0F - dark * shade);
        out.items.push_back({disc_, {}, glm::scale(glm::translate(glm::mat4{1.0F}, {feet.x, lift, feet.z}), {width * scale, 1.0F, width * scale * 0.6F}),
                             glm::vec4(v, v * 0.8F, v * 0.7F, 1.0F)});
    }
}

void MatchTable::Draw(const MatchState &s, const nlohmann::json &ui, int hover, umbra::core::DrawList &out) const
{
    const Board &b = s.board;
    // 1. Light: indoors, one warm lamp over the middle.
    umbra::core::EnvironmentSettings settings;
    settings.indoor = true, settings.ambient = 1.3F, settings.fog = 0.0F, settings.shadows = false;
    out.environment = umbra::core::EvaluateEnvironment(settings, 20.0F);
    // The look: the grade mixed by how far the match has run; the lamp stutters a little more toward the end.
    const float t = std::clamp(static_cast<float>(s.round - 1) / static_cast<float>(std::max(1, def_.options.rounds - 1)), 0.0F, 1.0F);
    for (auto [key, field] : {std::pair{"exposure", &umbra::core::PostSettings::exposure}, {"contrast", &umbra::core::PostSettings::contrast},
                              {"saturation", &umbra::core::PostSettings::saturation}, {"temperature", &umbra::core::PostSettings::temperature},
                              {"vignette", &umbra::core::PostSettings::vignette}, {"bloom", &umbra::core::PostSettings::bloom}})
        out.post.*field = Look("grade", key, t, out.post.*field);
    const float stutter = std::max(0.0F, std::sin(clock_ * 7.3F) * std::sin(clock_ * 2.1F + 1.0F));
    const float lamp = 1.0F - Look("flicker", "x", t, 0.0F) * stutter;
    out.lights.push_back({{0.0F, 9.0F, 4.0F}, {1.0F, 0.88F, 0.72F}, 140.0F * lamp, 70.0F, 0.0F});
    out.camera = view_.Camera();

    // 2. Squares by kind; the stage glows gold; walls stay dark under the table.
    Box(out, box_, {0.0F, -0.2F, 0.0F}, {static_cast<float>(b.height) * tile + 0.6F, 0.2F, static_cast<float>(b.width) * tile + 0.6F}, Hex(0x1a1411));
    for (int c = 0; c < static_cast<int>(b.cells.size()); ++c)
    {
        const CellKind kind = b.cells[U(c)].kind;
        if (kind == CellKind::Wall)
            continue;
        glm::vec4 color = kind == CellKind::Hall ? Hex(0x3a2b21) : kind == CellKind::Room ? Hex(0x47393a) : kind == CellKind::Centre ? Hex(0x5c2226)
                        : c == b.hub[0] ? Hex(0x24405e) : Hex(0x5e3a1c);
        float glow = 0.0F;
        if (c == s.stage)
            color = Hex(0x8a6a24), glow = 0.35F + 0.1F * std::sin(clock_ * 2.0F);
        Box(out, box_, Centre(b, c) + glm::vec3{0, -0.05F, 0}, {tile * 0.96F, 0.1F, tile * 0.96F}, color, glow);
        if (b.cells[U(c)].event >= 0 && !b.cells[U(c)].resolved) // an event room: a small violet mark in its corner (optional goals stay quiet)
            Box(out, box_, Centre(b, c) + glm::vec3{-tile * 0.3F, 0.02F, -tile * 0.3F}, {0.22F, 0.03F, 0.22F}, Hex(0x8a6cc8), 0.25F);
    }
    // 2b. An item on the floor: a small gold mark in the opposite corner.
    for (const Floor &f : s.floor)
        if (f.cell >= 0)
            Box(out, box_, Centre(b, f.cell) + glm::vec3{tile * 0.3F, 0.02F, tile * 0.3F}, {0.22F, 0.03F, 0.22F}, Hex(0xC9A54A), 0.25F);
    // 3. Announced spawns: an orange marker pulsing on the square.
    for (const Spawn &w : s.spawns)
        if (w.round - def_.rules.spawn.warn <= s.round)
            Outline(out, box_, Centre(b, w.cell), tile * 0.9F, Hex(0xE08A3C), 0.5F + 0.4F * std::sin(clock_ * 4.0F));
    // 4. The player's turn: the reach (blue, or orange as an armed ability's range), the path, the targets (red), the hovered square.
    const bool armed = ui.contains("armed");
    for (int c : Cells(ui.value("reach", nlohmann::json::array())))
        Box(out, box_, Centre(b, c) + glm::vec3{0, 0.01F, 0}, {tile * 0.9F, 0.02F, tile * 0.9F}, armed ? Hex(0xC07030) : Hex(0x4a7fb5), 0.25F);
    for (int c : Cells(ui.value("path", nlohmann::json::array())))
        Box(out, box_, Centre(b, c) + glm::vec3{0, 0.08F, 0}, {0.2F, 0.06F, 0.2F}, Hex(0xEFE6D6), 0.8F);
    for (int c : Cells(ui.value("targets", nlohmann::json::array())))
        Outline(out, box_, Centre(b, c), tile * 0.92F, Hex(0xE8645F), 0.7F);
    if (hover >= 0 && hover < static_cast<int>(b.cells.size()) && b.cells[U(hover)].kind != CellKind::Wall)
        Outline(out, box_, Centre(b, hover), tile, Hex(0xEFE6D6), 0.6F);
    // 5. Figures, a ring in the side's colour under each character, the acting one's arrow drawn as a card (Labels).
    for (const Unit &u : s.units)
    {
        if (u.cell < 0 || u.hp <= 0)
            continue;
        const glm::vec3 feet = Feet(b, u);
        Shadow(feet + glm::vec3{0.0F, 0.0F, 0.15F}, tile * Look("shadow", "width", 0.0F, 0.75F) * static_cast<float>(u.size), out);
        if (u.faction >= 0)
            Outline(out, box_, feet - glm::vec3{0, 0.1F, 0}, tile * 0.7F * static_cast<float>(u.size), u.faction == 0 ? Hex(0x5b9bd5) : Hex(0xe08a3c), 0.5F);
        Sprite(u, feet, out);
    }
}

std::optional<glm::vec2> MatchTable::ScreenPoint(glm::vec3 world, glm::vec2 pixels) const
{
    const umbra::core::Camera camera = view_.Camera();
    const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / std::max(pixels.y, 1.0F), camera.nearPlane, camera.farPlane);
    const glm::vec4 clip = projection * view * glm::vec4(world, 1.0F);
    if (clip.w <= 0.0F)
        return std::nullopt;
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return glm::vec2{(ndc.x * 0.5F + 0.5F) * pixels.x, (0.5F - ndc.y * 0.5F) * pixels.y};
}

nlohmann::json MatchTable::Labels(const MatchState &s, glm::vec2 pixels) const
{
    // 1. Above each figure: the acting one's arrow, then its card; carried essence beside it.
    nlohmann::json labels = nlohmann::json::array();
    const int acting = Current(s);
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
    {
        const Unit &u = s.units[U(i)];
        if (u.cell < 0 || u.hp <= 0)
            continue;
        const auto at = ScreenPoint(Feet(s.board, u) + glm::vec3{0.0F, Height(u) + 0.15F, 0.0F}, pixels);
        if (!at)
            continue;
        const bool character = u.kind == 0;
        const std::string sprite = character ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
        const std::string &name = character ? def_.characters[U(u.def)].name : def_.monsters[U(u.def)].name;
        labels.push_back({{"at", {at->x, at->y}}, {"name", name.substr(name.rfind(' ') + 1)}, {"level", character ? u.level : def_.monsters[U(u.def)].level},
                          {"hp", u.hp}, {"maxHp", u.maxHp}, {"portrait", "art/sprites/" + sprite + "/face_calm.png"}, {"acting", i == acting},
                          {"essence", character && u.essence > 0 ? std::to_string(u.essence) : ""}, {"side", u.faction}});
    }
    return labels;
}

int MatchTable::Pick(const Board &b, glm::vec2 mouse, glm::vec2 pixels) const
{
    // 1. The mouse's ray through the camera, where it meets the table top (y = 0), then the square there.
    if (pixels.x <= 0.0F || pixels.y <= 0.0F)
        return -1;
    const glm::vec2 ndc{mouse.x / pixels.x * 2.0F - 1.0F, 1.0F - mouse.y / pixels.y * 2.0F};
    const umbra::core::Camera camera = view_.Camera();
    const glm::mat4 inverse = glm::inverse(glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / pixels.y, camera.nearPlane, camera.farPlane) *
                                           glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F}));
    const glm::vec4 n = inverse * glm::vec4{ndc, -1.0F, 1.0F}, f = inverse * glm::vec4{ndc, 1.0F, 1.0F};
    const glm::vec3 origin = glm::vec3(n) / n.w, direction = glm::vec3(f) / f.w - origin;
    if (std::abs(direction.y) < 1e-6F)
        return -1;
    const glm::vec3 p = origin + direction * (-origin.y / direction.y);
    const int x = static_cast<int>(std::floor(p.z / tile + static_cast<float>(b.width) * 0.5F));
    const int y = static_cast<int>(std::floor(-p.x / tile + static_cast<float>(b.height) * 0.5F));
    return x < 0 || y < 0 || x >= b.width || y >= b.height ? -1 : y * b.width + x;
}
