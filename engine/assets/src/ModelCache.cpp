#include <GltfImporter.hpp>
#include <ModelCache.hpp>

#include <cstring>
#include <fstream>
#include <system_error>
#include <type_traits>

namespace umbra::assets
{
    namespace
    {
        constexpr char magic[4] = {'U', 'M', 'D', 'L'};

        // FNV-1a 64-bit: a small, fast, non-cryptographic hash. See AssetPipeline.md.
        struct Fnv1a final
        {
            std::uint64_t value = 14695981039346656037ULL;
            void Add(const void *data, std::size_t size)
            {
                const auto *bytes = static_cast<const std::uint8_t *>(data);
                for (std::size_t i = 0; i < size; ++i)
                {
                    value ^= bytes[i];
                    value *= 1099511628211ULL;
                }
            }
        };

        bool HashFile(const std::filesystem::path &path, Fnv1a &hash)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return false;
            char buffer[64 * 1024];
            while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
                hash.Add(buffer, static_cast<std::size_t>(file.gcount()));
            return true;
        }

        // Appends plain values to a byte array (little-endian on every supported platform).
        struct Writer final
        {
            std::vector<std::uint8_t> bytes;

            template <typename T>
            void Put(const T &value)
            {
                static_assert(std::is_trivially_copyable_v<T>);
                const auto *p = reinterpret_cast<const std::uint8_t *>(&value);
                bytes.insert(bytes.end(), p, p + sizeof(T));
            }
            void PutBytes(const void *data, std::size_t size)
            {
                const auto *p = static_cast<const std::uint8_t *>(data);
                bytes.insert(bytes.end(), p, p + size);
            }
            void PutString(const std::string &text)
            {
                Put(static_cast<std::uint32_t>(text.size()));
                PutBytes(text.data(), text.size());
            }
        };

        // Reads values back; every read checks there are enough bytes left, so a truncated
        // or corrupt file fails cleanly instead of reading past the end or allocating huge arrays.
        struct Reader final
        {
            const std::vector<std::uint8_t> &bytes;
            std::size_t position = 0;
            bool ok = true;

            bool Take(void *out, std::size_t size)
            {
                if (!ok || size > bytes.size() - position)
                    return ok = false;
                if (size > 0) // memcpy with a null pointer is undefined even for 0 bytes
                    std::memcpy(out, bytes.data() + position, size);
                position += size;
                return true;
            }
            template <typename T>
            T Get()
            {
                T value{};
                Take(&value, sizeof(T));
                return value;
            }
            // Reads an element count and checks count * elementSize still fits in the file.
            std::uint32_t Count(std::size_t elementSize)
            {
                const auto count = Get<std::uint32_t>();
                if (ok && std::size_t{count} * elementSize > bytes.size() - position)
                    ok = false;
                return ok ? count : 0;
            }
            std::string String()
            {
                std::string text(Count(1), '\0');
                Take(text.data(), text.size());
                return text;
            }
        };

        void WriteHeader(Writer &w, const BuildInfo &info)
        {
            w.PutBytes(magic, sizeof(magic));
            w.Put(modelFormatVersion);
            w.Put(importerVersion);
            w.Put(info.sourceHash);
            w.Put(static_cast<std::uint32_t>(info.dependencies.size()));
            for (const std::string &dependency : info.dependencies)
                w.PutString(dependency);
        }

        bool ReadHeader(Reader &r, BuildInfo &info, std::string &error)
        {
            char fileMagic[4]{};
            r.Take(fileMagic, sizeof(fileMagic));
            const auto format = r.Get<std::uint32_t>();
            const auto importer = r.Get<std::uint32_t>();
            if (!r.ok || std::memcmp(fileMagic, magic, sizeof(magic)) != 0)
            {
                error = "not a .umodel file";
                return false;
            }
            if (format != modelFormatVersion || importer != importerVersion)
            {
                error = "built by a different format/importer version";
                return false;
            }
            info.sourceHash = r.Get<std::uint64_t>();
            info.dependencies.resize(r.Count(4));
            for (std::string &dependency : info.dependencies)
                dependency = r.String();
            if (!r.ok)
                error = "truncated header";
            return r.ok;
        }

