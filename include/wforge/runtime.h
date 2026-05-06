#ifndef WFORGE_RUNTIME_H
#define WFORGE_RUNTIME_H

#include "wforge/ctti.h"
#include "wforge/js_engine.h"
#include <SFML/Graphics/Texture.hpp>
#include <cstdint>
#include <proxy/v4/proxy.h>
#include <proxy/v4/proxy_macros.h>
#include <quickjs.h>
#include <string>
#include <vector>

namespace sf {
class RenderTarget;
}

namespace wf {

class PixelFont;

// CRTP base for QuickJS native classes.
// Derived must define `static constexpr const char *className`.
template<typename Derived>
class QuickJSClass {
public:
	// FNV-1a of className, evaluated at compile time.
	static constexpr std::size_t typeHash() noexcept {
		return fnv1a(Derived::className);
	}

	static JSClassID clsId(JSRuntime *rt) {
		return static_cast<QuickJSEngine *>(JS_GetRuntimeOpaque(rt))
			->template classId<Derived>();
	}

	static void finalize(JSRuntime *rt, JSValue val) {
		delete static_cast<Derived *>(JS_GetOpaque(val, clsId(rt)));
	}

	static Derived *unwrap(JSContext *ctx, JSValueConst obj) {
		return static_cast<Derived *>(
			JS_GetOpaque(obj, clsId(JS_GetRuntime(ctx)))
		);
	}

	static void registerClass(JSRuntime *rt) {
		JSClassDef def = {
			.class_name = Derived::className,
			.finalizer = finalize,
		};
		JS_NewClass(rt, clsId(rt), &def);
	}
};

// Texture wrapper exposed to JS.
// Constructed as `new waveforge.Texture(assetId)` from JS.
struct TextureClass : QuickJSClass<TextureClass> {
	static constexpr const char *className = "Texture";

	static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static void bindContext(JSContext *ctx, JSValue ns);

	static JSValue get_id(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_width(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_height(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);

	sf::Texture *texture = nullptr;
	std::string id;
};

// ── Draw command type-erased facade ──

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemDrawRender, render);
PRO_DEF_MEM_DISPATCH(MemDrawToJS, toJSValue);

} // namespace _dispatch

/* clang-format off */
struct DrawCmdFacade : pro::facade_builder
	::add_convention<_dispatch::MemDrawRender, void(sf::RenderTarget&, const PixelFont*, int) const>
	::add_convention<_dispatch::MemDrawToJS, JSValue(JSContext*) const>
	::support_relocation<pro::constraint_level::nontrivial>
	::build {};
/* clang-format on */

// ── Draw command data types ──

struct DrawTextData {
	int x = 0;
	int y = 0;
	std::string text;
	int size = 1;
	uint8_t r = 255;
	uint8_t g = 255;
	uint8_t b = 255;

	void render(
		sf::RenderTarget &target, const PixelFont *font, int scale
	) const;
	JSValue toJSValue(JSContext *ctx) const;
};

struct DrawSpriteData {
	int x = 0;
	int y = 0;
	std::string texture_id;

	void render(
		sf::RenderTarget &target, const PixelFont *font, int scale
	) const;
	JSValue toJSValue(JSContext *ctx) const;
};

struct DrawRectData {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	uint8_t r = 255;
	uint8_t g = 255;
	uint8_t b = 255;

	void render(
		sf::RenderTarget &target, const PixelFont *font, int scale
	) const;
	JSValue toJSValue(JSContext *ctx) const;
};

// ── Draw command JS binding classes ──

struct DrawTextClass : QuickJSClass<DrawTextClass> {
	static constexpr const char *className = "DrawText";

	DrawTextData data;

	static JSValue create(JSContext *ctx, const DrawTextData &d);
	static void bindContext(JSContext *ctx);

	static pro::proxy<DrawCmdFacade> invoke(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);

	static JSValue get_type(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_x(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_y(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_text(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_size(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_r(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_g(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_b(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
};

struct DrawSpriteClass : QuickJSClass<DrawSpriteClass> {
	static constexpr const char *className = "DrawSprite";

	DrawSpriteData data;

	static JSValue create(JSContext *ctx, const DrawSpriteData &d);
	static void bindContext(JSContext *ctx);

	static pro::proxy<DrawCmdFacade> invoke(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);

	static JSValue get_type(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_x(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_y(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_textureId(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
};

struct DrawRectClass : QuickJSClass<DrawRectClass> {
	static constexpr const char *className = "DrawRect";

	DrawRectData data;

	static JSValue create(JSContext *ctx, const DrawRectData &d);
	static void bindContext(JSContext *ctx);

	static pro::proxy<DrawCmdFacade> invoke(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);

	static JSValue get_type(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_x(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_y(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_w(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_h(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_r(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_g(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
	static JSValue get_b(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	);
};

// Register all draw command classes on the engine.
void initDrawCommands(QuickJSEngine &engine, JSContext *ctx);

// Render all commands in the buffer.
void flushDrawCommands(
	const std::vector<pro::proxy<DrawCmdFacade>> &cmd_buffer,
	sf::RenderTarget &target, int scale, const PixelFont *font
);

} // namespace wf

#endif // WFORGE_RUNTIME_H
