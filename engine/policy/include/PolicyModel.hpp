#pragma once

#include <Diagnostic.hpp>
#include <PolicyContract.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace umbra::policy
{
    // A small fully connected network: tanh hidden layers, linear output (one score per action).
    // Weights are row-major: weights[o * in + i]. The math is in Policy.md.
    struct Layer final
    {
        std::size_t in = 0;
        std::size_t out = 0;
        bool tanh = true; // false for the output layer
        std::vector<float> weights;
        std::vector<float> bias;
    };

    class Network final
    {
    public:
        Network() = default;
        explicit Network(std::vector<Layer> layers) : layers_(std::move(layers)) {}

        // Action scores (logits) for one observation. Empty if the size is wrong.
        [[nodiscard]] std::vector<float> Forward(const std::vector<float> &observation) const;
        // The legal action with the highest score; -1 if none is legal or a score isn't finite.
        [[nodiscard]] int Choose(const std::vector<float> &observation, const std::vector<std::uint8_t> &mask) const;
        [[nodiscard]] const std::vector<Layer> &Layers() const noexcept { return layers_; }

    private:
        std::vector<Layer> layers_;
    };

    // A trained policy file ("format": "umbra.policy"): its contract, network and training notes.
    struct PolicyModel final
    {
        Contract contract;
        std::string observation;
        Network network;
    };

    // Reads and checks the file on its own (format, shapes, finite numbers).
    bool LoadPolicy(const std::filesystem::path &file, PolicyModel &model, std::vector<core::Diagnostic> &diagnostics);
    // Checks the model still fits this content: same observation version, same action catalogue,
    // same fingerprint (roadmap T26/T27). A model that doesn't fit must not be used.
    bool CheckCompatible(const PolicyModel &model, const rpg::Content &content, const std::string &file,
                         std::vector<core::Diagnostic> &diagnostics);
}
