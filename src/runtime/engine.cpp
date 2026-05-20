#include "hacks.h"
#include "wforge/runtime.h"
#include <chrono>
#include <cpptrace/cpptrace.hpp>
#include <iostream>
#include <utility>

namespace wf::js {

void dumpJSError(JSContext *ctx) {
	JSValue exception_val = JS_GetException(ctx);
	const char *str = JS_ToCString(ctx, exception_val);
	if (str) {
		fprintf(stderr, "%s\n", str);
		JS_FreeCString(ctx, str);
	}
	if (JS_IsError(exception_val)) {
		JSValue stack = JS_GetPropertyStr(ctx, exception_val, "stack");
		if (!JS_IsUndefined(stack)) {
			str = JS_ToCString(ctx, stack);
			if (str) {
				fprintf(stderr, "%s\n", str);
				JS_FreeCString(ctx, str);
			}
			JS_FreeValue(ctx, stack);
		}
	}
	JS_FreeValue(ctx, exception_val);
}

void RuntimeDeleter::operator()(JSRuntime *rt) const noexcept {
	if (rt) {
		JS_FreeRuntime(rt);
	}
}

// ── Internal context deleter (only used by EngineContext) ──

namespace {

struct ContextDeleter {
	void operator()(JSContext *ctx) const noexcept {
		if (ctx) {
			JS_FreeContext(ctx);
		}
	}
};
using ContextPtr = std::unique_ptr<JSContext, ContextDeleter>;

} // namespace

// ── Engine ──

Engine::Engine(): _runtime(JS_NewRuntime()) {
	if (!_runtime) {
		throw std::runtime_error("Failed to create QuickJS runtime");
	}
	JS_SetRuntimeOpaque(_runtime.get(), this);

	// Register ES module loader (normalizer + loader + attributes checker).
	// Callbacks use ModuleRegistry::instance() as opaque; the registry must
	// be populated (via loadFromMetafile) before any module import triggers.
	JS_SetModuleLoaderFunc2(
		_runtime.get(), moduleNormalizer, moduleLoader, moduleCheckAttributes,
		&ModuleRegistry::instance()
	);
}

EngineContext Engine::createContext() {
	return EngineContext(_runtime.get());
}

// ── EngineContext::Impl ──

struct EngineContext::Impl {
	ContextPtr ctx;
	OpaqueEntry opaque;

	// Timer
	struct TimerEntry {
		uint64_t fireAt;
		JSValue callback;          // owned via manual JS_DupValue/JS_FreeValue
		std::vector<JSValue> args; // extra args forwarded to callback
		uint32_t id;
		uint32_t intervalMs;
		bool isInterval;
	};
	std::vector<TimerEntry> timers;
	uint32_t nextTimerId = 1;
	std::chrono::steady_clock::time_point epoch;

	explicit Impl(JSRuntime *rt)
		: ctx(JS_NewContext(rt)), epoch(std::chrono::steady_clock::now()) {
		if (!ctx) {
			throw std::runtime_error("Failed to create QuickJS context");
		}
		JS_SetContextOpaque(ctx.get(), this);
	}

	~Impl() {
		auto *c = ctx.get();
		for (auto &t : timers) {
			if (!JS_IsUndefined(t.callback)) {
				JS_FreeValue(c, t.callback);
			}
			for (auto &arg : t.args) {
				JS_FreeValue(c, arg);
			}
		}
	}

	Impl &operator=(const Impl &) = delete;

	// Timer API (called directly by native timer functions)
	uint32_t setTimeout(
		JSValueConst callback, uint32_t delayMs, int numArgs, JSValueConst *args
	) {
		auto *c = ctx.get();
		uint32_t id = nextTimerId++;
		uint64_t fireAt = nowMs() + delayMs;
		TimerEntry entry = {fireAt, JS_DupValue(c, callback), {}, id, 0, false};
		for (int i = 0; i < numArgs; i++) {
			entry.args.push_back(JS_DupValue(c, args[i]));
		}
		timers.push_back(std::move(entry));
		return id;
	}

	void clearTimeout(uint32_t id) {
		auto *c = ctx.get();
		for (auto &t : timers) {
			if (t.id == id) {
				JS_FreeValue(c, t.callback);
				t.callback = JS_UNDEFINED;
				for (auto &arg : t.args) {
					JS_FreeValue(c, arg);
				}
				t.args.clear();
				break;
			}
		}
	}

	uint32_t setInterval(
		JSValueConst callback, uint32_t intervalMs, int numArgs,
		JSValueConst *args
	) {
		auto *c = ctx.get();
		uint32_t id = nextTimerId++;
		uint64_t fireAt = nowMs() + intervalMs;
		TimerEntry entry = {
			fireAt, JS_DupValue(c, callback), {}, id, intervalMs, true
		};
		for (int i = 0; i < numArgs; i++) {
			entry.args.push_back(JS_DupValue(c, args[i]));
		}
		timers.push_back(std::move(entry));
		return id;
	}

