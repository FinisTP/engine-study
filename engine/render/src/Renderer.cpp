#include <Renderer.hpp>

#include "VulkanInstance.hpp"
#include "VulkanPhysicalDevice.hpp"
#include "VulkanSurface.hpp"

#include <Log.hpp>
#include <Window.hpp>

#include <memory>
#include <utility>

namespace umbra::render
{
    struct Renderer::Implementation final
    {
        VulkanInstance instance;
        VulkanSurface surface;
        VulkanPhysicalDevice physicalDevice;
    };

    Renderer::Renderer() = default;

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(platform::Window &window)
    {
        if (implementation_ != nullptr)
        {
            diagnostics::Error("Cannot initialize renderer: it is already initialized.");
            return false;
        }

        auto implementation = std::make_unique<Implementation>();
        if (!implementation->instance.Initialize())
        {
            return false;
        }

        if (!implementation->surface.Initialize(window.GetNativeHandle(), implementation->instance.GetHandle()))
        {
            return false;
        }

        if (!implementation->physicalDevice.Initialize(
                implementation->instance.GetHandle(), implementation->surface.GetHandle()))
        {
            return false;
        }

        implementation_ = std::move(implementation);
        diagnostics::Info("Renderer initialized.");
        return true;
    }

    void Renderer::Shutdown() noexcept
    {
        if (implementation_ == nullptr)
        {
            return;
        }

        implementation_.reset();
        diagnostics::Info("Renderer shut down.");
    }
}
