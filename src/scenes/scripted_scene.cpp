#include "hacks.h"
#include "wforge/layout.h"
#include "wforge/router.h"
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

	js::EngineContext engineCtx;
	std::array<std::vector<js::Value>, std::to_underlying(CallbackIdx::COUNT)>
		callbacks;

	js::Value cmds_val;
	js::DrawCmdList *cmds = nullptr;

	js::Value layout_root_val;
	js::LayoutNode *layout_root = nullptr;

	std::string route_data;

	// For lifetime management only, no direct access to the array later
	std::unique_ptr<const JSCFunctionListEntry[]> waveforge_props;

	Impl(const std::string &script_id, std::string_view route_data = "");
};

namespace {

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

// ── Impl accessor ──

ScriptedScene::Impl *getImpl(JSContext *ctx) noexcept {
	return js::EngineContext::opaqueFrom<ScriptedScene::Impl>(ctx);
}

// ── waveforge functions ──

JSValue f_setWindowTitle(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = getImpl(ctx);
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
	auto *impl = getImpl(ctx);
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
	auto *impl = getImpl(ctx);
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
	auto *impl = getImpl(ctx);
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
	auto *impl = getImpl(ctx);
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

std::string toString(JSContext *ctx, JSValueConst val) noexcept {
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);
	if (!str) {
		return {};
	}
	std::string result(str, len);
	JS_FreeCString(ctx, str);
	return result;
}

JSValue f_navigateTo(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *impl = getImpl(ctx);
	if (!impl || !impl->scene_mgr) {
		return JS_UNDEFINED;
	}

	// Extract route ID
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "navigateTo: route id must be a string");
	}
	std::string id = toString(ctx, argv[0]);

	// Extract optional data string
	std::string data;
	if (argc > 1 && JS_IsString(argv[1])) {
		data = toString(ctx, argv[1]);
	}

	// Look up route and create scene (may throw JS TypeError if unknown)
	Scene scene;
	try {
		scene = SceneRouter::instance().create(id, data);
	} catch (const std::exception &e) {
		return JS_ThrowTypeError(ctx, "%s", e.what());
	}

	// NOTE: after changeScene, the current ScriptedScene (and its JS context)
	// is destroyed. Do not touch any JS values after this call.
	impl->scene_mgr->changeScene(std::move(scene));

	return JS_UNDEFINED;
}

JSValue get_routeData(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *impl = getImpl(ctx);
	if (!impl || impl->route_data.empty()) {
		return JS_NULL;
	}
	return JS_NewStringLen(
		ctx, impl->route_data.data(), impl->route_data.size()
	);
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

} // namespace

using SceneBindings = js::BindingList<
	js::Texture, js::Color, js::TextContent, js::SpriteContent, js::DrawTextCmd,
	js::DrawSpriteCmd, js::DrawRectCmd, js::DrawCmdList, js::DrawCmdListIter,
	js::LayoutNode, js::KeyEvent, js::MouseButtonEvent, js::MouseMoveEvent>;

// ── Impl constructor / destructor ──

ScriptedScene::Impl::Impl(
	const std::string &script_id, std::string_view route_data
)
	: script(nullptr) {
	script = AssetsManager::instance().getAssetChecked<Script>(script_id);
	if (!script) {
		throw std::runtime_error(
			std::format("ScriptedScene: script '{}' not found", script_id)
		);
	}
	width = script->width;
	height = script->height;
	this->route_data = route_data;
}

ScriptedScene::ScriptedScene(
	const std::string &script_id, std::string_view route_data
)
	: _impl(std::make_unique<Impl>(script_id, route_data)) {}

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

	impl.engineCtx = engine.createContext();
	auto *ctx = impl.engineCtx.ctx();
	impl.engineCtx.setOpaque<ScriptedScene::Impl>(&impl);
	impl.engineCtx.bindTimerGlobals();

	// Helper: copies a stack array to heap (stack lifetime is unsafe for
	// QuickJS lazy-init which stores a raw pointer to each entry).
	auto make_unique_array = []<typename T, std::size_t N>(const T(&arr)[N])
		-> std::unique_ptr<T[]> {
		auto ptr = std::make_unique_for_overwrite<T[]>(N);
		std::copy_n(arr, N, ptr.get());
		return ptr;
	};

	JSValue ns = JS_NewObject(ctx);
	SceneBindings::bindContext(ctx, ns);

	const JSCFunctionListEntry WAVEFORGE_PROPS[] = {
		js::cFuncDef("setTitle", 1, f_setWindowTitle),
		js::cFuncDef("addEventListener", 2, f_addEventListener),
		js::cFuncDef("removeEventListener", 2, f_removeEventListener),
		js::cFuncDef("commitDraw", 1, f_commitDraw),
		js::cFuncDef("commitLayout", 1, f_commitLayout),
		js::cFuncDef("navigateTo", 2, f_navigateTo),
		js::cGetSetDef("routeData", get_routeData, nullptr),
		js::propInt32Def("width", impl.width, JS_PROP_CONFIGURABLE),
		js::propInt32Def("height", impl.height, JS_PROP_CONFIGURABLE),
	};

	impl.waveforge_props = make_unique_array(WAVEFORGE_PROPS);

	JS_SetPropertyFunctionList(
		ctx, ns, impl.waveforge_props.get(), std::size(WAVEFORGE_PROPS)
	);

	js::Value global(ctx, JS_GetGlobalObject(ctx));
	JS_SetPropertyStr(ctx, *global, "waveforge", ns);

	// Look up source: ModuleRegistry first (ES module), then Script (global
	// fallback)
	auto &registry = js::ModuleRegistry::instance();
	const std::string *module_source = registry.find(impl.script->filename);
	const std::string *source = module_source
		? module_source
		: &impl.script->source;
	int eval_flags = module_source ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;

	js::Value eval_guard(
		ctx,
		JS_Eval(
			ctx, source->c_str(), source->size(), impl.script->filename.c_str(),
			eval_flags
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
	auto *ctx = _impl->engineCtx.ctx();

	auto [evtType, raw_val] = createJSEvent(ctx, evt, mgr.scale());
	js::Value evt_guard(ctx, raw_val);
	if (evtType == CallbackIdx::COUNT) {
		return;
	}

	auto &callbacks = _impl->callbacks[std::to_underlying(evtType)];
	JSValue event_val = *evt_guard;
	invokeCallbacks(ctx, callbacks, 1, &event_val);
	_impl->engineCtx.drainPromises();
}

void ScriptedScene::step(SceneManager &mgr) {
	_impl->engineCtx.processTimers();
	invokeCallbacks(
		_impl->engineCtx.ctx(),
		_impl->callbacks[std::to_underlying(CallbackIdx::STEP)], 0, nullptr
	);
	_impl->engineCtx.drainPromises();
}

void ScriptedScene::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	if (_impl->layout_root) {
		_impl->layout_root->calculateLayout(
			static_cast<float>(_impl->width), static_cast<float>(_impl->height)
		);
		_impl->layout_root->render(target, _impl->engineCtx.ctx(), scale);
	}

	if (_impl->cmds) {
		_impl->cmds->render(target, _impl->engineCtx.ctx(), scale);
	}

	_impl->cmds_val = js::Value();
	_impl->cmds = nullptr;
	_impl->layout_root_val = js::Value();
	_impl->layout_root = nullptr;
}

} // namespace wf
