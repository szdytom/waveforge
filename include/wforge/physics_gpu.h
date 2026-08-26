#ifndef WFORGE_PHYSICS_GPU_H
#define WFORGE_PHYSICS_GPU_H

#include "wforge/fallsand.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wf {

constexpr std::uint32_t PHYSICS_CHUNK_SIZE = 32;
constexpr std::uint32_t PHYSICS_READBACK_RING_SIZE = 3;
constexpr std::uint32_t PHYSICS_MAX_QUERY_CELLS = 640 * 1024;

enum class PhysicsInteractionFlag : std::uint32_t {
	None = 0,
	LaserActive = 1U << 0U,
	LaserStroke = 1U << 1U,
	ExternalEntity = 1U << 2U,
	Reflective = 1U << 3U,
};

constexpr PhysicsInteractionFlag operator|(
	PhysicsInteractionFlag lhs, PhysicsInteractionFlag rhs
) noexcept {
	return static_cast<PhysicsInteractionFlag>(
		std::to_underlying(lhs) | std::to_underlying(rhs)
	);
}

struct PackedCellState {
	std::uint32_t metadata = 0;
	std::uint32_t dynamics = 0;

	[[nodiscard]] static PackedCellState fromTags(
		PixelTag tag, StaticPixelTag static_tag = {}
	) noexcept;
	[[nodiscard]] PixelTag pixelTag() const noexcept;
	[[nodiscard]] StaticPixelTag staticPixelTag() const noexcept;

	[[nodiscard]] PixelType type() const noexcept;
	[[nodiscard]] PixelClass pixelClass() const noexcept;
	[[nodiscard]] std::uint8_t colorIndex() const noexcept;
	[[nodiscard]] std::uint8_t heat() const noexcept;
	[[nodiscard]] std::uint8_t burnLifetime() const noexcept;
	[[nodiscard]] std::int8_t velocityX() const noexcept;
	[[nodiscard]] std::int8_t velocityY() const noexcept;

	void setType(PixelType type) noexcept;
	void setPixelClass(PixelClass pixel_class) noexcept;
	void setColorIndex(std::uint8_t color_index) noexcept;
	void setHeat(std::uint8_t heat) noexcept;
	void setBurnLifetime(std::uint8_t lifetime) noexcept;
	void setVelocity(std::int8_t velocity_x, std::int8_t velocity_y) noexcept;
};

static_assert(sizeof(PackedCellState) == 8);

struct MaterialProperties {
	std::uint32_t classes = 0;
	std::uint32_t conductivity_density = 0;
	std::uint32_t phase_thresholds = 0;
	std::uint32_t colors = 0;
};

static_assert(sizeof(MaterialProperties) == 16);

enum class WorldEditKind : std::uint32_t {
	PaintMaterial,
	ClearRegion,
	AddHeat,
	ChargeRegion,
	SetStructurePixel,
	SetEntityMask,
	SetLaser,
};

struct WorldEditCommand {
	WorldEditKind kind = WorldEditKind::PaintMaterial;
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t value = 0;
	std::uint32_t secondary_value = 0;
	std::uint32_t flags = 0;
};

static_assert(sizeof(WorldEditCommand) == 32);

class WorldEditBatch {
public:
	void paintMaterial(
		int x, int y, int width, int height, PixelType type,
		std::uint8_t color_index = 255
	);
	void clearRegion(int x, int y, int width, int height);
	void addHeat(int x, int y, int width, int height, int amount);
	void chargeRegion(int x, int y, int width, int height, int power);
	void setStructurePixel(
		int x, int y, PixelType type, std::uint8_t color_index
	);
	void setEntityMask(int x, int y, int width, int height, bool present);
	void setLaser(int x, int y, bool active, bool stroke);

	[[nodiscard]] bool empty() const noexcept;
	[[nodiscard]] std::span<const WorldEditCommand> commands() const noexcept;
	[[nodiscard]] std::size_t byteSize() const noexcept;
	void clear() noexcept;

private:
	std::vector<WorldEditCommand> _commands;
};

enum class WorldQueryKind : std::uint32_t {
	DuckRegion,
	StructureSensors,
	ItemRegion,
	DebugRegion,
};

