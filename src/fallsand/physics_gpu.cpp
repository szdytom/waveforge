#include "wforge/physics_gpu.h"
#include "wforge/colorpalette.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

namespace wf {

namespace {

constexpr std::uint32_t WORKGROUP_SIZE = 256;
constexpr std::uint32_t MAX_EDIT_COMMANDS = 4096;
constexpr std::uint32_t MAX_QUERY_REQUESTS = 256;
constexpr std::uint32_t PRESSURE_ITERATIONS = 2;
constexpr std::uint32_t INVALID_CELL = 0xffffffffU;
constexpr std::uint32_t TIMESTAMP_COUNT = 16;

[[nodiscard]] constexpr WGPUInstanceBackend platformInstanceBackend() noexcept {
#if defined(_WIN32)
	return WGPUInstanceBackend_DX12;
#elif defined(__APPLE__)
	return WGPUInstanceBackend_Metal;
#else
	return WGPUInstanceBackend_Vulkan;
#endif
}

[[nodiscard]] constexpr WGPUBackendType platformBackendType() noexcept {
#if defined(_WIN32)
	return WGPUBackendType_D3D12;
#elif defined(__APPLE__)
	return WGPUBackendType_Metal;
#else
	return WGPUBackendType_Vulkan;
#endif
}

constexpr char PHYSICS_SHADER[] = R"(
struct Cell {
	metadata: u32,
	dynamics: u32,
}

struct Params {
	width: u32,
	height: u32,
	tick: u32,
	command_count: u32,
	query_count: u32,
	query_cell_count: u32,
	chunk_width: u32,
	chunk_count: u32,
	pressure_direction: u32,
	air_metadata: u32,
	fire_colors: u32,
	laser_stroke_color: u32,
}

struct Material {
	classes: u32,
	conductivity_density: u32,
	phase_thresholds: u32,
	colors: u32,
}

struct EditCommand {
	kind: u32,
	x: i32,
	y: i32,
	width: u32,
	height: u32,
	value: u32,
	secondary_value: u32,
	flags: u32,
}

struct QueryRequest {
	kind: u32,
	id: u32,
	x: i32,
	y: i32,
	width: u32,
	height: u32,
	cell_offset: u32,
	cell_count: u32,
}

struct Scratch {
	transfers: vec4<u32>,
	proposal_dest: u32,
	winner: atomic<u32>,
	electricity: atomic<u32>,
	padding: u32,
}

@group(0) @binding(0) var<storage, read_write> state_in: array<Cell>;
@group(0) @binding(1) var<storage, read_write> state_out: array<Cell>;
@group(0) @binding(2) var<storage, read_write> scratch: array<Scratch>;
@group(0) @binding(3) var<storage, read> edits: array<EditCommand>;
@group(0) @binding(4) var<storage, read> queries: array<QueryRequest>;
@group(0) @binding(5) var<storage, read> materials: array<Material>;
@group(0) @binding(6) var<storage, read_write> chunk_activity: array<atomic<u32>>;
@group(0) @binding(7) var<storage, read_write> rgba: array<u32>;
@group(0) @binding(8) var<storage, read_write> heat_rgba: array<u32>;
@group(0) @binding(9) var<storage, read_write> query_cells: array<Cell>;
@group(0) @binding(10) var<storage, read_write> counters: array<atomic<u32>>;
@group(0) @binding(11) var<storage, read> palette: array<u32>;
@group(0) @binding(12) var<uniform> params: Params;

const TYPE_MASK = 0x3fu;
const CLASS_MASK = 0xc0u;
const COLOR_MASK = 0xff00u;
const IGNITED_MASK = 0x10000u;
const FALLING_MASK = 0x20000u;
const FLUID_DIRECTION_MASK = 0xc0000u;
const ELECTRICITY_MASK = 0xf00000u;
const TRANSIENT_MASK = 0x7000000u;
const LOCATION_MASK = 0xf000000u;
const HEAT_MASK = 0x7fu;
const BURN_MASK = 0x7f80u;
const AIR = 2u;
const PARTICLE = 3u;
const OIL = 4u;
const WATER = 5u;
const WOOD = 8u;
const COPPER = 9u;
const SAND = 10u;
const SMOKE = 0u;
const STEAM = 1u;
const GAS_CLASS = 2u;
const FLUID_CLASS = 1u;
const SOLID_CLASS = 0u;
const PARTICLE_CLASS = 3u;
const INVALID_CELL = 0xffffffffu;

fn cell_count() -> u32 { return params.width * params.height; }
fn cell_type(cell: Cell) -> u32 { return cell.metadata & TYPE_MASK; }
fn cell_class(cell: Cell) -> u32 { return (cell.metadata & CLASS_MASK) >> 6u; }
fn cell_heat(cell: Cell) -> u32 { return cell.dynamics & HEAT_MASK; }
fn conductivity(cell: Cell) -> u32 {
	return materials[cell_type(cell)].conductivity_density & 0x3fu;
}
fn density(cell: Cell) -> u32 {
	return (materials[cell_type(cell)].conductivity_density >> 8u) & 0xffu;
}
fn with_heat(cell: Cell, heat: u32) -> Cell {
	return Cell(cell.metadata, (cell.dynamics & ~HEAT_MASK) | min(heat, 127u));
}
fn with_type(cell: Cell, kind: u32) -> Cell {
	let material = materials[kind];
	let class_bits = (material.classes & 3u) << 6u;
	let default_color = (material.colors & 0xffu) << 8u;
	return Cell(
		(cell.metadata & ~(TYPE_MASK | CLASS_MASK | COLOR_MASK | IGNITED_MASK | FALLING_MASK | FLUID_DIRECTION_MASK))
			| kind | class_bits | default_color,
		cell.dynamics
	);
}
fn initial_burn_lifetime(kind: u32, index: u32) -> u32 {
	if (kind == OIL) {
		return 36u + countOneBits(hash(index, 9u, 0u) & 0xffffffu);
	}
	if (kind == WOOD) {
		return 72u + countOneBits(hash(index, 9u, 0u) & 0xffffffu)
			+ countOneBits(hash(index, 9u, 1u) & 0xffffffu);
	}
	return 0u;
}
fn at_location(cell: Cell, location: Cell) -> Cell {
	return Cell(
		(cell.metadata & ~LOCATION_MASK) | (location.metadata & LOCATION_MASK),
		cell.dynamics
	);
}
fn hash(cell: u32, pass_id: u32, direction: u32) -> u32 {
	var value = params.tick ^ cell * 0x9e3779b9u ^ pass_id * 0x85ebca6bu ^ direction * 0xc2b2ae35u;
	value = (value ^ (value >> 16u)) * 0x7feb352du;
	value = (value ^ (value >> 15u)) * 0x846ca68bu;
	return value ^ (value >> 16u);
}
fn valid_neighbor(index: u32, direction: u32) -> bool {
	let x = index % params.width;
	let y = index / params.width;
	return (direction != 0u || x > 0u)
		&& (direction != 1u || x + 1u < params.width)
		&& (direction != 2u || y > 0u)
		&& (direction != 3u || y + 1u < params.height);
}
fn neighbor(index: u32, direction: u32) -> u32 {
	if (direction == 0u) { return index - 1u; }
	if (direction == 1u) { return index + 1u; }
	if (direction == 2u) { return index - params.width; }
	return index + params.width;
}
fn chunk_index(index: u32) -> u32 {
	return (index / params.width / 32u) * params.chunk_width
		+ (index % params.width / 32u);
}
fn chunk_active(index: u32) -> bool {
	return atomicLoad(&chunk_activity[chunk_index(index)]) != 0u;
}
fn entity_blocks(index: u32) -> bool {
	return (state_in[index].metadata & (1u << 26u)) != 0u;
}
fn gas_can_swap(target_index: u32, source: Cell) -> bool {
	let target_cell = state_in[target_index];
	return cell_class(target_cell) == FLUID_CLASS
		|| (cell_class(target_cell) == GAS_CLASS
			&& density(target_cell) > density(source));
}
fn in_command(command: EditCommand, x: i32, y: i32) -> bool {
	return x >= command.x && y >= command.y
		&& x < command.x + i32(command.width)
		&& y < command.y + i32(command.height);
}

@compute @workgroup_size(256)
fn apply_commands(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	let x = i32(index % params.width);
	let y = i32(index / params.width);
	var cell = state_in[index];
	cell.metadata &= ~TRANSIENT_MASK;
	for (var command_index = 0u; command_index < params.command_count; command_index++) {
		let command = edits[command_index];
		if (!in_command(command, x, y)) { continue; }
		if (command.kind == 0u || command.kind == 4u) {
			cell = with_type(cell, command.value & TYPE_MASK);
			if ((command.secondary_value & 0xffu) != 255u) {
				cell.metadata = (cell.metadata & ~COLOR_MASK) | ((command.secondary_value & 0xffu) << 8u);
			}
			if (command.value == OIL || command.value == WOOD) {
				cell.dynamics = (cell.dynamics & ~BURN_MASK)
					| (initial_burn_lifetime(command.value, index) << 7u);
			}
		} else if (command.kind == 1u) {
			cell = Cell(params.air_metadata, 0u);
		} else if (command.kind == 2u) {
			let signed_amount = bitcast<i32>(command.value);
			let next_heat = clamp(i32(cell_heat(cell)) + signed_amount, 0, 127);
			cell = with_heat(cell, u32(next_heat));
		} else if (command.kind == 3u) {
			cell.metadata = (cell.metadata & ~ELECTRICITY_MASK) | ((command.value & 15u) << 20u);
		} else if (command.kind == 5u) {
			cell.metadata = (cell.metadata & ~(1u << 26u)) | ((command.value & 1u) << 26u);
		} else if (command.kind == 6u) {
			cell.metadata = (cell.metadata & ~((1u << 24u) | (1u << 25u)))
				| ((command.value & 1u) << 24u) | ((command.secondary_value & 1u) << 25u);
		}
	}
	state_in[index] = cell;
}

@compute @workgroup_size(256)
fn update_chunks(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	let cell = state_in[index];
	if (cell_type(cell) != AIR || cell_heat(cell) != 0u || (cell.metadata & (ELECTRICITY_MASK | TRANSIENT_MASK)) != 0u) {
		let own = chunk_index(index);
		atomicStore(&chunk_activity[own], 1u);
		let chunk_x = own % params.chunk_width;
		let chunk_y = own / params.chunk_width;
		if (index % params.width % 32u == 0u && chunk_x > 0u) { atomicStore(&chunk_activity[own - 1u], 1u); }
		if (index % params.width % 32u == 31u && chunk_x + 1u < params.chunk_width) { atomicStore(&chunk_activity[own + 1u], 1u); }
		if (index / params.width % 32u == 0u && chunk_y > 0u) { atomicStore(&chunk_activity[own - params.chunk_width], 1u); }
		if (index / params.width % 32u == 31u && own + params.chunk_width < params.chunk_count) { atomicStore(&chunk_activity[own + params.chunk_width], 1u); }
	}
}

@compute @workgroup_size(256)
fn thermal_propose(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	var proposal = vec4<u32>(0u);
	if (!chunk_active(index)) { scratch[index].transfers = proposal; return; }
	let cell = state_in[index];
	let source_heat = cell_heat(cell);
	let source_conductivity = conductivity(cell);
	if (source_heat == 0u || source_conductivity == 0u) { scratch[index].transfers = proposal; return; }
	var weights = vec4<u32>(0u);
	var total_weight = source_heat * (63u - source_conductivity) * 100u / 15u;
	for (var direction = 0u; direction < 4u; direction++) {
		if (!valid_neighbor(index, direction)) { continue; }
		let other = state_in[neighbor(index, direction)];
		let delta = select(0u, source_heat - cell_heat(other), source_heat > cell_heat(other));
		weights[direction] = delta * min(source_conductivity, conductivity(other));
		total_weight += weights[direction];
	}
	if (total_weight != 0u) {
		var available = source_heat;
		for (var direction = 0u; direction < 4u; direction++) {
			let numerator = source_heat * weights[direction];
			var amount = numerator / total_weight;
			let remainder = numerator % total_weight;
			if (remainder != 0u && hash(index, 2u, direction) % (2u * total_weight) < remainder) { amount++; }
			amount = min(amount, available);
			proposal[direction] = amount;
			available -= amount;
		}
	}
	scratch[index].transfers = proposal;
}

@compute @workgroup_size(256)
fn thermal_gather(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	if (!chunk_active(index)) { state_out[index] = state_in[index]; return; }
	let outgoing = scratch[index].transfers;
	var next_heat = cell_heat(state_in[index]) - outgoing.x - outgoing.y - outgoing.z - outgoing.w;
	let opposite = array<u32, 4>(1u, 0u, 3u, 2u);
	for (var direction = 0u; direction < 4u; direction++) {
		if (valid_neighbor(index, direction)) {
			next_heat += scratch[neighbor(index, direction)].transfers[opposite[direction]];
		}
	}
	let remainder = next_heat % 200u;
	next_heat -= next_heat / 200u;
	if (next_heat > 0u && hash(index, 3u, 0u) % 200u < remainder) { next_heat--; }
	state_out[index] = with_heat(state_in[index], next_heat);
}

@compute @workgroup_size(256)
fn transitions(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count() || !chunk_active(index)) { return; }
	var cell = state_in[index];
	var kind = cell_type(cell);
	let heat = cell_heat(cell);
	if ((kind == SMOKE || kind == STEAM) && index < params.width) {
		cell = with_type(cell, AIR);
		atomicAdd(&counters[3], 1u);
	} else if (kind == WATER && heat >= 30u) {
		cell = with_type(cell, STEAM);
		atomicAdd(&counters[3], 1u);
	} else if (kind == STEAM && heat <= 10u) {
		cell = with_type(cell, WATER);
		atomicAdd(&counters[3], 1u);
	} else if (kind == SMOKE && heat <= 6u) {
		cell = with_type(cell, AIR);
		atomicAdd(&counters[3], 1u);
	} else if (kind == OIL || kind == WOOD) {
		let threshold = select(40u, 60u, kind == WOOD);
		var ignited = (cell.metadata & IGNITED_MASK) != 0u;
		if (!ignited && heat >= threshold && (kind == OIL || hash(index, 4u, 0u) % 100u < 10u)) { ignited = true; }
		if (kind == OIL && ignited && heat < threshold) { ignited = false; }
		if (ignited) {
			cell.metadata |= IGNITED_MASK;
			var lifetime = (cell.dynamics >> 7u) & 0xffu;
			if (lifetime > 0u) { lifetime--; }
			cell.dynamics = (cell.dynamics & ~BURN_MASK) | (lifetime << 7u);
			cell = with_heat(cell, min(127u, heat + select(50u, 40u, kind == WOOD)));
			for (var direction = 0u; direction < 4u; direction++) {
				if (valid_neighbor(index, direction)) {
					atomicAdd(&scratch[neighbor(index, direction)].electricity, select(1u, 2u, kind == WOOD));
				}
			}
			if (valid_neighbor(index, 2u)) {
				// Normalize oil smoke for simultaneous GPU updates; wood remains 2%.
				let smoke_chance = select(1u, 6u, kind == WOOD);
				if (hash(index, 4u, 2u) % 300u < smoke_chance) {
					let smoke_bits = 0x80000000u | select(0x40000000u, 0u, kind == WOOD);
					atomicOr(&scratch[neighbor(index, 2u)].electricity, smoke_bits);
				}
			}
			if (lifetime == 0u) {
				cell = with_type(cell, select(AIR, SMOKE, hash(index, 4u, 1u) % 100u < 25u));
				atomicAdd(&counters[3], 1u);
			}
		}
	}
	state_in[index] = cell;
}

@compute @workgroup_size(256)
fn transition_spawns(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count() || !chunk_active(index)) { return; }
	let proposal = atomicLoad(&scratch[index].electricity);
	var cell = state_in[index];
	let added_heat = proposal & 0xffu;
	if (added_heat != 0u) {
		cell = with_heat(cell, min(127u, cell_heat(cell) + added_heat));
	}
	if ((proposal & 0x80000000u) != 0u && cell_type(cell) == AIR) {
		cell = with_type(cell, SMOKE);
		cell = with_heat(cell, select(40u, 50u, (proposal & 0x40000000u) != 0u));
		atomicAdd(&counters[3], 1u);
	}
	state_in[index] = cell;
}

