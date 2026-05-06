#include "wforge/level.h"
#include "wforge/assets.h"
#include "wforge/colorpalette.h"
#include "wforge/save.h"
#include "wforge/scene.h"
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <cmath>
#include <proxy/v4/proxy.h>
#include <string_view>

namespace wf::scene {

namespace {

enum PausedMenuButton {
	RESUME = 0,
	RETRY,
	HELP,
	QUIT,
	BUTTON_COUNT
};

auto loadFont() {
	static PixelFont *font = nullptr;
	if (!font) {
		font = &AssetsManager::instance().getAsset<PixelFont>("font");
	}
	return font;
}

constexpr int hint_fade_speed = 3;
constexpr int hint_max_opacity = 200;

enum HintType {
	None = 0,
	RestartLevel,
	QuitLevel,
};

std::string_view hintTextOf(int type) {
	switch (type) {
	case HintType::RestartLevel:
		return "Press R again to retry";

	case HintType::QuitLevel:
		return "Press ESC again to quit";

	default:
		return "";
	}
}

} // namespace

LevelPlaying::LevelPlaying(const std::string &level_id)
	: LevelPlaying(
		  Level::loadFromMetadata(
			  AssetsManager::instance().getAsset<LevelMetadata>(
				  "level/" + level_id
			  )
		  )
	  ) {}

LevelPlaying::LevelPlaying(Level level)
	: _tick(0)
	, _paused(false)
	, _show_help(false)
	, _paused_menu_current_button_index(PausedMenuButton::RESUME)
	, _level(std::move(level))
	, _renderer(_level)
	, _hint_type(HintType::None)
	, _hint_opacity(0)
	, font(*loadFont()) {
	_help_texture = &AssetsManager::instance().getAsset<sf::Texture>("ui/help");
}

std::array<int, 2> LevelPlaying::size() const {
	return {_level.width(), _level.height()};
}

void LevelPlaying::setup(SceneManager &mgr) {
	mgr.setWindowTitle(
		std::format(
			"Level {} - {}", _level.metadata.index + 1, _level.metadata.name
		)
	);
	_level.selectItem(0);
	BGMManager::instance().setCollection("background/level-music");
	const auto &bgm_fade = FadeIOConfig::load();
	BGMManager::instance().fadeInCurrent(
		bgm_fade.fade_in_ticks, bgm_fade.fade_in_starting_volume
	);
}

void LevelPlaying::pause(SceneManager &mgr) noexcept {
	_paused = true;
	BGMManager::instance().setVolume(0.15f);
}

void LevelPlaying::unpause(SceneManager &mgr) noexcept {
	_paused = false;
	BGMManager::instance().setVolume(1.0f);
}

void LevelPlaying::handleEvent(SceneManager &mgr, sf::Event &ev) {
	if (_paused) {
		if (auto kb = ev.getIf<sf::Event::KeyPressed>()) {
			if (_show_help) {
				switch (kb->code) {
				case sf::Keyboard::Key::Escape:
				case sf::Keyboard::Key::Enter:
				case sf::Keyboard::Key::Space:
					UISounds::instance().backward.play();
					_show_help = false;
					return;

				default:
					break;
				}
			} else {
				switch (kb->code) {
				case sf::Keyboard::Key::Escape:
					unpause(mgr);
					break;

				case sf::Keyboard::Key::Enter:
				case sf::Keyboard::Key::Space:
					switch (_paused_menu_current_button_index) {
					case PausedMenuButton::RESUME:
						unpause(mgr);
						return;

					case PausedMenuButton::HELP:
						_show_help = true;
						return;

					case PausedMenuButton::RETRY:
						_restartLevel(mgr, false);
						return;

					case PausedMenuButton::QUIT:
						mgr.changeScene(
							pro::make_proxy<SceneFacade, LevelSelectionMenu>()
						);
						return;

					default:
						// Erroneous state?
						_paused_menu_current_button_index = PausedMenuButton::
							RESUME;
						break;
					}
					break;

				case sf::Keyboard::Key::Up:
				case sf::Keyboard::Key::W:
					UISounds::instance().backward.play();
					_paused_menu_current_button_index--;
					if (_paused_menu_current_button_index < 0) {
						_paused_menu_current_button_index
							= PausedMenuButton::BUTTON_COUNT - 1;
					}
					break;

				case sf::Keyboard::Key::Down:
				case sf::Keyboard::Key::S:
					UISounds::instance().forward.play();
					_paused_menu_current_button_index++;
					if (_paused_menu_current_button_index
					    >= PausedMenuButton::BUTTON_COUNT) {
						_paused_menu_current_button_index = 0;
					}
					break;

				default:
					break;
				}
			}
		}

		// Ignore other events when paused
		return;
	}

	constexpr int retry_cooldown_ticks = 24 * 2;

	if (auto mw = ev.getIf<sf::Event::MouseWheelScrolled>()) {
		if (mw->delta > 0) {
			_level.changeActiveItemBrushSize(1);
		} else if (mw->delta < 0) {
			_level.changeActiveItemBrushSize(-1);
		}
	}

	if (auto mb = ev.getIf<sf::Event::MouseButtonPressed>()) {
		auto mouse_pos = mgr.mousePosition();
		if (mb->button == sf::Mouse::Button::Left) {
			_level.useActiveItem(mouse_pos.x, mouse_pos.y, mgr.scale());
		}
	}

	if (auto kb = ev.getIf<sf::Event::KeyPressed>()) {
		constexpr int min_num_key = static_cast<int>(sf::Keyboard::Key::Num1);
		constexpr int max_num_key = static_cast<int>(sf::Keyboard::Key::Num9);
		constexpr int min_numpad_key = static_cast<int>(
			sf::Keyboard::Key::Numpad1
		);
		constexpr int max_numpad_key = static_cast<int>(
			sf::Keyboard::Key::Numpad9
		);

		const int key_code = static_cast<int>(kb->code);
		if (key_code >= min_num_key && key_code <= max_num_key) {
			int index = key_code - min_num_key;
			_level.selectItem(index);
			return;
		}
		if (key_code >= min_numpad_key && key_code <= max_numpad_key) {
			int index = key_code - min_numpad_key;
			_level.selectItem(index);
			return;
		}

		switch (kb->code) {
		case sf::Keyboard::Key::R:
			if (_tick < retry_cooldown_ticks) {
				break;
			}

			if (_hint_opacity > 0 && _hint_type == HintType::RestartLevel) {
				_restartLevel(mgr, false);
				return;
			} else {
				_hint_type = HintType::RestartLevel;
				_hint_opacity = hint_max_opacity;
			}
			break;

		case sf::Keyboard::Key::Escape:
			pause(mgr);
			break;

		case sf::Keyboard::Key::Up:
		case sf::Keyboard::Key::PageUp:
		case sf::Keyboard::Key::W:
			_level.prevItem();
			break;

		case sf::Keyboard::Key::Down:
		case sf::Keyboard::Key::PageDown:
		case sf::Keyboard::Key::S:
			_level.nextItem();
			break;

		default:
			break;
		}
	}
}

void LevelPlaying::_restartLevel(SceneManager &mgr, bool is_failed) {
	int duck_x = std::round(_level.duck.position.x);
	int duck_y = std::round(_level.duck.position.y);

	if (is_failed) {
		// duck is out of world bounds
		// place it to center of the level for animation
		int duck_w = _level.duck.width();
		int duck_h = _level.duck.height();
		duck_x = (_level.width() - duck_w) / 2;
		duck_y = (_level.height() - duck_h) / 2;
	}

	mgr.changeScene(
		pro::make_proxy<SceneFacade, DuckDeath>(
			_level.width(), _level.height(), duck_x, duck_y,
			LevelMetadata(_level.metadata)
		)
	);
	return;
}

void LevelPlaying::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	auto mouse_pos = mgr.mousePosition();
	_renderer.render(target, mouse_pos.x, mouse_pos.y, scale);

