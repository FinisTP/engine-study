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
    constexpr float tile = 1.0F;           // metres per tile (MatchTable.md)
    constexpr float figureMetres = 0.625F; // figure heights below are for 1.6 m squares: scaled to the tile
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
    // 2b. The turn's arrow: a triangle pointing down, its tip at the origin.
    umbra::core::MeshData arrow;
    arrow.vertices = {{{0, 0, 0}, {0, 0, 1}, {0.5F, 1}}, {{0.5F, 1, 0}, {0, 0, 1}, {1, 0}}, {{-0.5F, 1, 0}, {0, 0, 1}, {0, 0}}};
    arrow.indices = {0, 1, 2, 0, 2, 1};
    triangle_ = renderer.CreateMesh(arrow);
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
    load("specter"); // a ghost without its own art yet takes the shared specter (Sprite)
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

float MatchTable::Height(const Unit &u, int index) const
{
    // A fighter grows to the battle's size as the view comes in (smoothstep of fightT_).
    const float e = fightT_ * fightT_ * (3.0F - 2.0F * fightT_);
    const float scale = Fighter(index) >= 0 ? FigureScale() + (Look("battle", "figureScale", 0.0F, 0.7F) - FigureScale()) * e : FigureScale();
    if (u.kind == 0)
        return 3.0F * scale * figureMetres;
    static const float byTier[] = {1.5F, 2.2F, 2.6F, 2.6F}; // weak, strong, great, guardian
    return byTier[static_cast<int>(def_.monsters[U(u.def)].tier)] * std::min(static_cast<float>(u.size), 1.8F) * scale * figureMetres;
}

float MatchTable::FigureScale() const
{
    // Height in metres grows with the camera's distance, so a figure keeps its size on screen.
    const umbra::core::Camera c = view_.Camera();
    const float distance = glm::length(c.position - c.target), board = Look("rooms", "boardDistance", 0.0F, 19.0F);
    return std::clamp(distance / board, Look("figures", "minScale", 0.0F, 0.3F), Look("figures", "maxScale", 0.0F, 1.6F));
}

int MatchTable::Fighter(int unit) const
{
    for (int p = 0; p < 4; ++p)
        if (unit >= 0 && staged_[U(p)] == unit)
            return p;
    return -1;
}

void MatchTable::SetFight(const std::vector<int> &places, int picking)
{
    fight_.fill(-1);
    for (std::size_t p = 0; p < places.size() && p < 4; ++p)
        fight_[p] = places[p];
    picking_ = picking;
}

bool MatchTable::Present(const MatchState &s, int unit) const
{
    return s.units[U(unit)].cell >= 0 || std::ranges::find(standing_, unit) != standing_.end();
}

glm::vec3 MatchTable::SpriteUp() const
{
    // Leaning back with the camera keeps a figure upright on screen; only part way, so its head stays out of the wall behind.
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0}));
    return glm::normalize(glm::mix(glm::vec3{0, 1, 0}, glm::cross(right, forward), Look("figures", "lean", 0.0F, 0.5F)));
}

void MatchTable::LookAt(const MatchState &s, int unit)
{
    if (unit < 0 || U(unit) >= s.units.size() || !Present(s, unit))
        return;
    follow_ = false;
    const glm::vec3 at = Shown(s, unit);
    view_.Aim({at.x, 0.0F, at.z}, Look("follow", "seconds", 0.0F, 0.5F));
}

std::optional<glm::vec2> MatchTable::HeadOnScreen(const MatchState &s, int unit, glm::vec2 pixels) const
{
    if (unit < 0 || U(unit) >= s.units.size() || !Present(s, unit))
        return std::nullopt;
    return ScreenPoint(Head(s, unit), pixels);
}

glm::vec3 MatchTable::Head(const MatchState &s, int unit) const
{
    // Along the figure's own up, so its card and arrow sit on its head even when it leans.
    const glm::vec3 feet = Shown(s, unit) + (fightT_ >= 0.5F && Fighter(unit) >= 0 ? FightNear() : glm::vec3{0.0F});
    return feet + SpriteUp() * Height(s.units[U(unit)], unit);
}

