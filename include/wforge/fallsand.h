#ifndef WFORGE_FALLSAND_H
#define WFORGE_FALLSAND_H

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <proxy/proxy.h>
#include <proxy/v4/proxy.h>
#include <proxy/v4/proxy_macros.h>
#include <span>
#include <vector>

namespace wf {

namespace _dispatch {

// See microsoft/proxy library for the semantics of dispatch conventions
PRO_DEF_MEM_DISPATCH(MemNewTag, newTag);
PRO_DEF_MEM_DISPATCH(MemStep, step);
PRO_DEF_MEM_DISPATCH(MemOnCharge, onCharge);

PRO_DEF_MEM_DISPATCH(MemSetup, setup);
PRO_DEF_MEM_DISPATCH(MemCustomRender, customRender);
PRO_DEF_MEM_DISPATCH(MemPriority, priority);
PRO_DEF_MEM_DISPATCH(MemQueryBounds, queryBounds);

} // namespace _dispatch

struct PixelTag;
class PixelWorld;
class GpuPhysicsBackend;
enum class WorldQueryKind : std::uint32_t;

/* clang-format off */
// See microsoft/proxy library for the semantics of proxy and facade
struct PixelFacade : pro::facade_builder
	::add_convention<_dispatch::MemNewTag, PixelTag() const noexcept>
	::add_convention<_dispatch::MemStep, void(PixelWorld &world, int x, int y) noexcept>
	::add_convention<_dispatch::MemOnCharge, void(PixelWorld &world, int x, int y) noexcept>
	::build {};

struct StructureEntityFacade : pro::facade_builder
	::add_convention<_dispatch::MemSetup, void(PixelWorld &world)>
	::add_convention<_dispatch::MemCustomRender, void(std::span<std::uint8_t> buf, const PixelWorld &world) const noexcept>
	::add_convention<_dispatch::MemStep, bool(PixelWorld &world) noexcept>
	::add_convention<_dispatch::MemPriority, int() const noexcept> // lower value means higher priority
	::add_convention<_dispatch::MemQueryBounds, std::array<int, 4>() const noexcept>
	::build {};
/* clang-format on */

using PixelElement = pro::proxy<PixelFacade>;
using StructureEntity = pro::proxy<StructureEntityFacade>;

// Insert new types to correct position! Don't just append at the end!
enum class PixelType : std::uint8_t {
	// Gas types (order: lowest to highest density)
	Smoke,
	Steam,
	Air,

	// Particle types
	FluidParticle,

	// Fluid types (order: lowest to highest density)
	Oil,
	Water,

	// Solid types
	Decoration,
	Stone,
	Wood,
	Copper,
	Sand,

	// for internal use only, keep at the end
	_count
};

static_assert(std::to_underlying(PixelType::_count) <= 64);

bool isDenser(PixelType a, PixelType b) noexcept;
bool isDenserOrEqual(PixelType a, PixelType b) noexcept;

enum class PixelClass : std::uint8_t {
	Solid = 0,
	Fluid,
	Gas,
	Particle,
};

struct PixelTag {
	static constexpr unsigned int heat_max = 127;
	static constexpr unsigned int thermal_conductivity_max = 63;
	static constexpr unsigned int electric_power_max = 15;

	PixelType type : 6;
	PixelClass pclass : 2;
	unsigned int color_index : 8; // 256 colors, see Colorpalette.h
	bool dirty : 1 = false;       // updated in current step, for physics
	bool is_free_falling : 1 = false;
	signed int fluid_dir : 2; // -1 = left, 0 = none, +1 = right
	unsigned int heat : 7 = 0;
	bool ignited : 1 = false; // on fire
	unsigned int thermal_conductivity : 6 = 0;
	unsigned int electric_power : 4 = 0;
};

struct StaticPixelTag {
	bool laser_active : 1 = false;
	bool laser_stroke : 1 = false;
	bool external_entity_present : 1 = false;
	bool is_reflective_surface : 1 = false;
};

class PixelWorld {
public:
	constexpr static float gAcceleration = 0.5f;

	PixelWorld() noexcept;
	PixelWorld(int width, int height);
	~PixelWorld() noexcept;
	PixelWorld(PixelWorld &&) noexcept;
	PixelWorld &operator=(PixelWorld &&) noexcept;
	PixelWorld(const PixelWorld &) = delete;
	PixelWorld &operator=(const PixelWorld &) = delete;

	int width() const noexcept {
		return _width;
	}

	int height() const noexcept {
		return _height;
	}

	bool inBounds(int x, int y) const noexcept {
		return x >= 0 && x < _width && y >= 0 && y < _height;
	}

	PixelTag tagOf(int x, int y) const noexcept;
	PixelTag &tagOf(int x, int y) noexcept;
	PixelElement &elementOf(int x, int y) noexcept;

	StaticPixelTag staticTagOf(int x, int y) const noexcept;
	StaticPixelTag &staticTagOf(int x, int y) noexcept;

	void activateLaserAt(int x, int y) noexcept;
	bool isExternalEntityPresent(int x, int y) const noexcept;

	void swapPixels(int x1, int y1, int x2, int y2) noexcept;

	// swapPixels without swapping fluid_dir
	void swapFluids(int x1, int y1, int x2, int y2) noexcept;

	void replacePixel(int x, int y, PixelElement new_pixel) noexcept;
	void replacePixel(
		int x, int y, PixelElement new_pixel, PixelTag new_tag
	) noexcept;
	void replacePixelWithAir(int x, int y) noexcept;

	void chargeElement(int x, int y) noexcept;

	bool typeOfIs(int x, int y, PixelType ptype) const noexcept;
	bool classOfIs(int x, int y, PixelClass pclass) const noexcept;

	void step();

	void requestQueryRegion(
		WorldQueryKind kind, int x, int y, int width, int height
	);

	void renderToBuffer(std::span<std::uint8_t> buf) const noexcept;
	[[nodiscard]] bool renderHeatToBuffer(
		std::span<std::uint8_t> buf
	) const noexcept;

	void addStructure(StructureEntity structure);

	void resetEntityPresenceTags() noexcept;

protected:
	void resetDirtyFlags() noexcept;

	// Global fluid analysis, custom heuristics
	void fluidAnalysisStep() noexcept;

	// Global thermal analysis
	void thermalAnalysisStep() noexcept;

private:
	int _width;
	int _height;

	std::unique_ptr<PixelTag[]> _tags;
	std::unique_ptr<PixelElement[]> _elements;
	std::unique_ptr<StaticPixelTag[]> _static_tags;
	std::vector<StructureEntity> _structures;

#ifdef WAVEFORGE_ENABLE_WEBGPU
	void _applyCompletedGpuFrame();
	void _submitGpuEdits();

	std::unique_ptr<GpuPhysicsBackend> _gpu_backend;
	std::unique_ptr<PixelTag[]> _submitted_tags;
	std::unique_ptr<StaticPixelTag[]> _submitted_static_tags;
	std::vector<std::array<int, 6>> _gpu_query_regions;
	std::uint64_t _last_gpu_frame = std::numeric_limits<std::uint64_t>::max();
	std::uint32_t _next_gpu_query_id = 1;
	bool _gpu_level_uploaded = false;
#endif
};
} // namespace wf

#endif // WFORGE_FALLSAND_H
