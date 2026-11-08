#include "EditorPanel.hpp"

namespace umbra::editor
{
    namespace
    {
        // Function-local, so registrations from other files' static objects work in any order.
        std::vector<PanelFactory> &Factories()
        {
            static std::vector<PanelFactory> factories;
            return factories;
        }
    }

    bool RegisterPanel(PanelFactory factory)
    {
        Factories().push_back(std::move(factory));
        return true;
    }

    std::vector<std::unique_ptr<EditorPanel>> CreatePanels()
    {
        std::vector<std::unique_ptr<EditorPanel>> panels;
        for (const PanelFactory &factory : Factories())
            panels.push_back(factory());
        return panels;
    }
}
