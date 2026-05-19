#include "hacks.h"
#include "helper.h"
#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <cstring>
#include <memory>

namespace wf::js {

namespace {

// -- DrawTextCmd --
WF_JS_DEF_GETTER_STR(DrawTextCmd, getText, self->text.c_str())
WF_JS_DEF_SETTER_STR(DrawTextCmd, setText, text)
WF_JS_DEF_GETTER_I32(DrawTextCmd, getX, self->x)
WF_JS_DEF_SETTER_I32(DrawTextCmd, setX, x)
WF_JS_DEF_GETTER_I32(DrawTextCmd, getY, self->y)
WF_JS_DEF_SETTER_I32(DrawTextCmd, setY, y)
WF_JS_DEF_GETTER_I32(DrawTextCmd, getSize, self->size)
WF_JS_DEF_SETTER_I32(DrawTextCmd, setSize, size)

JSValue DrawTextCmd_getColor(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = DrawTextCmd::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->color);
}

JSValue DrawTextCmd_setColor(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = DrawTextCmd::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto color_object = Color::interpretAsValue(ctx, val);
	if (!color_object) {
		return JS_ThrowTypeError(ctx, "%s", color_object.error());
	}

	JS_FreeValue(ctx, self->color);
	self->color = *color_object;
	return JS_UNDEFINED;
}

WF_JS_METHOD(DrawTextCmd, toString, {
	return JS_NewString(ctx, self->text.c_str());
})

// -- DrawSpriteCmd --
JSValue DrawSpriteCmd_getTexture(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = DrawSpriteCmd::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->texture_val);
}

JSValue DrawSpriteCmd_setTexture(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = DrawSpriteCmd::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto *tex = Texture::unwrap(ctx, val);
	if (!tex) {
		return JS_ThrowTypeError(ctx, "Expected a Texture object");
	}

	JS_FreeValue(ctx, self->texture_val);
	self->texture_val = JS_DupValue(ctx, val);
	self->texture = tex->texture;
	return JS_UNDEFINED;
}

WF_JS_DEF_GETTER_I32(DrawSpriteCmd, getX, self->x)
WF_JS_DEF_SETTER_I32(DrawSpriteCmd, setX, x)
WF_JS_DEF_GETTER_I32(DrawSpriteCmd, getY, self->y)
WF_JS_DEF_SETTER_I32(DrawSpriteCmd, setY, y)

// -- DrawRectCmd --
WF_JS_DEF_GETTER_I32(DrawRectCmd, getX, self->x)
WF_JS_DEF_SETTER_I32(DrawRectCmd, setX, x)
WF_JS_DEF_GETTER_I32(DrawRectCmd, getY, self->y)
WF_JS_DEF_SETTER_I32(DrawRectCmd, setY, y)
WF_JS_DEF_GETTER_I32(DrawRectCmd, getWidth, self->width)
WF_JS_DEF_SETTER_I32(DrawRectCmd, setWidth, width)
WF_JS_DEF_GETTER_I32(DrawRectCmd, getHeight, self->height)
WF_JS_DEF_SETTER_I32(DrawRectCmd, setHeight, height)

JSValue DrawRectCmd_getColor(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = DrawRectCmd::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->color);
}

JSValue DrawRectCmd_setColor(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = DrawRectCmd::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto color_object = Color::interpretAsValue(ctx, val);
	if (!color_object) {
		return JS_ThrowTypeError(ctx, "%s", color_object.error());
	}

	JS_FreeValue(ctx, self->color);
	self->color = *color_object;
	return JS_UNDEFINED;
}

// -- DrawCmdList --
WF_JS_METHOD(DrawCmdList, push, {
	auto pushCmd = ([ctx, self](JSValueConst arg, auto *cmd) noexcept {
		DrawCmdList::DrawCmdEntry entry;
		entry.val = JS_DupValue(ctx, arg);
		entry.cmd = pro::make_proxy_view<DrawCmdFacade>(*cmd);
		self->cmds.push_back(std::move(entry));
		return JS_UNDEFINED;
	});

	if (auto *textCmd = DrawTextCmd::unwrap(ctx, argv[0])) {
		return pushCmd(argv[0], textCmd);
	}
	if (auto *spriteCmd = DrawSpriteCmd::unwrap(ctx, argv[0])) {
		return pushCmd(argv[0], spriteCmd);
	}
	if (auto *rectCmd = DrawRectCmd::unwrap(ctx, argv[0])) {
		return pushCmd(argv[0], rectCmd);
	}
	return JS_ThrowTypeError(
		ctx, "Expected a DrawTextCmd, DrawSpriteCmd, or DrawRectCmd"
	);
})

WF_JS_METHOD(DrawCmdList, iter, {
	auto iter = std::make_unique<DrawCmdListIter>();
	iter->index = 0;
	iter->list = self;
	iter->list_val = JS_DupValue(ctx, this_val);

	Value list_guard(ctx, iter->list_val);

	JSValue obj = JS_NewObjectClass(
		ctx, DrawCmdListIter::clsId(JS_GetRuntime(ctx))
	);

	if (JS_IsException(obj)) {
		return obj;
	}
	if (JS_SetOpaque(obj, iter.get()) < 0) {
		return JS_ThrowTypeError(ctx, "Internal error");
	}

	(void)iter.release();
	(void)list_guard.release();
	return obj;
})

