#ifndef WFORGE_AUDIO_H
#define WFORGE_AUDIO_H

#include "wforge/assets.h"
#include <SFML/Audio.hpp>
#include <list>

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

class BGMManager {
public:
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
	BGMManager(const BGMManager &) = delete;
	BGMManager &operator=(const BGMManager &) = delete;

	float _cur_volume;
	float _volume_delta;
	sf::Music *_cur_bgm;
	MusicCollection *_collection;
};

class ActiveSoundManager {
public:
	static ActiveSoundManager &instance() noexcept;

	void play(const sf::SoundBuffer &buffer);
	void step();

private:
	ActiveSoundManager() noexcept = default;
	ActiveSoundManager(const ActiveSoundManager &) = delete;
	ActiveSoundManager &operator=(const ActiveSoundManager &) = delete;

	std::vector<sf::Sound> _active_sounds;
};

}; // namespace wf

#endif // WFORGE_AUDIO_H