fn movement_target(index: u32, pressure_only: bool) -> u32 {
	let cell = state_in[index];
	let kind = cell_type(cell);
	let pixel_class = cell_class(cell);
	let preferred = select(0u, 1u, ((params.tick + params.pressure_direction) & 1u) != 0u);
	if (pressure_only) {
		if (pixel_class != FLUID_CLASS) { return INVALID_CELL; }
		if (valid_neighbor(index, 2u) && valid_neighbor(index, 3u)) {
			let above_index = neighbor(index, 2u);
			let below_index = neighbor(index, 3u);
			if (!entity_blocks(above_index)
				&& cell_class(state_in[above_index]) == GAS_CLASS
				&& cell_type(state_in[below_index]) == kind) {
				return above_index;
			}
		}
		for (var attempt = 0u; attempt < 2u; attempt++) {
			let direction = select(preferred, 1u - preferred, attempt != 0u);
			if (valid_neighbor(index, direction)) {
				let target_index = neighbor(index, direction);
				if (!entity_blocks(target_index) && cell_class(state_in[target_index]) == GAS_CLASS) { return target_index; }
			}
		}
		return INVALID_CELL;
	}
	if (pixel_class == FLUID_CLASS || kind == SAND || pixel_class == PARTICLE_CLASS) {
		if (valid_neighbor(index, 3u)) {
			let target_index = neighbor(index, 3u);
			let other = state_in[target_index];
			if (!entity_blocks(target_index) && (cell_class(other) == GAS_CLASS || (cell_class(other) == FLUID_CLASS && density(cell) > density(other)))) { return target_index; }
		}
		for (var attempt = 0u; attempt < 2u; attempt++) {
			let side = select(preferred, 1u - preferred, attempt != 0u);
			if (!valid_neighbor(index, side)) { continue; }
			let side_index = neighbor(index, side);
			if (valid_neighbor(side_index, 3u)) {
				let diagonal = neighbor(side_index, 3u);
				if (!entity_blocks(diagonal) && cell_class(state_in[diagonal]) == GAS_CLASS) { return diagonal; }
			}
			if (pixel_class == FLUID_CLASS && !entity_blocks(side_index) && cell_class(state_in[side_index]) == GAS_CLASS) { return side_index; }
		}
	} else if (pixel_class == GAS_CLASS && kind != AIR) {
		let gas_direction = select(0u, 1u, (hash(index, 5u, 1u) & 1u) != 0u);
		if (valid_neighbor(index, gas_direction) && valid_neighbor(index, 2u)
			&& hash(index, 5u, 2u) % 100u < 50u) {
			let diagonal = neighbor(neighbor(index, gas_direction), 2u);
			if (!entity_blocks(diagonal) && gas_can_swap(diagonal, cell)) {
				return diagonal;
			}
		}
		if (valid_neighbor(index, 2u)) {
			let above_index = neighbor(index, 2u);
			if (!entity_blocks(above_index) && gas_can_swap(above_index, cell)) {
				return above_index;
			}
		}
		let direction_delta = select(-1, 1, gas_direction == 1u);
		let source_x = i32(index % params.width);
		let source_y = i32(index / params.width);
		var candidate = INVALID_CELL;
		for (var distance = 1; distance <= 4; distance++) {
			let target_x = source_x + direction_delta * distance;
			if (target_x < 0 || target_x >= i32(params.width)) { break; }
			let side_index = u32(source_y) * params.width + u32(target_x);
			if (cell_class(state_in[side_index]) == SOLID_CLASS) { break; }
			if (source_y > 0) {
				let diagonal = side_index - params.width;
				if (!entity_blocks(diagonal) && gas_can_swap(diagonal, cell)) {
					return diagonal;
				}
			}
			if (!entity_blocks(side_index) && gas_can_swap(side_index, cell)) {
				candidate = side_index;
			}
		}
		return candidate;
	}
	return INVALID_CELL;
}

