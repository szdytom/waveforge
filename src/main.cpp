#include "wforge/assets.h"
#include "wforge/save.h"
#include "wforge/scene.h"
#include "wforge/version.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <argparse/argparse.hpp>
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>
#include <iostream>
#include <proxy/proxy.h>

void entry(
	const std::string &level_id, int scale_config, bool is_first_launch,
	const std::string &debug_js_scene = ""
);

std::filesystem::path wf::_executable_path;
int main(int argc, char **argv) {
	if (argc > 0) {
		try {
			wf::_executable_path = std::filesystem::absolute(argv[0]);
		} catch (const std::exception &e) {
			std::cerr << "Warning: could not determine executable path: "
					  << e.what() << "\n";
			wf::_executable_path = std::filesystem::current_path();
		}
	} else {
		// fallback: current working directory
		wf::_executable_path = std::filesystem::current_path();
	}

	CPPTRACE_TRY {
		wf::AssetsManager::loadAllAssets();
		wf::SaveData::instance(); // load / initialize save data
	}
	CPPTRACE_CATCH(const std::exception &e) {
		std::cerr << "Failed to load assets: " << e.what() << "\n";
		cpptrace::from_current_exception().print();
		return 1;
	}

	auto &save = wf::SaveData::instance();

	argparse::ArgumentParser program(
		"waveforge", WAVEFORGE_VERSION, argparse::default_arguments::help
	);

	program.add_argument("level")
		.help("Level ID to load (- for main menu)")
		.default_value("-");

	program.add_argument("--scale")
		.help("Set rendering scale (0 for automatic)")
		.default_value(save.user_settings.scale)
		.scan<'i', int>();

	program.add_argument("--debug-js-scene")
		.help("Load a ScriptedScene from a JS asset ID")
		.default_value(std::string(""));

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &e) {
		std::cerr << e.what() << "\n";
		std::cerr << program;
		return 1;
	}

	CPPTRACE_TRY {
		entry(
			program.get<std::string>("level"), program.get<int>("--scale"),
			save.isFirstLaunch(), program.get<std::string>("--debug-js-scene")
		);
	}
	CPPTRACE_CATCH(const std::exception &e) {
		std::cerr << "Unhandled exception: " << e.what() << "\n";
		cpptrace::from_current_exception().print();
		return 1;
	}

	return 0;
}

void entry(
	const std::string &level_id, int scale_config, bool is_first_launch,
	const std::string &debug_js_scene
) {
	auto initialScene = [&](const std::string &level_id, bool is_first_launch) {
		if (!debug_js_scene.empty()) {
			return pro::make_proxy<wf::SceneFacade, wf::ScriptedScene>(
				debug_js_scene
			);
		}
		if (level_id == "-") {
			if (is_first_launch) {
				return pro::make_proxy<wf::SceneFacade, wf::scene::Help>();
			} else {
				return pro::make_proxy<wf::SceneFacade, wf::scene::MainMenu>();
			}
		} else {
			return pro::make_proxy<wf::SceneFacade, wf::scene::LevelPlaying>(
				level_id
			);
		}
	};

	wf::SceneManager scene_mgr(
		initialScene(level_id, is_first_launch), scale_config
	);

	if (wf::BGMManager::instance().isEmpty()) {
		wf::BGMManager::instance().setCollection("background/main-menu-music");
	}

	auto &window = scene_mgr.window;

	while (window.isOpen()) {
		while (auto ev = window.pollEvent()) {
			// window closed
			if (ev->is<sf::Event::Closed>()) {
				return;
			}

			scene_mgr.handleEvent(*ev);
		}

		scene_mgr.tick();
	}
}
