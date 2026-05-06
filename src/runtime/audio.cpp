#include "wforge/audio.h"
#include "helpers.h"
#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <quickjs.h>

namespace wf {

namespace {

JSValue buildProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);

	auto add = AddGetter{ctx, proto};
	add("id", SoundClass::get_id);
	add("duration", SoundClass::get_duration);

	// .play() method
	JSValue play_fn = JS_NewCFunction(ctx, SoundClass::play, "play", 0);
	JS_SetPropertyStr(ctx, proto, "play", play_fn);

	return proto;
}

} // anonymous namespace

JSValue SoundClass::ctor(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 1) {
		return JS_ThrowTypeError(ctx, "Sound requires an id");
	}

	const char *id_str = JS_ToCString(ctx, argv[0]);
	if (!id_str) {
		return JS_EXCEPTION;
	}

	auto res = std::make_unique<SoundClass>();
	res->id = id_str;
	res->buffer = AssetsManager::instance().getAssetChecked<sf::SoundBuffer>(
		id_str
	);
	if (!res->buffer) {
		JS_FreeCString(ctx, id_str);
		return JS_ThrowReferenceError(ctx, "Sound not found: %s", id_str);
	}

	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		JS_FreeCString(ctx, id_str);
		return obj;
	}

	JS_SetOpaque(obj, res.release());
	JS_FreeCString(ctx, id_str);
	return obj;
}

void SoundClass::bindContext(JSContext *ctx, JSValue ns) {
	auto ctor_func = JS_NewCFunction2(
		ctx, ctor, "Sound", 1, JS_CFUNC_constructor, 0
	);
	auto proto = buildProto(ctx);
	JS_SetConstructor(ctx, ctor_func, proto);
	JS_SetClassProto(ctx, clsId(JS_GetRuntime(ctx)), proto);
	JS_DefinePropertyValueStr(
		ctx, ns, "Sound", ctor_func, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
}

WF_JS_STR_GETTER(SoundClass, get_id, id.c_str())

JSValue SoundClass::get_duration(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *ptr = unwrap(ctx, this_val);
	if (!ptr || !ptr->buffer) {
		return JS_ThrowTypeError(ctx, "Invalid Sound object");
	}
	return JS_NewInt32(ctx, ptr->buffer->getDuration().asMilliseconds());
}

JSValue SoundClass::play(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *ptr = unwrap(ctx, this_val);
	if (!ptr || !ptr->buffer) {
		return JS_ThrowTypeError(ctx, "Invalid Sound object");
	}
	ActiveSoundManager::instance().play(*ptr->buffer);
	return JS_UNDEFINED;
}

} // namespace wf
