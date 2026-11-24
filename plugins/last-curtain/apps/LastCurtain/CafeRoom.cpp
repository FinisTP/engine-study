#include "CafeRoom.hpp"

#include "CampaignRoom.hpp"

#include <Lighting.hpp>
#include <MatchDef.hpp>
#include <MeshData.hpp>
#include <TextureTools.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

// The café drawn (CafeRoom.md).
namespace
{
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
        for (const glm::vec2 c : {glm::vec2{-0.5F, 0.0F}, glm::vec2{0.5F, 0.0F}, glm::vec2{0.5F, 1.0F}, glm::vec2{-0.5F, 1.0F}}) // feet at the bottom middle
            mesh.vertices.push_back({{c.x, c.y, 0.0F}, {0.0F, 0.0F, 1.0F}, {mirrored ? 0.5F - c.x : c.x + 0.5F, 1.0F - c.y}});
        mesh.indices = {0, 1, 2, 0, 2, 3};
        return mesh;
    }

    void Box(umbra::core::DrawList &out, umbra::core::MeshHandle mesh, glm::vec3 centre, glm::vec3 size, glm::vec4 color, float glow = 0.0F,
             umbra::core::TextureHandle texture = {}, float repeat = 0.0F)
    {
        out.items.push_back({mesh, texture, glm::scale(glm::translate(glm::mat4{1.0F}, centre), size), color, repeat, glow});
    }

    // Room metres (x across, y deep) to the world: x, 0, z.
    glm::vec3 World(glm::vec2 p, float y = 0.0F) { return {p.x, y, p.y}; }
    glm::vec2 At(const nlohmann::json &spot) { return {spot["at"][0].get<float>(), spot["at"][1].get<float>()}; }

    std::uint32_t StationColour(const std::string &id)
    {
        return id == "phin" ? 0x3a2416 : id == "tea" ? 0x5f7f3a : id == "milk" ? 0xe8dcc0 : id == "ice" ? 0xa8d4e8 : 0x8a8178;
    }
}

CafeRoom::CafeRoom(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets, const umbra::cafe::CafeDef &def,
                   const umbra::match::MatchDef &match)
    : def_(def), match_(match), renderer_(renderer), assets_(assets), box_(renderer.CreateMesh(Cube())), quad_(renderer.CreateMesh(Quad(false))),
      mirrorQuad_(renderer.CreateMesh(Quad(true)))
{
    umbra::core::TextureData data;
    std::string error;
    if (umbra::assets::LoadImageFile(assets / "art/ui/match/wall.jpg", data, error))
        wall_ = renderer.CreateTexture(data);
    if (umbra::assets::LoadImageFile(assets / "art/cafe/back_wall.jpg", data, error))
        backWall_ = renderer.CreateTexture(data);
    for (const umbra::match::CharacterDef &c : match.characters)
        Picture(c.sprite);
    for (const umbra::cafe::Guest &g : def.guests)
        Picture(g.sprite);
}

umbra::core::TextureHandle CafeRoom::Picture(const std::string &sprite)
{
    // 1. Its calm figure; a ghost without its own picture yet takes the shared specter.
    if (const auto it = pictures_.find(sprite); it != pictures_.end())
        return it->second;
    for (const std::string &folder : {sprite, std::string{"specter"}})
    {
        umbra::core::TextureData data;
        std::string error;
        if (umbra::assets::LoadImageFile(assets_ / ("art/sprites/" + folder + "/body_calm.png"), data, error))
        {
            aspects_[sprite] = static_cast<float>(data.width) / static_cast<float>(std::max(1u, data.height));
            data.clamp = true; // a figure: no bleeding at its edges
            return pictures_[sprite] = renderer_.CreateTexture(data);
        }
    }
    return pictures_[sprite] = {};
}

umbra::core::Camera CafeRoom::Camera() const
{
    umbra::core::Camera c;
    c.target = look_ + glm::vec3{0.0F, 0.8F + 0.5F * close_, 0.0F};
    c.position = look_ + glm::mix(glm::vec3{0.0F, 4.2F, 6.4F}, glm::vec3{0.0F, 1.9F, 3.4F}, close_);
    return c;
}

void CafeRoom::Update(const umbra::night::CafePlay &play, float dt)
{
    Follow(play.Position(), {play.Room()["size"][0].get<float>(), play.Room()["size"][1].get<float>()}, dt);
}

void CafeRoom::Look(glm::vec2 at, float close, float dt)
{
    clock_ += dt;
    const float k = std::min(1.0F, dt * 3.0F);
    look_ += (glm::vec3{at.x, 0.0F, at.y} - look_) * k;
    close_ += (close - close_) * k;
}

