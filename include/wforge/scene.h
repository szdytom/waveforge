#ifndef WFORGE_SCENE_H
#define WFORGE_SCENE_H

#include "wforge/assets.h"
#include "wforge/audio.h"
#include "wforge/fallsand.h"
#include "wforge/level.h"
#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <proxy/v4/proxy.h>
#include <proxy/v4/proxy_macros.h>
#include <string_view>
#include <vector>

namespace wf {

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemSize, size);
PRO_DEF_MEM_DISPATCH(MemHandleEvent, handleEvent);

}; // namespace _dispatch

class SceneManager;

/* clang-format off */
struct SceneFacade : pro::facade_builder
	::add_convention<_dispatch::MemSize, std::array<int, 2>() const>
	::add_convention<_dispatch::MemSetup, void(SceneManager &)>
	::add_convention<_dispatch::MemHandleEvent, void(SceneManager &, sf::Event &)>
	::add_convention<_dispatch::MemStep, void(SceneManager &)>
	::add_convention<_dispatch::MemRender, void(const SceneManager &, sf::RenderTarget &, int) const>
	::build {};
/* clang-format on */

using Scene = pro::proxy<SceneFacade>;

struct UITextDescriptor {
	int x;
	int y;
	int size;
	sf::Color color;

	void render(
		sf::RenderTarget &target, const PixelFont &font, std::string_view text,
		int scale
	) const;

	static UITextDescriptor fromJson(const nlohmann::json &json_data);
};

int automaticScale(int width, int height, int scale_configured = 0);

class SceneManager {
public:
	SceneManager(Scene initial_scene, int scale = 0);
	~SceneManager();

	void changeScene(Scene new_scene);
	void handleEvent(sf::Event &evt);
	void tick();
	void setWindowTitle(std::string title);

	sf::Vector2i mousePosition() const;
	int scale() const {
		return _scale;
	}

	sf::RenderWindow window;

private:
	Scene _current_scene;
	bool _scene_changed;
	int _config_scale;
	int _scale;
};

struct ButtonDescriptor {
	int x;
	int y;
	int size;
};

namespace scene {

struct LevelPlaying {
	LevelPlaying(const std::string &level_id);
	LevelPlaying(Level level);

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

	void pause(SceneManager &mgr) noexcept;
	void unpause(SceneManager &mgr) noexcept;

private:
	void _restartLevel(SceneManager &mgr, bool is_failed = true);

	int _tick;
	Level _level;
	mutable LevelRenderer _renderer;
	int _hint_type;
	int _hint_opacity;
	PixelFont &font;
	bool _paused;

	// Paused Menu
	int _paused_menu_current_button_index;
	bool _show_help;
	sf::Texture *_help_texture;
};

struct DuckDeath {
	DuckDeath(
		int level_width, int level_height, int duck_x, int duck_y,
		LevelMetadata level_metadata
	);

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	int _level_width;
	int _level_height;

	int _duck_x;
	int _duck_y;
	int _duck_anchor_bx;
	int _duck_anchor_by;

	int _tick;
	int _animation_frame;

	int _total_duration;
	int _animation_start;
	int _animation_frame_duration;
	int _sperate_sfx_start;
	int _reborn_sfx_start;

	LevelMetadata _level_metadata;
	PixelAnimationFrames &_animation;
	sf::Sound _reborn_sound;
	sf::Sound _duck_death_separate_sound;
};

struct LevelComplete {
	LevelComplete(int level_width, int level_height, int duck_x, int duck_y);

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	int _level_width;
	int _level_height;
	int _pending_timer;
	int _current_step;
	bool _display_text;
	const PixelFont &font;
	sf::Texture &_duck_texture;
	sf::Sound _level_complete_sound;
	std::vector<std::array<int, 2>> _step_positions;
	int _top_left_x;
};

struct LevelLoading {
	LevelLoading(int width, int height, LevelMetadata level_metadata);

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	int _width;
	int _height;
	int _tick;
	int _total_duration;
	const PixelFont &font;
	LevelMetadata _level_metadata;
};

struct LevelSelectionMenu {
	LevelSelectionMenu();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	int _selected_index;
	const LevelSequence &_level_seq;

	int _width;
	int _height;
	const PixelFont &font;

	UITextDescriptor _header;
	UITextDescriptor _level_button_text;
	UITextDescriptor _level_title;
	UITextDescriptor _level_desc;
	UITextDescriptor _level_difficulty;
	UITextDescriptor _play_hint;
	UITextDescriptor _enter_hint;
	std::vector<std::array<int, 2>> _level_button;
	std::vector<std::array<int, 2>> _level_links;
	std::array<int, 2> _duck_rel;

	sf::Texture *_duck_texture;
	sf::Texture *_level_button_texture_frame;
	sf::Texture *_level_button_texture_locked;
	sf::Texture *_level_link_texture_activated;
	sf::Texture *_level_link_texture_locked;
};

struct MainMenu {
	MainMenu();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	int _width;
	int _height;
	const PixelFont &font;
	sf::Texture *_background_texture;

	int _current_button_index;
	ButtonDescriptor _play_button;
	ButtonDescriptor _settings_button;
	ButtonDescriptor _help_button;
	ButtonDescriptor _exit_button;
	UITextDescriptor _version_text;
};

class SettingsMenu {
public:
	SettingsMenu();
	~SettingsMenu();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

	struct Option;

private:
	int _width;
	int _height;
	const PixelFont &font;

	UITextDescriptor _header;
	UITextDescriptor _restart_hint;

	int _current_option_index;
	std::vector<std::unique_ptr<Option>> _options;

	std::array<int, 2> _option_start_pos;
	int _option_spacing;
	int _option_text_size;
	int _option_width;
	sf::Color _option_color;
	sf::Color _option_active_color;

	int _cheat_code_step;
	int _cheat_code_hint_opacity;
};

struct Credits {
	Credits();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	int _width;
	int _height;
	const PixelFont &font;

	UITextDescriptor _header;

	std::array<int, 2> _credits_pos;
	int _credits_size;
	int _credits_spacing;
	int _credits_width;
	sf::Color _credits_color;

	std::vector<std::pair<std::string, std::string>> _content;
};

struct Help {
	Help();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	sf::Texture *_background_texture;
	int _width;
	int _height;
};

} // namespace scene

class ScriptedScene {
public:
	ScriptedScene(
		const std::string &script_id, std::string_view route_data = ""
	);
	~ScriptedScene();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, const sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

	struct Impl;

private:
	std::unique_ptr<Impl> _impl;
};

} // namespace wf

#endif // WFORGE_SCENE_H
