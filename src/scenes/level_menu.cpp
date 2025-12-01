#include "wforge/assets.h"
#include "wforge/audio.h"
#include "wforge/level.h"
#include "wforge/save.h"
#include "wforge/scene.h"
#include <SFML/Graphics/Texture.hpp>
#include <nlohmann/json.hpp>
#include <proxy/v4/proxy.h>

namespace wf::scene {

LevelSelectionMenu::LevelSelectionMenu()
	: _level_seq(
		  AssetsManager::instance().getAsset<LevelSequence>("level-sequence")
	  )
	, font(AssetsManager::instance().getAsset<PixelFont>("font")) {
	const auto &json_data = AssetsManager::instance().getAsset<nlohmann::json>(
		"ui-config/level-menu"
	);

	_width = json_data.at("width");
	_height = json_data.at("height");

	_header = UITextDescriptor::fromJson(json_data.at("header"));
	_level_button_text = UITextDescriptor::fromJson(
		json_data.at("level-button-text")
	);
	_level_title = UITextDescriptor::fromJson(json_data.at("level-title"));
	_level_desc = UITextDescriptor::fromJson(json_data.at("level-description"));
	_level_difficulty = UITextDescriptor::fromJson(
		json_data.at("level-difficulty")
	);
	_play_hint = UITextDescriptor::fromJson(json_data.at("play-hint"));
	_enter_hint = UITextDescriptor::fromJson(json_data.at("enter-hint"));

	for (const auto &btn : json_data.at("level-buttons")) {
		_level_button.push_back({btn.at("x"), btn.at("y")});
	}

	for (const auto &link : json_data.at("level-links")) {
		_level_links.push_back({link.at("x"), link.at("y")});
	}

	if (_level_links.size() != _level_button.size() + 1) {
		throw std::runtime_error(
			"Level menu configuration error: "
			"number of level links must be equal to number of level buttons + 1"
		);
	}

	_duck_rel[0] = json_data.at("level-duck").at("xrel");
	_duck_rel[1] = json_data.at("level-duck").at("yrel");

	const auto &texture_data = json_data.at("texture");
	auto loadTexture = [&](const std::string &key) {
		return &AssetsManager::instance().getAsset<sf::Texture>(
			texture_data.at(key)
		);
	};

	_duck_texture = loadTexture("duck");

	_level_button_texture_frame = loadTexture("level-button-selected-frame");
	_level_button_texture_locked = loadTexture("level-button-locked");
	_level_link_texture_activated = loadTexture("link-activated");
	_level_link_texture_locked = loadTexture("link-locked");

	auto &save_data = SaveData::instance();
	_selected_index = save_data.completed_levels;
	if (_selected_index >= _level_seq.levels.size()) {
		_selected_index = _level_seq.levels.size() - 1;
	}
}

std::array<int, 2> LevelSelectionMenu::size() const {
	return {_width, _height};
}

void LevelSelectionMenu::setup(SceneManager &mgr) {
	mgr.bgm.setCollection("background/main-menu-music");
	mgr.setWindowTitle("Level Selection");
}

void LevelSelectionMenu::handleEvent(SceneManager &mgr, sf::Event &evt) {
	const auto &save = SaveData::instance();
	if (auto kb = evt.getIf<sf::Event::KeyPressed>()) {
		switch (kb->code) {
		case sf::Keyboard::Key::Left:
		case sf::Keyboard::Key::A:
			UISounds::instance().backward.play();
			if (_selected_index > 0) {
				_selected_index--;
			}
			break;

		case sf::Keyboard::Key::Right:
		case sf::Keyboard::Key::D:
			UISounds::instance().forward.play();
			if (_selected_index + 1 <= save.completed_levels) {
				_selected_index++;
			}
			break;

		case sf::Keyboard::Key::Enter:
		case sf::Keyboard::Key::Space:
			if (_selected_index <= save.completed_levels) {
				UISounds::instance().forward.play();
				if (save.user_settings.skip_animations) {
					mgr.changeScene(
						pro::make_proxy<SceneFacade, LevelPlaying>(
							Level::loadFromMetadata(
								*_level_seq.levels.at(_selected_index)
							)
						)
					);
				} else {
					mgr.changeScene(
						pro::make_proxy<SceneFacade, LevelLoading>(
							_width, _height,
							*_level_seq.levels.at(_selected_index)
						)
					);
				}
				return;
			}
			break;

		case sf::Keyboard::Key::Escape:
			mgr.changeScene(pro::make_proxy<SceneFacade, MainMenu>());
			return;

		default:
			break;
		}
	}
}

void LevelSelectionMenu::step(SceneManager &mgr) {
	// nothing to do here
}

void LevelSelectionMenu::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	// Render header
	_header.render(target, font, "Levels", scale);

