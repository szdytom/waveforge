#include "wforge/assets.h"
#include "wforge/audio.h"
#include "wforge/colorpalette.h"
#include "wforge/save.h"
#include "wforge/scene.h"
#include <cstdlib>
#include <format>
#include <nlohmann/json.hpp>
#include <string_view>

namespace wf::scene {

namespace {

enum MainMenuButton {
	PLAY = 0,
	SETTINGS,
	HELP,
	EXIT,
	BUTTON_COUNT
};

} // namespace

MainMenu::MainMenu()
	: font(AssetsManager::instance().getAsset<PixelFont>("font"))
	, _current_button_index(MainMenuButton::PLAY) {
	const auto &json_data = AssetsManager::instance().getAsset<nlohmann::json>(
		"ui-config/main-menu"
	);

	_width = json_data.at("width");
	_height = json_data.at("height");

	const auto &textures = json_data.at("textures");
	_background_texture = &AssetsManager::instance().getAsset<sf::Texture>(
		textures.at("background")
	);

	if (_background_texture->getSize().x != _width
	    || _background_texture->getSize().y != _height) {
		throw std::runtime_error(
			std::format(
				"Main menu background texture size mismatch: "
				"configured {}x{}, got {}x{}",
				_width, _height, _background_texture->getSize().x,
				_background_texture->getSize().y
			)
		);
	}

	const auto &buttons = json_data.at("buttons");

	auto parseButtonDescriptor = [](const nlohmann::json &data) {
		ButtonDescriptor desc;
		desc.x = data.at("x");
		desc.y = data.at("y");
		desc.size = data.at("size");
		return desc;
	};

	_play_button = parseButtonDescriptor(buttons.at("play"));
	_settings_button = parseButtonDescriptor(buttons.at("settings"));
	_help_button = parseButtonDescriptor(buttons.at("help"));
	_exit_button = parseButtonDescriptor(buttons.at("exit"));

	_version_text = UITextDescriptor::fromJson(json_data.at("version-text"));
}

std::array<int, 2> MainMenu::size() const {
	return {_width, _height};
}

void MainMenu::setup(SceneManager &mgr) {
	mgr.setWindowTitle("Waveforge " WAVEFORGE_VERSION);
	mgr.bgm.setCollection("background/main-menu-music");
}

void MainMenu::handleEvent(SceneManager &mgr, sf::Event &evt) {
	if (auto kb = evt.getIf<sf::Event::KeyPressed>()) {
		switch (kb->code) {
		case sf::Keyboard::Key::Up:
		case sf::Keyboard::Key::W:
			UISounds::instance().backward.play();
			_current_button_index--;
			if (_current_button_index < 0) {
				_current_button_index = MainMenuButton::BUTTON_COUNT - 1;
			}
			break;

		case sf::Keyboard::Key::Down:
		case sf::Keyboard::Key::S:
			UISounds::instance().forward.play();
			_current_button_index++;
			if (_current_button_index >= MainMenuButton::BUTTON_COUNT) {
				_current_button_index = 0;
			}
			break;

		case sf::Keyboard::Key::Enter:
		case sf::Keyboard::Key::Space:
			switch (_current_button_index) {
			case MainMenuButton::PLAY:
				mgr.changeScene(
					pro::make_proxy<SceneFacade, LevelSelectionMenu>()
				);
				return;

			case MainMenuButton::SETTINGS:
				mgr.changeScene(pro::make_proxy<SceneFacade, SettingsMenu>());
				return;

			case MainMenuButton::HELP:
				mgr.changeScene(pro::make_proxy<SceneFacade, Help>());
				return;

			case MainMenuButton::EXIT:
				std::exit(0);
				return;

			default:
				// Erroneous state?
				_current_button_index = MainMenuButton::PLAY;
				break;
			}
			break;

		case sf::Keyboard::Key::Escape:
			std::exit(0);
			return;

		default:
			break;
		}
	}
}

void MainMenu::step(SceneManager &mgr) {
	// No-op
}

void MainMenu::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	// Render background
	sf::Sprite bg_sprite(*_background_texture);
	bg_sprite.setPosition(sf::Vector2f(0, 0));
	bg_sprite.setScale(sf::Vector2f(scale, scale));
	target.draw(bg_sprite);

	// Render buttons
	auto renderButton = [&](std::string_view label,
	                        const ButtonDescriptor &desc, bool is_active) {
		sf::Color color = is_active ? ui_active_color : ui_text_color(255);
		font.renderText(
			target, std::string(label), color, desc.x, desc.y, scale, desc.size
		);
	};

	auto &save = SaveData::instance();
	renderButton(
		save.is_first_launch() ? "New Game" : "Play", _play_button,
		_current_button_index == MainMenuButton::PLAY
	);

	renderButton(
		"Settings", _settings_button,
		_current_button_index == MainMenuButton::SETTINGS
	);

	renderButton(
		"Help", _help_button, _current_button_index == MainMenuButton::HELP
	);

	renderButton(
		"Exit", _exit_button, _current_button_index == MainMenuButton::EXIT
	);

#ifdef WAVEFORGE_GIT_COMMIT_HASH
	constexpr std::string_view version_str = "V" WAVEFORGE_VERSION
											 "-" WAVEFORGE_GIT_COMMIT_HASH;
#else
	constexpr std::string_view version_str = "V" WAVEFORGE_VERSION;
#endif
	_version_text.render(target, font, version_str, scale);
}

} // namespace wf::scene
