#include <Campaign.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <map>
#include <set>

namespace umbra::campaign
{
    using json = nlohmann::json;

    bool KnownFace(const std::string &face)
    {
        static const std::set<std::string> faces{"calm", "talk", "happy", "angry", "afraid", "scheming", "neardead"};
        return faces.contains(face);
    }

    namespace
    {
        std::string Str(const json &j, const char *key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
    }

    bool LoadScene(const std::filesystem::path &file, const Cast &cast, Scene &scene, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        const auto error = [&](const std::string &field, const std::string &message) {
            d.push_back({core::Severity::Error, "campaign.scene", file.string(), field, message});
        };
        // 1. The file and its header.
        std::ifstream in(file, std::ios::binary);
        const json j = json::parse(in, nullptr, false);
        if (!in || j.is_discarded() || !j.is_object())
        {
            error("", "not readable JSON");
            return false;
        }
        if (Str(j, "format") != "umbra.lc-scene" || j.value("version", 0) != 1)
        {
            error("format", "expected \"format\": \"umbra.lc-scene\" and \"version\": 1");
            return false;
        }
        if (!j.contains("lines") || !j["lines"].is_array() || j["lines"].empty())
        {
            error("lines", "a scene needs at least one line");
            return false;
        }
        // 2. Read every line; check names and faces; play the stage in file order.
        Scene out{Str(j, "id"), {}};
        std::map<std::string, Stage> onStage;
        for (std::size_t i = 0; i < j["lines"].size(); ++i)
        {
            const json &l = j["lines"][i];
            const std::string at = "lines[" + std::to_string(i) + "].";
            Line line{Str(l, "id"), Str(l, "speaker"), Str(l, "face"), Str(l, "text"), Str(l, "sound"), Str(l, "jump"), {}, {}, {}, l.value("model", false), Str(l, "en"), Str(l, "when")};
            if (!line.face.empty() && !KnownFace(line.face))
                error(at + "face", "expected calm, talk, happy, angry, afraid, scheming or neardead");
            for (const json &who : l.value("leave", json::array()))
            {
                line.leave.push_back(who.is_string() ? who.get<std::string>() : std::string());
                onStage.erase(line.leave.back());
            }
            const json enter = l.value("enter", json::array());
            for (std::size_t k = 0; k < enter.size(); ++k)
            {
                const std::string where = at + "enter[" + std::to_string(k) + "].";
                Stage s{Str(enter[k], "who"), Str(enter[k], "side") == "right" ? Side::Right : Side::Left, enter[k].value("slot", 0), Str(enter[k], "face")};
                if (!cast.Find(s.who))
                    error(where + "who", "\"" + s.who + "\" is not in cast.csv");
                if (Str(enter[k], "side") != "left" && Str(enter[k], "side") != "right")
                    error(where + "side", "expected left or right");
                if (s.slot != 0 && s.slot != 1)
                    error(where + "slot", "expected 0 (near the middle) or 1 (behind)");
                if (!s.face.empty() && !KnownFace(s.face))
                    error(where + "face", "expected calm, talk, happy, angry, afraid, scheming or neardead");
                onStage[s.who] = s;
                line.enter.push_back(s);
            }
            // 3. At most two portraits a side.
            std::array<int, 2> count{};
            for (const auto &[who, s] : onStage)
                ++count[static_cast<int>(s.side)];
            if (count[0] > 2 || count[1] > 2)
                error(at + "enter", "at most 2 portraits a side; take one out with \"leave\"");
            // 4. The speaker is known and on stage.
            if (!line.speaker.empty() && !cast.Find(line.speaker))
                error(at + "speaker", "\"" + line.speaker + "\" is not in cast.csv");
            else if (!line.speaker.empty() && !onStage.contains(line.speaker))
                error(at + "speaker", "\"" + line.speaker + "\" speaks but is not on stage; bring them in with \"enter\"");
            // 5. Choices: none, or two to four.
            const json choices = l.value("choices", json::array());
            if (choices.size() == 1 || choices.size() > 4)
                error(at + "choices", "a line has no choices or 2 to 4");
            for (const json &c : choices)
                line.choices.push_back({Str(c, "text"), Str(c, "fact"), Str(c, "value"), Str(c, "jump"), Str(c, "en")});
            out.lines.push_back(std::move(line));
        }
        // 6. Ids are unique; jumps go to an id.
        std::map<std::string, std::size_t> ids;
        for (std::size_t i = 0; i < out.lines.size(); ++i)
            if (!out.lines[i].id.empty() && !ids.emplace(out.lines[i].id, i).second)
                error("lines[" + std::to_string(i) + "].id", "\"" + out.lines[i].id + "\" is used twice");
        for (std::size_t i = 0; i < out.lines.size(); ++i)
        {
            const std::string at = "lines[" + std::to_string(i) + "].";
            if (!out.lines[i].jump.empty() && !ids.contains(out.lines[i].jump))
                error(at + "jump", "no line has the id \"" + out.lines[i].jump + "\"");
            for (std::size_t k = 0; k < out.lines[i].choices.size(); ++k)
                if (!out.lines[i].choices[k].jump.empty() && !ids.contains(out.lines[i].choices[k].jump))
                    error(at + "choices[" + std::to_string(k) + "].jump", "no line has the id \"" + out.lines[i].choices[k].jump + "\"");
        }
        if (d.size() != before)
            return false;
        // 7. No loop of jumps without a choice: follow the lines from each one until a choice or the end.
        for (std::size_t start = 0; start < out.lines.size(); ++start)
        {
            std::set<std::size_t> seen;
            for (std::size_t i = start; i < out.lines.size() && out.lines[i].choices.empty();)
            {
                if (!seen.insert(i).second)
                {
                    error("lines[" + std::to_string(start) + "].jump", "these lines jump in a loop with no choice; end one with no jump");
                    return false;
                }
                i = out.lines[i].jump.empty() ? i + 1 : ids.at(out.lines[i].jump);
            }
        }
        scene = std::move(out);
        return true;
    }

    void UseEnglish(Scene &scene)
    {
        for (Line &l : scene.lines)
        {
            l.text = l.en.empty() ? l.text : l.en;
            for (Choice &c : l.choices)
                c.text = c.en.empty() ? c.text : c.en;
        }
    }
}