void CafeRoom::Follow(glm::vec2 at, glm::vec2 size, float dt)
{
    // 1. The camera eases after the walker across the room; the walker faces the way it goes.
    clock_ += dt;
    const glm::vec3 want{std::clamp(at.x, 3.5F, size.x - 3.5F), 0.0F, std::clamp(at.y, 3.0F, size.y - 2.0F)};
    look_ += (want - look_) * std::min(1.0F, dt * 3.0F);
    close_ -= close_ * std::min(1.0F, dt * 3.0F); // back from a scene's close-up
    if (std::abs(at.x - last_.x) > 0.001F)
        facing_ = at.x > last_.x ? 1.0F : -1.0F;
    walk_ = glm::length(at - last_) > 0.001F ? walk_ + dt : 0.0F;
    last_ = at;
}

void CafeRoom::Figure(const std::string &sprite, glm::vec3 feet, float height, float facing, umbra::core::DrawList &out) const
{
    // A flat figure facing the camera, upright, its feet on the floor.
    const auto it = pictures_.find(sprite);
    const float aspect = aspects_.contains(sprite) ? aspects_.at(sprite) : 0.4F;
    glm::mat4 model{1.0F};
    model = glm::translate(model, feet);
    model = glm::scale(model, {height * aspect, height, 1.0F});
    out.items.push_back({facing < 0.0F ? mirrorQuad_ : quad_, it != pictures_.end() ? it->second : umbra::core::TextureHandle{}, model, glm::vec4(1.0F), 0.0F, 0.0F});
    out.items.back().surface = out.SurfaceIndex("sprite");
}

void CafeRoom::DrawShell(const nlohmann::json &room, umbra::core::DrawList &out) const
{
    const glm::vec2 size{room["size"][0].get<float>(), room["size"][1].get<float>()};
    // 1. Light: indoors and warm; a lamp over each table and two over the counter.
    umbra::core::EnvironmentSettings settings;
    settings.indoor = true, settings.ambient = 1.0F, settings.fog = 0.0F, settings.shadows = false;
    out.environment = umbra::core::EvaluateEnvironment(settings, 20.0F);
    out.post.exposure = 0.1F, out.post.saturation = 0.9F, out.post.vignette = 0.35F, out.post.bloom = 0.25F, out.post.temperature = 0.2F;
    out.camera = Camera();
    for (const float x : {size.x * 0.25F, size.x * 0.75F})
        out.lights.push_back({{x, 2.8F, 1.2F}, {1.0F, 0.82F, 0.6F}, 14.0F, 9.0F, 0.0F});
    for (const nlohmann::json &b : room["blocks"])
        if (b.value("note", std::string{}).starts_with("table"))
            out.lights.push_back({{(b["min"][0].get<float>() + b["max"][0].get<float>()) * 0.5F, 2.4F, 3.8F}, {1.0F, 0.78F, 0.55F}, 9.0F, 6.0F, 0.0F});
    // 2. The floor, the walls (the painted café wall at the back, paper at the sides), the counter and the tables.
    Box(out, box_, World(size * 0.5F, -0.05F), {size.x, 0.1F, size.y}, Hex(0x4a3324));
    Box(out, box_, {size.x * 0.5F, 1.6F, -0.1F}, {size.x + 0.4F, 3.2F, 0.2F}, glm::vec4(0.8F, 0.75F, 0.7F, 1.0F), 0.0F, wall_, 1.0F);
    if (backWall_) // the painting over the wall's face, upright, edge to edge
        out.items.push_back({quad_, backWall_, glm::scale(glm::translate(glm::mat4{1.0F}, {size.x * 0.5F, 0.0F, 0.005F}), {size.x + 0.4F, 3.2F, 1.0F}), glm::vec4(1.0F), 0.0F, 0.0F});
    for (const float x : {-0.1F, size.x + 0.1F})
        Box(out, box_, {x, 1.6F, size.y * 0.5F}, {0.2F, 3.2F, size.y}, glm::vec4(0.7F, 0.66F, 0.62F, 1.0F), 0.0F, wall_, 1.0F);
    for (const nlohmann::json &b : room["blocks"])
    {
        const glm::vec2 lo{b["min"][0].get<float>(), b["min"][1].get<float>()}, hi{b["max"][0].get<float>(), b["max"][1].get<float>()};
        const bool counter = b.value("note", std::string{}) == "the counter";
        const float h = counter ? 1.0F : 0.72F;
        Box(out, box_, World((lo + hi) * 0.5F, h * 0.5F), {hi.x - lo.x, h, hi.y - lo.y}, Hex(counter ? 0x3b2418 : 0x5a3a26));
        Box(out, box_, World((lo + hi) * 0.5F, h + 0.02F), {hi.x - lo.x + 0.06F, 0.04F, hi.y - lo.y + 0.06F}, Hex(counter ? 0x8a6a3a : 0x7a5434)); // the top
    }
}

