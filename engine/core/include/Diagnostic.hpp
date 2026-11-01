#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace umbra::core
{
    enum class Severity
    {
        Warning,
        Error
    };

    // One problem found while validating content. Shared by the engine, the CLI tools
    // and (later) the editor, so all of them report the same thing the same way.
    struct Diagnostic final
    {
        Severity severity = Severity::Error;
        std::string code;    // stable machine-readable id, e.g. "entity.parent.missing"
        std::string file;    // file the problem is in
        std::string field;   // where in the file, e.g. "entities[3].parent"
        std::string message; // what is wrong and, when useful, how to fix it
    };

    [[nodiscard]] inline bool HasErrors(const std::vector<Diagnostic> &diagnostics)
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(),
                           [](const Diagnostic &d) { return d.severity == Severity::Error; });
    }

    // "file: error [code] field: message"
    [[nodiscard]] inline std::string Format(const Diagnostic &d)
    {
        std::string text = d.file.empty() ? "" : d.file + ": ";
        text += d.severity == Severity::Error ? "error" : "warning";
        text += " [" + d.code + "] ";
        if (!d.field.empty())
            text += d.field + ": ";
        return text + d.message;
    }
}
