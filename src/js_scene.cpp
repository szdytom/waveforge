#include "wforge/js_scene.h"
#include "wforge/js_engine.h"
#include "wforge/version.h"
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window/Mouse.hpp>
#include <cstdint>
#include <format>
#include <iostream>
#include <proxy/proxy.h>
#include <proxy/v4/proxy.h>
#include <proxy/v4/proxy_macros.h>
#include <quickjs-libc.h>
#include <quickjs.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wf {

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemDrawRender, render);
PRO_DEF_MEM_DISPATCH(MemDrawFillJS, fillJSObject);

} // namespace _dispatch

/* clang-format off */
struct DrawCmdFacade : pro::facade_builder
	::add_convention<_dispatch::MemDrawRender, void(sf::RenderTarget&, const PixelFont*, int) const>
	::add_convention<_dispatch::MemDrawFillJS, void(JSContext*, JSValue) const>
	::support_relocation<pro::constraint_level::nontrivial>
	::build {};
/* clang-format on */

struct DrawTextCmd {
	int x;
	int y;
	std::string text;
	int size;
	sf::Color color;

	void render(
		sf::RenderTarget &target, const PixelFont *font, int scale
	) const {
		if (font) {
			font->renderText(target, text, color, x, y, scale, size);
		}
	}

	void fillJSObject(JSContext *ctx, JSValue obj) const {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "text"));
		JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, x));
		JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, y));
		JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, text.c_str()));
		JS_SetPropertyStr(ctx, obj, "size", JS_NewInt32(ctx, size));
		JS_SetPropertyStr(ctx, obj, "r", JS_NewInt32(ctx, color.r));
		JS_SetPropertyStr(ctx, obj, "g", JS_NewInt32(ctx, color.g));
		JS_SetPropertyStr(ctx, obj, "b", JS_NewInt32(ctx, color.b));
	}
};

struct DrawSpriteCmd {
	int x;
	int y;
	sf::Texture *texture = nullptr;
	std::string texture_id;

	void render(sf::RenderTarget &target, const PixelFont *, int scale) const {
		if (!texture) {
			return;
		}
		sf::Sprite sprite(*texture);
		sprite.setPosition(sf::Vector2f(x * scale, y * scale));
		sprite.setScale(sf::Vector2f(scale, scale));
		target.draw(sprite);
	}

	void fillJSObject(JSContext *ctx, JSValue obj) const {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "sprite"));
		JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, x));
		JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, y));
		JS_SetPropertyStr(
			ctx, obj, "textureId", JS_NewString(ctx, texture_id.c_str())
		);
	}
};

struct DrawRectCmd {
	int x;
	int y;
	int w;
	int h;
	sf::Color color;

	void render(sf::RenderTarget &target, const PixelFont *, int scale) const {
		sf::RectangleShape rect(sf::Vector2f(w * scale, h * scale));
		rect.setPosition(sf::Vector2f(x * scale, y * scale));
		rect.setFillColor(color);
		target.draw(rect);
	}

	void fillJSObject(JSContext *ctx, JSValue obj) const {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "rect"));
		JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, x));
		JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, y));
		JS_SetPropertyStr(ctx, obj, "w", JS_NewInt32(ctx, w));
		JS_SetPropertyStr(ctx, obj, "h", JS_NewInt32(ctx, h));
		JS_SetPropertyStr(ctx, obj, "r", JS_NewInt32(ctx, color.r));
		JS_SetPropertyStr(ctx, obj, "g", JS_NewInt32(ctx, color.g));
		JS_SetPropertyStr(ctx, obj, "b", JS_NewInt32(ctx, color.b));
	}
};

using DrawCmd = pro::proxy<DrawCmdFacade>;

struct NativeModuleState {
	std::vector<DrawCmd> cmd_buffer;
	std::string pending_scene_id;
	bool scene_change_pending = false;
	JSValueGuard module_ns;
};

namespace {

NativeModuleState *getState(JSContext *ctx) {
	return static_cast<NativeModuleState *>(JS_GetContextOpaque(ctx));
}

int getIntArg(JSContext *ctx, JSValueConst val, int default_val) {
	int32_t result;
	if (JS_ToInt32(ctx, &result, val) < 0) {
		JS_FreeValue(ctx, JS_GetException(ctx));
		return default_val;
	}
	return result;
}

struct TextureClass {
	static JSClassID clsId() {
		static JSClassID cid = 0;
		JS_NewClassID(&cid);
		return cid;
	}

