#include <GltfImporter.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <cstring>
#include <exception>
#include <functional>

namespace umbra::assets
{
    namespace
    {
        // Returns the first byte of an accessor's data, or nullptr if any element would be
        // read outside its buffer view (malformed or hostile files must not read out of bounds).
        const unsigned char *AccessorData(const tinygltf::Model &model, const tinygltf::Accessor &accessor,
                                          std::size_t elementSize, std::size_t &stride)
        {
            if (accessor.sparse.isSparse || accessor.count == 0 || accessor.bufferView < 0 ||
                static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size())
            {
                return nullptr;
            }
            const tinygltf::BufferView &view = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
            if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= model.buffers.size())
            {
                return nullptr;
            }
            const int byteStride = accessor.ByteStride(view);
            if (byteStride <= 0)
            {
                return nullptr;
            }
            stride = static_cast<std::size_t>(byteStride);
            const std::vector<unsigned char> &data = model.buffers[static_cast<std::size_t>(view.buffer)].data;
            const std::size_t begin = view.byteOffset + accessor.byteOffset;
            const std::size_t end = begin + (accessor.count - 1) * stride + elementSize;
            if (end > view.byteOffset + view.byteLength || end > data.size())
            {
                return nullptr;
            }
            return data.data() + begin;
        }

        std::size_t ComponentSize(int componentType)
        {
            switch (componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return 4;
            default: return 0;
            }
        }

        std::uint32_t ReadIndex(const unsigned char *data, std::size_t size)
        {
            std::uint32_t value = 0;
            std::memcpy(&value, data, size); // little-endian: low bytes first
            return value;
        }

