#pragma once

#include <Diagnostic.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <filesystem>
#include <string>
#include <vector>

namespace umbra::campaign
{
    // How the engine tints a character (plan 7).
    enum class Form : std::uint8_t
    {
        Player,
        Ally,
        Strung,
        Ghost,
        Host,
        Zodiac
    };

    // One row of cast.csv.
    struct CastMember
    {
        std::string id, name, sprite, bound; // bound: the sprite while strung ("" = none)
        Form form = Form::Ghost;
        std::string voice;
    };

    struct Cast
    {
        std::vector<CastMember> members;
        const CastMember *Find(const std::string &id) const;
    };

    bool LoadCast(const std::filesystem::path &file, Cast &cast, std::vector<core::Diagnostic> &d);

    enum class Side : std::uint8_t
    {
        Left,
        Right
    };

    // A portrait brought in (or moved) before a line.
    struct Stage
    {
        std::string who;
        Side side = Side::Left;
        int slot = 0; // 0 nearest the middle, 1 behind
        std::string face;
    };

    struct Choice
    {
        std::string text, fact, value, jump;
    };

    struct Line
    {
        std::string id, speaker, face, text, sound, jump; // speaker "" = narration; jump: the next line's id
        std::vector<Stage> enter;
        std::vector<std::string> leave;
        std::vector<Choice> choices; // 0 or 2-4
        bool model = false;          // plan 6: the model may reword it
    };

    struct Scene
    {
        std::string id;
        std::vector<Line> lines;
    };

    // The faces every portrait may have (face_<name>.png).
    bool KnownFace(const std::string &face);

    // Reads and checks a scene; each problem names the file, the line's field and the fix.
    bool LoadScene(const std::filesystem::path &file, const Cast &cast, Scene &scene, std::vector<core::Diagnostic> &d);

    // The player as the save knows them: the typed name and "male" or "female".
    struct Speaker
    {
        std::string name, gender;
    };

    using Facts = std::map<std::string, std::string>;

    struct Portrait
    {
        std::string who, face;
        bool lit = false;
    };

    // {player}, {anh/chi}, {Anh/Chi}; an empty name becomes the cast's "nameless" name.
    std::string FillTokens(const std::string &text, const Speaker &player, const Cast &cast);

    // Plays a scene with no drawing: the line so far, who stands where, the choices (Campaign.md).
    class ScenePlayer final
    {
    public:
        ScenePlayer(const Scene &scene, const Cast &cast, Speaker player, Facts &facts);
        void Update(float seconds, float lettersPerSecond); // the typewriter
        void Advance();                                     // complete the line, else the next one (not past a choice)
        void Skip();                                        // to the end, stopping at the first choice
        bool Choose(int index);                             // sets the fact and jumps; false with no choice shown
        [[nodiscard]] bool Done() const { return at_ >= scene_.lines.size(); }
        [[nodiscard]] std::string Shown() const;
        [[nodiscard]] const std::string &SpeakerName() const { return name_; }
        [[nodiscard]] Side SpeakerSide() const;
        [[nodiscard]] std::array<Portrait, 2> Left() const { return Row(Side::Left); }
        [[nodiscard]] std::array<Portrait, 2> Right() const { return Row(Side::Right); }
        [[nodiscard]] std::vector<std::string> Choices() const;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> Log() const { return log_; }
        std::string TakeSound(); // the line's sound, once, as it starts

    private:
        void Enter(std::size_t index);
        void Next(const std::string &jump);
        bool Complete() const { return shown_ >= letters_; }
        std::array<Portrait, 2> Row(Side side) const;

        const Scene &scene_;
        const Cast &cast_;
        Speaker player_;
        Facts &facts_;
        std::map<std::string, Stage> stage_;
        std::size_t at_ = 0, letters_ = 0, shown_ = 0;
        float progress_ = 0.0F;
        std::string text_, name_, sound_;
        std::vector<std::pair<std::string, std::string>> log_;
    };
}
