#ifndef WFORGE_LEVEL_H
#define WFORGE_LEVEL_H

#include "wforge/assets.h"
#include "wforge/fallsand.h"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <memory>
#include <proxy/proxy.h>
#include <proxy/v4/proxy_macros.h>
#include <tuple>
#include <vector>

namespace wf {

class Level;

namespace _dispatch {

// See microsoft/proxy library for the semantics of dispatch conventions
PRO_DEF_MEM_DISPATCH(MemUse, use);
PRO_DEF_MEM_DISPATCH(MemRender, render);
PRO_DEF_MEM_DISPATCH(MemChangeBrushSize, changeBrushSize);
PRO_DEF_MEM_DISPATCH(MemName, name);

} // namespace _dispatch

/* clang-format off */
// See microsoft/proxy library for the semantics of proxy and facade
struct ItemFacade : pro::facade_builder
	::add_convention<_dispatch::MemUse, bool(Level &level, int x, int y) noexcept>
	::add_convention<_dispatch::MemRender, void(sf::RenderTarget &target, int x, int y, int scale) const>
	::add_convention<_dispatch::MemChangeBrushSize, void(int delta) noexcept>
	::add_convention<_dispatch::MemName, std::string_view() const noexcept>
	::build {};
/* clang-format on */

using Item = pro::proxy<ItemFacade>;

struct ItemStack {
	int id;
	int amount;
	Item item;
};

struct DuckEntity {
	DuckEntity(sf::Vector2f pos = {.0f, .0f}) noexcept;

	auto width() const noexcept {
		return shape.width();
	}

	auto height() const noexcept {
		return shape.height();
	}

	PixelShape shape;
	sf::Vector2f position; // anchor at top-left
	sf::Vector2f velocity;

	void setPosition(float x, float y) noexcept;

	bool isOutOfWorld(const Level &level) const noexcept;

	// Pixel-perfect collision detection against fallsand world
	bool willCollideAt(
		const Level &level, int target_x, int target_y
	) const noexcept;

	// basicly willCollideAt at current position
	bool currentlyColliding(const Level &level) const noexcept;

	void step(const Level &level) noexcept;

	void commitEntityPresence(PixelWorld &world) noexcept;
};

struct CheckpointArea {
	// Anchor at top-left
	int x;
	int y;

	CheckpointArea();
	CheckpointArea(int x, int y);

	int width() const noexcept {
		return _width;
	}

	int height() const noexcept {
		return _height;
	}

	void setPosition(int x, int y) noexcept;

	void resetProgress() noexcept;
	int progress() const noexcept;
	int maxProgress() const noexcept;

	void step(const Level &level) noexcept;
	void render(sf::RenderTarget &target, int scale) const noexcept;

	bool isCompleted() const noexcept;

private:
	int _width, _height;
	int _progress;
	CheckpointSprite &_sprite;

	bool _isDuckInside(const Level &level) const noexcept;
};

struct LevelMetadata {
	enum class Difficulty {
		Unkown,
		Easy,
		Average,
		Hard
	};

	int index;
	std::string map_id;
	std::string name;
	std::string description;
	std::string author;
	Difficulty difficulty;
	sf::Texture *minimap_texture;
	std::vector<std::tuple<std::string, int>> items;

	static Difficulty parseDifficulty(std::string_view diff_str) noexcept;
	static std::string_view difficultyToString(Difficulty difficulty);
};

struct Level {
	Level(int width, int height) noexcept;

	static Level loadFromAsset(const std::string &level_id);
	static Level loadFromMetadata(LevelMetadata metadata);

	LevelMetadata metadata;
	PixelWorld fallsand;
	DuckEntity duck; // Quack!
	CheckpointArea checkpoint;

	std::vector<ItemStack> items;

	auto width() const noexcept {
		return fallsand.width();
	}

	auto height() const noexcept {
		return fallsand.height();
	}

	ItemStack *activeItemStack() noexcept;
	void useActiveItem(int x, int y) noexcept;
	void changeActiveItemBrushSize(int delta) noexcept;
	void selectItem(int index) noexcept;
	void prevItem() noexcept;
	void nextItem() noexcept;

	bool isFailed() const noexcept;
	bool isCompleted() const noexcept;

	void step();

private:
	int _prevItemId() const noexcept;
	int _nextItemId() const noexcept;
	void _normalizeActiveItemIndex() noexcept;

	int _active_item_index; // -1 means no active item
	int _item_use_cooldown; // ticks until next item use allowed
};

struct LevelRenderer {
	LevelRenderer(Level &level);

	void render(sf::RenderTarget &target, int mouse_x, int mouse_y, int scale);

private:
	Level &_level;
	std::unique_ptr<std::uint8_t[]> _fallsand_buffer;
	sf::Texture _fallsand_texture;
	sf::Sprite _fallsand_sprite;
	std::unique_ptr<std::uint8_t[]> _heat_buffer;
	sf::Texture _heat_texture;
	sf::Sprite _heat_sprite;
	sf::Sprite _duck_sprite;
	PixelFont &_font;

	void _renderFallsand(sf::RenderTarget &target);
	void _renderHeat(sf::RenderTarget &target);
	void _renderDuck(sf::RenderTarget &target, int scale);
	void _renderItemText(sf::RenderTarget &target, int scale);
};

struct LevelSequence {
	std::vector<LevelMetadata *> levels; // non-owning pointers
};

namespace item {

struct BrushSizeChangableItem {
	BrushSizeChangableItem(int max_brush_size) noexcept;
	BrushSizeChangableItem(int max_brush_size, int initial_brush_size) noexcept;

	void changeBrushSize(int delta) noexcept;
	void render(sf::RenderTarget &target, int x, int y, int scale) const;

protected:
	int brushSize() const noexcept;
	std::array<int, 2> brushTopLeft(int x, int y) const noexcept;

private:
	int _brush_size;
	int _max_brush_size;
};

struct WaterBrush : BrushSizeChangableItem {
	WaterBrush(bool is_large_brush) noexcept;

	bool use(Level &level, int x, int y) noexcept;
	std::string_view name() const noexcept;

	static Item create() noexcept;
	static Item createLarge() noexcept;

private:
	bool _is_large;
};

struct OilBrush : BrushSizeChangableItem {
	OilBrush(bool is_large_brush) noexcept;

	bool use(Level &level, int x, int y) noexcept;
	std::string_view name() const noexcept;

	static Item create() noexcept;
	static Item createLarge() noexcept;

private:
	bool _is_large;
};

struct FireBrush : BrushSizeChangableItem {
	FireBrush() noexcept;

	bool use(Level &level, int x, int y) noexcept;
	std::string_view name() const noexcept;

	static Item create() noexcept;
};

struct CopperBrush : BrushSizeChangableItem {
	CopperBrush() noexcept;

	bool use(Level &level, int x, int y) noexcept;
	std::string_view name() const noexcept;

	static Item create() noexcept;
};

} // namespace item

} // namespace wf

#endif // WFORGE_LEVEL_H