	void processTimers() {
		auto *c = ctx.get();
		auto now = nowMs();

		std::vector<uint32_t> due;
		for (size_t i = 0; i < timers.size(); i++) {
			auto &t = timers[i];
			if (!JS_IsUndefined(t.callback) && now >= t.fireAt) {
				due.push_back(t.id);
			}
		}

		for (auto id : due) {
			auto it = std::find_if(
				timers.begin(), timers.end(), [id](const auto &t) {
				return t.id == id && !JS_IsUndefined(t.callback);
			}
			);
			if (it == timers.end()) {
				continue;
			}

			auto idx = static_cast<size_t>(it - timers.begin());
			bool isInterval = it->isInterval;
			uint32_t intervalMs = it->intervalMs;

			JSValue ret = JS_Call(
				c, timers[idx].callback, JS_UNDEFINED,
				static_cast<int>(timers[idx].args.size()),
				timers[idx].args.data()
			);
			if (JS_IsException(ret)) {
				dumpJSError(c);
			}
			JS_FreeValue(c, ret);

			if (isInterval) {
				timers[idx].fireAt = nowMs() + intervalMs;
			} else {
				JS_FreeValue(c, timers[idx].callback);
				timers[idx].callback = JS_UNDEFINED;
				for (auto &arg : timers[idx].args) {
					JS_FreeValue(c, arg);
				}
				timers[idx].args.clear();
			}
		}

		std::erase_if(timers, [](const auto &t) {
			return JS_IsUndefined(t.callback);
		});
	}

	void drainPromises() {
		auto *c = ctx.get();
		auto *rt = JS_GetRuntime(c);
		while (JS_IsJobPending(rt)) {
			JSContext *ctx1 = nullptr;
			int ret = JS_ExecutePendingJob(rt, &ctx1);
			if (ret < 0 && ctx1) {
				dumpJSError(ctx1);
			}
		}
	}

	[[nodiscard]] uint64_t nowMs() const noexcept {
		auto elapsed = std::chrono::steady_clock::now() - epoch;
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
				.count()
		);
	}
};

// ── EngineContext ──

EngineContext::EngineContext(JSRuntime *rt)
	: _impl(std::make_unique<Impl>(rt)) {}

EngineContext::~EngineContext() = default;

EngineContext::EngineContext() noexcept = default;

EngineContext::EngineContext(EngineContext &&other) noexcept
	: _impl(std::move(other._impl)) {}

EngineContext &EngineContext::operator=(EngineContext &&other) noexcept {
	if (this != &other) {
		_impl = std::move(other._impl);
	}
	return *this;
}

JSContext *EngineContext::ctx() const noexcept {
	return _impl ? _impl->ctx.get() : nullptr;
}

EngineContext::operator bool() const noexcept {
	return _impl != nullptr;
}

void EngineContext::processTimers() {
	_impl->processTimers();
}

void EngineContext::drainPromises() {
	_impl->drainPromises();
}

void EngineContext::_setOpaque(void *ptr, std::size_t typeHash) noexcept {
	_impl->opaque = {ptr, typeHash};
}

EngineContext::OpaqueEntry EngineContext::_opaqueFrom(JSContext *ctx) noexcept {
	auto *impl = static_cast<Impl *>(JS_GetContextOpaque(ctx));
	return impl->opaque;
}

// ── Native timer functions ──

namespace {

JSValue global_setTimeout(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (!JS_IsFunction(ctx, argv[0])) {
		// setTimeout(code, delay) is valid JS
		// but discouraged and not worth supporting
		return JS_ThrowTypeError(
			ctx, "code argument to setTimeout not supported"
		);
	}

	uint32_t delayMs = 0;
	if (argc > 1) {
		if (JS_ToUint32(ctx, &delayMs, argv[1]) < 0) {
			return JS_ThrowTypeError(ctx, "Invalid delay value for setTimeout");
		}
	}
	auto *impl = static_cast<EngineContext::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_ThrowTypeError(ctx, "Internal error: no EngineContext");
	}
	int numArgs = argc > 2 ? argc - 2 : 0;
	uint32_t id = impl->setTimeout(argv[0], delayMs, numArgs, argv + 2);
	return JS_NewUint32(ctx, id);
}

JSValue global_clearTimeout(
	JSContext *ctx, JSValueConst, int, JSValueConst *argv
) noexcept {
	uint32_t id;
	if (JS_ToUint32(ctx, &id, argv[0]) < 0) {
		return JS_ThrowTypeError(ctx, "Invalid timeout ID for clearTimeout");
	}
	auto *impl = static_cast<EngineContext::Impl *>(JS_GetContextOpaque(ctx));
	if (impl) {
		impl->clearTimeout(id);
	}
	return JS_UNDEFINED;
}

