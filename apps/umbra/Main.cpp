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
#include "GameFeel.hpp"
#include "GameFlow.hpp"
#include "GameUi.hpp"
#include "TouchControls.hpp"
#include <RpgContent.hpp>
#include <GamePack.hpp>

#if defined(__ANDROID__)
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_main.h> // the app package calls SDL_main (SDLActivity)
#endif

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <TextureTools.hpp>

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
	// Phones (P11, Mobile.md): the assets and shaders come inside the app package as game.pack;
	// on first start (and after an update: a new hash) they are written to the app's folder, which
	// then counts as the program folder. False only if a pack is there but cannot be unpacked.
	bool UnpackPackagedGame()
	{
#if defined(__ANDROID__)
		std::size_t indexSize = 0, packSize = 0;
		char *index = static_cast<char *>(SDL_LoadFile("game.pack.json", &indexSize)); // read from the APK
		if (index == nullptr)
			return true; // no pack: a development run
		const std::string indexText(index, indexSize);
		SDL_free(index);
		const std::filesystem::path target = umbra::platform::Platform::PreferencesFolder() / "game";
		umbra::platform::Platform::SetProgramFolder(target);
		if (umbra::assets::InstalledPackHash(target) == umbra::assets::PackHash(indexText))
			return true; // unpacked before
		umbra::diagnostics::Info("Unpacking the game's files to " + target.string());
		void *pack = SDL_LoadFile("game.pack", &packSize);
		std::string error = pack ? "" : std::string("game.pack missing from the app: ") + SDL_GetError();
		const bool ok = pack && umbra::assets::UnpackGame(indexText, std::span(static_cast<const std::byte *>(pack), packSize), target, error);
		SDL_free(pack);
		if (!ok)
			umbra::diagnostics::Error("Could not unpack the game: " + error);
		return ok;
#else
		return true;
#endif
	}

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

// --capture <png> [frames] [--hour <h>]: run, save one frame of the 3D view as a picture, quit (tests, docs; S6).
// --demo <skill>: a fight that plays itself with that skill; a capture then keeps the screens (S9).
struct CaptureOptions final
{
	std::filesystem::path file;
	int frames = 90;
	std::optional<float> hour;
	std::string demo;
};

