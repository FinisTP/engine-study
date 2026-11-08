// UmbraTool policy-check: is a learned enemy policy usable with this RPG content (contract, fingerprint, actions)?

#include <ToolCommand.hpp>

#include <PolicyModel.hpp>
#include <RpgContent.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;
    using umbra::tool::Arguments;
    using umbra::tool::Report;
    using umbra::tool::Usage;
    using Json = nlohmann::json;

    // A learned enemy model: readable on its own, and still fitting this content (same checks as the game).
    int PolicyCheck(const Arguments &args)
    {
        if (args.positional.size() != 1 || !args.Has("content"))
            return Usage();
        std::vector<Diagnostic> diagnostics;
        umbra::rpg::Content content;
        umbra::policy::PolicyModel model;
        if (umbra::rpg::LoadContent(args.Path("content"), content, diagnostics) && umbra::policy::LoadPolicy(args.positional[0], model, diagnostics))
            umbra::policy::CheckCompatible(model, content, args.positional[0], diagnostics);
        return Report(diagnostics, 1, args.json);
    }
}

UMBRA_TOOL_COMMAND(PolicyCheck, "policy-check", "policy-check <policy file> --content <rpg file> [--json]", PolicyCheck);
