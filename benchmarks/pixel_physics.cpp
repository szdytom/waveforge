#include "wforge/elements.h"
#include "wforge/fallsand.h"
#include "wforge/xoroshiro.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

class BenchmarkWorld : public wf::PixelWorld {
public:
	using PixelWorld::PixelWorld;

	void runFluidAnalysis() noexcept {
		fluidAnalysisStep();
	}

	void runThermalAnalysis() noexcept {
		thermalAnalysisStep();
	}

	std::array<double, 3> runTankTick() noexcept {
		auto measure = [](auto operation) {
			auto start = Clock::now();
			operation();
			return std::chrono::duration<double, std::milli>(
					   Clock::now() - start
			)
				.count();
		};

		double fluid_ms = measure([this] {
			fluidAnalysisStep();
		});
		double thermal_ms = measure([this] {
			thermalAnalysisStep();
		});
		double elements_ms = measure([this] {
			auto &rng = wf::Xoroshiro128PP::globalInstance();
			for (int y = height() - 1; y >= 0; --y) {
				bool reverse_x = (rng.next() % 2 == 0);
				for (int ix = 0; ix < width(); ++ix) {
					int x = reverse_x ? width() - 1 - ix : ix;
					while (!tagOf(x, y).dirty) {
						tagOf(x, y).dirty = true;
						elementOf(x, y)->step(*this, x, y);
					}
				}
			}
			resetDirtyFlags();
		});
		return {fluid_ms, thermal_ms, elements_ms};
	}
};

enum class Scenario {
	ColdAir,
	HotCopper,
	HalfWater,
	WarmWater,
};

BenchmarkWorld makeWorld(int width, int height, Scenario scenario) {
	BenchmarkWorld world(width, height);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (scenario == Scenario::HotCopper) {
				world.replacePixel(x, y, wf::element::Copper::create());
				world.tagOf(x, y).heat = ((x + y) % 2 == 0)
					? wf::PixelTag::heat_max
					: wf::PixelTag::heat_max / 4;
			} else if (
				(scenario == Scenario::HalfWater
			     || scenario == Scenario::WarmWater)
				&& y >= height / 2
			) {
				world.replacePixel(x, y, wf::element::Water::create());
				if (scenario == Scenario::WarmWater) {
					world.tagOf(x, y).heat = ((x + y) % 2 == 0) ? 29 : 5;
				}
			}
		}
	}
	return world;
}

BenchmarkWorld makeBurningTank(int width, int height) {
	BenchmarkWorld world(width, height);
	const int left = width / 16;
	const int right = width - left - 1;
	const int bottom = height - height / 16 - 1;
	const int water_top = height / 2;
	const int oil_top = height / 4;

	for (int y = oil_top; y <= bottom; ++y) {
		for (int x = left; x <= right; ++x) {
			if (x == left || x == right || y == bottom) {
				world.replacePixel(x, y, wf::element::Stone::create());
			} else if (y >= water_top) {
				world.replacePixel(x, y, wf::element::Water::create());
			} else {
				world.replacePixel(x, y, wf::element::Oil::create());
				auto &tag = world.tagOf(x, y);
				tag.heat = wf::PixelTag::heat_max;
				tag.ignited = true;
			}
		}
	}
	return world;
}

struct TankResult {
	std::array<double, 3> stage_ms{};
	std::uint64_t total_heat = 0;
	int oil_pixels = 0;
	int water_pixels = 0;
	int steam_pixels = 0;
	int smoke_pixels = 0;
};

TankResult runBurningTank(int width, int height, int ticks) {
	auto world = makeBurningTank(width, height);
	TankResult result;
	for (int tick = 0; tick < ticks; ++tick) {
		auto timings = world.runTankTick();
		for (int stage = 0; stage < timings.size(); ++stage) {
			result.stage_ms[stage] += timings[stage];
		}
	}
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			result.total_heat += world.tagOf(x, y).heat;
			result.oil_pixels += world.typeOfIs(x, y, wf::PixelType::Oil);
			result.water_pixels += world.typeOfIs(x, y, wf::PixelType::Water);
			result.steam_pixels += world.typeOfIs(x, y, wf::PixelType::Steam);
			result.smoke_pixels += world.typeOfIs(x, y, wf::PixelType::Smoke);
		}
	}
	return result;
}

