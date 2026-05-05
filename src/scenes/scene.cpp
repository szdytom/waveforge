#include "wforge/scene.h"
#include "wforge/assets.h"
#include "wforge/version.h"
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Window.hpp>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>

namespace wf {

void UITextDescriptor::render(
	sf::RenderTarget &target, const PixelFont &font, std::string_view text,
	int scale
) const {
	font.renderText(target, text, color, x, y, scale, size);
}

UITextDescriptor UITextDescriptor::fromJson(const nlohmann::json &json_data) {
	UITextDescriptor desc;
	desc.x = json_data.at("x");
	desc.y = json_data.at("y");
	desc.size = json_data.at("size");
	desc.color = sf::Color(
		json_data.at("color").at(0), json_data.at("color").at(1),
		json_data.at("color").at(2), json_data.at("color").at(3)
	);
	return desc;
}

namespace {

sf::RenderWindow createWindow(Scene &scene, int scale) {
	auto [width, height] = scene->size();
	scale = automaticScale(width, height, scale);
	sf::Vector2u window_size(width * scale, height * scale);
	sf::RenderWindow window(
		sf::VideoMode(window_size), "Waveforge " WAVEFORGE_VERSION,
		sf::Style::Titlebar | sf::Style::Close
	);
	window.setFramerateLimit(24);
	return window;
}

#ifdef WAVEFORGE_DEMO_VIDEO

std::FILE *demoVideoFile() {
	static std::FILE *file = nullptr;
	if (file == nullptr) {
		file = std::fopen("demo_video_frames.dat", "wb");
		std::atexit([]() {
			if (file != nullptr) {
				std::fclose(file);
			}
		});
	}
	return file;
}

#endif

} // namespace

int automaticScale(int width, int height, int scale_configured) {
	if (scale_configured > 0) {
		return scale_configured;
	}

	auto player_screen_size = sf::VideoMode::getDesktopMode().size;
	return std::max<int>(
		1,
		std::min<int>(
			player_screen_size.x / width - 1, player_screen_size.y / height - 1
		)
	);
}

SceneManager::SceneManager(Scene initial_scene, int scale)
	: window(createWindow(initial_scene, scale))
	, bgm(BGMManager::instance())
	, _current_scene(std::move(initial_scene))
	, _scene_changed(false)
	, _config_scale(scale) {
	auto [width, height] = _current_scene->size();
	_scale = automaticScale(width, height, scale);
	std::cerr << std::format(
		"Screen size {}x{}, using scale {}x\n", width, height, _scale
	);
	_current_scene->setup(*this);
}

SceneManager::~SceneManager() {
	window.close();
}

void SceneManager::changeScene(Scene new_scene) {
	auto [old_width, old_height] = _current_scene->size();
	_current_scene = std::move(new_scene);
	auto [width, height] = _current_scene->size();
	if (width != old_width || height != old_height) {
		window.close();
		_scale = automaticScale(width, height, _config_scale);
		std::cerr << std::format(
			"Screen size changed to {}x{}, using scale {}x\n", width, height,
			_scale
		);
		window = createWindow(_current_scene, _config_scale);
	}
	_current_scene->setup(*this);
	_scene_changed = true;
}

void SceneManager::setWindowTitle(std::string title) {
	window.setTitle(title);
}

void SceneManager::handleEvent(sf::Event &evt) {
	_current_scene->handleEvent(*this, evt);
}

void SceneManager::tick() {
	_scene_changed = false;
	_current_scene->step(*this);
	if (_scene_changed) {
		return;
	}

	BGMManager::instance().step();
	ActiveSoundManager::instance().step();
	window.clear(sf::Color::White);
	_current_scene->render(*this, window, _scale);

#ifdef WAVEFORGE_DEMO_VIDEO
	auto [width, height] = _current_scene->size();
	sf::RenderTexture render_texture(sf::Vector2u(width, height));
	render_texture.clear(sf::Color::White);
	_current_scene->render(*this, render_texture, 1);

	auto image = render_texture.getTexture().copyToImage();
	auto file = demoVideoFile();
	std::fwrite(image.getPixelsPtr(), 1, width * height * 4, file);
#endif

	window.display();
}

sf::Vector2i SceneManager::mousePosition() const {
	return sf::Mouse::getPosition(window) / _scale;
}

} // namespace wf