fn propose_movement(index: u32, pressure_only: bool) {
	if (index >= cell_count()) { return; }
	scratch[index].proposal_dest = INVALID_CELL;
	if (!chunk_active(index)) { return; }
	let target_index = movement_target(index, pressure_only);
	scratch[index].proposal_dest = target_index;
	if (target_index == INVALID_CELL) { return; }
	let priority = min(4095u, 256u + density(state_in[index]) * 8u + (hash(index, select(5u, 6u, pressure_only), 0u) & 7u));
	let encoded = (priority << 20u) | ((index + 1u) & 0xfffffu);
	atomicMax(&scratch[target_index].winner, encoded);
}

@compute @workgroup_size(256)
fn clear_movement_scratch(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	scratch[index].proposal_dest = INVALID_CELL;
	atomicStore(&scratch[index].winner, 0u);
}

@compute @workgroup_size(256)
fn movement_propose(@builtin(global_invocation_id) id: vec3<u32>) { propose_movement(id.x, false); }

@compute @workgroup_size(256)
fn pressure_propose(@builtin(global_invocation_id) id: vec3<u32>) { propose_movement(id.x, true); }

fn lift_pressurized_fluid(index: u32, phase: u32) {
	if (index >= cell_count()) { return; }
	let cell = state_in[index];
	if (!chunk_active(index)) { state_out[index] = cell; return; }
	let y = index / params.width;
	let lower_half = ((y + phase) & 1u) == 0u;
	let direction = select(2u, 3u, lower_half);
	if (!valid_neighbor(index, direction)) { state_out[index] = cell; return; }
	let paired_index = neighbor(index, direction);
	let upper_index = min(index, paired_index);
	let lower_index = max(index, paired_index);
	let upper = state_in[upper_index];
	let lower = state_in[lower_index];
	if (cell_class(upper) == GAS_CLASS && cell_class(lower) == FLUID_CLASS
		&& !entity_blocks(upper_index) && valid_neighbor(lower_index, 3u)
		&& cell_type(state_in[neighbor(lower_index, 3u)]) == cell_type(lower)) {
		if (index == lower_index) {
			state_out[index] = at_location(upper, lower);
		} else {
			state_out[index] = at_location(lower, upper);
		}
		return;
	}
	state_out[index] = cell;
}

@compute @workgroup_size(256)
fn pressure_lift_0(@builtin(global_invocation_id) id: vec3<u32>) { lift_pressurized_fluid(id.x, 0u); }
@compute @workgroup_size(256)
fn pressure_lift_1(@builtin(global_invocation_id) id: vec3<u32>) { lift_pressurized_fluid(id.x, 1u); }

fn accepted_target(source: u32) -> u32 {
	let target_index = scratch[source].proposal_dest;
	if (target_index == INVALID_CELL) { return INVALID_CELL; }
	let target_winner = atomicLoad(&scratch[target_index].winner);
	if ((target_winner & 0xfffffu) != ((source + 1u) & 0xfffffu)) {
		return INVALID_CELL;
	}
	return target_index;
}

@compute @workgroup_size(256)
fn movement_apply(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	if (!chunk_active(index)) { state_out[index] = state_in[index]; return; }
	let winning = atomicLoad(&scratch[index].winner);
	if (winning != 0u) {
		let source = (winning & 0xfffffu) - 1u;
		state_out[index] = at_location(state_in[source], state_in[index]);
		return;
	}
	var target_index = accepted_target(index);
	if (target_index != INVALID_CELL) {
		for (var step = 0u; step < cell_count(); step++) {
			let next_target = accepted_target(target_index);
			if (next_target == INVALID_CELL) {
				state_out[index] = at_location(state_in[target_index], state_in[index]);
				return;
			}
			target_index = next_target;
		}
	}
	let proposed_target = scratch[index].proposal_dest;
	if (proposed_target != INVALID_CELL
		&& accepted_target(index) == INVALID_CELL
		&& atomicLoad(&scratch[proposed_target].winner) != 0u) {
		atomicAdd(&counters[2], 1u);
	}
	state_out[index] = state_in[index];
}

@compute @workgroup_size(256)
fn electricity_propose(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count() || cell_type(state_in[index]) != COPPER) { return; }
	let power = (state_in[index].metadata & ELECTRICITY_MASK) >> 20u;
	if (power != 14u) { return; }
	let offsets = array<vec2<i32>, 8>(
		vec2<i32>(-1, -1), vec2<i32>(0, -1), vec2<i32>(1, -1),
		vec2<i32>(-1, 0), vec2<i32>(1, 0), vec2<i32>(-1, 1),
		vec2<i32>(0, 1), vec2<i32>(1, 1)
	);
	let x = i32(index % params.width);
	let y = i32(index / params.width);
	for (var direction = 0u; direction < 8u; direction++) {
		let target_position = vec2<i32>(x, y) + offsets[direction];
		if (target_position.x < 0 || target_position.y < 0 || target_position.x >= i32(params.width) || target_position.y >= i32(params.height)) { continue; }
		let target_index = u32(target_position.y) * params.width + u32(target_position.x);
		if (cell_type(state_in[target_index]) == COPPER) { atomicMax(&scratch[target_index].electricity, 15u); }
	}
}

@compute @workgroup_size(256)
fn electricity_apply(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	var cell = state_in[index];
	let old_power = (cell.metadata & ELECTRICITY_MASK) >> 20u;
	let proposed = atomicLoad(&scratch[index].electricity);
	let decayed_power = select(0u, old_power - 1u, old_power > 0u);
	let next_power = select(proposed, decayed_power, decayed_power > 0u);
	cell.metadata = (cell.metadata & ~ELECTRICITY_MASK) | ((next_power & 15u) << 20u);
	state_in[index] = cell;
}

@compute @workgroup_size(256)
fn output_pixels(@builtin(global_invocation_id) id: vec3<u32>) {
	let index = id.x;
	if (index >= cell_count()) { return; }
	let cell = state_in[index];
	var color_index = (cell.metadata & COLOR_MASK) >> 8u;
	if ((cell.metadata & IGNITED_MASK) != 0u) {
		let fire = (params.fire_colors >> ((hash(index, 8u, 0u) % 3u) * 8u)) & 0xffu;
		color_index = fire;
	}
	var color = palette[color_index];
	if ((cell.metadata & (1u << 24u)) != 0u) {
		color = palette[512u + color_index];
	} else if (((cell.metadata & ELECTRICITY_MASK) >> 20u) >= 12u) {
		color = palette[256u + color_index];
	} else if (cell_type(cell) == AIR && (cell.metadata & (1u << 25u)) != 0u) {
		color = palette[params.laser_stroke_color];
	}
	rgba[index] = color;
	let alpha = min(255u, cell_heat(cell) * 256u / 127u);
	heat_rgba[index] = 255u | (alpha << 24u);
	atomicAdd(&counters[1], 1u);
}

@compute @workgroup_size(256)
fn output_queries(@builtin(global_invocation_id) id: vec3<u32>) {
	let output_index = id.x;
	if (output_index >= params.query_cell_count) { return; }
	for (var query_index = 0u; query_index < params.query_count; query_index++) {
		let query = queries[query_index];
		if (output_index < query.cell_offset || output_index >= query.cell_offset + query.cell_count) { continue; }
		let local = output_index - query.cell_offset;
		let world_x = u32(query.x) + local % query.width;
		let world_y = u32(query.y) + local / query.width;
		query_cells[output_index] = state_in[world_y * params.width + world_x];
		return;
	}
}

@compute @workgroup_size(256)
fn count_chunks(@builtin(global_invocation_id) id: vec3<u32>) {
	if (id.x < params.chunk_count && atomicLoad(&chunk_activity[id.x]) != 0u) { atomicAdd(&counters[0], 1u); }
}
)";

WGPUStringView stringView(const char *text) noexcept {
	return {.data = text, .length = WGPU_STRLEN};
}

std::string stringFromView(WGPUStringView view) {
	return view.data == nullptr ? std::string{}
								: std::string(view.data, view.length);
}

std::string backendName(WGPUBackendType backend) {
	switch (backend) {
	case WGPUBackendType_Vulkan:
		return "Vulkan";
	case WGPUBackendType_Metal:
		return "Metal";
	case WGPUBackendType_D3D12:
		return "DX12";
	case WGPUBackendType_D3D11:
		return "DX11";
	case WGPUBackendType_OpenGL:
		return "OpenGL";
	case WGPUBackendType_OpenGLES:
		return "OpenGL ES";
	default:
		return "Unknown";
	}
}

std::uint64_t alignedSize(std::uint64_t size, std::uint64_t alignment = 256) {
	return (size + alignment - 1) / alignment * alignment;
}

std::uint32_t packedRgba(sf::Color color) noexcept {
	return color.r | (static_cast<std::uint32_t>(color.g) << 8U)
		| (static_cast<std::uint32_t>(color.b) << 16U)
		| (static_cast<std::uint32_t>(color.a) << 24U);
}

template<typename Result>
void waitForResult(WGPUInstance instance, Result &result, const char *name) {
	while (!result.completed) {
		wgpuInstanceProcessEvents(instance);
	}
	if (!result.success) {
		throw std::runtime_error(std::format("{}: {}", name, result.message));
	}
}

