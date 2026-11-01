#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <Version.hpp>
#include <FrameClock.hpp>
#include <FixedStepAccumulator.hpp>
#include <Log.hpp>

#include <Platform.hpp>
#include <Input.hpp>
#include <EventPump.hpp>
#include <Window.hpp>
#include <ApplicationEvents.hpp>
#include <CrashHandler.hpp>
#include <Renderer.hpp>
#include <World.hpp>
#include <ModelInstance.hpp>
#include <SceneFile.hpp>

#include "ModelLibrary.hpp"

#include <glm/gtc/constants.hpp>

namespace
{
	// Loads a scene file into a fresh world. The current world is replaced only on success,
	// so a broken file never leaves a half-loaded scene on screen.
	bool LoadSceneFile(std::unique_ptr<umbra::world::World> &world, ModelLibrary &models, const std::string &scenePath)
	{
		umbra::scene::SceneLoadOptions options;
		options.assetRoot = UMBRA_ASSET_DIRECTORY;
		options.models = [&](const std::string &path) -> const umbra::assets::Model * {
			const LoadedModel *loaded = models.Load(path);
			return loaded ? &loaded->model : nullptr;
		};

		auto fresh = std::make_unique<umbra::world::World>();
		std::vector<umbra::core::Diagnostic> diagnostics;
		const bool loaded = umbra::scene::LoadScene(options.assetRoot / scenePath, options, *fresh, diagnostics);
		for (const umbra::core::Diagnostic &diagnostic : diagnostics)
		{
			if (diagnostic.severity == umbra::core::Severity::Error)
				umbra::diagnostics::Error(umbra::core::Format(diagnostic));
			else
				umbra::diagnostics::Info(umbra::core::Format(diagnostic));
		}
		if (!loaded)
		{
			umbra::diagnostics::Error("Scene " + scenePath + " was not loaded; keeping the current scene.");
			return false;
		}

		umbra::scene::BindRenderables(*fresh, [&](const std::string &model, std::uint32_t mesh) { return models.Resolve(model, mesh); });
		world = std::move(fresh);
		umbra::diagnostics::Info("Loaded scene " + scenePath + " with " + std::to_string(world->Count()) + " entities.");
		return true;
	}

	void SpinEntity(umbra::world::World &world, const char *id, float radians)
	{
		const umbra::world::Entity entity = world.Find(id);
		if (const umbra::world::Transform *current = world.GetLocalTransform(entity))
		{
			umbra::world::Transform next = *current;
			next.rotation = glm::angleAxis(radians, glm::vec3{0.0F, 1.0F, 0.0F}) * next.rotation;
			world.SetLocalTransform(entity, next);
		}
	}
}

int RunApplication(const std::string &scenePath)
{
	umbra::platform::Platform platform;

	umbra::diagnostics::Info(std::string{"Starting Umbra "} + std::string{umbra::core::Version()});

	{
		if (!platform.Initialize())
		{
			return 1;
		}

		{
			umbra::core::FrameClock clock;
			umbra::core::FixedStepAccumulator accumulator(1.0 / 60.0); // Fixed step of 1/60 seconds
			umbra::platform::Input input;
			umbra::platform::ApplicationEvents applicationEvents;
			umbra::platform::EventPump eventPump;

			{
				umbra::platform::Window window;
				if (!window.Create("Umbra", 1280, 720))
				{
					platform.Shutdown();
					return 1;
				}

				umbra::render::Renderer renderer;
				if (!renderer.Initialize(window))
				{
					return 1;
				}

				ModelLibrary models(renderer, UMBRA_ASSET_DIRECTORY, UMBRA_CACHE_DIRECTORY);
				auto world = std::make_unique<umbra::world::World>();
				umbra::core::DrawList drawList;
				LoadSceneFile(world, models, scenePath);

				bool isSuspended = false;

				while (true)
				{
					input.BeginFrame();
					applicationEvents.BeginFrame();
					eventPump.PumpEvents(input, applicationEvents);

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::Quit) ||
						applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowClose) ||
						input.IsKeyPressed(umbra::platform::InputKey::Escape))
					{
						break;
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowResize))
					{
						const int newWidth = applicationEvents.GetWindowWidth();
						const int newHeight = applicationEvents.GetWindowHeight();
						window.UpdateSizeFromEvent(newWidth, newHeight);
						renderer.RequestResize();
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowPixelSizeChanged))
					{
						renderer.RequestResize();
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowFocusGained))
					{
						// Handle window focus gained event
						umbra::diagnostics::Info("Window focus gained.");
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowFocusLost))
					{
						// Handle window focus lost event
						umbra::diagnostics::Info("Window focus lost.");
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowSuspended))
					{
						// Handle window suspended event
						umbra::diagnostics::Info("Window suspended.");
						isSuspended = true;
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowResumed))
					{
						// Handle window resumed event
						umbra::diagnostics::Info("Window resumed.");
						clock.ResetClock();
						accumulator.Reset();
						renderer.RequestResize();
						isSuspended = false;
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::Terminated))
					{
						// Handle application termination event
						umbra::diagnostics::Info("Application terminated.");
						break;
					}

					if (isSuspended)
					{
						// If the application is suspended, skip the update and rendering loop
						std::this_thread::sleep_for(std::chrono::milliseconds{100});
						continue;
					}

					clock.Tick();
					const double deltaTime = clock.GetDeltaTimeSeconds();
					accumulator.AddDeltaTime(deltaTime);

					while (accumulator.MarkFixedStep())
					{
						// Perform fixed-step updates here (like physics)
					}

					const double interpolationFactor = accumulator.GetInterpolationFactor();
					(void)interpolationFactor;

					// World -> draw list -> renderer. The renderer never sees the world.
					SpinEntity(*world, "spinner", static_cast<float>(deltaTime) * glm::radians(45.0F));
					world->UpdateTransforms();
					drawList.items.clear();
					world->ExtractDrawList(drawList);

					if (!renderer.RenderFrame(input, static_cast<float>(deltaTime), drawList))
					{
						return 1;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds{1});
				}
			}
		}
	}

	platform.Shutdown();
	return 0;
}

int main(int argc, char **argv)
{
	// Optional first argument: scene path relative to the asset folder.
	const std::string scenePath = argc > 1 ? argv[1] : "scenes/sample.scene.json";

	umbra::diagnostics::Initialize();
	umbra::diagnostics::InstallCrashHandlers();

	const int result = RunApplication(scenePath);

	umbra::diagnostics::UninstallCrashHandlers();
	umbra::diagnostics::Shutdown();
	return result;
}
