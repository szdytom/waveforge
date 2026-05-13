#include "hacks.h"
#include "helper.h"
#include "wforge/runtime.h"
#include <bit>
#include <cpptrace/cpptrace.hpp>
#include <cstdio>
#include <cstring>
#include <expected>
#include <iostream>
#include <optional>
#include <string_view>

namespace wf::js {

namespace {

WF_JS_DEF_GETTER_I32(Color, getR, self->color.r)
WF_JS_DEF_GETTER_I32(Color, getG, self->color.g)
WF_JS_DEF_GETTER_I32(Color, getB, self->color.b)
WF_JS_DEF_GETTER_I32(Color, getA, self->color.a)

WF_JS_DEF_SETTER_U8(Color, setR, color.r)
WF_JS_DEF_SETTER_U8(Color, setG, color.g)
WF_JS_DEF_SETTER_U8(Color, setB, color.b)
WF_JS_DEF_SETTER_U8(Color, setA, color.a)

JSValue colorToString(JSContext *ctx, const sf::Color &color) {
	if (color.a < 255) {
		char buf[10];
		std::snprintf(
			buf, sizeof(buf), "#%02x%02x%02x%02x", color.r, color.g, color.b,
			color.a
		);
		return JS_NewString(ctx, buf);
	} else {
		char buf[8];
		std::snprintf(
			buf, sizeof(buf), "#%02x%02x%02x", color.r, color.g, color.b
		);
		return JS_NewString(ctx, buf);
	}
}

[[nodiscard]] std::expected<sf::Color, const char *> parseHex(
	std::string_view str
) {
	if (str.empty() || str[0] != '#') {
		return std::unexpected("Color hex string must start with '#'");
	}

	auto hexVal = [](char ch) -> std::optional<uint8_t> {
		if (ch >= '0' && ch <= '9') {
			return uint8_t(ch - '0');
		}
		if (ch >= 'a' && ch <= 'f') {
			return uint8_t(ch - 'a' + 10);
		}
		if (ch >= 'A' && ch <= 'F') {
			return uint8_t(ch - 'A' + 10);
		}
		return std::nullopt;
	};

	sf::Color c;

	switch (str.size()) {
	case 4: // #RGB
		for (size_t i = 1; i < 4; i++) {
			auto v = hexVal(str[i]);
			if (!v) {
				return std::unexpected("Invalid hex character in color string");
			}
			(&c.r)[i - 1] = uint8_t(*v * 17);
		}
		c.a = 255;
		break;
	case 5: // #RGBA
		for (size_t i = 1; i < 5; i++) {
			auto v = hexVal(str[i]);
			if (!v) {
				return std::unexpected("Invalid hex character in color string");
			}
			(&c.r)[i - 1] = uint8_t(*v * 17);
		}
		break;
	case 7: // #RRGGBB
		for (size_t i = 1; i < 7; i += 2) {
			auto hi = hexVal(str[i]);
			auto lo = hexVal(str[i + 1]);
			if (!hi || !lo) {
				return std::unexpected("Invalid hex character in color string");
			}
			(&c.r)[(i - 1) / 2] = uint8_t(*hi << 4 | *lo);
		}
		c.a = 255;
		break;
	case 9: // #RRGGBBAA
		for (size_t i = 1; i < 9; i += 2) {
			auto hi = hexVal(str[i]);
			auto lo = hexVal(str[i + 1]);
			if (!hi || !lo) {
				return std::unexpected("Invalid hex character in color string");
			}
			(&c.r)[(i - 1) / 2] = uint8_t(*hi << 4 | *lo);
		}
		break;
	default:
		return std::unexpected("Invalid color string length");
	}

	return c;
}

WF_JS_METHOD(Color, toPrimitive, {
	bool hint_string = false;
	if (argc > 0) {
		JSValue hint = argv[0];
		if (JS_IsString(hint)) {
			const char *hint_str = JS_ToCString(ctx, hint);
			if (hint_str && std::strcmp(hint_str, "string") == 0) {
				hint_string = true;
			}
			JS_FreeCString(ctx, hint_str);
		}
	}

	if (hint_string) {
		return colorToString(ctx, self->color);
	}
	return JS_NewInt32(ctx, std::bit_cast<int32_t>(self->color.toInteger()));
})

WF_JS_METHOD(Color, valueOf, {
	return JS_NewInt32(ctx, std::bit_cast<int32_t>(self->color.toInteger()));
})

WF_JS_METHOD(Color, toString, { return colorToString(ctx, self->color); })

} // namespace

Color::Color(sf::Color color) noexcept: color(color) {}

static const JSCFunctionListEntry PROTO_FIELDS_DATA[] = {
	cGetSetDef("r", Color_getR, Color_setR),
	cGetSetDef("g", Color_getG, Color_setG),
	cGetSetDef("b", Color_getB, Color_setB),
	cGetSetDef("a", Color_getA, Color_setA),
	cFuncDef("toString", 0, Color_toString),
	cFuncDef("valueOf", 0, Color_valueOf),
	cFuncDef("[Symbol.toPrimitive]", 0, Color_toPrimitive),
	propStringDef("[Symbol.toStringTag]", "Color", JS_PROP_CONFIGURABLE),
};

const CFunctionList Color::PROTO_FIELDS{PROTO_FIELDS_DATA};

JSValue Color::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	sf::Color c;

