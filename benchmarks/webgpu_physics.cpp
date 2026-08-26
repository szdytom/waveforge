#include "wforge/colorpalette.h"
#include "wforge/physics_gpu.h"
#include <SFML/Window/Context.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr int WIDTH = 1024;
constexpr int HEIGHT = 576;
constexpr int WARMUP_TICKS = 16;
constexpr int BENCHMARK_TICKS = 96;
constexpr int CONSERVATION_WIDTH = 128;
constexpr int CONSERVATION_HEIGHT = 96;
constexpr int CONSERVATION_TICKS = 48;

wf::PackedCellState makeCell(
	wf::PixelType type, wf::PixelClass pixel_class, std::uint8_t color,
	std::uint8_t heat = 0, bool ignited = false
) {
	auto state = wf::PackedCellState::fromTags(
		wf::PixelTag{
			.type = type,
			.pclass = pixel_class,
			.color_index = color,
			.heat = heat,
			.ignited = ignited,
		}
	);
	if (type == wf::PixelType::Oil) {
		state.setBurnLifetime(48);
	} else if (type == wf::PixelType::Wood) {
		state.setBurnLifetime(96);
	}
	return state;
}

std::vector<wf::PackedCellState> makeBurningTank() {
	const auto air = makeCell(
		wf::PixelType::Air, wf::PixelClass::Gas, wf::colorIndexOf("Air")
	);
	const auto stone = makeCell(
		wf::PixelType::Stone, wf::PixelClass::Solid, wf::colorIndexOf("Stone1")
	);
	const auto water = makeCell(
		wf::PixelType::Water, wf::PixelClass::Fluid, wf::colorIndexOf("Water")
	);
	const auto oil = makeCell(
		wf::PixelType::Oil, wf::PixelClass::Fluid, wf::colorIndexOf("Oil"),
		wf::PixelTag::heat_max, true
	);
	std::vector state(static_cast<std::size_t>(WIDTH) * HEIGHT, air);
	const int left = WIDTH / 16;
	const int right = WIDTH - left - 1;
	const int bottom = HEIGHT - HEIGHT / 16 - 1;
	for (int y = HEIGHT / 4; y <= bottom; ++y) {
		for (int x = left; x <= right; ++x) {
			state[y * WIDTH + x] = x == left || x == right || y == bottom
				? stone
				: (y >= HEIGHT / 2 ? water : oil);
		}
	}
	return state;
}

void waitForTick(wf::GpuPhysicsBackend &backend, std::uint64_t tick) {
	while (!backend.frameReady() || backend.latestFrame().tick < tick) {
		backend.poll();
		std::this_thread::yield();
	}
}

using MaterialCounts = std::array<
	std::size_t, std::to_underlying(wf::PixelType::_count)>;

MaterialCounts materialCounts(std::span<const wf::PackedCellState> state) {
	MaterialCounts counts{};
	for (const auto cell : state) {
		++counts[std::to_underlying(cell.type())];
	}
	return counts;
}

