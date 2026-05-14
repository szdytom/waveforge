#include "wforge/layout.h"
#include "wforge/runtime.h"
#include "wforge/scene.h"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <format>
#include <iostream>
#include <tuple>
#include <utility>

namespace wf {

namespace {

enum class CallbackIdx : size_t {
	STEP = 0,
	KEY,
	MOUSEBUTTON,
	MOUSEMOVE,
	COUNT
};

CallbackIdx callbackIdx(std::string_view type) noexcept {
	if (type == "step") {
		return CallbackIdx::STEP;
	}
	if (type == "key") {
		return CallbackIdx::KEY;
	}
	if (type == "mousebutton") {
		return CallbackIdx::MOUSEBUTTON;
	}
	if (type == "mousemove") {
		return CallbackIdx::MOUSEMOVE;
	}
	return CallbackIdx::COUNT;
}

js::Engine &scriptEngine() {
	static std::unique_ptr<js::Engine> engine;
	if (!engine) {
		engine = std::make_unique<js::Engine>();
	}
	return *engine;
}

} // namespace

struct ScriptedScene::Impl {
	const Script *script = nullptr; // not owned, managed by AssetsManager

	int width = 0;
	int height = 0;
	SceneManager *scene_mgr = nullptr; // not owned

	js::ContextPtr ctx;
	std::array<std::vector<js::Value>, std::to_underlying(CallbackIdx::COUNT)>
		callbacks;

	js::Value cmds_val;
	js::DrawCmdList *cmds = nullptr;

	js::Value layout_root_val;
	js::LayoutNode *layout_root = nullptr;

	explicit Impl(const std::string &script_id);
};

namespace {

JSValue f_log(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	std::string msg;
	for (int i = 0; i < argc; i++) {
		if (i > 0) {
			msg += ' ';
		}
		const char *str = JS_ToCString(ctx, argv[i]);
		msg += str ? str : "null";
		JS_FreeCString(ctx, str);
	}
	std::cerr << "[JS] " << msg << "\n";
	return JS_UNDEFINED;
}

std::expected<CallbackIdx, JSValue> parseEventType(
	JSContext *ctx, JSValueConst arg
) noexcept {
	size_t type_len;
	const char *type_cstr = JS_ToCStringLen(ctx, &type_len, arg);
	if (!type_cstr) {
		return std::unexpected(
			JS_ThrowTypeError(ctx, "Event type must be a string")
		);
	}

	auto type = callbackIdx({type_cstr, type_len});
	JS_FreeCString(ctx, type_cstr);

	if (type == CallbackIdx::COUNT) {
		return std::unexpected(JS_ThrowTypeError(
			ctx, "Unknown event type (expected step/key/mousebutton/mousemove)"
		));
	}

	return type;
}

// ── waveforge functions ──

JSValue f_set_window_title(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = static_cast<ScriptedScene::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl || !impl->scene_mgr) {
		return JS_UNDEFINED;
	}
	const char *title = JS_ToCString(ctx, argv[0]);
	if (!title) {
		return JS_ThrowTypeError(ctx, "Title must be a string");
	}
	impl->scene_mgr->setWindowTitle(title);
	JS_FreeCString(ctx, title);
	return JS_UNDEFINED;
}

JSValue f_addEventListener(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = static_cast<ScriptedScene::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_UNDEFINED;
	}

	auto parsed = parseEventType(ctx, argv[0]);
	if (!parsed) {
		return parsed.error();
	}

	if (!JS_IsFunction(ctx, argv[1])) {
		return JS_ThrowTypeError(ctx, "Callback must be a function");
	}

	impl->callbacks[std::to_underlying(*parsed)].push_back(
		js::Value(ctx, JS_DupValue(ctx, argv[1]))
	);
	return JS_UNDEFINED;
}

JSValue f_removeEventListener(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = static_cast<ScriptedScene::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_UNDEFINED;
	}

	auto parsed = parseEventType(ctx, argv[0]);
	if (!parsed) {
		return parsed.error();
	}

	auto &vec = impl->callbacks[std::to_underlying(*parsed)];
	for (size_t i = 0; i < vec.size(); i++) {
		if (JS_IsSameValue(ctx, *vec[i], argv[1])) {
			vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(i));
			break;
		}
	}
	return JS_UNDEFINED;
}

