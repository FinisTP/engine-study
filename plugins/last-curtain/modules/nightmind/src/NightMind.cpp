#include "NightMind.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace umbra::night
{
    namespace
    {
        // What each kind asks for, and three worked examples (a small model copies examples far
        // better than it follows a description). Their people are not guests; the guard refuses
        // them if copied.
        struct Kind
        {
            const char *task;
            std::array<const char *, 3> examples;
        };

        const Kind &KindOf(LineKind kind)
        {
            static const Kind caption{
                "Reword one line of narration from a haunted opera house at night, as a short stage direction. Keep every name and the exact meaning; never say how it ends.",
                {"Speaker: narrator\nAbout: Co Hoa (she)\nLine: Co Hoa searches the cloakroom.\nReworded: Co Hoa runs her hands along the dusty coat hooks.\n\n",
                 "Speaker: narrator\nAbout: Ong Lam (he), Chi Tam (she)\nLine: Ong Lam attacks Chi Tam.\nReworded: Ong Lam lunges at Chi Tam across the dark aisle.\n\n",
                 "Speaker: narrator\nAbout: Chi Tam (she)\nLine: Chi Tam moves into the orchestra pit.\nReworded: Chi Tam climbs down into the silent orchestra pit.\n\n"}};
            static const Kind question{
                "Reword a question one guest asks another, in the speaker's own voice. Keep every name and the meaning; never hint at which answer is better.",
                {"Speaker: Co Hoa (nervous, kind)\nLine: Will you walk with me to the stage?\nReworded: Would you come with me to the stage, please?\n\n",
                 "Speaker: Ong Lam (gruff, few words)\nLine: Do you believe the ghost light keeps us safe?\nReworded: That ghost light, you think it keeps us safe?\n\n",
                 "Speaker: Chi Tam (sharp, curious)\nAbout: Ong Lam (he)\nLine: Do you trust Ong Lam?\nReworded: Tell me honestly, do you trust Ong Lam?\n\n"}};
            static const Kind goal{
                "Reword what the speaker says about their own goal tonight, in their own voice. Keep every name and the exact meaning.",
                {"Speaker: Co Hoa (nervous, kind)\nLine: I found the music box.\nReworded: I found it, the music box was here all along.\n\n",
                 "Speaker: Ong Lam (gruff, few words)\nLine: I could not reach the roof.\nReworded: The roof was too far, I never made it.\n\n",
                 "Speaker: Chi Tam (sharp, curious)\nLine: I have seen every mirror in the house.\nReworded: Every mirror in this house, I have looked into them all.\n\n"}};
            static const Kind turn{
                "Midnight: a guest turns into a monster. Write two sentences: the guest's last human words, then the monster's first words. Keep every name and the meaning.",
                {"Speaker: Co Hoa (nervous, kind), now The Paper Bride (whispering, sad)\nLine: Co Hoa becomes The Paper Bride.\nReworded: Please, someone hold my hand. The Paper Bride will hold yours forever.\n\n",
                 "Speaker: Ong Lam (gruff, few words), now The Rusted Usher (cold, polite)\nLine: Ong Lam becomes The Rusted Usher.\nReworded: Not now, not like this. The Rusted Usher will show you to your seat.\n\n",
                 "Speaker: Chi Tam (sharp, curious), now The Last Echo (repeating, hollow)\nLine: Chi Tam becomes The Last Echo.\nReworded: I wanted to see the end. The Last Echo sees the end, the end.\n\n"}};
            static const Kind taken{
                "Reword what a monster says as it acts, in its own voice, as a short taunt. Keep every name and the exact meaning; never say how it ends.",
                {"Speaker: The Paper Bride (whispering, sad)\nAbout: Ong Lam (he)\nLine: The Paper Bride attacks Ong Lam.\nReworded: Dance with me, Ong Lam, the music never stops.\n\n",
                 "Speaker: The Rusted Usher (cold, polite)\nAbout: Chi Tam (she)\nLine: The Rusted Usher moves toward Chi Tam.\nReworded: Your seat is waiting, Chi Tam, this way please.\n\n",
                 "Speaker: The Last Echo (repeating, hollow)\nLine: The Last Echo moves into the foyer.\nReworded: Into the foyer, the foyer, I hear you breathing.\n\n"}};
            switch (kind)
            {
            case LineKind::Caption: return caption;
            case LineKind::Question: return question;
            case LineKind::GoalDone:
            case LineKind::GoalFailed: return goal;
            case LineKind::Turn: return turn;
            case LineKind::Taken: return taken;
            }
            return caption;
        }

        constexpr std::array<const char *, 9> exampleNames{"Co Hoa", "Ong Lam", "Chi Tam", "Hoa", "Lam", "Tam", "Paper Bride", "Rusted Usher", "Last Echo"};

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // Whole words only: " " + lower letters + " ", so "her," and "Her" both count as "her".
        std::string Words(const std::string &text)
        {
            std::string words = " " + Lower(text) + " ";
            for (char &c : words)
                c = (c >= 'a' && c <= 'z') ? c : ' ';
            return words;
        }

        bool Has(const std::string &words, const std::string &phrase) { return words.find(Words(phrase)) != std::string::npos; }

        std::string Name(const NightDef &def, int guest, bool taken)
        {
            const GuestDef &g = def.guests[static_cast<std::size_t>(guest)];
            return taken ? g.taken : g.name;
        }

        // The given name: the last word of "Vo Minh Khang".
        std::string Given(const std::string &name) { return name.substr(name.find_last_of(' ') + 1); }

        std::string Tidy(std::string line)
        {
            const auto drop = [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '"'; };
            while (!line.empty() && drop(line.back()))
                line.pop_back();
            std::size_t first = 0;
            while (first < line.size() && drop(line[first]))
                ++first;
            return line.substr(first);
        }
    }

    umbra::town::MindRequest Request(const NightDef &def, const LineAsk &ask)
    {
        const Kind &kind = KindOf(ask.kind);
        const GuestDef &speaker = def.guests[static_cast<std::size_t>(ask.speaker)];
        umbra::town::MindRequest request;
        // 1. The task, then the examples in an order that turns with the seed.
        request.prompt = std::string(kind.task) + " Reply with the reworded line only.\n\n";
        for (std::size_t i = 0; i < 3; ++i)
            request.prompt += kind.examples[(ask.seed + i) % 3];
        // 2. Ours: who speaks and how, who it is about, the line.
        if (ask.kind == LineKind::Caption)
            request.prompt += "Speaker: narrator\n";
        else if (ask.kind == LineKind::Turn)
            request.prompt += "Speaker: " + speaker.name + " (" + speaker.voice + "), now " + speaker.taken + " (" + speaker.takenVoice + ")\n";
        else
            request.prompt += "Speaker: " + Name(def, ask.speaker, ask.taken) + " (" + (ask.taken ? speaker.takenVoice : speaker.voice) + ")\n";
        std::string about;
        for (const int guest : ask.about)
        {
            const GuestDef &g = def.guests[static_cast<std::size_t>(guest)];
            const std::string name = ask.line.find(g.taken) != std::string::npos ? g.taken : g.name;
            about += (about.empty() ? "About: " : ", ") + name + " (" + g.pronoun + ")";
        }
        request.prompt += (about.empty() ? "" : about + "\n") + "Line: " + ask.line + "\nReworded:";
        // 3. One plain sentence (two for the Turn): a capital, letters and light punctuation, no digits.
        const std::string sentence = "[A-Z] [a-zA-Z ,:'-]{8,110} [.!?]";
        request.grammar = "root ::= \" \"? " + sentence + (ask.kind == LineKind::Turn ? " \" \" " + sentence : "") + "\n";
        request.maxTokens = ask.kind == LineKind::Turn ? 80 : 40;
        request.seed = ask.seed;
        return request;
    }

    std::string Guard(const NightDef &def, const LineAsk &ask, const std::string &answer)
    {
        const std::string line = Tidy(answer);
        const std::size_t longest = ask.kind == LineKind::Turn ? 230 : 120;
        // 1. Plain short text: letters, spaces and light punctuation; a capital first, an end mark last.
        if (line.size() < 8 || line.size() > longest || !std::isupper(static_cast<unsigned char>(line.front())) || std::string(".!?").find(line.back()) == std::string::npos)
            return {};
        for (const char c : line)
            if (!std::isalpha(static_cast<unsigned char>(c)) && std::string(" ,:'-.!?").find(c) == std::string::npos)
                return {};
        // 2. Names: every guest or Taken named in the template is still named in full; no other
        //    guest, Taken or example person appears, even by a given name alone.
        const std::string words = Words(line), was = Words(ask.line);
        for (std::size_t g = 0; g < def.guests.size(); ++g)
            for (const bool taken : {false, true})
            {
                const std::string name = Name(def, static_cast<int>(g), taken);
                if (Has(was, name) && !Has(words, name))
                    return {};
                const bool speaker = static_cast<int>(g) == ask.speaker;
                if (!Has(was, name) && !speaker && (Has(words, name) || (!taken && Has(words, Given(name)) && !Has(was, Given(name)))))
                    return {};
            }
        for (const char *name : exampleNames)
            if (Has(words, name) && !Has(was, name))
                return {};
        // 3. "He" and "she" only for someone the line is about, or the speaker (guests.csv).
        const auto someone = [&](const char *pronoun) {
            if (ask.kind != LineKind::Caption && def.guests[static_cast<std::size_t>(ask.speaker)].pronoun == pronoun)
                return true;
            return std::any_of(ask.about.begin(), ask.about.end(), [&](int g) { return def.guests[static_cast<std::size_t>(g)].pronoun == pronoun; });
        };
        for (const char *word : {"he", "him", "his", "himself"})
            if (Has(words, word) && !someone("he"))
                return {};
        for (const char *word : {"she", "her", "hers", "herself"})
            if (Has(words, word) && !someone("she"))
                return {};
        // 4. Words the template does not have and the line must not add: a flipped meaning, an
        //    outcome before the dice, a stat or a hint at the easier answer.
        static const char *const flips[] = {"not", "never", "no", "nothing", "nobody", "cannot"};
        static const char *const outcomes[] = {"dies", "died", "dead", "kills", "killed", "misses", "missed", "hits", "wins", "won", "loses", "lost", "fails", "failed", "succeeds", "wounds", "wounded"};
        static const char *const hints[] = {"might", "sanity", "speed", "easy", "easier", "hard", "harder", "safe", "safer", "risky"};
        for (const char *word : flips)
            if (Has(words, word) != Has(was, word) && (ask.kind == LineKind::Caption || ask.kind == LineKind::Question || ask.kind == LineKind::GoalDone || ask.kind == LineKind::GoalFailed))
                return {};
        if (ask.kind == LineKind::Caption || ask.kind == LineKind::Taken)
            for (const char *word : outcomes)
                if (Has(words, word) && !Has(was, word))
                    return {};
        if (ask.kind == LineKind::Question)
            for (const char *word : hints)
                if (Has(words, word) && !Has(was, word))
                    return {};
        return line;
    }
}
