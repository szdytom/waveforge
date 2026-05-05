#include "wforge/assets.h"
#include "wforge/colorpalette.h"
#include "wforge/level.h"
#include "wforge/xoroshiro.h"
#include <SFML/Audio.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#endif

namespace fs = std::filesystem;

namespace wf {

PixelTypeAndColor pixelTypeFromColor(const sf::Color &color) noexcept {
	switch (color.toInteger()) {
	case packColorByName("Air"):
	case packColorByName("POIMarker"):
		return {PixelType::Air, colorIndexOf("Air")};

	case packColorByName("Stone1"):
		return {PixelType::Stone, colorIndexOf("Stone1")};
	case packColorByName("Stone2"):
		return {PixelType::Stone, colorIndexOf("Stone2")};
	case packColorByName("Stone3"):
		return {PixelType::Stone, colorIndexOf("Stone3")};
	case packColorByName("Stone4"):
		return {PixelType::Stone, colorIndexOf("Stone4")};

	case packColorByName("Wood1"):
		return {PixelType::Wood, colorIndexOf("Wood1")};
	case packColorByName("Wood2"):
		return {PixelType::Wood, colorIndexOf("Wood2")};
	case packColorByName("Wood3"):
		return {PixelType::Wood, colorIndexOf("Wood3")};

	case packColorByName("Copper1"):
		return {PixelType::Copper, colorIndexOf("Copper1")};
	case packColorByName("Copper2"):
		return {PixelType::Copper, colorIndexOf("Copper2")};
	case packColorByName("Copper3"):
		return {PixelType::Copper, colorIndexOf("Copper3")};
	case packColorByName("Copper4"):
		return {PixelType::Copper, colorIndexOf("Copper4")};
	case packColorByName("Copper5"):
		return {PixelType::Copper, colorIndexOf("Copper5")};

	case packColorByName("Sand1"):
		return {PixelType::Sand, colorIndexOf("Sand1")};

	case packColorByName("Sand2"):
		return {PixelType::Sand, colorIndexOf("Sand2")};

	case packColorByName("Water"):
	case packColorByNameNoAlpha("Water"):
		return {PixelType::Water, colorIndexOf("Water")};

	case packColorByName("Oil"):
	case packColorByNameNoAlpha("Oil"):
		return {PixelType::Oil, colorIndexOf("Oil")};

	default:
		return {PixelType::Decoration, 255};
	}
}

PixelShape::PixelShape(const sf::Image &img) noexcept
	: _width(img.getSize().x)
	, _height(img.getSize().y)
	, _data(img.getPixelsPtr()) {}

PixelShape::PixelShape() noexcept: _width(0), _height(0), _data(nullptr) {}

bool PixelShape::hasPixel(int x, int y) const noexcept {
	if (colorOf(x, y).a == 0) {
		return false;
	}

	return !isPOIPixel(x, y);
}

sf::Color PixelShape::colorOf(int x, int y) const noexcept {
#ifndef NDEBUG
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		std::cerr << std::format(
			"PixelMap::colorOf: index out of bounds: x = {}, y = {}, width = "
			"{}, height = {}\n",
			x, y, _width, _height
		);
		cpptrace::generate_trace().print();
		std::abort();
	}
#endif
	// Data: size is _width * _height * 4, each pixel is 4 bytes (RGBA)
	std::uint8_t r = _data[(y * _width + x) * 4 + 0];
	std::uint8_t g = _data[(y * _width + x) * 4 + 1];
	std::uint8_t b = _data[(y * _width + x) * 4 + 2];
	std::uint8_t a = _data[(y * _width + x) * 4 + 3];
	return sf::Color(r, g, b, a);
}

bool PixelShape::isPOIPixel(int x, int y) const noexcept {
	// Almost transparent red indicates POI
	return colorOf(x, y) == colorOfName("POIMarker");
}

sf::Image trimImage(const sf::Image &img) {
	int xmin = img.getSize().x;
	int xmax = -1;
	int ymin = img.getSize().y;
	int ymax = -1;

	for (unsigned int y = 0; y < img.getSize().y; ++y) {
		for (unsigned int x = 0; x < img.getSize().x; ++x) {
			auto color = img.getPixel({x, y});
			if (color.a != 0) {
				xmin = std::min<int>(xmin, x);
				xmax = std::max<int>(xmax, x);
				ymin = std::min<int>(ymin, y);
				ymax = std::max<int>(ymax, y);
			}
		}
	}

	if (xmax < xmin || ymax < ymin) {
		// fully transparent
		return {};
	}

	unsigned int width = xmax - xmin + 1;
	unsigned int height = ymax - ymin + 1;
	sf::Image trimmed({width, height}, sf::Color(0, 0, 0, 0));
	if (!trimmed.copy(
			img, {0, 0},
			sf::IntRect(
				{xmin, ymin},
				{static_cast<int>(width), static_cast<int>(height)}
			),
			true
		)) {
		throw std::runtime_error("trimImage: failed to copy image data");
	}
	return trimmed;
}

