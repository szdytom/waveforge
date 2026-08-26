#ifndef WFORGE_STRUCTURES_H
#define WFORGE_STRUCTURES_H

#include "wforge/2d.h"
#include "wforge/assets.h"
#include "wforge/fallsand.h"
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace wf::structure {

struct PositionedStructure {
	PositionedStructure() noexcept = default;
	PositionedStructure(int x, int y) noexcept;

protected:
	int x;
	int y;
};

struct PixelShapedStructure : PositionedStructure {
	PixelShapedStructure(int x, int y, const PixelShape &shape) noexcept;

	void setup(PixelWorld &world);

	void customRender(
		std::span<std::uint8_t> buf, const PixelWorld &world
	) const noexcept;

	bool step(PixelWorld &world) const noexcept;
	[[nodiscard]] std::array<int, 4> queryBounds() const noexcept;

protected:
	int width() const noexcept {
		return _shape.width();
	}

	int height() const noexcept {
		return _shape.height();
	}

	PixelType pixelTypeOf(int x, int y) const noexcept;

	std::vector<std::array<int, 2>> poi;

private:
	std::unique_ptr<PixelTypeAndColor[]> _pixel_types;
	const PixelShape &_shape;
};

struct InputElectricalStructure : PixelShapedStructure {
	InputElectricalStructure(int x, int y, const PixelShape &shape) noexcept;

	bool step(PixelWorld &world) noexcept;

protected:
	static constexpr int power_capacity = 12;

	bool isPowered() const noexcept;

private:
	int _power_cap = 0;
};

struct OutputElectricalStructure : PixelShapedStructure {
	OutputElectricalStructure(int x, int y, const PixelShape &shape) noexcept;

	// Step only if powered output is needed
	bool step(PixelWorld &world) noexcept;
};

struct LaserEmitter : InputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	LaserEmitter(int x, int y, FacingDirection dir);

private:
	FacingDirection _dir;
	int power_cap = 0;
};

struct LaserReceiver : OutputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	LaserReceiver(int x, int y, FacingDirection dir);
};

struct Mirror : PixelShapedStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	Mirror(int x, int y, FacingDirection dir);
};

struct PressurePlate : OutputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	PressurePlate(int x, int y);
};

struct HeavyPressurePlate : OutputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	HeavyPressurePlate(int x, int y);
};

struct PowerSource : OutputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	PowerSource(int x, int y);
};

struct Heater : InputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	Heater(int x, int y, FacingDirection dir);
};

struct Gate : InputElectricalStructure {
	void setup(PixelWorld &world);
	bool step(PixelWorld &world) noexcept;
	void customRender(
		std::span<std::uint8_t> buf, const PixelWorld &world
	) const noexcept;
	int priority() const noexcept;

	Gate(int x, int y, FacingDirection dir);

private:
	int _openProgress() const noexcept;
	bool _canPlaceAt(
		PixelWorld &world, int progress, int *block_x, int *block_y
	) const noexcept;
	void _placeTo(PixelWorld &world, int progress, bool remove) const noexcept;

	FacingDirection _dir;
	int _open_state;

	int _gate_length;
	int _max_open_length;
	int _base_place_x;
	int _base_place_y;
	const PixelShape &_gate_wall_shape;
	std::unique_ptr<PixelTypeAndColor[]> _gate_wall_pixel_types;
};

struct TransistorNPN : InputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	TransistorNPN(int x, int y, FacingDirection dir);

private:
	bool _conducting;
	FacingDirection _dir;
};

struct TransistorPNP : InputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	TransistorPNP(int x, int y, FacingDirection dir);

private:
	bool _insulating;
	FacingDirection _dir;
};

struct WaterTap : InputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	WaterTap(int x, int y, FacingDirection dir);
};

struct OilTap : InputElectricalStructure {
	bool step(PixelWorld &world) noexcept;
	int priority() const noexcept;

	OilTap(int x, int y, FacingDirection dir);
};

} // namespace wf::structure

#endif // WFORGE_STRUCTURES_H
