#include <GltfImporter.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <algorithm>
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

        // Reads one VEC4 attribute as floats (any component type; integer ones are normalized
        // when `normalized`, else taken as whole numbers). False if present but malformed.
        bool ReadVec4(const tinygltf::Model &model, const tinygltf::Primitive &primitive, const char *name, bool normalized,
                      std::size_t count, std::vector<glm::vec4> &out, bool &present)
        {
            present = false;
            const auto it = primitive.attributes.find(name);
            if (it == primitive.attributes.end())
                return true;
            if (it->second < 0 || static_cast<std::size_t>(it->second) >= model.accessors.size())
                return false;
            const tinygltf::Accessor &accessor = model.accessors[static_cast<std::size_t>(it->second)];
            const std::size_t size = accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ? 4 : ComponentSize(accessor.componentType);
            std::size_t stride = 0;
            const unsigned char *data = size ? AccessorData(model, accessor, size * 4, stride) : nullptr;
            if (data == nullptr || accessor.type != TINYGLTF_TYPE_VEC4 || accessor.count != count)
                return false;
            out.resize(count);
            const float scale = normalized && size < 4 ? 1.0F / static_cast<float>((1u << (8 * size)) - 1) : 1.0F;
            for (std::size_t i = 0; i < count; ++i)
                for (int k = 0; k < 4; ++k)
                {
                    const unsigned char *p = data + i * stride + static_cast<std::size_t>(k) * size;
                    if (size == 4 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                        std::memcpy(&out[i][k], p, 4);
                    else
                        out[i][k] = static_cast<float>(ReadIndex(p, size)) * scale;
                }
            present = true;
            return true;
        }

        bool ReadSkin(const tinygltf::Model &gltf, const tinygltf::Primitive &primitive, core::MeshData &mesh, const std::string &where,
                      ImportResult &result)
        {
            std::vector<glm::vec4> joints, weights;
            bool hasJoints = false, hasWeights = false;
            if (!ReadVec4(gltf, primitive, "JOINTS_0", false, mesh.vertices.size(), joints, hasJoints) ||
                !ReadVec4(gltf, primitive, "WEIGHTS_0", true, mesh.vertices.size(), weights, hasWeights) || hasJoints != hasWeights)
            {
                result.errors.push_back(where + ": malformed JOINTS_0/WEIGHTS_0");
                return false;
            }
            if (!hasJoints)
                return true;
            mesh.skin.resize(mesh.vertices.size());
            for (std::size_t i = 0; i < mesh.skin.size(); ++i)
            {
                // Weights are made to add up to 1 (exporters round them).
                const float sum = weights[i].x + weights[i].y + weights[i].z + weights[i].w;
                mesh.skin[i].weights = sum > 0.0F ? weights[i] / sum : glm::vec4(1.0F, 0.0F, 0.0F, 0.0F);
                for (int k = 0; k < 4; ++k)
                    mesh.skin[i].joints[k] = static_cast<std::uint16_t>(joints[i][k]);
            }
            return true;
        }

        // A node's local matrix (T * R * S).
        glm::mat4 LocalMatrix(const ModelNode &node)
        {
            return glm::translate(glm::mat4(1.0F), node.position) * glm::mat4_cast(node.rotation) * glm::scale(glm::mat4(1.0F), node.scale);
        }

        // The first skin as a skeleton, and every animation as a clip over joint names (Skeleton.md).
        void ImportSkinAndClips(const tinygltf::Model &gltf, ImportResult &result)
        {
            if (gltf.skins.size() > 1)
                result.warnings.push_back("only the first skin is used");
            std::vector<int> parent(gltf.nodes.size(), -1);
            for (std::size_t n = 0; n < gltf.nodes.size(); ++n)
                for (const int child : gltf.nodes[n].children)
                    if (child >= 0 && static_cast<std::size_t>(child) < parent.size())
                        parent[static_cast<std::size_t>(child)] = static_cast<int>(n);
            if (!gltf.skins.empty())
            {
                const tinygltf::Skin &skin = gltf.skins[0];
                core::Skeleton skeleton;
                std::vector<int> jointOf(gltf.nodes.size(), -1);
                for (std::size_t j = 0; j < skin.joints.size(); ++j)
                    if (skin.joints[j] >= 0 && static_cast<std::size_t>(skin.joints[j]) < gltf.nodes.size())
                        jointOf[static_cast<std::size_t>(skin.joints[j])] = static_cast<int>(j);
                // Inverse bind matrices (identity when the file has none).
                std::vector<glm::mat4> inverseBind(skin.joints.size(), glm::mat4(1.0F));
                if (skin.inverseBindMatrices >= 0 && static_cast<std::size_t>(skin.inverseBindMatrices) < gltf.accessors.size())
                {
                    const tinygltf::Accessor &accessor = gltf.accessors[static_cast<std::size_t>(skin.inverseBindMatrices)];
                    std::size_t stride = 0;
                    const unsigned char *data = AccessorData(gltf, accessor, sizeof(glm::mat4), stride);
                    if (data == nullptr || accessor.type != TINYGLTF_TYPE_MAT4 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
                        accessor.count != skin.joints.size())
                    {
                        result.errors.push_back("skin: malformed inverse bind matrices");
                        return;
                    }
                    for (std::size_t j = 0; j < inverseBind.size(); ++j)
                        std::memcpy(&inverseBind[j], data + j * stride, sizeof(glm::mat4)); // both column-major
                }
                for (std::size_t j = 0; j < skin.joints.size(); ++j)
                {
                    const int node = skin.joints[j];
                    if (node < 0 || static_cast<std::size_t>(node) >= gltf.nodes.size())
                    {
                        result.errors.push_back("skin: joint " + std::to_string(j) + " is not a node");
                        return;
                    }
                    core::Joint joint;
                    const ModelNode converted = ConvertNode(gltf.nodes[static_cast<std::size_t>(node)]);
                    joint.name = converted.name.empty() ? "joint" + std::to_string(j) : converted.name;
                    joint.rest = {converted.position, converted.rotation, converted.scale};
                    joint.inverseBind = inverseBind[j];
                    // 1. Walk up: fixed nodes go into `base` until the nearest ancestor joint.
                    for (int up = parent[static_cast<std::size_t>(node)]; up >= 0; up = parent[static_cast<std::size_t>(up)])
                    {
                        if (jointOf[static_cast<std::size_t>(up)] >= 0)
                        {
                            joint.parent = jointOf[static_cast<std::size_t>(up)];
                            break;
                        }
                        joint.base = LocalMatrix(ConvertNode(gltf.nodes[static_cast<std::size_t>(up)])) * joint.base;
                    }
                    skeleton.joints.push_back(std::move(joint));
                }
                core::SortJoints(skeleton);
                if (const std::string error = core::ValidateSkeleton(skeleton); !error.empty())
                {
                    result.errors.push_back("skin: " + error);
                    return;
                }
                result.model.skeleton = std::move(skeleton);
            }

            // 2. Clips: translation/rotation/scale channels of named nodes (morph weights are ignored).
            for (std::size_t a = 0; a < gltf.animations.size(); ++a)
            {
                const tinygltf::Animation &animation = gltf.animations[a];
                core::Clip clip;
                clip.name = animation.name.empty() ? "clip" + std::to_string(a) : animation.name;
                for (const tinygltf::AnimationChannel &channel : animation.channels)
                {
                    if (channel.target_node < 0 || static_cast<std::size_t>(channel.target_node) >= gltf.nodes.size() ||
                        channel.sampler < 0 || static_cast<std::size_t>(channel.sampler) >= animation.samplers.size() ||
                        channel.target_path == "weights")
                        continue;
                    const tinygltf::AnimationSampler &sampler = animation.samplers[static_cast<std::size_t>(channel.sampler)];
                    core::ClipChannel out;
                    out.joint = gltf.nodes[static_cast<std::size_t>(channel.target_node)].name;
                    out.path = channel.target_path == "translation" ? core::ClipChannel::Path::Position
                               : channel.target_path == "scale"     ? core::ClipChannel::Path::Scale
                                                                    : core::ClipChannel::Path::Rotation;
                    out.step = sampler.interpolation == "STEP";
                    const bool cubic = sampler.interpolation == "CUBICSPLINE";
                    const int width = out.path == core::ClipChannel::Path::Rotation ? 4 : 3;
                    if (sampler.input < 0 || sampler.output < 0 || static_cast<std::size_t>(sampler.input) >= gltf.accessors.size() ||
                        static_cast<std::size_t>(sampler.output) >= gltf.accessors.size())
                        continue;
                    const tinygltf::Accessor &in = gltf.accessors[static_cast<std::size_t>(sampler.input)];
                    const tinygltf::Accessor &value = gltf.accessors[static_cast<std::size_t>(sampler.output)];
                    std::size_t inStride = 0, valueStride = 0;
                    const unsigned char *times = AccessorData(gltf, in, 4, inStride);
                    const unsigned char *values = AccessorData(gltf, value, static_cast<std::size_t>(width) * 4, valueStride);
                    const std::size_t keys = in.count;
                    if (times == nullptr || values == nullptr || in.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
                        value.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || value.count != keys * (cubic ? 3 : 1))
                    {
                        result.warnings.push_back("clip '" + clip.name + "': a malformed channel was skipped");
                        continue;
                    }
                    out.times.resize(keys);
                    out.values.resize(keys, glm::vec4(0.0F));
                    for (std::size_t k = 0; k < keys; ++k)
                    {
                        std::memcpy(&out.times[k], times + k * inStride, 4);
                        // Cubic splines keep only the key values (in-tangent, value, out-tangent): played linearly.
                        std::memcpy(&out.values[k], values + (cubic ? 3 * k + 1 : k) * valueStride, static_cast<std::size_t>(width) * 4);
                    }
                    clip.duration = std::max(clip.duration, out.times.back());
                    clip.channels.push_back(std::move(out));
                }
                result.model.clips.push_back(std::move(clip));
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

                    // 4. Skin: which joints move each vertex (JOINTS_0 + WEIGHTS_0), S8.
                    if (!ReadSkin(gltf, primitive, mesh.data, where, result))
                        continue;

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
        ImportTextures(gltf, result);
        const auto primitivesOfMesh = ImportMeshes(gltf, result);
        ImportNodes(gltf, primitivesOfMesh, result);
        ImportSkinAndClips(gltf, result);

        // A skinned mesh must only name joints the skeleton has.
        for (const ModelMesh &mesh : result.model.meshes)
            for (const core::SkinVertex &v : mesh.data.skin)
                for (const std::uint16_t j : v.joints)
                    if (!result.model.skeleton || j >= result.model.skeleton->joints.size())
                    {
                        result.errors.push_back("a skinned mesh names joint " + std::to_string(j) + ", which the skin does not have");
                        return result;
                    }
        return result;
    }
}
