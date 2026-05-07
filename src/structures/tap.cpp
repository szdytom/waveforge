#include "wforge/2d.h"
#include "wforge/elements.h"
#include "wforge/structures.h"

namespace wf::structure {

namespace {

PixelShape &loadWaterTapShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("water-tap/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

PixelShape &loadOilTapShape(FacingDirection dir) {
	// TODO: draw a different shape for oil tap
	// For now, use the same shape as water tap
	return loadWaterTapShape(dir);
}

} // namespace

WaterTap::WaterTap(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, loadWaterTapShape(dir)) {}

bool WaterTap::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	if (isPowered()) {
		for (const auto &[px, py] : poi) {
			int wx = x + px;
			int wy = y + py;
			auto &tag = world.tagOf(wx, wy);
			if (tag.pclass == PixelClass::Gas) {
				world.replacePixel(wx, wy, element::Water::create());
			}
		}
	}
	return true;
}

int WaterTap::priority() const noexcept {
	return 5;
}

OilTap::OilTap(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, loadOilTapShape(dir)) {}

bool OilTap::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	if (isPowered()) {
		for (const auto &[px, py] : poi) {
			int wx = x + px;
			int wy = y + py;
			auto &tag = world.tagOf(wx, wy);
			if (tag.pclass == PixelClass::Gas) {
				world.replacePixel(wx, wy, element::Oil::create());
			}
		}
	}
	return true;
}

int OilTap::priority() const noexcept {
	return 5;
}

} // namespace wf::structure