	static void finalize(JSRuntime *rt, JSValue val) {
		auto *ptr = static_cast<TextureClass *>(JS_GetOpaque(val, clsId()));
		delete ptr;
	}

	static JSValue ctor(
		JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
	) {
		if (argc < 1) {
			return JS_ThrowTypeError(ctx, "Texture requires an id");
		}

		const char *id_str = JS_ToCString(ctx, argv[0]);
		if (!id_str) {
			return JS_EXCEPTION;
		}

		auto res = std::make_unique<TextureClass>();
		res->id = id_str;
		try {
			res->texture = &AssetsManager::instance().getAsset<sf::Texture>(
				id_str
			);
		} catch (const std::exception &) {
			JS_FreeCString(ctx, id_str);
			return JS_ThrowReferenceError(ctx, "Texture not found: %s", id_str);
		}

		JSValue obj = JS_NewObjectClass(ctx, TextureClass::clsId());
		if (JS_IsException(obj)) {
			JS_FreeCString(ctx, id_str);
			return obj;
		}

		JS_SetOpaque(obj, res.release());
		JS_FreeCString(ctx, id_str);
		return obj;
	}

	static void registerClass(JSRuntime *rt) {
		JSClassDef def = {
			.class_name = "Texture",
			.finalizer = finalize,
		};
		JS_NewClass(rt, clsId(), &def);
	}

	static void bindContext(JSContext *ctx, JSValue ns) {
		auto ctor_func = JS_NewCFunction2(
			ctx, ctor, "Texture", 1, JS_CFUNC_constructor, 0
		);
		auto proto = TextureClass::proto(ctx);
		JS_SetConstructor(ctx, ctor_func, proto);
		JS_SetClassProto(ctx, clsId(), proto);
		JS_DefinePropertyValueStr(
			ctx, ns, "Texture", ctor_func,
			JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
		);
	}

	static TextureClass *unwrap(JSValueConst obj) {
		return static_cast<TextureClass *>(JS_GetOpaque(obj, clsId()));
	}

	static JSValue get_id(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) {
		auto *ptr = unwrap(this_val);
		if (!ptr) {
			return JS_ThrowTypeError(ctx, "Invalid Texture object");
		}
		return JS_NewString(ctx, ptr->id.c_str());
	}

	static JSValue get_width(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) {
		auto *ptr = unwrap(this_val);
		if (!ptr || !ptr->texture) {
			return JS_ThrowTypeError(ctx, "Invalid Texture object");
		}
		return JS_NewInt32(ctx, ptr->texture->getSize().x);
	}

	static JSValue get_height(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) {
		auto *ptr = unwrap(this_val);
		if (!ptr || !ptr->texture) {
			return JS_ThrowTypeError(ctx, "Invalid Texture object");
		}
		return JS_NewInt32(ctx, ptr->texture->getSize().y);
	}

	static JSValue proto(JSContext *ctx) {
		JSValue proto = JS_NewObject(ctx);
		auto configure_getter = [&](const char *name, JSCFunction *getter) {
			JSAtomGuard atom(ctx, name);
			auto getter_val = JSValueGuard::fromCFunction(
				ctx, getter, name, 0, JS_CFUNC_getter
			);
			JS_DefineProperty(
				ctx, proto, atom.get(), JS_UNDEFINED, getter_val.get(),
				JS_UNDEFINED,
				JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_HAS_GET
			);
		};
		configure_getter("id", get_id);
		configure_getter("width", get_width);
		configure_getter("height", get_height);
		return proto;
	}

