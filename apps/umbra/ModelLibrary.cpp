#include "ModelLibrary.hpp"

#include <Log.hpp>
#include <Material.hpp>
#include <ModelCache.hpp>
#include <TextureTools.hpp>

ModelLibrary::ModelLibrary(umbra::render::Renderer &renderer, umbra::assets::AssetFolders assetRoot, std::filesystem::path cacheRoot)
    : renderer_(renderer), assetRoot_(std::move(assetRoot)), cacheRoot_(std::move(cacheRoot))
{
}

ModelLibrary::~ModelLibrary()
{
    for (auto &[path, loaded] : models_)
        Release(loaded);
    for (auto &[path, loaded] : materials_)
        renderer_.DestroyTexture(loaded.binding.texture);
}

const LoadedModel *ModelLibrary::Load(const std::string &modelPath)
{
    if (const auto it = models_.find(modelPath); it != models_.end())
        return &it->second;
    if (failed_.contains(modelPath))
        return nullptr;

    LoadedModel loaded;
    if (!BuildAndUpload(modelPath, loaded))
    {
        failed_.insert(modelPath);
        return nullptr;
    }
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

std::optional<umbra::scene::MaterialBinding> ModelLibrary::ResolveMaterial(const std::string &materialPath)
{
    if (const auto it = materials_.find(materialPath); it != materials_.end())
        return it->second.binding;
    if (failed_.contains(materialPath))
        return std::nullopt;
    LoadedMaterial loaded;
    if (!BuildMaterial(materialPath, loaded))
    {
        failed_.insert(materialPath);
        return std::nullopt;
    }
    return materials_.emplace(materialPath, std::move(loaded)).first->second.binding;
}

bool ModelLibrary::BuildMaterial(const std::string &materialPath, LoadedMaterial &loaded)
{
    // 1. The material file and its image, with the same checks as the scene loader.
    std::vector<umbra::core::Diagnostic> diagnostics;
    umbra::assets::Material material;
    umbra::core::TextureData image;
    std::string error;
    if (!umbra::assets::LoadMaterial(assetRoot_ / materialPath, material, diagnostics) ||
        !umbra::assets::CheckMaterialImage(assetRoot_, material, materialPath, diagnostics) ||
        !umbra::assets::LoadImageFile(assetRoot_ / material.baseColor, image, error))
    {
        for (const umbra::core::Diagnostic &d : diagnostics)
            umbra::diagnostics::Error(umbra::core::Format(d));
        if (!error.empty())
            umbra::diagnostics::Error(materialPath + ": " + error);
        return false;
    }
    // 2. Upload (with mipmaps, see MipMaps.md).
    loaded.binding.texture = renderer_.CreateTexture(image);
    if (!loaded.binding.texture)
    {
        umbra::diagnostics::Error("Cannot upload " + material.baseColor + " to the GPU.");
        return false;
    }
    loaded.binding.tint = material.tint;
    loaded.binding.metersPerRepeat = material.worldProjection ? material.metersPerRepeat : 0.0F;
    loaded.binding.surface = umbra::assets::SurfaceName(material.shader);
    loaded.files = {assetRoot_ / materialPath, assetRoot_ / material.baseColor};
    umbra::diagnostics::Info("Loaded material " + materialPath + " (" + std::to_string(image.width) + "x" + std::to_string(image.height) + ").");
    return true;
}

ModelLibrary::ReloadResult ModelLibrary::Reload(const std::string &modelPath)
{
    // A material: new texture first, then swap; on failure the old one stays.
    if (modelPath.ends_with(".material.json"))
    {
        LoadedMaterial fresh;
        if (!BuildMaterial(modelPath, fresh))
            return ReloadResult::Failed;
        failed_.erase(modelPath);
        if (const auto it = materials_.find(modelPath); it != materials_.end())
        {
            renderer_.DestroyTexture(it->second.binding.texture);
            it->second = std::move(fresh);
        }
        else
            materials_.emplace(modelPath, std::move(fresh));
        return ReloadResult::Updated;
    }

    // 1. Build and upload a complete new version next to the old one.
    LoadedModel fresh;
    if (!BuildAndUpload(modelPath, fresh))
    {
        umbra::diagnostics::Error("Reload of " + modelPath + " failed; keeping the previous version.");
        return ReloadResult::Failed;
    }

    // 2. Publish: the new version replaces the old in one step. The caller rebinds
    //    renderables before the next frame; old GPU data is retired, not freed at once.
    failed_.erase(modelPath);
    const auto it = models_.find(modelPath);
    const bool sameStructure = it != models_.end() && umbra::assets::SameStructure(it->second.model, fresh.model);
    if (it != models_.end())
    {
        Release(it->second);
        it->second = std::move(fresh);
    }
    else
        models_.emplace(modelPath, std::move(fresh));
    umbra::diagnostics::Info("Reloaded " + modelPath + (sameStructure ? "." : " (structure changed)."));
    return sameStructure ? ReloadResult::Updated : ReloadResult::StructureChanged;
}

std::map<std::filesystem::path, std::string> ModelLibrary::WatchedFiles() const
{
    std::map<std::filesystem::path, std::string> files;
    for (const auto &[path, loaded] : models_)
        for (const auto &file : loaded.files)
            files.emplace(file, path);
    for (const auto &[path, loaded] : materials_)
        for (const auto &file : loaded.files)
            files.emplace(file, path);
    for (const auto &path : failed_)
        files.emplace(assetRoot_ / path, path);
    return files;
}

bool ModelLibrary::BuildAndUpload(const std::string &modelPath, LoadedModel &loaded)
{
    // 1. Build the cached .umodel if the source (or a dependency) changed.
    using namespace umbra::assets;
    const BuildResult build = BuildModel(assetRoot_.RootOf(modelPath), modelPath, cacheRoot_);
    for (const std::string &warning : build.warnings)
        umbra::diagnostics::Info("Warning: " + modelPath + ": " + warning);
    for (const std::string &error : build.errors)
        umbra::diagnostics::Error(modelPath + ": " + error);
    if (build.status == BuildStatus::Failed)
        return false;

    // 2. Read it back.
    BuildInfo info;
    std::string error;
    if (!ReadModelFile(build.output, loaded.model, info, error))
    {
        umbra::diagnostics::Error("Cannot read " + build.output.string() + ": " + error);
        return false;
    }
    loaded.files = {assetRoot_ / modelPath};
    for (const std::string &dependency : info.dependencies)
        loaded.files.push_back((assetRoot_ / modelPath).parent_path() / dependency);

    // 3. Upload everything; on any failure release what was uploaded (all or nothing).
    for (const umbra::core::TextureData &texture : loaded.model.textures)
        loaded.textures.push_back(renderer_.CreateTexture(texture));
    for (const umbra::assets::ModelMesh &mesh : loaded.model.meshes)
        loaded.meshes.push_back(renderer_.CreateMesh(mesh.data));
    for (const auto handle : loaded.meshes)
    {
        if (!handle)
        {
            Release(loaded);
            umbra::diagnostics::Error("Cannot upload " + modelPath + " to the GPU.");
            return false;
        }
    }
    umbra::diagnostics::Info("Loaded model " + modelPath + (build.status == BuildStatus::UpToDate ? " (cached)" : " (built)") +
                             ": " + std::to_string(loaded.model.nodes.size()) + " nodes, " +
                             std::to_string(loaded.model.meshes.size()) + " meshes.");
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
