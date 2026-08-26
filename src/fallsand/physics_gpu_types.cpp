#include "wforge/physics_gpu.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace wf {

namespace {

constexpr std::uint32_t TYPE_MASK = 0x3fU;
constexpr std::uint32_t CLASS_MASK = 0x3U << 6U;
constexpr std::uint32_t COLOR_MASK = 0xffU << 8U;
constexpr std::uint32_t IGNITED_MASK = 1U << 16U;
constexpr std::uint32_t FALLING_MASK = 1U << 17U;
constexpr std::uint32_t FLUID_DIRECTION_MASK = 0x3U << 18U;
constexpr std::uint32_t ELECTRICITY_MASK = 0xfU << 20U;
constexpr std::uint32_t LASER_ACTIVE_MASK = 1U << 24U;
constexpr std::uint32_t LASER_STROKE_MASK = 1U << 25U;
constexpr std::uint32_t ENTITY_MASK = 1U << 26U;
constexpr std::uint32_t REFLECTIVE_MASK = 1U << 27U;
constexpr std::uint32_t CONDUCTIVITY_MASK = 0x3fU << 26U;

constexpr std::uint32_t HEAT_MASK = 0x7fU;
constexpr std::uint32_t BURN_LIFETIME_MASK = 0xffU << 7U;
constexpr std::uint32_t VELOCITY_X_MASK = 0xffU << 15U;
constexpr std::uint32_t VELOCITY_Y_MASK = 0xffU << 23U;

std::uint32_t clippedExtent(int origin, int extent, int limit) noexcept {
	if (extent <= 0 || origin >= limit || origin + extent <= 0) {
		return 0;
	}
	const int start = std::max(origin, 0);
	const int end = std::min(origin + extent, limit);
	return static_cast<std::uint32_t>(std::max(0, end - start));
}

void addRectCommand(
	std::vector<WorldEditCommand> &commands, WorldEditKind kind, int x, int y,
	int width, int height, std::uint32_t value = 0,
	std::uint32_t secondary_value = 0, std::uint32_t flags = 0
) {
	if (width <= 0 || height <= 0) {
		return;
	}
	commands.push_back({
		.kind = kind,
		.x = x,
		.y = y,
		.width = static_cast<std::uint32_t>(width),
		.height = static_cast<std::uint32_t>(height),
		.value = value,
		.secondary_value = secondary_value,
		.flags = flags,
	});
}

} // namespace

PackedCellState PackedCellState::fromTags(
	PixelTag tag, StaticPixelTag static_tag
) noexcept {
	PackedCellState state;
	state.metadata = std::to_underlying(tag.type)
		| (std::to_underlying(tag.pclass) << 6U) | (tag.color_index << 8U)
		| (static_cast<std::uint32_t>(tag.ignited) << 16U)
		| (static_cast<std::uint32_t>(tag.is_free_falling) << 17U)
		| (static_cast<std::uint32_t>(tag.fluid_dir + 1) << 18U)
		| (tag.electric_power << 20U)
		| (static_cast<std::uint32_t>(static_tag.laser_active) << 24U)
		| (static_cast<std::uint32_t>(static_tag.laser_stroke) << 25U)
		| (static_cast<std::uint32_t>(static_tag.external_entity_present)
	       << 26U)
		| (static_cast<std::uint32_t>(static_tag.is_reflective_surface) << 27U);
	state.dynamics = tag.heat;
	return state;
}

PixelTag PackedCellState::pixelTag() const noexcept {
	return PixelTag{
		.type = type(),
		.pclass = pixelClass(),
		.color_index = colorIndex(),
		.is_free_falling = (metadata & FALLING_MASK) != 0,
		.fluid_dir = static_cast<int>((metadata & FLUID_DIRECTION_MASK) >> 18U)
			- 1,
		.heat = heat(),
		.ignited = (metadata & IGNITED_MASK) != 0,
		.electric_power = (metadata & ELECTRICITY_MASK) >> 20U,
	};
}

StaticPixelTag PackedCellState::staticPixelTag() const noexcept {
	return StaticPixelTag{
		.laser_active = (metadata & LASER_ACTIVE_MASK) != 0,
		.laser_stroke = (metadata & LASER_STROKE_MASK) != 0,
		.external_entity_present = (metadata & ENTITY_MASK) != 0,
		.is_reflective_surface = (metadata & REFLECTIVE_MASK) != 0,
	};
}

