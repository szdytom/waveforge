#include "hacks.h"
#include "helper.h"
#include "wforge/assets.h"
#include "wforge/runtime.h"

namespace wf::js {

namespace {

WF_JS_DEF_GETTER_I32(Texture, getWidth, self->width())
WF_JS_DEF_GETTER_I32(Texture, getHeight, self->height())
WF_JS_DEF_GETTER_STR(Texture, getId, self->id.c_str())

} // namespace

Texture::Texture(sf::Texture *tex, std::string id) noexcept
	: texture(tex), id(std::move(id)) {}

static const JSCFunctionListEntry PROTO_FIELDS_DATA[] = {
	cGetSetDef("width", Texture_getWidth, nullptr),
	cGetSetDef("height", Texture_getHeight, nullptr),
	cGetSetDef("id", Texture_getId, nullptr),
	propStringDef("[Symbol.toStringTag]", "Texture", JS_PROP_CONFIGURABLE),
};

const CFunctionList Texture::PROTO_FIELDS{PROTO_FIELDS_DATA};

JSValue Texture::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	const char *id_cstr = JS_ToCString(ctx, argv[0]);
	if (!id_cstr) {
		return JS_ThrowTypeError(ctx, "Failed to convert texture id to string");
	}

	std::string id(id_cstr);
	JS_FreeCString(ctx, id_cstr);

	auto *tex = AssetsManager::instance().getAssetChecked<sf::Texture>(id);
	if (!tex) {
		return JS_ThrowTypeError(ctx, "Texture '%s' not found", id.c_str());
	}

	auto self = std::make_unique<Texture>(tex, id);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self.release();
	return obj;
}

int Texture::width() const noexcept {
	return texture->getSize().x;
}

int Texture::height() const noexcept {
	return texture->getSize().y;
}

} // namespace wf::js
