#pragma once

#include <AssetFolders.hpp>
#include <Document.hpp>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The agent's tools (roadmap S5, ADR 0008): one list used by the editor's Agent window and by
// "UmbraTool mcp". Every change goes through authoring::Document, so it is validated and
// undoable. See Agent.md.
namespace umbra::agent
{
    using authoring::Json;

    // What the tools work on. The editor and the MCP server each fill one.
    struct Workspace
    {
        assets::AssetFolders assets;
        authoring::Document *rpg = nullptr; // the project's RPG content (nullptr = none)
        // The scene document for an asset path ("" = the open scene, if there is one); nullptr if
        // it can't be opened (diagnostics say why).
        std::function<authoring::Document *(const std::string &scene, std::vector<core::Diagnostic> &)> scene;
        const std::atomic<bool> *stop = nullptr; // set to cancel a running generator
    };

    // Changes made by a plan's steps, kept on working copies until Commit. A later step sees the
    // earlier ones; every step is validated on its own, so a broken step is named exactly.
    class Edits final
    {
    public:
        // The document as the earlier steps left it.
        [[nodiscard]] const Json &Data(authoring::Document &doc);
        // Applies and validates on the working copy; nothing changes on failure.
        bool Change(authoring::Document &doc, const std::vector<authoring::Change> &changes, std::vector<core::Diagnostic> &diagnostics);
        // One Document::Apply per changed document, all or none (an applied one is undone if a
        // later one fails, e.g. the user edited it meanwhile). Returns the changed documents.
        bool Commit(const std::string &label, std::vector<authoring::Document *> &changed, std::vector<core::Diagnostic> &diagnostics);
        [[nodiscard]] bool Empty() const { return pending_.empty(); }

    private:
        struct Pending final
        {
            authoring::Document *doc;
            std::uint64_t revision; // the document's state when the first step touched it
            Json data;
            std::vector<authoring::Change> changes;
        };
        Pending &Of(authoring::Document &doc);
        std::vector<Pending> pending_;
    };

    struct ToolResult final
    {
        bool ok = false;
        std::string text;   // shown to the user and given to the model
        std::string value;  // what "$<step>" stands for in later steps (an entity id, an asset path)
        std::vector<core::Diagnostic> diagnostics;
    };

    struct Tool final
    {
        enum class Kind
        {
            Read,     // looks only
            Change,   // edits documents (one undo step for the whole plan)
            Generate  // makes new asset files with a local model (slow; run on a worker thread)
        };
        std::string name;
        std::string description; // one line for the model
        Json schema;             // JSON schema of the arguments ("type": "object")
        Kind kind = Kind::Read;
        std::function<ToolResult(Workspace &, Edits &, const Json &args)> run;
    };

    [[nodiscard]] const std::vector<Tool> &Tools();
    [[nodiscard]] const Tool *FindTool(const std::string &name);

    // Checks arguments against the tool's schema: required fields, types, enums, no unknown
    // fields. "" if they fit, otherwise what is wrong (given back to the model to fix).
    [[nodiscard]] std::string CheckArguments(const Tool &tool, const Json &args);

    // The tool list for a prompt or an MCP client: [{name, description, schema, kind}].
    [[nodiscard]] Json DescribeTools();

    // Scene documents opened from disk (the MCP server; tests). Scenes are checked like the game
    // loads them, models included.
    class FileWorkspace final
    {
    public:
        explicit FileWorkspace(assets::AssetFolders assets);
        ~FileWorkspace();
        [[nodiscard]] Workspace &Get() { return workspace_; }
        // Writes every changed document to disk (atomic, refuses files changed by someone else).
        bool Save(std::vector<core::Diagnostic> &diagnostics);
        // The scene that "" (no scene named) means.
        void SetOpenScene(std::string path) { openScene_ = std::move(path); }
        // Re-reads unchanged documents that someone else saved meanwhile (the editor).
        void Refresh();

    private:
        struct Models;
        Workspace workspace_;
        std::unique_ptr<authoring::Document> rpg_;
        std::map<std::string, std::unique_ptr<authoring::Document>> scenes_;
        std::unique_ptr<Models> models_;
        std::string openScene_;
    };
}
