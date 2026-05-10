#include "wforge/runtime.h"
#include "wforge/scene.h"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <format>
#include <iostream>

namespace wf {

namespace {

js::Engine &scriptEngine() {
	static js::Engine engine;
	return engine;
}

} // namespace

struct ScriptedScene::Impl {
	JSContext *ctx;
	const Script *script;

	JSValue setup_obj = JS_NULL;
	JSValue size_fn = JS_NULL;
	JSValue setup_fn = JS_NULL;
	JSValue step_fn = JS_NULL;
	JSValue render_fn = JS_NULL;

	JSValue cmds_val = JS_NULL;
	js::DrawCmdList *cmds = nullptr;

	int width = 256;
	int height = 192;

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

	impl->setup_obj = JS_DupValue(ctx, obj);

	JSValue size_fn = JS_GetPropertyStr(ctx, obj, "size");
	if (JS_IsFunction(ctx, size_fn)) {
		impl->size_fn = size_fn;
	} else {
		JS_FreeValue(ctx, size_fn);
	}

	JSValue setup_fn = JS_GetPropertyStr(ctx, obj, "setup");
	if (JS_IsFunction(ctx, setup_fn)) {
		impl->setup_fn = setup_fn;
	} else {
		JS_FreeValue(ctx, setup_fn);
	}

	JSValue step_fn = JS_GetPropertyStr(ctx, obj, "step");
	if (JS_IsFunction(ctx, step_fn)) {
		impl->step_fn = step_fn;
	} else {
		JS_FreeValue(ctx, step_fn);
	}

	JSValue render_fn = JS_GetPropertyStr(ctx, obj, "render");
	if (JS_IsFunction(ctx, render_fn)) {
		impl->render_fn = render_fn;
	} else {
		JS_FreeValue(ctx, render_fn);
	}

	if (JS_IsFunction(ctx, impl->size_fn)) {
		js::ValueGuard result_guard(
			ctx, JS_Call(ctx, impl->size_fn, impl->setup_obj, 0, nullptr)
		);
		JSValue result = result_guard.get();
		if (!JS_IsException(result)) {
			js::ValueGuard w_guard(ctx, JS_GetPropertyUint32(ctx, result, 0));
			js::ValueGuard h_guard(ctx, JS_GetPropertyUint32(ctx, result, 1));
			int32_t w, h;
			JS_ToInt32(ctx, &w, w_guard.get());
			JS_ToInt32(ctx, &h, h_guard.get());
			impl->width = w;
			impl->height = h;
		}
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
		return JS_ThrowTypeError(ctx, "commitDraw expects a DrawCmdBuffer");
	}

	if (!JS_IsNull(impl->cmds_val)) {
		JS_FreeValue(ctx, impl->cmds_val);
	}
	impl->cmds_val = JS_DupValue(ctx, argv[0]);
	impl->cmds = dc_list;

	return JS_UNDEFINED;
}

void initJSContext(JSContext *ctx, ScriptedScene::Impl *impl) {
	JS_SetContextOpaque(ctx, impl);

	JSValue ns = JS_NewObject(ctx);

	js::Texture::bindContext(ctx, ns);
	js::Color::bindContext(ctx, ns);
	js::DrawTextCmd::bindContext(ctx, ns);
	js::DrawSpriteCmd::bindContext(ctx, ns);
	js::DrawRectCmd::bindContext(ctx, ns);
	js::DrawCmdList::bindContext(ctx, ns);

	JSValue alias = JS_GetPropertyStr(ctx, ns, "DrawCmdList");
	JS_DefinePropertyValueStr(
		ctx, ns, "DrawCmdBuffer", alias, JS_PROP_CONFIGURABLE
	);
	JS_FreeValue(ctx, alias);

	JS_SetPropertyStr(ctx, ns, "log", JS_NewCFunction(ctx, f_log, "log", 1));
	JS_SetPropertyStr(
		ctx, ns, "setupScene",
		JS_NewCFunction(ctx, f_setupScene, "setupScene", 1)
	);
	JS_SetPropertyStr(
		ctx, ns, "commitDraw",
		JS_NewCFunction(ctx, f_commitDraw, "commitDraw", 1)
	);

	JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "waveforge", ns);
}

} // namespace