void validateMaterialConservation() {
	const auto air = makeCell(
		wf::PixelType::Air, wf::PixelClass::Gas, wf::colorIndexOf("Air")
	);
	const auto stone = makeCell(
		wf::PixelType::Stone, wf::PixelClass::Solid, wf::colorIndexOf("Stone1")
	);
	const auto sand = makeCell(
		wf::PixelType::Sand, wf::PixelClass::Solid, wf::colorIndexOf("Sand1")
	);
	const auto water = makeCell(
		wf::PixelType::Water, wf::PixelClass::Fluid, wf::colorIndexOf("Water")
	);
	const auto oil = makeCell(
		wf::PixelType::Oil, wf::PixelClass::Fluid, wf::colorIndexOf("Oil")
	);
	std::vector state(
		static_cast<std::size_t>(CONSERVATION_WIDTH) * CONSERVATION_HEIGHT, air
	);
	for (int y = 0; y < CONSERVATION_HEIGHT; ++y) {
		for (int x = 0; x < CONSERVATION_WIDTH; ++x) {
			const int index = y * CONSERVATION_WIDTH + x;
			if (x == 0 || x == CONSERVATION_WIDTH - 1 || y == 0
			    || y == CONSERVATION_HEIGHT - 1) {
				state[index] = stone;
			} else if (y == CONSERVATION_HEIGHT / 4) {
				state[index] = sand;
			} else if (
				y > CONSERVATION_HEIGHT / 4 && y < CONSERVATION_HEIGHT / 2
			) {
				state[index] = water;
			} else if (y >= CONSERVATION_HEIGHT / 2) {
				state[index] = oil;
			}
		}
	}
	const auto expected = materialCounts(state);
	wf::GpuPhysicsBackend backend(CONSERVATION_WIDTH, CONSERVATION_HEIGHT);
	backend.uploadLevel(state);
	for (int tick = 0; tick < CONSERVATION_TICKS; ++tick) {
		backend.submit({}, {});
		backend.step();
		waitForTick(backend, tick);
	}
	const auto actual = materialCounts(backend.serialize());
	if (actual != expected) {
		throw std::runtime_error("GPU movement does not conserve materials");
	}

	std::vector circuit_state(
		static_cast<std::size_t>(CONSERVATION_WIDTH) * CONSERVATION_HEIGHT, air
	);
	const auto copper = makeCell(
		wf::PixelType::Copper, wf::PixelClass::Solid,
		wf::colorIndexOf("Copper1")
	);
	const int center_x = CONSERVATION_WIDTH / 2;
	const int center_y = CONSERVATION_HEIGHT / 2;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			circuit_state[(center_y + dy) * CONSERVATION_WIDTH + center_x + dx]
				= copper;
		}
	}
	backend.uploadLevel(circuit_state);
	wf::WorldEditBatch charge;
	charge.chargeRegion(center_x, center_y, 1, 1, 15);
	backend.submit(std::move(charge), {});
	backend.step();
	waitForTick(backend, CONSERVATION_TICKS);
	auto circuit = backend.serialize();
	if (circuit[center_y * CONSERVATION_WIDTH + center_x]
	        .pixelTag()
	        .electric_power
	    != 14) {
		throw std::runtime_error(
			"GPU charge did not decay to propagation state"
		);
	}
	backend.submit({}, {});
	backend.step();
	waitForTick(backend, CONSERVATION_TICKS + 1);
	circuit = backend.serialize();
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const auto power
				= circuit[(center_y + dy) * CONSERVATION_WIDTH + center_x + dx]
					  .pixelTag()
					  .electric_power;
			const auto expected_power = dx == 0 && dy == 0 ? 13 : 15;
			if (power != expected_power) {
				throw std::runtime_error(
					"GPU electricity propagation differs from CPU rules"
				);
			}
		}
	}
}

} // namespace

