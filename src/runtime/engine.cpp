#include "hacks.h"
#include "wforge/runtime.h"
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

void ContextDeleter::operator()(JSContext *ctx) const noexcept {
	if (ctx) {
		JS_FreeContext(ctx);
	}
}

Engine::Engine(): _runtime(JS_NewRuntime()) {
	if (!_runtime) {
		throw std::runtime_error("Failed to create QuickJS runtime");
	}

	JS_SetRuntimeOpaque(_runtime.get(), this);
}

JSContext *Engine::createContext() {
	auto ctx = JS_NewContext(_runtime.get());
	if (!ctx) {
		throw std::runtime_error("Failed to create QuickJS context");
	}
	_contexts.emplace_back(ctx);
	return ctx;
}

void Engine::releaseContext(JSContext *ctx) noexcept {
	if (!ctx) {
		return;
	}

	for (auto it = _contexts.begin(); it != _contexts.end(); ++it) {
		if (it->get() == ctx) {
			it->release();
			_contexts.erase(it);
			return;
		}
	}

	std::cerr << "Context not found in engine's context list. Double free or "
				 "corruption."
			  << std::endl;
	cpptrace::generate_trace().print(std::cerr);
	std::abort();
}

void Engine::destroyContext(JSContext *ctx) noexcept {
	if (ctx == nullptr) {
		return;
	}

#ifndef NDEBUG
	if (JS_GetRuntime(ctx) != _runtime.get()) {
		std::cerr
			<< "Cannot destroy context that does not belong to this engine."
			<< std::endl;
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	for (auto it = _contexts.begin(); it != _contexts.end(); ++it) {
		if (it->get() == ctx) {
			_contexts.erase(it);
			return;
		}
	}

	std::cerr << "Context not found in engine's context list. Double free or "
				 "corruption."
			  << std::endl;
	cpptrace::generate_trace().print(std::cerr);
	std::abort();
}

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