	if (argc == 1) {
		auto result = interpret(ctx, argv[0]);
		if (!result) {
			return JS_ThrowTypeError(ctx, "%s", result.error());
		}
		c = *result;
	} else if (argc == 3 || argc == 4) {
		int32_t r, g, b, a = 255;
		if (JS_ToInt32(ctx, &r, argv[0]) < 0 || JS_ToInt32(ctx, &g, argv[1]) < 0
		    || JS_ToInt32(ctx, &b, argv[2]) < 0) {
			return JS_ThrowTypeError(
				ctx, "Failed to convert color components to int"
			);
		}
		if (argc == 4) {
			if (JS_ToInt32(ctx, &a, argv[3]) < 0) {
				return JS_ThrowTypeError(
					ctx, "Failed to convert alpha component to int"
				);
			}
		}
		c = sf::Color(uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a));
	} else {
		return JS_ThrowTypeError(
			ctx, "Color constructor expects 1, 3, or 4 arguments"
		);
	}

	auto self = std::make_unique<Color>(c);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self.release();
	return obj;
}

[[nodiscard]] std::expected<sf::Color, const char *> Color::interpret(
	JSContext *ctx, JSValueConst val
) {
	if (auto *self = Color::unwrap(ctx, val)) {
		return self->color;
	}

	if (JS_IsString(val)) {
		const char *str = JS_ToCString(ctx, val);
		if (!str) {
#ifndef NDEBUG
			std::cerr << "Failed to convert color string to C string\n";
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
#endif
			return std::unexpected("Internal error converting color string");
		}
		auto result = parseHex(str);
		JS_FreeCString(ctx, str);
		return result;
	}

	if (JS_IsArray(val)) {
		int64_t len;
		JS_GetLength(ctx, val, &len);
		if (len < 3 || len > 4) {
			return std::unexpected("Color array must have 3 or 4 elements");
		}
		int32_t comps[4] = {0, 0, 0, 255};
		for (int64_t i = 0; i < len; i++) {
			JSValue elem = JS_GetPropertyUint32(ctx, val, uint32_t(i));
			if (JS_ToInt32(ctx, &comps[i], elem) < 0) {
				JS_FreeValue(ctx, elem);
				return std::unexpected("Color array elements must be numbers");
			}
			JS_FreeValue(ctx, elem);
		}
		return sf::Color(
			uint8_t(comps[0]), uint8_t(comps[1]), uint8_t(comps[2]),
			uint8_t(comps[3])
		);
	}

	if (JS_IsNumber(val)) {
		uint32_t packed;
		if (JS_ToUint32(ctx, &packed, val) < 0) {
#ifndef NDEBUG
			std::cerr << "Failed to convert color number to uint32\n";
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
#endif
			return std::unexpected("Failed to convert color value to uint32");
		}
		return sf::Color(packed);
	}

	return std::unexpected("Unsupported color value type");
}

} // namespace wf::js