WGPUBuffer createBuffer(
	WGPUDevice device, std::uint64_t size, WGPUBufferUsage usage,
	const char *label
) {
	WGPUBufferDescriptor descriptor{
		.label = stringView(label),
		.usage = usage,
		.size = std::max<std::uint64_t>(size, 4),
	};
	WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);
	if (buffer == nullptr) {
		throw std::runtime_error(std::format("Failed to create {}", label));
	}
	return buffer;
}

struct GpuQueryRequest {
	std::uint32_t kind;
	std::uint32_t id;
	std::int32_t x;
	std::int32_t y;
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t cell_offset;
	std::uint32_t cell_count;
};

static_assert(sizeof(GpuQueryRequest) == 32);

struct GpuParams {
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t tick;
	std::uint32_t command_count;
	std::uint32_t query_count;
	std::uint32_t query_cell_count;
	std::uint32_t chunk_width;
	std::uint32_t chunk_count;
	std::uint32_t pressure_direction;
	std::uint32_t air_metadata;
	std::uint32_t fire_colors;
	std::uint32_t laser_stroke_color;
};

} // namespace

class GpuPhysicsBackend::Impl {
public:
	explicit Impl(int width, int height);
	~Impl() noexcept;

	void uploadLevel(std::span<const PackedCellState> state);
	void submit(WorldEditBatch edits, std::vector<WorldQueryRequest> queries);
	void step();
	void poll() noexcept;
	[[nodiscard]] PhysicsFrame latestFrame() const noexcept;
	[[nodiscard]] std::vector<PackedCellState> serialize();

	int width;
	int height;
	std::uint32_t cell_count;
	std::uint32_t chunk_width;
	std::uint32_t chunk_height;
	std::uint32_t chunk_count;
	std::uint64_t tick = 0;
	bool uploaded = false;
	bool lost = false;
	std::string error;
	PhysicsAdapterDiagnostics adapter_diagnostics;
	WorldEditBatch pending_edits;
	std::vector<WorldQueryRequest> pending_queries;

private:
	struct ReadbackSlot {
		Impl *owner = nullptr; // not owned, the slot is a member of the owner
		WGPUBuffer buffer = nullptr;
		bool busy = false;
		bool ready = false;
		std::uint64_t tick = 0;
		std::chrono::steady_clock::time_point submitted_at;
		std::vector<WorldQueryRequest> requests;
		std::vector<std::uint8_t> pixels;
		std::vector<std::uint8_t> heat_pixels;
		bool has_heat = false;
		WorldQuerySnapshot snapshot;
		PhysicsTimings timings;
		std::uint32_t command_bytes = 0;
		std::uint32_t query_bytes = 0;
	};

	void _createContext();
	void _createResources();
	void _createPipelines();
	void _createBindGroups();
	void _releaseResources() noexcept;
	void _encodePass(
		WGPUComputePassEncoder pass, WGPUComputePipeline pipeline,
		WGPUBindGroup bind_group, std::uint32_t workgroups
	) const noexcept;
	WGPUBindGroup _makeBindGroup(
		WGPUComputePipeline pipeline,
		std::span<const WGPUBindGroupEntry> entries, const char *label
	);
	void _scheduleReadback(WGPUCommandEncoder encoder, ReadbackSlot &slot);
	void _finishReadback(ReadbackSlot &slot, const void *mapped);
	ReadbackSlot *_freeReadbackSlot() noexcept;

	WGPUInstance instance = nullptr;
	WGPUAdapter adapter = nullptr;
	WGPUDevice device = nullptr;
	WGPUQueue queue = nullptr;
	WGPUShaderModule shader = nullptr;
	WGPUBuffer state_a = nullptr;
	WGPUBuffer state_b = nullptr;
	WGPUBuffer scratch = nullptr;
	WGPUBuffer commands = nullptr;
	WGPUBuffer query_requests = nullptr;
	WGPUBuffer materials = nullptr;
	WGPUBuffer chunks = nullptr;
	WGPUBuffer rgba = nullptr;
	WGPUBuffer heat_rgba = nullptr;
	WGPUBuffer query_cells = nullptr;
	WGPUBuffer counters = nullptr;
	WGPUBuffer palette = nullptr;
	WGPUBuffer params = nullptr;
	WGPUBuffer timestamp_values = nullptr;
	WGPUQuerySet timestamp_queries = nullptr;
	bool timestamps_supported = false;
	float timestamp_period = 1.0F;
	std::vector<WGPUComputePipeline> pipelines;
	std::vector<WGPUBindGroup> bind_groups;
	WGPUBindGroup apply_group = nullptr;
	WGPUBindGroup chunks_group = nullptr;
	WGPUBindGroup thermal_propose_group = nullptr;
	WGPUBindGroup thermal_gather_group = nullptr;
	WGPUBindGroup transitions_group = nullptr;
	WGPUBindGroup transition_spawns_group = nullptr;
	WGPUBindGroup movement_propose_b_group = nullptr;
	WGPUBindGroup pressure_propose_a_group = nullptr;
	WGPUBindGroup pressure_propose_b_group = nullptr;
	WGPUBindGroup movement_apply_ba_group = nullptr;
	WGPUBindGroup movement_apply_ab_group = nullptr;
	std::array<WGPUBindGroup, 2> pressure_lift_groups{};
	WGPUBindGroup electricity_propose_group = nullptr;
	WGPUBindGroup electricity_apply_group = nullptr;
	WGPUBindGroup output_pixels_group = nullptr;
	WGPUBindGroup output_queries_group = nullptr;
	WGPUBindGroup count_chunks_group = nullptr;
	WGPUBindGroup clear_scratch_group = nullptr;
	std::array<ReadbackSlot, PHYSICS_READBACK_RING_SIZE> readback_slots;
	std::size_t next_readback_slot = 0;
	std::optional<std::size_t> latest_readback_slot;
	std::uint64_t state_size;
	std::uint64_t rgba_size;
	std::uint64_t heat_size;
	std::uint64_t query_size;
	std::uint64_t counter_size = 4 * sizeof(std::uint32_t);
	std::uint64_t rgba_offset = 0;
	std::uint64_t heat_offset;
	std::uint64_t query_offset;
	std::uint64_t counter_offset;
	std::uint64_t timestamp_offset;
	std::uint64_t readback_size;
};

GpuPhysicsBackend::Impl::Impl(int width_value, int height_value)
	: width(width_value)
	, height(height_value)
	, cell_count(static_cast<std::uint32_t>(width_value * height_value))
	, chunk_width((width_value + PHYSICS_CHUNK_SIZE - 1) / PHYSICS_CHUNK_SIZE)
	, chunk_height((height_value + PHYSICS_CHUNK_SIZE - 1) / PHYSICS_CHUNK_SIZE)
	, chunk_count(chunk_width * chunk_height)
	, state_size(
		  static_cast<std::uint64_t>(cell_count) * sizeof(PackedCellState)
	  )
	, rgba_size(static_cast<std::uint64_t>(cell_count) * 4)
	, heat_size(rgba_size)
	, query_size(
		  static_cast<std::uint64_t>(PHYSICS_MAX_QUERY_CELLS)
		  * sizeof(PackedCellState)
	  )
	, heat_offset(alignedSize(rgba_size))
	, query_offset(heat_offset + alignedSize(heat_size))
	, counter_offset(query_offset + alignedSize(query_size))
	, timestamp_offset(counter_offset + alignedSize(counter_size))
	, readback_size(
		  timestamp_offset
		  + alignedSize(TIMESTAMP_COUNT * sizeof(std::uint64_t))
	  ) {
	if (width <= 0 || height <= 0 || cell_count >= (1U << 20U)) {
		throw std::invalid_argument(
			"GPU physics dimensions must contain fewer than 2^20 cells"
		);
	}
	try {
		_createContext();
		_createResources();
		_createPipelines();
		_createBindGroups();
	} catch (...) {
		_releaseResources();
		throw;
	}
}

GpuPhysicsBackend::Impl::~Impl() noexcept {
	if (instance != nullptr) {
		while (!lost
		       && std::ranges::any_of(readback_slots, &ReadbackSlot::busy)) {
			wgpuInstanceProcessEvents(instance);
		}
	}
	_releaseResources();
}