void CafeRoom::Draw(const umbra::night::CafePlay &play, umbra::core::DrawList &out) const
{
    const nlohmann::json &room = play.Room();
    const glm::vec2 size{room["size"][0].get<float>(), room["size"][1].get<float>()};
    DrawShell(room, out);
    // 3. The spots: stations on the counter, the sign, the bucket, the board, the door; the one in reach glows.
    const int near = play.Near();
    for (int i = 0; i < static_cast<int>(room["spots"].size()); ++i)
    {
        const nlohmann::json &spot = room["spots"][U(i)];
        const std::string kind = spot["kind"];
        const glm::vec2 p = At(spot);
        const float glow = i == near ? 0.6F + 0.3F * std::sin(clock_ * 5.0F) : 0.0F;
        if (kind == "station")
            Box(out, box_, World({p.x, 0.5F}, 1.2F), {0.6F, 0.4F, 0.5F}, Hex(StationColour(spot["station"])), glow + 0.1F);
        else if (kind == "sign")
        {
            Box(out, box_, World({p.x, 0.5F}, 1.6F), {0.1F, 1.2F, 0.1F}, Hex(0x2a1d16));
            Box(out, box_, World({p.x, 0.55F}, 2.2F), {1.2F, 0.45F, 0.06F}, play.Open() ? Hex(0x4fae5a) : Hex(0xb5423a), glow + 0.5F);
        }
        else if (kind == "trash")
            Box(out, box_, World(p, 0.3F), {0.45F, 0.6F, 0.45F}, Hex(0x2e2a26), glow);
        else if (kind == "board")
            Box(out, box_, World({0.05F, p.y}, 1.5F), {0.08F, 1.2F, 1.6F}, Hex(0xc9b48a), glow + 0.1F);
        else if (kind == "door")
            Box(out, box_, World({size.x - 0.02F, p.y - 0.4F}, 1.2F), {0.1F, 2.4F, 1.1F}, Hex(0x5a1a1a), glow + 0.1F);
        else if (kind == "seat" && i == near)
            Box(out, box_, World(p, 0.01F), {0.9F, 0.02F, 0.6F}, Hex(0xd8b25a), glow);
    }
    // 4. The ghosts at their tables (behind them, facing the room), then the walker.
    const umbra::cafe::Shift &s = play.Shift();
    for (int seat = 0; seat < static_cast<int>(s.seats.size()); ++seat)
        if (s.seats[U(seat)].guest >= 0)
            for (const nlohmann::json &spot : room["spots"])
                if (spot["kind"] == "seat" && spot["seat"] == seat)
                {
                    const float bob = 0.04F * std::sin(clock_ * 2.0F + static_cast<float>(seat));
                    Figure(def_.guests[U(s.seats[U(seat)].guest)].sprite, World({At(spot).x, 3.2F}, 0.1F + bob), 1.7F, seat % 2 ? -1.0F : 1.0F, out);
                }
    const int walker = umbra::match::MatchDef::Find(match_.characters, play.Walker());
    const std::string sprite = walker >= 0 ? match_.characters[U(walker)].sprite : "specter";
    Figure(sprite, World(play.Position(), 0.02F + (walk_ > 0.0F ? 0.04F * std::abs(std::sin(walk_ * 9.0F)) : 0.0F)), 1.8F, facing_, out);
}

std::optional<glm::vec2> CafeRoom::ScreenPoint(glm::vec3 world, glm::vec2 pixels) const
{
    const umbra::core::Camera camera = Camera();
    const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / std::max(pixels.y, 1.0F), camera.nearPlane, camera.farPlane);
    const glm::vec4 clip = projection * view * glm::vec4(world, 1.0F);
    if (clip.w <= 0.0F)
        return std::nullopt;
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return glm::vec2{(ndc.x * 0.5F + 0.5F) * pixels.x, (0.5F - ndc.y * 0.5F) * pixels.y};
}

