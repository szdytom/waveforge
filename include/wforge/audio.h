#ifndef WFORGE_AUDIO_H
#define WFORGE_AUDIO_H

#include "wforge/assets.h"
#include <SFML/Audio.hpp>
#include <memory>
#include <vector>

namespace wf {

struct FadeIOConfig {
	float fade_in_starting_volume;
	int fade_in_ticks;
	int fade_out_ticks;

	static FadeIOConfig &load();
};

class SoundManager {
public:
	SoundManager(const SoundManager &) = delete;
	SoundManager &operator=(const SoundManager &) = delete;
	SoundManager(SoundManager &&) = delete;
	SoundManager &operator=(SoundManager &&) = delete;

	static SoundManager &instance() noexcept;

	void play(const std::shared_ptr<sf::Sound> &sound);
	void step();

private:
	SoundManager() = default;

	std::vector<std::shared_ptr<sf::Sound>> _active_sounds;
};

class BGMManager {
public:
	BGMManager(const BGMManager &) = delete;
	BGMManager &operator=(const BGMManager &) = delete;
	BGMManager(BGMManager &&) = delete;
	BGMManager &operator=(BGMManager &&) = delete;

	static BGMManager &instance() noexcept;

	void unsetCollection();
	void setCollection(const std::string &id);
	void fadeOutCurrent(int duration_ticks);
	void fadeInCurrent(int duration_ticks, float starting_volume);
	void step();
	void nextMusic();
	void setVolume(float volume);

	bool isEmpty() const noexcept;

private:
	BGMManager() noexcept;

	float _cur_volume;
	float _volume_delta;
	sf::Music *_cur_bgm;
	MusicCollection *_collection;
};

}; // namespace wf

#endif // WFORGE_AUDIO_H