void GpuPhysicsBackend::Impl::_createContext() {
	WGPUInstanceExtras instance_extras{};
	instance_extras.chain.sType = static_cast<WGPUSType>(
		WGPUSType_InstanceExtras
	);
	instance_extras.backends = platformInstanceBackend();
	WGPUInstanceDescriptor instance_descriptor{
		.nextInChain = &instance_extras.chain,
	};
	instance = wgpuCreateInstance(&instance_descriptor);
	if (instance == nullptr) {
		throw std::runtime_error("Failed to create WebGPU instance");
	}

	struct AdapterResult {
		bool completed = false;
		bool success = false;
		WGPUAdapter adapter = nullptr;
		std::string message;
	} adapter_result;
	WGPURequestAdapterOptions options{
		.featureLevel = WGPUFeatureLevel_Core,
		.powerPreference = WGPUPowerPreference_HighPerformance,
		.backendType = platformBackendType(),
	};
	WGPURequestAdapterCallbackInfo callback{
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback =
			[](WGPURequestAdapterStatus status, WGPUAdapter found,
	           WGPUStringView message, void *userdata, void *) {
				auto &result = *static_cast<AdapterResult *>(userdata);
				result.completed = true;
				result.success = status == WGPURequestAdapterStatus_Success;
				result.adapter = found;
				result.message = stringFromView(message);
			},
		.userdata1 = &adapter_result,
	};
	wgpuInstanceRequestAdapter(instance, &options, callback);
	waitForResult(instance, adapter_result, "Request GPU physics adapter");
	adapter = adapter_result.adapter;
	timestamps_supported = wgpuAdapterHasFeature(
		adapter, WGPUFeatureName_TimestampQuery
	);

	WGPUAdapterInfo info{};
	wgpuAdapterGetInfo(adapter, &info);
	adapter_diagnostics = {
		.name = stringFromView(info.device),
		.vendor = stringFromView(info.vendor),
		.architecture = stringFromView(info.architecture),
		.backend = backendName(info.backendType),
		.driver = stringFromView(info.description),
	};
	wgpuAdapterInfoFreeMembers(info);

	struct DeviceResult {
		bool completed = false;
		bool success = false;
		WGPUDevice device = nullptr;
		std::string message;
	} device_result;
	const WGPUFeatureName timestamp_feature = WGPUFeatureName_TimestampQuery;
	WGPUDeviceDescriptor descriptor{
		.label = stringView("Waveforge authoritative physics device"),
		.requiredFeatureCount = timestamps_supported ? 1U : 0U,
		.requiredFeatures = timestamps_supported ? &timestamp_feature : nullptr,
		.deviceLostCallbackInfo = {
			.mode = WGPUCallbackMode_AllowProcessEvents,
			.callback = []( const WGPUDevice  *, WGPUDeviceLostReason,
			               WGPUStringView message, void *userdata, void *) {
				auto &self = *static_cast<Impl *>(userdata);
				self.lost = true;
				self.error = "GPU physics device lost: " + stringFromView(message);
			},
			.userdata1 = this,
		},
		.uncapturedErrorCallbackInfo = {
			.callback = []( const WGPUDevice  *, WGPUErrorType,
			               WGPUStringView message, void *userdata, void *) {
				auto &self = *static_cast<Impl *>(userdata);
				self.error = "GPU physics validation error: "
					+ stringFromView(message);
			},
			.userdata1 = this,
		},
	};
	WGPURequestDeviceCallbackInfo device_callback{
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback =
			[](WGPURequestDeviceStatus status, WGPUDevice found,
	           WGPUStringView message, void *userdata, void *) {
				auto &result = *static_cast<DeviceResult *>(userdata);
				result.completed = true;
				result.success = status == WGPURequestDeviceStatus_Success;
				result.device = found;
				result.message = stringFromView(message);
			},
		.userdata1 = &device_result,
	};
	wgpuAdapterRequestDevice(adapter, &descriptor, device_callback);
	waitForResult(instance, device_result, "Request GPU physics device");
	device = device_result.device;
	queue = wgpuDeviceGetQueue(device);
	if (timestamps_supported) {
		timestamp_period = wgpuQueueGetTimestampPeriod(queue);
	}
}

void GpuPhysicsBackend::Impl::_createResources() {
	state_a = createBuffer(
		device, state_size,
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst
			| WGPUBufferUsage_CopySrc,
		"Physics state A"
	);
	state_b = createBuffer(
		device, state_size,
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst
			| WGPUBufferUsage_CopySrc,
		"Physics state B"
	);
	scratch = createBuffer(
		device, static_cast<std::uint64_t>(cell_count) * 32,
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "Physics scratch"
	);
	commands = createBuffer(
		device,
		static_cast<std::uint64_t>(MAX_EDIT_COMMANDS)
			* sizeof(WorldEditCommand),
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "World edit commands"
	);
	query_requests = createBuffer(
		device,
		static_cast<std::uint64_t>(MAX_QUERY_REQUESTS)
			* sizeof(GpuQueryRequest),
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
		"World query requests"
	);
	materials = createBuffer(
		device,
		static_cast<std::uint64_t>(std::to_underlying(PixelType::_count))
			* sizeof(MaterialProperties),
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "Material properties"
	);
	chunks = createBuffer(
		device, static_cast<std::uint64_t>(chunk_count) * 4,
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "Chunk activity"
	);
	rgba = createBuffer(
		device, rgba_size, WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc,
		"RGBA output"
	);
	heat_rgba = createBuffer(
		device, heat_size, WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc,
		"Heat RGBA output"
	);
	query_cells = createBuffer(
		device, query_size, WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc,
		"Compacted query output"
	);
	counters = createBuffer(
		device, counter_size,
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc
			| WGPUBufferUsage_CopyDst,
		"Physics counters"
	);
	palette = createBuffer(
		device, 768 * sizeof(std::uint32_t),
		WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "Rendering palette"
	);
	params = createBuffer(
		device, sizeof(GpuParams),
		WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, "Physics parameters"
	);
	if (timestamps_supported) {
		timestamp_values = createBuffer(
			device, TIMESTAMP_COUNT * sizeof(std::uint64_t),
			WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
			"Physics timestamp values"
		);
		WGPUQuerySetDescriptor query_descriptor{
			.label = stringView("Physics pass timestamps"),
			.type = WGPUQueryType_Timestamp,
			.count = TIMESTAMP_COUNT,
		};
		timestamp_queries = wgpuDeviceCreateQuerySet(device, &query_descriptor);
		if (timestamp_queries == nullptr) {
			throw std::runtime_error(
				"Failed to create physics timestamp queries"
			);
		}
	}

	for (auto &slot : readback_slots) {
		slot.owner = this;
		slot.buffer = createBuffer(
			device, readback_size,
			WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
			"Physics readback ring"
		);
		slot.pixels.resize(rgba_size);
		slot.heat_pixels.resize(heat_size);
	}

	std::array<MaterialProperties, std::to_underlying(PixelType::_count)>
		table{};
	auto set_material =
		[&](PixelType type, PixelClass pixel_class, std::uint32_t conductivity,
	        std::uint32_t density_value, std::uint32_t color) {
		table[std::to_underlying(type)] = {
			.classes = std::to_underlying(pixel_class),
			.conductivity_density = conductivity | (density_value << 8U),
			.colors = color,
		};
	};
	set_material(
		PixelType::Smoke, PixelClass::Gas, 5, 0, colorIndexOf("Smoke1")
	);
	set_material(
		PixelType::Steam, PixelClass::Gas, 2, 1, colorIndexOf("Steam1")
	);
	set_material(PixelType::Air, PixelClass::Gas, 5, 2, colorIndexOf("Air"));
	set_material(
		PixelType::FluidParticle, PixelClass::Particle, 20, 3,
		colorIndexOf("Water")
	);
	set_material(PixelType::Oil, PixelClass::Fluid, 28, 4, colorIndexOf("Oil"));
	set_material(
		PixelType::Water, PixelClass::Fluid, 24, 5, colorIndexOf("Water")
	);
	set_material(
		PixelType::Decoration, PixelClass::Solid, 25, 6, colorIndexOf("Ruin")
	);
	set_material(
		PixelType::Stone, PixelClass::Solid, 10, 7, colorIndexOf("Stone1")
	);
	set_material(
		PixelType::Wood, PixelClass::Solid, 20, 8, colorIndexOf("Wood1")
	);
	set_material(
		PixelType::Copper, PixelClass::Solid, 60, 9, colorIndexOf("Copper1")
	);
	set_material(
		PixelType::Sand, PixelClass::Solid, 16, 10, colorIndexOf("Sand1")
	);
	wgpuQueueWriteBuffer(queue, materials, 0, table.data(), sizeof(table));

	std::array<std::uint32_t, 768> colors{};
	for (std::size_t index = 0; index < 256; ++index) {
		if (index < _color_palette_size) {
			colors[index] = packedRgba(colorOfIndex(index));
			colors[256 + index] = packedRgba(
				colorPaletteOfIndex(index).active_color
			);
			colors[512 + index] = packedRgba(laserBlendedColorOfIndex(index));
		}
	}
	wgpuQueueWriteBuffer(queue, palette, 0, colors.data(), sizeof(colors));
}

void GpuPhysicsBackend::Impl::_createPipelines() {
	WGPUShaderSourceWGSL source{
		.chain = {.sType = WGPUSType_ShaderSourceWGSL},
		.code = stringView(PHYSICS_SHADER),
	};
	WGPUShaderModuleDescriptor descriptor{
		.nextInChain = &source.chain,
		.label = stringView("Waveforge pixel physics shader"),
	};
	shader = wgpuDeviceCreateShaderModule(device, &descriptor);
	if (shader == nullptr) {
		throw std::runtime_error("Failed to create GPU physics shader");
	}
	wgpuInstanceProcessEvents(instance);
	if (!error.empty()) {
		throw std::runtime_error(std::exchange(error, {}));
	}
	constexpr const char *ENTRY_POINTS[] = {
		"apply_commands",    "update_chunks",     "thermal_propose",
		"thermal_gather",    "transitions",       "movement_propose",
		"pressure_propose",  "movement_apply",    "electricity_propose",
		"electricity_apply", "output_pixels",     "output_queries",
		"count_chunks",      "transition_spawns", "clear_movement_scratch",
		"pressure_lift_0",   "pressure_lift_1",
	};
	for (const char *entry_point : ENTRY_POINTS) {
		WGPUComputePipelineDescriptor pipeline_descriptor{
			.label = stringView(entry_point),
			.compute = {
				.module = shader, .entryPoint = stringView(entry_point)
			},
		};
		auto pipeline = wgpuDeviceCreateComputePipeline(
			device, &pipeline_descriptor
		);
		wgpuInstanceProcessEvents(instance);
		if (!error.empty()) {
			throw std::runtime_error(std::exchange(error, {}));
		}
		if (pipeline == nullptr) {
			throw std::runtime_error(
				std::format("Failed to create {} pipeline", entry_point)
			);
		}
		pipelines.push_back(pipeline);
	}
}

