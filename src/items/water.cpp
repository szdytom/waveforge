#include "wforge/elements.h"
#include "wforge/level.h"

namespace wf::item {

WaterBrush::WaterBrush(bool is_large_brush) noexcept
	: BrushSizeChangableItem(is_large_brush ? 24 : 12)
	, _is_large(is_large_brush) {}

Item WaterBrush::create() noexcept {
	return pro::make_proxy<ItemFacade, WaterBrush>(false);
}

Item WaterBrush::createLarge() noexcept {
	return pro::make_proxy<ItemFacade, WaterBrush>(true);
}

bool WaterBrush::use(Level &level, int x, int y) noexcept {
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

			if (world.classOfIs(wx, wy, PixelClass::Gas)) {
				world.replacePixel(wx, wy, element::Water::create());
				used = true;
			}
		}
	}
	return used;
}

std::string_view WaterBrush::name() const noexcept {
	if (_is_large) {
		return "Water[L]";
	} else {
		return "Water";
	}
}

} // namespace wf::item
