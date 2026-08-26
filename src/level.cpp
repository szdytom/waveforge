#include "wforge/level.h"
#include "wforge/colorpalette.h"
#include "wforge/fallsand.h"
#include "wforge/physics_gpu.h"
#include "wforge/save.h"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>

namespace wf {

namespace {

constexpr int ITEM_USE_COOLDOWN_TICKS = 6;

} // namespace

LevelMetadata::Difficulty LevelMetadata::parseDifficulty(
	std::string_view diff_str
) noexcept {
	if (diff_str == "easy") {
		return Difficulty::Easy;
	} else if (diff_str == "average") {
		return Difficulty::Average;
	} else if (diff_str == "hard") {
		return Difficulty::Hard;
	} else {
		return Difficulty::Unkown;
	}
}

std::string_view LevelMetadata::difficultyToString(Difficulty difficulty) {
	switch (difficulty) {
	case Difficulty::Easy:
		return "Easy";
	case Difficulty::Average:
		return "Average";
	case Difficulty::Hard:
		return "Hard";
	default:
		return "???";
	}
}

Level::Level(int width, int height)
	: fallsand(width, height), _item_use_cooldown(0) {}

void Level::step() {
	_item_use_cooldown = std::max(0, _item_use_cooldown - 1);
	fallsand.pollCompletedFrame();
	fallsand.resetEntityPresenceTags();
	duck.commitEntityPresence(fallsand);
	if (_pending_item_use.has_value()
	    && fallsand.consumeQueryResult(_pending_item_use->query_id)) {
		const auto pending = *_pending_item_use;
		_pending_item_use.reset();
		if (pending.item_index >= 0
		    && pending.item_index < static_cast<int>(items.size())
		    && items[pending.item_index].amount > 0
		    && items[pending.item_index].item->use(
				*this, pending.x, pending.y
			)) {
			items[pending.item_index].amount -= 1;
			_item_use_cooldown = ITEM_USE_COOLDOWN_TICKS;
			_normalizeActiveItemIndex();
		}
	}
	const int query_x = static_cast<int>(
		std::floor(std::min(duck.position.x, duck.position.x + duck.velocity.x))
	);
	const int query_y = static_cast<int>(
		std::floor(std::min(duck.position.y, duck.position.y + duck.velocity.y))
	);
	const int query_right = static_cast<int>(std::ceil(
								std::max(
									duck.position.x,
									duck.position.x + duck.velocity.x
								)
							))
		+ duck.width();
	const int query_bottom = static_cast<int>(std::ceil(
								 std::max(
									 duck.position.y,
									 duck.position.y + duck.velocity.y
								 )
							 ))
		+ duck.height();
	constexpr int QUERY_PADDING = 2;
	fallsand.requestQueryRegion(
		WorldQueryKind::DuckRegion, query_x - QUERY_PADDING,
		query_y - QUERY_PADDING, query_right - query_x + QUERY_PADDING * 2,
		query_bottom - query_y + QUERY_PADDING * 2
	);
	if (SaveData::instance().user_settings.debug_heat_render) {
		fallsand.requestQueryRegion(WorldQueryKind::DebugRegion, 0, 0, 1, 1);
	}
	fallsand.step();
	duck.step(*this);
	checkpoint.step(*this);
}

ItemStack *Level::activeItemStack() noexcept {
	_normalizeActiveItemIndex();
	if (_active_item_index == -1) {
		return nullptr;
	}

	return &items[_active_item_index];
}

void Level::useActiveItem(int x, int y) noexcept {
	if (_item_use_cooldown > 0 || _pending_item_use.has_value()) {
		return;
	}

	if (auto itemstack = activeItemStack()) {
#ifdef WAVEFORGE_ENABLE_WEBGPU
		(void)itemstack;
		constexpr int MAX_ITEM_BRUSH_SIZE = 24;
		const auto query_id = fallsand.requestQueryRegion(
			WorldQueryKind::ItemRegion, x - MAX_ITEM_BRUSH_SIZE / 2,
			y - MAX_ITEM_BRUSH_SIZE / 2, MAX_ITEM_BRUSH_SIZE,
			MAX_ITEM_BRUSH_SIZE
		);
		_pending_item_use = {
			.item_index = _active_item_index,
			.x = x,
			.y = y,
			.query_id = query_id,
		};
#else
		if (itemstack->item->use(*this, x, y)) {
			// item used successfully, decrease quantity
			itemstack->amount -= 1;
			_item_use_cooldown = ITEM_USE_COOLDOWN_TICKS;
			_normalizeActiveItemIndex();
		}
#endif
	}
}

void Level::changeActiveItemBrushSize(int delta) noexcept {
	if (auto itemstack = activeItemStack()) {
		itemstack->item->changeBrushSize(delta);
	}
}

void Level::selectItem(int index) noexcept {
	if (index >= 0 && index < static_cast<int>(items.size())
	    && items[index].amount > 0) {
		_active_item_index = index;
	}
}

void Level::prevItem() noexcept {
	int idx = _prevItemId();
	if (idx != -1) {
		_active_item_index = idx;
	}
}

void Level::nextItem() noexcept {
	int idx = _nextItemId();
	if (idx != -1) {
		_active_item_index = idx;
	}
}

int Level::_prevItemId() const noexcept {
	int start_index = _active_item_index == -1
		? items.size()
		: _active_item_index;

	for (int i = start_index - 1; i >= 0; --i) {
		if (items[i].amount > 0) {
			return i;
		}
	}
	return -1;
}

int Level::_nextItemId() const noexcept {
	int start_index = _active_item_index == -1 ? 0 : _active_item_index + 1;

	for (int i = start_index; i < items.size(); ++i) {
		if (items[i].amount > 0) {
			return i;
		}
	}
	return -1;
}

void Level::_normalizeActiveItemIndex() noexcept {
	if (_active_item_index < 0 || _active_item_index >= items.size()) {
		_active_item_index = -1;
	}

	if (_active_item_index != -1 && items[_active_item_index].amount > 0) {
		return;
	}

	int next_id = _nextItemId();
	if (next_id != -1) {
		_active_item_index = next_id;
	} else {
		_active_item_index = _prevItemId();
	}
}

bool Level::isFailed() const noexcept {
	return duck.isOutOfWorld(*this);
}

bool Level::isCompleted() const noexcept {
	return checkpoint.isCompleted();
}

LevelRenderer::LevelRenderer(Level &level)
	: _level(level)
	, _fallsand_buffer(
		  std::make_unique<std::uint8_t[]>(level.width() * level.height() * 4)
	  )
	, _fallsand_texture()
	, _fallsand_sprite(_fallsand_texture)
	, _heat_buffer(
		  std::make_unique<std::uint8_t[]>(level.width() * level.height() * 4)
	  )
	, _heat_texture()
	, _heat_sprite(_heat_texture)
	, _duck_sprite(
		  AssetsManager::instance().getAsset<sf::Texture>("duck/texture")
	  )
	, _font(AssetsManager::instance().getAsset<PixelFont>("font")) {
	if (!_fallsand_texture.resize(
			sf::Vector2u(level.width(), level.height())
		)) {
		throw std::runtime_error("Failed to create fallsand texture");
	}
	_fallsand_texture.setSmooth(false);
	_fallsand_sprite = sf::Sprite(_fallsand_texture);

	if (!_heat_texture.resize(sf::Vector2u(level.width(), level.height()))) {
		throw std::runtime_error("Failed to create heat texture");
	}
	_heat_texture.setSmooth(false);
	_heat_sprite = sf::Sprite(_heat_texture);
}

void LevelRenderer::_renderFallsand(sf::RenderTarget &target) {
	std::span<std::uint8_t> fallsand_buffer_view(
		_fallsand_buffer.get(), _level.width() * _level.height() * 4
	);
	_level.fallsand.renderToBuffer(fallsand_buffer_view);
	_fallsand_texture.update(_fallsand_buffer.get());
	target.draw(_fallsand_sprite);
}

void LevelRenderer::_renderHeat(sf::RenderTarget &target) {
#ifndef NDEBUG
	std::span<std::uint8_t> heat_buffer_view(
		_heat_buffer.get(), _level.width() * _level.height() * 4
	);
	if (_level.fallsand.renderHeatToBuffer(heat_buffer_view)) {
		_heat_texture.update(_heat_buffer.get());
		target.draw(_heat_sprite);
		return;
	}
	// Render heat overlay (semi-transparent red, brighter = hotter)
	const int width = _level.width();
	const int height = _level.height();

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			auto tag = _level.fallsand.tagOf(x, y);
			int idx = (y * width + x) * 4;

			// Heat value is 0-127, map it to red color with alpha
			if (tag.heat > 0) {
				// Normalize heat to 0-255 range
				int heat_normalized = (tag.heat * 256) / PixelTag::heat_max;
				_heat_buffer[idx + 0] = 255; // R
				_heat_buffer[idx + 1] = 0;   // G
				_heat_buffer[idx + 2] = 0;   // B
				_heat_buffer[idx + 3] = heat_normalized;
			} else {
				_heat_buffer[idx + 0] = 0;
				_heat_buffer[idx + 1] = 0;
				_heat_buffer[idx + 2] = 0;
				_heat_buffer[idx + 3] = 0;
			}
		}
	}

	_heat_texture.update(_heat_buffer.get());
	target.draw(_heat_sprite);
