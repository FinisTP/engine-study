#include <Agent.hpp>
#include <Mcp.hpp>
#include <SceneEdits.hpp>

#include <assets/TestFiles.hpp>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <deque>
#include <mutex>
#include <sstream>
#include <thread>

using namespace umbra;
using agent::Json;
using State = agent::StepOutcome::State;

namespace
{
    const std::string settlement = "scenes/settlement.scene.json";

    // A copy of the reference project's assets (with the plugins' assets underneath), so tests may write.
    assets::AssetFolders CopyAssets(const std::string &name)
    {
        const auto dir = test::TempDir("agent-" + name);
        std::filesystem::copy(UMBRA_TEST_ASSET_DIRECTORY, dir, std::filesystem::copy_options::recursive);
        assets::AssetFolders folders(dir);
        folders.AddPlugins(UMBRA_TEST_PLUGIN_ASSETS);
        return folders;
    }

    agent::ToolResult Run(agent::Workspace &ws, agent::Edits &edits, const std::string &tool, const Json &args)
    {
        const agent::Tool *t = agent::FindTool(tool);
        REQUIRE(t != nullptr);
        REQUIRE(agent::CheckArguments(*t, args).empty());
        return t->run(ws, edits, args);
    }

    // A stand-in Ollama that answers with scripted JSON texts, one per request.
    class ScriptedOllama final
    {
    public:
        explicit ScriptedOllama(std::deque<Json> answers) : answers_(std::move(answers))
        {
            server_.Post("/api/chat", [this](const httplib::Request &request, httplib::Response &response) {
                std::string answer;
                {
                    std::lock_guard lock(mutex_);
                    requests.push_back(Json::parse(request.body));
                    answer = answers_.empty() ? R"({"look":[],"summary":"done","steps":[]})" : answers_.front().dump();
                    if (!answers_.empty())
                        answers_.pop_front();
                }
                const std::string line = Json{{"message", {{"role", "assistant"}, {"content", answer}}}, {"done", true}}.dump() + "\n";
                response.set_content(line, "application/x-ndjson");
            });
            port_ = server_.bind_to_any_port("127.0.0.1");
            thread_ = std::thread([this] { server_.listen_after_bind(); });
            server_.wait_until_ready();
        }
        ~ScriptedOllama()
        {
            server_.stop();
            thread_.join();
        }
        agent::PlannerSettings Settings() const
        {
            agent::PlannerSettings s;
            s.host = "http://127.0.0.1:" + std::to_string(port_);
            s.model = "scripted";
            s.timeoutSeconds = 10;
            return s;
        }
        // The last user message of request i (what the planner sent back).
        std::string Feedback(std::size_t i) const { return requests.at(i)["messages"].back()["content"]; }

        std::vector<Json> requests;

    private:
        std::deque<Json> answers_;
        std::mutex mutex_;
        httplib::Server server_;
        std::thread thread_;
        int port_ = 0;
    };

    Json Call(const std::string &tool, Json args) { return {{"tool", tool}, {"args", std::move(args)}, {"why", "test"}}; }

    // A JSON-RPC tools/call request.
    Json ToolCall(int id, const std::string &name, Json args)
    {
        Json params = {{"name", name}, {"arguments", std::move(args)}};
        return {{"jsonrpc", "2.0"}, {"id", id}, {"method", "tools/call"}, {"params", params}};
    }
}

