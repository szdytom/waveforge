#include "helpers.h"
#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <quickjs.h>

namespace wf {

namespace {

int getIntArg(JSContext *ctx, JSValueConst val, int default_val) {
	int32_t result;
	if (JS_ToInt32(ctx, &result, val) < 0) {
		JS_FreeValue(ctx, JS_GetException(ctx));
		return default_val;
	}
	return result;
}

JSValue buildTextProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	AddGetter addGetter(ctx, proto);
	addGetter("type", DrawTextCommand::get_type);
	addGetter("x", DrawTextCommand::get_x);
	addGetter("y", DrawTextCommand::get_y);
	addGetter("text", DrawTextCommand::get_text);
	addGetter("size", DrawTextCommand::get_size);
	addGetter("r", DrawTextCommand::get_r);
	addGetter("g", DrawTextCommand::get_g);
	addGetter("b", DrawTextCommand::get_b);
	return proto;
}

JSValue buildSpriteProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	AddGetter addGetter(ctx, proto);
	addGetter("type", DrawSpriteCommand::get_type);
	addGetter("x", DrawSpriteCommand::get_x);
	addGetter("y", DrawSpriteCommand::get_y);
	addGetter("textureId", DrawSpriteCommand::get_textureId);
	return proto;
}

JSValue buildRectProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	AddGetter addGetter(ctx, proto);
	addGetter("type", DrawRectCommand::get_type);
	addGetter("x", DrawRectCommand::get_x);
	addGetter("y", DrawRectCommand::get_y);
	addGetter("w", DrawRectCommand::get_w);
	addGetter("h", DrawRectCommand::get_h);
	addGetter("r", DrawRectCommand::get_r);
	addGetter("g", DrawRectCommand::get_g);
	addGetter("b", DrawRectCommand::get_b);
	return proto;
}

// Iterator next() helper
JSValue iterNext(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	JSValue result = JS_NewObject(ctx);
	JSValue items = JS_GetPropertyStr(ctx, this_val, "_items");
	JSValue idx_val = JS_GetPropertyStr(ctx, this_val, "_idx");
	int32_t idx;
	JS_ToInt32(ctx, &idx, idx_val);
	JS_FreeValue(ctx, idx_val);

	int64_t len;
	if (JS_GetLength(ctx, items, &len) < 0) {
		len = 0;
		JS_FreeValue(ctx, JS_GetException(ctx));
	}

	if (static_cast<int64_t>(idx) >= len) {
		JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, 1));
	} else {
		JSValue val = JS_GetPropertyUint32(
			ctx, items, static_cast<uint32_t>(idx)
		);
		JS_SetPropertyStr(ctx, result, "value", val);
		JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, 0));
		JS_SetPropertyStr(ctx, this_val, "_idx", JS_NewInt32(ctx, idx + 1));
	}
	JS_FreeValue(ctx, items);
	return result;
}

} // anonymous namespace

JSValue DrawTextCommand::ctor(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 7) {
		return JS_ThrowTypeError(ctx, "DrawText requires 7 arguments");
	}

	auto *ptr = new DrawTextCommand();
	ptr->x = getIntArg(ctx, argv[0], 0);
	ptr->y = getIntArg(ctx, argv[1], 0);

	const char *text = JS_ToCString(ctx, argv[2]);
	ptr->text = text ? text : "";
	JS_FreeCString(ctx, text);

	ptr->size = getIntArg(ctx, argv[3], 1);
	ptr->r = static_cast<uint8_t>(getIntArg(ctx, argv[4], 255));
	ptr->g = static_cast<uint8_t>(getIntArg(ctx, argv[5], 255));
	ptr->b = static_cast<uint8_t>(getIntArg(ctx, argv[6], 255));

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

void DrawTextCommand::bindContext(JSContext *ctx, JSValue ns) {
	auto ctor_func = JS_NewCFunction2(
		ctx, ctor, DrawTextCommand::className, 7, JS_CFUNC_constructor, 0
	);
	auto proto = buildTextProto(ctx);
	JS_SetConstructor(ctx, ctor_func, proto);
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), proto);
	JS_DefinePropertyValueStr(
		ctx, ns, DrawTextCommand::className, ctor_func,
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
}

void DrawTextCommand::render(
	sf::RenderTarget &target, const PixelFont *font, int scale
) const {
	if (font) {
		font->renderText(target, text, sf::Color(r, g, b), x, y, scale, size);
	}
}

WF_JS_LITERAL_GETTER(DrawTextCommand, get_type, "text")
WF_JS_INT_GETTER(DrawTextCommand, get_x, x)
WF_JS_INT_GETTER(DrawTextCommand, get_y, y)
WF_JS_STR_GETTER(DrawTextCommand, get_text, text.c_str())
WF_JS_INT_GETTER(DrawTextCommand, get_size, size)
WF_JS_INT_GETTER(DrawTextCommand, get_r, r)
WF_JS_INT_GETTER(DrawTextCommand, get_g, g)
WF_JS_INT_GETTER(DrawTextCommand, get_b, b)