glm::vec3 MatchTable::Shown(const MatchState &s, int unit) const
{
    const auto walked = [&](int i) {
        return U(i) < shown_.size() && (lastCell_[U(i)] == s.units[U(i)].cell || !s.units[U(i)].hp) ? shown_[U(i)] : Feet(s.board, s.units[U(i)]);
    };
    const int place = Fighter(unit);
    if (place < 0)
        return walked(unit);
    // A fighter: its group on its side of the stage's middle (the attackers on the left), the second a little behind, eased in.
    const umbra::core::Camera camera = view_.Camera();
    glm::vec3 forward = camera.target - camera.position;
    forward.y = 0.0F;
    forward = glm::normalize(forward);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0}));
    const float e = fightT_ * fightT_ * (3.0F - 2.0F * fightT_), side = place < 2 ? -1.0F : 1.0F, second = static_cast<float>(place % 2);
    const glm::vec3 stand = stageMiddle_ + right * side * (Look("battle", "gap", 0.0F, 0.7F) + second * Look("battle", "pairGap", 0.0F, 0.9F)) * tile +
                            forward * second * Look("battle", "pairDepth", 0.0F, 0.8F);
    return glm::mix(walked(unit), stand, e);
}

void MatchTable::Walk(const MatchState &s, float dt)
{
    const Board &b = s.board;
    shown_.resize(s.units.size()), route_.resize(s.units.size()), lastCell_.resize(s.units.size(), -1);
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 right = glm::normalize(glm::cross(camera.target - camera.position, glm::vec3{0, 1, 0}));
    const float speed = Look("walk", "squaresPerSecond", 0.0F, 3.5F) * tile;
    // 0. Where each stands: two on one tile stand apart, east and west of its middle.
    std::vector<glm::vec3> spot(s.units.size());
    for (std::size_t i = 0; i < s.units.size(); ++i)
        if (s.units[i].cell >= 0)
        {
            int before = 0, there = 0;
            for (std::size_t j = 0; j < s.units.size(); ++j)
                if (s.units[j].cell == s.units[i].cell && s.units[j].hp > 0)
                    ++there, before += j < i;
            const float apart = there > 1 ? (before == 0 ? -1.0F : 1.0F) * Look("walk", "shareApart", 0.0F, 0.22F) * tile : 0.0F;
            spot[i] = Feet(b, s.units[i]) + glm::vec3{0.0F, 0.0F, apart};
        }
    for (std::size_t i = 0; i < s.units.size(); ++i)
    {
        const Unit &u = s.units[i];
        // 1. A new square: the route there through doors (a breadth-first search, display only), or a jump.
        //    A fallen one whose fall is not yet told keeps standing where it was.
        if (u.cell != lastCell_[i] && !(u.cell < 0 && Present(s, static_cast<int>(i))))
        {
            const int from = lastCell_[i];
            lastCell_[i] = u.cell, route_[i].clear();
            if (u.cell < 0)
                continue;
            const glm::vec3 offset = spot[i] - Centre(b, u.cell);
            std::vector<int> back(b.cells.size(), -2);
            std::vector<int> queue{from};
            if (from >= 0)
                back[U(from)] = -1;
            for (std::size_t q = 0; from >= 0 && q < queue.size() && back[U(u.cell)] == -2; ++q)
                for (int dir = 0; dir < 4; ++dir)
                    if (const int n = Neighbour(b, queue[q], dir); n >= 0 && back[U(n)] == -2)
                        back[U(n)] = queue[q], queue.push_back(n);
            if (from < 0 || back[U(u.cell)] == -2)
            {
                shown_[i] = spot[i];
                continue;
            }
            for (int c = u.cell; c != from; c = back[U(c)])
                route_[i].insert(route_[i].begin(), Centre(b, c) + offset);
            if (route_[i].size() > 24) // too far to walk (a respawn): appear there
                shown_[i] = route_[i].back(), route_[i].clear();
            if (!route_[i].empty() && U(i) < states_.size())
                states_[i].Base("walk");
        }
        // 2. Walk toward the next point at a steady pace, facing the way it goes on screen.
        while (!route_[i].empty() && dt > 0.0F)
        {
            const glm::vec3 to = route_[i].front() - shown_[i];
            const float d = glm::length(to), step = speed * dt;
            if (std::abs(glm::dot(to, right)) > 0.01F && i < states_.size())
                states_[i].facing = glm::dot(to, right) > 0.0F ? 1.0F : -1.0F;
            if (d > step)
            {
                shown_[i] += to * (step / d);
                break;
            }
            shown_[i] = route_[i].front(), route_[i].erase(route_[i].begin()), dt -= d / speed;
            if (route_[i].empty() && i < states_.size())
                states_[i].Base("idle");
        }
        // 3. Standing: drift to its spot (a friend arrived on the tile, or left it).
        if (route_[i].empty() && u.cell >= 0 && u.cell == lastCell_[i])
        {
            const glm::vec3 to = spot[i] - shown_[i];
            const float d = glm::length(to), step = speed * 0.5F * std::max(dt, 0.0F);
            shown_[i] = d > step ? shown_[i] + to * (step / d) : spot[i];
        }
    }
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
    else if (motion == "walk") // the route sets the walk (Walk)
        return;
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
    states_.resize(s.units.size());
    Walk(s, dt);
    // 1b. The battle view eases in while a fight is asked for and out after; the camera comes down into the room.
    const bool fighting = fight_[0] >= 0 && fight_[2] >= 0 && U(std::max(fight_[0], fight_[2])) < s.units.size();
    if (fighting && (staged_[0] != fight_[0] || staged_[2] != fight_[2])) // a new fight: its stage stands between the two who began it
    {
        staged_.fill(-1);
        stageMiddle_ = (Shown(s, fight_[0]) + Shown(s, fight_[2])) * 0.5F;
        stageCell_ = s.units[U(fight_[2])].cell >= 0 ? s.units[U(fight_[2])].cell : s.units[U(fight_[0])].cell;
    }
    if (fighting)
        staged_ = fight_;
    fightT_ = std::clamp(fightT_ + (fighting ? dt : -dt) / Look("battle", "seconds", 0.0F, 0.6F), 0.0F, 1.0F);
    if (fightT_ <= 0.0F)
        staged_.fill(-1);
    // 1c. A told line's people: framed together, the camera backing off far enough to see them all.
    std::erase_if(focus_, [&](int u) { return u < 0 || U(u) >= s.units.size() || s.units[U(u)].cell < 0; });
    if (!focus_.empty())
        framed_ = focus_;
    focusT_ = std::clamp(focusT_ + (focus_.empty() ? -dt : dt) / Look("follow", "seconds", 0.0F, 0.5F), 0.0F, 1.0F);
    std::erase_if(framed_, [&](int u) { return U(u) >= s.units.size() || s.units[U(u)].cell < 0; });
    if (focusT_ <= 0.0F)
        framed_.clear();
    float spread = 0.0F;
    for (const int a : framed_)
        for (const int b : framed_)
            spread = std::max(spread, glm::length(Shown(s, a) - Shown(s, b)));
    if (scene_.empty())
    {
        const float e = fightT_ * fightT_ * (3.0F - 2.0F * fightT_), q = focusT_ * focusT_ * (3.0F - 2.0F * focusT_);
        const float far = spread * Look("follow", "fit", 0.0F, 2.4F) + Look("follow", "margin", 0.0F, 6.0F);
        if (e > 0.0F)
            view_.SetShot(true, view_.Distance() + (Look("battle", "distance", 0.0F, 7.0F) - view_.Distance()) * e, 0.0F,
                          view_.Tilt() + (Look("battle", "tilt", 0.0F, 30.0F) - view_.Tilt()) * e);
        else if (q > 0.0F && far > view_.Distance())
            view_.SetShot(true, view_.Distance() + (far - view_.Distance()) * q, 0.0F, view_.Tilt());
        else
            view_.SetShot(false, 0.0F, 0.0F, 50.0F);
    }
    if (follow_ && now >= 0 && s.units[U(now)].cell >= 0 && scene_.empty())
    {
        glm::vec3 at = Shown(s, now);
        if (Staged()) // a battle: its stage
            at = stageMiddle_;
        else if (!framed_.empty()) // a told line: the middle of whom it is about
        {
            at = glm::vec3{0.0F};
            for (const int u : framed_)
                at += Shown(s, u) / static_cast<float>(framed_.size());
        }
        // The figure in the middle, but the board stays on screen: the farther out, the nearer the board's middle it may go.
        const float pull = Look("follow", "pull", 0.0F, 1.0F), keep = Look("follow", "edge", 0.0F, 0.45F) + (1.0F - Look("follow", "edge", 0.0F, 0.45F)) * RoomT();
        const glm::vec2 half{static_cast<float>(s.board.height) * tile * 0.5F * keep, static_cast<float>(s.board.width) * tile * 0.5F * keep};
        const glm::vec3 target{std::clamp(at.x * pull, -half.x, half.x), 0.0F, std::clamp(at.z * pull, -half.y, half.y)};
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
    for (std::size_t i = 0; i < states_.size(); ++i)
    {
        if (Fighter(static_cast<int>(i)) >= 0) // fighters face each other
            states_[i].facing = Fighter(static_cast<int>(i)) < 2 ? 1.0F : -1.0F;
        else if (route_[i].empty()) // standing: side 1's characters face left
            states_[i].facing = s.units[i].kind == 0 && s.units[i].faction == 1 ? -1.0F : 1.0F;
        states_[i].Update(dt, motions_);
    }
}

void MatchTable::Sprite(int index, const Unit &u, glm::vec3 feet, umbra::core::DrawList &out) const
{
    // 1. Facing the camera square on, upright on screen (a 2D board's figures), moved by its motion.
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0})), up = SpriteUp();
    const std::string &sprite = u.kind == 0 ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
    const float h = Height(u, index);
    float aspect = aspects_.contains(sprite) ? aspects_.at(sprite) : 0.3F;
    const umbra::night::SpriteState idle;
    const umbra::night::SpriteState &st = U(index) < states_.size() ? states_[U(index)] : idle;
    const umbra::night::MotionPose pose = st.Pose(motions_);
    auto art = sprites_.find(sprite + "/" + pose.face);
    if (art == sprites_.end())
        art = sprites_.find(sprite + "/calm");
    if (art == sprites_.end())
        art = sprites_.find("specter/" + pose.face), aspect = aspects_.contains("specter") ? aspects_.at("specter") : aspect;
    if (art == sprites_.end())
        art = sprites_.find("specter/calm");
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
    // 1. The fight's backdrops by square kind, then the painted floors each kind may take; a missing picture is skipped.
    backdrops_.clear();
    if (look_.contains("battle") && look_["battle"].contains("backdrops"))
        for (const auto &[kind, file] : look_["battle"]["backdrops"].items())
        {
            umbra::core::TextureData data;
            std::string error;
            if (umbra::assets::LoadImageFile(assets_ / file.get<std::string>(), data, error))
                backdrops_[kind] = renderer_.CreateTexture(data);
        }
    floors_.clear();
    if (!look_.contains("rooms"))
        return;
    if (umbra::core::TextureData data; look_["rooms"].contains("wallArt"))
        if (std::string error; umbra::assets::LoadImageFile(assets_ / look_["rooms"]["wallArt"].get<std::string>(), data, error))
            wallArt_ = renderer_.CreateTexture(data);
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
    // A small glyph standing in the room's middle, facing the camera; all tint (the glyph is white).
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

