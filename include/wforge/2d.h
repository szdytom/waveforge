#ifndef WFORGE_2D_H
#define WFORGE_2D_H

#include "wforge/generator.h"
#include <array>
#include <utility>

namespace wf {

enum class FacingDirection : std::uint8_t {
	North = 0,
	East = 1,
	South = 2,
	West = 3,
};

inline constexpr int xDeltaOf(FacingDirection dir) noexcept {
	switch (dir) {
	case FacingDirection::North:
	case FacingDirection::South:
		return 0;
	case FacingDirection::East:
		return 1;
	case FacingDirection::West:
		return -1;
	}
	return 0;
}

inline constexpr int yDeltaOf(FacingDirection dir) noexcept {
	switch (dir) {
	case FacingDirection::East:
	case FacingDirection::West:
		return 0;
	case FacingDirection::North:
		return -1;
	case FacingDirection::South:
		return 1;
	}
	return 0;
}

inline constexpr FacingDirection rotate90CW(FacingDirection dir) noexcept {
	return static_cast<FacingDirection>((std::to_underlying(dir) + 1) % 4);
}

inline constexpr FacingDirection rotate90CCW(FacingDirection dir) noexcept {
	return static_cast<FacingDirection>((std::to_underlying(dir) + 3) % 4);
}

// All tiles from start to end inclusively
Generator<std::array<int, 2>> tilesOnSegment(
	std::array<int, 2> start, std::array<int, 2> end
) noexcept;

// All 4-neighbors of center within size bounds
Generator<std::array<int, 2>> neighbors4(
	std::array<int, 2> center, std::array<int, 2> size
) noexcept;

// All 8-neighbors of center within size bounds
Generator<std::array<int, 2>> neighbors8(
	std::array<int, 2> center, std::array<int, 2> size
) noexcept;

} // namespace wf

#endif // WFORGE_2D_H
