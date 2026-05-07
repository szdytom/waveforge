#include "wforge/assets.h"
#include "wforge/elements.h"
#include "wforge/structures.h"

namespace wf::structure {

namespace {

PixelShape &transistorShape(FacingDirection dir) noexcept {
	static PixelShape *ptr = nullptr;
	if (ptr == nullptr) {
		ptr = AssetsManager::instance()
				  .getAsset<std::array<PixelShape, 4>>("transistor/shapes")
				  .data();
	}
	return ptr[std::to_underlying(dir)];
}

} // namespace

TransistorNPN::TransistorNPN(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, transistorShape(dir))
	, _dir(dir)
	, _conducting(false) {}

bool TransistorNPN::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	if (isPowered() ^ _conducting) {
		_conducting = isPowered();
		for (auto [bx, by] : poi) {
			int wx = x + bx;
			int wy = y + by;
			auto &tag = world.tagOf(wx, wy);
			int old_heat = tag.heat;
			if (_conducting) {
				world.replacePixel(wx, wy, element::Copper::create());
			} else {
				world.replacePixelWithAir(wx, wy);
			}
			world.tagOf(wx, wy).heat = old_heat;
		}
	}
	return true;
}

int TransistorNPN::priority() const noexcept {
	return 5; // must be earlier than lasers
}

TransistorPNP::TransistorPNP(int x, int y, FacingDirection dir)
	: InputElectricalStructure(x, y, transistorShape(dir))
	, _dir(dir)
	, _insulating(true) {}

bool TransistorPNP::step(PixelWorld &world) noexcept {
	if (!PixelShapedStructure::step(world)) {
		return false;
	}

	if (!InputElectricalStructure::step(world)) {
		return false;
	}

	// PNP: insulating when powered, conducting when not powered
	if (isPowered() ^ _insulating) {
		_insulating = isPowered();
		for (auto [bx, by] : poi) {
			int wx = x + bx;
			int wy = y + by;
			auto &tag = world.tagOf(wx, wy);
			int old_heat = tag.heat;
			if (_insulating) {
				world.replacePixelWithAir(wx, wy);
			} else {
				world.replacePixel(wx, wy, element::Copper::create());
			}
			world.tagOf(wx, wy).heat = old_heat;
		}
	}
	return true;
}

int TransistorPNP::priority() const noexcept {
	return 5; // must be earlier than lasers
}

} // namespace wf::structure