std::optional<glm::vec2> CafeRoom::Floor(glm::vec2 mouse, glm::vec2 pixels, glm::vec2 roomSize) const
{
    // 1. The mouse's ray through the camera to the floor (y = 0).
    if (pixels.x <= 0.0F || pixels.y <= 0.0F)
        return std::nullopt;
    const umbra::core::Camera camera = Camera();
    const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(camera.fovYDegrees), pixels.x / pixels.y, camera.nearPlane, camera.farPlane);
    const glm::mat4 inverse = glm::inverse(projection * view);
    const glm::vec2 ndc{mouse.x / pixels.x * 2.0F - 1.0F, 1.0F - mouse.y / pixels.y * 2.0F};
    glm::vec4 a = inverse * glm::vec4(ndc, -1.0F, 1.0F), b = inverse * glm::vec4(ndc, 1.0F, 1.0F);
    a /= a.w, b /= b.w;
    if (std::abs(b.y - a.y) < 1e-5F)
        return std::nullopt;
    const float t = -a.y / (b.y - a.y);
    const glm::vec3 hit = glm::vec3(a) + (glm::vec3(b) - glm::vec3(a)) * t;
    if (t < 0.0F || hit.x < 0.0F || hit.z < 0.0F || hit.x > roomSize.x || hit.z > roomSize.y)
        return std::nullopt;
    return glm::vec2{hit.x, hit.z};
}

nlohmann::json CafeRoom::Labels(const umbra::night::CafePlay &play, glm::vec2 pixels) const
{
    // 1. Each order over its ghost; the talk's line over its speaker.
    const nlohmann::json ui = play.UiData();
    nlohmann::json labels = nlohmann::json::array();
    const auto seatAt = [&](int seat) -> std::optional<glm::vec2> {
        for (const nlohmann::json &spot : play.Room()["spots"])
            if (spot["kind"] == "seat" && spot["seat"] == seat)
                return ScreenPoint(World({At(spot).x, 3.2F}, 1.95F), pixels);
        return std::nullopt;
    };
    for (const nlohmann::json &o : ui["orders"])
        if (const auto at = seatAt(o["seat"]))
            labels.push_back({{"at", {at->x, at->y}}, {"order", true}, {"guest", o["guest"]}, {"drink", o["drink"]}, {"patience", o["patience"]}, {"maxPatience", o["maxPatience"]}});
    for (const nlohmann::json &b : ui["bubbles"])
    {
        const auto at = b["seat"].get<int>() >= 0 ? seatAt(b["seat"]) : ScreenPoint(World(play.Position(), 2.0F), pixels);
        if (at)
            labels.push_back({{"at", {at->x, at->y - (b["seat"].get<int>() >= 0 ? 48.0F : 0.0F)}}, {"say", b["text"]}});
    }
    return labels;
}

void CafeRoom::DrawCampaign(const CampaignRoom &room, const std::map<std::string, std::string> &sprites, const std::string &near, umbra::core::DrawList &out) const
{
    const nlohmann::json &r = room.Room();
    const glm::vec2 size{r["size"][0].get<float>(), r["size"][1].get<float>()};
    DrawShell(r, out);
    // 1. The spots: the brewing on the counter, the notice board, the door; the one E would use glows.
    for (const nlohmann::json &spot : r["campaign"]["spots"])
    {
        const std::string kind = spot["kind"];
        const glm::vec2 p = At(spot);
        const float glow = kind == near ? 0.6F + 0.3F * std::sin(clock_ * 5.0F) : 0.0F;
        if (kind == "brew")
            Box(out, box_, World({p.x, 0.5F}, 1.2F), {0.8F, 0.4F, 0.5F}, Hex(0x6b4a2a), glow + 0.1F);
        else if (kind == "notebook")
            Box(out, box_, World({0.05F, p.y}, 1.5F), {0.08F, 1.2F, 1.6F}, Hex(0xc9b48a), glow + 0.1F);
        else if (kind == "door")
            Box(out, box_, World({size.x - 0.02F, p.y - 0.4F}, 1.2F), {0.1F, 2.4F, 1.1F}, Hex(0x5a1a1a), glow + 0.1F);
    }
    // 2. Everyone standing at their place (a gold ring under the one E would talk to), then the walker.
    const auto sprite = [&](const std::string &who) { return sprites.contains(who) ? sprites.at(who) : std::string("specter"); };
    for (const CampaignRoom::Person &p : room.People())
    {
        if (near == "talk:" + p.who)
            Box(out, box_, World(p.at, 0.01F), {0.9F, 0.02F, 0.6F}, Hex(0xd8b25a), 0.6F + 0.3F * std::sin(clock_ * 5.0F));
        Figure(sprite(p.who), World(p.at, 0.02F + 0.02F * std::sin(clock_ * 1.5F + p.at.x)), 1.75F, p.at.x < size.x * 0.5F ? 1.0F : -1.0F, out);
    }
    Figure(sprite(room.Active()), World(room.Position(), 0.02F + (walk_ > 0.0F ? 0.04F * std::abs(std::sin(walk_ * 9.0F)) : 0.0F)), 1.8F, facing_, out);
}

std::optional<glm::vec2> CafeRoom::Above(glm::vec2 floor, float height, glm::vec2 pixels) const
{
    return ScreenPoint(World(floor, height), pixels);
}
