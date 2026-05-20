#include "wforge/audio.h"
#include "wforge/colorpalette.h"
#include "wforge/save_io.h"
#include "wforge/scene.h"
#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace wf::scene {

struct SettingsMenu::Option {
	virtual ~Option() = default;

	virtual std::string displayText() const = 0;
	virtual std::string valueText() const {
		return "";
	}

	virtual bool handleEnter() {
		return false;
	}

	virtual void handleLeft() {}
	virtual void handleRight() {}
};

namespace {

struct ScaleOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Scale";
	}

	std::string valueText() const override {
		int scale = SaveKV::instance().getInt("settings.scale");
		if (scale == 0) {
			return "Auto";
		}
		return std::format("{}x", scale);
	}

	void handleLeft() override {
		int scale = SaveKV::instance().getInt("settings.scale");
		if (scale > 0) {
			SaveKV::instance().setInt("settings.scale", scale - 1);
		}
	}

	void handleRight() override {
		constexpr int max_scale = 12;
		int scale = SaveKV::instance().getInt("settings.scale");
		if (scale < max_scale) {
			SaveKV::instance().setInt("settings.scale", scale + 1);
		}
	}
};

struct VolumnOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Volume";
	}

	std::string valueText() const override {
		int volume = SaveKV::instance().getInt("settings.global_volume", 80);
		if (volume == 0) {
			return "Mute";
		}
		return std::to_string(volume);
	}

	static constexpr int step = 5;

	void handleLeft() override {
		int volume = SaveKV::instance().getInt("settings.global_volume", 80);
		SaveKV::instance().setInt(
			"settings.global_volume", std::max(0, volume - step)
		);
	}

	void handleRight() override {
		int volume = SaveKV::instance().getInt("settings.global_volume", 80);
		SaveKV::instance().setInt(
			"settings.global_volume", std::min(100, volume + step)
		);
	}
};

struct StrictPixelPerfectionOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Pixel Perfect Rendering";
	}

	std::string valueText() const override {
		return SaveKV::instance().getBool("settings.strict_pixel_perfection")
			? "On"
			: "Off";
	}

	void handleLeft() override {
		SaveKV::instance().setBool("settings.strict_pixel_perfection", false);
	}

	void handleRight() override {
		SaveKV::instance().setBool("settings.strict_pixel_perfection", true);
	}
};

struct SkipAnimationsOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Skip Animations";
	}

	std::string valueText() const override {
		return SaveKV::instance().getBool("settings.skip_animations")
			? "On"
			: "Off";
	}

	void handleLeft() override {
		SaveKV::instance().setBool("settings.skip_animations", false);
	}

	void handleRight() override {
		SaveKV::instance().setBool("settings.skip_animations", true);
	}
};

struct DebugHeatRenderOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Debug Heat Render";
	}

	std::string valueText() const override {
		return SaveKV::instance().getBool("settings.debug_heat_render")
			? "On"
			: "Off";
	}

	void handleLeft() override {
		SaveKV::instance().setBool("settings.debug_heat_render", false);
	}

	void handleRight() override {
		SaveKV::instance().setBool("settings.debug_heat_render", true);
	}
};

struct ResetSettingsOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Reset Settings";
	}

	bool handleEnter() override {
		auto &kv = SaveKV::instance();
		kv.remove("settings.scale");
		kv.remove("settings.global_volume");
		kv.remove("settings.strict_pixel_perfection");
		kv.remove("settings.skip_animations");
		kv.remove("settings.debug_heat_render");
		return false;
	}
};

struct ResetAllOption : SettingsMenu::Option {
	bool comfirmed = false;

	std::string displayText() const override {
		if (comfirmed) {
			return "Reset All(Again to Confirm)";
		} else {
			return "Reset All";
		}
	}

	bool handleEnter() override {
		if (comfirmed) {
			auto &kv = SaveKV::instance();
			kv.remove("completed_levels");
			kv.remove("settings.scale");
			kv.remove("settings.global_volume");
			kv.remove("settings.strict_pixel_perfection");
			kv.remove("settings.skip_animations");
			kv.remove("settings.debug_heat_render");
			return true;
		} else {
			comfirmed = true;
		}
		return false;
	}
};

struct GoBackOption : SettingsMenu::Option {
	std::string displayText() const override {
		return "Back(esc)";
	}

	bool handleEnter() override {
		return true;
	}
};

constexpr std::string_view cheat_code_sequence = "XYZZY";
constexpr int hint_max_opacity = 200;
constexpr int hint_fade_speed = 3;

} // namespace

SettingsMenu::~SettingsMenu() = default;

SettingsMenu::SettingsMenu()
	: font(AssetsManager::instance().getAsset<PixelFont>("font"))
	, _current_option_index(0)
	, _cheat_code_step(0)
	, _cheat_code_hint_opacity(0) {
	const auto &json_data = AssetsManager::instance().getAsset<nlohmann::json>(
		"ui-config/settings-menu"
	);

	_width = json_data.at("width");
	_height = json_data.at("height");

	_header = UITextDescriptor::fromJson(json_data.at("header"));
	_restart_hint = UITextDescriptor::fromJson(json_data.at("restart-hint"));

	const auto &option_data = json_data.at("option");
	_option_start_pos[0] = option_data.at("x");
	_option_start_pos[1] = option_data.at("y");

	_option_spacing = option_data.at("spacing");
	_option_text_size = option_data.at("size");
	_option_width = option_data.at("width");

	_options.push_back(std::make_unique<ScaleOption>());
	_options.push_back(std::make_unique<VolumnOption>());
	_options.push_back(std::make_unique<StrictPixelPerfectionOption>());
	_options.push_back(std::make_unique<SkipAnimationsOption>());

#ifndef NDEBUG
	_options.push_back(std::make_unique<DebugHeatRenderOption>());
#endif

	_options.push_back(std::make_unique<ResetSettingsOption>());
	_options.push_back(std::make_unique<ResetAllOption>());
	_options.push_back(std::make_unique<GoBackOption>());
}

