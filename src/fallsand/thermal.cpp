#include "wforge/fallsand.h"
#include "wforge/xoroshiro.h"
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace wf {

namespace {

constexpr int HEAT_TRANSFER_SCALE = 100;
constexpr int HEAT_TRANSFER_FACTOR = 15;
constexpr int HEAT_DECAY_DENOMINATOR = 200;
constexpr int NUM_THERMAL_ANALYSIS_WORKERS = 4;

enum class WorkPhase {
	Idle,
	ProposeTransfers,
	GatherAndDecay,
};

struct ThermalBuffers {
	std::vector<std::array<std::uint8_t, 4>> transfers;
	std::vector<std::uint8_t> next_heat;

	void resize(std::size_t size) {
		transfers.resize(size);
		next_heat.resize(size);
	}
};

class ThermalWorker {
public:
	explicit ThermalWorker(int worker_id)
		: _worker_id(worker_id), _rng(Seed::device_random()) {
		_thread = std::jthread([this](std::stop_token stop_token) {
			workerLoop(stop_token);
		});
	}

	ThermalWorker(const ThermalWorker &) = delete;
	ThermalWorker &operator=(const ThermalWorker &) = delete;

	~ThermalWorker() noexcept {
		_thread.request_stop();
		_cv.notify_one();
		_thread.join();
	}

	void startWork(
		WorkPhase phase, const PixelWorld *world, int y_start, int y_end,
		ThermalBuffers *buffers
	) noexcept {
		{
			std::lock_guard lock(_work_mutex);
			_phase = phase;
			_world = world;
			_y_start = y_start;
			_y_end = y_end;
			_buffers = buffers;
			_work_ready = true;
		}
		_cv.notify_one();
	}

	void waitForCompletion() noexcept {
		std::unique_lock lock(_work_mutex);
		_cv_done.wait(lock, [this] {
			return !_work_ready;
		});
	}

private:
	void workerLoop(std::stop_token stop_token) noexcept {
		while (!stop_token.stop_requested()) {
			try {
				std::unique_lock lock(_work_mutex);
				_cv.wait(lock, [this, &stop_token] {
					return _work_ready || stop_token.stop_requested();
				});
				if (stop_token.stop_requested()) {
					break;
				}

				const WorkPhase phase = _phase;
				lock.unlock();
				if (phase == WorkPhase::ProposeTransfers) {
					proposeTransfers();
				} else if (phase == WorkPhase::GatherAndDecay) {
					gatherAndDecay();
				}
				lock.lock();
				_work_ready = false;
				lock.unlock();
				_cv_done.notify_one();
			} catch (const std::exception &exception) {
				std::cerr << "Fatal error in thermal worker " << _worker_id
						  << ": " << exception.what() << '\n';
				std::abort();
			} catch (...) {
				std::cerr << "Fatal unknown error in thermal worker "
						  << _worker_id << '\n';
				std::abort();
			}
		}
	}

	void proposeTransfers() noexcept {
		constexpr int DX[] = {-1, 1, 0, 0};
		constexpr int DY[] = {0, 0, -1, 1};
		const int width = _world->width();
		const int height = _world->height();

		for (int y = _y_start; y < _y_end; ++y) {
			for (int x = 0; x < width; ++x) {
				const int index = y * width + x;
				auto &transfers = _buffers->transfers[index];
				transfers.fill(0);
				const auto tag = _world->tagOf(x, y);
				if (tag.heat == 0 || tag.thermal_conductivity == 0) {
					continue;
				}

				std::array<int, 4> weights{};
				int total_weight = tag.heat
					* (PixelTag::thermal_conductivity_max
				       - tag.thermal_conductivity)
					* HEAT_TRANSFER_SCALE / HEAT_TRANSFER_FACTOR;
				for (int direction = 0; direction < 4; ++direction) {
					const int nx = x + DX[direction];
					const int ny = y + DY[direction];
					if (!_world->inBounds(nx, ny)) {
						continue;
					}
					const auto neighbor = _world->tagOf(nx, ny);
					weights[direction] = std::max<int>(
											 0, tag.heat - neighbor.heat
										 )
						* std::min(tag.thermal_conductivity,
					               neighbor.thermal_conductivity);
					total_weight += weights[direction];
				}

				if (total_weight == 0) {
					continue;
				}
				int available_heat = tag.heat;
				for (int direction = 0; direction < 4; ++direction) {
					const int numerator = tag.heat * weights[direction];
					int transfer = numerator / total_weight;
					const int remainder = numerator % total_weight;
					if (remainder > 0
					    && _rng.next() % (2U * total_weight)
					        < static_cast<std::uint64_t>(remainder)) {
						++transfer;
					}
					transfer = std::min(transfer, available_heat);
					transfers[direction] = transfer;
					available_heat -= transfer;
				}
			}
		}
	}

	void gatherAndDecay() noexcept {
		constexpr int DX[] = {-1, 1, 0, 0};
		constexpr int DY[] = {0, 0, -1, 1};
		constexpr int OPPOSITE[] = {1, 0, 3, 2};
		const int width = _world->width();

		for (int y = _y_start; y < _y_end; ++y) {
			for (int x = 0; x < width; ++x) {
				const int index = y * width + x;
				const auto &outgoing = _buffers->transfers[index];
				int next_heat = _world->tagOf(x, y).heat;
				for (std::uint8_t transfer : outgoing) {
					next_heat -= transfer;
				}
				for (int direction = 0; direction < 4; ++direction) {
					const int nx = x + DX[direction];
					const int ny = y + DY[direction];
					if (_world->inBounds(nx, ny)) {
						next_heat += _buffers->transfers[ny * width + nx]
														[OPPOSITE[direction]];
					}
				}

				const int remainder = next_heat % HEAT_DECAY_DENOMINATOR;
				const int decay = next_heat / HEAT_DECAY_DENOMINATOR;
				next_heat -= decay;
				if (next_heat > 0
				    && _rng.next() % HEAT_DECAY_DENOMINATOR
				        < static_cast<std::uint64_t>(remainder)) {
					--next_heat;
				}
				_buffers->next_heat[index] = std::clamp<int>(
					next_heat, 0, PixelTag::heat_max
				);
			}
		}
	}

	int _worker_id;
	Xoroshiro128PP _rng;
	std::jthread _thread;
	std::mutex _work_mutex;
	std::condition_variable _cv;
	std::condition_variable _cv_done;
	bool _work_ready = false;
	WorkPhase _phase = WorkPhase::Idle;
	const PixelWorld *_world = nullptr; // not owned, managed by the caller
	int _y_start = 0;
	int _y_end = 0;
	ThermalBuffers *_buffers = nullptr; // not owned, managed by the pool
};

class ThermalWorkerPool {
public:
	ThermalWorkerPool() {
		for (int i = 0; i < NUM_THERMAL_ANALYSIS_WORKERS; ++i) {
			_workers.emplace_back(std::make_unique<ThermalWorker>(i));
		}
	}

	const std::vector<std::uint8_t> &execute(const PixelWorld *world) noexcept {
		const std::size_t size = static_cast<std::size_t>(world->width())
			* world->height();
		_buffers.resize(size);
		executePhase(WorkPhase::ProposeTransfers, world);
		executePhase(WorkPhase::GatherAndDecay, world);
		return _buffers.next_heat;
	}

private:
	void executePhase(WorkPhase phase, const PixelWorld *world) noexcept {
		const int rows_per_worker = (world->height()
		                             + NUM_THERMAL_ANALYSIS_WORKERS - 1)
			/ NUM_THERMAL_ANALYSIS_WORKERS;
		for (int i = 0; i < NUM_THERMAL_ANALYSIS_WORKERS; ++i) {
			const int y_start = i * rows_per_worker;
			const int y_end = std::min(
				y_start + rows_per_worker, world->height()
			);
			_workers[i]->startWork(phase, world, y_start, y_end, &_buffers);
		}
		for (auto &worker : _workers) {
			worker->waitForCompletion();
		}
	}

	std::vector<std::unique_ptr<ThermalWorker>> _workers;
	ThermalBuffers _buffers;
};

ThermalWorkerPool &getThermalWorkerPool() noexcept {
	static ThermalWorkerPool pool;
	return pool;
}

} // namespace

void PixelWorld::thermalAnalysisStep() noexcept {
	const auto &next_heat = getThermalWorkerPool().execute(this);
	for (int i = 0; i < _width * _height; ++i) {
		_tags[i].heat = next_heat[i];
	}
}

} // namespace wf