	sf::Texture *texture;
	std::string id;
};

JSValue native_console_log(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	for (int i = 0; i < argc; ++i) {
		const char *str = JS_ToCString(ctx, argv[i]);
		if (i > 0) {
			std::fprintf(stderr, " ");
		}
		std::fprintf(stderr, "%s", str ? str : "<unknown>");
		JS_FreeCString(ctx, str);
	}
	std::fprintf(stderr, "\n");
	return JS_UNDEFINED;
}

JSValue native_draw_text(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	auto *state = getState(ctx);
	if (!state || argc < 7) {
		return JS_UNDEFINED;
	}

	DrawTextCmd cmd;
	cmd.x = getIntArg(ctx, argv[0], 0);
	cmd.y = getIntArg(ctx, argv[1], 0);

	const char *text = JS_ToCString(ctx, argv[2]);
	cmd.text = text ? text : "";
	JS_FreeCString(ctx, text);

	cmd.size = getIntArg(ctx, argv[3], 1);
	int r = getIntArg(ctx, argv[4], 255);
	int g = getIntArg(ctx, argv[5], 255);
	int b = getIntArg(ctx, argv[6], 255);
	cmd.color = sf::Color(
		static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
		static_cast<std::uint8_t>(b)
	);

	state->cmd_buffer.push_back(pro::make_proxy<DrawCmdFacade>(std::move(cmd)));
	return JS_UNDEFINED;
}

JSValue native_draw_sprite(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	auto *state = getState(ctx);
	if (!state || argc < 3) {
		return JS_UNDEFINED;
	}

	DrawSpriteCmd cmd;
	cmd.x = getIntArg(ctx, argv[0], 0);
	cmd.y = getIntArg(ctx, argv[1], 0);

	// argv[2] can be a Texture object or a string ID
	if (JS_IsObject(argv[2])) {
		TextureClass *tex = TextureClass::unwrap(argv[2]);
		if (tex) {
			cmd.texture = tex->texture;
			cmd.texture_id = tex->id;
			state->cmd_buffer.push_back(
				pro::make_proxy<DrawCmdFacade>(std::move(cmd))
			);
			return JS_UNDEFINED;
		}
	}

	// Fallback: string ID
	{
		const char *id_str = JS_ToCString(ctx, argv[2]);
		if (!id_str) {
			return JS_UNDEFINED;
		}
		cmd.texture_id = id_str;
		try {
			cmd.texture = &AssetsManager::instance().getAsset<sf::Texture>(
				id_str
			);
		} catch (const std::exception &) {}
		JS_FreeCString(ctx, id_str);
	}

	state->cmd_buffer.push_back(pro::make_proxy<DrawCmdFacade>(std::move(cmd)));
	return JS_UNDEFINED;
}

JSValue native_draw_rect(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	auto *state = getState(ctx);
	if (!state || argc < 7) {
		return JS_UNDEFINED;
	}

	DrawRectCmd cmd;
	cmd.x = getIntArg(ctx, argv[0], 0);
	cmd.y = getIntArg(ctx, argv[1], 0);
	cmd.w = getIntArg(ctx, argv[2], 0);
	cmd.h = getIntArg(ctx, argv[3], 0);
	int r = getIntArg(ctx, argv[4], 255);
	int g = getIntArg(ctx, argv[5], 255);
	int b = getIntArg(ctx, argv[6], 255);
	cmd.color = sf::Color(
		static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
		static_cast<std::uint8_t>(b)
	);

	state->cmd_buffer.push_back(pro::make_proxy<DrawCmdFacade>(std::move(cmd)));
	return JS_UNDEFINED;
}

JSValue native_play_sound(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	auto *state = getState(ctx);
	if (!state || argc < 1) {
		return JS_UNDEFINED;
	}

	const char *id = JS_ToCString(ctx, argv[0]);
	if (!id) {
		return JS_UNDEFINED;
	}

	try {
		auto &buffer = AssetsManager::instance().getAsset<sf::SoundBuffer>(id);
		ActiveSoundManager::instance().play(buffer);
	} catch (const std::exception &e) {
		std::cerr << "playSound: " << e.what() << "\n";
	}

	JS_FreeCString(ctx, id);
	return JS_UNDEFINED;
}

JSValue native_change_scene(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	auto *state = getState(ctx);
	if (!state || argc < 1) {
		return JS_UNDEFINED;
	}

	const char *id = JS_ToCString(ctx, argv[0]);
	if (id) {
		state->pending_scene_id = id;
		state->scene_change_pending = true;
		JS_FreeCString(ctx, id);
	}
	return JS_UNDEFINED;
}

JSValue native_get_commands(
	JSContext *ctx, JSValueConst /*this_val*/, int /*argc*/,
	JSValueConst * /*argv*/
) {
	auto *state = getState(ctx);
	if (!state) {
		return JS_UNDEFINED;
	}

	JSValue arr = JS_NewArray(ctx);
	uint32_t i = 0;
	for (const auto &cmd : state->cmd_buffer) {
		JSValue obj = JS_NewObject(ctx);

		cmd->fillJSObject(ctx, obj);

		JS_SetPropertyUint32(ctx, arr, i++, obj);
	}
	return arr;
}

JSValue native_setup_scene(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	auto *state = getState(ctx);
	if (!state || argc < 1) {
		return JS_UNDEFINED;
	}

	// Store the scene API object on NativeModuleState for JSScene to use
	state->module_ns = JSValueGuard(ctx, JS_DupValue(ctx, argv[0]));
	return JS_UNDEFINED;
}

JSValue native_clear_commands(
	JSContext *ctx, JSValueConst /*this_val*/, int /*argc*/,
	JSValueConst * /*argv*/
) {
	auto *state = getState(ctx);
	if (state) {
		state->cmd_buffer.clear();
	}
	return JS_UNDEFINED;
}

} // anonymous namespace

