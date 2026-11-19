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
    : def_(def), renderer_(renderer), assets_(assets), box_(renderer.CreateMesh(Cube())), quad_(renderer.CreateMesh(Quad(false))), mirrorQuad_(renderer.CreateMesh(Quad(true)))
{
    // 1. Each sprite folder's pictures, one per face the motions use; a missing calm one draws a box.
    std::string motionError;
    motions_ = umbra::night::Motions::Load(assets / "motions.json", motionError);
    auto load = [&](const std::string &sprite) {
        for (const char *face : {"calm", "afraid", "angry", "happy", "neardead", "scheming", "talk"})
        {
            umbra::core::TextureData data;
            std::string error;
            const std::string key = sprite + "/" + face;
            if (!sprites_.contains(key) && umbra::assets::LoadImageFile(assets / ("art/sprites/" + key.substr(0, key.find('/')) + "/body_" + face + ".png"), data, error))
                sprites_[key] = renderer.CreateTexture(data), aspects_[sprite] = static_cast<float>(data.width) / static_cast<float>(std::max(1u, data.height));
        }
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
    // 3. The marks of optional rooms: white glyphs, tinted when drawn.
    for (auto [file, handle] : {std::pair{"event", &eventIcon_}, {"item", &chestIcon_}})
    {
        umbra::core::TextureData data;
        std::string error;
        if (umbra::assets::LoadImageFile(assets / (std::string("art/ui/match/icons/") + file + ".png"), data, error))
            *handle = renderer.CreateTexture(data);
    }
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

void MatchTable::Play(int unit, const std::string &motion)
{
    if (unit < 0)
        return;
    if (static_cast<std::size_t>(unit) >= states_.size())
        states_.resize(U(unit) + 1);
    umbra::night::SpriteState &st = states_[U(unit)];
    if (motion == "down")
        st.Hold("down");
    else if (motion == "revive")
        st.Release(), st.Play("revive");
    else if (motion == "walk")
        st.Play("walk");
    else
        st.Play(motion);
}

void MatchTable::Animate(const MatchState &s, float dt)
{
    // 1. A new turn: the ring grows under the one whose turn it is, it nods, and the camera eases halfway to it.
    const int now = s.battle.on ? s.battle.attacker : Current(s);
    if (now >= 0 && now != acting_)
    {
        acting_ = now, follow_ = true;
        if (s.units[U(now)].kind == 0 && !s.battle.on)
            ringUnit_ = now, ringT_ = 0.0F, Play(now, "nod");
    }
    ringT_ = std::min(1.0F, ringT_ + dt / Look("ring", "seconds", 0.0F, 1.0F));
    if (follow_ && now >= 0 && s.units[U(now)].cell >= 0 && scene_.empty())
    {
        glm::vec3 at = Feet(s.board, s.units[U(now)]);
        if (s.battle.on && s.units[U(s.battle.defender)].cell >= 0)
            at = (at + Feet(s.board, s.units[U(s.battle.defender)])) * 0.5F;
        const glm::vec3 target{at.x * Look("follow", "pull", 0.0F, 0.2F), 0.0F, at.z * Look("follow", "pull", 0.0F, 0.2F)};
        if (glm::length(target - view_.Target()) > 0.05F)
            view_.Aim(target, Look("follow", "seconds", 0.0F, 0.6F));
    }
    // 2. A scene: its shot moves with its clock, then hands the camera back.
    if (!scene_.empty())
    {
        sceneT_ += dt;
        const float seconds = Look(scene_.c_str(), "seconds", 0.0F, 3.0F), t = std::min(1.0F, sceneT_ / seconds);
        const float e = t * t * (3.0F - 2.0F * t); // smoothstep
        if (scene_ == "open")
            view_.SetShot(true, Look("open", "fromDistance", 0.0F, 40.0F) * (1.0F - e) + Look("open", "toDistance", 0.0F, 19.0F) * e,
                          Look("open", "yaw", 0.0F, -40.0F) * (1.0F - e), Look("open", "fromTilt", 0.0F, 80.0F) * (1.0F - e) + Look("open", "toTilt", 0.0F, 57.0F) * e);
        else
        {
            if (sceneCell_ >= 0)
                view_.Aim(Centre(s.board, sceneCell_), 0.8F);
            view_.SetShot(true, Look("end", "distance", 0.0F, 11.0F), sceneT_ * Look("end", "turn", 0.0F, 12.0F), Look("end", "tilt", 0.0F, 40.0F));
        }
        if (t >= 1.0F && scene_ == "open")
            scene_.clear(), view_.SetShot(false, 0, 0, 50);
    }
    states_.resize(s.units.size());
    for (std::size_t i = 0; i < states_.size(); ++i)
    {
        states_[i].facing = s.units[i].kind == 0 && s.units[i].faction == 1 ? -1.0F : 1.0F;
        states_[i].Update(dt, motions_);
    }
}

void MatchTable::Sprite(int index, const Unit &u, glm::vec3 feet, umbra::core::DrawList &out) const
{
    // 1. Facing the camera square on, upright on screen (a 2D board's figures), moved by its motion.
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0})), up = glm::cross(right, forward);
    const std::string &sprite = u.kind == 0 ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
    const float h = Height(u), aspect = aspects_.contains(sprite) ? aspects_.at(sprite) : 0.3F;
    const umbra::night::SpriteState idle;
    const umbra::night::SpriteState &st = U(index) < states_.size() ? states_[U(index)] : idle;
    const umbra::night::MotionPose pose = st.Pose(motions_);
    auto art = sprites_.find(sprite + "/" + pose.face);
    if (art == sprites_.end())
        art = sprites_.find(sprite + "/calm");
    if (art == sprites_.end())
        return Box(out, box_, feet + glm::vec3{0, h * 0.5F, 0}, {h * aspect, h, h * aspect}, Hex(0x8a8178));
    // 2. The sprite surface: colour = tint and opacity, emissive = how much tint (a monster's colour, or the motion's).
    const float facing = pose.flip ? -st.facing : st.facing;
    const std::uint32_t own = u.kind == 0 ? 0xFFFFFF : def_.monsters[U(u.def)].tint;
    const bool tinted = pose.tint.a > 0.001F;
    const glm::vec3 tint = tinted ? glm::vec3(pose.tint) : glm::vec3(Hex(own));
    const float amount = tinted ? pose.tint.a : (u.kind == 0 ? 0.0F : 0.45F);
    out.items.push_back({facing < 0.0F ? mirrorQuad_ : quad_, art->second, umbra::night::SpriteModel(pose, feet, right, h, aspect, facing, clock_, up),
                         glm::vec4(tint, pose.alpha), 0.0F, std::floor(pose.dissolve * 99.0F) + std::clamp(amount, 0.0F, 1.0F) * 0.99F});
    out.items.back().surface = out.SurfaceIndex("sprite");
}

