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
#include <Renderer.hpp>
#include <SoundBank.hpp>
#include <World.hpp>
#include "Benchmark.hpp"
#include "ModelLibrary.hpp"
#include "SceneRuntime.hpp"
#include "GameFlow.hpp"
#include "GameUi.hpp"
#include <RpgContent.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

	// Where the game's files are, found at run time (no build path is compiled in, P9). A package
	// has assets/ next to the program (its plugins' assets merged in) and keeps its model cache
	// there too. A development build looks upwards: the build folder (it has CMakeCache.txt) holds
	// the cache, the source tree above it holds projects/<project>/assets/ and
	// plugins/<plugin>/assets/ (only the names are compiled in, not their paths).
	struct GameFolders final
	{
		umbra::assets::AssetFolders assets;
		std::filesystem::path cache;
	};
	GameFolders FindFolders()
	{
		std::filesystem::path program = umbra::platform::Platform::ProgramFolder().lexically_normal();
		if (!program.has_filename())
			program = program.parent_path(); // "C:/Game/" -> "C:/Game"
		const auto isAssets = [](const std::filesystem::path &dir) { return std::filesystem::is_directory(dir / "scenes"); };
		if (isAssets(program / "assets"))
			return {program / "assets", program / "asset-cache"};
		GameFolders found{program / "assets", program / "asset-cache"};
		bool cacheFound = false;
		for (std::filesystem::path dir = program; !dir.empty(); dir = dir.parent_path())
		{
			if (!cacheFound && std::filesystem::exists(dir / "CMakeCache.txt"))
				found.cache = dir / "asset-cache", cacheFound = true;
			if (const auto project = dir / "projects" / UMBRA_PROJECT_NAME / "assets"; isAssets(project))
			{
				// The enabled plugins' assets, below the project's ("a|b|c" from the build).
				found.assets = project;
				const std::string plugins = UMBRA_GAME_PLUGINS;
				for (std::size_t start = 0; start < plugins.size();)
				{
					const std::size_t end = std::min(plugins.find('|', start), plugins.size());
					if (const auto folder = dir / "plugins" / plugins.substr(start, end - start) / "assets"; std::filesystem::is_directory(folder))
						found.assets.AddPlugin(folder);
					start = end + 1;
				}
				break;
			}
			if (dir == dir.parent_path())
				break;
		}
		return found;
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

int RunApplication(std::string scenePath, std::optional<float> benchmarkSeconds, bool offerContinue)
{
	// Benchmark (P9): a scripted walk through the reference areas, then a report; no RPG layer.
	std::optional<Benchmark> benchmark;
	if (benchmarkSeconds)
	{
		benchmark.emplace(*benchmarkSeconds);
		scenePath = benchmark->Area();
	}
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
				if (!window.Create("Umbra", benchmark ? 1920 : 1280, benchmark ? 1080 : 720))
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
				const GameFolders folders = FindFolders();
				umbra::diagnostics::Info("Assets: " + folders.assets.string());
				umbra::audio::SoundBank sounds(folders.assets);
				umbra::platform::AudioDevice audioDevice;
				(void)audioDevice.Open(mixer); // without a device the game runs silently
				std::vector<std::string> soundProblems;
				const auto reportSounds = [&] {
					for (const umbra::core::Diagnostic &problem : sounds.TakeDiagnostics())
					{
						umbra::diagnostics::Error(umbra::core::Format(problem));
						soundProblems.push_back("Warning: sound " + problem.file + ": " + problem.message);
					}
				};
				const auto play = [&](const std::string &path, float volume, umbra::audio::Bus bus) {
					mixer.Play(sounds.Get(path), volume, bus);
					reportSounds();
				};
				int musicGeneration = -1; // the scene whose music is playing

				ModelLibrary models(renderer, folders.assets, folders.cache);
				SceneRuntime scene(models, folders.assets, scenePath);
				umbra::core::DrawList drawList;
				bool showColliders = false;
				// The RPG layer (story, fights, saves) needs the content file; without it the
				// world is still explorable.
				umbra::rpg::Content content;
				std::optional<GameFlow> flow;
				{
					std::vector<umbra::core::Diagnostic> diagnostics;
					const std::filesystem::path contentFile = folders.assets / "rpg/reference.rpg.json";
					if (!benchmark && umbra::rpg::LoadContent(contentFile, content, diagnostics))
					{
						std::filesystem::path saves = umbra::platform::Platform::PreferencesFolder();
						saves = saves.empty() ? folders.cache / "saves" : saves / "saves";
						flow.emplace(content, contentFile, scene, saves, scenePath, offerContinue);
						umbra::diagnostics::Info("Saves go to " + saves.string());
					}
					for (const umbra::core::Diagnostic &diagnostic : diagnostics)
						umbra::diagnostics::Error(umbra::core::Format(diagnostic));
				}
				scene.Load(); // after the RPG layer, so enemies already get their models

				// Menus and the HUD are screens (engine/ui, the rpg-ui plugin; GameFlow.md "Screens").
				// The pause, inventory, skills and quests screens stop the game; the mouse steers the
				// camera only while no screen takes the input.
				GameUi gameUi(renderer, folders.assets);
				const auto setPaused = [&](bool paused) {
					if (paused)
						gameUi.Screens().Open("pause");
					else
						gameUi.Screens().Close("pause");
				};
				const auto frozen = [&] {
					for (const char *screen : {"pause", "inventory", "skills", "quests", "hotkeys"})
						if (gameUi.Screens().IsOpen(screen))
							return true;
					return false;
				};
				bool mouseCaptured = true;
				window.SetMouseCaptured(true);
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
						if (!benchmark)
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
						umbra::platform::Platform::Idle(100);
						continue;
					}

					clock.Tick();
					const umbra::core::Stopwatch frameWork; // this frame's work, without the idle sleep
					const double deltaTime = clock.GetDeltaTimeSeconds();
					frameTime.Add(deltaTime * 1000.0);
					umbra::core::Stopwatch section;

					// Pick up edited scene/model files.
					scene.Update();
					reloadTime.Add(section.Lap());
					using umbra::platform::InputKey;

					// Screens: Esc / Start opens the pause screen while none takes the input (on an open
					// screen Esc / B is its "back"); I, K, J open the inventory, skills and quests.
					bool quit = false;
					const auto [pixelWidth, pixelHeight] = renderer.Extent();
					umbra::ui::UiInput uiInput = GameUi::MapInput(input, {static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())},
																  {static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)});
					if (!gameUi.Screens().TakesInput() && !benchmark)
					{
						if (input.IsKeyPressed(InputKey::Escape) || input.IsKeyPressed(InputKey::PadStart))
						{
							setPaused(true);
							uiInput.back = false; // the same press must not close it again
							play("audio/ui_click.wav", 0.6F, umbra::audio::Bus::Ui);
						}
						else if (flow && flow->IsExploring())
							for (const auto &[key, screen] : {std::pair{InputKey::I, "inventory"}, std::pair{InputKey::K, "skills"}, std::pair{InputKey::J, "quests"}, std::pair{InputKey::H, "hotkeys"}})
								if (input.IsKeyPressed(key))
								{
									gameUi.Screens().Open(screen);
									gameUi.Screens().ResetFocus(screen);
									play("audio/ui_click.wav", 0.6F, umbra::audio::Bus::Ui);
								}
					}
					else if (uiInput.up || uiInput.down || uiInput.left || uiInput.right)
						play("audio/ui_click.wav", 0.3F, umbra::audio::Bus::Ui);

					// Playing: keyboard and mouse -> player intent -> fixed-step simulation.
					// Music: each scene's "music" component; a new scene (portal, arena, restart) crossfades.
					if (scene.Generation() != musicGeneration)
					{
						musicGeneration = scene.Generation();
						const auto [track, volume] = scene.Music();
						mixer.PlayMusic(track.empty() ? nullptr : sounds.GetMusic(track), volume, 1.5F);
						reportSounds();
					}

					const umbra::world::Interactable *focused = nullptr;
					if (!frozen())
					{
						SpinEntity(scene.GetWorld(), "spinner", static_cast<float>(deltaTime) * glm::radians(45.0F));
						if (flow)
						{
							flow->Update(input, static_cast<float>(deltaTime));
							for (const std::string &sound : flow->TakeSounds()) // hits, skills, items
								play(sound, 0.8F, umbra::audio::Bus::Game);
						}
						const bool canMove = !flow || flow->PlayerCanMove();
						scene.GetWorld().UpdateTransforms();

						const auto axis = [&](InputKey plus, InputKey minus) {
							return (input.IsKeyHeld(plus) ? 1.0F : 0.0F) - (input.IsKeyHeld(minus) ? 1.0F : 0.0F);
						};
						umbra::gameplay::PlayerInput playerInput;
						if (benchmark)
						{
							playerInput = benchmark->Input();
							scene.Look(Benchmark::TurnRadiansPerSecond() * static_cast<float>(deltaTime), 0.0F);
						}
						else if (canMove)
						{
							playerInput.move = {axis(InputKey::D, InputKey::A), axis(InputKey::W, InputKey::S)};
							playerInput.sprint = input.IsKeyHeld(InputKey::LeftShift);
							playerInput.jump = input.IsKeyPressed(InputKey::Space);
						}
						constexpr float mouseSensitivity = 0.0025F; // radians per pixel
						if (!benchmark)
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
						const std::filesystem::path checkpointDirectory = folders.cache / "checkpoint";
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

					umbra::core::Hud &hud = drawList.hud;

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
					// The screens: the RPG's data plus the prompt, news and asset problems; then what was chosen.
					nlohmann::json uiData = flow ? flow->UiData() : nlohmann::json::object();
					// Target arrows: each target's point above its head, on the screen.
					if (uiData.contains("fight") && uiData["fight"].contains("targets"))
						for (nlohmann::json &target : uiData["fight"]["targets"])
						{
							const auto world = target["world"].get<std::vector<float>>();
							if (const auto at = GameUi::ScreenPoint(drawList.camera, {static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)}, {world[0], world[1], world[2]}))
								target["at"] = {at->x, at->y};
						}
					if (focused != nullptr && !uiData.contains("prompt") && (!flow || flow->IsExploring()))
						uiData["prompt"] = "[E] " + focused->prompt;
					if (messageSeconds > 0.0F)
						uiData["message"] = message;
					std::vector<std::string> warnings;
					AddWarnings(warnings, scene.GetProblems(), 6);
					AddWarnings(warnings, soundProblems, 3);
					AddWarnings(warnings, gameUi.Problems(), 3);
					uiData["warnings"] = warnings;
					if (!gameUi.Ready())
						hud.warnings = warnings; // without screens, at least say why
					for (const umbra::ui::UiAction &action : gameUi.Screens().Update(uiData, {static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)}, uiInput, drawList.ui))
					{
						play("audio/ui_click.wav", 0.6F, umbra::audio::Bus::Ui);
						if (flow && flow->HandleUi(action.screen, action.action, action.arg))
							continue;
						if (action.action == "resume")
							setPaused(false);
						else if (action.action == "restart")
						{
							mixer.StopAll();
							scene.Restart();
							messageSeconds = 0.0F;
							setPaused(false);
						}
						else if (action.action == "quit")
							quit = true;
					}
					if (quit)
						break;
					gameUi.ShowGameScreen(flow ? flow->Screen() : "");
					// Mouse and game sound follow the screens.
					if (const bool capture = !gameUi.Screens().TakesInput() && !frozen(); capture != mouseCaptured)
					{
						window.SetMouseCaptured(capture);
						mouseCaptured = capture;
					}
					mixer.SetGamePaused(frozen());
					hud.state = nlohmann::json{{"screens", gameUi.Screens().OpenScreens()}, {"hero", uiData.value("hero", nlohmann::json::object())},
										   {"mode", uiData.value("mode", "")}}
									 .dump();

					if (!renderer.RenderFrame(static_cast<float>(deltaTime), drawList))
					{
						return 1;
					}
					renderTime.Add(section.Lap()); // CPU side only; includes waiting for a free frame

					// Benchmark: record the frame; next area or the report when its time is up.
					if (benchmark && benchmark->AddFrame(static_cast<float>(deltaTime), deltaTime * 1000.0, frameWork.Milliseconds(), scene.GetWorld().Count()))
					{
						if (!benchmark->Finished())
						{
							scene.ChangeScene(benchmark->Area(), "player");
							continue;
						}
						const auto [width, height] = renderer.Extent();
						bool passed = false;
						const std::string report = benchmark->Report({std::string{umbra::core::Version()}, renderer.DeviceName(), width, height,
																	  umbra::platform::Platform::CpuThreads(), umbra::platform::Platform::SystemRamMb(),
																	  umbra::platform::Platform::PeakMemoryMb()},
																	 passed);
						std::filesystem::path file = umbra::platform::Platform::PreferencesFolder();
						file = (file.empty() ? folders.cache : file) / "benchmark.json";
						std::ofstream(file) << report;
						std::cout << report;
						umbra::diagnostics::Info("Benchmark " + std::string{passed ? "within" : "OVER"} + " budget; report: " + file.string());
						break;
					}

#ifndef __EMSCRIPTEN__
					std::this_thread::sleep_for(std::chrono::milliseconds{1}); // browser: RenderFrame already waited for the next frame
#endif
				}
			}
		}
	}

	platform.Shutdown();
	return 0;
}

int main(int argc, char **argv)
{
	// Optional: a scene path relative to the asset folder; --benchmark [seconds per area] (P9).
	std::string scenePath = "scenes/settlement.scene.json";
	std::optional<float> benchmarkSeconds;
	bool sceneGiven = false; // the editor's Play names a scene: start there, no "Continue" menu
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--benchmark")
		{
			benchmarkSeconds = 20.0F;
			if (i + 1 < argc && std::atof(argv[i + 1]) > 0.0)
				benchmarkSeconds = static_cast<float>(std::atof(argv[++i]));
		}
		else
			scenePath = arg, sceneGiven = true;
	}

	umbra::diagnostics::Initialize();
	umbra::diagnostics::InstallCrashHandlers();

	const int result = RunApplication(scenePath, benchmarkSeconds, !sceneGiven);

	umbra::diagnostics::UninstallCrashHandlers();
	umbra::diagnostics::Shutdown();
	return result;
}