struct WorldQueryRequest {
	WorldQueryKind kind = WorldQueryKind::DuckRegion;
	std::uint32_t id = 0;
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

static_assert(sizeof(WorldQueryRequest) == 24);

struct WorldQueryResult {
	std::uint32_t id = 0;
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t cell_offset = 0;
	std::uint32_t cell_count = 0;
};

static_assert(sizeof(WorldQueryResult) == 28);

class WorldQuerySnapshot {
public:
	[[nodiscard]] std::uint64_t tick() const noexcept;
	[[nodiscard]] std::optional<PackedCellState> cellAt(
		std::uint32_t query_id, int x, int y
	) const noexcept;
	[[nodiscard]] std::span<const PackedCellState> cells(
		std::uint32_t query_id
	) const noexcept;
	[[nodiscard]] std::span<const WorldQueryResult> results() const noexcept;

private:
	friend class GpuPhysicsBackend;
	friend WorldQuerySnapshot compactWorldQueries(
		std::uint64_t, int, int, std::span<const PackedCellState>,
		std::span<const WorldQueryRequest>
	);

	std::uint64_t _tick = 0;
	std::vector<WorldQueryResult> _results;
	std::vector<PackedCellState> _cells;
};

struct PhysicsTimings {
	double apply_commands_ms = 0.0;
	double chunk_activity_ms = 0.0;
	double thermal_ms = 0.0;
	double transitions_ms = 0.0;
	double movement_ms = 0.0;
	double pressure_ms = 0.0;
	double electricity_ms = 0.0;
	double output_ms = 0.0;
	double total_ms = 0.0;
	std::uint32_t active_chunks = 0;
	std::uint32_t processed_cells = 0;
	std::uint32_t movement_conflicts = 0;
	std::uint32_t transitions = 0;
	std::uint32_t command_bytes = 0;
	std::uint32_t query_bytes = 0;
};

struct PhysicsAdapterDiagnostics {
	std::string name;
	std::string vendor;
	std::string architecture;
	std::string backend;
	std::string driver;
};

struct PhysicsFrame {
	std::uint64_t tick = 0;
	std::span<const std::uint8_t> rgba;
	std::span<const std::uint8_t> heat_rgba;
	const WorldQuerySnapshot
		*queries = nullptr; // not owned, managed by backend
	PhysicsTimings timings;
};

[[nodiscard]] std::uint32_t physicsRandomHash(
	std::uint64_t tick, std::uint32_t cell, std::uint32_t pass,
	std::uint32_t direction
) noexcept;

[[nodiscard]] WorldQueryRequest clipWorldQuery(
	WorldQueryRequest request, int world_width, int world_height
) noexcept;

[[nodiscard]] WorldQuerySnapshot compactWorldQueries(
	std::uint64_t tick, int world_width, int world_height,
	std::span<const PackedCellState> state,
	std::span<const WorldQueryRequest> requests
);

class GpuPhysicsBackend {
public:
	GpuPhysicsBackend(int width, int height);
	~GpuPhysicsBackend() noexcept;
	GpuPhysicsBackend(GpuPhysicsBackend &&) noexcept;
	GpuPhysicsBackend &operator=(GpuPhysicsBackend &&) noexcept;
	GpuPhysicsBackend(const GpuPhysicsBackend &) = delete;
	GpuPhysicsBackend &operator=(const GpuPhysicsBackend &) = delete;

	void uploadLevel(std::span<const PackedCellState> state);
	void submit(WorldEditBatch edits, std::vector<WorldQueryRequest> queries);
	void step();
	void poll() noexcept;

	[[nodiscard]] bool frameReady() const noexcept;
	[[nodiscard]] PhysicsFrame latestFrame() const noexcept;
	[[nodiscard]] PhysicsAdapterDiagnostics diagnostics() const;
	[[nodiscard]] bool deviceLost() const noexcept;
	[[nodiscard]] std::string_view errorMessage() const noexcept;
	[[nodiscard]] std::vector<PackedCellState> serialize();

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
};

} // namespace wf

#endif // WFORGE_PHYSICS_GPU_H
