#include "wforge/runtime.h"
#include "wforge/scene.h"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <format>
#include <iostream>
#include <utility>

namespace wf {

namespace {

// script engine instance for all scripted scenes
js::Engine &scriptEngine() {
	static std::unique_ptr<js::Engine> engine;
	if (!engine) {
		engine = std::make_unique<js::Engine>();
	}
	return *engine;
}

} // namespace

struct ScriptedScene::Impl {
	js::ContextPtr ctx;
	const Script *script; // not owned, managed by AssetsManager

	js::Value setup_obj;
	js::Value size_fn;
	js::Value setup_fn;
	js::Value step_fn;
	js::Value render_fn;
	js::Value handle_event_fn;

	js::Value cmds_val;
	js::DrawCmdList *cmds = nullptr;

	int width;
	int height;

	explicit Impl(const std::string &script_id);
	~Impl();
};

namespace {

JSValue f_log(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	const char *str = JS_ToCString(ctx, argv[0]);
	std::cerr << "[JS] " << (str ? str : "null") << "\n";
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

JSValue f_setupScene(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = static_cast<ScriptedScene::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_UNDEFINED;
	}

	JSValue obj = argv[0];
	if (!JS_IsObject(obj)) {
		return JS_ThrowTypeError(ctx, "setupScene expects an object");
	}

	impl->setup_obj = js::Value(ctx, JS_DupValue(ctx, obj));

	JSValue size_fn = JS_GetPropertyStr(ctx, obj, "size");
	if (JS_IsFunction(ctx, size_fn)) {
		impl->size_fn = js::Value(ctx, size_fn);
	} else {
		JS_FreeValue(ctx, size_fn);
		return JS_ThrowTypeError(ctx, "Missing size()");
	}

	JSValue setup_fn = JS_GetPropertyStr(ctx, obj, "setup");
	if (JS_IsFunction(ctx, setup_fn)) {
		impl->setup_fn = js::Value(ctx, setup_fn);
	} else {
		JS_FreeValue(ctx, setup_fn);
	}

	JSValue step_fn = JS_GetPropertyStr(ctx, obj, "step");
	if (JS_IsFunction(ctx, step_fn)) {
		impl->step_fn = js::Value(ctx, step_fn);
	} else {
		JS_FreeValue(ctx, step_fn);
	}

	JSValue render_fn = JS_GetPropertyStr(ctx, obj, "render");
	if (JS_IsFunction(ctx, render_fn)) {
		impl->render_fn = js::Value(ctx, render_fn);
	} else {
		JS_FreeValue(ctx, render_fn);
	}

	JSValue handle_event_fn = JS_GetPropertyStr(ctx, obj, "handleEvent");
	if (JS_IsFunction(ctx, handle_event_fn)) {
		impl->handle_event_fn = js::Value(ctx, handle_event_fn);
	} else {
		JS_FreeValue(ctx, handle_event_fn);
	}

	js::Value result_guard(
		ctx, JS_Call(ctx, *impl->size_fn, *impl->setup_obj, 0, nullptr)
	);
	JSValue result = *result_guard;
	if (!JS_IsException(result)) {
		js::Value w_guard(ctx, JS_GetPropertyUint32(ctx, result, 0));
		js::Value h_guard(ctx, JS_GetPropertyUint32(ctx, result, 1));
		int32_t w, h;
		if (JS_ToInt32(ctx, &w, *w_guard) < 0) {
			return JS_ThrowTypeError(ctx, "Failed to convert width to int32");
		}
		if (JS_ToInt32(ctx, &h, *h_guard) < 0) {
			return JS_ThrowTypeError(ctx, "Failed to convert height to int32");
		}
		impl->width = w;
		impl->height = h;
	}

	return JS_UNDEFINED;
}

JSValue f_commitDraw(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = static_cast<ScriptedScene::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_UNDEFINED;
	}

	auto *dc_list = js::DrawCmdList::unwrap(ctx, argv[0]);
	if (!dc_list) {
		return JS_ThrowTypeError(ctx, "commitDraw expects a DrawCmdList");
	}

	impl->cmds_val = js::Value(ctx, JS_DupValue(ctx, argv[0]));
	impl->cmds = dc_list;

	return JS_UNDEFINED;
}

JSValue createJSEvent(JSContext *ctx, const sf::Event &evt) noexcept {
	if (const auto *e = evt.getIf<sf::Event::KeyPressed>()) {
		return js::KeyEvent::from(ctx, *e);
	}
	if (const auto *e = evt.getIf<sf::Event::KeyReleased>()) {
		return js::KeyEvent::from(ctx, *e);
	}
	if (const auto *e = evt.getIf<sf::Event::MouseButtonPressed>()) {
		return js::MouseButtonEvent::from(ctx, *e);
	}
	if (const auto *e = evt.getIf<sf::Event::MouseButtonReleased>()) {
		return js::MouseButtonEvent::from(ctx, *e);
	}
	if (const auto *e = evt.getIf<sf::Event::MouseMoved>()) {
		return js::MouseMoveEvent::from(ctx, *e);
	}
	return JS_NULL;
}

using SceneBindings = js::BindingList<
	js::Texture, js::Color, js::DrawTextCmd, js::DrawSpriteCmd, js::DrawRectCmd,
	js::DrawCmdList, js::DrawCmdListIter, js::KeyEvent, js::MouseButtonEvent,
	js::MouseMoveEvent>;

void initJSContext(JSContext *ctx, ScriptedScene::Impl *impl) {
	JS_SetContextOpaque(ctx, impl);

	JSValue ns = JS_NewObject(ctx);

	SceneBindings::bindContext(ctx, ns);

	JS_SetPropertyStr(ctx, ns, "log", JS_NewCFunction(ctx, f_log, "log", 1));
	JS_SetPropertyStr(
		ctx, ns, "setupScene",
		JS_NewCFunction(ctx, f_setupScene, "setupScene", 1)
	);
	JS_SetPropertyStr(
		ctx, ns, "commitDraw",
		JS_NewCFunction(ctx, f_commitDraw, "commitDraw", 1)
	);

	js::Value global(ctx, JS_GetGlobalObject(ctx));
	JS_SetPropertyStr(ctx, *global, "waveforge", ns);
}

} // namespace