JSValue global_setInterval(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (!JS_IsFunction(ctx, argv[0])) {
		// setInterval(code) is valid JS
		// but discouraged and not worth supporting
		return JS_ThrowTypeError(
			ctx, "code argument to setInterval not supported"
		);
	}

	uint32_t intervalMs = 0;
	if (argc > 1) {
		if (JS_ToUint32(ctx, &intervalMs, argv[1]) < 0) {
			return JS_ThrowTypeError(
				ctx, "Invalid interval value for setInterval"
			);
		}
	}
	auto *impl = static_cast<EngineContext::Impl *>(JS_GetContextOpaque(ctx));
	if (!impl) {
		return JS_ThrowTypeError(ctx, "Internal error: no EngineContext");
	}
	int numArgs = argc > 2 ? argc - 2 : 0;
	uint32_t id = impl->setInterval(argv[0], intervalMs, numArgs, argv + 2);
	return JS_NewUint32(ctx, id);
}

JSValue global_clearInterval(
	JSContext *ctx, JSValueConst, int, JSValueConst *argv
) noexcept {
	uint32_t id;
	if (JS_ToUint32(ctx, &id, argv[0]) < 0) {
		return JS_ThrowTypeError(ctx, "Invalid interval ID for clearInterval");
	}

	auto *impl = static_cast<EngineContext::Impl *>(JS_GetContextOpaque(ctx));
	if (impl) {
		impl->clearTimeout(id);
	}
	return JS_UNDEFINED;
}

JSValue f_consoleLog(
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

} // namespace

void EngineContext::bindTimerGlobals() {
	auto *ctx = _impl->ctx.get();
	JSValue global = JS_GetGlobalObject(ctx);

	// Lifetime is program-wide — all function pointers are compile-time
	// constants, no runtime values.
	static const JSCFunctionListEntry TIMER_FUNCS[] = {
		cFuncDef("setTimeout", 1, global_setTimeout),
		cFuncDef("clearTimeout", 1, global_clearTimeout),
		cFuncDef("setInterval", 1, global_setInterval),
		cFuncDef("clearInterval", 1, global_clearInterval),
	};
	JS_SetPropertyFunctionList(
		ctx, global, TIMER_FUNCS, std::size(TIMER_FUNCS)
	);

	// Set process.env.NODE_ENV for React / bundler conventions.
	// JS_SetPropertyStr / JS_DefinePropertyValueStr consume the value ref,
	// so always dup() to keep a local handle.
	JSValue process_obj = JS_GetPropertyStr(ctx, global, "process");
	if (JS_IsUndefined(process_obj)) {
		process_obj = JS_NewObject(ctx);
		JS_SetPropertyStr(
			ctx, global, "process", JS_DupValue(ctx, process_obj)
		);
	} // else: process_obj has +1 ref from GetPropertyStr, needs FreeValue

	JSValue env_obj = JS_GetPropertyStr(ctx, process_obj, "env");
	if (JS_IsUndefined(env_obj)) {
		env_obj = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, process_obj, "env", JS_DupValue(ctx, env_obj));
	} // else: env_obj has +1 ref from GetPropertyStr, needs FreeValue

#ifndef NDEBUG
	JS_SetPropertyStr(
		ctx, env_obj, "NODE_ENV", JS_NewString(ctx, "development")
	);
#else
	JS_SetPropertyStr(
		ctx, env_obj, "NODE_ENV", JS_NewString(ctx, "production")
	);
#endif

	JS_FreeValue(ctx, env_obj);
	JS_FreeValue(ctx, process_obj);

	// Set up console.log / console.warn
	JSValue console_obj = JS_GetPropertyStr(ctx, global, "console");
	if (JS_IsUndefined(console_obj)) {
		console_obj = JS_NewObject(ctx);
		JS_SetPropertyStr(
			ctx, global, "console", JS_DupValue(ctx, console_obj)
		);
	}
	static const JSCFunctionListEntry CONSOLE_FUNCS[] = {
		cFuncDef("log", 1, f_consoleLog),
		cFuncDef("warn", 1, f_consoleLog),
	};
	JS_SetPropertyFunctionList(
		ctx, console_obj, CONSOLE_FUNCS, std::size(CONSOLE_FUNCS)
	);
	JS_FreeValue(ctx, console_obj);

	JS_FreeValue(ctx, global);
}

// ── Value RAII, bindContextImpl, bindContextImplNoCtor ──