JSValue f_commitLayout(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = static_cast<ScriptedScene::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_UNDEFINED;
	}

	auto *root = js::LayoutNode::unwrap(ctx, argv[0]);
	if (!root) {
		return JS_ThrowTypeError(ctx, "commitLayout expects a LayoutNode");
	}

	impl->layout_root_val = js::Value(ctx, JS_DupValue(ctx, argv[0]));
	impl->layout_root = root;

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

// ── Event conversion ──

std::tuple<CallbackIdx, JSValue> createJSEvent(
	JSContext *ctx, const sf::Event &evt, int scale
) noexcept {
	if (const auto *e = evt.getIf<sf::Event::KeyPressed>()) {
		return {CallbackIdx::KEY, js::KeyEvent::from(ctx, *e)};
	}
	if (const auto *e = evt.getIf<sf::Event::KeyReleased>()) {
		return {CallbackIdx::KEY, js::KeyEvent::from(ctx, *e)};
	}
	if (const auto *e = evt.getIf<sf::Event::MouseButtonPressed>()) {
		return {
			CallbackIdx::MOUSEBUTTON, js::MouseButtonEvent::from(ctx, *e, scale)
		};
	}
	if (const auto *e = evt.getIf<sf::Event::MouseButtonReleased>()) {
		return {
			CallbackIdx::MOUSEBUTTON, js::MouseButtonEvent::from(ctx, *e, scale)
		};
	}
	if (const auto *e = evt.getIf<sf::Event::MouseMoved>()) {
		return {
			CallbackIdx::MOUSEMOVE, js::MouseMoveEvent::from(ctx, *e, scale)
		};
	}
	return {CallbackIdx::COUNT, JS_NULL};
}

void invokeCallbacks(
	JSContext *ctx, const std::vector<js::Value> &callbacks, int argc,
	JSValueConst *argv
) noexcept {
	std::vector<js::Value> snapshot;
	snapshot.reserve(callbacks.size());
	for (auto &cb : callbacks) {
		snapshot.push_back(cb.dup());
	}
	for (auto &cb : snapshot) {
		if (JS_IsFunction(ctx, *cb)) {
			js::Value result_guard(
				ctx, JS_Call(ctx, *cb, JS_UNDEFINED, argc, argv)
			);
			if (JS_IsException(*result_guard)) {
				js::dumpJSError(ctx);
			}
		}
	}
}

void drainJSPromises(JSContext *ctx) noexcept {
	JSRuntime *rt = JS_GetRuntime(ctx);
	while (JS_IsJobPending(rt)) {
		JSContext *ctx1 = nullptr;
		int ret = JS_ExecutePendingJob(rt, &ctx1);
		if (ret < 0 && ctx1) {
			js::dumpJSError(ctx1);
		}
	}
}

} // namespace

using SceneBindings = js::BindingList<
	js::Texture, js::Color, js::TextContent, js::SpriteContent, js::DrawTextCmd,
	js::DrawSpriteCmd, js::DrawRectCmd, js::DrawCmdList, js::DrawCmdListIter,
	js::LayoutNode, js::KeyEvent, js::MouseButtonEvent, js::MouseMoveEvent>;

// ── Impl constructor / destructor ──

ScriptedScene::Impl::Impl(const std::string &script_id): script(nullptr) {
	script = AssetsManager::instance().getAssetChecked<Script>(script_id);
	if (!script) {
		throw std::runtime_error(
			std::format("ScriptedScene: script '{}' not found", script_id)
		);
	}
	width = script->width;
	height = script->height;
}

ScriptedScene::ScriptedScene(const std::string &script_id)
	: _impl(std::make_unique<Impl>(script_id)) {}

ScriptedScene::~ScriptedScene() = default;

// ── SceneFacade interface ──

std::array<int, 2> ScriptedScene::size() const {
	return {_impl->width, _impl->height};
}