void MatchTable::SetLook(nlohmann::json look)
{
    look_ = std::move(look);
    // 1. The painted floors each square kind may take; a missing picture is skipped.
    floors_.clear();
    if (!look_.contains("rooms"))
        return;
    for (const char *kind : {"hall", "room", "centre", "hub", "stage"})
        for (const auto &name : look_["rooms"].value(kind, nlohmann::json::array()))
        {
            umbra::core::TextureData data;
            std::string error;
            if (umbra::assets::LoadImageFile(assets_ / ("art/rooms/" + name.get<std::string>() + ".jpg"), data, error))
                floors_[kind].push_back(renderer_.CreateTexture(data));
        }
}

umbra::core::TextureHandle MatchTable::FloorArt(const Board &b, int cell, bool stage) const
{
    // A square and its mirror share a picture, picked by the nearer of the two's index.
    static const char *kinds[] = {"", "hall", "room", "centre", "hub"};
    const auto it = floors_.find(stage ? "stage" : kinds[static_cast<int>(b.cells[U(cell)].kind)]);
    if (it == floors_.end() || it->second.empty())
        return {};
    const int key = std::min(cell, Mirror(b, cell));
    return it->second[U((key * 7 + 3) % static_cast<int>(it->second.size()))];
}

float MatchTable::RoomT() const
{
    const umbra::core::Camera c = view_.Camera();
    const float distance = glm::length(c.position - c.target), far = Look("rooms", "boardDistance", 0.0F, 19.0F), near = Look("rooms", "roomDistance", 0.0F, 7.0F);
    return std::clamp((far - distance) / std::max(0.1F, far - near), 0.0F, 1.0F);
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

void MatchTable::Mark(glm::vec3 at, umbra::core::TextureHandle icon, glm::vec3 color, float strength, umbra::core::DrawList &out) const
{
    // A small glyph standing in the room's corner, facing the camera; all tint (the glyph is white).
    if (!icon)
        return;
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0})), up = glm::cross(right, forward);
    const float size = tile * 0.38F;
    glm::mat4 model{1.0F};
    model[0] = glm::vec4(right * size, 0.0F);
    model[1] = glm::vec4(up * size, 0.0F);
    model[2] = glm::vec4(-forward, 0.0F);
    model[3] = glm::vec4(at + glm::vec3{0.0F, 0.05F, 0.0F} + up * (size * 0.5F), 1.0F);
    out.items.push_back({quad_, icon, model, glm::vec4(color * strength, 1.0F), 0.0F, 0.99F});
    out.items.back().surface = out.SurfaceIndex("sprite");
}