void bindContextImpl(
	JSContext *ctx, JSValueConst ns, const char *class_name, int class_id,
	JSCFunction *ctor_func, int ctor_length, CFunctionList proto_fields,
	CFunctionList ctor_fields, JSValue parent_ctor
) {
	JSValue parent_proto;

	if (JS_IsUndefined(parent_ctor)) {
		parent_ctor = JS_GetFunctionProto(ctx);
		parent_proto = getObjectProto(ctx);
	} else {
		parent_proto = JS_GetPropertyStr(ctx, parent_ctor, "prototype");
	}

	Value parent_ctor_guard(ctx, parent_ctor);
	Value parent_proto_guard(ctx, parent_proto);

	JSValue proto = JS_NewObjectProtoClass(ctx, parent_proto, CLASS_ID_OBJECT);
	if (JS_IsException(proto)) {
		throw std::runtime_error(
			std::format("Failed to create prototype for class '{}'", class_name)
		);
	}
	Value proto_guard(ctx, proto);

	if (JS_SetPropertyFunctionList(
			ctx, proto, proto_fields.data(), proto_fields.size()
		)) {
		throw std::runtime_error(
			std::format(
				"Failed to set prototype properties for class '{}'", class_name
			)
		);
	}

	JSValue ctor = JS_NewCFunction3(
		ctx, ctor_func, class_name, ctor_length, JS_CFUNC_constructor, 0,
		parent_ctor, ctor_fields.size() + 3
	);
	Value ctor_guard(ctx, ctor);

	if (JS_IsException(ctor)) {
		throw std::runtime_error(
			std::format(
				"Failed to create constructor for class '{}'", class_name
			)
		);
	}

	if (JS_SetPropertyFunctionList(
			ctx, ctor, ctor_fields.data(), ctor_fields.size()
		)) {
		throw std::runtime_error(
			std::format(
				"Failed to set constructor properties for class '{}'",
				class_name
			)
		);
	}

	if (JS_SetConstructor(ctx, ctor, proto)) {
		throw std::runtime_error(
			std::format(
				"Failed to link constructor and prototype for class '{}'",
				class_name
			)
		);
	}

	// JS_SetClassProto takes ownership of proto
	JS_SetClassProto(ctx, class_id, proto);
	proto_guard.release();

	if (!JS_IsUndefined(ns)) {
		if (JS_DefinePropertyValueStr(
				ctx, ns, class_name, JS_DupValue(ctx, ctor),
				JS_PROP_CONFIGURABLE
			)
		    < 0) {
			throw std::runtime_error(
				std::format(
					"Failed to define constructor in namespace for class '{}'",
					class_name
				)
			);
		}
	}
}

void bindContextImplNoCtor(
	JSContext *ctx, const char *class_name, int class_id, JSValue parent_proto,
	CFunctionList fields
) {
	if (JS_IsUndefined(parent_proto)) {
		parent_proto = getObjectProto(ctx);
	}

	Value parent_proto_guard(ctx, parent_proto);

	JSValue proto = JS_NewObjectProtoClass(ctx, parent_proto, CLASS_ID_OBJECT);
	if (JS_IsException(proto)) {
		throw std::runtime_error(
			std::format("Failed to create prototype for class '{}'", class_name)
		);
	}
	Value proto_guard(ctx, proto);

	if (JS_SetPropertyFunctionList(ctx, proto, fields.data(), fields.size())) {
		throw std::runtime_error(
			std::format(
				"Failed to set prototype properties for class '{}'", class_name
			)
		);
	}

	// JS_SetClassProto takes ownership of proto
	JS_SetClassProto(ctx, class_id, proto);
	proto_guard.release();
}

// ── Value ──

Value::Value() noexcept: _ctx(nullptr), _value(JS_UNDEFINED) {}

Value::Value(JSContext *ctx, JSValue value) noexcept
	: _ctx(ctx), _value(value) {}

Value::~Value() noexcept {
	if (_ctx) {
		JS_FreeValue(_ctx, _value);
	}
}

Value::Value(Value &&other) noexcept
	: _ctx(std::exchange(other._ctx, nullptr))
	, _value(std::exchange(other._value, JS_UNDEFINED)) {}

Value &Value::operator=(Value &&other) noexcept {
	if (this != &other) {
		if (_ctx) {
			JS_FreeValue(_ctx, _value);
		}
		_ctx = other._ctx;
		_value = other._value;
		other._ctx = nullptr;
		other._value = JS_UNDEFINED;
	}
	return *this;
}

Value Value::dup() const {
	if (_ctx) {
		return Value(_ctx, JS_DupValue(_ctx, _value));
	}
	return Value();
}

JSValue Value::operator*() const noexcept {
	return _value;
}

JSValue Value::release() noexcept {
	auto tmp = _value;
	_value = JS_UNDEFINED;
	return tmp;
}

} // namespace wf::js