TEST_CASE("Agent tools read the project and describe themselves (S5.1)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("read"));
    agent::Workspace &ws = files.Get();
    agent::Edits edits;

    CHECK(Run(ws, edits, "list_scenes", Json::object()).text.find(settlement) != std::string::npos);
    const auto outline = Run(ws, edits, "scene_outline", {{"scene", settlement}});
    REQUIRE(outline.ok);
    CHECK(outline.text.find("\"elder\"") != std::string::npos);
    CHECK(Run(ws, edits, "list_assets", {{"kind", "models"}}).text.find("models/elder.glb") != std::string::npos);
    CHECK(Run(ws, edits, "list_components", Json::object()).text.find("\"music\"") != std::string::npos); // a plugin's component
    CHECK(Run(ws, edits, "rpg_outline", Json::object()).text.find("health_potion") != std::string::npos);
    CHECK(Run(ws, edits, "rpg_get", {{"path", "/items/3"}}).text.find("Health Potion") != std::string::npos);
    CHECK_FALSE(Run(ws, edits, "get_entity", {{"scene", settlement}, {"id", "nobody"}}).ok);

    // Every tool has a schema an MCP client can read.
    const Json tools = agent::DescribeTools();
    CHECK(tools.size() == agent::Tools().size());
    for (const Json &t : tools)
        CHECK(t["inputSchema"]["type"] == "object");
}

TEST_CASE("Bad arguments are named before a tool runs (S5.1)", "[agent]")
{
    const agent::Tool &add = *agent::FindTool("add_entity");
    CHECK(agent::CheckArguments(add, {{"position", {0, 0, 0}}}) == "missing \"id\"");
    CHECK(agent::CheckArguments(add, {{"id", "x"}, {"position", {0, 0, 0}}, {"colour", 1}}) == "unknown argument \"colour\"");
    CHECK(agent::CheckArguments(add, {{"id", 3}, {"position", {0, 0, 0}}}) == "\"id\" must be a string");
    CHECK(agent::CheckArguments(add, {{"id", "x"}, {"position", {0, 0}}}).find("3 numbers") != std::string::npos);
    CHECK(agent::CheckArguments(*agent::FindTool("generate_sound"), {{"idea", "drums"}, {"kind", "song"}}).find("one of") != std::string::npos);
    CHECK(agent::CheckArguments(add, {{"id", "x"}, {"position", {0, 0, 0}}, {"model", "$1"}}).empty()); // a reference is filled in later
}

TEST_CASE("Changes stack on working copies and land as one undo step (S5.1)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("change"));
    agent::Workspace &ws = files.Get();
    std::vector<core::Diagnostic> diagnostics;
    authoring::Document *scene = ws.scene(settlement, diagnostics);
    REQUIRE(scene != nullptr);
    const Json before = scene->Data();
    const std::size_t count = before["entities"].size();

    agent::Edits edits;
    const auto added = Run(ws, edits, "add_entity", {{"scene", settlement}, {"id", "well"}, {"position", {4, 0, 2}}, {"model", "scenes/phase2.gltf"}});
    REQUIRE(added.ok);
    CHECK(added.value == "well_2"); // "well" is taken
    CHECK(Run(ws, edits, "set_component", {{"scene", settlement}, {"id", "well_2"}, {"type", "interactable"}, {"fields", {{"prompt", "Drink"}}}}).ok);
    CHECK(Run(ws, edits, "move_entity", {{"scene", settlement}, {"id", "well_2"}, {"turn_degrees", 90}}).ok);
    CHECK(Run(ws, edits, "place_model", {{"scene", settlement}, {"model", "models/elder.glb"}, {"position", {1, 0, 1}}, {"height", 1.8}, {"name", "twin"}}).ok);
    CHECK(Run(ws, edits, "rpg_add", {{"path", "/items"}, {"value", {{"id", "bread"}, {"name", "Bread"}, {"stack", 5}, {"use", {{{"kind", "heal"}, {"power", 10}}}}}}}).value == "bread");

    // Changing one field of an older-style built-in component keeps the others and moves it into "components".
    REQUIRE(Run(ws, edits, "set_component", {{"scene", settlement}, {"id", "signpost"}, {"type", "interactable"}, {"fields", {{"message", "East: the old dungeon"}}}}).ok);
    const Json &sign = edits.Data(*scene)["entities"][*authoring::FindEntity(edits.Data(*scene), "signpost")];
    CHECK(sign["components"]["interactable"]["prompt"] == "Read sign");
    CHECK(sign["components"]["interactable"]["message"] == "East: the old dungeon");
    CHECK_FALSE(sign.contains("interactable"));

    // A later step sees the earlier ones; the documents are untouched until Commit.
    CHECK(edits.Data(*scene)["entities"].size() == count + 2);
    CHECK(scene->Data() == before);

    // A change the game's loader refuses is named and changes nothing.
    const auto bad = Run(ws, edits, "set_field", {{"scene", settlement}, {"id", "well_2"}, {"field", "model"}, {"value", "models/missing.glb"}});
    CHECK_FALSE(bad.ok);
    CHECK(bad.text.find("asset.missing") != std::string::npos);
    CHECK_FALSE(Run(ws, edits, "rpg_add", {{"path", "/items"}, {"value", {{"name", "No id"}}}}).ok);

    std::vector<authoring::Document *> changed;
    REQUIRE(edits.Commit("Agent: well", changed, diagnostics));
    CHECK(changed.size() == 2); // the scene and the RPG content
    CHECK(scene->Data()["entities"].size() == count + 2);
    CHECK(scene->UndoLabel() == "Agent: well");
    REQUIRE(scene->Undo()); // one step undoes the whole plan
    CHECK(scene->Data() == before);
}