void MatchTable::Shadow(glm::vec3 feet, float width, umbra::core::DrawList &out) const
{
    // A wide rim and a smaller dark core, flat on the floor, a little wider than deep.
    const float dark = Look("shadow", "opacity", 0.0F, 0.6F);
    for (const auto &[scale, shade, lift] : {std::tuple{1.0F, 0.45F, 0.025F}, std::tuple{0.6F, 0.75F, 0.03F}})
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
        // A painted floor, tinted by its kind (the colour shows where no picture is).
        const umbra::core::TextureHandle art = FloorArt(b, c, c == s.stage);
        const glm::vec4 tint = art ? glm::mix(glm::vec4{1.0F}, color, 0.45F) * 0.9F : color;
        out.items.push_back({box_, art, glm::scale(glm::translate(glm::mat4{1.0F}, Centre(b, c) + glm::vec3{0, -0.05F, 0}), {tile * 0.96F, 0.1F, tile * 0.96F}),
                             glm::vec4(glm::vec3(tint), 1.0F), 0.0F, glow});
        // Walls rise on the sides with no door as the camera comes close (the room view); board N is +x, E is +z.
        if (const float t = RoomT(); t > 0.02F)
        {
            const float h = Look("rooms", "wallHeight", 0.0F, 1.1F) * t, half = tile * 0.5F;
            const glm::vec3 side[] = {{half, 0, 0}, {0, 0, half}, {-half, 0, 0}, {0, 0, -half}};
            for (int dir = 0; dir < 4; ++dir)
                if (!(b.cells[U(c)].doors & (1 << dir)))
                    Box(out, box_, Centre(b, c) + side[dir] + glm::vec3{0, h * 0.5F, 0}, dir % 2 == 0 ? glm::vec3{0.08F, h, tile} : glm::vec3{tile, h, 0.08F}, Hex(0x2a1d16));
        }
        if (b.cells[U(c)].event >= 0 && !b.cells[U(c)].resolved) // an event room: a "?" in its corner (optional goals stay quiet)
            Mark(Centre(b, c) + glm::vec3{tile * 0.22F, 0.0F, -tile * 0.22F}, eventIcon_, glm::vec3(Hex(0xB9A2E8)), 1.0F, out);
    }
    // 2b. An item on the floor: a small gold mark in the opposite corner.
    for (const Floor &f : s.floor)
        if (f.cell >= 0 && f.treasure) // a treasure: a chest
            Mark(Centre(b, f.cell) + glm::vec3{tile * 0.22F, 0.0F, tile * 0.22F}, chestIcon_, glm::vec3(Hex(0xE2C068)), 1.0F, out);
        else if (f.cell >= 0) // dropped essence or an item
            Box(out, box_, Centre(b, f.cell) + glm::vec3{tile * 0.3F, 0.02F, tile * 0.3F}, {0.18F, 0.03F, 0.18F}, Hex(0xC9A54A), 0.25F);
    // 2c. A room coming back: its mark, faint, from the round before.
    for (const RoomSpawn &r : s.roomSpawns)
        if (r.round - 1 <= s.round)
            Mark(Centre(b, r.cell) + glm::vec3{tile * 0.22F, 0.0F, r.kind == 0 ? -tile * 0.22F : tile * 0.22F}, r.kind == 0 ? eventIcon_ : chestIcon_,
                 glm::vec3(Hex(0x8a8178)), 0.6F, out);
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
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
    {
        const Unit &u = s.units[U(i)];
        if (u.cell < 0 || u.hp <= 0)
            continue;
        const glm::vec3 feet = Feet(b, u);
        if (i == ringUnit_ && ringT_ < 1.0F) // the turn's ring: grows and fades
            Outline(out, box_, feet - glm::vec3{0, 0.08F, 0}, tile * (0.6F + 0.9F * ringT_), Hex(0xD8B25A), 1.2F * (1.0F - ringT_));
        Shadow(feet + glm::vec3{0.0F, 0.0F, 0.15F}, tile * Look("shadow", "width", 0.0F, 0.75F) * static_cast<float>(u.size), out);
        if (u.faction >= 0)
            Outline(out, box_, feet - glm::vec3{0, 0.1F, 0}, tile * 0.7F * static_cast<float>(u.size), u.faction == 0 ? Hex(0x5b9bd5) : Hex(0xe08a3c), 0.5F);
        Sprite(i, u, feet, out);
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
        labels.push_back({{"unit", i}, {"at", {at->x, at->y}}, {"name", name.substr(name.rfind(' ') + 1)}, {"level", character ? u.level : def_.monsters[U(u.def)].level},
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
