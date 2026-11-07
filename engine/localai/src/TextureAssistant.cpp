#include <TextureAssistant.hpp>

#include <TextureTools.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <random>

namespace umbra::localai
{
    namespace
    {
        // Write to "<file>.tmp" then rename, so a crash never leaves half a file under the real name.
        bool WriteFileAtomic(const std::filesystem::path &file, const std::string &bytes)
        {
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            const std::filesystem::path temporary = file.string() + ".tmp";
            {
                std::ofstream out(temporary, std::ios::binary);
                if (!out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
                    return false;
            }
            std::filesystem::rename(temporary, file, error);
            return !error;
        }

        std::string Today()
        {
            const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char text[16];
            std::strftime(text, sizeof(text), "%Y-%m-%d", std::localtime(&now));
            return text;
        }
    }

    TextureStyle LoadTextureStyle(const std::filesystem::path &assetRoot)
    {
        TextureStyle style;
        std::ifstream in(assetRoot / "materials/style.json");
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_object())
        {
            style.prompt = j.value("prompt", style.prompt);
            style.negative = j.value("negative", style.negative);
        }
        return style;
    }

    std::string PreferredCheckpoint(const std::vector<std::string> &installed, const std::string &wanted)
    {
        for (const std::string &name : installed)
            if (name == wanted)
                return name;
        for (const std::string &name : installed)
            if (name.find("sd_xl") != std::string::npos || name.find("sdxl") != std::string::npos)
                return name;
        return installed.empty() ? "" : installed.front();
    }

    std::vector<long long> TextureSeeds(int count, long long first)
    {
        std::random_device device;
        std::vector<long long> seeds;
        for (int i = 0; i < count; ++i)
            seeds.push_back(first >= 0 ? first + i : static_cast<long long>(device() & 0x7fffffff));
        return seeds;
    }

    std::vector<ImageRequest> MakeTextureRequests(const std::string &idea, const TextureStyle &style, const std::string &model,
                                                  const std::string &host, const std::vector<long long> &seeds)
    {
        std::vector<ImageRequest> requests;
        for (const long long seed : seeds)
        {
            ImageRequest request;
            request.host = host;
            request.model = model;
            request.prompt = idea + ", " + style.prompt;
            request.negative = style.negative;
            request.seed = seed;
            requests.push_back(request);
        }
        return requests;
    }

    TextureCandidate FinishTexture(const core::TextureData &raw, const ImageRequest &request, std::uint32_t size)
    {
        TextureCandidate candidate;
        candidate.request = request;
        candidate.image = raw.width == size && raw.height == size ? raw : assets::Resize(raw, size, size);
        candidate.seamBefore = assets::SeamScore(candidate.image);
        assets::MakeSeamless(candidate.image);
        candidate.seam = assets::SeamScore(candidate.image);
        return candidate;
    }

    std::string TextureName(const std::string &text)
    {
        std::string name;
        for (char c : text)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                name += c;
            else if (!name.empty() && name.back() != '_')
                name += '_';
        }
        while (!name.empty() && name.back() == '_')
            name.pop_back();
        name = name.substr(0, 32);
        return name.empty() ? "texture" : name;
    }

    std::string SaveTextureMaterial(const std::filesystem::path &assetRoot, const std::string &name, const TextureCandidate &candidate,
                                    float metersPerRepeat, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. A name that is free for both files.
        const std::string base = TextureName(name);
        std::string free = base;
        for (int n = 2; std::filesystem::exists(assetRoot / ("textures/" + free + ".png")) ||
                        std::filesystem::exists(assetRoot / ("materials/" + free + ".material.json"));
             ++n)
            free = base + "_" + std::to_string(n);

        // 2. The material with its provenance (QuestDraft-style: how to make it again, and its license).
        assets::Material material;
        material.baseColor = "textures/" + free + ".png";
        material.worldProjection = true;
        material.metersPerRepeat = metersPerRepeat;
        const ImageRequest &r = candidate.request;
        material.generated = assets::Generated{"comfyui", r.model, assets::ModelLicense(r.model).name, r.prompt, r.negative, r.seed, r.steps, Today()};

        // 3. Image first, then the material that points at it.
        std::vector<std::uint8_t> png;
        const std::string materialPath = "materials/" + free + ".material.json";
        if (!assets::EncodePng(candidate.image, png) || !WriteFileAtomic(assetRoot / material.baseColor, std::string(png.begin(), png.end())) ||
            !WriteFileAtomic(assetRoot / materialPath, assets::MaterialText(material)))
        {
            diagnostics.push_back({core::Severity::Error, "save.write", (assetRoot / materialPath).string(), "", "couldn't write the texture or material file"});
            return "";
        }
        return materialPath;
    }
}
