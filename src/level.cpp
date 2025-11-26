#include "wforge/level.h"
#include "wforge/colorpalette.h"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>

namespace wf {

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

Level::Level(int width, int height) noexcept
	: fallsand(width, height), _item_use_cooldown(0) {}

void Level::step() {
	_item_use_cooldown = std::max(0, _item_use_cooldown - 1);
	fallsand.resetEntityPresenceTags();
	duck.commitEntityPresence(fallsand);
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

void Level::useActiveItem(int x, int y, int scale) noexcept {
	constexpr int item_use_cooldown_ticks = 6;
	if (_item_use_cooldown > 0) {
		return;
	}

	if (auto itemstack = activeItemStack()) {
		if (itemstack->item->use(*this, x, y, scale)) {
			// item used successfully, decrease quantity
			itemstack->amount -= 1;
			_item_use_cooldown = item_use_cooldown_ticks;
			_normalizeActiveItemIndex();
		}
	}
}

void Level::changeActiveItemBrushSize(int delta) noexcept {
	if (auto itemstack = activeItemStack()) {
		itemstack->item->changeBrushSize(delta);
	}
}

void Level::selectItem(int index) noexcept {
	if (index < 0 || index >= static_cast<int>(items.size())) {
		return;
	} else {
		if (items[index].amount > 0) {
			_active_item_index = index;
		} else {
			return;
		}
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

LevelRenderer::LevelRenderer(Level &level, int scale)
	: _level(level)
	, scale(scale)
	, _fallsand_buffer(
		  std::make_unique<std::uint8_t[]>(level.width() * level.height() * 4)
	  )
	, _fallsand_texture()
	, _fallsand_sprite(_fallsand_texture)
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

	sf::Vector2f scale_vec(scale, scale);
	_fallsand_sprite = sf::Sprite(_fallsand_texture);
	_fallsand_sprite.setScale(scale_vec);
	_duck_sprite.setScale(scale_vec);
}

void LevelRenderer::_renderFallsand(sf::RenderTarget &target) {
	std::span<std::uint8_t> fallsand_buffer_view(
		_fallsand_buffer.get(), _level.width() * _level.height() * 4
	);
	_level.fallsand.renderToBuffer(fallsand_buffer_view);
	_fallsand_texture.update(_fallsand_buffer.get());
	target.draw(_fallsand_sprite);
}

void LevelRenderer::_renderDuck(sf::RenderTarget &target) {
	sf::Vector2f duck_pos(
		std::round(_level.duck.position.x) * scale,
		std::round(_level.duck.position.y) * scale
	);

	_duck_sprite.setPosition(duck_pos);
	target.draw(_duck_sprite);
}

void LevelRenderer::render(sf::RenderTarget &target, int mouse_x, int mouse_y) {
	_renderFallsand(target);
	_renderDuck(target);
	_level.checkpoint.render(target, scale); // checkpoint can render itself
	_renderItemText(target);
	if (auto itemstack = _level.activeItemStack()) {
		itemstack->item->render(target, mouse_x, mouse_y, scale);
	}
}

void LevelRenderer::_renderItemText(sf::RenderTarget &target) {
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