glm::vec3 MatchTable::FightNear() const
{
    const umbra::core::Camera camera = view_.Camera();
    glm::vec3 back = camera.position - camera.target;
    back.y = 0.0F;
    return glm::length(back) > 0.0F ? glm::normalize(back) * Look("battle", "near", 0.0F, 2.5F) : glm::vec3{0.0F};
}

void MatchTable::DrawFight(const MatchState &s, umbra::core::DrawList &out) const
{
    // 1. Where: the fighters' middle; the backdrop stands behind them square to the camera, a floor before it.
    const umbra::core::Camera camera = view_.Camera();
    glm::vec3 forward = camera.target - camera.position;
    forward.y = 0.0F;
    forward = glm::normalize(forward);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0})), middle = stageMiddle_;
    const float width = Look("battle", "backdropWidth", 0.0F, 24.0F), depth = Look("battle", "backdropDepth", 0.0F, 3.0F);
    // The picture by the kind of square the defender stands on (the stage counts as the centre).
    static const char *kinds[] = {"", "hall", "room", "centre", "hub"};
    const int cell = std::max(0, stageCell_);
    auto art = backdrops_.find(cell == s.stage ? "centre" : kinds[static_cast<int>(s.board.cells[U(cell)].kind)]);
    if (art == backdrops_.end())
        art = backdrops_.find("default");
    const float height = width / Look("battle", "backdropAspect", 0.0F, 1.75F);
    glm::mat4 model{1.0F};
    model[0] = glm::vec4(right * width, 0.0F);
    model[1] = glm::vec4(glm::vec3{0, 1, 0} * height, 0.0F);
    model[2] = glm::vec4(-forward, 0.0F);
    model[3] = glm::vec4(middle + forward * depth + glm::vec3{0.0F, height * 0.5F - Look("battle", "backdropDrop", 0.0F, 1.5F), 0.0F}, 1.0F);
    out.items.push_back({quad_, art != backdrops_.end() ? art->second : umbra::core::TextureHandle{}, model, glm::vec4(1.0F), 0.0F, Look("battle", "backdropGlow", 0.0F, 0.7F)});
    Box(out, box_, middle + forward * (depth * 0.5F - 3.0F) + glm::vec3{0.0F, -0.05F, 0.0F}, glm::vec3{width, 0.1F, width} * glm::vec3{1.0F, 1.0F, 0.5F}, Hex(0x0d0907));
    // 2. A pool of warm light on the two, then the two themselves and their shadows.
    out.lights.push_back({middle + glm::vec3{0.0F, 4.0F, 0.0F} - forward * 2.0F, {1.0F, 0.85F, 0.65F}, Look("battle", "light", 0.0F, 25.0F), 14.0F, 0.0F});
    for (const int i : staged_)
    {
        if (i < 0 || !Present(s, i)) // an empty place, or one whose fall has been told
            continue;
        const Unit &u = s.units[U(i)];
        const glm::vec3 at = Shown(s, i) + FightNear(); // a little nearer the camera than its middle: low on the screen, in front of the backdrop
        Shadow(at + glm::vec3{0.0F, 0.0F, 0.15F}, tile * Look("shadow", "width", 0.0F, 0.75F) * Height(u, i) / std::max(0.01F, Height(u, -1)) * FigureScale(), out);
        Sprite(i, u, at, out);
        if (i == picking_) // whose pick it is: the gold arrow bobbing over its head
            Arrow(Head(s, i) + glm::vec3{0.0F, 0.25F + 0.1F * std::sin(clock_ * 4.0F), 0.0F}, 0.35F, out);
    }
}