JSValue DrawSpriteCommand::ctor(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 3) {
		return JS_ThrowTypeError(ctx, "DrawSprite requires 3 arguments");
	}

	auto *ptr = new DrawSpriteCommand();
	ptr->x = getIntArg(ctx, argv[0], 0);
	ptr->y = getIntArg(ctx, argv[1], 0);

	// argv[2] can be a Texture object or a string ID
	if (JS_IsObject(argv[2])) {
		auto *tex = TextureClass::unwrap(ctx, argv[2]);
		if (tex) {
			ptr->texture_id = tex->id;
			ptr->texture = tex->texture;
			goto done;
		}
	}
	{
		const char *id_str = JS_ToCString(ctx, argv[2]);
		if (!id_str) {
			delete ptr;
			return JS_EXCEPTION;
		}
		ptr->texture_id = id_str;
		ptr->texture = AssetsManager::instance().getAssetChecked<sf::Texture>(
			id_str
		);
		JS_FreeCString(ctx, id_str);
	}

done:
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

void DrawSpriteCommand::bindContext(JSContext *ctx, JSValue ns) {
	auto ctor_func = JS_NewCFunction2(
		ctx, ctor, DrawSpriteCommand::className, 3, JS_CFUNC_constructor, 0
	);
	auto proto = buildSpriteProto(ctx);
	JS_SetConstructor(ctx, ctor_func, proto);
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), proto);
	JS_DefinePropertyValueStr(
		ctx, ns, DrawSpriteCommand::className, ctor_func,
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
}

void DrawSpriteCommand::render(
	sf::RenderTarget &target, const PixelFont *, int scale
) const {
	if (texture_id.empty()) {
		return;
	}
	auto *tex = texture;
	if (!tex) {
		tex = AssetsManager::instance().getAssetChecked<sf::Texture>(
			texture_id
		);
	}
	if (!tex) {
		return;
	}
	sf::Sprite sprite(*tex);
	sprite.setPosition(sf::Vector2f(x * scale, y * scale));
	sprite.setScale(sf::Vector2f(scale, scale));
	target.draw(sprite);
}

WF_JS_LITERAL_GETTER(DrawSpriteCommand, get_type, "sprite")
WF_JS_INT_GETTER(DrawSpriteCommand, get_x, x)
WF_JS_INT_GETTER(DrawSpriteCommand, get_y, y)
WF_JS_STR_GETTER(DrawSpriteCommand, get_textureId, texture_id.c_str())

JSValue DrawRectCommand::ctor(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 7) {
		return JS_ThrowTypeError(ctx, "DrawRect requires 7 arguments");
	}

	auto *ptr = new DrawRectCommand();
	ptr->x = getIntArg(ctx, argv[0], 0);
	ptr->y = getIntArg(ctx, argv[1], 0);
	ptr->w = getIntArg(ctx, argv[2], 0);
	ptr->h = getIntArg(ctx, argv[3], 0);
	ptr->r = static_cast<uint8_t>(getIntArg(ctx, argv[4], 255));
	ptr->g = static_cast<uint8_t>(getIntArg(ctx, argv[5], 255));
	ptr->b = static_cast<uint8_t>(getIntArg(ctx, argv[6], 255));

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

void DrawRectCommand::bindContext(JSContext *ctx, JSValue ns) {
	auto ctor_func = JS_NewCFunction2(
		ctx, ctor, DrawRectCommand::className, 7, JS_CFUNC_constructor, 0
	);
	auto proto = buildRectProto(ctx);
	JS_SetConstructor(ctx, ctor_func, proto);
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), proto);
	JS_DefinePropertyValueStr(
		ctx, ns, DrawRectCommand::className, ctor_func,
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
}

void DrawRectCommand::render(
	sf::RenderTarget &target, const PixelFont *, int scale
) const {
	sf::RectangleShape rect(sf::Vector2f(w * scale, h * scale));
	rect.setPosition(sf::Vector2f(x * scale, y * scale));
	rect.setFillColor(sf::Color(r, g, b));
	target.draw(rect);
}

WF_JS_LITERAL_GETTER(DrawRectCommand, get_type, "rect")
WF_JS_INT_GETTER(DrawRectCommand, get_x, x)
WF_JS_INT_GETTER(DrawRectCommand, get_y, y)
WF_JS_INT_GETTER(DrawRectCommand, get_w, w)
WF_JS_INT_GETTER(DrawRectCommand, get_h, h)
WF_JS_INT_GETTER(DrawRectCommand, get_r, r)
WF_JS_INT_GETTER(DrawRectCommand, get_g, g)
WF_JS_INT_GETTER(DrawRectCommand, get_b, b)

