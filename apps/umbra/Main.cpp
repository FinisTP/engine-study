#include <chrono>
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

#include <glm/gtc/constants.hpp>

namespace
{
	// Temporary sample scene until scenes load from files (P3.6):
	// a ring of tinted cubes plus a spinning parent with an orbiting child.
	void BuildSampleWorld(umbra::world::World &world, umbra::core::MeshHandle mesh, umbra::core::TextureHandle texture)
	{
		using namespace umbra::world;
		const auto add = [&](const std::string &id, glm::vec3 position, float scale, glm::vec4 color) {
			const Entity entity = world.Create(id);
			Transform transform;
			transform.position = position;
			transform.scale = glm::vec3{scale};
			world.SetLocalTransform(entity, transform);
			world.SetRenderable(entity, Renderable{"scenes/phase2.gltf", 0, color, mesh, texture});
			return entity;
		};

		constexpr int ringCount = 6;
		for (int i = 0; i < ringCount; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / ringCount;
			const glm::vec4 color{0.5F + 0.5F * glm::cos(angle), 0.6F, 0.5F + 0.5F * glm::sin(angle), 1.0F};
			add("ring_" + std::to_string(i), {2.5F * glm::cos(angle), 0.0F, -2.0F + 2.5F * glm::sin(angle)}, 0.5F, color);
		}

		const Entity spinner = add("spinner", {0.0F, 0.0F, -2.0F}, 0.6F, glm::vec4{1.0F});
		const Entity moon = add("moon", {2.0F, 0.0F, 0.0F}, 0.4F, glm::vec4{1.0F, 0.8F, 0.4F, 1.0F});
		world.SetParent(moon, spinner);
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

int RunApplication()
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

				umbra::world::World world;
				umbra::core::DrawList drawList;
				BuildSampleWorld(world, renderer.GetSampleMesh(), renderer.GetSampleTexture());

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
					SpinEntity(world, "spinner", static_cast<float>(deltaTime) * glm::radians(45.0F));
					world.UpdateTransforms();
					drawList.items.clear();
					world.ExtractDrawList(drawList);

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

int main()
{
	umbra::diagnostics::Initialize();
	umbra::diagnostics::InstallCrashHandlers();

	const int result = RunApplication();

	umbra::diagnostics::UninstallCrashHandlers();
	umbra::diagnostics::Shutdown();
	return result;
}