JSValue DrawCmdListIter_next(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
	int *pdone, int magic
) noexcept {
	auto *self = DrawCmdListIter::unwrap(ctx, this_val);
	if (!self || self->index >= self->list->cmds.size()) {
		*pdone = 1;
		return JS_UNDEFINED;
	}
	*pdone = 0;
	return JS_DupValue(ctx, self->list->cmds[self->index++].val);
}

} // namespace

DrawTextCmd::DrawTextCmd(std::string text, int x, int y) noexcept
	: text(std::move(text)), x(x), y(y), size(1) {}

sf::Color DrawTextCmd::nativeColor(JSContext *ctx) const noexcept {
	return Color::fromValue(ctx, color).value_or(sf::Color::Black);
}

void DrawTextCmd::render(
	sf::RenderTarget &target, JSContext *ctx, int scale
) const {
	_font->renderText(target, text, nativeColor(ctx), x, y, scale, size);
}

static const JSCFunctionListEntry DRAW_TEXT_CMD_PROTO[] = {
	cGetSetDef("text", DrawTextCmd_getText, DrawTextCmd_setText),
	cGetSetDef("x", DrawTextCmd_getX, DrawTextCmd_setX),
	cGetSetDef("y", DrawTextCmd_getY, DrawTextCmd_setY),
	cGetSetDef("size", DrawTextCmd_getSize, DrawTextCmd_setSize),
	cGetSetDef("color", DrawTextCmd_getColor, DrawTextCmd_setColor),
	cFuncDef("toString", 0, DrawTextCmd_toString),
	propStringDef("[Symbol.toStringTag]", "DrawTextCmd", JS_PROP_CONFIGURABLE),
};

const CFunctionList DrawTextCmd::PROTO_FIELDS{DRAW_TEXT_CMD_PROTO};

JSValue DrawTextCmd::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	const char *text = JS_ToCString(ctx, argv[0]);
	if (!text) {
		return JS_ThrowTypeError(ctx, "Expected a string for text");
	}

	int32_t x, y;
	if (JS_ToInt32(ctx, &x, argv[1]) < 0 || JS_ToInt32(ctx, &y, argv[2]) < 0) {
		JS_FreeCString(ctx, text);
		return JS_ThrowTypeError(ctx, "Expected integers for x and y");
	}

	auto self = std::make_unique<DrawTextCmd>(std::string(text), x, y);
	self->_font = &AssetsManager::instance().getAsset<PixelFont>("font");
	JS_FreeCString(ctx, text);

	// Optional 4th argument for size
	if (argc >= 4) {
		int32_t size;
		if (JS_ToInt32(ctx, &size, argv[3]) < 0) {
			return JS_ThrowTypeError(ctx, "Expected an integer for size");
		}
		if (size <= 0) {
			return JS_ThrowTypeError(ctx, "Size must be positive");
		}
		self->size = size;
	}

	if (argc >= 5) {
		auto cv = Color::interpretAsValue(ctx, argv[4]);
		if (!cv) {
			return JS_ThrowTypeError(ctx, "%s", cv.error());
		}
		self->color = *cv;
	}

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self.release();
	return obj;
}

DrawRectCmd::DrawRectCmd(int x, int y, int width, int height) noexcept
	: x(x), y(y), width(width), height(height) {}

sf::Color DrawRectCmd::nativeColor(JSContext *ctx) const noexcept {
	return Color::fromValue(ctx, color).value_or(sf::Color::Black);
}

void DrawRectCmd::render(
	sf::RenderTarget &target, JSContext *ctx, int scale
) const {
	sf::RectangleShape rect(sf::Vector2f(width * scale, height * scale));
	rect.setPosition(sf::Vector2f(x * scale, y * scale));
	rect.setFillColor(nativeColor(ctx));
	target.draw(rect);
}

DrawSpriteCmd::DrawSpriteCmd(
	JSValue texture_val, sf::Texture *texture, int x, int y
) noexcept
	: texture_val(texture_val), texture(texture), x(x), y(y) {}

void DrawSpriteCmd::render(
	sf::RenderTarget &target, JSContext * /*ctx*/, int scale
) const {
	sf::Sprite sprite(*texture);
	sprite.setPosition(sf::Vector2f(x * scale, y * scale));
	sprite.setScale(sf::Vector2f(scale, scale));
	target.draw(sprite);
}

void DrawSpriteCmd::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	JS_MarkValue(rt, self->texture_val, mark_func);
}

void DrawSpriteCmd::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	JS_FreeValueRT(rt, self->texture_val);
	delete self;
}

static const JSCFunctionListEntry DRAW_SPRITE_CMD_PROTO[] = {
	cGetSetDef("texture", DrawSpriteCmd_getTexture, DrawSpriteCmd_setTexture),
	cGetSetDef("x", DrawSpriteCmd_getX, DrawSpriteCmd_setX),
	cGetSetDef("y", DrawSpriteCmd_getY, DrawSpriteCmd_setY),
	propStringDef(
		"[Symbol.toStringTag]", "DrawSpriteCmd", JS_PROP_CONFIGURABLE
	),
};

