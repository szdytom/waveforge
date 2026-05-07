#include "wforge/2d.h"
#include "wforge/fallsand.h"
#include "wforge/structures.h"

namespace wf {
namespace structure {

namespace {

PixelShape &heaterShape(FacingDirection dir) {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("heater/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

} // namespace

Heater::Heater(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, heaterShape(dir)) {
	if (poi.empty()) {
		throw std::runtime_error("Heater: missing POIs in heater shape\n");
	}
}

bool Heater::step(PixelWorld &world) noexcept {
	constexpr unsigned int heat_production = 15;

	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	if (isPowered()) {
		for (auto [bx, by] : poi) {
			auto &tag = world.tagOf(x + bx, y + by);
			tag.heat = std::min(tag.heat + heat_production, PixelTag::heat_max);
		}
	}

	return true;
}

int Heater::priority() const noexcept {
	return 10;
}

} // namespace structure
} // namespace wf
