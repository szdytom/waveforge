#include "wforge/2d.h"
#include "wforge/audio.h"
#include "wforge/colorpalette.h"
#include "wforge/level.h"
#include "wforge/save.h"
#include "wforge/scene.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>
#include <proxy/v4/proxy.h>
#include <string_view>

namespace wf::scene {

namespace {

constexpr std::string_view level_complete_text = "LEVEL COMPLETED!";

}

LevelComplete::LevelComplete(
	int level_width, int level_height, int duck_x, int duck_y
)
	: _level_width(level_width)
	, _level_height(level_height)
	, _pending_timer(0)
	, _current_step(0)
	, _display_text(false)
	, font(AssetsManager::instance().getAsset<PixelFont>("font"))
	, _duck_texture(
		  AssetsManager::instance().getAsset<sf::Texture>("duck/texture")
	  )
	, _level_complete_sound(
		  AssetsManager::instance().getAsset<sf::SoundBuffer>(
			  "sfx/level-complete"
		  )
	  ) {
	constexpr int play_speed = 4;

	int banner_width = level_complete_text.size() * font.charWidth() + 3
		+ _duck_texture.getSize().x;

	int banner_height = _duck_texture.getSize().y;

	_top_left_x = (_level_width - banner_width) / 2;
	int top_left_y = (_level_height - banner_height) / 2;

	int frame = 0;
	for (auto [tx, ty] :
	     tilesOnSegment({duck_x, duck_y}, {_top_left_x, top_left_y})) {
		if (frame % play_speed == 0) {
			_step_positions.push_back({tx, ty});
		}
		frame++;
	}
	_step_positions.push_back({_top_left_x, top_left_y});
}

std::array<int, 2> LevelComplete::size() const {
	return {_level_width, _level_height};
}

void LevelComplete::setup(SceneManager &mgr) {
	BGMManager::instance().unsetCollection();
	if (SaveData::instance().user_settings.skip_animations) {
		_current_step = _step_positions.size() - 1;
		_display_text = true;
	} else {
		_level_complete_sound.play();
	}
}

void LevelComplete::handleEvent(SceneManager &mgr, sf::Event &evt) {}

void LevelComplete::step(SceneManager &mgr) {
	constexpr int pending_duration = 24;

	if (_pending_timer < pending_duration) {
		_pending_timer++;
		return;
	}

	if (_display_text) {
		// Go back to level menu
		mgr.changeScene(Scene(std::make_unique<LevelSelectionMenu>()));
		return;
	}

	if (_current_step + 1 < _step_positions.size()) {
		_current_step++;
		if (_current_step == _step_positions.size() - 1) {
			_pending_timer = 0;
			_display_text = true;
		}
	}
}

void LevelComplete::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	sf::Sprite duck_sprite(_duck_texture);
	duck_sprite.setScale(sf::Vector2f(scale, scale));

	auto [cur_x, cur_y] = _step_positions[_current_step];
	duck_sprite.setPosition(sf::Vector2f(cur_x * scale, cur_y * scale));
	target.draw(duck_sprite);

	if (_display_text) {
		int text_x = _top_left_x + _duck_texture.getSize().x + 3;
		int text_y = (_level_height - font.charHeight()) / 2;
		font.renderText(
			target, level_complete_text, ui_text_color(255), text_x, text_y,
			scale
		);
	}
}

LevelLoading::LevelLoading(int width, int height, LevelMetadata level_metadata)
	: _width(width)
	, _height(height)
	, _tick(0)
	, font(AssetsManager::instance().getAsset<PixelFont>("font"))
	, _level_metadata(std::move(level_metadata)) {
	auto fade_io = FadeIOConfig::load();
	_total_duration = fade_io.fade_out_ticks;
}

std::array<int, 2> LevelLoading::size() const {
	return {_width, _height};
}

void LevelLoading::setup(SceneManager &mgr) {
	BGMManager::instance().fadeOutCurrent(_total_duration);
}

void LevelLoading::handleEvent(SceneManager &mgr, sf::Event &evt) {}

void LevelLoading::step(SceneManager &mgr) {
	_tick++;
	if (_tick >= _total_duration) {
		// Load level scene
		mgr.changeScene(Scene(
			pro::make_proxy<SceneFacade, LevelPlaying>(
				Level::loadFromMetadata(std::move(_level_metadata))
			)
		));
	}
}

void LevelLoading::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	constexpr std::string_view loading_text = "LOADING...";

	int text_width = loading_text.size() * font.charWidth();
	int x = (_width - text_width) / 2;
	int y = (_height - font.charHeight()) / 2;
	font.renderText(target, loading_text, ui_text_color(255), x, y, scale);
}

} // namespace wf::scene