#endif
}

void LevelRenderer::_renderDuck(sf::RenderTarget &target, int scale) {
	sf::Vector2f duck_pos(
		std::round(_level.duck.position.x) * scale,
		std::round(_level.duck.position.y) * scale
	);

	_duck_sprite.setPosition(duck_pos);
	_duck_sprite.setScale(sf::Vector2f(scale, scale));
	target.draw(_duck_sprite);
}

void LevelRenderer::render(
	sf::RenderTarget &target, int mouse_x, int mouse_y, int scale
) {
	_fallsand_sprite.setScale(sf::Vector2f(scale, scale));
	_heat_sprite.setScale(sf::Vector2f(scale, scale));
	_renderFallsand(target);

	// Render heat overlay if debug mode is enabled
	if (SaveData::instance().user_settings.debug_heat_render) {
		_renderHeat(target);
	}

	_renderDuck(target, scale);
	_level.checkpoint.render(target, scale); // checkpoint can render itself
	_renderItemText(target, scale);
	if (auto itemstack = _level.activeItemStack()) {
		itemstack->item->render(target, mouse_x, mouse_y, scale);
	}
}

void LevelRenderer::_renderItemText(sf::RenderTarget &target, int scale) {
	constexpr sf::Color active_color = ui_text_color(200);
	constexpr sf::Color inactive_color = ui_text_color(120);
	constexpr int start_x = 2;
	constexpr int start_y = 2;
	constexpr int line_spacing = 1;

	auto active_stack = _level.activeItemStack();
	if (!active_stack) {
		return;
	}

	int y = start_y;
	for (const auto &itemstack : _level.items) {
		if (itemstack.amount <= 0) {
			continue;
		}

		bool is_active = (itemstack.id == active_stack->id);
		auto color = is_active ? active_color : inactive_color;
		auto display_text = std::format(
			"{}{}({})", is_active ? '>' : ' ', itemstack.item->name(),
			itemstack.amount
		);

		_font.renderText(target, display_text, color, start_x, y, scale);
		y += _font.charHeight(1) + line_spacing;
	}
}

} // namespace wf