std::array<int, 2> SettingsMenu::size() const {
	return {_width, _height};
}

void SettingsMenu::setup(SceneManager &mgr) {
	mgr.setWindowTitle("Settings");
}

void SettingsMenu::handleEvent(SceneManager &mgr, sf::Event &evt) {
	if (auto kb = evt.getIf<sf::Event::KeyPressed>()) {
		switch (kb->code) {
		case sf::Keyboard::Key::Escape:
			mgr.changeScene(pro::make_proxy<SceneFacade, MainMenu>());
			return;

		case sf::Keyboard::Key::Tab:
			if (kb->shift) {
				UISounds::instance().backward.play();
				if (_current_option_index > 0) {
					_current_option_index--;
				} else {
					_current_option_index = _options.size() - 1;
				}
			} else {
				UISounds::instance().forward.play();
				if (_current_option_index + 1 < _options.size()) {
					_current_option_index++;
				} else {
					_current_option_index = 0;
				}
			}

		case sf::Keyboard::Key::Up:
		case sf::Keyboard::Key::W:
			UISounds::instance().backward.play();
			if (_current_option_index > 0) {
				_current_option_index--;
			}
			break;

		case sf::Keyboard::Key::Down:
		case sf::Keyboard::Key::S:
			UISounds::instance().forward.play();
			if (_current_option_index + 1 < _options.size()) {
				_current_option_index++;
			}
			break;

		case sf::Keyboard::Key::Left:
		case sf::Keyboard::Key::A:
			UISounds::instance().backward.play();
			_options[_current_option_index]->handleLeft();
			break;

		case sf::Keyboard::Key::Right:
		case sf::Keyboard::Key::D:
			UISounds::instance().forward.play();
			_options[_current_option_index]->handleRight();
			break;

		case sf::Keyboard::Key::Enter:
		case sf::Keyboard::Key::Space:
			if (_options[_current_option_index]->handleEnter()) {
				// go back to main menu
				mgr.changeScene(pro::make_proxy<SceneFacade, MainMenu>());
				return;
			}
			break;

		case sf::Keyboard::Key::X:
			if (_cheat_code_step < cheat_code_sequence.size()
			    && cheat_code_sequence[_cheat_code_step] == 'X') {
				_cheat_code_step++;
				_cheat_code_hint_opacity = hint_max_opacity;
			}
			break;

		case sf::Keyboard::Key::Y:
			if (_cheat_code_step < cheat_code_sequence.size()
			    && cheat_code_sequence[_cheat_code_step] == 'Y') {
				_cheat_code_step++;
				_cheat_code_hint_opacity = hint_max_opacity;
			}
			break;

		case sf::Keyboard::Key::Z:
			if (_cheat_code_step < cheat_code_sequence.size()
			    && cheat_code_sequence[_cheat_code_step] == 'Z') {
				_cheat_code_step++;
				_cheat_code_hint_opacity = hint_max_opacity;
			}
			break;

		default:
			break;
		}
	}
}

void SettingsMenu::step(SceneManager &mgr) {
	if (_cheat_code_step >= cheat_code_sequence.size()
	    && _cheat_code_hint_opacity == hint_max_opacity) {
		// Cheat code entered, unlock all levels
		const auto &level_seqs = AssetsManager::instance()
									 .getAsset<LevelSequence>("level-sequence");
		SaveKV::instance().setInt(
			"completed_levels", static_cast<int>(level_seqs.levels.size())
		);
	}

	_cheat_code_hint_opacity -= hint_fade_speed;
	if (_cheat_code_hint_opacity < 0) {
		_cheat_code_step = 0;
	}
}

void SettingsMenu::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	// Render header & restart hint
	_header.render(target, font, "Settings", scale);
	_restart_hint.render(target, font, "some changes require restart", scale);

	// Render options
	for (size_t i = 0; i < _options.size(); ++i) {
		const auto &option = _options[i];
		sf::Color color = (i == _current_option_index)
			? ui_active_color
			: ui_text_color(255);

		std::string option_text = option->displayText();
		std::string value_text = option->valueText();

		int text_opt_x = _option_start_pos[0];
		int text_y = _option_start_pos[1] + i * _option_spacing;
		font.renderText(
			target, option_text, color, text_opt_x, text_y, scale,
			_option_text_size
		);

		// Right-aligned value text
		if (value_text.empty()) {
			continue;
		}

		if (i == _current_option_index) {
			value_text = "<" + value_text + ">";
		} else {
			value_text.push_back(' ');
		}

		int value_text_width = value_text.size()
			* font.charWidth(_option_text_size);
		int border_x = _option_start_pos[0] + _option_width;
		int text_value_x = border_x - value_text_width;
		font.renderText(
			target, value_text, color, text_value_x, text_y, scale,
			_option_text_size
		);
	}

	if (_cheat_code_hint_opacity > 0 && _cheat_code_step > 0) {
		std::string_view hint_text = (_cheat_code_step
		                              >= cheat_code_sequence.size())
			? "All levels unlocked"
			: cheat_code_sequence.substr(0, _cheat_code_step);
		int text_width = hint_text.size() * font.charWidth();
		int x = (_width - text_width) / 2;
		int y = _height - font.charHeight() - 10;
		sf::Color text_color = ui_text_color(_cheat_code_hint_opacity);
		font.renderText(target, hint_text, text_color, x, y, scale);
	}
}

} // namespace wf::scene
