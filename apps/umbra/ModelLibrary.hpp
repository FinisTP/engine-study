#pragma once

#include <Model.hpp>
#include <ModelInstance.hpp>
#include <Renderer.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// A model file turned into GPU resources. meshes[i] / textures[i] match model.meshes / model.textures.
struct LoadedModel final
{
    umbra::assets::Model model;
    std::vector<umbra::core::MeshHandle> meshes;
    std::vector<umbra::core::TextureHandle> textures;
    std::vector<std::filesystem::path> files; // source + dependencies, for change watching
};

// A material file and its image on the GPU.
struct LoadedMaterial final
{
    umbra::scene::MaterialBinding binding;
    std::vector<std::filesystem::path> files; // material file + image, for change watching
};

// Model path ("models/tower.gltf") -> build or refresh the cached .umodel -> read it -> upload it.
class ModelLibrary final
{
public:
    enum class ReloadResult
    {
        Failed,          // kept the previous version
        Updated,         // new GPU data, same node structure: rebind handles
        StructureChanged // nodes changed: entities built from this model must be rebuilt
    };

    ModelLibrary(umbra::render::Renderer &renderer, std::filesystem::path assetRoot, std::filesystem::path cacheRoot);
    ~ModelLibrary();

    ModelLibrary(const ModelLibrary &) = delete;
    ModelLibrary &operator=(const ModelLibrary &) = delete;

    // Loads a model once; later calls return the same entry. nullptr on failure (logged once).
    const LoadedModel *Load(const std::string &modelPath);
    // GPU handles for one mesh of a model (loading it if needed). Invalid handles if unknown.
    std::pair<umbra::core::MeshHandle, umbra::core::TextureHandle> Resolve(const std::string &modelPath, std::uint32_t mesh);

    // Material path ("materials/stone.material.json") -> texture, tint, texture scale. Loaded once;
    // nullopt on failure (logged once; the model's own texture is used).
    std::optional<umbra::scene::MaterialBinding> ResolveMaterial(const std::string &materialPath);

    // Editor previews: a texture that belongs to no file. The caller destroys it.
    umbra::core::TextureHandle CreatePreviewTexture(const umbra::core::TextureData &data) { return renderer_.CreateTexture(data); }
    void DestroyPreviewTexture(umbra::core::TextureHandle handle) { renderer_.DestroyTexture(handle); }
    // Editor helpers (the ground grid): a mesh that belongs to no file. The caller destroys it.
    umbra::core::MeshHandle CreateLooseMesh(const umbra::core::MeshData &data) { return renderer_.CreateMesh(data); }
    void DestroyLooseMesh(umbra::core::MeshHandle handle) { renderer_.DestroyMesh(handle); }

    // Rebuilds and re-uploads a model (or a material: path ends in ".material.json") as one transaction (see Reload.md). A model that failed
    // before is simply tried again.
    ReloadResult Reload(const std::string &modelPath);
    // Every watched file -> the model path it belongs to (failed models: source only).
    [[nodiscard]] std::map<std::filesystem::path, std::string> WatchedFiles() const;
    // Models currently loaded (kept for the whole run, so this grows only with distinct files).
    [[nodiscard]] std::size_t Count() const noexcept { return models_.size(); }

private:
    bool BuildAndUpload(const std::string &modelPath, LoadedModel &loaded);
    bool BuildMaterial(const std::string &materialPath, LoadedMaterial &loaded);
    void Release(LoadedModel &loaded);

    umbra::render::Renderer &renderer_;
    std::filesystem::path assetRoot_;
    std::filesystem::path cacheRoot_;
    std::map<std::string, LoadedModel> models_;
    std::map<std::string, LoadedMaterial> materials_;
    std::set<std::string> failed_;
};