sf::Image rotateImageTo(const sf::Image &img, FacingDirection dir) noexcept {
	auto width = img.getSize().x;
	auto height = img.getSize().y;

	unsigned int rotated_width, rotated_height;
	if (dir == FacingDirection::North || dir == FacingDirection::South) {
		rotated_width = width;
		rotated_height = height;
	} else { // East or West
		rotated_width = height;
		rotated_height = width;
	}

	sf::Image rotated({rotated_width, rotated_height}, sf::Color(0, 0, 0, 0));
	for (unsigned int y = 0; y < height; ++y) {
		for (unsigned int x = 0; x < width; ++x) {
			sf::Color color = img.getPixel({x, y});
			unsigned int rx, ry;
			switch (dir) {
			case FacingDirection::North:
				rx = x;
				ry = y;
				break;
			case FacingDirection::East:
				rx = height - 1 - y;
				ry = x;
				break;

			case FacingDirection::South:
				rx = width - 1 - x;
				ry = height - 1 - y;
				break;

			case FacingDirection::West:
				rx = y;
				ry = width - 1 - x;
				break;
			}

			rotated.setPixel({rx, ry}, color);
		}
	}
	return rotated;
}

sf::Music *MusicCollection::getRandomMusic() const {
	// Quick path
	if (music.empty()) {
		return nullptr;
	}

	if (music.size() == 1) {
		return music[0];
	}

	// Random selection
	auto &rng = Xoroshiro128PP::globalInstance();
	std::uniform_int_distribution<std::size_t> dist(0, music.size() - 1);
	return music[dist(rng)];
}

AssetsManager &AssetsManager::instance() noexcept {
	static AssetsManager mgr;
	return mgr;
}

void *AssetsManager::_getAssetRaw(const std::string &id) {
	auto it = _asset_cache.find(id);
	if (it == _asset_cache.end()) {
		throw std::invalid_argument(
			std::format("AssetsManager: asset not found: {}", id)
		);
	}

	return it->second;
}

void AssetsManager::_cacheAssetRaw(const std::string &id, void *asset) {
	if (_asset_cache.find(id) != _asset_cache.end()) {
		throw std::invalid_argument(
			std::format("AssetsManager: asset ID '{}' is already cached", id)
		);
	}
	_asset_cache[id] = asset;
}

MusicCollection &AssetsManager::getMusicCollection(const std::string &id) {
	auto it = _music_collections.find(id);
	if (it == _music_collections.end()) {
		auto [it, _] = _music_collections.insert({id, MusicCollection{id, {}}});
		return it->second;
	}
	return it->second;
}

namespace {

std::filesystem::path findAssetsRoot() {
	if (auto env_path = std::getenv("WAVEFORGE_ASSETS_PATH")) {
		fs::path res(env_path);
		fs::path manifest_path = res / "manifest.json";
		if (fs::exists(manifest_path)) {
			return fs::absolute(res);
		}

		throw std::runtime_error(
			std::format(
				"AssetsManager: WAVEFORGE_ASSETS_PATH is set to '{}', but file "
				"'{}' does not exist.",
				res.string(), manifest_path.string()
			)
		);
	}

	auto cur_path = fs::current_path();
	fs::path possible_pathes[] = {
		cur_path / "assets",
		cur_path.parent_path() / "assets",
		wf::_executable_path / "assets",
		wf::_executable_path.parent_path() / "assets",
#ifdef __linux__
		fs::path("/usr/share/waveforge/assets"),
		fs::path("/usr/local/share/waveforge/assets"),
#endif
	};

	for (const auto &res : possible_pathes) {
		fs::path manifest_path = res / "manifest.json";
		if (fs::exists(manifest_path)) {
			return fs::absolute(res);
		}
	}

	std::cerr << "AssetsManager: could not find assets root. Tried:\n";
	for (const auto &res : possible_pathes) {
		std::cerr << "  " << res << "\n";
	}
	throw std::runtime_error("AssetsManager: could not find assets root.");
}

constexpr int current_manifest_format = 1;

using operationFunc = void (*)(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
);

void fJSON(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &file = entry.at("file");
	auto file_path = assets_root / file;
	const std::string &id = entry.at("id");

	std::ifstream file_stream(file_path);
	if (!file_stream.is_open()) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to open JSON asset file '{}'",
				file_path.string()
			)
		);
	}

	auto json_data = new nlohmann::json(nlohmann::json::parse(file_stream));
	mgr.cacheAsset(id, json_data);
}

