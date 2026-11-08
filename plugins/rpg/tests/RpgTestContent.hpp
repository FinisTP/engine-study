#pragma once

#include <Character.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace rpgtest
{
    inline const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};

    // The shipped reference content (assets/rpg/reference.rpg.json).
    inline const umbra::rpg::Content &Reference()
    {
        static const umbra::rpg::Content content = [] {
            umbra::rpg::Content c;
            std::vector<umbra::core::Diagnostic> diagnostics;
            REQUIRE(umbra::rpg::LoadContent(assetDir / "rpg" / "reference.rpg.json", c, diagnostics));
            return c;
        }();
        return content;
    }

    inline umbra::rpg::Character Make(const std::string &actor, const std::string &id)
    {
        auto character = umbra::rpg::MakeCharacter(Reference(), actor, id);
        REQUIRE(character);
        return *character;
    }

    // Parses `sections` (the inside of the root object) and reports the codes found.
    struct Parsed
    {
        bool ok = false;
        umbra::rpg::Content content;
        std::vector<umbra::core::Diagnostic> diagnostics;

        [[nodiscard]] bool Has(const std::string &code) const
        {
            for (const auto &d : diagnostics)
            {
                if (d.code == code)
                    return true;
            }
            return false;
        }
    };

    inline Parsed Parse(const std::string &sections)
    {
        Parsed p;
        p.ok = umbra::rpg::ParseContent(R"({"format":"umbra.rpg","version":1,)" + sections + "}", "test.rpg.json", p.content, p.diagnostics);
        return p;
    }
}
