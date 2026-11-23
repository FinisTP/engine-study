#include <Play.hpp>

namespace umbra::authoring
{
    std::vector<PlayStart> LoadPlayStarts(const nlohmann::json &project, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Each entry of "play" with a label and a program; arguments are text.
        std::vector<PlayStart> starts;
        const nlohmann::json play = project.is_object() ? project.value("play", nlohmann::json::array()) : nlohmann::json::array();
        for (std::size_t i = 0; i < play.size(); ++i)
        {
            const nlohmann::json &p = play[i];
            const std::string at = "play[" + std::to_string(i) + "]";
            if (!p.is_object() || !p.contains("program") || !p["program"].is_string())
            {
                diagnostics.push_back({core::Severity::Error, "project.play", "project.json", at + ".program", "a start needs \"program\": a program the build makes, e.g. \"Umbra\""});
                continue;
            }
            PlayStart start{p.value("label", p["program"].get<std::string>()), p["program"].get<std::string>(), {}};
            for (const nlohmann::json &a : p.value("args", nlohmann::json::array()))
                start.args.push_back(a.is_string() ? a.get<std::string>() : a.dump());
            starts.push_back(std::move(start));
        }
        // 2. None: the game on the open scene, as before.
        if (starts.empty())
            starts.push_back({"Play this scene", "Umbra", {"{scene}"}});
        return starts;
    }

    std::vector<std::string> PlayCommand(const PlayStart &start, const nlohmann::json &programs, const std::string &scene)
    {
        if (!programs.is_object() || !programs.contains(start.program) || !programs[start.program].is_string())
            return {};
        std::vector<std::string> command{programs[start.program].get<std::string>()};
        for (const std::string &a : start.args)
            command.push_back(a == "{scene}" ? scene : a);
        return command;
    }
}