	// Render level buttons and links
	auto &save_data = SaveData::instance();
	int btn_cnt = _level_button.size();
	int ideal_duck_btn_index = (btn_cnt - 1) / 2;
	int duck_btn_index = ideal_duck_btn_index;
	if (_selected_index < ideal_duck_btn_index) {
		duck_btn_index = _selected_index;
	} else if (_selected_index + (btn_cnt - ideal_duck_btn_index)
	           > _level_seq.levels.size()) {
		duck_btn_index = btn_cnt - (_level_seq.levels.size() - _selected_index);
	}
	int first_btn_level = _selected_index - duck_btn_index;

	for (int i = 0; i < btn_cnt; ++i) {
		int level_index = first_btn_level + i;
		if (level_index >= _level_seq.levels.size()) {
			break;
		}

		bool level_locked = level_index > save_data.completed_levels;

		auto [btn_x, btn_y] = _level_button[i];
		const auto &btn_texture = level_locked
			? *_level_button_texture_locked
			: *_level_seq.levels[level_index]->minimap_texture;

		sf::Sprite btn_sprite(btn_texture);
		btn_sprite.setPosition(sf::Vector2f(btn_x * scale, btn_y * scale));
		btn_sprite.setScale(sf::Vector2f(scale, scale));
		target.draw(btn_sprite);

		if (level_index != 0) {
			// Render link to previous level
			auto [link_x, link_y] = _level_links[i];
			auto link_texture = level_locked
				? _level_link_texture_locked
				: _level_link_texture_activated;
			sf::Sprite link_sprite(*link_texture);
			link_sprite.setPosition(
				sf::Vector2f(link_x * scale, link_y * scale)
			);
			link_sprite.setScale(sf::Vector2f(scale, scale));
			target.draw(link_sprite);
		}

		// Render button text
		if (!level_locked) {
			int topleft_x = btn_x + _level_button_text.x;
			int topleft_y = btn_y + _level_button_text.y;
			int size = _level_button_text.size;
			std::string level_label = std::to_string(level_index + 1);
			// caclulate text centering
			int text_width = level_label.size() * font.charWidth(size);
			int text_height = font.charHeight(size);

			int bottom_right_x = btn_x + btn_texture.getSize().x;
			int bottom_right_y = btn_y + btn_texture.getSize().y;
			int text_x = topleft_x
				+ (bottom_right_x - topleft_x - text_width) / 2;
			int text_y = topleft_y
				+ (bottom_right_y - topleft_y - text_height) / 2;

			font.renderText(
				target, level_label, _level_button_text.color, text_x, text_y,
				scale, size
			);
		}

		// Render selection frame
		if (level_index == _selected_index) {
			auto [frame_x, frame_y] = _level_button[i];
			sf::Texture *frame_texture = _level_button_texture_frame;
			sf::Sprite frame_sprite(*frame_texture);
			frame_sprite.setPosition(
				sf::Vector2f(frame_x * scale, frame_y * scale)
			);
			frame_sprite.setScale(sf::Vector2f(scale, scale));
			target.draw(frame_sprite);
		}

		// Render duck
		if (level_index == _selected_index) {
			int duck_x = btn_x + _duck_rel[0];
			int duck_y = btn_y + _duck_rel[1];
			// duck_x and duck_y are of anchor bottom-middle

			int duck_render_x = duck_x - (_duck_texture->getSize().x / 2);
			int duck_render_y = duck_y - _duck_texture->getSize().y + 1;
			sf::Sprite duck_sprite(*_duck_texture);
			duck_sprite.setPosition(
				sf::Vector2f(duck_render_x * scale, duck_render_y * scale)
			);
			duck_sprite.setScale(sf::Vector2f(scale, scale));
			target.draw(duck_sprite);
		}
	}

	// Render the last link if applicable
	if (first_btn_level + btn_cnt < _level_seq.levels.size()) {
		auto [link_x, link_y] = _level_links[btn_cnt];
		bool level_locked = first_btn_level + btn_cnt
			> save_data.completed_levels;
		auto link_texture = level_locked
			? _level_link_texture_locked
			: _level_link_texture_activated;
		sf::Sprite link_sprite(*link_texture);
		link_sprite.setPosition(sf::Vector2f(link_x * scale, link_y * scale));
		link_sprite.setScale(sf::Vector2f(scale, scale));
		target.draw(link_sprite);
	}

	auto selected_metadata = _level_seq.levels.at(_selected_index);
	// Render level title
	_level_title.render(target, font, selected_metadata->name, scale);

	if (_selected_index <= save_data.completed_levels) {
		// Render level description
		_level_desc.render(target, font, selected_metadata->description, scale);

		_level_difficulty.render(
			target, font,
			std::format(
				"Difficulty:{}",
				LevelMetadata::difficultyToString(selected_metadata->difficulty)
			),
			scale
		);

		// Render enter hint
		_play_hint.render(target, font, "Play", scale);
		_enter_hint.render(target, font, "[Enter]", scale);
	}
}

} // namespace wf::scene