int main() {
	try {
		sf::Context sfml_context;
		validateMaterialConservation();
		wf::GpuPhysicsBackend backend(WIDTH, HEIGHT);
		const auto diagnostics = backend.diagnostics();
		std::cout << std::format(
			"WebGPU adapter: {} ({}, {})\n", diagnostics.name,
			diagnostics.vendor, diagnostics.backend
		);
		backend.uploadLevel(makeBurningTank());
		for (int tick = 0; tick < WARMUP_TICKS; ++tick) {
			backend.submit({}, {});
			backend.step();
			waitForTick(backend, tick);
		}

		const auto start = std::chrono::steady_clock::now();
		double gpu_elapsed_ms = 0.0;
		for (int tick = 0; tick < BENCHMARK_TICKS; ++tick) {
			backend.submit(
				{},
				{{
					.kind = wf::WorldQueryKind::DuckRegion,
					.id = 1,
					.x = WIDTH / 2 - 8,
					.y = HEIGHT / 2 - 8,
					.width = 16,
					.height = 16,
				}}
			);
			backend.step();
			waitForTick(backend, WARMUP_TICKS + tick);
			gpu_elapsed_ms += backend.latestFrame().timings.total_ms;
		}
		const double elapsed_ms = std::chrono::duration<double, std::milli>(
									  std::chrono::steady_clock::now() - start
		)
									  .count();
		const auto frame = backend.latestFrame();
		if (frame.rgba.size() != static_cast<std::size_t>(WIDTH) * HEIGHT * 4
		    || frame.queries == nullptr
		    || frame.queries->cells(1).size() != 256) {
			throw std::runtime_error("GPU physics output validation failed");
		}
		const auto serialized = backend.serialize();
		const auto query_cell = frame.queries->cellAt(
			1, WIDTH / 2 - 8, HEIGHT / 2 - 8
		);
		const auto
			world_cell = serialized[(HEIGHT / 2 - 8) * WIDTH + WIDTH / 2 - 8];
		if (!query_cell.has_value()
		    || query_cell->metadata != world_cell.metadata
		    || query_cell->dynamics != world_cell.dynamics) {
			throw std::runtime_error("Compact query does not match GPU state");
		}
		const auto smoke_count = std::ranges::count_if(
			serialized, [](wf::PackedCellState cell) {
			return cell.type() == wf::PixelType::Smoke;
		}
		);
		const auto steam_count = std::ranges::count_if(
			serialized, [](wf::PackedCellState cell) {
			return cell.type() == wf::PixelType::Steam;
		}
		);
		const auto water_count = std::ranges::count_if(
			serialized, [](wf::PackedCellState cell) {
			return cell.type() == wf::PixelType::Water;
		}
		);
		const auto oil_count = std::ranges::count_if(
			serialized, [](wf::PackedCellState cell) {
			return cell.type() == wf::PixelType::Oil;
		}
		);
		const auto total_heat = std::ranges::fold_left(
			serialized, std::uint64_t{0}, [](std::uint64_t total, auto cell) {
			return total + cell.heat();
		}
		);
		if (smoke_count == 0 || steam_count == 0) {
			throw std::runtime_error(
				"Burning tank did not produce smoke and steam"
			);
		}
		if (steam_count < 20'000 || steam_count > 31'000 || smoke_count < 24'000
		    || smoke_count > 38'000 || water_count < 180'000
		    || water_count > 220'000 || oil_count > 5'000
		    || total_heat < 14'300'000 || total_heat > 17'600'000) {
			throw std::runtime_error(
				"Burning-tank behavior exceeds CPU parity tolerances"
			);
		}
		std::cout << std::format(
			"1024x576 burning-tank submission and readback: {:.3f} ms/tick\n",
			elapsed_ms / BENCHMARK_TICKS
		);
		std::cout << std::format(
			"1024x576 burning-tank GPU physics: {:.3f} ms/tick\n",
			gpu_elapsed_ms / BENCHMARK_TICKS
		);
		std::cout << std::format(
			"active chunks: {}, processed cells: {}, conflicts: {}, "
			"transitions: {}, oil: {}, water: {}, smoke: {}, steam: {}, "
			"heat: {}\n",
			frame.timings.active_chunks, frame.timings.processed_cells,
			frame.timings.movement_conflicts, frame.timings.transitions,
			oil_count, water_count, smoke_count, steam_count, total_heat
		);
		std::cout << std::format(
			"GPU passes: apply {:.3f}, chunks {:.3f}, thermal {:.3f}, "
			"transitions {:.3f}, movement {:.3f}, pressure {:.3f}, "
			"electricity {:.3f}, output {:.3f} ms\n",
			frame.timings.apply_commands_ms, frame.timings.chunk_activity_ms,
			frame.timings.thermal_ms, frame.timings.transitions_ms,
			frame.timings.movement_ms, frame.timings.pressure_ms,
			frame.timings.electricity_ms, frame.timings.output_ms
		);
		return gpu_elapsed_ms / BENCHMARK_TICKS < 8.0 ? 0 : 2;
	} catch (const std::exception &exception) {
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
