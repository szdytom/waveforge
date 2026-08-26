#include "wforge/physics_gpu.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

void testPacking() {
	wf::PixelTag tag{
		.type = wf::PixelType::Oil,
		.pclass = wf::PixelClass::Fluid,
		.color_index = 17,
		.is_free_falling = true,
		.fluid_dir = -1,
		.heat = 91,
		.ignited = true,
		.electric_power = 13,
	};
	wf::StaticPixelTag static_tag{
		.laser_active = true,
		.external_entity_present = true,
		.is_reflective_surface = true,
	};
	auto packed = wf::PackedCellState::fromTags(tag, static_tag);
	assert(packed.burnLifetime() == 48);
	packed.setBurnLifetime(47);
	packed.setVelocity(-12, 23);
	const auto unpacked = packed.pixelTag();
	const auto unpacked_static = packed.staticPixelTag();
	assert(unpacked.type == tag.type);
	assert(unpacked.pclass == tag.pclass);
	assert(unpacked.color_index == tag.color_index);
	assert(unpacked.is_free_falling == tag.is_free_falling);
	assert(unpacked.fluid_dir == tag.fluid_dir);
	assert(unpacked.heat == tag.heat);
	assert(unpacked.ignited == tag.ignited);
	assert(unpacked.electric_power == tag.electric_power);
	assert(unpacked_static.laser_active);
	assert(unpacked_static.external_entity_present);
	assert(unpacked_static.is_reflective_surface);
	assert(packed.burnLifetime() == 47);
	assert(packed.velocityX() == -12);
	assert(packed.velocityY() == 23);
}

void testCommands() {
	wf::WorldEditBatch batch;
	batch.paintMaterial(1, 2, 3, 4, wf::PixelType::Water, 15);
	batch.clearRegion(5, 6, 2, 3);
	batch.addHeat(7, 8, 1, 1, -9);
	batch.chargeRegion(9, 10, 4, 5, 99);
	batch.setEntityMask(2, 3, 4, 5, true);
	assert(batch.commands().size() == 5);
	assert(batch.byteSize() == 5 * sizeof(wf::WorldEditCommand));
	assert(
		batch.commands()[0].value == std::to_underlying(wf::PixelType::Water)
	);
	assert(batch.commands()[2].value == static_cast<std::uint32_t>(-9));
	assert(batch.commands()[3].value == 15);
	batch.clear();
	assert(batch.empty());
}

void testClippingAndCompaction() {
	constexpr int WIDTH = 5;
	constexpr int HEIGHT = 4;
	std::vector<wf::PackedCellState> state(WIDTH * HEIGHT);
	for (int index = 0; index < WIDTH * HEIGHT; ++index) {
		state[index].setHeat(static_cast<std::uint8_t>(index));
	}
	std::vector<wf::WorldQueryRequest> requests{
		{.kind = wf::WorldQueryKind::DuckRegion,
	     .id = 7,
	     .x = -2,
	     .y = 1,
	     .width = 5,
	     .height = 4},
		{.kind = wf::WorldQueryKind::StructureSensors,
	     .id = 9,
	     .x = 4,
	     .y = 3,
	     .width = 3,
	     .height = 2},
	};
	auto snapshot = wf::compactWorldQueries(42, WIDTH, HEIGHT, state, requests);
	assert(snapshot.tick() == 42);
	assert(snapshot.results().size() == 2);
	assert(snapshot.cells(7).size() == 9);
	assert(snapshot.cells(9).size() == 1);
	assert(snapshot.cellAt(7, 0, 1)->heat() == 5);
	assert(snapshot.cellAt(7, 2, 3)->heat() == 17);
	assert(!snapshot.cellAt(7, 4, 1).has_value());
	assert(snapshot.cellAt(9, 4, 3)->heat() == 19);

	bool threw = false;
	try {
		[[maybe_unused]] const auto invalid = wf::compactWorldQueries(
			0, WIDTH, HEIGHT,
			std::span<const wf::PackedCellState>(state).first(3), requests
		);
	} catch (const std::invalid_argument &) {
		threw = true;
	}
	assert(threw);
}

void testRandomHash() {
	const auto first = wf::physicsRandomHash(123, 456, 7, 1);
	assert(first == wf::physicsRandomHash(123, 456, 7, 1));
	assert(first != wf::physicsRandomHash(124, 456, 7, 1));
	assert(first != wf::physicsRandomHash(123, 457, 7, 1));
	assert(first != wf::physicsRandomHash(123, 456, 8, 1));
	assert(first != wf::physicsRandomHash(123, 456, 7, 2));
}

} // namespace

int main() {
	testPacking();
	testCommands();
	testClippingAndCompaction();
	testRandomHash();
}