void fImage(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &file = entry.at("file");
	auto file_path = assets_root / file;
	const std::string &id = entry.at("id");
	auto img = new sf::Image();
	if (!img->loadFromFile(file_path)) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to load image asset from file '{}'",
				file_path.string()
			)
		);
	}

	mgr.cacheAsset(id, img);
}

void fTexture(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	const auto &img = mgr.getAsset<sf::Image>(input_id);
	const std::string &id = entry.at("id");

	auto texture = new sf::Texture();
	if (!texture->loadFromImage(img)) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to create texture from image asset '{}'",
				input_id
			)
		);
	}
	texture->setSmooth(false);

	mgr.cacheAsset(id, texture);
}

void fMusic(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &file = entry.at("file");
	auto file_path = assets_root / file;
	const std::string &id = entry.at("id");

	auto music = new sf::Music();
	if (!music->openFromFile(file_path)) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to load music asset from file '{}'",
				file_path.string()
			)
		);
	}

	mgr.cacheAsset(id, music);

	if (entry.contains("collections")) {
		for (const auto &collection_name : entry.at("collections")) {
			auto &collection = mgr.getMusicCollection(
				collection_name.get<std::string>()
			);
			collection.music.push_back(music);
		}
	}
}

void fSound(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &file = entry.at("file");
	auto file_path = assets_root / file;
	const std::string &id = entry.at("id");

	auto sound_buffer = new sf::SoundBuffer();

	if (!sound_buffer->loadFromFile(file_path)) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to load sound asset from file '{}'",
				file_path.string()
			)
		);
	}

	mgr.cacheAsset(id, sound_buffer);
}

void fTrimImage(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	const auto &img = mgr.getAsset<sf::Image>(input_id);
	const std::string &id = entry.at("id");
	auto trimmed = new sf::Image(trimImage(img));
	mgr.cacheAsset(id, trimmed);
}

void fImageAllRotated(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	auto &img = mgr.getAsset<sf::Image>(input_id);
	const std::string &id = entry.at("id");

	auto imgs = new std::array<sf::Image, 4>;
	for (int i = 0; i < 4; ++i) {
		imgs->at(i) = rotateImageTo(
			img, static_cast<FacingDirection>(static_cast<std::uint8_t>(i))
		);
	}

	mgr.cacheAsset(id, imgs);
}

void fPixelShape(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	auto &img = mgr.getAsset<sf::Image>(input_id);
	const std::string &id = entry.at("id");
	auto shape = new PixelShape(img);
	mgr.cacheAsset(id, shape);
}

void fPixelShapeAllRotated(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	auto &img = mgr.getAsset<std::array<sf::Image, 4>>(input_id);
	const std::string &id = entry.at("id");

	auto shapes = new PixelShape[4];
	for (int i = 0; i < 4; ++i) {
		shapes[i] = PixelShape(img[i]);
	}

	mgr.cacheAsset(id, shapes);
}

void fCheckpointSprite(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	auto &img1 = mgr.getAsset<sf::Image>("checkpoint/image_1");
	auto &img2 = mgr.getAsset<sf::Image>("checkpoint/image_2");
	const std::string &id = entry.at("id");
	auto checkpoint_sprite = new CheckpointSprite(img1, img2);
	mgr.cacheAsset(id, checkpoint_sprite);
}

void fLevelMetadata(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	constexpr int current_levelmetadata_format = 1;

	const std::string &file = entry.at("file");
	auto file_path = assets_root / file;
	const std::string &id = entry.at("id");

	std::ifstream file_stream(file_path);
	if (!file_stream.is_open()) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to open level metadata file '{}'",
				file_path.string()
			)
		);
	}

	nlohmann::json json_data = nlohmann::json::parse(file_stream);
	if (json_data.at("format").get<int>() != current_levelmetadata_format) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: unsupported level metadata format: {}, "
				"expected {}",
				json_data.at("format").get<int>(), current_levelmetadata_format
			)
		);
	}

	const auto &metadata_json = json_data.at("metadata");

	LevelMetadata *metadata = new LevelMetadata{
		.map_id = json_data.at("map"),
		.name = metadata_json.at("level_name"),
		.description = metadata_json.value("description", ""),
		.author = metadata_json.value("author", ""),
		.difficulty = LevelMetadata::parseDifficulty(
			metadata_json.value("difficulty", "unkown")
		),
		.minimap_texture = &mgr.getAsset<sf::Texture>(
			metadata_json.value("minimap_asset_id", "level/minimap/fallback")
		)
	};

	for (const auto &item_entry : json_data.at("items")) {
		const std::string &item_name = item_entry.at("id");
		int item_count = item_entry.at("amount");
		metadata->items.emplace_back(item_name, item_count);
	}

	mgr.cacheAsset(id, metadata);
}

