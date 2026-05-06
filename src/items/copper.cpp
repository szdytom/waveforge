#include "wforge/elements.h"
#include "wforge/level.h"

namespace wf::item {

CopperBrush::CopperBrush() noexcept: BrushSizeChangableItem(2) {}

Item CopperBrush::create() noexcept {
	return pro::make_proxy<ItemFacade, CopperBrush>();
}

bool CopperBrush::use(Level &level, int x, int y) noexcept {
	auto &world = level.fallsand;
	auto [top_left_x, top_left_y] = brushTopLeft(x, y);
	int brush_size = brushSize();
	bool used = false;
	for (int dx = 0; dx < brush_size; ++dx) {
		int wx = top_left_x + dx;
		if (wx < 0 || wx >= world.width()) {
			continue;
		}

		for (int dy = 0; dy < brush_size; ++dy) {
			int wy = top_left_y + dy;
			if (wy < 0 || wy >= world.height()) {
				continue;
			}

			if (!world.isExternalEntityPresent(wx, wy)) {
				auto &tag = world.tagOf(wx, wy);
				int old_heat = tag.heat;
				world.replacePixel(wx, wy, element::Copper::create());
				tag.heat = old_heat;
				used = true;
			}
		}
	}
	return used;
}

std::string_view CopperBrush::name() const noexcept {
	return "Copper";
}

} // namespace wf::item
