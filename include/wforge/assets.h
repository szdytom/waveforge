#ifndef WFORGE_ASSETS_H
#define WFORGE_ASSETS_H

#include "wforge/2d.h"
#include "wforge/ctti.h"
#include "wforge/fallsand.h"
#include <SFML/Audio/Music.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <cstdint>
#include <generator>
#include <map>
#include <string>
#include <string_view>

namespace wf {

struct PixelTypeAndColor {
	PixelType type : 8;
	unsigned int color_index : 8;
};

// Determine pixel type and color index from a color
// Returns {PixelType::Decoration, 255} for not recognized colors
PixelTypeAndColor pixelTypeFromColor(const sf::Color &color) noexcept;

class PixelFont {
public:
	PixelFont(
		int char_width, int char_height, std::string_view charset,
		const sf::Image &img
	);

	int charWidth(int size = 1) const noexcept {
		return (_char_width - 1) * size + 1;
	}

	int charHeight(int size = 1) const noexcept {
		return (_char_height - 1) * size + 1;
	}

	void renderText(
		sf::RenderTarget &target, std::string_view text, sf::Color color, int x,
		int y, int scale, int size = 1
	) const;

	std::generator<std::array<int, 2>> textBitmap(
		std::string_view text
	) const noexcept;

	bool hasChar(char c) const noexcept;

private:
	struct CharInfo {
		int x = -1;
		int y = -1;

		bool isValid() const noexcept;
	};

	CharInfo _getCharInfo(char c) const noexcept;

	int _char_width;
	int _char_height;
	const sf::Image &_image;
	sf::Texture _texture;
	CharInfo _char_info[128]; // ASCII
};

// Bitmap shape of a pixel-based entity
class PixelShape {
public:
	PixelShape(const sf::Image &img) noexcept;
	PixelShape() noexcept;

	int width() const noexcept {
		return _width;
	}

	int height() const noexcept {
		return _height;
	}

	// not fully transparent at (x, y)
	bool hasPixel(int x, int y) const noexcept;
	sf::Color colorOf(int x, int y) const noexcept;

	bool isPOIPixel(int x, int y) const noexcept;

protected:
	int _width;
	int _height;
	const std::uint8_t *_data; // no ownership, ~static
};

class PixelAnimationFrames {
public:
	PixelAnimationFrames(
		sf::Texture &sprite_sheet, int frame_width, int frame_height
	);

	int length() const noexcept {
		return _length;
	}

	int frameWidth() const noexcept {
		return _frame_width;
	}

	int frameHeight() const noexcept {
		return _frame_height;
	}

	void render(
		sf::RenderTarget &target, int frame_index, int x, int y, int scale
	) const;

private:
	sf::Texture &_texture;

	int _frame_width;
	int _frame_height;
	int _length;
	std::vector<sf::IntRect> _frames;
};

// Trim fully-transparent borders from an image
sf::Image trimImage(const sf::Image &img);

// Assumes the input image is facing North
sf::Image rotateImageTo(const sf::Image &img, FacingDirection dir) noexcept;

struct Script {
	std::string source;
	std::string filename;
};

struct MusicCollection {
	std::string id;
	std::vector<sf::Music *> music;

	sf::Music *getRandomMusic() const;
};

// Cache of loaded assets, singleton
class AssetsManager {
public:
	static AssetsManager &instance() noexcept;

	// Load all assets from the `assets/` directory, respecting manifest.json
	// See assets/README.md and assets/manifest.json for details
	static void loadAllAssets();

	// Undefined behavior if asset not found or type mismatch
	// (Expect crash with diagnostic message in debug build)
	// Use `getAssetChecked` for unreliable asset retrieval
	template<typename T>
	T &getAsset(std::string_view id) {
		return *static_cast<T *>(_getAssetRawUnchecked(id, typeHash<T>()));
	}

	// nullptr if asset not found or type mismatch
	template<typename T>
	T *getAssetChecked(std::string_view id) {
		return static_cast<T *>(_getAssetRaw(id, typeHash<T>()));
	}

	// asset ownership is transferred to AssetsManager
	template<typename T>
	void cacheAsset(std::string id, T *asset) {
		_cacheAssetRaw(std::move(id), asset, typeHash<T>());
	}

	MusicCollection &getMusicCollection(const std::string &id);

private:
	AssetsManager() = default;

	void *_getAssetRaw(std::string_view id, std::size_t expected_type_hash);
	void *_getAssetRawUnchecked(
		std::string_view id, std::size_t expected_type_hash
	);
	void _cacheAssetRaw(std::string id, void *asset, std::size_t type_hash);
	void _sortAssetsIfNeeded();

	struct AssetEntry {
		std::size_t type_hash;
		void *asset; // owned pointer, but never deleted for now.
		std::string id;
	};

	bool _is_sorted = false;
	std::vector<AssetEntry> _assets;
	std::map<std::string, MusicCollection> _music_collections;
};

// Checkpoint area sprite, which animates based on progress
struct CheckpointSprite {
	CheckpointSprite(sf::Image &checkpoint_1, sf::Image &checkpoint_2);

	int width() const noexcept;
	int height() const noexcept;

	void render(
		sf::RenderTarget &target, int x, int y, int progress, int scale
	) const;

private:
	sf::Texture _ckeckpoint_1, _checkpoint_2;
};

// defined and set at program start, used to locate assets directory
extern std::filesystem::path _executable_path;

} // namespace wf

#endif // WFORGE_ASSETS_H