void MatchTable::Arrow(glm::vec3 tip, float size, umbra::core::DrawList &out) const
{
    // Square to the camera like the figures; both faces drawn so it shows from any side.
    const umbra::core::Camera camera = view_.Camera();
    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0})), up = glm::cross(right, forward);
    glm::mat4 model{1.0F};
    model[0] = glm::vec4(right * size, 0.0F);
    model[1] = glm::vec4(up * size, 0.0F);
    model[2] = glm::vec4(-forward, 0.0F);
    model[3] = glm::vec4(tip, 1.0F);
    out.items.push_back({triangle_, {}, model, Hex(0xE8C060), 0.0F, 1.2F});
}

void MatchTable::Edge(glm::vec3 centre, int dir, glm::vec4 color, float glow, umbra::core::DrawList &out) const
{
    // A glowing bar along one side of a square, just inside it and raised off the floor.
    const float bar = glow > 0.7F ? 0.18F : 0.12F, h = tile * 0.5F - bar * 0.5F;
    const glm::vec3 side[] = {{h, 0, 0}, {0, 0, h}, {-h, 0, 0}, {0, 0, -h}};
    Box(out, box_, centre + side[dir] + glm::vec3{0, 0.04F, 0}, dir % 2 == 0 ? glm::vec3{bar, 0.06F, tile} : glm::vec3{tile, 0.06F, bar}, color, glow);
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
    // 1b. The battle view: the screen dips to dark halfway, and past it the fight has its own stage (DrawFight).
    const float fe = fightT_ * fightT_ * (3.0F - 2.0F * fightT_);
    out.post.exposure -= Look("battle", "fade", 0.0F, 3.0F) * std::max(0.0F, 1.0F - std::abs(2.0F * fe - 1.0F));
    if (fe >= 0.5F && Staged())
        return DrawFight(s, out);

    // 2. Squares by kind; the stage glows gold; walls stay dark under the table. The marks: reach cyan, an armed range orange, targets red.
    std::map<int, glm::vec4> marks;
    for (int c : Cells(ui.value("reach", nlohmann::json::array())))
        marks[c] = ui.contains("armed") ? Hex(0xE08A3C) : Hex(0x3FD0E0);
    for (int c : Cells(ui.value("targets", nlohmann::json::array())))
        marks[c] = Hex(0xE8453F);
    Box(out, box_, {0.0F, -0.2F, 0.0F}, {static_cast<float>(b.height) * tile + 0.6F, 0.2F, static_cast<float>(b.width) * tile + 0.6F}, Hex(0x1a1411));
    for (int c = 0; c < static_cast<int>(b.cells.size()); ++c)
    {
        const CellKind kind = b.cells[U(c)].kind;
        if (kind == CellKind::Wall)
        {
            // A wall is a tile of its own: a papered block, low on the board, rising as the camera comes close.
            const float low = Look("rooms", "wallLow", 0.0F, 0.45F), h = low + (Look("rooms", "wallHeight", 0.0F, 0.9F) - low) * RoomT();
            const float shade = Look("rooms", "wallShade", 0.0F, 0.55F); // darker than the floors, so the rooms stand out
            out.items.push_back({box_, wallArt_, glm::scale(glm::translate(glm::mat4{1.0F}, Centre(b, c) + glm::vec3{0, h * 0.5F, 0}), {tile, h, tile}),
                                 wallArt_ ? glm::vec4(shade, shade * 0.95F, shade * 0.9F, 1.0F) : Hex(0x5e4838), wallArt_ ? Look("rooms", "wallRepeat", 0.0F, 0.8F) : 0.0F});
            continue;
        }
        glm::vec4 color = kind == CellKind::Hall ? Hex(0x3a2b21) : kind == CellKind::Room ? Hex(0x47393a) : kind == CellKind::Centre ? Hex(0x5c2226)
                        : c == b.hub[0] ? Hex(0x24405e) : Hex(0x5e3a1c);
        float glow = 0.0F;
        if (c == s.stage)
            color = Hex(0x8a6a24), glow = 0.35F + 0.1F * std::sin(clock_ * 2.0F);
        // A painted floor, tinted by its kind (the colour shows where no picture is).
        const umbra::core::TextureHandle art = FloorArt(b, c, c == s.stage);
        const bool zone = c == s.stage || c == b.hub[0] || c == b.hub[1]; // the goals keep a strong colour
        glm::vec4 tint = art ? glm::mix(glm::vec4{1.0F}, color, zone ? 0.7F : 0.45F) * (zone ? 1.2F : 0.9F) : color;
        // The player's reach (cyan) and targets (red): the whole tile washed half over and blinking, its picture still seen.
        if (const auto mark = marks.find(c); mark != marks.end())
        {
            const float blink = 0.5F + 0.5F * std::sin(clock_ * 5.0F);
            tint = glm::mix(tint, mark->second * 1.6F, Look("reach", "wash", 0.0F, 0.5F) * (0.6F + 0.4F * blink));
            glow = std::max(glow, Look("reach", "glow", 0.0F, 0.35F) * blink);
        }
        out.items.push_back({box_, art, glm::scale(glm::translate(glm::mat4{1.0F}, Centre(b, c) + glm::vec3{0, -0.05F, 0}), {tile * 0.98F, 0.1F, tile * 0.98F}),
                             glm::vec4(glm::vec3(tint), 1.0F), 0.0F, glow});
        if (b.cells[U(c)].event >= 0 && !b.cells[U(c)].resolved) // an event room: a "?" in its corner (optional goals stay quiet)
            Mark(Centre(b, c), eventIcon_, glm::vec3(Hex(0xB9A2E8)), 1.0F, out);
    }
    // 2a. The zones, edged where they meet other squares: each hub in its side's colour, the centre red, the stage gold.
    for (int c = 0; c < static_cast<int>(b.cells.size()); ++c)
    {
        const CellKind kind = b.cells[U(c)].kind;
        const int x = c % b.width, y = c / b.width;
        for (int dir = 0; dir < 4; ++dir)
        {
            // Board N is y - 1 (+x on the table), E x + 1.
            const int nx = x + (dir == 1) - (dir == 3), ny = y - (dir == 0) + (dir == 2);
            const bool inside = nx >= 0 && ny >= 0 && nx < b.width && ny < b.height;
            const int n = inside ? ny * b.width + nx : -1;
            if (kind == CellKind::Centre && (n < 0 || b.cells[U(n)].kind != CellKind::Centre))
                Edge(Centre(b, c), dir, Hex(0xC0393B), 0.6F, out);
            if (c == b.hub[0] || c == b.hub[1])
                Edge(Centre(b, c), dir, c == b.hub[0] ? Hex(0x5b9bd5) : Hex(0xe08a3c), 0.8F + 0.3F * std::sin(clock_ * 2.0F), out);
            if (c == s.stage)
                Edge(Centre(b, c), dir, Hex(0xE8C060), 1.0F + 0.4F * std::sin(clock_ * 2.0F), out);
        }
    }
    // 2b. An item on the floor: a small gold mark in the opposite corner.
    for (const Floor &f : s.floor)
        if (f.cell >= 0 && f.treasure) // a treasure: a chest
            Mark(Centre(b, f.cell), chestIcon_, glm::vec3(Hex(0xE2C068)), 1.0F, out);
        else if (f.cell >= 0) // dropped essence or an item
            Box(out, box_, Centre(b, f.cell) + glm::vec3{tile * 0.3F, 0.02F, tile * 0.3F}, {0.18F, 0.03F, 0.18F}, Hex(0xC9A54A), 0.25F);
    // 2c. A room coming back: its mark, faint, from the round before.
    for (const RoomSpawn &r : s.roomSpawns)
        if (r.round - 1 <= s.round)
            Mark(Centre(b, r.cell), r.kind == 0 ? eventIcon_ : chestIcon_,
                 glm::vec3(Hex(0x8a8178)), 0.6F, out);
    // 3. Announced spawns: an orange marker pulsing on the square.
    for (const Spawn &w : s.spawns)
        if (w.round - def_.rules.spawn.warn <= s.round)
            Outline(out, box_, Centre(b, w.cell), tile * 0.9F, Hex(0xE08A3C), 0.5F + 0.4F * std::sin(clock_ * 4.0F));
    // 4. The player's turn: the reach (blue, or orange as an armed ability's range), the path, the targets (red), the hovered square.
    // (the reach and the targets are blinking fills on the floor itself, step 2)
    for (int c : Cells(ui.value("path", nlohmann::json::array())))
        Box(out, box_, Centre(b, c) + glm::vec3{0, 0.08F, 0}, {0.2F, 0.06F, 0.2F}, Hex(0xEFE6D6), 0.8F);
    if (hover >= 0 && hover < static_cast<int>(b.cells.size()) && b.cells[U(hover)].kind != CellKind::Wall)
        Outline(out, box_, Centre(b, hover), tile, Hex(0xEFE6D6), 0.6F);
    // 5. Figures, a ring in the side's colour under each character, the acting one's arrow drawn as a card (Labels).
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
    {
        const Unit &u = s.units[U(i)];
        if (!Present(s, i))
            continue;
        const glm::vec3 feet = Shown(s, i);
        if (i == ringUnit_ && ringT_ < 1.0F) // the turn's ring: grows and fades
            Outline(out, box_, feet - glm::vec3{0, 0.08F, 0}, tile * (0.6F + 0.9F * ringT_), Hex(0xD8B25A), 1.2F * (1.0F - ringT_));
        const float k = Height(u, i) / std::max(0.01F, Height(u, -1)) * FigureScale(); // a fighter's shadow grows with it
        Shadow(feet + glm::vec3{0.0F, 0.0F, 0.15F * k}, tile * Look("shadow", "width", 0.0F, 0.75F) * static_cast<float>(u.size) * k, out);
        if (u.faction >= 0)
            Outline(out, box_, feet - glm::vec3{0, 0.1F, 0}, tile * 0.7F * static_cast<float>(u.size), u.faction == 0 ? Hex(0x5b9bd5) : Hex(0xe08a3c), 0.5F);
        Sprite(i, u, feet, out);
        if (i == Current(s) && !s.battle.on) // the one whose turn it is: a gold arrow bobbing over its head
            Arrow(Head(s, i) + glm::vec3{0.0F, 0.35F * FigureScale() + 0.12F * FigureScale() * std::sin(clock_ * 4.0F), 0.0F}, 0.45F * FigureScale(), out);
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
        if (!Present(s, i))
            continue;
        if (fightT_ > 0.3F && Fighter(i) < 0) // the battle view: only the fighters, and only their bubbles
            continue;
        const auto at = ScreenPoint(Head(s, i) + glm::vec3{0.0F, 0.12F, 0.0F}, pixels);
        if (!at)
            continue;
        const bool character = u.kind == 0;
        const std::string sprite = character ? def_.characters[U(u.def)].sprite : def_.monsters[U(u.def)].sprite;
        const std::string &name = character ? def_.characters[U(u.def)].name : def_.monsters[U(u.def)].name;
        labels.push_back({{"unit", i}, {"at", {at->x, at->y}}, {"name", name.substr(name.rfind(' ') + 1)}, {"level", character ? u.level : def_.monsters[U(u.def)].level},
                          {"hp", U(i) < toldHp_.size() ? toldHp_[U(i)] : u.hp}, {"maxHp", u.maxHp}, {"portrait", "art/sprites/" + sprite + "/face_calm.png"}, {"acting", i == acting},
                          {"essence", character && u.essence > 0 ? std::to_string(u.essence) : ""}, {"side", u.faction},
                          {"mine", u.faction == 0}, {"theirs", u.faction == 1}, {"wild", u.faction < 0}, {"foe", u.faction != 0}, {"plate", fightT_ <= 0.3F}});
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
