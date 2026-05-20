#include "wforge/level.h"
#include "wforge/save_io.h"
#include <algorithm>

namespace wf::item {
BrushSizeChangableItem::BrushSizeChangableItem(
	int max_brush_size, int initial_brush_size
) noexcept
	: _brush_size(std::clamp(initial_brush_size, 1, max_brush_size))
	, _max_brush_size(max_brush_size) {}

BrushSizeChangableItem::BrushSizeChangableItem(int max_brush_size) noexcept
	: _brush_size(max_brush_size), _max_brush_size(max_brush_size) {}

void BrushSizeChangableItem::changeBrushSize(int delta) noexcept {
	_brush_size = std::clamp(_brush_size + delta, 1, _max_brush_size);
}

int BrushSizeChangableItem::brushSize() const noexcept {
	return _brush_size;
}

std::array<int, 2> BrushSizeChangableItem::brushTopLeft(
	int x, int y
) const noexcept {
	const int half_brush = _brush_size / 2;
	return {x - half_brush, y - half_brush};
}

void BrushSizeChangableItem::render(
	sf::RenderTarget &target, int x, int y, int scale
) const {
	constexpr sf::Color outline_color = sf::Color::Red;

	const auto [top_left_x, top_left_y] = brushTopLeft(x, y);
	sf::RectangleShape rect;
	rect.setPosition(sf::Vector2f(top_left_x * scale, top_left_y * scale));
	rect.setSize(sf::Vector2f(_brush_size * scale, _brush_size * scale));
	rect.setFillColor(sf::Color::Transparent);
	rect.setOutlineColor(outline_color);

	rect.setOutlineThickness(
		SaveKV::instance().getBool("settings.strict_pixel_perfection")
			? scale
			: 1.f
	);
	target.draw(rect);
}

} // namespace wf::item