static std::string sfKeyToString(sf::Keyboard::Key key) {
	using sf::Keyboard::Key;
	auto v = std::to_underlying(key);

	if (v >= std::to_underlying(Key::A) && v <= std::to_underlying(Key::Z)) {
		return std::string(
			1, static_cast<char>('A' + (v - std::to_underlying(Key::A)))
		);
	}

	if (v >= std::to_underlying(Key::Num0)
	    && v <= std::to_underlying(Key::Num9)) {
		return std::string(
			1, static_cast<char>('0' + (v - std::to_underlying(Key::Num0)))
		);
	}

	if (v >= std::to_underlying(Key::F1) && v <= std::to_underlying(Key::F12)) {
		return "F" + std::to_string(v - std::to_underlying(Key::F1) + 1);
	}

	switch (key) {
	case Key::Escape:
		return "Escape";
	case Key::Enter:
		return "Enter";
	case Key::Space:
		return "Space";
	case Key::Backspace:
		return "Backspace";
	case Key::Tab:
		return "Tab";
	case Key::Up:
		return "Up";
	case Key::Down:
		return "Down";
	case Key::Left:
		return "Left";
	case Key::Right:
		return "Right";
	case Key::LControl:
	case Key::RControl:
		return "Control";
	case Key::LShift:
	case Key::RShift:
		return "Shift";
	case Key::LAlt:
	case Key::RAlt:
		return "Alt";
	default:
		return "Unknown";
	}
}

struct JSScene::Impl {
	mutable QuickJSEngine engine;
	mutable NativeModuleState native_state;
	PixelFont *font = nullptr;

	std::string source_asset_id;
	std::string module_name;

	mutable int width = 400;
	mutable int height = 300;
	mutable bool size_fetched = false;

	Impl(const std::string &scene_id);