template<typename Setup, typename Operation>
double measureMedian(int samples, Setup setup, Operation operation) {
	std::vector<double> timings;
	timings.reserve(samples);
	for (int i = 0; i < samples; ++i) {
		auto world = setup();
		auto start = Clock::now();
		operation(world);
		auto end = Clock::now();
		timings.push_back(
			std::chrono::duration<double, std::milli>(end - start).count()
		);
	}
	std::ranges::sort(timings);
	return timings[timings.size() / 2];
}

void printResult(
	std::string_view stage, std::string_view scenario, int width, int height,
	double milliseconds
) {
	const double megapixels = static_cast<double>(width) * height / 1'000'000.0;
	std::cout << std::left << std::setw(10) << stage << std::setw(12)
			  << scenario << std::right << std::setw(5) << width << "x"
			  << std::left << std::setw(6) << height << std::right << std::fixed
			  << std::setprecision(3) << std::setw(11) << milliseconds
			  << std::setw(13) << milliseconds / megapixels << '\n';
}

} // namespace

int main(int argc, char **argv) {
	int samples = 9;
	if (argc == 2) {
		samples = std::max(3, std::atoi(argv[1]));
	}

	constexpr std::pair<int, int> dimensions[] = {
		{128, 72},
		{256, 144},
		{512, 288},
		{1024, 576},
	};

	std::cout << "Median of " << samples << " fresh-world samples\n";
	std::cout << std::left << std::setw(10) << "stage" << std::setw(12)
			  << "scenario" << std::right << std::setw(12) << "dimensions"
			  << std::setw(11) << "ms/call" << std::setw(13) << "ms/MPix"
			  << '\n';

	for (auto [width, height] : dimensions) {
		auto run = [=](std::string_view stage, std::string_view scenario,
		               Scenario setup_scenario, auto operation) {
			double elapsed = measureMedian(samples, [=] {
				return makeWorld(width, height, setup_scenario);
			}, operation);
			printResult(stage, scenario, width, height, elapsed);
		};

		run("thermal", "cold-air", Scenario::ColdAir, [](auto &world) {
			world.runThermalAnalysis();
		});
		run("thermal", "hot-copper", Scenario::HotCopper, [](auto &world) {
			world.runThermalAnalysis();
		});
		run("fluid", "cold-air", Scenario::ColdAir, [](auto &world) {
			world.runFluidAnalysis();
		});
		run("fluid", "half-water", Scenario::HalfWater, [](auto &world) {
			world.runFluidAnalysis();
		});
		run("thermal", "warm-water", Scenario::WarmWater, [](auto &world) {
			world.runThermalAnalysis();
		});
		run("fluid", "warm-water", Scenario::WarmWater, [](auto &world) {
			world.runFluidAnalysis();
		});
		run("full-tick", "cold-air", Scenario::ColdAir, [](auto &world) {
			world.step();
		});
		run("full-tick", "half-water", Scenario::HalfWater, [](auto &world) {
			world.step();
		});
		run("full-tick", "warm-water", Scenario::WarmWater, [](auto &world) {
			world.step();
		});
	}

	constexpr int tank_ticks = 96;
	std::cout << "\nBurning oil/water tank, average over " << tank_ticks
			  << " evolving ticks\n";
	std::cout << std::left << std::setw(12) << "dimensions" << std::right
			  << std::setw(12) << "fluid ms" << std::setw(12) << "thermal ms"
			  << std::setw(13) << "elements ms" << std::setw(10) << "oil"
			  << std::setw(10) << "water" << std::setw(10) << "steam"
			  << std::setw(10) << "smoke" << std::setw(14) << "heat" << '\n';
	for (auto [width, height] : dimensions) {
		auto result = runBurningTank(width, height, tank_ticks);
		std::cout << std::left << std::setw(5) << width << "x" << std::setw(6)
				  << height << std::right << std::fixed << std::setprecision(3)
				  << std::setw(12) << result.stage_ms[0] / tank_ticks
				  << std::setw(12) << result.stage_ms[1] / tank_ticks
				  << std::setw(13) << result.stage_ms[2] / tank_ticks
				  << std::setw(10) << result.oil_pixels << std::setw(10)
				  << result.water_pixels << std::setw(10) << result.steam_pixels
				  << std::setw(10) << result.smoke_pixels << std::setw(14)
				  << result.total_heat << '\n';
	}
}
