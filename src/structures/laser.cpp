#include "wforge/2d.h"
#include "wforge/assets.h"
#include "wforge/fallsand.h"
#include "wforge/structures.h"
#include <format>

namespace wf {
namespace structure {

namespace {

PixelShape &laserEmitterShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("laser-emitter/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

PixelShape &laserReceiverShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("laser-receiver/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

PixelShape &mirrorShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("mirror/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

void shootLaserBeam(
	PixelWorld &world, int start_x, int start_y, FacingDirection dir
) noexcept {
	constexpr int laser_heat_amount = 10;
	constexpr int max_reflections = 8;

	int cur_x = start_x;
	int cur_y = start_y;
	for (int _ = 0; _ < max_reflections; ++_) {
		int dx = xDeltaOf(dir);
		int dy = yDeltaOf(dir);
		for (; cur_x >= 0 && cur_x < world.width() && cur_y >= 0
		     && cur_y < world.height();
		     (cur_x += dx), (cur_y += dy)) {
			auto &pixel_tag = world.tagOf(cur_x, cur_y);

			// Solid and smoke can block the laser beam
			if (pixel_tag.pclass == PixelClass::Solid) {
				auto static_tag = world.staticTagOf(cur_x - dx, cur_y - dy);
				if (static_tag.is_reflective_surface) {
					cur_x -= dx;
					cur_y -= dy;
					break;
				}

				pixel_tag.heat += laser_heat_amount;
				return;
			}

			if (pixel_tag.type == PixelType::Smoke) {
				return;
			}

			// Activate laser
			world.activateLaserAt(cur_x, cur_y);
		}

		// Decide reflected direction
		bool reflected = false;
		for (auto next_dir : {rotate90CW(dir), rotate90CCW(dir)}) {
			int check_x = cur_x + xDeltaOf(next_dir);
			int check_y = cur_y + yDeltaOf(next_dir);
			if (!world.inBounds(check_x, check_y)) {
				continue;
			}

			auto &check_tag = world.tagOf(check_x, check_y);
			if (check_tag.pclass == PixelClass::Solid) {
				continue;
			}

			dir = next_dir;
			reflected = true;
			break;
		}

		if (!reflected) {
			break;
		}
	}
}

} // namespace

LaserEmitter::LaserEmitter(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, laserEmitterShape(dir)), _dir(dir) {
	if (poi.size() != 1) {
		throw std::runtime_error(
			std::format(
				"LaserEmitter: expected 1 POI, got {} POIs\n", poi.size()
			)
		);
	}
}

bool LaserEmitter::step(PixelWorld &world) noexcept {
	constexpr int laser_heat_amount = 10;

	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	if (!isPowered()) {
		return true;
	}

	int poi_x = x + poi[0][0];
	int poi_y = y + poi[0][1];
	shootLaserBeam(world, poi_x, poi_y, _dir);
	return true;
}

int LaserEmitter::priority() const noexcept {
	return 50;
}

LaserReceiver::LaserReceiver(int x, int y, FacingDirection dir)
	: OutputElectricalStructure(x, y, laserReceiverShape(dir)) {
	if (poi.size() != 1) {
		throw std::runtime_error(
			std::format(
				"LaserReceiver: expected 1 POI, got {} POIs\n", poi.size()
			)
		);
	}
}

bool LaserReceiver::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	// Check for laser beam at POI
	int poi_x = x + poi[0][0];
	int poi_y = y + poi[0][1];
	auto static_tag = world.staticTagOf(poi_x, poi_y);
	if (static_tag.laser_active) {
		if (!OutputElectricalStructure::step(world)) {
			return false;
		}
	}
	return true;
}

int LaserReceiver::priority() const noexcept {
	// Must run after LaserEmitter
	return 100;
}

Mirror::Mirror(int x, int y, FacingDirection dir)
	: PixelShapedStructure(x, y, mirrorShape(dir)) {
	if (poi.empty()) {
		throw std::runtime_error("Mirror: expected at least 1 POI, got 0\n");
	}
}

bool Mirror::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	for (const auto &point : poi) {
		int poi_x = x + point[0];
		int poi_y = y + point[1];
		world.staticTagOf(poi_x, poi_y).is_reflective_surface = true;
	}
	return true;
}

int Mirror::priority() const noexcept {
	return 0; // High priority to set reflective surfaces before laser steps
}

} // namespace structure
} // namespace wf