	void installConsole();
	bool callExport(const char *name, JSValue arg = JS_UNDEFINED) const;
	bool callExport(const char *name, JSValue arg1, JSValue arg2) const;
	JSValue eventToJSObject(sf::Event &evt);
	void flushDrawCommands(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;
	void resolvePendingSceneChange(SceneManager &mgr);

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;
};

JSScene::Impl::Impl(const std::string &scene_id)
	: source_asset_id("js/" + scene_id + "/source")
	, module_name("js/" + scene_id + "/source.js") {
	JS_SetContextOpaque(engine.context(), &native_state);
	installConsole();

	// Evaluate source directly (not bytecode) to isolate module issues
	const auto &source = AssetsManager::instance().getAsset<std::string>(
		source_asset_id
	);
	JSContext *ctx = engine.context();
	JSValueGuard ret(
		ctx,
		JS_Eval(
			ctx, source.c_str(), source.size(), module_name.c_str(),
			JS_EVAL_TYPE_MODULE
		)
	);
	if (JS_IsException(ret.get())) {
		js_std_dump_error(ctx);
		throw std::runtime_error("JSScene: module source evaluation failed");
	}

	if (!native_state.module_ns) {
		throw std::runtime_error("JSScene: module did not call setupScene()");
	}

	// Cache font reference for rendering
	font = &AssetsManager::instance().getAsset<PixelFont>("font");
}

void JSScene::Impl::installConsole() {
	JSContext *ctx = engine.context();
	JSValueGuard global(ctx, JS_GetGlobalObject(ctx));

	// console.log
	JSValue console = JS_NewObject(ctx);
	JSValue log_func = JS_NewCFunction(ctx, native_console_log, "log", 1);
	JS_SetPropertyStr(ctx, console, "log", log_func);
	JS_SetPropertyStr(ctx, global.get(), "console", console);

	// globalThis.waveforge — namespace object with all native functions.
	// JS scenes use this instead of import syntax:
	//   waveforge.log(...)
	//   waveforge.drawText(x, y, text, size, r, g, b)
	//   waveforge.drawSprite(x, y, textureId)
	//   waveforge.drawRect(x, y, w, h, r, g, b)
	//   waveforge.playSound(id)
	//   waveforge.changeScene(sceneId)
	//   waveforge.setupScene({size, setup, handleEvent, step, render})
	//   waveforge.getCommands()
	//   waveforge.clearCommands()
	JSValue wf = JS_NewObject(ctx);
	auto setFn = [&](const char *name, JSCFunction *func, int len) {
		JSValue f = JS_NewCFunction(ctx, func, name, len);
		JS_SetPropertyStr(ctx, wf, name, f);
	};
	setFn("log", native_console_log, 1);
	setFn("setupScene", native_setup_scene, 1);
	setFn("drawText", native_draw_text, 7);
	setFn("drawSprite", native_draw_sprite, 3);
	setFn("drawRect", native_draw_rect, 7);
	setFn("playSound", native_play_sound, 1);
	setFn("changeScene", native_change_scene, 1);
	setFn("getCommands", native_get_commands, 0);
	setFn("clearCommands", native_clear_commands, 0);
	// Register Texture class and constructor
	TextureClass::registerClass(engine.runtime());
	TextureClass::bindContext(ctx, wf);
	JS_SetPropertyStr(ctx, global.get(), "waveforge", wf);
}

bool JSScene::Impl::callExport(const char *name, JSValue arg) const {
	JSContext *ctx = engine.context();

	JSValueGuard func(
		ctx, JS_GetPropertyStr(ctx, native_state.module_ns.get(), name)
	);
	if (!func || !JS_IsFunction(ctx, func.get())) {
		return false;
	}

	JSValue arg_ptr = arg;
	JSValueGuard ret(ctx, JS_Call(ctx, func.get(), JS_UNDEFINED, 1, &arg_ptr));

	if (JS_IsException(ret.get())) {
		std::cerr << "JSScene: export '" << name << "' threw:\n";
		js_std_dump_error(ctx);
		return false;
	}

	return true;
}

bool JSScene::Impl::callExport(
	const char *name, JSValue arg1, JSValue arg2
) const {
	JSContext *ctx = engine.context();

	JSValueGuard func(
		ctx, JS_GetPropertyStr(ctx, native_state.module_ns.get(), name)
	);
	if (!func || !JS_IsFunction(ctx, func.get())) {
		return false;
	}

	JSValue args[2] = {arg1, arg2};
	JSValueGuard ret(ctx, JS_Call(ctx, func.get(), JS_UNDEFINED, 2, args));

	if (JS_IsException(ret.get())) {
		std::cerr << "JSScene: export '" << name << "' threw:\n";
		js_std_dump_error(ctx);
		return false;
	}

	return true;
}

std::array<int, 2> JSScene::Impl::size() const {
	if (size_fetched) {
		return {width, height};
	}

	JSContext *ctx = engine.context();
	JSValueGuard func(
		ctx, JS_GetPropertyStr(ctx, native_state.module_ns.get(), "size")
	);
	if (!func || !JS_IsFunction(ctx, func.get())) {
		return {width, height};
	}

	JSValueGuard ret(ctx, JS_Call(ctx, func.get(), JS_UNDEFINED, 0, nullptr));

	if (!JS_IsException(ret.get())) {
		uint32_t w, h;
		JSValueGuard wv(ctx, JS_GetPropertyUint32(ctx, ret.get(), 0));
		JSValueGuard hv(ctx, JS_GetPropertyUint32(ctx, ret.get(), 1));
		if (JS_ToUint32(ctx, &w, wv.get()) >= 0
		    && JS_ToUint32(ctx, &h, hv.get()) >= 0 && w > 0 && h > 0) {
			width = static_cast<int>(w);
			height = static_cast<int>(h);
		}
	}

	size_fetched = true;
	return {width, height};
}

void JSScene::Impl::setup(SceneManager &mgr) {
	mgr.setWindowTitle("Waveforge " WAVEFORGE_VERSION);
	callExport("setup");
}

void JSScene::Impl::handleEvent(SceneManager &mgr, sf::Event &evt) {
	JSValueGuard js_event(engine.context(), eventToJSObject(evt));
	callExport("handleEvent", js_event.get());
}

void JSScene::Impl::step(SceneManager &mgr) {
	callExport("step");
	ActiveSoundManager::instance().step();
	resolvePendingSceneChange(mgr);
}

void JSScene::Impl::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	callExport("render");
	flushDrawCommands(mgr, target, scale);
	native_state.cmd_buffer.clear();
}

