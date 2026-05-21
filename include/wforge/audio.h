#ifndef WFORGE_AUDIO_H
#define WFORGE_AUDIO_H

#include "wforge/assets.h"
#include <SFML/Audio.hpp>

namespace wf {

struct FadeIOConfig {
	float fade_in_starting_volume;
	int fade_in_ticks;
	int fade_out_ticks;

	static FadeIOConfig &load();
};

struct UISounds {
	sf::Sound forward;
	sf::Sound backward;

	static UISounds &instance() noexcept;
};

class ActiveSoundManager {
public:
	ActiveSoundManager(const ActiveSoundManager &) = delete;
	ActiveSoundManager &operator=(const ActiveSoundManager &) = delete;
	ActiveSoundManager(ActiveSoundManager &&) = delete;
	ActiveSoundManager &operator=(ActiveSoundManager &&) = delete;

	static ActiveSoundManager &instance() noexcept;

	void play(sf::SoundBuffer &buffer);
	void cleanup();

private:
	ActiveSoundManager() = default;

	std::vector<sf::Sound> _active_sounds;
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
