#include "ModelLibrary.hpp"

#include <Log.hpp>
#include <ModelCache.hpp>

ModelLibrary::ModelLibrary(umbra::render::Renderer &renderer, std::filesystem::path assetRoot, std::filesystem::path cacheRoot)
    : renderer_(renderer), assetRoot_(std::move(assetRoot)), cacheRoot_(std::move(cacheRoot))
{
}

ModelLibrary::~ModelLibrary()
{
    for (auto &[path, loaded] : models_)
        Release(loaded);
}

const LoadedModel *ModelLibrary::Load(const std::string &modelPath)
{
    if (const auto it = models_.find(modelPath); it != models_.end())
        return &it->second;
    if (failed_.contains(modelPath))
        return nullptr;

    // 1. Build the cached .umodel if the source (or a dependency) changed.
    using namespace umbra::assets;
    const BuildResult build = BuildModel(assetRoot_, modelPath, cacheRoot_);
    for (const std::string &warning : build.warnings)
        umbra::diagnostics::Info("Warning: " + modelPath + ": " + warning);
    for (const std::string &error : build.errors)
        umbra::diagnostics::Error(modelPath + ": " + error);

    // 2. Read the cached file (still the last good one if the build just failed).
    Model model;
    BuildInfo info;
    std::string error;
    LoadedModel loaded;
    if (!ReadModelFile(build.output, model, info, error) || !Upload(model, loaded))
    {
        umbra::diagnostics::Error("Cannot load model " + modelPath + (error.empty() ? "" : ": " + error));
        failed_.insert(modelPath);
        return nullptr;
    }
    umbra::diagnostics::Info("Loaded model " + modelPath + (build.status == BuildStatus::UpToDate ? " (cached)" : " (built)") +
                             ": " + std::to_string(model.nodes.size()) + " nodes, " + std::to_string(model.meshes.size()) + " meshes.");
    loaded.model = std::move(model);
    return &models_.emplace(modelPath, std::move(loaded)).first->second;
}

std::pair<umbra::core::MeshHandle, umbra::core::TextureHandle> ModelLibrary::Resolve(const std::string &modelPath, std::uint32_t mesh)
{
    const LoadedModel *loaded = Load(modelPath);
    if (loaded == nullptr || mesh >= loaded->meshes.size())
        return {};
    const std::int32_t texture = loaded->model.meshes[mesh].texture;
    return {loaded->meshes[mesh], texture >= 0 ? loaded->textures[static_cast<std::size_t>(texture)] : umbra::core::TextureHandle{}};
}

bool ModelLibrary::Upload(const umbra::assets::Model &model, LoadedModel &loaded)
{
    // All or nothing: on any failure, release what was already uploaded.
    for (const umbra::core::TextureData &texture : model.textures)
        loaded.textures.push_back(renderer_.CreateTexture(texture));
    for (const umbra::assets::ModelMesh &mesh : model.meshes)
        loaded.meshes.push_back(renderer_.CreateMesh(mesh.data));
    for (const auto handle : loaded.meshes)
    {
        if (!handle)
        {
            Release(loaded);
            return false;
        }
    }
    return true;
}

void ModelLibrary::Release(LoadedModel &loaded)
{
    for (const auto handle : loaded.meshes)
        renderer_.DestroyMesh(handle);
    for (const auto handle : loaded.textures)
        renderer_.DestroyTexture(handle);
    loaded.meshes.clear();
    loaded.textures.clear();
}
