#include "hacks.h"
#include "wforge/assets.h"
#include "wforge/audio.h"
#include "wforge/level.h"
#include "wforge/runtime_platform.h"
#include "wforge/save_io.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <quickjs.h>
#include <string>

namespace wf::js {

namespace {

std::string toString(JSContext *ctx, JSValueConst value) noexcept {
	size_t length;
	const char *chars = JS_ToCStringLen(ctx, &length, value);
	if (!chars) {
		return {};
	}
	std::string result(chars, length);
	JS_FreeCString(ctx, chars);
	return result;
}

JSValue storageGet(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "storage.get expects a string key");
	}
	auto value = SaveKV::instance().get(toString(ctx, argv[0]));
	if (!value) {
		return argc > 1 ? JS_DupValue(ctx, argv[1]) : JS_NULL;
	}
	return JS_NewStringLen(ctx, value->data(), value->size());
}

JSValue storageGetInt(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "storage.getInt expects a string key");
	}
	int32_t default_value = 0;
	if (argc > 1 && JS_ToInt32(ctx, &default_value, argv[1]) < 0) {
		return JS_ThrowTypeError(
			ctx, "storage.getInt expects an integer default"
		);
	}
	return JS_NewInt32(
		ctx, SaveKV::instance().getInt(toString(ctx, argv[0]), default_value)
	);
}

JSValue storageGetBool(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "storage.getBool expects a string key");
	}
	if (argc > 1 && !JS_IsBool(argv[1])) {
		return JS_ThrowTypeError(
			ctx, "storage.getBool expects a boolean default"
		);
	}
	bool default_value = argc > 1 && JS_ToBool(ctx, argv[1]);
	return JS_NewBool(
		ctx, SaveKV::instance().getBool(toString(ctx, argv[0]), default_value)
	);
}

JSValue storageSet(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
		return JS_ThrowTypeError(
			ctx, "storage.set expects string key and value"
		);
	}
	SaveKV::instance().set(toString(ctx, argv[0]), toString(ctx, argv[1]));
	return JS_UNDEFINED;
}

JSValue storageSetInt(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsNumber(argv[1])) {
		return JS_ThrowTypeError(
			ctx, "storage.setInt expects a string key and integer value"
		);
	}
	int32_t value;
	if (JS_ToInt32(ctx, &value, argv[1]) < 0) {
		return JS_ThrowTypeError(
			ctx, "storage.setInt expects an integer value"
		);
	}
	SaveKV::instance().setInt(toString(ctx, argv[0]), value);
	return JS_UNDEFINED;
}

JSValue storageSetBool(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 2 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(
			ctx, "storage.setBool expects a string key and boolean value"
		);
	}
	SaveKV::instance().setBool(
		toString(ctx, argv[0]), JS_ToBool(ctx, argv[1]) != 0
	);
	return JS_UNDEFINED;
}

JSValue storageRemove(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "storage.remove expects a string key");
	}
	SaveKV::instance().remove(toString(ctx, argv[0]));
	return JS_UNDEFINED;
}

JSValue musicSetCollection(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1 || !JS_IsString(argv[0])) {
		return JS_ThrowTypeError(ctx, "music.setCollection expects an id");
	}
	BGMManager::instance().setCollection(toString(ctx, argv[0]));
	return JS_UNDEFINED;
}

JSValue musicUnset(JSContext *, JSValueConst, int, JSValueConst *) noexcept {
	BGMManager::instance().unsetCollection();
	return JS_UNDEFINED;
}

JSValue musicFadeOut(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	int32_t duration;
	if (argc < 1 || JS_ToInt32(ctx, &duration, argv[0]) < 0) {
		return JS_ThrowTypeError(ctx, "music.fadeOut expects a duration");
	}
	BGMManager::instance().fadeOutCurrent(duration);
	return JS_UNDEFINED;
}

JSValue musicFadeIn(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	int32_t duration;
	double starting_volume;
	if (argc < 2 || JS_ToInt32(ctx, &duration, argv[0]) < 0
	    || JS_ToFloat64(ctx, &starting_volume, argv[1]) < 0) {
		return JS_ThrowTypeError(
			ctx, "music.fadeIn expects duration and starting volume"
		);
	}
	BGMManager::instance().fadeInCurrent(
		duration, static_cast<float>(starting_volume)
	);
	return JS_UNDEFINED;
}

JSValue musicSetVolume(
	JSContext *ctx, JSValueConst, int argc, JSValueConst *argv
) noexcept {
	double volume;
	if (argc < 1 || JS_ToFloat64(ctx, &volume, argv[0]) < 0) {
		return JS_ThrowTypeError(ctx, "music.setVolume expects a volume");
	}
	BGMManager::instance().setVolume(
		static_cast<float>(std::clamp(volume, 0.0, 1.0))
	);
	return JS_UNDEFINED;
}

void setStringProperty(
	JSContext *ctx, JSValueConst object, const char *name,
	std::string_view value
) noexcept {
	JS_SetPropertyStr(
		ctx, object, name, JS_NewStringLen(ctx, value.data(), value.size())
	);
}

JSValue createLevelCatalog(JSContext *ctx) noexcept {
	const auto &sequence = AssetsManager::instance().getAsset<LevelSequence>(
		"level-sequence"
	);
	JSValue levels = JS_NewArray(ctx);
	for (uint32_t index = 0; index < sequence.levels.size(); index++) {
		const auto &metadata = *sequence.levels[index];
		JSValue level = JS_NewObject(ctx);
		JS_SetPropertyStr(
			ctx, level, "index", JS_NewInt32(ctx, metadata.index)
		);
		setStringProperty(ctx, level, "id", metadata.id);
		setStringProperty(ctx, level, "name", metadata.name);
		setStringProperty(ctx, level, "description", metadata.description);
		setStringProperty(ctx, level, "author", metadata.author);
		setStringProperty(
			ctx, level, "difficulty",
			LevelMetadata::difficultyToString(metadata.difficulty)
		);
		setStringProperty(ctx, level, "minimapId", metadata.minimap_id);
		JS_SetPropertyUint32(ctx, levels, index, level);
	}
	return levels;
}

} // namespace

void bindPlatformApi(JSContext *ctx, JSValueConst waveforge) {
	static const JSCFunctionListEntry STORAGE_API[] = {
		cFuncDef("get", 2, storageGet),
		cFuncDef("getInt", 2, storageGetInt),
		cFuncDef("getBool", 2, storageGetBool),
		cFuncDef("set", 2, storageSet),
		cFuncDef("setInt", 2, storageSetInt),
		cFuncDef("setBool", 2, storageSetBool),
		cFuncDef("remove", 1, storageRemove),
	};
	JSValue storage = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(
		ctx, storage, STORAGE_API, std::size(STORAGE_API)
	);
	JS_SetPropertyStr(ctx, waveforge, "storage", storage);

	static const JSCFunctionListEntry MUSIC_API[] = {
		cFuncDef("setCollection", 1, musicSetCollection),
		cFuncDef("unset", 0, musicUnset),
		cFuncDef("fadeOut", 1, musicFadeOut),
		cFuncDef("fadeIn", 2, musicFadeIn),
		cFuncDef("setVolume", 1, musicSetVolume),
	};
	JSValue music = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, music, MUSIC_API, std::size(MUSIC_API));
	JS_SetPropertyStr(ctx, waveforge, "music", music);

	JS_SetPropertyStr(ctx, waveforge, "levels", createLevelCatalog(ctx));
}

} // namespace wf::js