WGPUBindGroup GpuPhysicsBackend::Impl::_makeBindGroup(
	WGPUComputePipeline pipeline, std::span<const WGPUBindGroupEntry> entries,
	const char *label
) {
	WGPUBindGroupLayout layout = wgpuComputePipelineGetBindGroupLayout(
		pipeline, 0
	);
	WGPUBindGroupDescriptor descriptor{
		.label = stringView(label),
		.layout = layout,
		.entryCount = entries.size(),
		.entries = entries.data(),
	};
	WGPUBindGroup group = wgpuDeviceCreateBindGroup(device, &descriptor);
	wgpuBindGroupLayoutRelease(layout);
	if (group == nullptr) {
		throw std::runtime_error(std::format("Failed to create {}", label));
	}
	bind_groups.push_back(group);
	return group;
}

void GpuPhysicsBackend::Impl::_createBindGroups() {
	auto entry = [](std::uint32_t binding, WGPUBuffer buffer,
	                std::uint64_t size) {
		return WGPUBindGroupEntry{
			.binding = binding, .buffer = buffer, .size = size
		};
	};
	const auto scratch_size = static_cast<std::uint64_t>(cell_count) * 32;
	const auto chunk_size = static_cast<std::uint64_t>(chunk_count) * 4;
	const auto command_size = static_cast<std::uint64_t>(MAX_EDIT_COMMANDS)
		* sizeof(WorldEditCommand);
	const auto request_size = static_cast<std::uint64_t>(MAX_QUERY_REQUESTS)
		* sizeof(GpuQueryRequest);
	const auto material_size = static_cast<std::uint64_t>(
								   std::to_underlying(PixelType::_count)
							   )
		* sizeof(MaterialProperties);

	std::array apply_entries{
		entry(0, state_a, state_size), entry(3, commands, command_size),
		entry(5, materials, material_size), entry(12, params, sizeof(GpuParams))
	};
	apply_group = _makeBindGroup(
		pipelines[0], apply_entries, "Apply commands bind group"
	);
	std::array chunks_entries{
		entry(0, state_a, state_size), entry(6, chunks, chunk_size),
		entry(12, params, sizeof(GpuParams))
	};
	chunks_group = _makeBindGroup(
		pipelines[1], chunks_entries, "Chunk activity bind group"
	);
	std::array propose_entries{
		entry(0, state_a, state_size), entry(2, scratch, scratch_size),
		entry(5, materials, material_size), entry(6, chunks, chunk_size),
		entry(12, params, sizeof(GpuParams))
	};
	thermal_propose_group = _makeBindGroup(
		pipelines[2], propose_entries, "Thermal proposal bind group"
	);
	std::array gather_entries{
		entry(0, state_a, state_size), entry(1, state_b, state_size),
		entry(2, scratch, scratch_size), entry(6, chunks, chunk_size),
		entry(12, params, sizeof(GpuParams))
	};
	thermal_gather_group = _makeBindGroup(
		pipelines[3], gather_entries, "Thermal gather bind group"
	);
	std::array transition_entries{
		entry(0, state_b, state_size),
		entry(2, scratch, scratch_size),
		entry(5, materials, material_size),
		entry(6, chunks, chunk_size),
		entry(10, counters, counter_size),
		entry(12, params, sizeof(GpuParams))
	};
	transitions_group = _makeBindGroup(
		pipelines[4], transition_entries, "Transition bind group"
	);
	transition_spawns_group = _makeBindGroup(
		pipelines[13], transition_entries, "Transition spawn bind group"
	);
	std::array movement_propose_b_entries{
		entry(0, state_b, state_size), entry(2, scratch, scratch_size),
		entry(5, materials, material_size), entry(6, chunks, chunk_size),
		entry(12, params, sizeof(GpuParams))
	};
	movement_propose_b_group = _makeBindGroup(
		pipelines[5], movement_propose_b_entries,
		"Movement proposal B bind group"
	);
	std::array pressure_propose_a_entries{
		entry(0, state_a, state_size), entry(2, scratch, scratch_size),
		entry(5, materials, material_size), entry(6, chunks, chunk_size),
		entry(12, params, sizeof(GpuParams))
	};
	pressure_propose_a_group = _makeBindGroup(
		pipelines[6], pressure_propose_a_entries,
		"Pressure proposal A bind group"
	);
	std::array pressure_propose_b_entries{
		entry(0, state_b, state_size), entry(2, scratch, scratch_size),
		entry(5, materials, material_size), entry(6, chunks, chunk_size),
		entry(12, params, sizeof(GpuParams))
	};
	pressure_propose_b_group = _makeBindGroup(
		pipelines[6], pressure_propose_b_entries,
		"Pressure proposal B bind group"
	);
	std::array movement_apply_ba_entries{
		entry(0, state_b, state_size),
		entry(1, state_a, state_size),
		entry(2, scratch, scratch_size),
		entry(6, chunks, chunk_size),
		entry(10, counters, counter_size),
		entry(12, params, sizeof(GpuParams))
	};
	movement_apply_ba_group = _makeBindGroup(
		pipelines[7], movement_apply_ba_entries,
		"Movement apply B to A bind group"
	);
	std::array movement_apply_ab_entries{
		entry(0, state_a, state_size),
		entry(1, state_b, state_size),
		entry(2, scratch, scratch_size),
		entry(6, chunks, chunk_size),
		entry(10, counters, counter_size),
		entry(12, params, sizeof(GpuParams))
	};
	movement_apply_ab_group = _makeBindGroup(
		pipelines[7], movement_apply_ab_entries,
		"Movement apply A to B bind group"
	);
	for (std::uint32_t phase = 0; phase < pressure_lift_groups.size();
	     ++phase) {
		const bool reads_a = phase % 2 == 0;
		std::array lift_entries{
			entry(0, reads_a ? state_a : state_b, state_size),
			entry(1, reads_a ? state_b : state_a, state_size),
			entry(6, chunks, chunk_size),
			entry(12, params, sizeof(GpuParams)),
		};
		pressure_lift_groups[phase] = _makeBindGroup(
			pipelines[15 + phase], lift_entries, "Pressure lift bind group"
		);
	}
	std::array electricity_entries{
		entry(0, state_a, state_size), entry(2, scratch, scratch_size),
		entry(12, params, sizeof(GpuParams))
	};
	electricity_propose_group = _makeBindGroup(
		pipelines[8], electricity_entries, "Electricity proposal bind group"
	);
	electricity_apply_group = _makeBindGroup(
		pipelines[9], electricity_entries, "Electricity apply bind group"
	);
	std::array output_pixel_entries{
		entry(0, state_a, state_size),
		entry(7, rgba, rgba_size),
		entry(8, heat_rgba, heat_size),
		entry(10, counters, counter_size),
		entry(11, palette, 768 * sizeof(std::uint32_t)),
		entry(12, params, sizeof(GpuParams))
	};
	output_pixels_group = _makeBindGroup(
		pipelines[10], output_pixel_entries, "Pixel output bind group"
	);
	std::array output_query_entries{
		entry(0, state_a, state_size), entry(4, query_requests, request_size),
		entry(9, query_cells, query_size), entry(12, params, sizeof(GpuParams))
	};
	output_queries_group = _makeBindGroup(
		pipelines[11], output_query_entries, "Query output bind group"
	);
	std::array count_chunk_entries{
		entry(6, chunks, chunk_size), entry(10, counters, counter_size),
		entry(12, params, sizeof(GpuParams))
	};
	count_chunks_group = _makeBindGroup(
		pipelines[12], count_chunk_entries, "Chunk counter bind group"
	);
	std::array clear_scratch_entries{
		entry(2, scratch, scratch_size), entry(12, params, sizeof(GpuParams))
	};
	clear_scratch_group = _makeBindGroup(
		pipelines[14], clear_scratch_entries, "Scratch reset bind group"
	);
}

void GpuPhysicsBackend::Impl::uploadLevel(
	std::span<const PackedCellState> state
) {
	if (state.size() != cell_count) {
		throw std::invalid_argument(
			"Level state size does not match GPU world"
		);
	}
	std::vector prepared_state(state.begin(), state.end());
	for (std::uint32_t index = 0; index < prepared_state.size(); ++index) {
		auto &cell = prepared_state[index];
		if (cell.type() == PixelType::Oil) {
			cell.setBurnLifetime(
				static_cast<std::uint8_t>(
					36
					+ std::popcount(
						physicsRandomHash(0, index, 9, 0) & 0xffffffU
					)
				)
			);
		} else if (cell.type() == PixelType::Wood) {
			cell.setBurnLifetime(
				static_cast<std::uint8_t>(
					72
					+ std::popcount(
						physicsRandomHash(0, index, 9, 0) & 0xffffffU
					)
					+ std::popcount(
						physicsRandomHash(0, index, 9, 1) & 0xffffffU
					)
				)
			);
		}
	}
	wgpuQueueWriteBuffer(queue, state_a, 0, prepared_state.data(), state_size);
	wgpuQueueWriteBuffer(queue, state_b, 0, prepared_state.data(), state_size);
	uploaded = true;
}

void GpuPhysicsBackend::Impl::submit(
	WorldEditBatch edits_value, std::vector<WorldQueryRequest> queries_value
) {
	if (edits_value.commands().size() > MAX_EDIT_COMMANDS) {
		throw std::length_error("Too many world edit commands in one tick");
	}
	if (queries_value.size() > MAX_QUERY_REQUESTS) {
		throw std::length_error("Too many world query requests in one tick");
	}
	pending_edits = std::move(edits_value);
	pending_queries = std::move(queries_value);
}

void GpuPhysicsBackend::Impl::_encodePass(
	WGPUComputePassEncoder pass, WGPUComputePipeline pipeline,
	WGPUBindGroup bind_group, std::uint32_t workgroups
) const noexcept {
	wgpuComputePassEncoderSetPipeline(pass, pipeline);
	wgpuComputePassEncoderSetBindGroup(pass, 0, bind_group, 0, nullptr);
	wgpuComputePassEncoderDispatchWorkgroups(pass, workgroups, 1, 1);
}