	if (_hint_opacity > 0) {
		auto hint_text = hintTextOf(_hint_type);
		int text_width = hint_text.size() * font.charWidth();
		int x = (_level.width() - text_width) / 2;
		int y = _level.height() - font.charHeight() - 10;
		sf::Color text_color = ui_text_color(_hint_opacity);
		font.renderText(target, hint_text, text_color, x, y, scale);
	}

	// Render Paused Menu
	if (_paused) {
		sf::RectangleShape overlay_mask;
		overlay_mask.setSize(
			sf::Vector2f(_level.width() * scale, _level.height() * scale)
		);
		overlay_mask.setFillColor(sf::Color(0, 0, 0, 200));
		target.draw(overlay_mask);

		// Render "PAUSED" text
		const std::string pause_text = "PAUSED";
		int x = (_level.width() - pause_text.size() * font.charWidth() * 2) / 2;
		int y = 64;
		font.renderText(
			target, pause_text, sf::Color(255, 255, 255, 255), x, y, scale, 2
		);

		// Render Buttons
		auto renderButton = [&](std::string_view label,
		                        const ButtonDescriptor &desc, bool is_active) {
			sf::Color color = is_active ? ui_active_color
										: ui_text_bright_color(255);
			font.renderText(
				target, std::string(label), color, desc.x, desc.y, scale,
				desc.size
			);
		};

		const auto _button_list = std::array<
			std::pair<std::string, int>, PausedMenuButton::BUTTON_COUNT>{
			std::make_pair("Resume", PausedMenuButton::RESUME),
			std::make_pair("Retry", PausedMenuButton::RETRY),
			std::make_pair("Help", PausedMenuButton::HELP),
			std::make_pair("Quit", PausedMenuButton::QUIT)
		};

		for (std::size_t i = 0; i < _button_list.size(); i++) {
			const auto button = _button_list[i];

			// Center the buttons horizontally
			int button_x = (_level.width()
			                - button.first.size() * font.charWidth())
				/ 2;
			int button_y = (_level.height() / 2) + i * (font.charHeight() + 5);

			renderButton(
				button.first, ButtonDescriptor{button_x, button_y, 1},
				_paused_menu_current_button_index == button.second
			);
		}
	}

	// Render help Overlay
	if (_show_help) {
		sf::Sprite help_sprite(*_help_texture);
		help_sprite.setPosition(sf::Vector2f(0, 0));
		help_sprite.setScale(sf::Vector2f(scale, scale));

		target.draw(help_sprite);
	}
}

void LevelPlaying::step(SceneManager &mgr) {
	// Only step the level when not paused
	if (!_paused) {
		_tick += 1;
		_level.step();
	}

	if (_hint_opacity > 0) {
		_hint_opacity -= hint_fade_speed;

		if (_hint_opacity <= 0) {
			_hint_opacity = 0;
			_hint_type = HintType::None;
		}
	}

	if (_level.isFailed()) {
		_restartLevel(mgr);
		return;
	}

	if (_level.isCompleted()) {
		auto &save = SaveData::instance();
		if (save.completed_levels < _level.metadata.index + 1) {
			save.completed_levels = _level.metadata.index + 1;
			save.save();
		}

		mgr.changeScene(
			pro::make_proxy<SceneFacade, LevelComplete>(
				_level.width(), _level.height(),
				std::round(_level.duck.position.x),
				std::round(_level.duck.position.y)
			)
		);
		return;
	}
}

} // namespace wf::scene