TEST_CASE("A commit after someone else edited the document changes nothing (S5.1)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("conflict"));
    agent::Workspace &ws = files.Get();
    std::vector<core::Diagnostic> diagnostics;
    authoring::Document *scene = ws.scene(settlement, diagnostics);
    agent::Edits edits;
    REQUIRE(Run(ws, edits, "rpg_set", {{"path", "/items/3/name"}, {"value", "Big Potion"}}).ok);
    REQUIRE(Run(ws, edits, "remove_entity", {{"scene", settlement}, {"id", "crate_0"}}).ok);
    // The user moves something in the scene meanwhile.
    REQUIRE(scene->Apply({{authoring::Change::Op::Set, "/entities/0/name", "Ground"}}, "user", diagnostics));
    std::vector<authoring::Document *> changed;
    CHECK_FALSE(edits.Commit("Agent", changed, diagnostics));
    CHECK(ws.rpg->Data()["items"][3]["name"] == "Health Potion"); // the RPG part was undone too
}

TEST_CASE("The dry run checks quick steps and leaves generated files for later (S5.2)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("dryrun"));
    agent::Workspace &ws = files.Get();
    agent::Plan plan;
    plan.steps = {{"generate_sound", {{"idea", "creaky well"}, {"kind", "effect"}}, ""},
                  {"add_entity", {{"scene", settlement}, {"id", "well"}, {"position", {4, 0, 2}}, {"model", "scenes/phase2.gltf"}}, ""},
                  {"set_component", {{"scene", settlement}, {"id", "$2"}, {"type", "interactable"}, {"fields", {{"prompt", "Use well"}, {"sound", "$1"}}}}, ""},
                  {"move_entity", {{"scene", settlement}, {"id", "$2"}, {"position", {5, 0, 2}}}, ""}};
    auto check = agent::CheckPlan(ws, plan);
    CHECK(check[0].state == State::Later);   // a generator
    CHECK(check[1].state == State::Checked);
    CHECK(check[2].state == State::Later);   // uses the generated sound
    CHECK(check[3].state == State::Checked); // uses only step 2's id
    CHECK(agent::Passed(check));

    plan.steps.push_back({"move_entity", {{"scene", settlement}, {"id", "$9"}, {"position", {0, 0, 0}}}, ""});
    plan.steps.push_back({"remove_entity", {{"scene", settlement}, {"id", "ghost"}}, ""});
    check = agent::CheckPlan(ws, plan);
    CHECK(check[4].result.text.find("earlier step") != std::string::npos);
    CHECK(check[5].state == State::Failed);
    CHECK_FALSE(agent::Passed(check));

    std::vector<core::Diagnostic> diagnostics;
    CHECK_FALSE(ws.scene(settlement, diagnostics)->IsDirty()); // nothing was changed
}