PixelType PackedCellState::type() const noexcept {
	return static_cast<PixelType>(metadata & TYPE_MASK);
}

PixelClass PackedCellState::pixelClass() const noexcept {
	return static_cast<PixelClass>((metadata & CLASS_MASK) >> 6U);
}

std::uint8_t PackedCellState::colorIndex() const noexcept {
	return static_cast<std::uint8_t>((metadata & COLOR_MASK) >> 8U);
}

std::uint8_t PackedCellState::heat() const noexcept {
	return static_cast<std::uint8_t>(dynamics & HEAT_MASK);
}

std::uint8_t PackedCellState::burnLifetime() const noexcept {
	return static_cast<std::uint8_t>((dynamics & BURN_LIFETIME_MASK) >> 7U);
}

std::int8_t PackedCellState::velocityX() const noexcept {
	return std::bit_cast<std::int8_t>(
		static_cast<std::uint8_t>((dynamics & VELOCITY_X_MASK) >> 15U)
	);
}

std::int8_t PackedCellState::velocityY() const noexcept {
	return std::bit_cast<std::int8_t>(
		static_cast<std::uint8_t>((dynamics & VELOCITY_Y_MASK) >> 23U)
	);
}

void PackedCellState::setType(PixelType type) noexcept {
	metadata = (metadata & ~TYPE_MASK) | std::to_underlying(type);
}

void PackedCellState::setPixelClass(PixelClass pixel_class) noexcept {
	metadata = (metadata & ~CLASS_MASK)
		| (std::to_underlying(pixel_class) << 6U);
}

void PackedCellState::setColorIndex(std::uint8_t color_index) noexcept {
	metadata = (metadata & ~COLOR_MASK)
		| (static_cast<std::uint32_t>(color_index) << 8U);
}

void PackedCellState::setHeat(std::uint8_t heat) noexcept {
	dynamics = (dynamics & ~HEAT_MASK) | std::min<std::uint8_t>(heat, 127);
}

void PackedCellState::setBurnLifetime(std::uint8_t lifetime) noexcept {
	dynamics = (dynamics & ~BURN_LIFETIME_MASK)
		| (static_cast<std::uint32_t>(lifetime) << 7U);
}

void PackedCellState::setVelocity(
	std::int8_t velocity_x, std::int8_t velocity_y
) noexcept {
	dynamics = (dynamics & ~(VELOCITY_X_MASK | VELOCITY_Y_MASK))
		| (static_cast<std::uint32_t>(std::bit_cast<std::uint8_t>(velocity_x))
	       << 15U)
		| (static_cast<std::uint32_t>(std::bit_cast<std::uint8_t>(velocity_y))
	       << 23U);
}

void WorldEditBatch::paintMaterial(
	int x, int y, int width, int height, PixelType type,
	std::uint8_t color_index
) {
	addRectCommand(
		_commands, WorldEditKind::PaintMaterial, x, y, width, height,
		std::to_underlying(type), color_index
	);
}

void WorldEditBatch::clearRegion(int x, int y, int width, int height) {
	addRectCommand(_commands, WorldEditKind::ClearRegion, x, y, width, height);
}

void WorldEditBatch::addHeat(int x, int y, int width, int height, int amount) {
	addRectCommand(
		_commands, WorldEditKind::AddHeat, x, y, width, height,
		static_cast<std::uint32_t>(amount)
	);
}

void WorldEditBatch::chargeRegion(
	int x, int y, int width, int height, int power
) {
	addRectCommand(
		_commands, WorldEditKind::ChargeRegion, x, y, width, height,
		static_cast<std::uint32_t>(std::clamp(power, 0, 15))
	);
}

void WorldEditBatch::setStructurePixel(
	int x, int y, PixelType type, std::uint8_t color_index
) {
	addRectCommand(
		_commands, WorldEditKind::SetStructurePixel, x, y, 1, 1,
		std::to_underlying(type), color_index
	);
}

void WorldEditBatch::setEntityMask(
	int x, int y, int width, int height, bool present
) {
	addRectCommand(
		_commands, WorldEditKind::SetEntityMask, x, y, width, height, present
	);
}

void WorldEditBatch::setLaser(int x, int y, bool active, bool stroke) {
	addRectCommand(
		_commands, WorldEditKind::SetLaser, x, y, 1, 1, active, stroke
	);
}

