#ifndef WFORGE_RUNTIME_HELPER_H
#define WFORGE_RUNTIME_HELPER_H

#include <quickjs.h>

#define WF_JS_DEF_GETTER(Class, name, expr)                                  \
	JSValue Class##_##name(JSContext *ctx, JSValueConst this_val) noexcept { \
		auto *self = Class::unwrap(ctx, this_val);                           \
		if (!self) {                                                         \
			return JS_UNDEFINED;                                             \
		}                                                                    \
		return (expr);                                                       \
	}

#define WF_JS_DEF_GETTER_I32(Class, name, expr) \
	WF_JS_DEF_GETTER(Class, name, JS_NewInt32(ctx, (expr)))

#define WF_JS_DEF_GETTER_STR(Class, name, expr) \
	WF_JS_DEF_GETTER(Class, name, JS_NewString(ctx, (expr)))

#define WF_JS_DEF_GETTER_BOOL(Class, name, expr) \
	WF_JS_DEF_GETTER(Class, name, JS_NewBool(ctx, (expr)))

#define WF_JS_DEF_GETTER_F64(Class, name, expr) \
	WF_JS_DEF_GETTER(Class, name, JS_NewFloat64(ctx, (expr)))

#define WF_JS_DEF_SETTER(Class, name, body)                     \
	JSValue Class##_##name(                                     \
		JSContext *ctx, JSValueConst this_val, JSValueConst val \
	) noexcept {                                                \
		auto *self = Class::unwrap(ctx, this_val);              \
		if (!self) {                                            \
			return JS_UNDEFINED;                                \
		}                                                       \
		body return JS_UNDEFINED;                               \
	}

#define WF_JS_DEF_SETTER_U8(Class, name, field) \
	WF_JS_DEF_SETTER(Class, name, {             \
		int32_t _wf_v;                          \
		JS_ToInt32(ctx, &_wf_v, val);           \
		self->field = uint8_t(_wf_v);           \
	})

#define WF_JS_DEF_SETTER_I32(Class, name, field) \
	WF_JS_DEF_SETTER(Class, name, {              \
		int32_t _wf_v;                           \
		JS_ToInt32(ctx, &_wf_v, val);            \
		self->field = _wf_v;                     \
	})

#define WF_JS_DEF_SETTER_STR(Class, name, field)                \
	WF_JS_DEF_SETTER(Class, name, {                             \
		const char *_wf_v = JS_ToCString(ctx, val);             \
		if (!_wf_v) {                                           \
			return JS_ThrowTypeError(ctx, "Expected a string"); \
		}                                                       \
		self->field = _wf_v;                                    \
		JS_FreeCString(ctx, _wf_v);                             \
	})

#define WF_JS_METHOD(Class, name, body)                                     \
	JSValue Class##_##name(                                                 \
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv \
	) noexcept {                                                            \
		auto *self = Class::unwrap(ctx, this_val);                          \
		if (!self) {                                                        \
			return JS_UNDEFINED;                                            \
		}                                                                   \
		body                                                                \
	}

#endif // WFORGE_RUNTIME_HELPER_H
