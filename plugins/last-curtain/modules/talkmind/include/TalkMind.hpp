#pragma once

#include <Diagnostic.hpp>
#include <Mind.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <vector>

// The model's Vietnamese test (campaign plan 1, TalkMind.md): voice sheets, a fixed set of cafe
// situations, the checks every model line must pass, and the report the user reads.
namespace umbra::talk
{
    struct Fragment
    {
        std::string id, text; // one memory, Vietnamese
    };

    struct Voice
    {
        std::string id, name, dialect;         // dialect: a key of TalkRules::dialects
        std::string sheet;                     // register, habits, topics: one paragraph for the prompt
        std::vector<std::string> markers;      // the voice's own dialect words
        std::vector<std::string> never;        // words this voice never says
        std::vector<std::string> tts;          // VieNeu preset voices to try
        std::vector<Fragment> fragments;
    };

    struct Situation
    {
        std::string id, speaker, task, authored; // task: what the line must do; authored: the fallback line
        std::vector<std::string> facts, must, forbid; // facts for the prompt; words that must / must not appear
    };

    struct TalkRules
    {
        int minLength = 8, maxLength = 160;        // code points
        double markedShare = 0.33, passShare = 0.9; // words with Vietnamese letters; lines that must pass
        int maxMs = 1500, maxVideoMb = 3072, seeds = 3, maxTokens = 64;
        std::vector<std::string> never, english;   // for every speaker; English words
        std::map<std::string, std::vector<std::string>> dialects; // "central" -> {"rang", ...}
    };

    struct TestSet
    {
        std::vector<Voice> voices;
        std::vector<Situation> situations;
        TalkRules rules;
        std::string prompt; // prompt.txt
        [[nodiscard]] const Voice *Find(const std::string &id) const;
    };

    // Reads a campaign folder (voices/*.json, model-test/*). False with diagnostics naming file, field and fix.
    bool LoadTestSet(const std::filesystem::path &campaign, TestSet &set, std::vector<core::Diagnostic> &diagnostics);

    // Lower case for Vietnamese: ASCII, Latin-1, the Latin Extended letters Vietnamese uses, U+1EA0-1EF9.
    [[nodiscard]] std::string Lower(const std::string &utf8);
    // The line's words, lower case: runs of letters, with {tokens} left out.
    [[nodiscard]] std::vector<std::string> Words(const std::string &utf8);
    // Trims spaces and wrapping quotes (straight or curly) a model likes to add.
    [[nodiscard]] std::string Tidy(std::string line);
    // "" = the line passes; else the first objection (TalkMind.md "The checks"): "empty", "bad-text",
    // "length", "no-marks", "english", "never", "dialect", "token", "missing-fact", "added-memory".
    [[nodiscard]] std::string Check(const TestSet &set, const Situation &situation, const std::string &line);

    // The prompt.txt template filled for one situation; one line of text, at most rules.maxTokens.
    [[nodiscard]] town::MindRequest Request(const TestSet &set, const Situation &situation, std::uint32_t seed);

    struct LineResult
    {
        std::string situation, speaker, authored, line, objection; // objection "" = passed
        std::uint32_t seed = 0;
        long long ms = 0;
    };

    struct ModelRun
    {
        std::string model, loadError; // loadError set: this build's llama.cpp could not load the file
        long long loadMs = 0, warmMs = 0;
        int videoMb = -1;             // -1: not measured (no graphics card in this build)
        std::vector<LineResult> lines;
        [[nodiscard]] double PassShare() const;
        [[nodiscard]] long long AverageMs() const;
        [[nodiscard]] long long SlowestMs() const;
    };

    // Every situation for rules.seeds seeds (seed = base + i): asked, timed, tidied and checked.
    void RunTest(const TestSet &set, town::Mind &mind, std::uint32_t base, ModelRun &run);
    // "passes the automatic checks", or every rule it misses (load, share, speed, video memory).
    [[nodiscard]] std::string Verdict(const TestSet &set, const ModelRun &run);
    // The report the user reads (Markdown), and its summary for comparing models ("umbra.lc-model-run" v1).
    void WriteReport(const TestSet &set, const ModelRun &run, std::ostream &markdown);
    [[nodiscard]] nlohmann::json Summary(const TestSet &set, const ModelRun &run);
    void WriteComparison(const std::vector<nlohmann::json> &summaries, std::ostream &markdown);
}
