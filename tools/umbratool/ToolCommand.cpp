#include "ToolCommand.hpp"

#include <Validate.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace umbra::tool
{
    std::string Arguments::Text(const std::string &key, const std::string &fallback) const
    {
        const auto it = options.find(key);
        return it == options.end() ? fallback : it->second;
    }

    int Arguments::Int(const std::string &key, int fallback) const
    {
        return Has(key) ? std::stoi(Text(key)) : fallback;
    }

    long long Arguments::Long(const std::string &key, long long fallback) const
    {
        return Has(key) ? std::stoll(Text(key)) : fallback;
    }

    float Arguments::Float(const std::string &key, float fallback) const
    {
        return Has(key) ? std::stof(Text(key)) : fallback;
    }

    assets::AssetFolders Arguments::AssetsOf(const std::filesystem::path &folder)
    {
        if (folder.empty())
            return {};
        assets::AssetFolders folders(folder);
        folders.AddPlugins(UMBRA_PLUGIN_ASSET_DIRS);
        return folders;
    }

    namespace
    {
        std::vector<Command> &Registry()
        {
            static std::vector<Command> commands;
            return commands;
        }
    }

    bool RegisterCommand(Command command)
    {
        Registry().push_back(std::move(command));
        return true;
    }

    const std::vector<Command> &Commands()
    {
        return Registry();
    }

    int Usage()
    {
        std::vector<Command> sorted = Commands();
        std::sort(sorted.begin(), sorted.end(), [](const Command &a, const Command &b) { return a.name < b.name; });
        std::cerr << "usage:\n";
        for (const Command &command : sorted)
            std::cerr << "  UmbraTool " << command.usage << "\n";
        return 2;
    }

    int Report(const std::vector<core::Diagnostic> &diagnostics, std::size_t files, bool json)
    {
        int errors = 0, warnings = 0;
        for (const core::Diagnostic &d : diagnostics)
            (d.severity == core::Severity::Error ? errors : warnings)++;
        if (json)
            std::cout << nlohmann::json{{"files", files}, {"errors", errors}, {"warnings", warnings}, {"diagnostics", authoring::ToJson(diagnostics)}}.dump(2) << "\n";
        else
        {
            for (const core::Diagnostic &d : diagnostics)
                std::cout << core::Format(d) << "\n";
            std::cout << files << " file(s): " << errors << " error(s), " << warnings << " warning(s)\n";
        }
        return errors > 0 ? 1 : 0;
    }
}