        // Copies a float attribute (VEC2/VEC3) into every vertex. Returns false if malformed.
        template <typename Member>
        bool ReadFloats(const tinygltf::Model &model, const tinygltf::Primitive &primitive, const char *name,
                        int type, std::vector<core::Vertex> &vertices, Member member, bool &present)
        {
            present = false;
            const auto it = primitive.attributes.find(name);
            if (it == primitive.attributes.end())
            {
                return true;
            }
            if (it->second < 0 || static_cast<std::size_t>(it->second) >= model.accessors.size())
            {
                return false;
            }
            const tinygltf::Accessor &accessor = model.accessors[static_cast<std::size_t>(it->second)];
            using Value = std::remove_reference_t<decltype(vertices[0].*member)>;
            std::size_t stride = 0;
            const unsigned char *data = AccessorData(model, accessor, sizeof(Value), stride);
            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != type || data == nullptr ||
                (!vertices.empty() && accessor.count != vertices.size()))
            {
                return false;
            }
            vertices.resize(accessor.count);
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                std::memcpy(&(vertices[i].*member), data + i * stride, sizeof(Value));
            }
            present = true;
            return true;
        }

        bool IsSafeRelativePath(const std::string &uri)
        {
            const std::filesystem::path path{uri};
            if (path.is_absolute() || path.has_root_name() || uri.find(':') != std::string::npos)
            {
                return false;
            }
            for (const auto &part : path)
            {
                if (part == "..")
                {
                    return false;
                }
            }
            return true;
        }

        void ImportTextures(const tinygltf::Model &gltf, ImportResult &result)
        {
            for (std::size_t t = 0; t < gltf.textures.size(); ++t)
            {
                const int source = gltf.textures[t].source;
                core::TextureData texture;
                if (source < 0 || static_cast<std::size_t>(source) >= gltf.images.size())
                {
                    result.errors.push_back("texture " + std::to_string(t) + " has no image");
                    result.model.textures.push_back(std::move(texture));
                    continue;
                }
                const tinygltf::Image &image = gltf.images[static_cast<std::size_t>(source)];
                if (image.bits != 8 || image.component < 1 || image.component > 4 || image.width <= 0 || image.height <= 0 ||
                    image.image.size() != static_cast<std::size_t>(image.width) * image.height * image.component)
                {
                    result.errors.push_back("image " + std::to_string(source) + " is not 8-bit 1-4 channel pixel data");
                    result.model.textures.push_back(std::move(texture));
                    continue;
                }

                // Expand to RGBA: missing color channels copy the first one, missing alpha is 255.
                texture.width = static_cast<std::uint32_t>(image.width);
                texture.height = static_cast<std::uint32_t>(image.height);
                const std::size_t pixels = std::size_t{texture.width} * texture.height;
                const std::size_t c = static_cast<std::size_t>(image.component);
                texture.rgba.resize(pixels * 4);
                for (std::size_t p = 0; p < pixels; ++p)
                {
                    const unsigned char *in = image.image.data() + p * c;
                    std::uint8_t *out = texture.rgba.data() + p * 4;
                    out[0] = in[0];
                    out[1] = c >= 3 ? in[1] : in[0];
                    out[2] = c >= 3 ? in[2] : in[0];
                    out[3] = c == 4 ? in[3] : (c == 2 ? in[1] : 255);
                }
                result.model.textures.push_back(std::move(texture));
            }
        }

        // Returns, for each glTF mesh, the indices of its primitives in result.model.meshes.
        std::vector<std::vector<std::uint32_t>> ImportMeshes(const tinygltf::Model &gltf, ImportResult &result)
        {
            std::vector<std::vector<std::uint32_t>> primitivesOfMesh(gltf.meshes.size());
            for (std::size_t m = 0; m < gltf.meshes.size(); ++m)
            {
                for (std::size_t p = 0; p < gltf.meshes[m].primitives.size(); ++p)
                {
                    const tinygltf::Primitive &primitive = gltf.meshes[m].primitives[p];
                    const std::string where = "mesh " + std::to_string(m) + " primitive " + std::to_string(p);
                    if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES)
                    {
                        result.warnings.push_back(where + ": only triangle lists are supported, skipped");
                        continue;
                    }
                    if (!primitive.targets.empty())
                    {
                        result.warnings.push_back(where + ": morph targets ignored");
                    }

                    // 1. Vertex attributes. POSITION is required, NORMAL and TEXCOORD_0 are optional.
                    ModelMesh mesh;
                    bool hasPosition = false, hasNormal = false, hasUv = false;
                    if (!ReadFloats(gltf, primitive, "POSITION", TINYGLTF_TYPE_VEC3, mesh.data.vertices, &core::Vertex::position, hasPosition) || !hasPosition ||
                        !ReadFloats(gltf, primitive, "NORMAL", TINYGLTF_TYPE_VEC3, mesh.data.vertices, &core::Vertex::normal, hasNormal) ||
                        !ReadFloats(gltf, primitive, "TEXCOORD_0", TINYGLTF_TYPE_VEC2, mesh.data.vertices, &core::Vertex::uv, hasUv))
                    {
                        result.errors.push_back(where + ": missing or malformed float POSITION/NORMAL/TEXCOORD_0");
                        continue;
                    }
                    if (!hasNormal)
                    {
                        result.warnings.push_back(where + ": no normals, lighting will be wrong");
                    }

                    // 2. Indices, or 0..n-1 when the primitive is not indexed.
                    if (primitive.indices >= 0)
                    {
                        if (static_cast<std::size_t>(primitive.indices) >= gltf.accessors.size())
                        {
                            result.errors.push_back(where + ": index accessor out of range");
                            continue;
                        }
                        const tinygltf::Accessor &accessor = gltf.accessors[static_cast<std::size_t>(primitive.indices)];
                        const std::size_t size = ComponentSize(accessor.componentType);
                        std::size_t stride = 0;
                        const unsigned char *data = size ? AccessorData(gltf, accessor, size, stride) : nullptr;
                        if (data == nullptr || accessor.type != TINYGLTF_TYPE_SCALAR)
                        {
                            result.errors.push_back(where + ": malformed indices");
                            continue;
                        }
                        mesh.data.indices.resize(accessor.count);
                        for (std::size_t i = 0; i < accessor.count; ++i)
                        {
                            mesh.data.indices[i] = ReadIndex(data + i * stride, size);
                        }
                    }
                    else
                    {
                        mesh.data.indices.resize(mesh.data.vertices.size());
                        for (std::size_t i = 0; i < mesh.data.indices.size(); ++i)
                        {
                            mesh.data.indices[i] = static_cast<std::uint32_t>(i);
                        }
                    }

                    // 3. Material: base color factor and texture only.
                    if (primitive.material >= 0 && static_cast<std::size_t>(primitive.material) < gltf.materials.size())
                    {
                        const auto &pbr = gltf.materials[static_cast<std::size_t>(primitive.material)].pbrMetallicRoughness;
                        if (pbr.baseColorFactor.size() == 4)
                        {
                            mesh.data.baseColor = {pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
                        }
                        if (pbr.baseColorTexture.index >= 0 && static_cast<std::size_t>(pbr.baseColorTexture.index) < gltf.textures.size())
                        {
                            mesh.texture = pbr.baseColorTexture.index;
                            if (pbr.baseColorTexture.texCoord != 0)
                            {
                                result.warnings.push_back(where + ": only TEXCOORD_0 is supported");
                            }
                        }
                    }

                    if (const std::string error = core::ValidateMesh(mesh.data); !error.empty())
                    {
                        result.errors.push_back(where + ": " + error);
                        continue;
                    }
                    primitivesOfMesh[m].push_back(static_cast<std::uint32_t>(result.model.meshes.size()));
                    result.model.meshes.push_back(std::move(mesh));
                }
            }
            return primitivesOfMesh;
        }

        ModelNode ConvertNode(const tinygltf::Node &node)
        {
            ModelNode out;
            out.name = node.name;
            if (node.matrix.size() == 16)
            {
                // Split a full matrix back into translation, rotation and scale.
                glm::mat4 matrix{1.0F};
                for (int i = 0; i < 16; ++i)
                {
                    matrix[i / 4][i % 4] = static_cast<float>(node.matrix[static_cast<std::size_t>(i)]);
                }
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(matrix, out.scale, out.rotation, out.position, skew, perspective);
                return out;
            }
            if (node.translation.size() == 3)
                out.position = {node.translation[0], node.translation[1], node.translation[2]};
            if (node.rotation.size() == 4) // glTF stores x, y, z, w; glm::quat takes w first
                out.rotation = glm::quat(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                                         static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
            if (node.scale.size() == 3)
                out.scale = {node.scale[0], node.scale[1], node.scale[2]};
            return out;
        }

        void ImportNodes(const tinygltf::Model &gltf, const std::vector<std::vector<std::uint32_t>> &primitivesOfMesh, ImportResult &result)
        {
            // 1. Find each node's parent; a node with two parents is invalid glTF.
            const std::size_t count = gltf.nodes.size();
            std::vector<int> parent(count, -1);
            for (std::size_t n = 0; n < count; ++n)
            {
                for (const int child : gltf.nodes[n].children)
                {
                    if (child < 0 || static_cast<std::size_t>(child) >= count || parent[static_cast<std::size_t>(child)] != -1 ||
                        static_cast<std::size_t>(child) == n)
                    {
                        result.errors.push_back("node " + std::to_string(n) + " has an invalid or shared child " + std::to_string(child));
                        return;
                    }
                    parent[static_cast<std::size_t>(child)] = static_cast<int>(n);
                }
            }

            // 2. Visit roots depth-first so parents are emitted before children.
            std::vector<int> newIndex(count, -1);
            const std::function<void(std::size_t, int)> visit = [&](std::size_t n, int newParent) {
                ModelNode node = ConvertNode(gltf.nodes[n]);
                node.parent = newParent;
                const int mesh = gltf.nodes[n].mesh;
                if (mesh >= 0 && static_cast<std::size_t>(mesh) < primitivesOfMesh.size())
                    node.meshes = primitivesOfMesh[static_cast<std::size_t>(mesh)];
                if (gltf.nodes[n].skin >= 0)
                    result.warnings.push_back("node " + std::to_string(n) + ": skinning ignored");
                newIndex[n] = static_cast<int>(result.model.nodes.size());
                result.model.nodes.push_back(std::move(node));
                for (const int child : gltf.nodes[n].children)
                    visit(static_cast<std::size_t>(child), newIndex[n]);
            };
            for (std::size_t n = 0; n < count; ++n)
            {
                if (parent[n] == -1)
                    visit(n, -1);
            }

            // 3. Anything not reached from a root sits in a cycle.
            for (std::size_t n = 0; n < count; ++n)
            {
                if (newIndex[n] == -1)
                {
                    result.errors.push_back("node " + std::to_string(n) + " is part of a cycle");
                    return;
                }
            }
        }
    }

    ImportResult ImportGltf(const std::filesystem::path &path)
    {
        ImportResult result;
        tinygltf::TinyGLTF loader;
        tinygltf::Model gltf;
        std::string error;
        std::string warning;
        bool loaded = false;
        try
        {
            loaded = path.extension() == ".glb" ? loader.LoadBinaryFromFile(&gltf, &error, &warning, path.string())
                                                : loader.LoadASCIIFromFile(&gltf, &error, &warning, path.string());
        }
        catch (const std::exception &exception)
        {
            error = exception.what();
        }
        if (!warning.empty())
            result.warnings.push_back(warning);
        if (!loaded)
        {
            result.errors.push_back(error.empty() ? "could not read glTF file" : error);
            return result;
        }

        // External files this source depends on (for change tracking).
        const auto addDependency = [&](const std::string &uri) {
            if (uri.empty() || uri.rfind("data:", 0) == 0)
                return;
            if (!IsSafeRelativePath(uri))
                result.errors.push_back("unsafe external path '" + uri + "' (must be relative, without '..')");
            else
                result.dependencies.push_back(uri);
        };
        for (const tinygltf::Buffer &buffer : gltf.buffers)
            addDependency(buffer.uri);
        for (const tinygltf::Image &image : gltf.images)
            addDependency(image.uri);

        for (const std::string &extension : gltf.extensionsRequired)
            result.errors.push_back("requires unsupported extension " + extension);
        if (!gltf.animations.empty())
            result.warnings.push_back("animations ignored");

        ImportTextures(gltf, result);
        const auto primitivesOfMesh = ImportMeshes(gltf, result);
        ImportNodes(gltf, primitivesOfMesh, result);
        return result;
    }
}