ScriptedScene::Impl::Impl(const std::string &script_id)
	: ctx(nullptr), script(nullptr) {
	auto &engine = scriptEngine();

	engine.registerClass<js::Texture>();
	engine.registerClass<js::Color>();
	engine.registerClass<js::DrawTextCmd>();
	engine.registerClass<js::DrawSpriteCmd>();
	engine.registerClass<js::DrawRectCmd>();
	engine.registerClass<js::DrawCmdList>();
	engine.registerClass<js::DrawCmdListIter>();

	ctx = engine.createContext();

	initJSContext(ctx, this);

	auto *script_ptr = AssetsManager::instance().getAssetChecked<Script>(
		script_id
	);
	if (!script_ptr) {
		throw std::runtime_error(
			std::format("ScriptedScene: script '{}' not found", script_id)
		);
	}
	script = script_ptr;

	js::ValueGuard eval_guard(
		ctx,
		JS_Eval(
			ctx, script->source.c_str(), script->source.size(),
			script->filename.c_str(), JS_EVAL_TYPE_GLOBAL
		)
	);
	JSValue result = eval_guard.get();

	if (JS_IsException(result)) {
		js::dumpJSError(ctx);
		throw std::runtime_error(
			std::format("ScriptedScene: failed to evaluate '{}'", script_id)
		);
	}

	if (!JS_IsFunction(ctx, size_fn)) {
		throw std::runtime_error(
			std::format(
				"ScriptedScene: script '{}' did not call "
				"waveforge.setupScene()",
				script_id
			)
		);
	}
}

ScriptedScene::Impl::~Impl() {
	if (!ctx) {
		return;
	}

	auto freeVal = [this](JSValue &val) {
		if (!JS_IsNull(val)) {
			JS_FreeValue(ctx, val);
			val = JS_NULL;
		}
	};

	freeVal(setup_obj);
	freeVal(size_fn);
	freeVal(setup_fn);
	freeVal(step_fn);
	freeVal(render_fn);
	freeVal(cmds_val);

	scriptEngine().destroyContext(ctx);
}

ScriptedScene::ScriptedScene(const std::string &script_id)
	: _impl(std::make_unique<Impl>(script_id)) {}

ScriptedScene::~ScriptedScene() = default;

std::array<int, 2> ScriptedScene::size() const {
	return {_impl->width, _impl->height};
}

void ScriptedScene::setup(SceneManager &mgr) {
	if (JS_IsFunction(_impl->ctx, _impl->setup_fn)) {
		js::ValueGuard result_guard(
			_impl->ctx,
			JS_Call(_impl->ctx, _impl->setup_fn, _impl->setup_obj, 0, nullptr)
		);
		JSValue result = result_guard.get();
		if (JS_IsException(result)) {
			js::ValueGuard exc_guard(_impl->ctx, JS_GetException(_impl->ctx));
			const char *str = JS_ToCString(_impl->ctx, exc_guard.get());
			std::cerr << "[JS] setup error: " << (str ? str : "unknown")
					  << "\n";
			JS_FreeCString(_impl->ctx, str);
		}
	}
}

void ScriptedScene::handleEvent(SceneManager &mgr, sf::Event &evt) {}

void ScriptedScene::step(SceneManager &mgr) {
	if (JS_IsFunction(_impl->ctx, _impl->step_fn)) {
		js::ValueGuard result_guard(
			_impl->ctx,
			JS_Call(_impl->ctx, _impl->step_fn, _impl->setup_obj, 0, nullptr)
		);
		JSValue result = result_guard.get();
		if (JS_IsException(result)) {
			js::ValueGuard exc_guard(_impl->ctx, JS_GetException(_impl->ctx));
			const char *str = JS_ToCString(_impl->ctx, exc_guard.get());
			std::cerr << "[JS] step error: " << (str ? str : "unknown") << "\n";
			JS_FreeCString(_impl->ctx, str);
		}
	}
}

void ScriptedScene::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	if (JS_IsFunction(_impl->ctx, _impl->render_fn)) {
		js::ValueGuard result_guard(
			_impl->ctx,
			JS_Call(_impl->ctx, _impl->render_fn, _impl->setup_obj, 0, nullptr)
		);
		JSValue result = result_guard.get();
		if (JS_IsException(result)) {
			js::ValueGuard exc_guard(_impl->ctx, JS_GetException(_impl->ctx));
			const char *str = JS_ToCString(_impl->ctx, exc_guard.get());
			std::cerr << "[JS] render error: " << (str ? str : "unknown")
					  << "\n";
			JS_FreeCString(_impl->ctx, str);
		}
	}

	if (_impl->cmds) {
		_impl->cmds->render(target, scale);
	}
}

} // namespace wf
