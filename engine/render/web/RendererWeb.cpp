// Browser version of Renderer (roadmap P10, ADR 0001): the same interface as the Vulkan renderer,
// drawn with WebGPU from JavaScript (umbra_render.js) and the HUD as HTML. See WebRenderer.md.
#include <Renderer.hpp>

#include <Lighting.hpp>
#include <Post.hpp>
#include <Log.hpp>
#include <SlotMap.hpp>

#include <emscripten.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

// JavaScript side (umbra_render.js). Ids are small integers owned by JavaScript.
extern "C"
{
    int umbra_gpu_init();                                               // async: 1 ok, 0 no WebGPU
    int umbra_gpu_mesh(const float *vertices, int vertexCount, const unsigned *indices, int indexCount);
    void umbra_gpu_replace_mesh(int id, const float *vertices, int vertexCount, const unsigned *indices, int indexCount);
    void umbra_gpu_mesh_skin(int id, const void *skin, int vertexCount); // core::SkinVertex per vertex
    int umbra_gpu_texture(int width, int height, int levels);
    int umbra_gpu_surface(const char *wgsl); // a custom surface shader's pipeline
    void umbra_gpu_texture_level(int id, int level, int width, int height, const unsigned char *rgba);
    void umbra_gpu_destroy_mesh(int id);
    void umbra_gpu_destroy_texture(int id);
    // `frame`: viewProjection, shadowMatrix, inverseViewProjection (16 floats each), then core::LightingBlock (160 floats).
    // `post`: core::PostBlock::exposure (4 floats).
    void umbra_gpu_particles(const void *particles, int count); // core::ParticleInstance each
    void umbra_gpu_frame(const float *frame, const float *post, const float *draws, int drawCount, const float *lines, int lineCount, const char *hud,
                         const float *joints, int jointCount);
    void umbra_gpu_lut(const unsigned char *strip); // the post color table (256x16 RGBA)
    // The game UI (core::UiDrawList), drawn on top of the frame just sent: 20-byte vertices,
    // uint16 indices, and per batch 8 floats: texture id, clip x0 y0 x1 y1, first vertex, first index, index count.
    void umbra_gpu_ui(const void *vertices, int vertexCount, const unsigned short *indices, int indexCount, const float *batches, int batchCount);
    void umbra_gpu_screen(int on);
    void umbra_gpu_analog(const float *block);
    void umbra_wait_frame();                                            // async: next animation frame
    int umbra_gpu_width();
    int umbra_gpu_height();
    const char *umbra_gpu_name();
}

namespace umbra::render
{
    namespace
    {
        // Floats per draw for JavaScript: model (16), color (4), texturing (4; z = first joint), mesh id,
        // texture id, surface id, skinned (1 = use this frame's joints).
        constexpr int drawFloats = 28;
        constexpr std::size_t maxFrameJoints = 16384; // like the Vulkan renderer; past it items draw unskinned
        constexpr std::size_t shadowMapSize = 2048; // umbra_render.js makes it this size

        struct WebMesh final
        {
            int id = 0;
            glm::vec4 baseColor{1.0F};
            bool skinned = false; // has skin data on the GPU (S8)
        };

        std::string Escape(const std::string &text)
        {
            std::string out;
            for (const char c : text)
            {
                if (c == '"' || c == '\\')
                    out += '\\', out += c;
                else if (c == '\n')
                    out += "\\n";
                else if (static_cast<unsigned char>(c) >= 0x20)
                    out += c;
            }
            return out;
        }

        std::string List(const std::vector<std::string> &items)
        {
            std::string out = "[";
            for (std::size_t i = 0; i < items.size(); ++i)
                out += (i ? ",\"" : "\"") + Escape(items[i]) + "\"";
            return out + "]";
        }

        // The HUD as JSON for the HTML overlay; JavaScript only touches the page when it changed.
        std::string HudJson(const core::Hud &h)
        {
            return "{\"warnings\":" + List(h.warnings) + ",\"diagnostics\":" + (h.diagnosticsWindow ? "true" : "false") +
                   ",\"stats\":" + List(h.stats) + ",\"state\":" + (h.state.empty() ? std::string("{}") : h.state) + "}";
        }
    }