ScriptedScene::Impl::Impl(const std::string &script_id): script(nullptr) {
	auto &engine = scriptEngine();

	SceneBindings::registerClass(engine);

	auto *raw_ctx = engine.createContext();
	engine.releaseContext(raw_ctx);
	ctx.reset(raw_ctx);

	initJSContext(ctx.get(), this);

	script = AssetsManager::instance().getAssetChecked<Script>(script_id);
	if (!script) {
		throw std::runtime_error(
			std::format("ScriptedScene: script '{}' not found", script_id)
		);
	}
	js::Value eval_guard(
		ctx.get(),
		JS_Eval(
			ctx.get(), script->source.c_str(), script->source.size(),
			script->filename.c_str(), JS_EVAL_TYPE_GLOBAL
		)
	);
	JSValue result = *eval_guard;

	if (JS_IsException(result)) {
		js::dumpJSError(ctx.get());
		throw std::runtime_error(
			std::format("ScriptedScene: failed to evaluate '{}'", script_id)
		);
	}

	if (!JS_IsFunction(ctx.get(), *size_fn)) {
		throw std::runtime_error(
			std::format(
				"ScriptedScene: script '{}' did not call "
				"waveforge.setupScene()",
				script_id
			)
		);
	}
}

ScriptedScene::Impl::~Impl() = default;

ScriptedScene::ScriptedScene(const std::string &script_id)
	: _impl(std::make_unique<Impl>(script_id)) {}

ScriptedScene::~ScriptedScene() = default;

std::array<int, 2> ScriptedScene::size() const {
	return {_impl->width, _impl->height};
}

void ScriptedScene::setup(SceneManager &mgr) {
	if (JS_IsFunction(_impl->ctx.get(), *_impl->setup_fn)) {
		js::Value result_guard(
			_impl->ctx.get(),
			JS_Call(
				_impl->ctx.get(), *_impl->setup_fn, *_impl->setup_obj, 0,
				nullptr
			)
		);
		JSValue result = *result_guard;
		if (JS_IsException(result)) {
			js::Value exc_guard(
				_impl->ctx.get(), JS_GetException(_impl->ctx.get())
			);
			const char *str = JS_ToCString(_impl->ctx.get(), *exc_guard);
			std::cerr << "[JS] setup error: " << (str ? str : "unknown")
					  << "\n";
			JS_FreeCString(_impl->ctx.get(), str);
		}
	}
}

void ScriptedScene::handleEvent(SceneManager &mgr, sf::Event &evt) {
	auto *ctx = _impl->ctx.get();
	if (!JS_IsFunction(ctx, *_impl->handle_event_fn)) {
		return;
	}

	js::Value evt_guard(ctx, createJSEvent(ctx, evt));
	JSValue event_val = *evt_guard;
	if (JS_IsNull(event_val)) {
		return;
	}

	js::Value result_guard(
		ctx,
		JS_Call(ctx, *_impl->handle_event_fn, *_impl->setup_obj, 1, &event_val)
	);
	JSValue result = *result_guard;
	if (JS_IsException(result)) {
		js::Value exc_guard(ctx, JS_GetException(ctx));
		const char *str = JS_ToCString(ctx, *exc_guard);
		std::cerr << "[JS] handleEvent error: " << (str ? str : "unknown")
				  << "\n";
		JS_FreeCString(ctx, str);
	}
}

void ScriptedScene::step(SceneManager &mgr) {
	if (JS_IsFunction(_impl->ctx.get(), *_impl->step_fn)) {
		js::Value result_guard(
			_impl->ctx.get(),
			JS_Call(
				_impl->ctx.get(), *_impl->step_fn, *_impl->setup_obj, 0, nullptr
			)
		);
		JSValue result = *result_guard;
		if (JS_IsException(result)) {
			js::Value exc_guard(
				_impl->ctx.get(), JS_GetException(_impl->ctx.get())
			);
			const char *str = JS_ToCString(_impl->ctx.get(), *exc_guard);
			std::cerr << "[JS] step error: " << (str ? str : "unknown") << "\n";
			JS_FreeCString(_impl->ctx.get(), str);
		}
	}
}

void ScriptedScene::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	this->_impl->cmds_val = js::Value();
	this->_impl->cmds = nullptr;
	if (JS_IsFunction(_impl->ctx.get(), *_impl->render_fn)) {
		js::Value result_guard(
			_impl->ctx.get(),
			JS_Call(
				_impl->ctx.get(), *_impl->render_fn, *_impl->setup_obj, 0,
				nullptr
			)
		);
		JSValue result = *result_guard;
		if (JS_IsException(result)) {
			js::Value exc_guard(
				_impl->ctx.get(), JS_GetException(_impl->ctx.get())
			);
			const char *str = JS_ToCString(_impl->ctx.get(), *exc_guard);
			std::cerr << "[JS] render error: " << (str ? str : "unknown")
					  << "\n";
			JS_FreeCString(_impl->ctx.get(), str);
		}
	}

	if (_impl->cmds) {
		_impl->cmds->render(target, scale);
	}
}

} // namespace wf
