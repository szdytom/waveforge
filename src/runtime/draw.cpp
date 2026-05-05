#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <iostream>
#include <quickjs.h>

namespace wf {

#define JS_INT_GETTER(Cls, name, field)                            \
	JSValue Cls::name(                                             \
		JSContext *ctx, JSValueConst this_val, int, JSValueConst * \
	) {                                                            \
		auto *ptr = unwrap(ctx, this_val);                              \
		if (!ptr)                                                  \
			return JS_UNDEFINED;                                   \
		return JS_NewInt32(ctx, ptr->field);                       \
	}

#define JS_STR_GETTER(Cls, name, field)                            \
	JSValue Cls::name(                                             \
		JSContext *ctx, JSValueConst this_val, int, JSValueConst * \
	) {                                                            \
		auto *ptr = unwrap(ctx, this_val);                              \
		if (!ptr)                                                  \
			return JS_UNDEFINED;                                   \
		return JS_NewString(ctx, ptr->field);                      \
	}

#define JS_CONST_GETTER(Cls, name, str)                                    \
	JSValue Cls::name(JSContext *ctx, JSValueConst, int, JSValueConst *) { \
		return JS_NewString(ctx, str);                                     \
	}

namespace {

int getIntArg(JSContext *ctx, JSValueConst val, int default_val) {
	int32_t result;
	if (JS_ToInt32(ctx, &result, val) < 0) {
		JS_FreeValue(ctx, JS_GetException(ctx));
		return default_val;
	}
	return result;
}

} // anonymous namespace

void DrawTextData::render(
	sf::RenderTarget &target, const PixelFont *font, int scale
) const {
	if (font) {
		font->renderText(target, text, sf::Color(r, g, b), x, y, scale, size);
	}
}

JSValue DrawTextData::toJSValue(JSContext *ctx) const {
	return DrawTextClass::create(ctx, *this);
}

void DrawSpriteData::render(
	sf::RenderTarget &target, const PixelFont *, int scale
) const {
	if (texture_id.empty()) {
		return;
	}
	auto *texture = AssetsManager::instance().getAssetChecked<sf::Texture>(
		texture_id
	);
	if (!texture) {
		return;
	}
	sf::Sprite sprite(*texture);
	sprite.setPosition(sf::Vector2f(x * scale, y * scale));
	sprite.setScale(sf::Vector2f(scale, scale));
	target.draw(sprite);
}

JSValue DrawSpriteData::toJSValue(JSContext *ctx) const {
	return DrawSpriteClass::create(ctx, *this);
}

void DrawRectData::render(
	sf::RenderTarget &target, const PixelFont *, int scale
) const {
	sf::RectangleShape rect(sf::Vector2f(w * scale, h * scale));
	rect.setPosition(sf::Vector2f(x * scale, y * scale));
	rect.setFillColor(sf::Color(r, g, b));
	target.draw(rect);
}

JSValue DrawRectData::toJSValue(JSContext *ctx) const {
	return DrawRectClass::create(ctx, *this);
}

// ── Prototype builders ──

namespace {

void addGetter(
	JSContext *ctx, JSValue proto, const char *name, JSCFunction *getter
) {
	JSAtomGuard atom(ctx, name);
	auto getter_val = JSValueGuard::fromCFunction(
		ctx, getter, name, 0, JS_CFUNC_getter
	);
	JS_DefineProperty(
		ctx, proto, atom.get(), JS_UNDEFINED, getter_val.get(), JS_UNDEFINED,
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_HAS_GET
	);
}

JSValue buildTextProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	addGetter(ctx, proto, "type", DrawTextClass::get_type);
	addGetter(ctx, proto, "x", DrawTextClass::get_x);
	addGetter(ctx, proto, "y", DrawTextClass::get_y);
	addGetter(ctx, proto, "text", DrawTextClass::get_text);
	addGetter(ctx, proto, "size", DrawTextClass::get_size);
	addGetter(ctx, proto, "r", DrawTextClass::get_r);
	addGetter(ctx, proto, "g", DrawTextClass::get_g);
	addGetter(ctx, proto, "b", DrawTextClass::get_b);
	return proto;
}

JSValue buildSpriteProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	addGetter(ctx, proto, "type", DrawSpriteClass::get_type);
	addGetter(ctx, proto, "x", DrawSpriteClass::get_x);
	addGetter(ctx, proto, "y", DrawSpriteClass::get_y);
	addGetter(ctx, proto, "textureId", DrawSpriteClass::get_textureId);
	return proto;
}

JSValue buildRectProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	addGetter(ctx, proto, "type", DrawRectClass::get_type);
	addGetter(ctx, proto, "x", DrawRectClass::get_x);
	addGetter(ctx, proto, "y", DrawRectClass::get_y);
	addGetter(ctx, proto, "w", DrawRectClass::get_w);
	addGetter(ctx, proto, "h", DrawRectClass::get_h);
	addGetter(ctx, proto, "r", DrawRectClass::get_r);
	addGetter(ctx, proto, "g", DrawRectClass::get_g);
	addGetter(ctx, proto, "b", DrawRectClass::get_b);
	return proto;
}

} // anonymous namespace

// ── DrawTextClass ──

JSValue DrawTextClass::create(JSContext *ctx, const DrawTextData &d) {
	auto *ptr = new DrawTextClass();
	ptr->data = d;
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

void DrawTextClass::bindContext(JSContext *ctx) {
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), buildTextProto(ctx));
}

