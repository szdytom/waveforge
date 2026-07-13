#include "wforge/assets.h"
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <format>
#include <stdexcept>

namespace wf {

bool PixelFont::CharInfo::isValid() const noexcept {
	return x >= 0 && y >= 0;
}

PixelFont::PixelFont(
	int char_width, int char_height, std::string_view charset,
	const sf::Image &img
)
	: _char_width(char_width)
	, _char_height(char_height)
	, _image(img)
	, _texture(img) {
	// Precompute character positions
	int img_width = img.getSize().x;
	int img_height = img.getSize().y;

	if (img_width % char_width != 0) {
		throw std::invalid_argument(
			std::format(
				"Font image width {} is not a multiple of char width {}",
				img_width, char_width
			)
		);
	}

	if (img_height % char_height != 0) {
		throw std::invalid_argument(
			std::format(
				"Font image height {} is not a multiple of char height {}",
				img_height, char_height
			)
		);
	}

	int chars_per_row = img_width / char_width;
	for (size_t i = 0; i < charset.size(); ++i) {
		char c = charset[i];
		int char_x = (i % chars_per_row) * char_width;
		int char_y = (i / chars_per_row) * char_height;
		_char_info[c] = CharInfo{.x = char_x, .y = char_y};
	}

	for (int i = 0; i < 26; ++i) {
		unsigned char upper = 'A' + i;
		unsigned char lower = 'a' + i;

		// If lower case exists but upper case does not, map upper to lower
		// Similarly for the reverse
		if (hasChar(lower) && !hasChar(upper)) {
			_char_info[upper] = _char_info[lower];
		} else if (hasChar(upper) && !hasChar(lower)) {
			_char_info[lower] = _char_info[upper];
		}
	}
}

bool PixelFont::hasChar(char c) const noexcept {
	return _getCharInfo(c).isValid();
}

PixelFont::CharInfo PixelFont::_getCharInfo(char c) const noexcept {
	if (static_cast<unsigned char>(c) >= 128) {
		return CharInfo{};
	}
	return _char_info[static_cast<unsigned char>(c)];
}

void PixelFont::renderText(
	sf::RenderTarget &target, std::string_view text, sf::Color color, int x,
	int y, int scale, int size
) const {
	x *= scale;
	y *= scale;
	for (char c : text) {
		CharInfo char_info = _getCharInfo(c);
		if (!char_info.isValid()) {
			throw std::invalid_argument(
				std::format("Font does not contain character '{}'", c)
			);
		}
		sf::Sprite sprite(_texture);
		sprite.setTextureRect(
			sf::IntRect({char_info.x, char_info.y}, {_char_width, _char_height})
		);
		sprite.setColor(color);
		sprite.setPosition(sf::Vector2f(x, y));
		sprite.setScale(sf::Vector2f(scale * size, scale * size));
		target.draw(sprite);

		x += _char_width * scale * size;
	}
}

Generator<std::array<int, 2>> PixelFont::textBitmap(
	std::string_view text
) const noexcept {
	int x = 0;
	for (char c : text) {
		CharInfo char_info = _getCharInfo(c);
		if (!char_info.isValid()) {
			// We are not throwing in coroutines
			x += _char_width;
			continue;
		}

		for (int dy = 0; dy < _char_height; ++dy) {
			for (int dx = 0; dx < _char_width; ++dx) {
				sf::Color pixel_color = _image.getPixel(
					sf::Vector2u(char_info.x + dx, char_info.y + dy)
				);
				if (pixel_color.a != 0) {
					co_yield {x + dx, dy};
				}
			}
		}
		x += _char_width;
	}
}

} // namespace wf