int RunApplication(std::string scenePath, std::optional<float> benchmarkSeconds, bool offerContinue, const CaptureOptions &capture)
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
		// Android: the shaders and assets come out of the app before the renderer looks for them.
		if (!UnpackPackagedGame())
			return EXIT_FAILURE;

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
				int capturedFrames = 0;
				bool captured = false; // set by the capture callback, which runs when the frame is shown
				const auto frozen = [&] {
					for (const char *screen : {"pause", "inventory", "skills", "quests", "hotkeys"})
						if (gameUi.Screens().IsOpen(screen))
							return true;
					return false;
				};
				bool mouseCaptured = true;
				window.SetMouseCaptured(true);
				// Phones (P11, Mobile.md): on-screen stick, camera drag and buttons; a gamepad's sticks too.
				TouchControls touch;
				umbra::core::UiDrawList touchLayer;
				TouchControls::Output touchOutput;
				std::string message; // shown after using an interactable
				float messageSeconds = 0.0F;
				GameFeel feel; // numbers, freeze, shake and flash on blows (S9)

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

					// Phones (P11): leaving the screen destroys the window's surface, and the system may end
					// the app without warning; save, pause, let go of the surface. Back: a new surface.
					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::EnteredBackground))
					{
						if (flow)
							flow->SaveForSuspend();
						setPaused(true);
						renderer.ReleaseSurface();
					}
					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::EnteredForeground))
						renderer.RestoreSurface();

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::Terminated))
					{
						// Handle application termination event
						umbra::diagnostics::Info("Application terminated.");
						if (flow)
							flow->SaveForSuspend();
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
					const double realDeltaTime = clock.GetDeltaTimeSeconds();
					frameTime.Add(realDeltaTime * 1000.0);
					// A heavy blow freezes the game for a moment (hit-stop); the screens keep real time.
					const double deltaTime = realDeltaTime * static_cast<double>(feel.Update(static_cast<float>(realDeltaTime)));
					umbra::core::Stopwatch section;

					// Pick up edited scene/model files.
					scene.Update();
					reloadTime.Add(section.Lap());
					using umbra::platform::InputKey;

					// Screens: Esc / Start opens the pause screen while none takes the input (on an open
					// screen Esc / B is its "back"); I, K, J open the inventory, skills and quests.
					bool quit = false;
					const auto [pixelWidth, pixelHeight] = renderer.Extent();
					// Touch first: its buttons press keys that the rest of this frame reads.
					touchLayer.Clear();
					if (!benchmark)
						touchOutput = touch.Update(input, {static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)},
												   {flow ? flow->IsExploring() : true, flow && flow->InRealtimeFight(), gameUi.Screens().TakesInput() || frozen()},
												   gameUi.Screens().Fonts(), gameUi.AtlasTexture(), [&](const std::string &path) { return gameUi.Image(path); }, touchLayer);
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
							if (!capture.demo.empty())
								flow->Demo(capture.demo);
							flow->Update(input, static_cast<float>(deltaTime));
							for (const std::string &sound : flow->TakeSounds()) // hits, skills, items
								play(sound, 0.8F, umbra::audio::Bus::Game);
							for (const GameFlow::Impact &impact : flow->TakeImpacts())
								feel.Add(impact);
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
							playerInput.move += touchOutput.move + glm::vec2{input.Axis(0), -input.Axis(1)}; // touch stick, gamepad left stick
							if (glm::length(playerInput.move) > 1.0F)
								playerInput.move = glm::normalize(playerInput.move);
							playerInput.sprint = input.IsKeyHeld(InputKey::LeftShift);
							playerInput.jump = input.IsKeyPressed(InputKey::Space);
						}
						constexpr float mouseSensitivity = 0.0025F; // radians per pixel
						constexpr float touchSensitivity = 0.006F, stickTurn = 2.5F; // radians per dragged pixel / per second at full tilt
						if (!benchmark)
						{
							scene.Look(-input.GetMouseDeltaX() * mouseSensitivity, -input.GetMouseDeltaY() * mouseSensitivity);
							scene.Look(-touchOutput.look.x * touchSensitivity, -touchOutput.look.y * touchSensitivity);
							scene.Look(-input.Axis(2) * stickTurn * static_cast<float>(deltaTime), -input.Axis(3) * stickTurn * static_cast<float>(deltaTime));
						}

						umbra::core::Stopwatch simulateWatch;
						const SimulationEvents events = scene.Simulate(static_cast<float>(deltaTime), playerInput);
						simulateTime.Add(simulateWatch.Milliseconds());
						physicsSteps = events.steps;
						for (int i = 0; i < events.footsteps; ++i)
							play("audio/footstep.wav", 0.5F, umbra::audio::Bus::Game);
						for (const auto &[sound, volume] : events.sounds) // clip events: steps, swings (AnimGraph.md)
							play(sound, 0.6F * volume, umbra::audio::Bus::Game);
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

					// The time of day moves on while the game runs (the "environment" component's day length).
					if (capture.hour)
						scene.GetWorld().SetHour(*capture.hour);
					else if (!frozen())
						scene.GetWorld().AdvanceTime(static_cast<float>(deltaTime));

					section.Lap(); // extraction starts here
					drawList.Clear();
					drawList.camera = scene.GetCamera();
					feel.Shake(drawList.camera);
					scene.GetWorld().ExtractDrawList(drawList); // fetched now: restart/portal/checkpoint replace the world
					scene.ExtractEffects(drawList);
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
								 "Entities: " + std::to_string(scene.GetWorld().Count()) + " | particles " + std::to_string(scene.ParticleCount()) +
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
					if (gameUi.Ready())
						feel.Draw(drawList.camera, {static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)}, gameUi.Screens().Fonts(), gameUi.AtlasTexture(), drawList.ui);
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
					drawList.ui.Append(touchLayer); // over the HUD (hidden while a menu is open)
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

					// A capture: the 3D view only (no screens or diagnostics window); take the frame, save it, stop.
					if (!capture.file.empty())
					{
						drawList.hud.diagnosticsWindow = false;
						if (capture.demo.empty())
							drawList.ui.Clear();
						if (++capturedFrames == capture.frames)
							renderer.CaptureNextFrame([&](const umbra::core::TextureData &picture) {
								std::vector<std::uint8_t> png;
								captured = umbra::assets::EncodePng(picture, png) &&
										   static_cast<bool>(std::ofstream(capture.file, std::ios::binary).write(reinterpret_cast<const char *>(png.data()),
																												  static_cast<std::streamsize>(png.size())));
								umbra::diagnostics::Info((captured ? "Captured " : "Could not write ") + capture.file.string());
							});
					}
					if (!renderer.RenderFrame(static_cast<float>(realDeltaTime), drawList))
					{
						return 1;
					}
					if (captured || (!capture.file.empty() && capturedFrames > capture.frames + 5))
						return captured ? 0 : 1;
					renderTime.Add(section.Lap()); // CPU side only; includes waiting for a free frame

					// Benchmark: record the frame; next area or the report when its time is up.
					if (benchmark && benchmark->AddFrame(static_cast<float>(realDeltaTime), realDeltaTime * 1000.0, frameWork.Milliseconds(), scene.GetWorld().Count()))
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
	// Optional: a scene path relative to the asset folder; --benchmark [seconds per area] (P9);
	// --capture <png> [frames] and --hour <h> (S6).
	std::string scenePath = "scenes/settlement.scene.json";
	std::optional<float> benchmarkSeconds;
	CaptureOptions capture;
	bool sceneGiven = false; // the editor's Play names a scene: start there, no "Continue" menu
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--capture" && i + 1 < argc)
		{
			capture.file = argv[++i];
			if (i + 1 < argc && std::atoi(argv[i + 1]) > 0)
				capture.frames = std::atoi(argv[++i]);
		}
		else if (arg == "--hour" && i + 1 < argc)
			capture.hour = static_cast<float>(std::atof(argv[++i]));
		else if (arg == "--demo" && i + 1 < argc)
			capture.demo = argv[++i];
		else if (arg == "--benchmark")
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

	const int result = RunApplication(scenePath, benchmarkSeconds, !sceneGiven, capture);

	umbra::diagnostics::UninstallCrashHandlers();
	umbra::diagnostics::Shutdown();
	return result;
}
