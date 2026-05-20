#include "wforge/assets.h"
#include "wforge/level.h"
#include "wforge/save_io.h"
#include "wforge/scene.h"
#include <SFML/Audio/SoundBuffer.hpp>
#include <limits>
#include <nlohmann/json.hpp>
#include <proxy/v4/proxy.h>

namespace wf::scene {

namespace {

PixelAnimationFrames &duckDeathAnimation() {
	static PixelAnimationFrames *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = &AssetsManager::instance().getAsset<PixelAnimationFrames>(
			"duckdeath/animation"
		);
	}
	return *ptr;
}

} // namespace

DuckDeath::DuckDeath(
	int level_width, int level_height, int duck_x, int duck_y,
	LevelMetadata level_metadata
)
	: _level_width(level_width)
	, _level_height(level_height)
	, _duck_x(duck_x)
	, _duck_y(duck_y)
	, _tick(0)
	, _animation_frame(0)
	, _level_metadata(std::move(level_metadata))
	, _animation(duckDeathAnimation())
	, _reborn_sound(
		  AssetsManager::instance().getAsset<sf::SoundBuffer>("sfx/duckdeath")
	  )
	, _duck_death_separate_sound(
		  AssetsManager::instance().getAsset<sf::SoundBuffer>(
			  "sfx/duckdeath-separate"
		  )
	  ) {
	_duck_anchor_bx = std::numeric_limits<int>::max();
	_duck_anchor_by = std::numeric_limits<int>::max();
	auto &raw_duck = AssetsManager::instance().getAsset<sf::Image>("duck/raw");
	int raw_duck_width = raw_duck.getSize().x;
	int raw_duck_height = raw_duck.getSize().y;
	for (unsigned int x = 0; x < raw_duck_width; ++x) {
		for (unsigned int y = 0; y < raw_duck_height; ++y) {
			sf::Color color = raw_duck.getPixel({x, y});
			if (color.a != 0) {
				_duck_anchor_bx = std::min<int>(_duck_anchor_bx, x);
				_duck_anchor_by = std::min<int>(_duck_anchor_by, y);
			}
		}
	}

	const auto &json_data = AssetsManager::instance().getAsset<nlohmann::json>(
		"duckdeath/timeline"
	);

	_total_duration = json_data.at("total-duration");
	const auto &timeline = json_data.at("timeline");
	_animation_start = timeline.at("animation-start");
	_animation_frame_duration = timeline.at("animation-frame-duration");
	_sperate_sfx_start = timeline.at("separate-sfx-start");
	_reborn_sfx_start = timeline.at("reborn-sfx-start");
}

std::array<int, 2> DuckDeath::size() const {
	return {_level_width, _level_height};
}

void DuckDeath::setup(SceneManager &mgr) {
	BGMManager::instance().unsetCollection();
	if (SaveKV::instance().getBool("settings.skip_animations")) {
		_tick = _total_duration;
	}
}

void DuckDeath::handleEvent(SceneManager &mgr, sf::Event &evt) {
	if (auto kb = evt.getIf<sf::Event::KeyPressed>()) {
		switch (kb->code) {
		case sf::Keyboard::Key::Space:
		case sf::Keyboard::Key::Enter:
		case sf::Keyboard::Key::R:
			_tick = _total_duration;
			break;

		default:
			break;
		}
	}
}

void DuckDeath::step(SceneManager &mgr) {
	_tick += 1;
	if (_tick > _total_duration) {
		_reborn_sound.stop();
		mgr.changeScene(
			pro::make_proxy<SceneFacade, LevelPlaying>(
				Level::loadFromMetadata(std::move(_level_metadata))
			)
		);
		return;
	}

	if (_tick == _sperate_sfx_start) {
		_duck_death_separate_sound.play();
	}

	if (_tick == _reborn_sfx_start) {
		_reborn_sound.play();
	}

	if (_tick >= _animation_start) {
		if (_tick % _animation_frame_duration == 0) {
			_animation_frame += 1;
			if (_animation_frame >= _animation.length()) {
				_animation_frame = _animation.length() - 1;
			}
		}
	}
}

void DuckDeath::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	target.clear(sf::Color(220, 220, 220, 255));
	int rx = _duck_x - _duck_anchor_bx;
	int ry = _duck_y - _duck_anchor_by;
	_animation.render(target, _animation_frame, rx, ry, scale);
}

} // namespace wf::scene
