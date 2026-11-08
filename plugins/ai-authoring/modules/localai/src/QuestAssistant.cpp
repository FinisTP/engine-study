#include <QuestAssistant.hpp>

namespace umbra::localai
{
    ChatRequest MakeDraftRequest(const authoring::Json &rpg, const std::string &npc, const std::string &idea, const std::string &model,
                                 const std::string &host)
    {
        ChatRequest request;
        request.host = host;
        request.model = model;
        request.format = authoring::DraftSchema();
        request.messages.push_back({"system", authoring::DraftInstructions(rpg, npc)});
        request.messages.push_back({"user", "Quest idea: " + idea});
        return request;
    }

    void AddFixRequest(ChatRequest &request, const std::string &previousAnswer, const std::vector<core::Diagnostic> &problems)
    {
        std::string list;
        for (const core::Diagnostic &d : problems)
            if (d.severity == core::Severity::Error)
                list += "- " + (d.field.empty() ? "" : d.field + ": ") + d.message + "\n";
        request.messages.push_back({"assistant", previousAnswer});
        request.messages.push_back({"user", "These problems were found in your draft:\n" + list +
                                                "Return the whole corrected draft, keeping everything that was fine."});
    }

    std::string PreferredModel(const std::vector<std::string> &installed, const std::string &wanted)
    {
        for (const std::string &name : installed)
            if (name == wanted || name == wanted + ":latest")
                return name;
        for (const std::string &name : installed)
            if (name.starts_with("qwen"))
                return name;
        return installed.empty() ? wanted : installed.front();
    }

    bool ParseDraft(const std::string &text, authoring::Json &draft, std::vector<core::Diagnostic> &diagnostics)
    {
        draft = authoring::Json::parse(text, nullptr, false);
        if (draft.is_object())
            return true;
        diagnostics.push_back({core::Severity::Error, "draft.json", "draft", "", "The model's answer is not a JSON object. Try again, or a different model."});
        return false;
    }
}