        bool LoadBytes(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes, std::string &error)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
            {
                error = "cannot open " + path.string();
                return false;
            }
            bytes.resize(static_cast<std::size_t>(file.tellg()));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!file)
            {
                error = "cannot read " + path.string();
                return false;
            }
            return true;
        }
    }

    bool IsSafeAssetPath(const std::filesystem::path &path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
            return false;
        for (const auto &part : path)
        {
            if (part == "..")
                return false;
        }
        return true;
    }

    std::uint64_t HashSource(const std::filesystem::path &sourceFile, const std::vector<std::string> &dependencies)
    {
        Fnv1a hash;
        hash.Add(&modelFormatVersion, sizeof(modelFormatVersion));
        hash.Add(&importerVersion, sizeof(importerVersion));
        if (!HashFile(sourceFile, hash))
            return 0;
        for (const std::string &dependency : dependencies)
        {
            hash.Add(dependency.data(), dependency.size());
            if (!HashFile(sourceFile.parent_path() / dependency, hash))
                return 0;
        }
        return hash.value == 0 ? 1 : hash.value; // 0 is reserved for "unreadable"
    }

    bool WriteModelFile(const std::filesystem::path &path, const Model &model, const BuildInfo &info, std::string &error)
    {
        // 1. Serialize everything into memory.
        Writer w;
        WriteHeader(w, info);
        w.Put(static_cast<std::uint32_t>(model.nodes.size()));
        for (const ModelNode &node : model.nodes)
        {
            w.PutString(node.name);
            w.Put(node.parent);
            w.Put(node.position);
            w.Put(node.rotation);
            w.Put(node.scale);
            w.Put(static_cast<std::uint32_t>(node.meshes.size()));
            w.PutBytes(node.meshes.data(), node.meshes.size() * sizeof(std::uint32_t));
        }
        w.Put(static_cast<std::uint32_t>(model.meshes.size()));
        for (const ModelMesh &mesh : model.meshes)
        {
            w.Put(mesh.data.baseColor);
            w.Put(mesh.texture);
            w.Put(static_cast<std::uint32_t>(mesh.data.vertices.size()));
            w.PutBytes(mesh.data.vertices.data(), mesh.data.vertices.size() * sizeof(core::Vertex));
            w.Put(static_cast<std::uint32_t>(mesh.data.indices.size()));
            w.PutBytes(mesh.data.indices.data(), mesh.data.indices.size() * sizeof(std::uint32_t));
        }
        w.Put(static_cast<std::uint32_t>(model.textures.size()));
        for (const core::TextureData &texture : model.textures)
        {
            w.Put(texture.width);
            w.Put(texture.height);
            w.PutBytes(texture.rgba.data(), texture.rgba.size());
        }
        // S8: per-mesh skin, the skeleton, the clips.
        for (const ModelMesh &mesh : model.meshes)
        {
            w.Put(static_cast<std::uint32_t>(mesh.data.skin.size()));
            w.PutBytes(mesh.data.skin.data(), mesh.data.skin.size() * sizeof(core::SkinVertex));
        }
        w.Put(static_cast<std::uint32_t>(model.skeleton ? model.skeleton->joints.size() : 0));
        if (model.skeleton)
            for (const core::Joint &joint : model.skeleton->joints)
            {
                w.PutString(joint.name);
                w.Put(joint.parent);
                w.Put(joint.base);
                w.Put(joint.inverseBind);
                w.Put(joint.rest);
            }
        w.Put(static_cast<std::uint32_t>(model.clips.size()));
        for (const core::Clip &clip : model.clips)
        {
            w.PutString(clip.name);
            w.Put(clip.duration);
            w.Put(static_cast<std::uint32_t>(clip.channels.size()));
            for (const core::ClipChannel &channel : clip.channels)
            {
                w.PutString(channel.joint);
                w.Put(static_cast<std::uint8_t>(channel.path));
                w.Put(static_cast<std::uint8_t>(channel.step));
                w.Put(static_cast<std::uint32_t>(channel.times.size()));
                w.PutBytes(channel.times.data(), channel.times.size() * sizeof(float));
                w.PutBytes(channel.values.data(), channel.values.size() * sizeof(glm::vec4));
            }
        }
        Fnv1a checksum;
        checksum.Add(w.bytes.data(), w.bytes.size());
        w.Put(checksum.value);

        // 2. Write a temp file, then rename it over the real one.
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        const std::filesystem::path temp = path.string() + ".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char *>(w.bytes.data()), static_cast<std::streamsize>(w.bytes.size()));
            if (!file.flush())
            {
                error = "cannot write " + temp.string();
                return false;
            }
        }
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            error = "cannot replace " + path.string() + ": " + ec.message();
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }

    bool ReadModelInfo(const std::filesystem::path &path, BuildInfo &info, std::string &error)
    {
        std::vector<std::uint8_t> bytes;
        if (!LoadBytes(path, bytes, error))
            return false;
        Reader r{bytes};
        return ReadHeader(r, info, error);
    }

    bool ReadModelFile(const std::filesystem::path &path, Model &model, BuildInfo &info, std::string &error)
    {
        std::vector<std::uint8_t> bytes;
        if (!LoadBytes(path, bytes, error))
            return false;

        // 1. Checksum over everything except the last 8 bytes.
        std::uint64_t stored = 0;
        if (bytes.size() < sizeof(stored))
        {
            error = "file too small";
            return false;
        }
        std::memcpy(&stored, bytes.data() + bytes.size() - sizeof(stored), sizeof(stored));
        Fnv1a checksum;
        checksum.Add(bytes.data(), bytes.size() - sizeof(stored));
        if (checksum.value != stored)
        {
            error = "checksum mismatch (corrupt or partial file)";
            return false;
        }
        bytes.resize(bytes.size() - sizeof(stored));

        // 2. Header, nodes, meshes, textures.
        Reader r{bytes};
        if (!ReadHeader(r, info, error))
            return false;
        Model out;
        out.nodes.resize(r.Count(4));
        for (ModelNode &node : out.nodes)
        {
            node.name = r.String();
            node.parent = r.Get<std::int32_t>();
            node.position = r.Get<glm::vec3>();
            node.rotation = r.Get<glm::quat>();
            node.scale = r.Get<glm::vec3>();
            node.meshes.resize(r.Count(sizeof(std::uint32_t)));
            r.Take(node.meshes.data(), node.meshes.size() * sizeof(std::uint32_t));
        }
        out.meshes.resize(r.Count(4));
        for (ModelMesh &mesh : out.meshes)
        {
            mesh.data.baseColor = r.Get<glm::vec4>();
            mesh.texture = r.Get<std::int32_t>();
            mesh.data.vertices.resize(r.Count(sizeof(core::Vertex)));
            r.Take(mesh.data.vertices.data(), mesh.data.vertices.size() * sizeof(core::Vertex));
            mesh.data.indices.resize(r.Count(sizeof(std::uint32_t)));
            r.Take(mesh.data.indices.data(), mesh.data.indices.size() * sizeof(std::uint32_t));
        }
        out.textures.resize(r.Count(8));
        for (core::TextureData &texture : out.textures)
        {
            texture.width = r.Get<std::uint32_t>();
            texture.height = r.Get<std::uint32_t>();
            const std::size_t size = std::size_t{texture.width} * texture.height * 4;
            if (!r.ok || size > bytes.size() - r.position)
            {
                r.ok = false;
                break;
            }
            texture.rgba.resize(size);
            r.Take(texture.rgba.data(), size);
        }
        for (ModelMesh &mesh : out.meshes)
        {
            mesh.data.skin.resize(r.Count(sizeof(core::SkinVertex)));
            r.Take(mesh.data.skin.data(), mesh.data.skin.size() * sizeof(core::SkinVertex));
        }
        if (const std::uint32_t joints = r.Count(4); joints > 0)
        {
            core::Skeleton skeleton;
            skeleton.joints.resize(joints);
            for (core::Joint &joint : skeleton.joints)
            {
                joint.name = r.String();
                joint.parent = r.Get<std::int32_t>();
                joint.base = r.Get<glm::mat4>();
                joint.inverseBind = r.Get<glm::mat4>();
                joint.rest = r.Get<core::JointPose>();
            }
            if (!core::SortJoints(skeleton) || !core::ValidateSkeleton(skeleton).empty())
                r.ok = false;
            out.skeleton = std::move(skeleton);
        }
        out.clips.resize(r.Count(4));
        for (core::Clip &clip : out.clips)
        {
            clip.name = r.String();
            clip.duration = r.Get<float>();
            clip.channels.resize(r.Count(4));
            for (core::ClipChannel &channel : clip.channels)
            {
                channel.joint = r.String();
                channel.path = static_cast<core::ClipChannel::Path>(std::min<std::uint8_t>(r.Get<std::uint8_t>(), 2));
                channel.step = r.Get<std::uint8_t>() != 0;
                const std::uint32_t keys = r.Count(sizeof(float) + sizeof(glm::vec4));
                channel.times.resize(keys);
                channel.values.resize(keys);
                r.Take(channel.times.data(), keys * sizeof(float));
                r.Take(channel.values.data(), keys * sizeof(glm::vec4));
            }
        }
        if (!r.ok || r.position != bytes.size())
        {
            error = "malformed model data";
            return false;
        }
        model = std::move(out);
        return true;
    }

    std::filesystem::path CachedModelPath(const std::filesystem::path &source, const std::filesystem::path &cacheRoot)
    {
        return cacheRoot / (source.string() + ".umodel");
    }

    BuildResult BuildModel(const std::filesystem::path &assetRoot, const std::filesystem::path &source,
                           const std::filesystem::path &cacheRoot)
    {
        BuildResult result;
        if (!IsSafeAssetPath(source))
        {
            result.errors.push_back("asset path '" + source.string() + "' must be relative and stay inside the asset folder");
            return result;
        }
        const std::filesystem::path sourceFile = assetRoot / source;
        result.output = CachedModelPath(source, cacheRoot);

        // 1. Up to date if the cached hash still matches source + recorded dependencies.
        BuildInfo cached;
        std::string ignored;
        if (ReadModelInfo(result.output, cached, ignored))
        {
            const std::uint64_t hash = HashSource(sourceFile, cached.dependencies);
            if (hash != 0 && hash == cached.sourceHash)
            {
                result.status = BuildStatus::UpToDate;
                return result;
            }
        }

        // 2. Import. Any error leaves the old cached file alone.
        ImportResult imported = ImportGltf(sourceFile);
        result.warnings = std::move(imported.warnings);
        result.errors = std::move(imported.errors);
        if (!result.errors.empty())
            return result;

        // 3. Hash with the dependencies the importer found, then write.
        BuildInfo info{HashSource(sourceFile, imported.dependencies), imported.dependencies};
        std::string error;
        if (info.sourceHash == 0)
            error = "a dependency of " + source.string() + " cannot be read";
        else if (WriteModelFile(result.output, imported.model, info, error))
        {
            result.status = BuildStatus::Built;
            return result;
        }
        result.errors.push_back(error);
        return result;
    }
}
