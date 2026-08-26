#include "hacks.h"
#include "helper.h"
#include "wforge/runtime_level.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cmath>
#include <memory>

namespace wf::js {

namespace {

float resolveDimension(
	float intrinsic, float available, YGMeasureMode mode
) noexcept {
	switch (mode) {
	case YGMeasureModeExactly:
		return available;
	case YGMeasureModeAtMost:
		return std::min(available, intrinsic);
	case YGMeasureModeUndefined:
	default:
		return intrinsic;
	}
}

JSValue getWidth(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewInt32(ctx, self->width()) : JS_UNDEFINED;
}

JSValue getHeight(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewInt32(ctx, self->height()) : JS_UNDEFINED;
}

JSValue getDuckX(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewInt32(ctx, self->duckX()) : JS_UNDEFINED;
}

JSValue getDuckY(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewInt32(ctx, self->duckY()) : JS_UNDEFINED;
}

JSValue getFailed(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewBool(ctx, self->failed()) : JS_UNDEFINED;
}

JSValue getCompleted(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewBool(ctx, self->completed()) : JS_UNDEFINED;
}

JSValue getLevelId(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewString(ctx, self->metadata().id.c_str()) : JS_UNDEFINED;
}

JSValue getLevelIndex(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewInt32(ctx, self->metadata().index) : JS_UNDEFINED;
}

JSValue getLevelName(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	return self ? JS_NewString(ctx, self->metadata().name.c_str())
				: JS_UNDEFINED;
}

JSValue step(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	if (self) {
		self->step();
	}
	return JS_UNDEFINED;
}

JSValue applyItem(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	int32_t x;
	int32_t y;
	if (!self || argc < 2 || JS_ToInt32(ctx, &x, argv[0])
	    || JS_ToInt32(ctx, &y, argv[1])) {
		return JS_ThrowTypeError(ctx, "applyItem expects x and y coordinates");
	}
	self->applyItem(x, y);
	return JS_UNDEFINED;
}

JSValue changeBrushSize(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	int32_t delta;
	if (!self || argc < 1 || JS_ToInt32(ctx, &delta, argv[0])) {
		return JS_ThrowTypeError(ctx, "changeBrushSize expects a delta");
	}
	self->changeBrushSize(delta);
	return JS_UNDEFINED;
}

JSValue selectItem(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	int32_t index;
	if (!self || argc < 1 || JS_ToInt32(ctx, &index, argv[0])) {
		return JS_ThrowTypeError(ctx, "selectItem expects an index");
	}
	self->selectItem(index);
	return JS_UNDEFINED;
}

JSValue prevItem(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	if (self) {
		self->prevItem();
	}
	return JS_UNDEFINED;
}

JSValue nextItem(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	if (self) {
		self->nextItem();
	}
	return JS_UNDEFINED;
}

JSValue setPointer(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LevelSurface::unwrap(ctx, this_val);
	int32_t x;
	int32_t y;
	if (!self || argc < 2 || JS_ToInt32(ctx, &x, argv[0])
	    || JS_ToInt32(ctx, &y, argv[1])) {
		return JS_ThrowTypeError(ctx, "setPointer expects x and y coordinates");
	}
	self->setPointer(x, y);
	return JS_UNDEFINED;
}

const JSCFunctionListEntry LEVEL_SURFACE_PROTO[] = {
	cGetSetDef("width", getWidth, nullptr),
	cGetSetDef("height", getHeight, nullptr),
	cGetSetDef("duckX", getDuckX, nullptr),
	cGetSetDef("duckY", getDuckY, nullptr),
	cGetSetDef("failed", getFailed, nullptr),
	cGetSetDef("completed", getCompleted, nullptr),
	cGetSetDef("levelId", getLevelId, nullptr),
	cGetSetDef("levelIndex", getLevelIndex, nullptr),
	cGetSetDef("levelName", getLevelName, nullptr),
	cFuncDef("step", 0, step),
	cFuncDef("applyItem", 2, applyItem),
	cFuncDef("changeBrushSize", 1, changeBrushSize),
	cFuncDef("selectItem", 1, selectItem),
	cFuncDef("prevItem", 0, prevItem),
	cFuncDef("nextItem", 0, nextItem),
	cFuncDef("setPointer", 2, setPointer),
	propStringDef("[Symbol.toStringTag]", "LevelSurface", JS_PROP_CONFIGURABLE),
};

} // namespace

const CFunctionList LevelSurface::PROTO_FIELDS{LEVEL_SURFACE_PROTO};

LevelSurface::LevelSurface(Level level)
	: _level(std::move(level))
	, _renderer(_level)
	, _render_texture(sf::Vector2u(_level.width(), _level.height())) {
	_level.selectItem(0);
}

JSValue LevelSurface::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "LevelSurface expects a level id");
	}

	const char *level_id = JS_ToCString(ctx, argv[0]);
	if (!level_id) {
		return JS_EXCEPTION;
	}

	try {
		auto self = std::make_unique<LevelSurface>(
			Level::loadFromAsset(level_id)
		);
		JS_FreeCString(ctx, level_id);

		JSValue object = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
		if (JS_IsException(object)) {
			return object;
		}
		JS_SetOpaque(object, self.release());
		return object;
	} catch (const std::exception &error) {
		JS_FreeCString(ctx, level_id);
		return JS_ThrowInternalError(ctx, "%s", error.what());
	}
}

int LevelSurface::width() const noexcept {
	return _level.width();
}

int LevelSurface::height() const noexcept {
	return _level.height();
}

int LevelSurface::duckX() const noexcept {
	return static_cast<int>(std::round(_level.duck.position.x));
}

int LevelSurface::duckY() const noexcept {
	return static_cast<int>(std::round(_level.duck.position.y));
}

bool LevelSurface::failed() const noexcept {
	return _level.isFailed();
}

bool LevelSurface::completed() const noexcept {
	return _level.isCompleted();
}

const LevelMetadata &LevelSurface::metadata() const noexcept {
	return _level.metadata;
}

void LevelSurface::step() {
	_level.step();
}

void LevelSurface::applyItem(int x, int y) noexcept {
	_level.useActiveItem(x, y);
}

void LevelSurface::changeBrushSize(int delta) noexcept {
	_level.changeActiveItemBrushSize(delta);
}

void LevelSurface::selectItem(int index) noexcept {
	_level.selectItem(index);
}

void LevelSurface::prevItem() noexcept {
	_level.prevItem();
}

void LevelSurface::nextItem() noexcept {
	_level.nextItem();
}

void LevelSurface::setPointer(int x, int y) noexcept {
	_pointer_x = x;
	_pointer_y = y;
}

YGSize LevelSurface::measure(
	YGMeasureMode width_mode, float width, YGMeasureMode height_mode,
	float height
) const noexcept {
	return {
		resolveDimension(static_cast<float>(_level.width()), width, width_mode),
		resolveDimension(
			static_cast<float>(_level.height()), height, height_mode
		),
	};
}

void LevelSurface::render(
	sf::RenderTarget &target, JSContext *ctx, LayoutParameters parameters
) const {
	_render_texture.clear();
	_renderer.render(_render_texture, _pointer_x, _pointer_y, 1);
	_render_texture.display();

	sf::Sprite sprite(_render_texture.getTexture());
	sprite.setPosition(
		sf::Vector2f(
			parameters.x * parameters.scale, parameters.y * parameters.scale
		)
	);
	sprite.setScale(sf::Vector2f(parameters.scale, parameters.scale));
	target.draw(sprite);
}

} // namespace wf::js