TEST_CASE("The planner looks, has a broken plan fixed, then returns a checked plan (S5.2)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("planner"));
    files.SetOpenScene(settlement);
    ScriptedOllama ollama({
        {{"look", {Call("get_entity", {{"scene", settlement}, {"id", "elder"}})}}, {"summary", ""}, {"steps", Json::array()}},
        {{"look", Json::array()}, {"summary", "a bench"}, {"steps", {Call("add_entity", {{"scene", settlement}, {"id", "bench"}, {"pos", {0, 0, 0}}})}}},
        {{"look", Json::array()}, {"summary", "a bench by the elder"},
         {"steps", {Call("add_entity", {{"scene", settlement}, {"id", "bench"}, {"position", {2, 0, 3}}, {"model", "scenes/phase2.gltf"}})}}},
    });
    agent::Planner planner;
    agent::PlanNow(planner, files.Get(), "put a bench next to the elder", ollama.Settings());
    REQUIRE(planner.ok);
    CHECK(planner.plan.summary == "a bench by the elder");
    REQUIRE(planner.plan.steps.size() == 1);
    CHECK(planner.check[0].state == State::Checked);
    REQUIRE(ollama.requests.size() == 3);
    CHECK(ollama.requests[0]["format"]["properties"].contains("steps")); // the answer's shape is forced
    CHECK(ollama.requests[0]["messages"][1]["content"].get<std::string>().find("\"elder\"") != std::string::npos); // the open scene is in the context
    CHECK(ollama.Feedback(1).find("Results:") != std::string::npos);
    CHECK(ollama.Feedback(2).find("Step 1 (add_entity): missing \"position\"") != std::string::npos);
}

TEST_CASE("The planner gives up on a model that keeps answering badly (S5.2)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("giveup"));
    const Json bad = {{"look", Json::array()}, {"summary", ""}, {"steps", {Call("remove_entity", {{"scene", settlement}, {"id", "ghost"}})}}};
    ScriptedOllama ollama({bad, bad, bad, bad});
    agent::Planner planner;
    auto settings = ollama.Settings();
    settings.maxFixes = 2;
    agent::PlanNow(planner, files.Get(), "remove the ghost", settings);
    CHECK_FALSE(planner.ok);
    REQUIRE_FALSE(planner.diagnostics.empty());
    CHECK(planner.diagnostics[0].code == "agent.plan");
    CHECK(ollama.requests.size() == 3); // the first answer and two fixes
}

TEST_CASE("Ollama being away is reported, not hidden (S5.2)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("away"));
    agent::PlannerSettings settings;
    settings.host = "http://api.example.com:11434"; // not this computer: refused before connecting
    agent::Planner planner;
    agent::PlanNow(planner, files.Get(), "anything", settings);
    CHECK_FALSE(planner.ok);
    REQUIRE_FALSE(planner.diagnostics.empty());
    CHECK(planner.diagnostics[0].code == "ai.host");
}

