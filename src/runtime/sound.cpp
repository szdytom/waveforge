#include "hacks.h"
#include "helper.h"
#include "wforge/assets.h"
#include "wforge/audio.h"
#include "wforge/runtime.h"
#include <memory>

namespace wf::js {

Sound::Sound(sf::SoundBuffer *buffer) noexcept: _buffer(buffer) {}

namespace {

WF_JS_METHOD(Sound, play, {
	ActiveSoundManager::instance().play(*self->_buffer);
	return JS_UNDEFINED;
})

} // namespace

static const JSCFunctionListEntry PROTO_FIELDS_DATA[] = {
	cFuncDef("play", 0, Sound_play),
	propStringDef("[Symbol.toStringTag]", "Sound", JS_PROP_CONFIGURABLE),
};

const CFunctionList Sound::PROTO_FIELDS{PROTO_FIELDS_DATA};

JSValue Sound::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	const char *id_cstr = JS_ToCString(ctx, argv[0]);
	if (!id_cstr) {
		return JS_ThrowTypeError(ctx, "Sound id must be a string");
	}

	std::string id(id_cstr);
	JS_FreeCString(ctx, id_cstr);

	auto *buffer = AssetsManager::instance().getAssetChecked<sf::SoundBuffer>(
		id
	);
	if (!buffer) {
		return JS_ThrowTypeError(ctx, "Sound '%s' not found", id.c_str());
	}

	auto self = std::make_unique<Sound>(buffer);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.release());
	return obj;
}

} // namespace wf::js