bool WorldEditBatch::empty() const noexcept {
	return _commands.empty();
}

std::span<const WorldEditCommand> WorldEditBatch::commands() const noexcept {
	return _commands;
}

std::size_t WorldEditBatch::byteSize() const noexcept {
	return _commands.size() * sizeof(WorldEditCommand);
}

void WorldEditBatch::clear() noexcept {
	_commands.clear();
}

std::uint64_t WorldQuerySnapshot::tick() const noexcept {
	return _tick;
}

std::optional<PackedCellState> WorldQuerySnapshot::cellAt(
	std::uint32_t query_id, int x, int y
) const noexcept {
	for (const auto &result : _results) {
		if (result.id != query_id || x < result.x || y < result.y
		    || x >= result.x + static_cast<int>(result.width)
		    || y >= result.y + static_cast<int>(result.height)) {
			continue;
		}
		const std::size_t offset = result.cell_offset
			+ static_cast<std::size_t>(y - result.y) * result.width + x
			- result.x;
		return _cells[offset];
	}
	return std::nullopt;
}

std::span<const PackedCellState> WorldQuerySnapshot::cells(
	std::uint32_t query_id
) const noexcept {
	for (const auto &result : _results) {
		if (result.id == query_id) {
			return std::span(_cells).subspan(
				result.cell_offset, result.cell_count
			);
		}
	}
	return {};
}

std::span<const WorldQueryResult> WorldQuerySnapshot::results() const noexcept {
	return _results;
}

std::uint32_t physicsRandomHash(
	std::uint64_t tick, std::uint32_t cell, std::uint32_t pass,
	std::uint32_t direction
) noexcept {
	std::uint32_t value = static_cast<std::uint32_t>(tick)
		^ std::rotl(static_cast<std::uint32_t>(tick >> 32U), 13)
		^ (cell * 0x9e3779b9U) ^ (pass * 0x85ebca6bU)
		^ (direction * 0xc2b2ae35U);
	value ^= value >> 16U;
	value *= 0x7feb352dU;
	value ^= value >> 15U;
	value *= 0x846ca68bU;
	return value ^ (value >> 16U);
}

WorldQueryRequest clipWorldQuery(
	WorldQueryRequest request, int world_width, int world_height
) noexcept {
	const int original_x = request.x;
	const int original_y = request.y;
	request.width = clippedExtent(
		request.x, static_cast<int>(request.width), world_width
	);
	request.height = clippedExtent(
		request.y, static_cast<int>(request.height), world_height
	);
	request.x = std::clamp(request.x, 0, world_width);
	request.y = std::clamp(request.y, 0, world_height);
	if (original_x < 0 && request.width > 0) {
		request.x = 0;
	}
	if (original_y < 0 && request.height > 0) {
		request.y = 0;
	}
	return request;
}

WorldQuerySnapshot compactWorldQueries(
	std::uint64_t tick, int world_width, int world_height,
	std::span<const PackedCellState> state,
	std::span<const WorldQueryRequest> requests
) {
	if (state.size() != static_cast<std::size_t>(world_width) * world_height) {
		throw std::invalid_argument("World state dimensions do not match");
	}
	WorldQuerySnapshot snapshot;
	snapshot._tick = tick;
	for (auto request : requests) {
		request = clipWorldQuery(request, world_width, world_height);
		const std::uint64_t cell_count = static_cast<std::uint64_t>(
											 request.width
										 )
			* request.height;
		if (snapshot._cells.size() + cell_count > PHYSICS_MAX_QUERY_CELLS) {
			throw std::length_error("World query snapshot exceeds cell limit");
		}
		WorldQueryResult result{
			.id = request.id,
			.x = request.x,
			.y = request.y,
			.width = request.width,
			.height = request.height,
			.cell_offset = static_cast<std::uint32_t>(snapshot._cells.size()),
			.cell_count = static_cast<std::uint32_t>(cell_count),
		};
		for (std::uint32_t y = 0; y < request.height; ++y) {
			const std::size_t offset = static_cast<std::size_t>(request.y + y)
					* world_width
				+ request.x;
			snapshot._cells.insert(
				snapshot._cells.end(), state.begin() + offset,
				state.begin() + offset + request.width
			);
		}
		snapshot._results.push_back(result);
	}
	return snapshot;
}

} // namespace wf
