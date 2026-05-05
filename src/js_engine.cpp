#include "wforge/js_engine.h"
#include <cstdio>
#include <quickjs.h>
#include <stdexcept>

namespace wf {

void JSRuntimeDeleter::operator()(JSRuntime *rt) const noexcept {
	if (rt) {
		JS_FreeRuntime(rt);
	}
}

void JSContextDeleter::operator()(JSContext *ctx) const noexcept {
	if (ctx) {
		JS_FreeContext(ctx);
	}
}

QuickJSEngine::QuickJSEngine()
	: _rt(JS_NewRuntime()), _ctx(_rt ? JS_NewContext(_rt.get()) : nullptr) {
	if (!_rt) {
		throw std::runtime_error("QuickJSEngine: JS_NewRuntime() failed");
	}
	if (!_ctx) {
		throw std::runtime_error("QuickJSEngine: JS_NewContext() failed");
	}
	JS_SetRuntimeOpaque(_rt.get(), this);
}

void js_std_dump_error(JSContext *ctx) {
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

} // namespace wf
