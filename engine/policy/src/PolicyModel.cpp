#include <PolicyModel.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>

namespace umbra::policy
{
    using json = nlohmann::json;

    std::vector<float> Network::Forward(const std::vector<float> &observation) const
    {
        if (layers_.empty() || observation.size() != layers_.front().in)
            return {};
        // Each layer: out = activation(W * in + b).
        std::vector<float> values = observation;
        for (const Layer &layer : layers_)
        {
            std::vector<float> next(layer.out);
            for (std::size_t o = 0; o < layer.out; ++o)
            {
                float sum = layer.bias[o];
                for (std::size_t i = 0; i < layer.in; ++i)
                    sum += layer.weights[o * layer.in + i] * values[i];
                next[o] = layer.tanh ? std::tanh(sum) : sum;
            }
            values = std::move(next);
        }
        return values;
    }

    int Network::Choose(const std::vector<float> &observation, const std::vector<std::uint8_t> &mask) const
    {
        const std::vector<float> scores = Forward(observation);
        if (scores.size() != mask.size())
            return -1;
        int best = -1;
        for (std::size_t a = 0; a < scores.size(); ++a)
        {
            if (!std::isfinite(scores[a]))
                return -1; // a broken model: let the caller fall back
            if (mask[a] != 0 && (best < 0 || scores[a] > scores[static_cast<std::size_t>(best)]))
                best = static_cast<int>(a);
        }
        return best;
    }

    bool LoadPolicy(const std::filesystem::path &file, PolicyModel &model, std::vector<core::Diagnostic> &diagnostics)
    {
        const auto fail = [&](const std::string &code, const std::string &field, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, code, file.string(), field, message});
            return false;
        };
        // 1. JSON with the right header.
        std::ifstream in(file, std::ios::binary);
        if (!in)
            return fail("policy.read", "", "cannot open file");
        const json root = json::parse(in, nullptr, false);
        if (root.is_discarded() || !root.is_object())
            return fail("policy.format", "", "not valid JSON");
        if (root.value("format", "") != "umbra.policy" || root.value("version", 0) != 1)
            return fail("policy.format", "format", "expected \"format\": \"umbra.policy\", \"version\": 1");

        // 2. Contract.
        PolicyModel loaded;
        loaded.contract.learner = root.value("learner", "");
        loaded.contract.opponent = root.value("opponent", "");
        loaded.contract.fingerprint = root.value("fingerprint", "");
        loaded.observation = root.value("observation", "");
        for (const json &a : root.value("actions", json::array()))
            loaded.contract.actions.push_back({a.value("skill", ""), a.value("item", "")});
        if (loaded.contract.actions.empty())
            return fail("policy.actions", "actions", "no action catalogue");

        // 3. Layers: shapes chain from the observation to one score per action, numbers are finite.
        std::vector<Layer> layers;
        std::size_t expectIn = observationSize;
        for (const json &l : root.value("layers", json::array()))
        {
            Layer layer{l.value("in", 0u), l.value("out", 0u), l.value("activation", "") == "tanh", {}, {}};
            try
            {
                layer.weights = l.at("weights").get<std::vector<float>>();
                layer.bias = l.at("bias").get<std::vector<float>>();
            }
            catch (const json::exception &)
            {
                return fail("policy.shape", "layers", "weights and bias must be lists of numbers");
            }
            if (layer.in != expectIn || layer.weights.size() != layer.in * layer.out || layer.bias.size() != layer.out)
                return fail("policy.shape", "layers", "layer sizes don't chain (expected input " + std::to_string(expectIn) + ")");
            for (float w : layer.weights)
                if (!std::isfinite(w))
                    return fail("policy.shape", "layers", "a weight is not a finite number");
            expectIn = layer.out;
            layers.push_back(std::move(layer));
        }
        if (layers.empty() || expectIn != loaded.contract.actions.size() || layers.back().tanh)
            return fail("policy.shape", "layers", "the last layer must be linear with one output per action");
        loaded.network = Network(std::move(layers));
        model = std::move(loaded);
        return true;
    }

    bool CheckCompatible(const PolicyModel &model, const rpg::Content &content, const std::string &file,
                         std::vector<core::Diagnostic> &diagnostics)
    {
        const auto fail = [&](const std::string &code, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, code, file, "", message});
            return false;
        };
        if (model.observation != observationVersion)
            return fail("policy.observation", "trained for observation '" + model.observation + "', this game uses '" + observationVersion + "'");
        const Contract now = MakeContract(content, model.contract.learner, model.contract.opponent);
        if (now.actions.empty())
            return fail("policy.actor", "actor '" + model.contract.learner + "' or '" + model.contract.opponent + "' no longer exists");
        if (now.actions != model.contract.actions)
            return fail("policy.actions", "the action catalogue changed (skills or items of '" + model.contract.learner + "'); retrain");
        if (now.fingerprint != model.contract.fingerprint)
            return fail("policy.fingerprint", "skills, items, statuses or stats it was trained on changed; retrain");
        return true;
    }
}