JS_CONST_GETTER(DrawTextClass, get_type, "text")
JS_INT_GETTER(DrawTextClass, get_x, data.x)
JS_INT_GETTER(DrawTextClass, get_y, data.y)
JS_STR_GETTER(DrawTextClass, get_text, data.text.c_str())
JS_INT_GETTER(DrawTextClass, get_size, data.size)
JS_INT_GETTER(DrawTextClass, get_r, data.r)
JS_INT_GETTER(DrawTextClass, get_g, data.g)
JS_INT_GETTER(DrawTextClass, get_b, data.b)

pro::proxy<DrawCmdFacade> DrawTextClass::invoke(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 7) {
		return {};
	}

	DrawTextData data;
	data.x = getIntArg(ctx, argv[0], 0);
	data.y = getIntArg(ctx, argv[1], 0);

	const char *text = JS_ToCString(ctx, argv[2]);
	data.text = text ? text : "";
	JS_FreeCString(ctx, text);

	data.size = getIntArg(ctx, argv[3], 1);
	data.r = static_cast<uint8_t>(getIntArg(ctx, argv[4], 255));
	data.g = static_cast<uint8_t>(getIntArg(ctx, argv[5], 255));
	data.b = static_cast<uint8_t>(getIntArg(ctx, argv[6], 255));

	return pro::make_proxy<DrawCmdFacade>(std::move(data));
}

// ── DrawSpriteClass ──

JSValue DrawSpriteClass::create(JSContext *ctx, const DrawSpriteData &d) {
	auto *ptr = new DrawSpriteClass();
	ptr->data = d;
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

void DrawSpriteClass::bindContext(JSContext *ctx) {
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), buildSpriteProto(ctx));
}

JS_CONST_GETTER(DrawSpriteClass, get_type, "sprite")
JS_INT_GETTER(DrawSpriteClass, get_x, data.x)
JS_INT_GETTER(DrawSpriteClass, get_y, data.y)
JS_STR_GETTER(DrawSpriteClass, get_textureId, data.texture_id.c_str())

pro::proxy<DrawCmdFacade> DrawSpriteClass::invoke(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 3) {
		return {};
	}

	DrawSpriteData data;
	data.x = getIntArg(ctx, argv[0], 0);
	data.y = getIntArg(ctx, argv[1], 0);

	// argv[2] can be a Texture object or a string ID
	if (JS_IsObject(argv[2])) {
		auto *tex = TextureClass::unwrap(ctx, argv[2]);
		if (tex) {
			data.texture_id = tex->id;
			return pro::make_proxy<DrawCmdFacade>(std::move(data));
		}
	}

	// Fallback: string ID
	{
		const char *id_str = JS_ToCString(ctx, argv[2]);
		if (!id_str) {
			return {};
		}
		data.texture_id = id_str;
		JS_FreeCString(ctx, id_str);
	}

	return pro::make_proxy<DrawCmdFacade>(std::move(data));
}

// ── DrawRectClass ──

JSValue DrawRectClass::create(JSContext *ctx, const DrawRectData &d) {
	auto *ptr = new DrawRectClass();
	ptr->data = d;
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		delete ptr;
		return obj;
	}
	JS_SetOpaque(obj, ptr);
	return obj;
}

void DrawRectClass::bindContext(JSContext *ctx) {
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), buildRectProto(ctx));
}

JS_CONST_GETTER(DrawRectClass, get_type, "rect")
JS_INT_GETTER(DrawRectClass, get_x, data.x)
JS_INT_GETTER(DrawRectClass, get_y, data.y)
JS_INT_GETTER(DrawRectClass, get_w, data.w)
JS_INT_GETTER(DrawRectClass, get_h, data.h)
JS_INT_GETTER(DrawRectClass, get_r, data.r)
JS_INT_GETTER(DrawRectClass, get_g, data.g)
JS_INT_GETTER(DrawRectClass, get_b, data.b)

pro::proxy<DrawCmdFacade> DrawRectClass::invoke(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 7) {
		return {};
	}

	DrawRectData data;
	data.x = getIntArg(ctx, argv[0], 0);
	data.y = getIntArg(ctx, argv[1], 0);
	data.w = getIntArg(ctx, argv[2], 0);
	data.h = getIntArg(ctx, argv[3], 0);
	data.r = static_cast<uint8_t>(getIntArg(ctx, argv[4], 255));
	data.g = static_cast<uint8_t>(getIntArg(ctx, argv[5], 255));
	data.b = static_cast<uint8_t>(getIntArg(ctx, argv[6], 255));

	return pro::make_proxy<DrawCmdFacade>(std::move(data));
}

// ── Init ──

void initDrawCommands(QuickJSEngine &engine, JSContext *ctx) {
	engine.registerClass<DrawTextClass>();
	engine.registerClass<DrawSpriteClass>();
	engine.registerClass<DrawRectClass>();

	DrawTextClass::bindContext(ctx);
	DrawSpriteClass::bindContext(ctx);
	DrawRectClass::bindContext(ctx);
}

// ── Render dispatch ──

void flushDrawCommands(
	const std::vector<pro::proxy<DrawCmdFacade>> &cmd_buffer,
	sf::RenderTarget &target, int scale, const PixelFont *font
) {
	for (const auto &cmd : cmd_buffer) {
		cmd->render(target, font, scale);
	}
}

} // namespace wf
