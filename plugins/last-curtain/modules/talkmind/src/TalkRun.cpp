#include "TalkMind.hpp"

#include <algorithm>
#include <chrono>
#include <map>

namespace umbra::talk
{
    namespace
    {
        void Replace(std::string &text, const std::string &from, const std::string &to)
        {
            for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
                text.replace(at, from.size(), to);
        }

        // A table cell: no line breaks, no column bars.
        std::string Cell(std::string text)
        {
            Replace(text, "|", "\\|");
            Replace(text, "\n", " ");
            return text;
        }

        std::string Percent(double share) { return std::to_string(static_cast<int>(share * 100.0 + 0.5)) + "%"; }
    }

    town::MindRequest Request(const TestSet &set, const Situation &situation, std::uint32_t seed)
    {
        const Voice *voice = set.Find(situation.speaker);
        std::string facts;
        for (const std::string &fact : situation.facts)
            facts += "- " + fact + "\n";
        std::string prompt = set.prompt;
        Replace(prompt, "{name}", voice != nullptr ? voice->name : situation.speaker);
        Replace(prompt, "{sheet}", voice != nullptr ? voice->sheet : "");
        Replace(prompt, "{facts}", facts.empty() ? "" : facts.substr(0, facts.size() - 1));
        Replace(prompt, "{task}", situation.task);
        Replace(prompt, "{authored}", situation.authored);
        return {prompt, "root ::= [^\\n\\r]+", set.rules.maxTokens, seed};
    }

    double ModelRun::PassShare() const
    {
        if (lines.empty())
            return 0.0;
        const auto passed = std::count_if(lines.begin(), lines.end(), [](const LineResult &l) { return l.objection.empty(); });
        return static_cast<double>(passed) / static_cast<double>(lines.size());
    }

    long long ModelRun::AverageMs() const
    {
        long long total = 0;
        for (const LineResult &l : lines)
            total += l.ms;
        return lines.empty() ? 0 : total / static_cast<long long>(lines.size());
    }

    long long ModelRun::SlowestMs() const
    {
        long long slowest = 0;
        for (const LineResult &l : lines)
            slowest = std::max(slowest, l.ms);
        return slowest;
    }

    void RunTest(const TestSet &set, town::Mind &mind, std::uint32_t base, ModelRun &run)
    {
        for (const Situation &s : set.situations)
            for (int i = 0; i < set.rules.seeds; ++i)
            {
                const std::uint32_t seed = base + static_cast<std::uint32_t>(i);
                // 1. Ask and time it; 2. tidy; 3. check.
                const auto start = std::chrono::steady_clock::now();
                const std::string answer = mind.Complete(Request(set, s, seed));
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                const std::string line = Tidy(answer);
                run.lines.push_back({s.id, s.speaker, s.authored, line, Check(set, s, line), seed, ms});
            }
    }

    std::string Verdict(const TestSet &set, const ModelRun &run)
    {
        if (!run.loadError.empty())
            return "cannot load: " + run.loadError;
        const TalkRules &r = set.rules;
        std::string misses;
        const auto miss = [&](const std::string &text) { misses += (misses.empty() ? "" : "; ") + text; };
        if (run.PassShare() < r.passShare)
            miss(Percent(run.PassShare()) + " of lines pass the checks (needs " + Percent(r.passShare) + ")");
        if (run.AverageMs() > r.maxMs)
            miss("average " + std::to_string(run.AverageMs()) + " ms a line (needs under " + std::to_string(r.maxMs) + ")");
        if (run.videoMb > r.maxVideoMb)
            miss("uses " + std::to_string(run.videoMb) + " MB of video memory (needs under " + std::to_string(r.maxVideoMb) + ")");
        return misses.empty() ? "passes the automatic checks" : misses;
    }

    void WriteReport(const TestSet &set, const ModelRun &run, std::ostream &out)
    {
        // 1. The header: the numbers and the verdict.
        out << "# Vietnamese model test: " << run.model << "\n\n"
            << "| | |\n|---|---|\n"
            << "| Verdict | " << Cell(Verdict(set, run)) << " |\n"
            << "| Lines passing the checks | " << Percent(run.PassShare()) << " of " << run.lines.size() << " (needs " << Percent(set.rules.passShare) << ") |\n"
            << "| Time a line | average " << run.AverageMs() << " ms, slowest " << run.SlowestMs() << " ms (needs under " << set.rules.maxMs << " ms) |\n"
            << "| Video memory | " << (run.videoMb < 0 ? std::string("not measured") : std::to_string(run.videoMb) + " MB") << " (needs under " << set.rules.maxVideoMb << " MB) |\n"
            << "| Load, warm-up | " << run.loadMs << " ms, " << run.warmMs << " ms |\n\n"
            << "The automatic checks are half of the pass: the other half is whether the lines read well to a native speaker.\n";
        if (!run.loadError.empty())
            return;
        // 2. Every line, by speaker: the authored line beside the model's.
        for (const Voice &v : set.voices)
        {
            out << "\n## " << v.name << "\n\n| Situation | Seed | Authored | Model | Check | ms |\n|---|---|---|---|---|---|\n";
            for (const LineResult &l : run.lines)
                if (l.speaker == v.id)
                    out << "| " << l.situation << " | " << l.seed << " | " << Cell(l.authored) << " | " << Cell(l.line) << " | "
                        << (l.objection.empty() ? "ok" : l.objection) << " | " << l.ms << " |\n";
        }
        // 3. How often each check objected.
        std::map<std::string, int> counts;
        for (const LineResult &l : run.lines)
            if (!l.objection.empty())
                ++counts[l.objection];
        out << "\n## Failures by check\n\n";
        if (counts.empty())
            out << "None.\n";
        for (const auto &[check, n] : counts)
            out << "- " << check << ": " << n << "\n";
    }

    nlohmann::json Summary(const TestSet &set, const ModelRun &run)
    {
        return {{"format", "umbra.lc-model-run"}, {"version", 1}, {"model", run.model}, {"loads", run.loadError.empty()},
                {"loadMs", run.loadMs}, {"warmMs", run.warmMs}, {"videoMb", run.videoMb}, {"lines", run.lines.size()},
                {"passShare", run.PassShare()}, {"averageMs", run.AverageMs()}, {"slowestMs", run.SlowestMs()}, {"verdict", Verdict(set, run)}};
    }

    void WriteComparison(const std::vector<nlohmann::json> &summaries, std::ostream &out)
    {
        out << "# Vietnamese model test: comparison\n\n| Model | Loads | Lines passing | Average ms | Slowest ms | Video MB | Verdict |\n|---|---|---|---|---|---|---|\n";
        for (const nlohmann::json &s : summaries)
            out << "| " << Cell(s.value("model", "?")) << " | " << (s.value("loads", false) ? "yes" : "no") << " | "
                << Percent(s.value("passShare", 0.0)) << " | " << s.value("averageMs", 0LL) << " | " << s.value("slowestMs", 0LL) << " | "
                << (s.value("videoMb", -1) < 0 ? std::string("-") : std::to_string(s.value("videoMb", -1))) << " | " << Cell(s.value("verdict", "")) << " |\n";
        out << "\nThe user picks the winner: the reports' lines, each model's licence, and these numbers.\n";
    }
}