JSValue JSScene::Impl::eventToJSObject(sf::Event &evt) {
	JSContext *ctx = engine.context();
	JSValue obj = JS_NewObject(ctx);

	if (auto *key = evt.getIf<sf::Event::KeyPressed>()) {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "keyPressed"));
		JS_SetPropertyStr(
			ctx, obj, "key", JS_NewString(ctx, sfKeyToString(key->code).c_str())
		);
		JS_SetPropertyStr(
			ctx, obj, "code", JS_NewInt32(ctx, static_cast<int>(key->code))
		);
		JS_SetPropertyStr(ctx, obj, "alt", JS_NewBool(ctx, key->alt));
		JS_SetPropertyStr(ctx, obj, "ctrl", JS_NewBool(ctx, key->control));
		JS_SetPropertyStr(ctx, obj, "shift", JS_NewBool(ctx, key->shift));
	} else if (auto *key = evt.getIf<sf::Event::KeyReleased>()) {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "keyReleased"));
		JS_SetPropertyStr(
			ctx, obj, "key", JS_NewString(ctx, sfKeyToString(key->code).c_str())
		);
		JS_SetPropertyStr(
			ctx, obj, "code", JS_NewInt32(ctx, static_cast<int>(key->code))
		);
	} else if (auto *mouse = evt.getIf<sf::Event::MouseMoved>()) {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "mouseMoved"));
		JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, mouse->position.x));
		JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, mouse->position.y));
	} else if (auto *btn = evt.getIf<sf::Event::MouseButtonPressed>()) {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "mousePressed"));
		JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, btn->position.x));
		JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, btn->position.y));
		JS_SetPropertyStr(
			ctx, obj, "button", JS_NewInt32(ctx, static_cast<int>(btn->button))
		);
	} else if (auto *btn = evt.getIf<sf::Event::MouseButtonReleased>()) {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "mouseReleased"));
		JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, btn->position.x));
		JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, btn->position.y));
		JS_SetPropertyStr(
			ctx, obj, "button", JS_NewInt32(ctx, static_cast<int>(btn->button))
		);
	} else if (evt.is<sf::Event::Closed>()) {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "closed"));
	} else {
		JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "unknown"));
	}

	return obj;
}

void JSScene::Impl::flushDrawCommands(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	for (const auto &cmd : native_state.cmd_buffer) {
		try {
			cmd->render(target, font, scale);
		} catch (const std::exception &e) {
			std::cerr << "JSScene: draw command error: " << e.what() << "\n";
		}
	}
}

void JSScene::Impl::resolvePendingSceneChange(SceneManager &mgr) {
	if (!native_state.scene_change_pending) {
		return;
	}

	native_state.scene_change_pending = false;
	std::string scene_id = std::move(native_state.pending_scene_id);
	native_state.pending_scene_id.clear();

	mgr.changeScene(createSceneFromId(scene_id));
}

JSScene::JSScene(const std::string &scene_id)
	: _impl(std::make_unique<Impl>(scene_id)) {}

JSScene::JSScene(JSScene &&) noexcept = default;
JSScene &JSScene::operator=(JSScene &&) noexcept = default;
JSScene::~JSScene() = default;

std::array<int, 2> JSScene::size() const {
	return _impl->size();
}
void JSScene::setup(SceneManager &mgr) {
	_impl->setup(mgr);
}
void JSScene::handleEvent(SceneManager &mgr, sf::Event &evt) {
	_impl->handleEvent(mgr, evt);
}
void JSScene::step(SceneManager &mgr) {
	_impl->step(mgr);
}
void JSScene::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	_impl->render(mgr, target, scale);
}

Scene createSceneFromId(const std::string &scene_id) {
	if (scene_id == "main-menu") {
		return pro::make_proxy<SceneFacade, scene::MainMenu>();
	}
	if (scene_id == "settings") {
		return pro::make_proxy<SceneFacade, scene::SettingsMenu>();
	}
	if (scene_id == "help") {
		return pro::make_proxy<SceneFacade, scene::Help>();
	}
	if (scene_id == "credits") {
		return pro::make_proxy<SceneFacade, scene::Credits>();
	}
	if (scene_id == "level-selection") {
		return pro::make_proxy<SceneFacade, scene::LevelSelectionMenu>();
	}
	if (scene_id == "__exit__") {
		std::exit(0);
	}

	if (scene_id.starts_with("js:")) {
		std::string inner = scene_id.substr(3);
		return pro::make_proxy<SceneFacade, JSScene>(inner);
	}

	throw std::invalid_argument(
		std::format("createSceneFromId: unknown scene '{}'", scene_id)
	);
}

} // namespace wf