void fFont(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	const auto &img = mgr.getAsset<sf::Image>(input_id);
	const std::string &id = entry.at("id");

	const auto &font_size = entry.at("size");
	int char_width = font_size.at("width");
	int char_height = font_size.at("height");
	std::string charset = entry.at("charset");

	auto font = new PixelFont(char_width, char_height, std::move(charset), img);
	mgr.cacheAsset(id, font);
}

void fAnimationFrames(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &input_id = entry.at("input");
	auto &texture = mgr.getAsset<sf::Texture>(input_id);
	const std::string &id = entry.at("id");

	const auto &frame_size = entry.at("frame_size");
	int frame_width = frame_size.at("width");
	int frame_height = frame_size.at("height");

	auto animation_frames = new PixelAnimationFrames(
		texture, frame_width, frame_height
	);
	mgr.cacheAsset(id, animation_frames);
}

void fLevelSequence(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &id = entry.at("id");
	LevelSequence *level_seq = new LevelSequence();

	for (const auto &level_id : entry.at("levels")) {
		auto level_metadata = &mgr.getAsset<LevelMetadata>(level_id);
		level_metadata->index = static_cast<int>(level_seq->levels.size());
		level_seq->levels.push_back(level_metadata);
	}

	mgr.cacheAsset(id, level_seq);
}

void fLoadJS(
	const nlohmann::json &entry, const fs::path &assets_root, AssetsManager &mgr
) {
	const std::string &file = entry.at("file");
	auto file_path = assets_root / file;
	const std::string &id = entry.at("id");

	std::ifstream file_stream(file_path);
	if (!file_stream.is_open()) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to open JS file '{}'", file_path.string()
			)
		);
	}

	std::string content(
		(std::istreambuf_iterator<char>(file_stream)),
		std::istreambuf_iterator<char>()
	);

	auto js_source = new std::string(std::move(content));
	mgr.cacheAsset(id, js_source);
}

} // namespace

void AssetsManager::loadAllAssets() {
	auto assets_root = findAssetsRoot();
	std::cerr << "AssetsManager: loading assets from " << assets_root << "\n";

	auto start_loading_time = std::chrono::steady_clock::now();

	std::ifstream manifest_file(assets_root / "manifest.json");
	if (!manifest_file.is_open()) {
		throw std::runtime_error(
			std::format(
				"AssetsManager: failed to open manifest file at '{}'",
				(assets_root / "manifest.json").string()
			)
		);
	}

	std::unordered_map<std::string, operationFunc> operations = {
		{"json", fJSON},
		{"image", fImage},
		{"create-texture", fTexture},
		{"music", fMusic},
		{"sound", fSound},
		{"trim-image", fTrimImage},
		{"create-image-of-all-facings", fImageAllRotated},
		{"calculate-shape", fPixelShape},
		{"create-pixel-shape-of-all-facings", fPixelShapeAllRotated},
		{"create-checkpoint-sprite", fCheckpointSprite},
		{"level-metadata", fLevelMetadata},
		{"font", fFont},
		{"animation", fAnimationFrames},
		{"level-sequence", fLevelSequence},
		{"load-js", fLoadJS},
	};

	nlohmann::json manifest = nlohmann::json::parse(manifest_file);
	AssetsManager &mgr = AssetsManager::instance();

	if (manifest.at("format").get<int>() != current_manifest_format) {
		throw std::runtime_error(
			std::format(
				"Unsupported manifest format: {}, expected {}",
				manifest.at("format").get<int>(), current_manifest_format
			)
		);
	}

	const auto &entries = manifest.at("sequence");
	int total_entries = entries.size();
	int current_entry = 0;
	for (const auto &entry : entries) {
		const std::string &op_name = entry.at("type");
		const std::string &description = entry.at("description");
		current_entry += 1;
		std::cerr << std::format(
			"[{:02}/{:02}] {}...\n", current_entry, total_entries, description
		);
		auto it = operations.find(op_name);
		if (it == operations.end()) {
			throw std::runtime_error(
				std::format("Unknown asset operation type: '{}'", op_name)
			);
		}

		auto func = it->second;
		func(entry, assets_root, mgr);
	}

	auto dur = std::chrono::steady_clock::now() - start_loading_time;
	std::cerr << std::format(
		"Successfully executed {} asset loading operations in {} ms.\n",
		total_entries,
		std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
	);
}

} // namespace wf
