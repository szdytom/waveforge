#include "wforge/2d.h"
#include "wforge/colorpalette.h"
#include "wforge/elements.h"
#include "wforge/fallsand.h"
#include "wforge/xoroshiro.h"
#include <SFML/Graphics/BlendMode.hpp>
#include <algorithm>
#include <memory>
#include <proxy/proxy.h>
#include <random>
#include <utility>
#include <vector>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include <format>
#include <iostream>
#endif

namespace wf {

bool isDenser(PixelType a, PixelType b) noexcept {
	return std::to_underlying(a) > std::to_underlying(b);
}

bool isDenserOrEqual(PixelType a, PixelType b) noexcept {
	return std::to_underlying(a) >= std::to_underlying(b);
}

PixelWorld::PixelWorld() noexcept: _width(0), _height(0) {}

PixelWorld::PixelWorld(int width, int height) noexcept
	: _width(width)
	, _height(height)
	, _tags(std::make_unique<PixelTag[]>(width * height))
	, _elements(std::make_unique<PixelElement[]>(width * height))
	, _static_tags(std::make_unique<StaticPixelTag[]>(width * height)) {
	PixelTag airTag = element::Air().newTag();
	for (int i = 0; i < width * height; ++i) {
		_tags[i] = airTag;
		_elements[i] = element::Air::create();
	}
}

PixelTag PixelWorld::tagOf(int x, int y) const noexcept {
#ifndef NDEBUG
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		std::cerr << std::format(
			"PixelWorld::tagOf: index out of bounds: x = {}, y = {}, width = "
			"{}, height = {}\n",
			x, y, _width, _height
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif
	return _tags[y * _width + x];
}

PixelTag &PixelWorld::tagOf(int x, int y) noexcept {
#ifndef NDEBUG
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		std::cerr << std::format(
			"PixelWorld::tagOf: index out of bounds: x = {}, y = {}, width = "
			"{}, height = {}\n",
			x, y, _width, _height
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif
	return _tags[y * _width + x];
}

PixelElement &PixelWorld::elementOf(int x, int y) noexcept {
#ifndef NDEBUG
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		std::cerr << std::format(
			"PixelWorld::elementOf: index out of bounds: x = {}, y = {}, width "
			"= "
			"{}, height = {}\n",
			x, y, _width, _height
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif
	return _elements[y * _width + x];
}

StaticPixelTag PixelWorld::staticTagOf(int x, int y) const noexcept {
#ifndef NDEBUG
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		std::cerr << std::format(
			"PixelWorld::staticTagOf: index out of bounds: x = {}, y = {}, "
			"width = {}, height = {}\n",
			x, y, _width, _height
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif
	return _static_tags[y * _width + x];
}

StaticPixelTag &PixelWorld::staticTagOf(int x, int y) noexcept {
#ifndef NDEBUG
	if (x < 0 || x >= _width || y < 0 || y >= _height) {
		std::cerr << std::format(
			"PixelWorld::staticTagOf: index out of bounds: x = {}, y = {}, "
			"width = {}, height = {}\n",
			x, y, _width, _height
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif
	return _static_tags[y * _width + x];
}

void PixelWorld::activateLaserAt(int x, int y) noexcept {
	auto &stag = staticTagOf(x, y);
	stag.laser_active = true;
	for (auto [nx, ny] : neighbors4({x, y}, {_width, _height})) {
		auto &nstag = staticTagOf(nx, ny);
		nstag.laser_stroke = true;
	}
}

bool PixelWorld::isExternalEntityPresent(int x, int y) const noexcept {
	return staticTagOf(x, y).external_entity_present;
}

void PixelWorld::swapPixels(int x1, int y1, int x2, int y2) noexcept {
	using std::swap; // ADL two steps
	swap(tagOf(x1, y1), tagOf(x2, y2));
	swap(elementOf(x1, y1), elementOf(x2, y2));
}

void PixelWorld::swapFluids(int x1, int y1, int x2, int y2) noexcept {
	// std::swap doesn't work for bitfields
	auto &tag1 = tagOf(x1, y1);
	auto &tag2 = tagOf(x2, y2);
	int t = tag1.fluid_dir;
	tag1.fluid_dir = tag2.fluid_dir;
	tag2.fluid_dir = t;

	using std::swap; // ADL two steps
	swap(tag1, tag2);
	swap(elementOf(x1, y1), elementOf(x2, y2));
}

void PixelWorld::replacePixel(int x, int y, PixelElement new_pixel) noexcept {
	tagOf(x, y) = new_pixel->newTag();
	elementOf(x, y) = std::move(new_pixel);
}

void PixelWorld::replacePixel(
	int x, int y, PixelElement new_pixel, PixelTag new_tag
) noexcept {
	tagOf(x, y) = new_tag;
	elementOf(x, y) = std::move(new_pixel);
}

void PixelWorld::replacePixelWithAir(int x, int y) noexcept {
	replacePixel(x, y, element::Air::create());
}

void PixelWorld::chargeElement(int x, int y) noexcept {
	elementOf(x, y)->onCharge(*this, x, y);
}

bool PixelWorld::typeOfIs(int x, int y, PixelType ptype) const noexcept {
	return tagOf(x, y).type == ptype;
}

bool PixelWorld::classOfIs(int x, int y, PixelClass pclass) const noexcept {
	return tagOf(x, y).pclass == pclass;
}

void PixelWorld::resetDirtyFlags() noexcept {
	for (int i = 0; i < _width * _height; ++i) {
		_tags[i].dirty = false;
	}
}

void PixelWorld::step() noexcept {
	for (int i = 0; i < _width * _height; ++i) {
		_static_tags[i].laser_active = false;
		_static_tags[i].laser_stroke = false;
		if (_tags[i].electric_power > 0) {
			_tags[i].electric_power -= 1;
		}
	}

	fluidAnalysisStep();
	thermalAnalysisStep();

	std::vector<StructureEntity> next_structures;
	next_structures.reserve(_structures.size());
	for (auto &s : _structures) {
		if (s->step(*this)) {
			next_structures.push_back(std::move(s));
		}
	}
	_structures = std::move(next_structures);

	auto &rng = Xoroshiro128PP::globalInstance();
	for (int y = _height - 1; y >= 0; --y) {
		bool reverse_x = (rng.next() % 2 == 0);
		for (int ix = 0; ix < _width; ++ix) {
			int x = reverse_x ? (_width - 1 - ix) : ix;
			while (!tagOf(x, y).dirty) {
				tagOf(x, y).dirty = true;
				elementOf(x, y)->step(*this, x, y);
			}
		}
	}

	resetDirtyFlags();
}

void PixelWorld::resetEntityPresenceTags() noexcept {
	for (int i = 0; i < _width * _height; ++i) {
		_static_tags[i].external_entity_present = false;
	}
}

void PixelWorld::addStructure(StructureEntity structure) {
	structure->setup(*this);

	// Insert structure based on priority
	// Smaller priority value = higher priority = earlier position in vector
	auto it = std::lower_bound(
		_structures.begin(), _structures.end(), structure,
		[](const StructureEntity &a, const StructureEntity &b) {
		return a->priority() < b->priority();
	}
	);
	_structures.insert(it, std::move(structure));
}

void PixelWorld::renderToBuffer(std::span<std::uint8_t> buf) const noexcept {
#ifndef NDEBUG
	if (buf.size() != _width * _height * 4) {
		std::cerr << std::format(
			"PixelWorld::renderToTexture: buffer size mismatch: expected {}, "
			"got "
			"{}\n",
			_width * _height * 4, buf.size()
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	constexpr unsigned int render_electric_power_threshold = 12;

	auto &rng = Xoroshiro128PP::globalInstance();
	std::uniform_int_distribution<int> dist(0, 5);
	for (int i = 0; i < _width * _height; ++i) {
		int color_idx;
		if (_tags[i].ignited) {
			int rd = dist(rng);
			if (rd == 0) {
				color_idx = colorIndexOf("Fire1");
			} else if (rd <= 4) {
				color_idx = colorIndexOf("Fire2");
			} else {
				color_idx = colorIndexOf("Fire3");
			}
		} else {
			color_idx = _tags[i].color_index;
		}

		sf::Color color;
		if (_static_tags[i].laser_active) {
			color = laserBlendedColorOfIndex(color_idx);
		} else if (_tags[i].electric_power >= render_electric_power_threshold) {
			color = colorPaletteOfIndex(color_idx).active_color;
		} else if (
			_tags[i].type == PixelType::Air && _static_tags[i].laser_stroke
		) {
			color = colorOfName("LaserStroke");
		} else {
			color = colorOfIndex(color_idx);
		}

		buf[i * 4 + 0] = color.r;
		buf[i * 4 + 1] = color.g;
		buf[i * 4 + 2] = color.b;
		buf[i * 4 + 3] = color.a;
	}

	for (auto &s : _structures) {
		s->customRender(buf, *this);
	}
}

} // namespace wf
