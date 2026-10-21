#include "wforge/2d.h"
#include <cmath>

namespace wf {

Generator<std::array<int, 2>> tilesOnSegment(
	std::array<int, 2> start, std::array<int, 2> end
) noexcept {
	// Use Bresenham's line algorithm to generate integer tiles between
	// start and end inclusively. The previous implementation used a
	// rounding-based approach which can introduce directional bias due to
	// the order of increments; Bresenham is symmetric and avoids that.
	int x0 = start[0];
	int y0 = start[1];
	int x1 = end[0];
	int y1 = end[1];

	int dx = std::abs(x1 - x0);
	int sx = (x0 < x1) ? 1 : -1;
	int dy = -std::abs(y1 - y0);
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx + dy; // error value

	// yield the starting tile first (preserves previous behavior)
	co_yield {x0, y0};
	if (x0 == x1 && y0 == y1) {
		co_return;
	}

	while (x0 != x1 || y0 != y1) {
		int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
		co_yield {x0, y0};
	}
}

Generator<std::array<int, 2>> neighbors4(
	std::array<int, 2> center, std::array<int, 2> size
) noexcept {
	const int dx[] = {-1, 1, 0, 0};
	const int dy[] = {0, 0, -1, 1};
	for (int i = 0; i < 4; ++i) {
		int nx = center[0] + dx[i];
		int ny = center[1] + dy[i];
		if (nx >= 0 && nx < size[0] && ny >= 0 && ny < size[1]) {
			co_yield {nx, ny};
		}
	}
}

Generator<std::array<int, 2>> neighbors8(
	std::array<int, 2> center, std::array<int, 2> size
) noexcept {
	const int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
	const int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};
	for (int i = 0; i < 8; ++i) {
		int nx = center[0] + dx[i];
		int ny = center[1] + dy[i];
		if (nx >= 0 && nx < size[0] && ny >= 0 && ny < size[1]) {
			co_yield {nx, ny};
		}
	}
}

} // namespace wf