    struct Renderer::Implementation
    {
        core::SlotMap<WebMesh, core::MeshTag> meshes;
        core::SlotMap<int, core::TextureTag> textures;
        std::vector<float> draws, lines;
        float seconds = 0; // drives torch flicker
        std::map<std::string, int> surfaces; // custom surface shader name -> pipeline id (CustomShaders.md)
        core::PostSettings grade;                         // the color table now on the GPU (Post.md)
        std::shared_ptr<const core::TextureData> gradeImage;
    };

    Renderer::Renderer() = default;
    Renderer::~Renderer() { Shutdown(); }

    bool Renderer::Initialize(platform::Window &)
    {
        if (umbra_gpu_init() == 0)
        {
            diagnostics::Error("This browser has no WebGPU. Use Chrome or Edge (version 113 or newer).");
            return false;
        }
        implementation_ = std::make_unique<Implementation>();
        umbra_gpu_lut(core::BakeGradeLut(implementation_->grade).rgba.data()); // neutral until a scene sets a grade
        // Custom surface shaders, converted to WGSL at build time and embedded at /shaders/surface.
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator("/shaders/surface", ec))
            if (entry.path().extension() == ".wgsl")
            {
                std::ifstream in(entry.path());
                const std::string code((std::istreambuf_iterator<char>(in)), {});
                implementation_->surfaces[entry.path().stem().string()] = umbra_gpu_surface(code.c_str());
            }
        diagnostics::Info(std::string{"WebGPU renderer: "} + umbra_gpu_name());
        return true;
    }

    core::MeshHandle Renderer::CreateMesh(const core::MeshData &data)
    {
        if (!implementation_ || !core::ValidateMesh(data).empty())
            return {};
        const int id = umbra_gpu_mesh(&data.vertices[0].position.x, static_cast<int>(data.vertices.size()), data.indices.data(),
                                      static_cast<int>(data.indices.size()));
        if (!data.skin.empty())
            umbra_gpu_mesh_skin(id, data.skin.data(), static_cast<int>(data.skin.size()));
        return implementation_->meshes.Insert({id, data.baseColor, !data.skin.empty()});
    }

    bool Renderer::ReplaceMesh(core::MeshHandle handle, const core::MeshData &data)
    {
        WebMesh *mesh = implementation_ ? implementation_->meshes.Get(handle) : nullptr;
        if (mesh == nullptr || !core::ValidateMesh(data).empty())
            return false;
        umbra_gpu_replace_mesh(mesh->id, &data.vertices[0].position.x, static_cast<int>(data.vertices.size()), data.indices.data(),
                               static_cast<int>(data.indices.size()));
        if (!data.skin.empty())
            umbra_gpu_mesh_skin(mesh->id, data.skin.data(), static_cast<int>(data.skin.size()));
        mesh->baseColor = data.baseColor;
        mesh->skinned = !data.skin.empty();
        return true;
    }

    core::TextureHandle Renderer::CreateTexture(const core::TextureData &data)
    {
        if (!implementation_ || !core::ValidateTexture(data).empty())
            return {};
        // All mip levels, like the Vulkan renderer (MipMaps.md).
        const std::vector<core::TextureData> levels = core::MipChain(data);
        const int id = umbra_gpu_texture(static_cast<int>(data.width), static_cast<int>(data.height), static_cast<int>(levels.size()));
        for (std::size_t level = 0; level < levels.size(); ++level)
            umbra_gpu_texture_level(id, static_cast<int>(level), static_cast<int>(levels[level].width), static_cast<int>(levels[level].height),
                                    levels[level].rgba.data());
        return implementation_->textures.Insert(id);
    }

    bool Renderer::ReplaceTexture(core::TextureHandle handle, const core::TextureData &data)
    {
        int *id = implementation_ ? implementation_->textures.Get(handle) : nullptr;
        if (id == nullptr || !core::ValidateTexture(data).empty())
            return false;
        umbra_gpu_destroy_texture(*id);
        const std::vector<core::TextureData> levels = core::MipChain(data);
        *id = umbra_gpu_texture(static_cast<int>(data.width), static_cast<int>(data.height), static_cast<int>(levels.size()));
        for (std::size_t level = 0; level < levels.size(); ++level)
            umbra_gpu_texture_level(*id, static_cast<int>(level), static_cast<int>(levels[level].width), static_cast<int>(levels[level].height),
                                    levels[level].rgba.data());
        return true;
    }

    bool Renderer::DestroyMesh(core::MeshHandle handle)
    {
        const WebMesh *mesh = implementation_ ? implementation_->meshes.Get(handle) : nullptr;
        if (mesh == nullptr)
            return false;
        umbra_gpu_destroy_mesh(mesh->id);
        return implementation_->meshes.Remove(handle);
    }

    bool Renderer::DestroyTexture(core::TextureHandle handle)
    {
        const int *id = implementation_ ? implementation_->textures.Get(handle) : nullptr;
        if (id == nullptr)
            return false;
        umbra_gpu_destroy_texture(*id);
        return implementation_->textures.Remove(handle);
    }

    bool Renderer::RenderFrame(float deltaSeconds, const core::DrawList &drawList, const std::function<void()> &)
    {
        if (!implementation_)
            return false;
        Implementation &i = *implementation_;
        // 1. Camera: WebGPU has the same 0..1 depth as Vulkan but no Y flip (ADR 0001).
        const auto [width, height] = Extent();
        const core::Camera &camera = drawList.camera;
        const glm::mat4 view = glm::lookAtRH(camera.position, camera.target, glm::vec3{0.0F, 1.0F, 0.0F});
        const glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(camera.fovYDegrees), static_cast<float>(width) / static_cast<float>(std::max(height, 1U)),
                                                           camera.nearPlane, camera.farPlane);
        const glm::mat4 viewProjection = projection * view;
        // Lights, fog and the sun's shadow box: the same numbers as the Vulkan renderer (Lighting.md).
        struct Frame final
        {
            glm::mat4 viewProjection, shadowMatrix, inverseViewProjection;
            core::LightingBlock lighting;
        };
        static_assert(sizeof(Frame) == 208 * sizeof(float));
        i.seconds += deltaSeconds;
        // An item's custom surface shader pipeline id (0 = the plain one).
        const auto surfaceId = [&](const core::DrawItem &item) {
            if (item.surface == 0 || item.surface >= drawList.surfaces.size())
                return 0.0F;
            const auto it = i.surfaces.find(drawList.surfaces[item.surface]);
            return it != i.surfaces.end() ? static_cast<float>(it->second) : 0.0F;
        };
        const Frame frame{viewProjection, core::SunShadowMatrix(drawList.environment.sunDirection, camera.target, 25.0F, shadowMapSize),
                          glm::inverse(viewProjection), core::PackLighting(drawList.environment, drawList.lights, camera.position, i.seconds, shadowMapSize)};

        // 2. One record per draw (mesh color folded in, as the Vulkan renderer does).
        i.draws.clear();
        for (const core::DrawItem &item : drawList.items)
        {
            const WebMesh *mesh = i.meshes.Get(item.mesh);
            if (mesh == nullptr)
                continue;
            const int *texture = i.textures.Get(item.texture);
            const glm::vec4 color = item.color * mesh->baseColor;
            const float *m = glm::value_ptr(item.model);
            const bool skinned = item.jointCount > 0 && mesh->skinned && item.jointOffset + item.jointCount <= maxFrameJoints;
            i.draws.insert(i.draws.end(), m, m + 16);
            i.draws.insert(i.draws.end(), {color.r, color.g, color.b, color.a, item.metersPerRepeat, item.emissive,
                                           skinned ? static_cast<float>(item.jointOffset) : 0.0F, 0.0F, static_cast<float>(mesh->id),
                                           static_cast<float>(texture ? *texture : -1), surfaceId(item), skinned ? 1.0F : 0.0F});
        }
        i.lines.clear();
        for (const core::DebugLine &line : drawList.lines)
            i.lines.insert(i.lines.end(), {line.from.x, line.from.y, line.from.z, line.to.x, line.to.y, line.to.z, line.color.r, line.color.g,
                                           line.color.b, line.color.a});

        // 3. Draw, then wait for the browser's next frame (this is where the game loop yields).
        const std::string hud = HudJson(drawList.hud);
        // The color table again only when the grade changed; the post constants every frame.
        const core::PostSettings &post = drawList.post;
        if (post.temperature != i.grade.temperature || post.saturation != i.grade.saturation || post.contrast != i.grade.contrast ||
            drawList.postLut != i.gradeImage)
        {
            umbra_gpu_lut(core::BakeGradeLut(post, drawList.postLut.get()).rgba.data());
            i.grade = post;
            i.gradeImage = drawList.postLut;
        }
        const core::PostBlock postBlock = core::PackPost(post, 1, 1);
        const bool analog = core::AnalogOn(post); // the frame and the UI into the screen picture, then CRT/VHS (Post.md "6")
        umbra_gpu_screen(analog ? 1 : 0);
        umbra_gpu_particles(drawList.particles.data(), static_cast<int>(std::min(drawList.particles.size(), core::ParticleSystem::maxDrawn)));
        umbra_gpu_frame(glm::value_ptr(frame.viewProjection), glm::value_ptr(postBlock.exposure), i.draws.data(), static_cast<int>(i.draws.size() / drawFloats), i.lines.data(),
                        static_cast<int>(i.lines.size() / 10), hud.c_str(), drawList.joints.empty() ? nullptr : glm::value_ptr(drawList.joints[0]),
                        static_cast<int>(std::min(drawList.joints.size(), maxFrameJoints)));
        if (!drawList.ui.batches.empty())
        {
            std::vector<float> batches;
            for (const core::UiBatch &b : drawList.ui.batches)
            {
                const int *texture = i.textures.Get(b.texture);
                batches.insert(batches.end(), {static_cast<float>(texture ? *texture : -1), b.clip.x, b.clip.y, b.clip.z, b.clip.w,
                                               static_cast<float>(b.firstVertex), static_cast<float>(b.firstIndex), static_cast<float>(b.indexCount)});
            }
            umbra_gpu_ui(drawList.ui.vertices.data(), static_cast<int>(drawList.ui.vertices.size()), drawList.ui.indices.data(),
                         static_cast<int>(drawList.ui.indices.size()), batches.data(), static_cast<int>(drawList.ui.batches.size()));
        }
        if (analog)
        {
            const core::PostBlock block = core::PackAnalog(post, static_cast<std::uint32_t>(umbra_gpu_width()), static_cast<std::uint32_t>(umbra_gpu_height()),
                                                           static_cast<float>(i.seconds));
            umbra_gpu_analog(glm::value_ptr(block.exposure));
        }
        umbra_wait_frame();
        return true;
    }

    void Renderer::RequestResize() noexcept {} // the page sizes the canvas each frame
    void Renderer::ReleaseSurface() noexcept {} // phones only
    void Renderer::RestoreSurface() noexcept {}

    void Renderer::CaptureNextFrame(std::function<void(const core::TextureData &)>) {} // not in the browser (yet)

    std::string Renderer::DeviceName() const { return implementation_ ? umbra_gpu_name() : ""; }

    std::pair<std::uint32_t, std::uint32_t> Renderer::Extent() const
    {
        return {static_cast<std::uint32_t>(std::max(umbra_gpu_width(), 1)), static_cast<std::uint32_t>(std::max(umbra_gpu_height(), 1))};
    }

    void Renderer::Shutdown() noexcept { implementation_.reset(); }
}
