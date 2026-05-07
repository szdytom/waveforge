#include "wforge/elements.h"
#include "wforge/fallsand.h"
#include "wforge/structures.h"
#include <cstdlib>

namespace wf::structure {

namespace {

constexpr int gate_open_speed = 3; // ticks per pixel, i.e. larger is slower

PixelShape &gateShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("gate/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

PixelShape &gateWallShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("gate/wall/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

} // namespace

Gate::Gate(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, gateShape(dir))
	, _dir(dir)
	, _open_state(0)
	, _gate_wall_shape(gateWallShape(dir)) {
	if (poi.size() != 2) {
		throw std::runtime_error(
			std::format("Gate: expected 2 POIs, got {} POIs", poi.size())
		);
	}

	_gate_length = std::abs(
		_gate_wall_shape.width() * xDeltaOf(dir)
		+ _gate_wall_shape.height() * yDeltaOf(dir)
	);

	int poi_delta = std::abs(
		(poi[1][0] - poi[0][0]) * xDeltaOf(dir)
		+ (poi[1][1] - poi[0][1]) * yDeltaOf(dir)
	);

	_max_open_length = _gate_length - poi_delta - 1;
	if (_max_open_length <= 0) {
		throw std::runtime_error(
			"Gate: invalid POI configuration, cannot open"
		);
	}

	// Decide which poi is the base placement point
	int poi_idx;
	if ((poi[0][0] - poi[1][0]) * xDeltaOf(dir)
	        + (poi[0][1] - poi[1][1]) * yDeltaOf(dir)
	    < 0) {
		poi_idx = 0;
	} else {
		poi_idx = 1;
	}

	_base_place_x = poi[poi_idx][0] + x;
	_base_place_y = poi[poi_idx][1] + y;

	// Fix base placement point to anchor at top-left
	switch (dir) {
	case FacingDirection::North:
		_base_place_y -= _gate_length - 1;
		break;

	case FacingDirection::East:
		break;

	case FacingDirection::South:
		_base_place_x -= _gate_wall_shape.width() - 1;
		break;

	case FacingDirection::West:
		_base_place_x -= _gate_length - 1;
		_base_place_y -= _gate_wall_shape.height() - 1;
		break;
	}

	_gate_wall_pixel_types = std::make_unique<PixelTypeAndColor[]>(
		_gate_wall_shape.width() * _gate_wall_shape.height()
	);
	for (int i = 0; i < _gate_wall_shape.width(); ++i) {
		for (int j = 0; j < _gate_wall_shape.height(); ++j) {
			_gate_wall_pixel_types[j * _gate_wall_shape.width() + i]
				= pixelTypeFromColor(_gate_wall_shape.colorOf(i, j));
		}
	}
}

void Gate::setup(PixelWorld &world) {
	PixelShapedStructure::setup(world);
	int block_x = -1, block_y = -1;
	if (!_canPlaceAt(world, 0, &block_x, &block_y)) {
		throw std::runtime_error(
			std::format(
				"Gate: cannot place: blocked at ({}, {})", block_x, block_y
			)
		);
	}
	_placeTo(world, 0, false);
}

int Gate::_openProgress() const noexcept {
	return _open_state / gate_open_speed;
}

bool Gate::_canPlaceAt(
	PixelWorld &world, int progress, int *block_x, int *block_y
) const noexcept {
	int dx = xDeltaOf(_dir);
	int dy = yDeltaOf(_dir);

	int offset_x = -progress * dx;
	int offset_y = -progress * dy;

	for (int i = 0; i < _gate_wall_shape.width(); ++i) {
		int wx = _base_place_x + offset_x + i;
		if (wx < 0 || wx >= world.width()) {
			continue;
		}

		for (int j = 0; j < _gate_wall_shape.height(); ++j) {
			if (!_gate_wall_shape.hasPixel(i, j)) {
				continue;
			}
			int wy = _base_place_y + offset_y + j;
			if (wy < 0 || wy >= world.height()) {
				continue;
			}

			auto tag = world.tagOf(wx, wy);
			if (tag.pclass == PixelClass::Solid) {
				if (block_x) {
					*block_x = wx;
				}

				if (block_y) {
					*block_y = wy;
				}
				return false;
			}

			if (world.isExternalEntityPresent(wx, wy)) {
				return false;
			}
		}
	}
	return true;
}

void Gate::_placeTo(
	PixelWorld &world, int progress, bool remove
) const noexcept {
	int dx = xDeltaOf(_dir);
	int dy = yDeltaOf(_dir);

	int offset_x = -progress * dx;
	int offset_y = -progress * dy;

	for (int i = 0; i < _gate_wall_shape.width(); ++i) {
		int wx = _base_place_x + offset_x + i;
		if (wx < 0 || wx >= world.width()) {
			continue;
		}

		for (int j = 0; j < _gate_wall_shape.height(); ++j) {
			if (!_gate_wall_shape.hasPixel(i, j)) {
				continue;
			}
			int wy = _base_place_y + offset_y + j;
			if (wy < 0 || wy >= world.height()) {
				continue;
			}

			auto &tag = world.tagOf(wx, wy);
			int old_heat = tag.heat;
			if (remove) {
				world.replacePixelWithAir(wx, wy);
			} else {
				auto p = _gate_wall_pixel_types
					[j * _gate_wall_shape.width() + i];
				world.replacePixel(wx, wy, constructElementByType(p.type));

				if (tag.color_index != 255) {
					tag.color_index = p.color_index;
				}
			}
			tag.heat = old_heat;
		}
	}
}

bool Gate::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	int move_dir = isPowered() ? 1 : -1;
	int old_progress = _openProgress();
	_open_state = std::clamp(
		_open_state + move_dir, 0, _max_open_length * gate_open_speed
	);
	int new_progress = _openProgress();

	if (new_progress != old_progress) {
		_placeTo(world, old_progress, true);
		if (_canPlaceAt(world, new_progress, nullptr, nullptr)) {
			_placeTo(world, new_progress, false);
		} else {
			_open_state = old_progress * gate_open_speed;
			_placeTo(world, old_progress, false);
		}
	}
	return true;
}

void Gate::customRender(
	std::span<std::uint8_t> buf, const PixelWorld &world
) const noexcept {
	PixelShapedStructure::customRender(buf, world);
	int progress = _openProgress();
	int dx = xDeltaOf(_dir);
	int dy = yDeltaOf(_dir);

	int offset_x = -progress * dx;
	int offset_y = -progress * dy;

	for (int i = 0; i < _gate_wall_shape.width(); ++i) {
		int wx = _base_place_x + offset_x + i;
		if (wx < 0 || wx >= world.width()) {
			continue;
		}

		for (int j = 0; j < _gate_wall_shape.height(); ++j) {
			if (!_gate_wall_shape.hasPixel(i, j)) {
				continue;
			}
			int wy = _base_place_y + offset_y + j;
			if (wy < 0 || wy >= world.height()) {
				continue;
			}

			if (world.tagOf(wx, wy).type != PixelType::Decoration) {
				continue;
			}

			int buf_index = (wy * world.width() + wx) * 4;
			sf::Color color = _gate_wall_shape.colorOf(i, j);
			buf[buf_index + 0] = color.r;
			buf[buf_index + 1] = color.g;
			buf[buf_index + 2] = color.b;
			buf[buf_index + 3] = color.a;
		}
	}
}

int Gate::priority() const noexcept {
	return 5; // gates must be earlier than lasers
}

} // namespace wf::structure