GpuPhysicsBackend::Impl::ReadbackSlot *
GpuPhysicsBackend::Impl::_freeReadbackSlot() noexcept {
	for (std::size_t count = 0; count < readback_slots.size(); ++count) {
		auto index = (next_readback_slot + count) % readback_slots.size();
		if (!readback_slots[index].busy) {
			next_readback_slot = (index + 1) % readback_slots.size();
			return &readback_slots[index];
		}
	}
	return nullptr;
}

void GpuPhysicsBackend::Impl::_scheduleReadback(
	WGPUCommandEncoder encoder, ReadbackSlot &slot
) {
	wgpuCommandEncoderCopyBufferToBuffer(
		encoder, rgba, 0, slot.buffer, rgba_offset, rgba_size
	);
	slot.has_heat = std::ranges::any_of(
		pending_queries, [](const WorldQueryRequest &request) {
		return request.kind == WorldQueryKind::DebugRegion;
	}
	);
	if (slot.has_heat) {
		wgpuCommandEncoderCopyBufferToBuffer(
			encoder, heat_rgba, 0, slot.buffer, heat_offset, heat_size
		);
	}
	std::uint64_t query_copy_size = 0;
	for (const auto &request : pending_queries) {
		const auto clipped = clipWorldQuery(request, width, height);
		query_copy_size += static_cast<std::uint64_t>(clipped.width)
			* clipped.height * sizeof(PackedCellState);
	}
	if (query_copy_size > 0) {
		wgpuCommandEncoderCopyBufferToBuffer(
			encoder, query_cells, 0, slot.buffer, query_offset, query_copy_size
		);
	}
	wgpuCommandEncoderCopyBufferToBuffer(
		encoder, counters, 0, slot.buffer, counter_offset, counter_size
	);
	if (timestamps_supported) {
		wgpuCommandEncoderCopyBufferToBuffer(
			encoder, timestamp_values, 0, slot.buffer, timestamp_offset,
			TIMESTAMP_COUNT * sizeof(std::uint64_t)
		);
	}
	slot.busy = true;
	slot.ready = false;
	slot.tick = tick;
	slot.requests = pending_queries;
	slot.command_bytes = static_cast<std::uint32_t>(pending_edits.byteSize());
	slot.query_bytes = 0;
	for (const auto &request : pending_queries) {
		const auto clipped = clipWorldQuery(request, width, height);
		slot.query_bytes += clipped.width * clipped.height
			* sizeof(PackedCellState);
	}
	slot.submitted_at = std::chrono::steady_clock::now();
}

void GpuPhysicsBackend::Impl::step() {
	if (!uploaded) {
		throw std::logic_error(
			"A level must be uploaded before GPU physics stepping"
		);
	}
	if (lost) {
		throw std::runtime_error(error);
	}
	poll();
	ReadbackSlot *slot = _freeReadbackSlot();
	while (slot == nullptr) {
		poll();
		if (lost) {
			throw std::runtime_error(error);
		}
		slot = _freeReadbackSlot();
	}

	std::vector<GpuQueryRequest> gpu_queries;
	gpu_queries.reserve(pending_queries.size());
	std::uint32_t query_cell_count = 0;
	for (auto &request : pending_queries) {
		request = clipWorldQuery(request, width, height);
		const std::uint32_t count = request.width * request.height;
		if (query_cell_count + count > PHYSICS_MAX_QUERY_CELLS) {
			throw std::length_error(
				"World queries exceed compact readback capacity"
			);
		}
		gpu_queries.push_back({
			.kind = std::to_underlying(request.kind),
			.id = request.id,
			.x = request.x,
			.y = request.y,
			.width = request.width,
			.height = request.height,
			.cell_offset = query_cell_count,
			.cell_count = count,
		});
		query_cell_count += count;
	}
	if (!pending_edits.empty()) {
		wgpuQueueWriteBuffer(
			queue, commands, 0, pending_edits.commands().data(),
			pending_edits.byteSize()
		);
	}
	if (!gpu_queries.empty()) {
		wgpuQueueWriteBuffer(
			queue, query_requests, 0, gpu_queries.data(),
			gpu_queries.size() * sizeof(GpuQueryRequest)
		);
	}
	const auto air = PackedCellState::fromTags(
		PixelTag{
			.type = PixelType::Air,
			.pclass = PixelClass::Gas,
			.color_index = colorIndexOf("Air")
		}
	);
	GpuParams gpu_params{
		.width = static_cast<std::uint32_t>(width),
		.height = static_cast<std::uint32_t>(height),
		.tick = static_cast<std::uint32_t>(tick),
		.command_count = static_cast<std::uint32_t>(
			pending_edits.commands().size()
		),
		.query_count = static_cast<std::uint32_t>(gpu_queries.size()),
		.query_cell_count = query_cell_count,
		.chunk_width = chunk_width,
		.chunk_count = chunk_count,
		.pressure_direction = static_cast<std::uint32_t>(tick & 1U),
		.air_metadata = air.metadata,
		.fire_colors = colorIndexOf("Fire1") | (colorIndexOf("Fire2") << 8U)
			| (colorIndexOf("Fire3") << 16U),
		.laser_stroke_color = colorIndexOf("LaserStroke"),
	};
	wgpuQueueWriteBuffer(queue, params, 0, &gpu_params, sizeof(gpu_params));

	WGPUCommandEncoderDescriptor encoder_descriptor{
		.label = stringView("Pixel physics tick")
	};
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
		device, &encoder_descriptor
	);
	wgpuCommandEncoderClearBuffer(
		encoder, chunks, 0, static_cast<std::uint64_t>(chunk_count) * 4
	);
	wgpuCommandEncoderClearBuffer(
		encoder, scratch, 0, static_cast<std::uint64_t>(cell_count) * 32
	);
	wgpuCommandEncoderClearBuffer(encoder, counters, 0, counter_size);
	const std::uint32_t cell_workgroups = (cell_count + WORKGROUP_SIZE - 1)
		/ WORKGROUP_SIZE;
	const std::uint32_t query_workgroups = (query_cell_count + WORKGROUP_SIZE
	                                        - 1)
		/ WORKGROUP_SIZE;
	const std::uint32_t chunk_workgroups = (chunk_count + WORKGROUP_SIZE - 1)
		/ WORKGROUP_SIZE;
	auto run_stage = [&](std::uint32_t stage, const char *label, auto encode) {
		WGPUComputePassTimestampWrites timestamp_writes{
			.querySet = timestamp_queries,
			.beginningOfPassWriteIndex = stage * 2,
			.endOfPassWriteIndex = stage * 2 + 1,
		};
		WGPUComputePassDescriptor descriptor{
			.label = stringView(label),
			.timestampWrites = timestamps_supported ? &timestamp_writes
													: nullptr,
		};
		WGPUComputePassEncoder stage_pass = wgpuCommandEncoderBeginComputePass(
			encoder, &descriptor
		);
		encode(stage_pass);
		wgpuComputePassEncoderEnd(stage_pass);
		wgpuComputePassEncoderRelease(stage_pass);
	};
	run_stage(0, "Apply physics commands", [&](WGPUComputePassEncoder pass) {
		_encodePass(pass, pipelines[0], apply_group, cell_workgroups);
	});
	run_stage(1, "Update active chunks", [&](WGPUComputePassEncoder pass) {
		_encodePass(pass, pipelines[1], chunks_group, cell_workgroups);
	});
	run_stage(2, "Thermal simulation", [&](WGPUComputePassEncoder pass) {
		_encodePass(pass, pipelines[2], thermal_propose_group, cell_workgroups);
		_encodePass(pass, pipelines[3], thermal_gather_group, cell_workgroups);
	});
	run_stage(3, "Material transitions", [&](WGPUComputePassEncoder pass) {
		_encodePass(pass, pipelines[4], transitions_group, cell_workgroups);
		_encodePass(
			pass, pipelines[13], transition_spawns_group, cell_workgroups
		);
	});
	run_stage(4, "Movement resolution", [&](WGPUComputePassEncoder pass) {
		_encodePass(
			pass, pipelines[5], movement_propose_b_group, cell_workgroups
		);
		_encodePass(
			pass, pipelines[7], movement_apply_ba_group, cell_workgroups
		);
	});
	run_stage(5, "Liquid pressure", [&](WGPUComputePassEncoder pass) {
		for (std::uint32_t iteration = 0; iteration < PRESSURE_ITERATIONS;
		     ++iteration) {
			_encodePass(
				pass, pipelines[14], clear_scratch_group, cell_workgroups
			);
			WGPUBindGroup propose_group = iteration % 2 == 0
				? pressure_propose_a_group
				: pressure_propose_b_group;
			WGPUBindGroup apply_group = iteration % 2 == 0
				? movement_apply_ab_group
				: movement_apply_ba_group;
			_encodePass(pass, pipelines[6], propose_group, cell_workgroups);
			_encodePass(pass, pipelines[7], apply_group, cell_workgroups);
		}
		for (std::uint32_t phase = 0; phase < pressure_lift_groups.size();
		     ++phase) {
			_encodePass(
				pass, pipelines[15 + phase], pressure_lift_groups[phase],
				cell_workgroups
			);
		}
	});
	wgpuCommandEncoderClearBuffer(
		encoder, scratch, 0, static_cast<std::uint64_t>(cell_count) * 32
	);
	run_stage(6, "Electricity", [&](WGPUComputePassEncoder pass) {
		_encodePass(
			pass, pipelines[8], electricity_propose_group, cell_workgroups
		);
		_encodePass(
			pass, pipelines[9], electricity_apply_group, cell_workgroups
		);
	});
	run_stage(7, "Physics output", [&](WGPUComputePassEncoder pass) {
		_encodePass(pass, pipelines[10], output_pixels_group, cell_workgroups);
		if (query_workgroups > 0) {
			_encodePass(
				pass, pipelines[11], output_queries_group, query_workgroups
			);
		}
		_encodePass(pass, pipelines[12], count_chunks_group, chunk_workgroups);
	});
	if (timestamps_supported) {
		wgpuCommandEncoderResolveQuerySet(
			encoder, timestamp_queries, 0, TIMESTAMP_COUNT, timestamp_values, 0
		);
	}

	_scheduleReadback(encoder, *slot);
	WGPUCommandBufferDescriptor command_descriptor{
		.label = stringView("Pixel physics tick commands")
	};
	WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
		encoder, &command_descriptor
	);
	wgpuQueueSubmit(queue, 1, &command_buffer);

	WGPUBufferMapCallbackInfo map_callback{
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback =
			[](WGPUMapAsyncStatus status, WGPUStringView message,
	           void *userdata, void *) {
				auto &readback = *static_cast<ReadbackSlot *>(userdata);
				if (status != WGPUMapAsyncStatus_Success) {
					readback.owner->error = "GPU physics readback failed: "
						+ stringFromView(message);
					readback.busy = false;
					return;
				}
				const void *mapped = wgpuBufferGetConstMappedRange(
					readback.buffer, 0, readback.owner->readback_size
				);
				readback.owner->_finishReadback(readback, mapped);
				wgpuBufferUnmap(readback.buffer);
			},
		.userdata1 = slot,
	};
	wgpuBufferMapAsync(
		slot->buffer, WGPUMapMode_Read, 0, readback_size, map_callback
	);
	wgpuCommandBufferRelease(command_buffer);
	wgpuCommandEncoderRelease(encoder);
	pending_edits.clear();
	pending_queries.clear();
	++tick;
}

