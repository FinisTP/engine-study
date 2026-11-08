#pragma once

#include <AgentTools.hpp>

#include <functional>
#include <iosfwd>
#include <string>

// An MCP server (Model Context Protocol) over stdio for external coding agents (roadmap S5.4,
// ADR 0008): the agent tools plus, optionally, build and test. JSON-RPC 2.0, one message per
// line. Every change is checked like the editor's and saved to disk at once. See Agent.md.
namespace umbra::agent
{
    struct McpOptions final
    {
        // Extra tools for C++ work: "build" and "run_tests". Given the tool name, returns
        // {ok, output}. Empty = not offered.
        std::function<std::pair<bool, std::string>(const std::string &tool, const Json &args)> command;
    };

    // Answers one JSON-RPC message; "" for a notification (no answer).
    [[nodiscard]] std::string McpAnswer(FileWorkspace &files, const McpOptions &options, const std::string &message);

    // Reads messages from `in` until it closes, answering on `out`.
    void RunMcp(FileWorkspace &files, const McpOptions &options, std::istream &in, std::ostream &out);
}
