#ifndef WFORGE_RUNTIME_LEVEL_H
#define WFORGE_RUNTIME_LEVEL_H

#include "wforge/layout.h"
#include "wforge/level.h"
#include <SFML/Graphics/RenderTexture.hpp>

namespace wf::js {

struct LevelSurface final : BindingBase<LevelSurface> {
	static constexpr const char *CLASS_NAME = "LevelSurface";
	static constexpr int CTOR_LENGTH = 1;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	explicit LevelSurface(Level level);

	[[nodiscard]] int width() const noexcept;
	[[nodiscard]] int height() const noexcept;
	[[nodiscard]] int duckX() const noexcept;
	[[nodiscard]] int duckY() const noexcept;
	[[nodiscard]] bool failed() const noexcept;
	[[nodiscard]] bool completed() const noexcept;
	[[nodiscard]] const LevelMetadata &metadata() const noexcept;

	void step();
	void useItem(int x, int y) noexcept;
	void changeBrushSize(int delta) noexcept;
	void selectItem(int index) noexcept;
	void prevItem() noexcept;
	void nextItem() noexcept;
	void setPointer(int x, int y) noexcept;

	YGSize measure(
		YGMeasureMode width_mode, float width, YGMeasureMode height_mode,
		float height
	) const noexcept;
	void render(
		sf::RenderTarget &target, JSContext *ctx, LayoutParameters parameters
	) const;

private:
	Level _level;
	mutable LevelRenderer _renderer;
	mutable sf::RenderTexture _render_texture;
	int _pointer_x = 0;
	int _pointer_y = 0;
};

} // namespace wf::js

#endif // WFORGE_RUNTIME_LEVEL_H
