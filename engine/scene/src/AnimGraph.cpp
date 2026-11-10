#include <AnimGraph.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>

namespace umbra::scene
{
    using nlohmann::json;

    namespace
    {
        void Add(std::vector<core::Diagnostic> &out, const std::string &code, const std::string &file, const std::string &field, const std::string &message)
        {
            out.push_back({core::Severity::Error, code, file, field, message});
        }
    }

    bool ParseAnimGraph(const std::string &text, const std::string &file, core::AnimGraph &graph, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        const json j = json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object() || j.value("format", "") != "umbra.animgraph" || j.value("version", 0) != 1)
        {
            Add(diagnostics, "animgraph.format", file, "", "not an animation graph (format \"umbra.animgraph\", version 1)");
            return false;
        }
        core::AnimGraph out;
        out.fade = j.value("fade", 0.2F);
        if (!(out.fade >= 0.0F && out.fade <= 2.0F))
            Add(diagnostics, "animgraph.fade", file, "fade", "fade must be 0..2 seconds");
        const json states = j.value("states", json::array());
        if (!states.is_array() || states.empty())
            Add(diagnostics, "animgraph.states", file, "states", "needs at least one state");
        for (std::size_t i = 0; states.is_array() && i < states.size(); ++i)
        {
            // 1. Name and clip.
            const json &s = states[i];
            const std::string at = "states[" + std::to_string(i) + "]";
            if (!s.is_object() || !s.value("name", json()).is_string() || !s.value("clip", json()).is_string())
            {
                Add(diagnostics, "animgraph.state", file, at, "a state needs a \"name\" and a \"clip\"");
                continue;
            }
            core::AnimState state;
            state.name = s["name"];
            state.clip = s["clip"];
            state.loop = s.value("loop", true);
            // 2. When it plays: a trigger ("on"), a flag, a number above a value, or always.
            state.on = s.value("on", "");
            if (s.contains("when"))
            {
                const json &w = s["when"];
                state.flag = w.value("flag", "");
                state.param = w.value("param", "");
                state.above = w.value("above", state.above);
                if (state.flag.empty() == state.param.empty())
                    Add(diagnostics, "animgraph.when", file, at + ".when", "\"when\" needs either a \"flag\" or a \"param\" with \"above\"");
            }
            if (!state.on.empty() && s.contains("when"))
                Add(diagnostics, "animgraph.when", file, at, "a state is started by \"on\" or chosen by \"when\", not both");
            // 3. Playback rate from a number (walk cycles keep up with the speed).
            if (s.contains("rate"))
            {
                state.rateParam = s["rate"].value("param", "");
                state.ratePer = s["rate"].value("per", 0.0F);
                if (state.rateParam.empty() || state.ratePer <= 0.0F)
                    Add(diagnostics, "animgraph.rate", file, at + ".rate", "\"rate\" needs a \"param\" and \"per\" above 0");
            }
            // 4. Events at times in the clip (used by S9: footsteps, hits).
            for (const json &e : s.value("events", json::array()))
                if (e.is_object() && e.value("at", -1.0F) >= 0.0F && !e.value("name", "").empty())
                    state.events.emplace_back(e["at"].get<float>(), e["name"].get<std::string>());
                else
                    Add(diagnostics, "animgraph.event", file, at + ".events", "an event needs \"at\" (seconds) and a \"name\"");
            for (const core::AnimState &other : out.states)
                if (other.name == state.name)
                    Add(diagnostics, "animgraph.state", file, at, "two states are called '" + state.name + "'");
            out.states.push_back(std::move(state));
        }
        if (diagnostics.size() != before)
            return false;
        graph = std::move(out);
        return true;
    }

    std::shared_ptr<const core::AnimGraph> LoadAnimGraph(const std::filesystem::path &file, const std::string &name,
                                                         std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            Add(diagnostics, "animgraph.read", name, "", "cannot open the file");
            return nullptr;
        }
        core::AnimGraph graph;
        if (!ParseAnimGraph(std::string(std::istreambuf_iterator<char>(in), {}), name, graph, diagnostics))
            return nullptr;
        return std::make_shared<const core::AnimGraph>(std::move(graph));
    }

    void BindAnimators(world::World &world, const ModelFinder &models, const GraphFinder &graphs, std::vector<core::Diagnostic> &diagnostics)
    {
        world.ForEach([&](world::Entity entity) {
            const std::string *graphPath = world.GetAnimationGraph(entity);
            const world::Renderable *renderable = world.GetRenderable(entity);
            const std::string id = *world.GetId(entity);
            world.SetAnimator(entity, std::nullopt);
            // 0. A rigged model without an animation component still needs its rest pose: its
            //    vertices are stored in bind space (Skeleton.md), so an empty graph holds it at rest.
            if (graphPath == nullptr)
            {
                static const auto still = std::make_shared<const core::AnimGraph>();
                if (const std::shared_ptr<const assets::Model> model = renderable ? models(renderable->model) : nullptr; model && model->skeleton)
                    world.SetAnimator(entity, core::Animator(std::shared_ptr<const core::Skeleton>(model, &*model->skeleton), still, {}));
                return;
            }
            // 1. The graph and the model's skeleton.
            const std::shared_ptr<const core::AnimGraph> graph = graphs(*graphPath);
            const std::shared_ptr<const assets::Model> model = renderable ? models(renderable->model) : nullptr;
            if (!graph)
            {
                Add(diagnostics, "entity.animation.graph", id, "components.animation.graph", "'" + *graphPath + "' could not be loaded");
                return;
            }
            if (!model || !model->skeleton)
            {
                Add(diagnostics, "entity.animation.skeleton", id, "model", "the model has no skeleton (rig it first)");
                return;
            }
            // 2. Clips: "name" from this model, "models/x.glb#name" from another model.
            const auto find = [&](const std::string &name) -> std::shared_ptr<const core::Clip> {
                const std::size_t hash = name.find('#');
                std::shared_ptr<const assets::Model> source = hash == std::string::npos ? model : models(name.substr(0, hash));
                const std::string clipName = hash == std::string::npos ? name : name.substr(hash + 1);
                if (!source)
                    return nullptr;
                for (const core::Clip &clip : source->clips)
                    if (clip.name == clipName)
                        return std::shared_ptr<const core::Clip>(source, &clip); // keeps its model alive
                return nullptr;
            };
            // The skeleton shares the model's lifetime; the animator holds both.
            const std::shared_ptr<const core::Skeleton> skeleton(model, &*model->skeleton);
            core::Animator animator(skeleton, graph, find);
            if (animator.Missing())
                Add(diagnostics, "entity.animation.clip", id, "components.animation", "clip '" + animator.MissingClip() + "' was not found");
            world.SetAnimator(entity, std::move(animator));
        });
    }
}
