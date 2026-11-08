#pragma once

#include <TextureAssistant.hpp>

#include <string>
#include <vector>

// The ComfyUI address and checkpoint, shared by the AI texture and AI model windows.
struct ComfySettings final
{
    std::string host, checkpoint;
    std::vector<std::string> checkpoints;
    std::vector<umbra::core::Diagnostic> problems;
    bool searched = false;

    // Asks the usual local ports (or the typed address) which checkpoints ComfyUI has.
    void Find()
    {
        problems.clear();
        if (host.empty())
            host = umbra::localai::FindComfyUi(checkpoints);
        else
            checkpoints = umbra::localai::ListCheckpoints(host, problems);
        if (host.empty())
            problems.push_back({umbra::core::Severity::Error, "ai.connect", "comfyui", "",
                                "ComfyUI is not running on port 8000 (Desktop) or 8188 (command line). Start it, then press Find."});
        checkpoint = umbra::localai::PreferredCheckpoint(checkpoints, checkpoint);
        searched = true;
    }

    static ComfySettings &Shared()
    {
        static ComfySettings settings;
        return settings;
    }
};
