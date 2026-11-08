#pragma once

#include <AssetFolders.hpp>
#include <Diagnostic.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

// UmbraTool commands (README.md). The core commands live in Main.cpp; plugins add theirs from
// their tool/ folder with UMBRA_TOOL_COMMAND, so the tool never names a plugin.
namespace umbra::tool
{
    // The command line after the command name: words, "--key value" options and two flags.
    struct Arguments final
    {
        std::vector<std::string> positional;
        std::map<std::string, std::string> options; // "--npc elder" -> {"npc", "elder"}
        bool json = false;                          // --json
        bool apply = false;                         // --apply

        [[nodiscard]] bool Has(const std::string &key) const { return options.contains(key); }
        [[nodiscard]] std::string Text(const std::string &key, const std::string &fallback = {}) const;
        [[nodiscard]] std::filesystem::path Path(const std::string &key) const { return Text(key); }
        // Numbers: a bad value throws std::invalid_argument (the tool then prints its usage).
        [[nodiscard]] int Int(const std::string &key, int fallback) const;
        [[nodiscard]] long long Long(const std::string &key, long long fallback) const;
        [[nodiscard]] float Float(const std::string &key, float fallback) const;
        // --assets (or `folder`) with the enabled plugins' assets underneath; empty without one.
        [[nodiscard]] assets::AssetFolders Assets() const { return AssetsOf(Path("assets")); }
        [[nodiscard]] static assets::AssetFolders AssetsOf(const std::filesystem::path &folder);
    };

    using CommandFunction = std::function<int(const Arguments &)>;
    struct Command final
    {
        std::string name;
        std::string usage; // one or two lines after "UmbraTool "
        CommandFunction run;
    };

    bool RegisterCommand(Command command);
    [[nodiscard]] const std::vector<Command> &Commands();

    // Prints the usage of every command; returns 2 (bad usage).
    int Usage();
    // Prints diagnostics as text lines or JSON; returns the exit code (1 = errors).
    int Report(const std::vector<core::Diagnostic> &diagnostics, std::size_t files, bool json);
}

// In a plugin's tool/*.cpp: UMBRA_TOOL_COMMAND(Fight, "fight", "fight <rpg file> <hero> <enemy>... [--json]", RunFight);
#define UMBRA_TOOL_COMMAND(ID, NAME, USAGE, FUNCTION) \
    static const bool umbraCommand##ID = ::umbra::tool::RegisterCommand({NAME, USAGE, FUNCTION})
