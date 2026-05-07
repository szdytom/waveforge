#ifndef WFORGE_SAVE_H
#define WFORGE_SAVE_H

namespace wf {

struct UserSettings {
	int scale;
	int global_volume;
	bool strict_pixel_perfection;
	bool skip_animations;
	bool debug_heat_render;

	static UserSettings defaultSettings() noexcept;
};

struct SaveData {
	int completed_levels;
	UserSettings user_settings;

	static SaveData &instance() noexcept;
	void save() const;
	void resetSettings();
	void resetAll();
	bool isFirstLaunch() const noexcept;

	SaveData(const SaveData &) = delete;
	SaveData &operator=(const SaveData &) = delete;

private:
	SaveData();
};

} // namespace wf

#endif // WFORGE_SAVE_H
