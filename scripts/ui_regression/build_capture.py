"""Relink a capture entry point without modifying either source tree."""

import json
import pathlib
import shlex
import subprocess


def build_capture(root: pathlib.Path, build: pathlib.Path, out: pathlib.Path) -> pathlib.Path:
    out.mkdir(parents=True, exist_ok=True)
    legacy = (root / 'src/save.cpp').exists()
    source = (root / 'src/main.cpp').read_text()
    source = '#include "wforge/xoroshiro.h"\n#include <sstream>\n#include <map>\n#include <cstdlib>\n' + source
    source = source.replace('int main(int argc, char **argv) {', 'int main(int argc, char **argv) {\nsf::Listener::setGlobalVolume(0);\nwf::Xoroshiro128PP::globalInstance() = wf::Xoroshiro128PP(wf::Seed::from_string("ui-comparison"));')
    if legacy:
        marker = '\t\tif (level_id == "-") {'
        dispatch = '''
            if (const char *capture_scene = std::getenv("WF_SCENE")) {
                std::string scene(capture_scene);
                if (scene == "main-menu") return pro::make_proxy<wf::SceneFacade, wf::scene::MainMenu>();
                if (scene == "settings") return pro::make_proxy<wf::SceneFacade, wf::scene::SettingsMenu>();
                if (scene == "help") return pro::make_proxy<wf::SceneFacade, wf::scene::Help>();
                if (scene == "credits") return pro::make_proxy<wf::SceneFacade, wf::scene::Credits>();
                if (scene == "level-menu") return pro::make_proxy<wf::SceneFacade, wf::scene::LevelSelectionMenu>();
                if (scene == "play-level") return pro::make_proxy<wf::SceneFacade, wf::scene::LevelPlaying>("hello");
                if (scene == "load-level") return pro::make_proxy<wf::SceneFacade, wf::scene::LevelLoading>(256, 192, wf::LevelMetadata(wf::AssetsManager::instance().getAsset<wf::LevelMetadata>("level/hello")));
                if (scene == "level-death") return pro::make_proxy<wf::SceneFacade, wf::scene::DuckDeath>(256, 192, 40, 60, wf::LevelMetadata(wf::AssetsManager::instance().getAsset<wf::LevelMetadata>("level/hello")));
                if (scene == "level-complete") return pro::make_proxy<wf::SceneFacade, wf::scene::LevelComplete>(256, 192, 40, 60);
            }
    '''
    else:
        marker = '\t\tif (!debug_js_scene.empty()) {'
        dispatch = '''
            if (const char *capture_scene = std::getenv("WF_SCENE")) {
                const char *data = std::getenv("WF_DATA");
                return router.create(capture_scene, data ? data : "");
            }
    '''
    if marker not in source:
        raise RuntimeError(f'Capture insertion point missing in {root / "src/main.cpp"}')
    source = source.replace(marker, dispatch + marker, 1)
    marker = '\twhile (window.isOpen()) {'
    setup = '''
        sf::Mouse::setPosition({-100, -100}, window);
        sf::Event pointer_event = sf::Event::MouseMoved{.position = {-100, -100}};
        scene_mgr.handleEvent(pointer_event);
        int capture_tick = 0;
        int capture_end = std::atoi(std::getenv("WF_END"));
        std::map<int, sf::Keyboard::Key> capture_keys;
        std::map<std::string, sf::Keyboard::Key> key_map = {
            {"esc", sf::Keyboard::Key::Escape}, {"enter", sf::Keyboard::Key::Enter},
            {"up", sf::Keyboard::Key::Up}, {"down", sf::Keyboard::Key::Down},
            {"left", sf::Keyboard::Key::Left}, {"right", sf::Keyboard::Key::Right},
            {"r", sf::Keyboard::Key::R}, {"x", sf::Keyboard::Key::X},
            {"y", sf::Keyboard::Key::Y}, {"z", sf::Keyboard::Key::Z}
        };
        if (const char *keys = std::getenv("WF_KEYS")) {
            std::istringstream stream(keys);
            std::string token;
            while (std::getline(stream, token, ',')) {
                auto split = token.find(':');
                capture_keys[std::stoi(token.substr(0, split))] = key_map.at(token.substr(split + 1));
            }
        }
    '''
    source = source.replace(marker, setup + marker, 1)
    marker = '\t\tscene_mgr.tick();'
    body = '''
            scene_mgr.handleEvent(pointer_event);
            ++capture_tick;
            if (capture_keys.contains(capture_tick)) {
                sf::Event event = sf::Event::KeyPressed{.code = capture_keys.at(capture_tick)};
                scene_mgr.handleEvent(event);
            }
            scene_mgr.tick();
            if (capture_tick == capture_end) {
                sf::Texture snapshot(window.getSize());
                snapshot.update(window);
                if (!snapshot.copyToImage().saveToFile(std::getenv("WF_OUTPUT"))) std::exit(2);
                return;
            }
    '''
    if marker not in source:
        raise RuntimeError(f'Capture insertion point missing in {root / "src/main.cpp"}')
    source = source.replace(marker, body, 1)
    main = out / 'capture-main.cpp'
    main.write_text(source)
    commands = json.loads((build / 'compile_commands.json').read_text())
    command = next(c for c in commands if pathlib.Path(c['file']) == root / 'src/main.cpp')
    args = shlex.split(command['command'])
    args[args.index('-o') + 1] = str(out / 'capture-main.o')
    args[-1] = str(main)
    subprocess.run(args, cwd=build, check=True)
    link = subprocess.check_output(['ninja', '-t', 'commands', 'waveforge'], cwd=build, text=True).splitlines()[-1]
    args = shlex.split(link)
    # CMake wraps the linker in shell no-ops.
    args = args[args.index('&&') + 1:args.index('&&', args.index('&&') + 1)] if '&&' in args else args
    args = [str(out / 'capture-main.o') if a == 'CMakeFiles/waveforge.dir/src/main.cpp.o' else a for a in args]
    args[args.index('-o') + 1] = str(out / 'waveforge-capture')
    subprocess.run(args, cwd=build, check=True)
    return out / 'waveforge-capture'
