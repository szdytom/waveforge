#ifndef WFORGE_RUNTIME_HELPERS_H
#define WFORGE_RUNTIME_HELPERS_H

#include "wforge/js_engine.h"
#include <quickjs.h>

namespace wf {

// Callable struct that adds a JS getter to a prototype object.
// Usage: AddGetter{ctx, proto}("name", Class::getter);
struct AddGetter {
	JSContext *ctx;
	JSValue proto;

	void operator()(const char *name, JSCFunction *getter) const {
		JSAtomGuard atom(ctx, name);
		auto getter_val = JSValueGuard::fromCFunction(
			ctx, getter, name, 0, JS_CFUNC_getter
		);
		JS_DefineProperty(
			ctx, proto, atom.get(), JS_UNDEFINED, getter_val.get(),
			JS_UNDEFINED,
			JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_HAS_GET
		);
	}
};

} // namespace wf

// Getter definition macros.
// Must be used inside `namespace wf { ... }` where `unwrap` is in scope.

#define WF_JS_INT_GETTER(Cls, name, field)                         \
	JSValue Cls::name(                                             \
		JSContext *ctx, JSValueConst this_val, int, JSValueConst * \
	) {                                                            \
		auto *ptr = unwrap(ctx, this_val);                         \
		if (!ptr)                                                  \
			return JS_UNDEFINED;                                   \
		return JS_NewInt32(ctx, ptr->field);                       \
	}

#define WF_JS_STR_GETTER(Cls, name, field)                         \
	JSValue Cls::name(                                             \
		JSContext *ctx, JSValueConst this_val, int, JSValueConst * \
	) {                                                            \
		auto *ptr = unwrap(ctx, this_val);                         \
		if (!ptr)                                                  \
			return JS_UNDEFINED;                                   \
		return JS_NewString(ctx, ptr->field);                      \
	}

#define WF_JS_LITERAL_GETTER(Cls, name, str)                               \
	JSValue Cls::name(JSContext *ctx, JSValueConst, int, JSValueConst *) { \
		return JS_NewString(ctx, str);                                     \
	}

#endif