JSValue DrawCmdBuffer::ctor(
	JSContext *ctx, JSValueConst /*this_val*/, int /*argc*/,
	JSValueConst * /*argv*/
) {
	auto *ptr = new DrawCmdBuffer();
	ptr->ctx = ctx;

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

DrawCmdBuffer::~DrawCmdBuffer() {
	if (ctx) {
		for (auto &entry : entries) {
			JS_FreeValue(ctx, entry.js_val);
		}
	}
}

void DrawCmdBuffer::bindContext(JSContext *ctx, JSValue ns) {
	auto ctor_func = JS_NewCFunction2(
		ctx, ctor, DrawCmdBuffer::className, 0, JS_CFUNC_constructor, 0
	);
	auto proto = JS_NewObject(ctx);
	JS_SetConstructor(ctx, ctor_func, proto);
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), proto);
	JS_DefinePropertyValueStr(
		ctx, ns, DrawCmdBuffer::className, ctor_func,
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
	JS_DefinePropertyValueStr(
		ctx, proto, "add", JS_NewCFunction(ctx, add, "add", 1),
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
	JS_DefinePropertyValueStr(
		ctx, proto, "clear", JS_NewCFunction(ctx, clear, "clear", 0),
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
	// TODO: iterator
}

JSValue DrawCmdBuffer::add(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) {
	auto *buf = unwrap(ctx, this_val);
	if (!buf || argc < 1) {
		return JS_UNDEFINED;
	}

	pro::proxy<DrawCmdFacade> cmd_proxy;
	if (auto *cmd = DrawTextCommand::unwrap(ctx, argv[0])) {
		cmd_proxy = pro::make_proxy<DrawCmdFacade>(*cmd);
	} else if (auto *cmd = DrawSpriteCommand::unwrap(ctx, argv[0])) {
		cmd_proxy = pro::make_proxy<DrawCmdFacade>(*cmd);
	} else if (auto *cmd = DrawRectCommand::unwrap(ctx, argv[0])) {
		cmd_proxy = pro::make_proxy<DrawCmdFacade>(*cmd);
	} else {
		return JS_ThrowTypeError(ctx, "unsupported draw command type");
	}

	buf->entries.push_back({std::move(cmd_proxy), JS_DupValue(ctx, argv[0])});
	return JS_UNDEFINED;
}

JSValue DrawCmdBuffer::clear(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *buf = unwrap(ctx, this_val);
	if (!buf) {
		return JS_UNDEFINED;
	}

	for (auto &entry : buf->entries) {
		JS_FreeValue(ctx, entry.js_val);
	}
	buf->entries.clear();
	return JS_UNDEFINED;
}

JSValue DrawCmdBuffer::iterator(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *buf = unwrap(ctx, this_val);
	if (!buf) {
		return JS_ThrowTypeError(ctx, "Invalid DrawCmdBuffer");
	}

	JSValue iter = JS_NewObject(ctx);
	JSValue arr = JS_NewArray(ctx);
	uint32_t i = 0;
	for (auto &entry : buf->entries) {
		JS_SetPropertyUint32(ctx, arr, i++, JS_DupValue(ctx, entry.js_val));
	}
	JS_SetPropertyStr(ctx, iter, "_items", arr);
	JS_SetPropertyStr(ctx, iter, "_idx", JS_NewInt32(ctx, 0));

	JSValue next_fn = JS_NewCFunction(ctx, iterNext, "next", 0);
	JS_SetPropertyStr(ctx, iter, "next", next_fn);
	return iter;
}

void DrawCmdBuffer::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) {
	auto *buf = unwrap(rt, val);
	if (!buf) {
		return;
	}

	for (auto &entry : buf->entries) {
		JS_MarkValue(rt, entry.js_val, mark_func);
	}
}

void installDrawCommands(QuickJSEngine &engine, JSContext *ctx, JSValue ns) {
	engine.registerClass<DrawTextCommand>();
	engine.registerClass<DrawSpriteCommand>();
	engine.registerClass<DrawRectCommand>();
	engine.registerClass<DrawCmdBuffer>();

	DrawTextCommand::bindContext(ctx, ns);
	DrawSpriteCommand::bindContext(ctx, ns);
	DrawRectCommand::bindContext(ctx, ns);
	DrawCmdBuffer::bindContext(ctx, ns);
}

void flushDrawCommands(
	const std::vector<pro::proxy<DrawCmdFacade>> &cmd_buffer,
	sf::RenderTarget &target, int scale, const PixelFont *font
) {
	for (const auto &cmd : cmd_buffer) {
		cmd->render(target, font, scale);
	}
}

} // namespace wf
