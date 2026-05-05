#include "wforge/js_scene.h"
#include "wforge/js_engine.h"
#include "wforge/runtime.h"
#include "wforge/version.h"
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Window/Mouse.hpp>
#include <cstdint>
#include <format>
#include <iostream>
#include <quickjs.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace wf {

namespace {

struct NativeModuleState {
	std::vector<pro::proxy<DrawCmdFacade>> cmd_buffer;
	std::string pending_scene_id;
	bool scene_change_pending = false;
	JSValueGuard module_ns;
};

NativeModuleState *getState(JSContext *ctx) {
	return static_cast<NativeModuleState *>(JS_GetContextOpaque(ctx));
}

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

	auto *buffer = AssetsManager::instance().getAssetChecked<sf::SoundBuffer>(
		id
	);
	if (buffer) {
		ActiveSoundManager::instance().play(*buffer);
	} else {
		std::cerr << "playSound: sound buffer not found: " << id << "\n";
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

// ── Draw command wrappers (proxy → buffer) ──

JSValue drawTextWrapper(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) {
	auto proxy = DrawTextClass::invoke(ctx, this_val, argc, argv);
	if (proxy) {
		auto *state = getState(ctx);
		state->cmd_buffer.push_back(std::move(proxy));
	}
	return JS_UNDEFINED;
}

JSValue drawSpriteWrapper(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) {
	auto proxy = DrawSpriteClass::invoke(ctx, this_val, argc, argv);
	if (proxy) {
		auto *state = getState(ctx);
		state->cmd_buffer.push_back(std::move(proxy));
	}
	return JS_UNDEFINED;
}

JSValue drawRectWrapper(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) {
	auto proxy = DrawRectClass::invoke(ctx, this_val, argc, argv);
	if (proxy) {
		auto *state = getState(ctx);
		state->cmd_buffer.push_back(std::move(proxy));
	}
	return JS_UNDEFINED;
}

JSValue getCommandsWrapper(
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
		JSValue obj = cmd->toJSValue(ctx);
		JS_SetPropertyUint32(ctx, arr, i++, obj);
	}
	return arr;
}

JSValue clearCommandsWrapper(
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
	JSValue wf = JS_NewObject(ctx);
	auto setFn = [&](const char *name, JSCFunction *func, int len) {
		JSValue f = JS_NewCFunction(ctx, func, name, len);
		JS_SetPropertyStr(ctx, wf, name, f);
	};
	setFn("log", native_console_log, 1);
	setFn("setupScene", native_setup_scene, 1);
	setFn("playSound", native_play_sound, 1);
	setFn("changeScene", native_change_scene, 1);

	// Register Texture class and constructor
	engine.registerClass<TextureClass>();
	TextureClass::bindContext(ctx, wf);

	// Register draw command classes
	initDrawCommands(engine, ctx);
	setFn("drawText", drawTextWrapper, 7);
	setFn("drawSprite", drawSpriteWrapper, 3);
	setFn("drawRect", drawRectWrapper, 7);
	setFn("getCommands", getCommandsWrapper, 0);
	setFn("clearCommands", clearCommandsWrapper, 0);

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
	wf::flushDrawCommands(native_state.cmd_buffer, target, scale, font);
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
