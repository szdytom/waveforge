#include "wforge/assets.h"
#include "wforge/elements.h"
#include "wforge/structures.h"
#include <format>
#include <memory>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include <format>
#include <iostream>
#endif

namespace wf {
namespace structure {

PixelShapedStructure::PixelShapedStructure(
	int x, int y, const PixelShape &shape
) noexcept
	: PositionedStructure(x, y)
	, _shape(shape)
	, _pixel_types(
		  std::make_unique<PixelTypeAndColor[]>(shape.width() * shape.height())
	  ) {
	for (int sy = 0; sy < shape.height(); ++sy) {
		for (int sx = 0; sx < shape.width(); ++sx) {
			_pixel_types[sy * shape.width() + sx] = pixelTypeFromColor(
				_shape.colorOf(sx, sy)
			);
			if (shape.isPOIPixel(sx, sy)) {
				poi.push_back({sx, sy});
			}
		}
	}
}

void PixelShapedStructure::setup(PixelWorld &world) {
	if (x < 0 || y < 0 || x + width() > world.width()
	    || y + height() > world.height()) {
		throw std::runtime_error(
			std::format(
				"PixelShapedStructure::setup: structure at ({}, {}) with size "
				"({}, {}) "
				"out of world bounds ({}, {})\n",
				x, y, width(), height(), world.width(), world.height()
			)
		);
	}

	for (int sy = 0; sy < height(); ++sy) {
		for (int sx = 0; sx < width(); ++sx) {
			int wx = x + sx;
			int wy = y + sy;
			auto p = _pixel_types[sy * width() + sx];
			if (p.type == PixelType::Air) {
				continue;
			}

			world.replacePixel(wx, wy, constructElementByType(p.type));
			if (p.color_index != 255) {
				world.tagOf(wx, wy).color_index = p.color_index;
			}
		}
	}
}

PixelType PixelShapedStructure::pixelTypeOf(int px, int py) const noexcept {
#ifndef NDEBUG
	if (px < 0 || px >= width() || py < 0 || py >= height()) {
		std::cerr << std::format(
			"PixelShapedStructure::pixelTypeOf: out of bounds access at ({}, "
			"{}) "
			"for size ({}, {})\n",
			px, py, width(), height()
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	return _pixel_types[py * width() + px].type;
}

void PixelShapedStructure::customRender(
	std::span<std::uint8_t> buf, const PixelWorld &world
) const noexcept {
	for (int sy = 0; sy < height(); ++sy) {
		for (int sx = 0; sx < width(); ++sx) {
			int world_x = x + sx;
			int world_y = y + sy;
			if (_pixel_types[sy * width() + sx].type != PixelType::Decoration) {
				continue;
			}

			int buf_index = (world_y * world.width() + world_x) * 4;
			sf::Color color = _shape.colorOf(sx, sy);
			buf[buf_index + 0] = color.r;
			buf[buf_index + 1] = color.g;
			buf[buf_index + 2] = color.b;
			buf[buf_index + 3] = color.a;
		}
	}
}

bool PixelShapedStructure::step(PixelWorld &world) const noexcept {
	for (int sy = 0; sy < height(); ++sy) {
		for (int sx = 0; sx < width(); ++sx) {
			int world_x = x + sx;
			int world_y = y + sy;
			PixelType expected_type = _pixel_types[sy * width() + sx].type;
			if (expected_type == PixelType::Air) {
				continue;
			}

			PixelTag tag = world.tagOf(world_x, world_y);
			if (tag.type != expected_type) {
				return false;
			}
		}
	}

	return true;
}

std::array<int, 4> PixelShapedStructure::queryBounds(
	const PixelWorld &world
) const noexcept {
	(void)world;
	return {x, y, width(), height()};
}

} // namespace structure
} // namespace wf
