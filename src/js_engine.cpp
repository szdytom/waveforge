#include "wforge/js_engine.h"
#include <quickjs-libc.h>
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
	js_std_init_handlers(_rt.get());
}

} // namespace wf
