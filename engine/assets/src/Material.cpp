#include <Material.hpp>

#include <ModelCache.hpp>
#include <TextureTools.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace umbra::assets
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        void Add(std::vector<core::Diagnostic> &d, core::Severity severity, const std::string &code, const std::string &file,
                 const std::string &field, const std::string &message)
        {
            d.push_back({severity, code, file, field, message});
        }

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }
    }

    bool ParseMaterial(const std::string &text, const std::string &name, Material &material, std::vector<core::Diagnostic> &diagnostics)
    {
        std::vector<core::Diagnostic> d;
        auto error = [&](const std::string &code, const std::string &field, const std::string &message) {
            Add(d, core::Severity::Error, code, name, field, message);
        };
        const Json j = Json::parse(text, nullptr, false);
        if (!j.is_object())
        {
            error("json.parse", "", "not a JSON object");
            diagnostics.insert(diagnostics.end(), d.begin(), d.end());
            return false;
        }
        static const std::set<std::string> known{"format", "version", "baseColor", "projection", "metersPerRepeat", "tint", "generated"};
        for (const auto &[key, value] : j.items())
            if (!known.contains(key))
                Add(d, core::Severity::Warning, "material.field.unknown", name, key, "unknown field is ignored (typo?)");

        // 1. Header and image.
        Material m;
        if (j.value("format", "") != "umbra.material")
            error("material.format", "format", "must be \"umbra.material\"");
        if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() != materialFormatVersion)
            error("material.version", "version", "must be " + std::to_string(materialFormatVersion));
        if (!j.contains("baseColor") || !j["baseColor"].is_string() || j["baseColor"].get<std::string>().empty())
            error("material.image", "baseColor", "needs the image path, e.g. \"textures/stone.png\"");
        else if (!IsSafeAssetPath(m.baseColor = j["baseColor"].get<std::string>()))
            error("asset.path.unsafe", "baseColor", "'" + m.baseColor + "' must be relative to the asset folder without '..'");

        // 2. How it is laid on surfaces.
        const std::string projection = j.value("projection", "uv");
        if (projection != "uv" && projection != "world")
            error("material.projection", "projection", "must be \"uv\" or \"world\"");
        m.worldProjection = projection == "world";
        if (j.contains("metersPerRepeat"))
        {
            if (!j["metersPerRepeat"].is_number() || j["metersPerRepeat"].get<float>() < 0.05F || j["metersPerRepeat"].get<float>() > 100.0F)
                error("material.range", "metersPerRepeat", "must be a number from 0.05 to 100");
            else
                m.metersPerRepeat = j["metersPerRepeat"].get<float>();
        }
        if (j.contains("tint"))
        {
            const Json &t = j["tint"];
            if (!t.is_array() || t.size() != 4 || !std::all_of(t.begin(), t.end(), [](const Json &v) { return v.is_number() && v.get<float>() >= 0.0F && v.get<float>() <= 10.0F; }))
                error("material.range", "tint", "must be 4 numbers from 0 to 10 (r, g, b, a)");
            else
                m.tint = {t[0].get<float>(), t[1].get<float>(), t[2].get<float>(), t[3].get<float>()};
        }

        // 3. Provenance (optional).
        if (j.contains("generated"))
        {
            const Json &g = j["generated"];
            if (!g.is_object())
                error("material.generated", "generated", "must be an object");
            else
            {
                Generated info;
                info.provider = g.value("provider", "");
                info.model = g.value("model", "");
                info.license = g.value("license", "");
                info.prompt = g.value("prompt", "");
                info.negative = g.value("negative", "");
                info.seed = g.contains("seed") && g["seed"].is_number_integer() ? g["seed"].get<long long>() : 0;
                info.steps = g.contains("steps") && g["steps"].is_number_integer() ? g["steps"].get<int>() : 0;
                info.date = g.value("date", "");
                m.generated = info;
            }
        }

        diagnostics.insert(diagnostics.end(), d.begin(), d.end());
        if (core::HasErrors(d))
            return false;
        material = m;
        return true;
    }

    bool LoadMaterial(const std::filesystem::path &file, Material &material, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            Add(diagnostics, core::Severity::Error, "file.read", file.string(), "", "cannot open file");
            return false;
        }
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        return ParseMaterial(text, file.string(), material, diagnostics);
    }

    bool CheckMaterialImage(const AssetFolders &assetRoot, const Material &material, const std::string &name,
                            std::vector<core::Diagnostic> &diagnostics)
    {
        int width = 0, height = 0;
        const std::filesystem::path file = assetRoot / material.baseColor;
        std::string problem;
        if (!std::filesystem::is_regular_file(file))
            problem = "'" + material.baseColor + "' does not exist in " + assetRoot.string();
        else if (!ImageSize(file, width, height))
            problem = "'" + material.baseColor + "' is not a readable image";
        else if (!GoodTextureSize(width, height))
            problem = "'" + material.baseColor + "' is " + std::to_string(width) + "x" + std::to_string(height) +
                      "; sides must be powers of two from 64 to 2048";
        if (problem.empty())
            return true;
        Add(diagnostics, core::Severity::Error, "material.image", name, "baseColor", problem);
        return false;
    }

    std::string MaterialText(const Material &material)
    {
        Json j{{"format", "umbra.material"}, {"version", materialFormatVersion}, {"baseColor", material.baseColor},
               {"projection", material.worldProjection ? "world" : "uv"}, {"metersPerRepeat", material.metersPerRepeat}};
        if (material.tint != glm::vec4{1.0F})
            j["tint"] = {material.tint.r, material.tint.g, material.tint.b, material.tint.a};
        if (const auto &g = material.generated)
            j["generated"] = {{"provider", g->provider}, {"model", g->model}, {"license", g->license}, {"prompt", g->prompt},
                              {"negative", g->negative}, {"seed", g->seed}, {"steps", g->steps}, {"date", g->date}};
        return j.dump(2) + "\n";
    }

    ModelLicenseInfo ModelLicense(const std::string &checkpoint)
    {
        // Matched on the lower-case file name; first match wins (more specific names first).
        struct Rule
        {
            const char *contains;
            const char *name;
            bool commercial;
        };
        static const Rule rules[] = {
            {"flux1-dev", "FLUX.1 [dev] Non-Commercial License", false},
            {"flux.1-dev", "FLUX.1 [dev] Non-Commercial License", false},
            {"flux2-klein-9b", "FLUX.2 [klein] 9B Non-Commercial License", false},
            {"flux1-schnell", "Apache-2.0", true},
            {"trellis_2", "MIT (TRELLIS.2)", true},
            {"ace_step", "Apache-2.0 (ACE-Step)", true},
            {"acestep", "Apache-2.0 (ACE-Step)", true},
            {"stable_audio", "Stability AI Community License (free below USD 1M yearly revenue)", true},
            {"flux2-klein-4b", "Apache-2.0", true},
            {"z_image", "Apache-2.0", true},
            {"z-image", "Apache-2.0", true},
            {"sd3.5", "Stability AI Community License (free below USD 1M yearly revenue)", true},
            {"sd_xl", "CreativeML OpenRAIL++-M", true},
            {"sdxl", "CreativeML OpenRAIL++-M", true},
            {"v1-5", "CreativeML OpenRAIL-M", true},
            {"sd15", "CreativeML OpenRAIL-M", true},
        };
        const std::string file = Lower(checkpoint);
        for (const Rule &rule : rules)
            if (file.find(rule.contains) != std::string::npos)
                return {rule.name, rule.commercial, true};
        return {"unknown", false, false};
    }

    void CheckMaterialLicense(const Material &material, const std::string &name, std::vector<core::Diagnostic> &diagnostics)
    {
        if (!material.generated)
            return;
        const ModelLicenseInfo info = ModelLicense(material.generated->model);
        if (!info.known)
            Add(diagnostics, core::Severity::Warning, "material.license", name, "generated.model",
                "license of '" + material.generated->model + "' is unknown: check it before shipping this texture");
        else if (!info.commercial)
            Add(diagnostics, core::Severity::Warning, "material.license", name, "generated.model",
                "made with '" + material.generated->model + "' (" + info.name + "): not for a game you sell");
    }
}