void GpuPhysicsBackend::Impl::_finishReadback(
	ReadbackSlot &slot, const void *mapped
) {
	const auto *bytes = static_cast<const std::uint8_t *>(mapped);
	std::memcpy(slot.pixels.data(), bytes + rgba_offset, rgba_size);
	if (slot.has_heat) {
		slot.heat_pixels.resize(heat_size);
		std::memcpy(slot.heat_pixels.data(), bytes + heat_offset, heat_size);
	} else {
		slot.heat_pixels.clear();
	}
	slot.snapshot = {};
	slot.snapshot._tick = slot.tick;
	std::uint32_t cell_offset = 0;
	for (auto request : slot.requests) {
		request = clipWorldQuery(request, width, height);
		const std::uint32_t count = request.width * request.height;
		slot.snapshot._results.push_back({
			.id = request.id,
			.x = request.x,
			.y = request.y,
			.width = request.width,
			.height = request.height,
			.cell_offset = cell_offset,
			.cell_count = count,
		});
		cell_offset += count;
	}
	slot.snapshot._cells.resize(cell_offset);
	std::memcpy(
		slot.snapshot._cells.data(), bytes + query_offset,
		static_cast<std::size_t>(cell_offset) * sizeof(PackedCellState)
	);
	std::array<std::uint32_t, 4> values{};
	std::memcpy(values.data(), bytes + counter_offset, sizeof(values));
	const double completion_ms = std::chrono::duration<double, std::milli>(
									 std::chrono::steady_clock::now()
									 - slot.submitted_at
	)
									 .count();
	if (timestamps_supported) {
		std::array<std::uint64_t, TIMESTAMP_COUNT> timestamps{};
		std::memcpy(
			timestamps.data(), bytes + timestamp_offset, sizeof(timestamps)
		);
		auto duration = [&](std::size_t stage) {
			return static_cast<double>(
					   timestamps[stage * 2 + 1] - timestamps[stage * 2]
				   )
				* timestamp_period / 1'000'000.0;
		};
		slot.timings.apply_commands_ms = duration(0);
		slot.timings.chunk_activity_ms = duration(1);
		slot.timings.thermal_ms = duration(2);
		slot.timings.transitions_ms = duration(3);
		slot.timings.movement_ms = duration(4);
		slot.timings.pressure_ms = duration(5);
		slot.timings.electricity_ms = duration(6);
		slot.timings.output_ms = duration(7);
		slot.timings.total_ms = slot.timings.apply_commands_ms
			+ slot.timings.chunk_activity_ms + slot.timings.thermal_ms
			+ slot.timings.transitions_ms + slot.timings.movement_ms
			+ slot.timings.pressure_ms + slot.timings.electricity_ms
			+ slot.timings.output_ms;
	} else {
		slot.timings.total_ms = completion_ms;
	}
	slot.timings.active_chunks = values[0];
	slot.timings.processed_cells = values[1];
	slot.timings.movement_conflicts = values[2];
	slot.timings.transitions = values[3];
	slot.timings.command_bytes = slot.command_bytes;
	slot.timings.query_bytes = slot.query_bytes;
	slot.ready = true;
	slot.busy = false;
	latest_readback_slot = static_cast<std::size_t>(
		&slot - readback_slots.data()
	);
}

void GpuPhysicsBackend::Impl::poll() noexcept {
	if (instance != nullptr) {
		wgpuInstanceProcessEvents(instance);
	}
}

PhysicsFrame GpuPhysicsBackend::Impl::latestFrame() const noexcept {
	if (!latest_readback_slot.has_value()) {
		return {};
	}
	const auto &slot = readback_slots[*latest_readback_slot];
	return {
		.tick = slot.tick,
		.rgba = slot.pixels,
		.heat_rgba = slot.heat_pixels,
		.queries = &slot.snapshot,
		.timings = slot.timings,
	};
}

std::vector<PackedCellState> GpuPhysicsBackend::Impl::serialize() {
	if (lost) {
		throw std::runtime_error(error);
	}
	WGPUBuffer readback = createBuffer(
		device, state_size, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
		"Explicit serialization readback"
	);
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
		device, nullptr
	);
	wgpuCommandEncoderCopyBufferToBuffer(
		encoder, state_a, 0, readback, 0, state_size
	);
	WGPUCommandBuffer buffer = wgpuCommandEncoderFinish(encoder, nullptr);
	wgpuQueueSubmit(queue, 1, &buffer);
	struct Result {
		bool completed = false;
		bool success = false;
		std::string message;
	} result;
	WGPUBufferMapCallbackInfo callback{
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback =
			[](WGPUMapAsyncStatus status, WGPUStringView message,
	           void *userdata, void *) {
				auto &value = *static_cast<Result *>(userdata);
				value.completed = true;
				value.success = status == WGPUMapAsyncStatus_Success;
				value.message = stringFromView(message);
			},
		.userdata1 = &result,
	};
	wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, state_size, callback);
	waitForResult(instance, result, "Serialize GPU world");
	std::vector<PackedCellState> state(cell_count);
	std::memcpy(
		state.data(), wgpuBufferGetConstMappedRange(readback, 0, state_size),
		state_size
	);
	wgpuBufferUnmap(readback);
	wgpuBufferRelease(readback);
	wgpuCommandBufferRelease(buffer);
	wgpuCommandEncoderRelease(encoder);
	return state;
}

void GpuPhysicsBackend::Impl::_releaseResources() noexcept {
	for (auto &slot : readback_slots) {
		if (slot.buffer != nullptr) {
			wgpuBufferRelease(slot.buffer);
		}
	}
	for (auto group : bind_groups) {
		wgpuBindGroupRelease(group);
	}
	for (auto pipeline : pipelines) {
		wgpuComputePipelineRelease(pipeline);
	}
	if (timestamp_queries != nullptr) {
		wgpuQuerySetRelease(timestamp_queries);
	}
	if (shader != nullptr) {
		wgpuShaderModuleRelease(shader);
	}
	for (WGPUBuffer buffer :
	     {timestamp_values, params, palette, counters, query_cells, heat_rgba,
	      rgba, chunks, materials, query_requests, commands, scratch, state_b,
	      state_a}) {
		if (buffer != nullptr) {
			wgpuBufferRelease(buffer);
		}
	}
	if (queue != nullptr) {
		wgpuQueueRelease(queue);
	}
	if (device != nullptr) {
		wgpuDeviceRelease(device);
	}
	if (adapter != nullptr) {
		wgpuAdapterRelease(adapter);
	}
	if (instance != nullptr) {
		wgpuInstanceRelease(instance);
	}
}

GpuPhysicsBackend::GpuPhysicsBackend(int width, int height)
	: _impl(std::make_unique<Impl>(width, height)) {}

GpuPhysicsBackend::~GpuPhysicsBackend() noexcept = default;
GpuPhysicsBackend::GpuPhysicsBackend(GpuPhysicsBackend &&) noexcept = default;
GpuPhysicsBackend &GpuPhysicsBackend::operator=(
	GpuPhysicsBackend &&
) noexcept = default;

void GpuPhysicsBackend::uploadLevel(std::span<const PackedCellState> state) {
	_impl->uploadLevel(state);
}
void GpuPhysicsBackend::submit(
	WorldEditBatch edits, std::vector<WorldQueryRequest> queries
) {
	_impl->submit(std::move(edits), std::move(queries));
}
void GpuPhysicsBackend::step() {
	_impl->step();
}
void GpuPhysicsBackend::poll() noexcept {
	_impl->poll();
}
bool GpuPhysicsBackend::frameReady() const noexcept {
	return _impl->latestFrame().queries != nullptr;
}
PhysicsFrame GpuPhysicsBackend::latestFrame() const noexcept {
	return _impl->latestFrame();
}
PhysicsAdapterDiagnostics GpuPhysicsBackend::diagnostics() const {
	return _impl->adapter_diagnostics;
}
bool GpuPhysicsBackend::deviceLost() const noexcept {
	return _impl->lost;
}
std::string_view GpuPhysicsBackend::errorMessage() const noexcept {
	return _impl->error;
}
std::vector<PackedCellState> GpuPhysicsBackend::serialize() {
	return _impl->serialize();
}

} // namespace wf
