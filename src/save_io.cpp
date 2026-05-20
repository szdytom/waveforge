#include "wforge/save_io.h"
#include "wforge/version.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace wf {

namespace {

fs::path resolveSaveDir() noexcept {
	fs::path dir = fs::current_path();

#ifdef _WIN32
	if (const char *appdata = std::getenv("APPDATA")) {
		dir = fs::path(appdata) / "waveforge";
	}
#elifdef __APPLE__
	if (const char *home = std::getenv("HOME")) {
		dir = fs::path(home) / "Library" / "Application Support" / "waveforge";
	}
#elifdef __linux__
	if (const char *xdg_config = std::getenv("XDG_CONFIG_HOME")) {
		dir = fs::path(xdg_config) / "waveforge";
	} else if (const char *home = std::getenv("HOME")) {
		dir = fs::path(home) / ".config" / "waveforge";
	}
#else
	std::cerr << "Warning: unknown platform, using current directory for "
				 "save file.\n";
#endif

	if (!fs::exists(dir)) {
		try {
			fs::create_directories(dir);
		} catch (std::exception &e) {
			std::cerr << "Warning: failed to create save directory '"
					  << dir.string() << "': " << e.what()
					  << "\nUsing current directory for save file.\n";
			dir = fs::current_path();
		}
	}
	return dir;
}

fs::path saveFilePath() noexcept {
	static fs::path path = resolveSaveDir()
		/ ("save-" WAVEFORGE_VERSION ".json");
	return path;
}

} // namespace

SaveKV &SaveKV::instance() {
	static SaveKV kv;
	return kv;
}

void SaveKV::load() {
	auto path = saveFilePath();
	if (!fs::exists(path)) {
		return;
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << "Failed to open save file at '" << path.string()
				  << "'. Using empty save.\n";
		return;
	}

	try {
		nlohmann::json json_data;
		file >> json_data;

		if (!json_data.is_object()) {
			std::cerr << "Save file is not a JSON object, ignoring.\n";
			return;
		}

		for (auto it = json_data.begin(); it != json_data.end(); ++it) {
			if (it->is_string()) {
				_data[it.key()] = it->get<std::string>();
			}
		}
	} catch (std::exception &e) {
		std::cerr << "Failed to parse save file '" << path.string()
				  << "': " << e.what() << "\n";
	}
}

std::optional<std::string> SaveKV::get(std::string_view key) const {
	auto it = _data.find(std::string(key));
	if (it != _data.end()) {
		return it->second;
	}
	return std::nullopt;
}

void SaveKV::set(std::string_view key, std::string_view value) {
	_data[std::string(key)] = std::string(value);
	_dirty = true;
}

int SaveKV::getInt(std::string_view key, int default_val) const {
	auto val = get(key);
	if (!val) {
		return default_val;
	}
	try {
		return std::stoi(*val);
	} catch (...) {
		return default_val;
	}
}

bool SaveKV::getBool(std::string_view key, bool default_val) const {
	auto val = get(key);
	if (!val) {
		return default_val;
	}
	return *val == "true";
}

void SaveKV::setInt(std::string_view key, int value) {
	set(key, std::to_string(value));
}

void SaveKV::setBool(std::string_view key, bool value) {
	set(key, value ? "true" : "false");
}

void SaveKV::remove(std::string_view key) {
	_data.erase(std::string(key));
	_dirty = true;
}

void SaveKV::flush() {
	if (!_dirty) {
		return;
	}
	if (_flush_cooldown > 0) {
		return;
	}

	auto path = saveFilePath();
	nlohmann::json json_data;

	for (const auto &[key, value] : _data) {
		json_data[key] = value;
	}

	std::ofstream file(path);
	if (!file.is_open()) {
		std::cerr << "Failed to open save file at '" << path.string()
				  << "' for writing.\n";
		return;
	}

	try {
		file << json_data.dump();
		_dirty = false;
		_flush_cooldown = 24;
	} catch (std::exception &e) {
		std::cerr << "Failed to write save file: " << e.what() << "\n";
	}
}

} // namespace wf
