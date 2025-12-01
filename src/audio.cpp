#include "wforge/audio.h"
#include "wforge/save.h"
#include <SFML/Audio/SoundBuffer.hpp>
#include <nlohmann/json.hpp>

namespace wf {

FadeIOConfig &FadeIOConfig::load() {
	static FadeIOConfig config;
	static bool loaded = false;

	if (!loaded) {
		const auto &json_data = AssetsManager::instance()
									.getAsset<nlohmann::json>("config/fade-io");
		config.fade_in_ticks = json_data.at("fade-in-ticks");
		config.fade_out_ticks = json_data.at("fade-out-ticks");
		config.fade_in_starting_volume = json_data.at(
			"fade-in-starting-volume"
		);
		loaded = true;
	}

	return config;
}

BGMManager &BGMManager::instance() noexcept {
	static BGMManager instance;
	return instance;
}

UISounds &UISounds::instance() noexcept {
	static UISounds *instance = nullptr;
	if (instance == nullptr) {
		sf::Sound forward_sound(
			AssetsManager::instance().getAsset<sf::SoundBuffer>("sfx/forward")
		);

		sf::Sound backward_sound(
			AssetsManager::instance().getAsset<sf::SoundBuffer>("sfx/backward")
		);
		instance = new UISounds{
			.forward = std::move(forward_sound),
			.backward = std::move(backward_sound),
		};
	}
	return *instance;
}

BGMManager::BGMManager() noexcept
	: _cur_volume(1.f)
	, _volume_delta(0.f)
	, _cur_bgm(nullptr)
	, _collection(nullptr) {}

void BGMManager::unsetCollection() {
	_collection = nullptr;
	if (_cur_bgm) {
		_cur_bgm->stop();
		_cur_bgm = nullptr;
	}
}

void BGMManager::setCollection(const std::string &id) {
	if (_collection && _collection->id == id) {
		return; // already set
	}

	_collection = &AssetsManager::instance().getMusicCollection(id);
	if (_cur_bgm) {
		_cur_bgm->stop();
		_cur_bgm = nullptr;
	}
	nextMusic();
}

void BGMManager::fadeOutCurrent(int duration_ticks) {
	if (duration_ticks <= 0) {
		if (_cur_bgm) {
			_cur_bgm->stop();
			_cur_bgm = nullptr;
		}
	} else {
		_volume_delta = -_cur_volume / duration_ticks;
	}
}

void BGMManager::fadeInCurrent(int duration_ticks, float starting_volume) {
	if (duration_ticks <= 0) {
		_cur_volume = 1.f;
		_volume_delta = 0.f;
	} else {
		_cur_volume = starting_volume;
		_volume_delta = (1.f - starting_volume) / duration_ticks;
		if (_cur_bgm) {
			_cur_bgm->setVolume(
				starting_volume
				* SaveData::instance().user_settings.global_volume
			);
		}
	}
}

void BGMManager::step() {
	int global_volume = SaveData::instance().user_settings.global_volume;
	if (_cur_bgm && _cur_bgm->getStatus() == sf::Music::Status::Playing) {
		_cur_volume += _volume_delta;
		if (_cur_volume <= 0.f) {
			_cur_bgm->stop();
			_cur_bgm = nullptr;
			_volume_delta = 0.f;
			return;
		}
		if (_cur_volume >= 1.f) {
			_cur_volume = 1.f;
			_volume_delta = 0.f;
		}
		_cur_bgm->setVolume(_cur_volume * global_volume);
	} else if (_collection) {
		nextMusic();
	}
}

void BGMManager::setVolume(float volume) {
	{
		_cur_volume = volume;
		if (_cur_bgm) {
			_cur_bgm->setVolume(_cur_volume);
		}
	}
}

bool BGMManager::isEmpty() const noexcept {
	return _collection == nullptr;
}

void BGMManager::nextMusic() {
	if (!_collection) {
		return;
	}

	if (_cur_bgm) {
		_cur_bgm->stop();
	}

	if ((_cur_bgm = _collection->getRandomMusic())) {
		_cur_bgm->play();
		_cur_volume = 1.f;
		_volume_delta = 0.f;
	}
}

} // namespace wf
