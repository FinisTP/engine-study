#pragma once

#include <Diagnostic.hpp>

#include <filesystem>
#include <vector>

namespace umbra::authoring
{
    // Release-gate check of a packaged game folder (roadmap P9, Package.md): the program and the
    // content it needs are there; no editor, tools, scripts, secrets or absolute paths are; every
    // shipped DLL has a license notice. `sourceRoot` (may be empty) is the project folder: the game
    // program must not depend on it. Appends diagnostics; true if there are no errors.
    bool CheckPackage(const std::filesystem::path &folder, const std::filesystem::path &sourceRoot, std::vector<core::Diagnostic> &diagnostics);
}
