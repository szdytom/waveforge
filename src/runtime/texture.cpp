#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <memory>
#include <quickjs.h>

namespace wf {

namespace {

JSValue buildProto(JSContext *ctx) {
	JSValue proto = JS_NewObject(ctx);
	auto configure_getter = [&](const char *name, JSCFunction *getter) {
		JSAtomGuard atom(ctx, name);
		auto getter_val = JSValueGuard::fromCFunction(
			ctx, getter, name, 0, JS_CFUNC_getter
		);
		JS_DefineProperty(
			ctx, proto, atom.get(), JS_UNDEFINED, getter_val.get(),
			JS_UNDEFINED,
			JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_HAS_GET
		);
	};
	configure_getter("id", TextureClass::get_id);
	configure_getter("width", TextureClass::get_width);
	configure_getter("height", TextureClass::get_height);
	return proto;
}

} // namespace

JSValue TextureClass::ctor(
	JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv
) {
	if (argc < 1) {
		return JS_ThrowTypeError(ctx, "Texture requires an id");
	}

	const char *id_str = JS_ToCString(ctx, argv[0]);
	if (!id_str) {
		return JS_EXCEPTION;
	}

	auto res = std::make_unique<TextureClass>();
	res->id = id_str;
	res->texture = AssetsManager::instance().getAssetChecked<sf::Texture>(
		id_str
	);
	if (!res->texture) {
		JS_FreeCString(ctx, id_str);
		return JS_ThrowReferenceError(ctx, "Texture not found: %s", id_str);
	}

	JSValue obj = JS_NewObjectClass(ctx, clsId());
	if (JS_IsException(obj)) {
		JS_FreeCString(ctx, id_str);
		return obj;
	}

	JS_SetOpaque(obj, res.release());
	JS_FreeCString(ctx, id_str);
	return obj;
}

void TextureClass::bindContext(JSContext *ctx, JSValue ns) {
	auto ctor_func = JS_NewCFunction2(
		ctx, ctor, "Texture", 1, JS_CFUNC_constructor, 0
	);
	auto proto = buildProto(ctx);
	JS_SetConstructor(ctx, ctor_func, proto);
	JS_SetClassProto(ctx, clsId(), proto);
	JS_DefinePropertyValueStr(
		ctx, ns, "Texture", ctor_func, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);
}

JSValue TextureClass::get_id(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *ptr = unwrap(this_val);
	if (!ptr) {
		return JS_ThrowTypeError(ctx, "Invalid Texture object");
	}
	return JS_NewString(ctx, ptr->id.c_str());
}

JSValue TextureClass::get_width(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *ptr = unwrap(this_val);
	if (!ptr || !ptr->texture) {
		return JS_ThrowTypeError(ctx, "Invalid Texture object");
	}
	return JS_NewInt32(ctx, ptr->texture->getSize().x);
}

JSValue TextureClass::get_height(
	JSContext *ctx, JSValueConst this_val, int /*argc*/, JSValueConst * /*argv*/
) {
	auto *ptr = unwrap(this_val);
	if (!ptr || !ptr->texture) {
		return JS_ThrowTypeError(ctx, "Invalid Texture object");
	}
	return JS_NewInt32(ctx, ptr->texture->getSize().y);
}

} // namespace wf
