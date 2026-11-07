#include <chrono>
#include <string>
#include <thread>

#include <Version.hpp>
#include <FrameClock.hpp>
#include <Log.hpp>
#include <Profiling.hpp>

#include <Platform.hpp>
#include <Input.hpp>
#include <EventPump.hpp>
#include <Window.hpp>
#include <ApplicationEvents.hpp>
#include <CrashHandler.hpp>
#include <AudioDevice.hpp>
#include <Mixer.hpp>
#include <PauseMenu.hpp>
#include <Renderer.hpp>
#include <SoundBank.hpp>
#include <World.hpp>
#include "ModelLibrary.hpp"
#include "SceneRuntime.hpp"
#include "GameFlow.hpp"
#include <RpgContent.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <optional>

namespace
{
	// Shows at most `limit` lines and a count of the rest.
	void AddWarnings(std::vector<std::string> &out, const std::vector<std::string> &problems, std::size_t limit)
	{
		for (std::size_t i = 0; i < std::min(problems.size(), limit); ++i)
			out.push_back(problems[i]);
		if (problems.size() > limit)
			out.push_back("... and " + std::to_string(problems.size() - limit) + " more (see the log)");
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

				// Sound: the mixer outlives the device that pulls from it (declared first, destroyed last).
				umbra::audio::Mixer mixer;
				umbra::audio::SoundBank sounds(UMBRA_ASSET_DIRECTORY);
				umbra::platform::AudioDevice audioDevice;
				(void)audioDevice.Open(mixer); // without a device the game runs silently
				std::vector<std::string> soundProblems;
				const auto play = [&](const std::string &path, float volume, umbra::audio::Bus bus) {
					mixer.Play(sounds.Get(path), volume, bus);
					for (const umbra::core::Diagnostic &problem : sounds.TakeDiagnostics())
					{
						umbra::diagnostics::Error(umbra::core::Format(problem));
						soundProblems.push_back("Warning: sound " + problem.file + ": " + problem.message);
					}
				};

				ModelLibrary models(renderer, UMBRA_ASSET_DIRECTORY, UMBRA_CACHE_DIRECTORY);
				SceneRuntime scene(models, UMBRA_ASSET_DIRECTORY, scenePath);
				umbra::core::DrawList drawList;
				bool showColliders = false;
				scene.Load();

				// The RPG layer (story, fights, saves) needs the content file; without it the
				// world is still explorable.
				umbra::rpg::Content content;
				std::optional<GameFlow> flow;
				{
					std::vector<umbra::core::Diagnostic> diagnostics;
					if (umbra::rpg::LoadContent(std::filesystem::path{UMBRA_ASSET_DIRECTORY} / "rpg/reference.rpg.json", content, diagnostics))
					{
						std::filesystem::path saves = umbra::platform::Platform::PreferencesFolder();
						saves = saves.empty() ? std::filesystem::path{UMBRA_CACHE_DIRECTORY} / "saves" : saves / "saves";
						flow.emplace(content, scene, saves, scenePath);
						umbra::diagnostics::Info("Saves go to " + saves.string());
					}
					for (const umbra::core::Diagnostic &diagnostic : diagnostics)
						umbra::diagnostics::Error(umbra::core::Format(diagnostic));
				}

				// Esc opens the pause menu; the mouse is captured (steers the camera) only while playing.
				umbra::gameplay::PauseMenu menu;
				const auto setPaused = [&](bool paused) {
					if (paused)
						menu.Open();
					else
						menu.Close();
					window.SetMouseCaptured(!paused);
					mixer.SetGamePaused(paused);
				};
				setPaused(false);
				std::string message; // shown after using an interactable
				float messageSeconds = 0.0F;

				// CPU time per frame section, shown in the diagnostics window (see docs/testing/phase4-sandbox.md).
				umbra::core::RollingTimer frameTime, reloadTime, simulateTime, extractTime, renderTime;
				int physicsSteps = 0;

				bool isSuspended = false;

				while (true)
				{
					input.BeginFrame();
					applicationEvents.BeginFrame();
					eventPump.PumpEvents(input, applicationEvents);

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::Quit) ||
						applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowClose))
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
						setPaused(true); // releases the mouse; the player resumes from the menu
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
					frameTime.Add(deltaTime * 1000.0);
					umbra::core::Stopwatch section;

					// Pick up edited scene/model files.
					scene.Update();
					reloadTime.Add(section.Lap());
					using umbra::platform::InputKey;

					// Pause menu: Esc toggles; W/S or arrows move; Enter chooses.
					bool quit = false;
					if (input.IsKeyPressed(InputKey::Escape) && !menu.IsOpen() && flow && flow->HandleEscape())
					{
						// used by the conversation or the death menu
					}
					else if (input.IsKeyPressed(InputKey::Escape))
					{
						setPaused(!menu.IsOpen());
						play("audio/ui_click.wav", 0.6F, umbra::audio::Bus::Ui);
					}
					else if (menu.IsOpen())
					{
						const int move = (input.IsKeyPressed(InputKey::S) || input.IsKeyPressed(InputKey::Down) ? 1 : 0) -
										 (input.IsKeyPressed(InputKey::W) || input.IsKeyPressed(InputKey::Up) ? 1 : 0);
						if (move != 0)
						{
							menu.Move(move);
							play("audio/ui_click.wav", 0.6F, umbra::audio::Bus::Ui);
						}
						if (input.IsKeyPressed(InputKey::Enter))
						{
							play("audio/ui_click.wav", 0.6F, umbra::audio::Bus::Ui);
							switch (menu.Confirm())
							{
							case umbra::gameplay::MenuAction::Restart:
								mixer.StopAll();
								scene.Restart();
								messageSeconds = 0.0F;
								[[fallthrough]];
							case umbra::gameplay::MenuAction::Resume:
								setPaused(false);
								break;
							case umbra::gameplay::MenuAction::Quit:
								quit = true;
								break;
							case umbra::gameplay::MenuAction::None:
								break;
							}
						}
					}
					if (quit)
						break;

					// Playing: keyboard and mouse -> player intent -> fixed-step simulation.
					const umbra::world::Interactable *focused = nullptr;
					if (!menu.IsOpen())
					{
						SpinEntity(scene.GetWorld(), "spinner", static_cast<float>(deltaTime) * glm::radians(45.0F));
						if (flow)
							flow->Update(input, static_cast<float>(deltaTime));
						const bool canMove = !flow || flow->PlayerCanMove();
						scene.GetWorld().UpdateTransforms();

						const auto axis = [&](InputKey plus, InputKey minus) {
							return (input.IsKeyHeld(plus) ? 1.0F : 0.0F) - (input.IsKeyHeld(minus) ? 1.0F : 0.0F);
						};
						umbra::gameplay::PlayerInput playerInput;
						if (canMove)
						{
							playerInput.move = {axis(InputKey::D, InputKey::A), axis(InputKey::W, InputKey::S)};
							playerInput.sprint = input.IsKeyHeld(InputKey::LeftShift);
							playerInput.jump = input.IsKeyPressed(InputKey::Space);
						}
						constexpr float mouseSensitivity = 0.0025F; // radians per pixel
						scene.Look(-input.GetMouseDeltaX() * mouseSensitivity, -input.GetMouseDeltaY() * mouseSensitivity);

						umbra::core::Stopwatch simulateWatch;
						const SimulationEvents events = scene.Simulate(static_cast<float>(deltaTime), playerInput);
						simulateTime.Add(simulateWatch.Milliseconds());
						physicsSteps = events.steps;
						for (int i = 0; i < events.footsteps; ++i)
							play("audio/footstep.wav", 0.5F, umbra::audio::Bus::Game);
						if (events.jumped)
							play("audio/jump.wav", 0.5F, umbra::audio::Bus::Game);

						// Walking into a portal loads its area; the old one is released entirely.
						const auto showMessage = [&](std::string text) {
							message = std::move(text);
							messageSeconds = 3.0F;
						};
						if (events.portal && (!flow || flow->AllowsPortals()))
						{
							mixer.StopAll();
							if (!scene.ChangeScene(events.portal->scene, events.portal->spawn))
								showMessage("Cannot enter " + events.portal->scene + " (see warnings)");
							else if (flow)
								flow->OnAreaEntered(); // autosave at every area change
						}

						// Development keys: F5 checkpoint, F9 back to it, R restarts the area.
						const std::filesystem::path checkpointDirectory = std::filesystem::path{UMBRA_CACHE_DIRECTORY} / "checkpoint";
						if (input.IsKeyPressed(InputKey::F5))
							showMessage(scene.SaveCheckpoint(checkpointDirectory) ? "Checkpoint saved" : "Checkpoint failed (see log)");
						if (input.IsKeyPressed(InputKey::F9))
						{
							mixer.StopAll();
							showMessage(scene.LoadCheckpoint(checkpointDirectory) ? "Checkpoint loaded" : "No usable checkpoint (see log)");
						}
						if (input.IsKeyPressed(InputKey::R))
						{
							mixer.StopAll();
							scene.Restart();
							showMessage("Area restarted");
						}

						// Interaction: E uses what the player faces; its message stays up for a few seconds.
						// Conversations and save stones go to the RPG layer, only while exploring.
						focused = !flow || flow->IsExploring() ? scene.FocusedInteractable() : nullptr;
						if (focused != nullptr && input.IsKeyPressed(InputKey::E))
						{
							play(focused->sound.empty() ? "audio/interact.wav" : focused->sound, 0.8F, umbra::audio::Bus::Game);
							if (!flow || !flow->Interact(*focused))
								showMessage(focused->message);
						}
						if (flow)
							if (std::string news = flow->TakeMessage(); !news.empty())
							{
								message = std::move(news);
								messageSeconds = 4.0F;
							}
						messageSeconds = std::max(0.0F, messageSeconds - static_cast<float>(deltaTime));
					}
					if (input.IsKeyPressed(InputKey::F1))
						showColliders = !showColliders;

					section.Lap(); // extraction starts here
					drawList.Clear();
					drawList.camera = scene.GetCamera();
					scene.GetWorld().ExtractDrawList(drawList); // fetched now: restart/portal/checkpoint replace the world
					if (showColliders)
						scene.AddDebugLines(drawList);

					// Screen text: prompt, message, asset problems, menu.
					umbra::core::Hud &hud = drawList.hud;
					if (focused != nullptr)
						hud.prompt = "[E] " + focused->prompt;
					if (messageSeconds > 0.0F)
						hud.message = message;
					AddWarnings(hud.warnings, scene.GetProblems(), 6);
					AddWarnings(hud.warnings, soundProblems, 3);

					// Timings (average / worst over ~2 s) and live resource counts.
					const auto timing = [](const char *name, const umbra::core::RollingTimer &timer) {
						char line[96];
						std::snprintf(line, sizeof(line), "%-9s %6.2f ms avg %6.2f max", name, timer.Average(), timer.Max());
						return std::string{line};
					};
					hud.stats = {timing("Frame", frameTime), timing("Reload", reloadTime), timing("Simulate", simulateTime),
								 timing("Extract", extractTime), timing("Render", renderTime),
								 "Physics steps: " + std::to_string(physicsSteps) +
									 " | bodies " + std::to_string(scene.GetPhysics().BodyCount()) +
									 " | characters " + std::to_string(scene.GetPhysics().CharacterCount()),
								 "Entities: " + std::to_string(scene.GetWorld().Count()) +
									 " | models " + std::to_string(models.Count()),
								 "Sounds: " + std::to_string(mixer.VoiceCount()) + " playing | " +
									 std::to_string(sounds.Count()) + " cached" + (audioDevice.IsOpen() ? "" : " (no device)"),
								 "Area: " + scene.GetScenePath()};
					extractTime.Add(section.Lap());
					if (flow)
						flow->FillHud(hud); // status lines, conversation, fight and death menus
					if (menu.IsOpen())
					{
						hud.menuText.clear();
						hud.menuHint.clear();
						hud.menuDim = true;
						hud.menuTitle = "Paused";
						hud.menuItems.assign(umbra::gameplay::PauseMenu::items.begin(), umbra::gameplay::PauseMenu::items.end());
						hud.menuSelected = menu.GetSelected();
					}

					if (!renderer.RenderFrame(static_cast<float>(deltaTime), drawList))
					{
						return 1;
					}
					renderTime.Add(section.Lap()); // CPU side only; includes waiting for a free frame

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
	const std::string scenePath = argc > 1 ? argv[1] : "scenes/settlement.scene.json";

	umbra::diagnostics::Initialize();
	umbra::diagnostics::InstallCrashHandlers();

	const int result = RunApplication(scenePath);

	umbra::diagnostics::UninstallCrashHandlers();
	umbra::diagnostics::Shutdown();
	return result;
}
