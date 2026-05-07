#include "wforge/save.h"
#include "wforge/version.h"
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace wf {

namespace {

fs::path resolveSaveFilePath() noexcept {
	fs::path result = fs::current_path(); // default path

	// Try platform-specific paths
#ifdef _WIN32
	if (const char *appdata = std::getenv("APPDATA")) {
		result = fs::path(appdata) / "waveforge";
	}
#elifdef __APPLE__
	if (const char *home = std::getenv("HOME")) {
		result = fs::path(home) / "Library" / "Application Support"
			/ "waveforge";
	}
#elifdef __linux__
	if (const char *xdg_config = std::getenv("XDG_CONFIG_HOME")) {
		result = fs::path(xdg_config) / "waveforge";
	} else if (const char *home = std::getenv("HOME")) {
		result = fs::path(home) / ".config" / "waveforge";
	}
#else
	std::cerr << "Warning: unknown platform, using current directory for "
				 "save file.\n";
#endif

	if (!fs::exists(result)) {
		try {
			fs::create_directories(result);
		} catch (std::exception &e) {
			std::cerr << "Warning: failed to create save directory '"
					  << result.string() << "': " << e.what()
					  << "\nUsing current directory for save file.\n";
			result = fs::current_path();
		}
	}
	return result / "save-" WAVEFORGE_VERSION ".json";
}

fs::path saveFilePath() noexcept {
	// cache the resolved path
	static fs::path path = resolveSaveFilePath();
	return path;
}

void loadUserSettings(UserSettings &settings, nlohmann::json &json_data) {
	settings.scale = json_data.value("scale", 0);
	settings.global_volume = json_data.value("volume", 100);
	settings.strict_pixel_perfection = json_data.value(
		"strict_pixel_perfection", false
	);
	settings.skip_animations = json_data.value("skip_animations", false);
	settings.debug_heat_render = json_data.value("debug_heat_render", false);
}

void loadSaveData(SaveData &data, nlohmann::json &json_data) {
	data.completed_levels = json_data.value("completed_levels", 0);
	loadUserSettings(data.user_settings, json_data.at("user_settings"));
}

} // namespace

SaveData &SaveData::instance() noexcept {
	static SaveData *instance = nullptr;
	if (instance == nullptr) {
		auto path = saveFilePath();
		instance = new SaveData();
		if (fs::exists(path)) {
			std::ifstream file(path);
			if (!file.is_open()) {
				std::cerr << "Failed to open save file at '" << path.string()
						  << "'. Using default save data.\n";
			} else {
				try {
					auto json_data = nlohmann::json::parse(file);
					loadSaveData(*instance, json_data);
				} catch (std::exception &e) {
					std::cerr << "Failed to load save data from file '"
							  << path.string() << "': " << e.what()
							  << "\nUsing default save data.\n";
				}
			}
		}

		// Please don't rely on this saving mechanism
		// it only works as an extra safeguard against data loss
		// Please save manually whenever change is made
		std::atexit([]() {
			instance->save();
		});
	}
	return *instance;
}

void SaveData::save() const {
	auto path = saveFilePath();
	nlohmann::json json_data;
	json_data["completed_levels"] = completed_levels;
	json_data["user_settings"] = {
		{"scale", user_settings.scale},
		{"volume", user_settings.global_volume},
		{"strict_pixel_perfection", user_settings.strict_pixel_perfection},
		{"skip_animations", user_settings.skip_animations},
		{"debug_heat_render", user_settings.debug_heat_render},
	};

	std::ofstream file(path);
	if (!file.is_open()) {
		std::cerr << "Failed to open save file at '" << path.string()
				  << "' for writing. Save data not saved.\n";
		return;
	}

	try {
		file << json_data.dump();
	} catch (std::exception &e) {
		std::cerr << "Failed to write save data to file '" << path.string()
				  << "': " << e.what() << "\n";
	}
}

void SaveData::resetSettings() {
	user_settings = UserSettings::defaultSettings();
	save();
}

void SaveData::resetAll() {
	completed_levels = 0;
	user_settings = UserSettings::defaultSettings();
	save();
}

bool SaveData::isFirstLaunch() const noexcept {
	return completed_levels == 0;
}

UserSettings UserSettings::defaultSettings() noexcept {
	return UserSettings{
		.scale = 0,
		.global_volume = 80,
		.strict_pixel_perfection = false,
		.skip_animations = false,
		.debug_heat_render = false,
	};
}

SaveData::SaveData()
	: completed_levels(0), user_settings(UserSettings::defaultSettings()) {}

} // namespace wf
