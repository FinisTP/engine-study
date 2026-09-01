#include <Renderer.hpp>

#include "VulkanInstance.hpp"

#include <Log.hpp>

#include <memory>
#include <utility>

namespace umbra::render
{
    struct Renderer::Implementation final
    {
        VulkanInstance instance;
    };

    Renderer::Renderer() = default;

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize()
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