void ScriptedScene::setup(SceneManager &mgr) {
	auto &impl = *_impl;
	impl.scene_mgr = &mgr;

	auto &engine = scriptEngine();
	static bool classes_registered = false;
	if (!classes_registered) {
		SceneBindings::registerClass(engine);
		classes_registered = true;
	}

	auto *raw_ctx = engine.createContext();
	engine.releaseContext(raw_ctx);
	impl.ctx.reset(raw_ctx);

	auto *ctx = impl.ctx.get();
	JS_SetContextOpaque(ctx, &impl);

	JSValue ns = JS_NewObject(ctx);
	SceneBindings::bindContext(ctx, ns);

	JS_DefinePropertyValueStr(
		ctx, ns, "width", JS_NewInt32(ctx, impl.width), JS_PROP_CONFIGURABLE
	);
	JS_DefinePropertyValueStr(
		ctx, ns, "height", JS_NewInt32(ctx, impl.height), JS_PROP_CONFIGURABLE
	);

	JS_SetPropertyStr(ctx, ns, "log", JS_NewCFunction(ctx, f_log, "log", 1));
	JS_SetPropertyStr(
		ctx, ns, "setTitle",
		JS_NewCFunction(ctx, f_set_window_title, "setTitle", 1)
	);
	JS_SetPropertyStr(
		ctx, ns, "addEventListener",
		JS_NewCFunction(ctx, f_addEventListener, "addEventListener", 2)
	);
	JS_SetPropertyStr(
		ctx, ns, "removeEventListener",
		JS_NewCFunction(ctx, f_removeEventListener, "removeEventListener", 2)
	);
	JS_SetPropertyStr(
		ctx, ns, "commitDraw",
		JS_NewCFunction(ctx, f_commitDraw, "commitDraw", 1)
	);
	JS_SetPropertyStr(
		ctx, ns, "commitLayout",
		JS_NewCFunction(ctx, f_commitLayout, "commitLayout", 1)
	);

	js::Value global(ctx, JS_GetGlobalObject(ctx));
	JS_SetPropertyStr(ctx, *global, "waveforge", ns);

	js::Value eval_guard(
		ctx,
		JS_Eval(
			ctx, impl.script->source.c_str(), impl.script->source.size(),
			impl.script->filename.c_str(), JS_EVAL_TYPE_GLOBAL
		)
	);
	JSValue result = *eval_guard;

	if (JS_IsException(result)) {
		js::dumpJSError(ctx);
		throw std::runtime_error(
			std::format(
				"ScriptedScene: failed to evaluate '{}'", impl.script->filename
			)
		);
	}
}

void ScriptedScene::handleEvent(SceneManager &mgr, const sf::Event &evt) {
	auto *ctx = _impl->ctx.get();

	auto [evtType, raw_val] = createJSEvent(ctx, evt, mgr.scale());
	js::Value evt_guard(ctx, raw_val);
	if (evtType == CallbackIdx::COUNT) {
		return;
	}

	auto &callbacks = _impl->callbacks[std::to_underlying(evtType)];
	JSValue event_val = *evt_guard;
	invokeCallbacks(ctx, callbacks, 1, &event_val);
	drainJSPromises(ctx);
}

void ScriptedScene::step(SceneManager &mgr) {
	invokeCallbacks(
		_impl->ctx.get(),
		_impl->callbacks[std::to_underlying(CallbackIdx::STEP)], 0, nullptr
	);
	drainJSPromises(_impl->ctx.get());
}

void ScriptedScene::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	if (_impl->layout_root) {
		_impl->layout_root->calculateLayout(
			static_cast<float>(_impl->width), static_cast<float>(_impl->height)
		);
		_impl->layout_root->render(target, _impl->ctx.get(), scale);
	}

	if (_impl->cmds) {
		_impl->cmds->render(target, _impl->ctx.get(), scale);
	}

	_impl->cmds_val = js::Value();
	_impl->cmds = nullptr;
	_impl->layout_root_val = js::Value();
	_impl->layout_root = nullptr;
}

} // namespace wf
