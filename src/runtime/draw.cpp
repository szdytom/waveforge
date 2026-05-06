#include "helpers.h"
#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <iostream>
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

namespace {

JSValue buildTextProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	AddGetter addGetter(ctx, proto);
	addGetter("type", DrawTextClass::get_type);
	addGetter("x", DrawTextClass::get_x);
	addGetter("y", DrawTextClass::get_y);
	addGetter("text", DrawTextClass::get_text);
	addGetter("size", DrawTextClass::get_size);
	addGetter("r", DrawTextClass::get_r);
	addGetter("g", DrawTextClass::get_g);
	addGetter("b", DrawTextClass::get_b);
	return proto;
}

JSValue buildSpriteProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	AddGetter addGetter(ctx, proto);
	addGetter("type", DrawSpriteClass::get_type);
	addGetter("x", DrawSpriteClass::get_x);
	addGetter("y", DrawSpriteClass::get_y);
	addGetter("textureId", DrawSpriteClass::get_textureId);
	return proto;
}

JSValue buildRectProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	AddGetter addGetter(ctx, proto);
	addGetter("type", DrawRectClass::get_type);
	addGetter("x", DrawRectClass::get_x);
	addGetter("y", DrawRectClass::get_y);
	addGetter("w", DrawRectClass::get_w);
	addGetter("h", DrawRectClass::get_h);
	addGetter("r", DrawRectClass::get_r);
	addGetter("g", DrawRectClass::get_g);
	addGetter("b", DrawRectClass::get_b);
	return proto;
}

} // anonymous namespace

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

WF_JS_LITERAL_GETTER(DrawTextClass, get_type, "text")
WF_JS_INT_GETTER(DrawTextClass, get_x, data.x)
WF_JS_INT_GETTER(DrawTextClass, get_y, data.y)
WF_JS_STR_GETTER(DrawTextClass, get_text, data.text.c_str())
WF_JS_INT_GETTER(DrawTextClass, get_size, data.size)
WF_JS_INT_GETTER(DrawTextClass, get_r, data.r)
WF_JS_INT_GETTER(DrawTextClass, get_g, data.g)
WF_JS_INT_GETTER(DrawTextClass, get_b, data.b)

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

WF_JS_LITERAL_GETTER(DrawSpriteClass, get_type, "sprite")
WF_JS_INT_GETTER(DrawSpriteClass, get_x, data.x)
WF_JS_INT_GETTER(DrawSpriteClass, get_y, data.y)
WF_JS_STR_GETTER(DrawSpriteClass, get_textureId, data.texture_id.c_str())

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
			data.texture = tex->texture;
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

WF_JS_LITERAL_GETTER(DrawRectClass, get_type, "rect")
WF_JS_INT_GETTER(DrawRectClass, get_x, data.x)
WF_JS_INT_GETTER(DrawRectClass, get_y, data.y)
WF_JS_INT_GETTER(DrawRectClass, get_w, data.w)
WF_JS_INT_GETTER(DrawRectClass, get_h, data.h)
WF_JS_INT_GETTER(DrawRectClass, get_r, data.r)
WF_JS_INT_GETTER(DrawRectClass, get_g, data.g)
WF_JS_INT_GETTER(DrawRectClass, get_b, data.b)

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

void installDrawCommands(QuickJSEngine &engine, JSContext *ctx) {
	engine.registerClass<DrawTextClass>();
	engine.registerClass<DrawSpriteClass>();
	engine.registerClass<DrawRectClass>();

	DrawTextClass::bindContext(ctx);
	DrawSpriteClass::bindContext(ctx);
	DrawRectClass::bindContext(ctx);
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