TEST_CASE("An approved plan runs in order; a failing step stops the rest and commits nothing (S5.2)", "[agent]")
{
    agent::FileWorkspace files(CopyAssets("runner"));
    agent::Workspace &ws = files.Get();
    std::vector<core::Diagnostic> diagnostics;
    authoring::Document *scene = ws.scene(settlement, diagnostics);
    const Json before = scene->Data();

    agent::Plan good;
    good.steps = {{"add_entity", {{"scene", settlement}, {"id", "lamp"}, {"position", {1, 0, 1}}, {"model", "scenes/phase2.gltf"}}, ""},
                  {"set_component", {{"scene", settlement}, {"id", "$1"}, {"type", "spin"}}, ""}}; // the project's own component, with its defaults
    agent::Runner runner;
    agent::RunNow(runner, ws, good, "Agent: lamp");
    REQUIRE(runner.committed);
    CHECK(runner.outcomes[1].state == State::Done);
    CHECK(runner.changed.size() == 1);
    const Json &lamp = scene->Data()["entities"].back();
    CHECK(lamp["id"] == "lamp");
    CHECK(lamp["components"]["spin"].is_object());
    REQUIRE(scene->Undo());
    CHECK(scene->Data() == before);

    agent::Plan broken = good;
    broken.steps.insert(broken.steps.begin() + 1, {"remove_entity", {{"scene", settlement}, {"id", "ghost"}}, ""});
    agent::Runner second;
    agent::RunNow(second, ws, broken, "Agent: lamp");
    CHECK_FALSE(second.committed);
    CHECK(second.outcomes[0].state == State::Done);
    CHECK(second.outcomes[1].state == State::Failed);
    CHECK(second.outcomes[2].state == State::Skipped);
    CHECK(scene->Data() == before); // nothing landed

    // Saved changes reach the file on disk (the MCP server's way).
    agent::Runner third;
    agent::RunNow(third, ws, good, "Agent: lamp");
    REQUIRE(files.Save(diagnostics));
    CHECK(test::ReadText(ws.assets.Project() / settlement).find("\"lamp\"") != std::string::npos);
}

TEST_CASE("An MCP client lists the tools and edits a scene that is saved at once (S5.4)", "[agent][mcp]")
{
    const auto assets = CopyAssets("mcp");
    agent::FileWorkspace files(assets);
    agent::McpOptions options;
    const auto ask = [&](const Json &message) {
        const std::string answer = agent::McpAnswer(files, options, message.dump());
        return answer.empty() ? Json() : Json::parse(answer);
    };

    const Json init = ask({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}, {"params", {{"protocolVersion", "2025-06-18"}, {"capabilities", Json::object()}}}});
    CHECK(init["result"]["protocolVersion"] == "2025-06-18");
    CHECK(init["result"]["capabilities"].contains("tools"));
    CHECK(ask({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}).is_null()); // no answer to a notification

    const Json list = ask({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}});
    CHECK(list["result"]["tools"].size() == agent::Tools().size()); // no build tools without --build

    const Json add = ask(ToolCall(3, "add_entity", {{"scene", settlement}, {"id", "mcp_marker"}, {"position", {0, 0, 5}}}));
    CHECK(add["result"]["isError"] == false);
    CHECK(test::ReadText(assets.Project() / settlement).find("mcp_marker") != std::string::npos);

    const Json bad = ask(ToolCall(4, "remove_entity", {{"scene", settlement}}));
    CHECK(bad["result"]["isError"] == true);
    CHECK(bad["result"]["content"][0]["text"].get<std::string>().find("missing \"id\"") != std::string::npos);

    CHECK(ask({{"jsonrpc", "2.0"}, {"id", 5}, {"method", "resources/list"}})["error"]["code"] == -32601);
    CHECK(Json::parse(agent::McpAnswer(files, options, "{not json"))["error"]["code"] == -32700);

    // A file the editor saved meanwhile is read again before the next change.
    std::string text = test::ReadText(assets.Project() / settlement);
    text.replace(text.find("mcp_marker"), 10, "renamed_by_hand");
    test::WriteText(assets.Project() / settlement, text);
    const Json move = ask(ToolCall(6, "move_entity", {{"scene", settlement}, {"id", "renamed_by_hand"}, {"position", {1, 0, 5}}}));
    CHECK(move["result"]["isError"] == false);

    // The stdio loop: one answer line per request line.
    options.command = [](const std::string &tool, const Json &) { return std::pair<bool, std::string>{true, tool + " ok"}; };
    std::istringstream in(R"({"jsonrpc":"2.0","id":1,"method":"ping"})" "\n\n" R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"build","arguments":{}}})" "\n");
    std::ostringstream out;
    agent::RunMcp(files, options, in, out);
    std::istringstream lines(out.str());
    std::string first, second;
    std::getline(lines, first);
    std::getline(lines, second);
    CHECK(Json::parse(first)["result"].empty());
    CHECK(Json::parse(second)["result"]["content"][0]["text"] == "build ok");
}