const CFunctionList DrawSpriteCmd::PROTO_FIELDS{DRAW_SPRITE_CMD_PROTO};

JSValue DrawSpriteCmd::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *texObj = Texture::unwrap(ctx, argv[0]);
	if (!texObj) {
		return JS_ThrowTypeError(
			ctx, "Expected a Texture object as first argument"
		);
	}

	int32_t x, y;
	if (JS_ToInt32(ctx, &x, argv[1]) < 0) {
		return JS_ThrowTypeError(ctx, "Expected integer for x");
	}
	if (JS_ToInt32(ctx, &y, argv[2]) < 0) {
		return JS_ThrowTypeError(ctx, "Expected integer for y");
	}

	Value tex_guard(ctx, JS_DupValue(ctx, argv[0]));
	auto self = std::make_unique<DrawSpriteCmd>(
		*tex_guard, texObj->texture, x, y
	);

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self.release();
	tex_guard.release();
	return obj;
}

void DrawTextCmd::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	JS_MarkValue(rt, self->color, mark_func);
}

void DrawTextCmd::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	JS_FreeValueRT(rt, self->color);
	delete self;
}

static const JSCFunctionListEntry DRAW_RECT_CMD_PROTO[] = {
	cGetSetDef("x", DrawRectCmd_getX, DrawRectCmd_setX),
	cGetSetDef("y", DrawRectCmd_getY, DrawRectCmd_setY),
	cGetSetDef("width", DrawRectCmd_getWidth, DrawRectCmd_setWidth),
	cGetSetDef("height", DrawRectCmd_getHeight, DrawRectCmd_setHeight),
	cGetSetDef("color", DrawRectCmd_getColor, DrawRectCmd_setColor),
	propStringDef("[Symbol.toStringTag]", "DrawRectCmd", JS_PROP_CONFIGURABLE),
};

const CFunctionList DrawRectCmd::PROTO_FIELDS{DRAW_RECT_CMD_PROTO};

JSValue DrawRectCmd::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	int32_t x, y, width, height;
	if (JS_ToInt32(ctx, &x, argv[0]) < 0 || JS_ToInt32(ctx, &y, argv[1]) < 0
	    || JS_ToInt32(ctx, &width, argv[2]) < 0
	    || JS_ToInt32(ctx, &height, argv[3]) < 0) {
		return JS_ThrowTypeError(
			ctx, "Expected integers for x, y, width, and height"
		);
	}

	auto self = std::make_unique<DrawRectCmd>(x, y, width, height);

	if (argc >= 5) {
		auto cv = Color::interpretAsValue(ctx, argv[4]);
		if (!cv) {
			return JS_ThrowTypeError(ctx, "%s", cv.error());
		}
		self->color = *cv;
	}

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self.release();
	return obj;
}

void DrawRectCmd::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	JS_MarkValue(rt, self->color, mark_func);
}

void DrawRectCmd::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	JS_FreeValueRT(rt, self->color);
	delete self;
}

void DrawCmdList::render(
	sf::RenderTarget &target, JSContext *ctx, int scale
) const {
	for (const auto &entry : cmds) {
		entry.cmd->render(target, ctx, scale);
	}
}

void DrawCmdList::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	for (const auto &entry : self->cmds) {
		JS_MarkValue(rt, entry.val, mark_func);
	}
}

void DrawCmdList::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	for (auto &entry : self->cmds) {
		JS_FreeValueRT(rt, entry.val);
	}
	delete self;
}

static const JSCFunctionListEntry DRAW_CMD_LIST_PROTO[] = {
	cFuncDef("push", 1, DrawCmdList_push),
	cFuncDef("[Symbol.iterator]", 0, DrawCmdList_iter),
	propStringDef("[Symbol.toStringTag]", "DrawCmdList", JS_PROP_CONFIGURABLE),
};

const CFunctionList DrawCmdList::PROTO_FIELDS{DRAW_CMD_LIST_PROTO};

JSValue DrawCmdList::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto self = std::make_unique<DrawCmdList>();
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self.release();
	return obj;
}

void DrawCmdListIter::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	JS_MarkValue(rt, self->list_val, mark_func);
}

void DrawCmdListIter::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	JS_FreeValueRT(rt, self->list_val);
	delete self;
}

JSValue DrawCmdListIter::parentProto(JSContext *ctx) noexcept {
	return getIteratorProto(ctx);
}

static const JSCFunctionListEntry DRAW_CMD_LIST_ITER_PROTO[] = {
	cFuncIteratorNextDef("next", 0, DrawCmdListIter_next, 0),
	propStringDef(
		"[Symbol.toStringTag]", "DrawCmdList Iterator", JS_PROP_CONFIGURABLE
	),
};

const CFunctionList DrawCmdListIter::PROTO_FIELDS{DRAW_CMD_LIST_ITER_PROTO};

} // namespace wf::js
