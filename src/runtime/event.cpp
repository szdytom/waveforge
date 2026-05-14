#include "hacks.h"
#include "helper.h"
#include "wforge/runtime.h"

namespace wf::js {

namespace {

// --- KeyEvent getters ---

JSValue KeyEvent_getType(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = KeyEvent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	switch (self->type) {
	case KeyEvent::Type::KeyDown:
		return JS_NewString(ctx, "keydown");
	case KeyEvent::Type::KeyUp:
		return JS_NewString(ctx, "keyup");
	}
	return JS_NewString(ctx, "unknown");
}

WF_JS_DEF_GETTER_STR(KeyEvent, getCode, self->code.c_str())
WF_JS_DEF_GETTER_BOOL(KeyEvent, getAlt, self->alt)
WF_JS_DEF_GETTER_BOOL(KeyEvent, getControl, self->control)
WF_JS_DEF_GETTER_BOOL(KeyEvent, getShift, self->shift)
WF_JS_DEF_GETTER_BOOL(KeyEvent, getSystem, self->system)

// --- MouseButtonEvent getters ---

JSValue MouseButtonEvent_getType(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = MouseButtonEvent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	switch (self->type) {
	case MouseButtonEvent::Type::MouseDown:
		return JS_NewString(ctx, "mousedown");
	case MouseButtonEvent::Type::MouseUp:
		return JS_NewString(ctx, "mouseup");
	}
	return JS_NewString(ctx, "unknown");
}

WF_JS_DEF_GETTER_I32(MouseButtonEvent, getButton, self->button)
WF_JS_DEF_GETTER_I32(MouseButtonEvent, getX, self->x)
WF_JS_DEF_GETTER_I32(MouseButtonEvent, getY, self->y)

// --- MouseMoveEvent getters ---

JSValue MouseMoveEvent_getType(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = MouseMoveEvent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_NewString(ctx, "mousemove");
}

WF_JS_DEF_GETTER_I32(MouseMoveEvent, getX, self->x)
WF_JS_DEF_GETTER_I32(MouseMoveEvent, getY, self->y)

#define WF_KEY(key, js)          \
	case sf::Keyboard::Key::key: \
		return js;

const char *keyName(sf::Keyboard::Key key) noexcept {
	/* clang-format off */
	switch (key) {
	// Letters
	WF_KEY(A, "a")    WF_KEY(B, "b")    WF_KEY(C, "c")
	WF_KEY(D, "d")    WF_KEY(E, "e")    WF_KEY(F, "f")
	WF_KEY(G, "g")    WF_KEY(H, "h")    WF_KEY(I, "i")
	WF_KEY(J, "j")    WF_KEY(K, "k")    WF_KEY(L, "l")
	WF_KEY(M, "m")    WF_KEY(N, "n")    WF_KEY(O, "o")
	WF_KEY(P, "p")    WF_KEY(Q, "q")    WF_KEY(R, "r")
	WF_KEY(S, "s")    WF_KEY(T, "t")    WF_KEY(U, "u")
	WF_KEY(V, "v")    WF_KEY(W, "w")    WF_KEY(X, "x")
	WF_KEY(Y, "y")    WF_KEY(Z, "z")

	// Numbers
	WF_KEY(Num0, "0")    WF_KEY(Num1, "1")    WF_KEY(Num2, "2")
	WF_KEY(Num3, "3")    WF_KEY(Num4, "4")    WF_KEY(Num5, "5")
	WF_KEY(Num6, "6")    WF_KEY(Num7, "7")    WF_KEY(Num8, "8")
	WF_KEY(Num9, "9")

	// Numpad
	WF_KEY(Numpad0, "0") WF_KEY(Numpad1, "1") WF_KEY(Numpad2, "2")
	WF_KEY(Numpad3, "3") WF_KEY(Numpad4, "4") WF_KEY(Numpad5, "5")
	WF_KEY(Numpad6, "6") WF_KEY(Numpad7, "7") WF_KEY(Numpad8, "8")
	WF_KEY(Numpad9, "9")

	// Function keys
	WF_KEY(F1, "F1")  WF_KEY(F2, "F2")  WF_KEY(F3, "F3")
	WF_KEY(F4, "F4")  WF_KEY(F5, "F5")  WF_KEY(F6, "F6")
	WF_KEY(F7, "F7")  WF_KEY(F8, "F8")  WF_KEY(F9, "F9")
	WF_KEY(F10, "F10") WF_KEY(F11, "F11") WF_KEY(F12, "F12")
	WF_KEY(F13, "F13") WF_KEY(F14, "F14") WF_KEY(F15, "F15")

	// Modifier keys
	WF_KEY(LControl, "Control")  WF_KEY(RControl, "Control")
	WF_KEY(LShift, "Shift")      WF_KEY(RShift, "Shift")
	WF_KEY(LAlt, "Alt")          WF_KEY(RAlt, "Alt")
	WF_KEY(LSystem, "Meta")      WF_KEY(RSystem, "Meta")

	// Navigation
	WF_KEY(Left, "ArrowLeft")    WF_KEY(Right, "ArrowRight")
	WF_KEY(Up, "ArrowUp")        WF_KEY(Down, "ArrowDown")
	WF_KEY(PageUp, "PageUp")     WF_KEY(PageDown, "PageDown")
	WF_KEY(Home, "Home")         WF_KEY(End, "End")
	WF_KEY(Insert, "Insert")     WF_KEY(Delete, "Delete")

	// Named keys
	WF_KEY(Escape, "Escape")     WF_KEY(Space, "Space")
	WF_KEY(Enter, "Enter")       WF_KEY(Backspace, "Backspace")
	WF_KEY(Tab, "Tab")           WF_KEY(Pause, "Pause")
	WF_KEY(Menu, "ContextMenu")

	// Symbols
	WF_KEY(LBracket, "[")        WF_KEY(RBracket, "]")
	WF_KEY(Semicolon, ";")       WF_KEY(Comma, ",")
	WF_KEY(Period, ".")          WF_KEY(Apostrophe, "'")
	WF_KEY(Slash, "/")           WF_KEY(Backslash, "\\")
	WF_KEY(Grave, "`")           WF_KEY(Equal, "=")
	WF_KEY(Hyphen, "-")          WF_KEY(Add, "+")
	WF_KEY(Subtract, "-")        WF_KEY(Multiply, "*")
	WF_KEY(Divide, "/")

	default:
		return "Unknown";
	}
	/* clang-format on */
}

#undef WF_KEY

} // namespace

// ===== KeyEvent =====

KeyEvent::KeyEvent(
	Type type, std::string code, bool alt, bool control, bool shift, bool system
) noexcept
	: type(type)
	, code(std::move(code))
	, alt(alt)
	, control(control)
	, shift(shift)
	, system(system) {}

JSValue KeyEvent::parentProto(JSContext *ctx) noexcept {
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry KEY_EVENT_PROTO[] = {
	cGetSetDef("type", KeyEvent_getType, nullptr),
	cGetSetDef("code", KeyEvent_getCode, nullptr),
	cGetSetDef("alt", KeyEvent_getAlt, nullptr),
	cGetSetDef("control", KeyEvent_getControl, nullptr),
	cGetSetDef("shift", KeyEvent_getShift, nullptr),
	cGetSetDef("system", KeyEvent_getSystem, nullptr),
	propStringDef("[Symbol.toStringTag]", "KeyEvent", JS_PROP_CONFIGURABLE),
};

const CFunctionList KeyEvent::PROTO_FIELDS{KEY_EVENT_PROTO};

JSValue KeyEvent::from(
	JSContext *ctx, const sf::Event::KeyPressed &evt
) noexcept {
	auto self = std::make_unique<KeyEvent>(
		Type::KeyDown, keyName(evt.code), evt.alt, evt.control, evt.shift,
		evt.system
	);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	if (JS_SetOpaque(obj, self.get()) < 0) {
		return JS_ThrowTypeError(ctx, "Internal error");
	}
	self.release();
	return obj;
}

JSValue KeyEvent::from(
	JSContext *ctx, const sf::Event::KeyReleased &evt
) noexcept {
	auto self = std::make_unique<KeyEvent>(
		Type::KeyUp, keyName(evt.code), evt.alt, evt.control, evt.shift,
		evt.system
	);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	if (JS_SetOpaque(obj, self.get()) < 0) {
		return JS_ThrowTypeError(ctx, "Internal error");
	}
	self.release();
	return obj;
}

// ===== MouseButtonEvent =====

MouseButtonEvent::MouseButtonEvent(Type type, int button, int x, int y) noexcept
	: type(type), button(button), x(x), y(y) {}

JSValue MouseButtonEvent::parentProto(JSContext *ctx) noexcept {
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry MOUSE_BUTTON_EVENT_PROTO[] = {
	cGetSetDef("type", MouseButtonEvent_getType, nullptr),
	cGetSetDef("button", MouseButtonEvent_getButton, nullptr),
	cGetSetDef("x", MouseButtonEvent_getX, nullptr),
	cGetSetDef("y", MouseButtonEvent_getY, nullptr),
	propStringDef(
		"[Symbol.toStringTag]", "MouseButtonEvent", JS_PROP_CONFIGURABLE
	),
};

const CFunctionList MouseButtonEvent::PROTO_FIELDS{MOUSE_BUTTON_EVENT_PROTO};

JSValue MouseButtonEvent::from(
	JSContext *ctx, const sf::Event::MouseButtonPressed &evt, int scale
) noexcept {
	auto self = std::make_unique<MouseButtonEvent>(
		Type::MouseDown, std::to_underlying(evt.button), evt.position.x / scale,
		evt.position.y / scale
	);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	if (JS_SetOpaque(obj, self.get()) < 0) {
		return JS_ThrowTypeError(ctx, "Internal error");
	}
	self.release();
	return obj;
}

JSValue MouseButtonEvent::from(
	JSContext *ctx, const sf::Event::MouseButtonReleased &evt, int scale
) noexcept {
	auto self = std::make_unique<MouseButtonEvent>(
		Type::MouseUp, std::to_underlying(evt.button), evt.position.x / scale,
		evt.position.y / scale
	);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	if (JS_SetOpaque(obj, self.get()) < 0) {
		return JS_ThrowTypeError(ctx, "Internal error");
	}
	self.release();
	return obj;
}

// ===== MouseMoveEvent =====

MouseMoveEvent::MouseMoveEvent(Type type, int x, int y) noexcept
	: type(type), x(x), y(y) {}

JSValue MouseMoveEvent::parentProto(JSContext *ctx) noexcept {
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry MOUSE_MOVE_EVENT_PROTO[] = {
	cGetSetDef("type", MouseMoveEvent_getType, nullptr),
	cGetSetDef("x", MouseMoveEvent_getX, nullptr),
	cGetSetDef("y", MouseMoveEvent_getY, nullptr),
	propStringDef(
		"[Symbol.toStringTag]", "MouseMoveEvent", JS_PROP_CONFIGURABLE
	),
};

const CFunctionList MouseMoveEvent::PROTO_FIELDS{MOUSE_MOVE_EVENT_PROTO};

JSValue MouseMoveEvent::from(
	JSContext *ctx, const sf::Event::MouseMoved &evt, int scale
) noexcept {
	auto self = std::make_unique<MouseMoveEvent>(
		Type::MouseMove, evt.position.x / scale, evt.position.y / scale
	);
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	if (JS_SetOpaque(obj, self.get()) < 0) {
		return JS_ThrowTypeError(ctx, "Internal error");
	}
	self.release();
	return obj;
}

} // namespace wf::js
