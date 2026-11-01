#pragma once

#include <Model.hpp>
#include <Renderer.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
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
};

// Model path ("models/tower.gltf") -> build or refresh the cached .umodel -> read it -> upload it.
class ModelLibrary final
{
public:
    ModelLibrary(umbra::render::Renderer &renderer, std::filesystem::path assetRoot, std::filesystem::path cacheRoot);
    ~ModelLibrary();

    ModelLibrary(const ModelLibrary &) = delete;
    ModelLibrary &operator=(const ModelLibrary &) = delete;

    // Loads a model once; later calls return the same entry. nullptr on failure (logged once).
    const LoadedModel *Load(const std::string &modelPath);
    // GPU handles for one mesh of a model (loading it if needed). Invalid handles if unknown.
    std::pair<umbra::core::MeshHandle, umbra::core::TextureHandle> Resolve(const std::string &modelPath, std::uint32_t mesh);

private:
    bool Upload(const umbra::assets::Model &model, LoadedModel &loaded);
    void Release(LoadedModel &loaded);

    umbra::render::Renderer &renderer_;
    std::filesystem::path assetRoot_;
    std::filesystem::path cacheRoot_;
    std::map<std::string, LoadedModel> models_;
    std::set<std::string> failed_;
};
