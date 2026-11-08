#include "Mcp.hpp"

#include <algorithm>
#include <array>
#include <istream>
#include <ostream>

namespace umbra::agent
{
    namespace
    {
        // MCP revisions this server speaks; it answers with the client's if known, else the newest.
        const std::array<const char *, 4> versions{"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};

        // Text that isn't valid UTF-8 (a file name) must not stop the answer.
        std::string Dump(const Json &json) { return json.dump(-1, ' ', false, Json::error_handler_t::replace); }

        Json Reply(const Json &id, Json result) { return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}; }
        Json Error(const Json &id, int code, const std::string &message)
        {
            return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
        }
        Json Text(const std::string &text, bool error) { return {{"content", {{{"type", "text"}, {"text", text}}}}, {"isError", error}}; }

        Json CommandTools()
        {
            return Json::array({
                {{"name", "build"}, {"description", "Build the engine, editor, game and tests (cmake --build). Run after changing C++."}, {"inputSchema", {{"type", "object"}, {"properties", Json::object()}}}},
                {{"name", "run_tests"},
                 {"description", "Run the tests (ctest); optional regex filter."},
                 {"inputSchema", {{"type", "object"}, {"properties", {{"filter", {{"type", "string"}, {"description", "ctest -R regex"}}}}}}}},
            });
        }

        // One tools/call: check, run on fresh working copies, commit and save.
        Json CallTool(FileWorkspace &files, const McpOptions &options, const std::string &name, const Json &args)
        {
            if ((name == "build" || name == "run_tests") && options.command)
            {
                const auto [ok, output] = options.command(name, args);
                return Text(output, !ok);
            }
            const Tool *tool = FindTool(name);
            if (!tool)
                return Text("no tool named " + name, true);
            if (const std::string problem = CheckArguments(*tool, args); !problem.empty())
                return Text(name + ": " + problem, true);
            files.Refresh(); // the editor may have saved meanwhile
            Edits edits;
            ToolResult result = tool->run(files.Get(), edits, args);
            std::vector<authoring::Document *> changed;
            if (result.ok && !edits.Empty() && (!edits.Commit("MCP: " + name, changed, result.diagnostics) || !files.Save(result.diagnostics)))
            {
                result.ok = false;
                result.text = "checked, but not saved";
            }
            std::string text = result.text;
            for (const core::Diagnostic &d : result.diagnostics)
                text += "\n" + std::string(d.severity == core::Severity::Error ? "error " : "note ") + d.code + " " + d.file + " " + d.field + ": " + d.message;
            if (result.ok && !changed.empty())
                text += "\n(saved; an open editor reloads the scene if it has no unsaved changes)";
            return Text(text, !result.ok);
        }
    }

    std::string McpAnswer(FileWorkspace &files, const McpOptions &options, const std::string &message)
    {
        const Json request = Json::parse(message, nullptr, false);
        if (request.is_discarded() || !request.is_object())
            return Dump(Error(nullptr, -32700, "parse error"));
        const std::string method = request.value("method", std::string());
        if (!request.contains("id"))
            return ""; // a notification, e.g. notifications/initialized
        const Json &id = request["id"];
        const Json params = request.value("params", Json::object());

        if (method == "initialize")
        {
            const std::string asked = params.value("protocolVersion", std::string());
            const bool known = std::find(versions.begin(), versions.end(), asked) != versions.end();
            return Dump(Reply(id, {{"protocolVersion", known ? asked : versions[0]},
                              {"capabilities", {{"tools", Json::object()}}},
                              {"serverInfo", {{"name", "umbra"}, {"version", "0.1"}}},
                              {"instructions", "Umbra game project tools. Read with scene_outline/list_assets/rpg_outline; every change is validated like the game "
                                               "loads it and saved at once. C++ changes: edit the source files, then build and run_tests."}}));
        }
        if (method == "ping")
            return Dump(Reply(id, Json::object()));
        if (method == "tools/list")
        {
            Json tools = Json::array();
            for (const Json &t : DescribeTools())
                tools.push_back({{"name", t["name"]}, {"description", t["description"]}, {"inputSchema", t["inputSchema"]}});
            if (options.command)
                for (const Json &t : CommandTools())
                    tools.push_back(t);
            return Dump(Reply(id, {{"tools", tools}}));
        }
        if (method == "tools/call")
            return Dump(Reply(id, CallTool(files, options, params.value("name", std::string()), params.value("arguments", Json::object()))));
        return Dump(Error(id, -32601, "unknown method " + method));
    }

    void RunMcp(FileWorkspace &files, const McpOptions &options, std::istream &in, std::ostream &out)
    {
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line == "\r")
                continue;
            const std::string answer = McpAnswer(files, options, line);
            if (!answer.empty())
                out << answer << "\n" << std::flush;
        }
    }
}
