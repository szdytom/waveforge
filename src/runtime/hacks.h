#ifndef WFORGE_RUNTIME_HACKS_H
#define WFORGE_RUNTIME_HACKS_H

#include <quickjs.h>

// Normal helpers, please go to `js_engine.h` or `runtime.h`

// This header "hacks" into the internal structures of QuickJS to provide
// additional functionality that the public API doesn't directly export. The
// file is subject to breakage on even minor updates of QuickJS, so it MUST BE
// REVIEWED after updating the QuickJS submodule.

namespace wf::js {

// 1 is the class ID of Object, internal to QuickJS, unlikely to change
constexpr int CLASS_ID_OBJECT = 1;

inline JSValue getObjectProto(JSContext *ctx) noexcept {
	return JS_GetClassProto(ctx, CLASS_ID_OBJECT);
}

inline JSValue getIteratorProto(JSContext *ctx) noexcept {
	// 40 is the class ID of Iterator, internal to QuickJS
	// very likely to change
	return JS_GetClassProto(ctx, 40);
}

// C++ replacements for JSCFunctionListEntry initializer macro, since C++
// doesn't allow nested designated initializers and designated initializers in
// unions.

constexpr JSCFunctionListEntry cFuncDef(
	const char *name, uint8_t length, JSCFunction *func1
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = uint8_t(JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	res.def_type = JS_DEF_CFUNC;
	res.magic = 0;
	res.u.func.length = length;
	res.u.func.cproto = JS_CFUNC_generic;
	res.u.func.cfunc.generic = func1;
	return res;
}

constexpr JSCFunctionListEntry cFuncDef(
	const char *name, uint8_t length, JSCFunction *func1, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_CFUNC;
	res.magic = 0;
	res.u.func.length = length;
	res.u.func.cproto = JS_CFUNC_generic;
	res.u.func.cfunc.generic = func1;
	return res;
}

constexpr JSCFunctionListEntry cFuncMagicDef(
	const char *name, uint8_t length, JSCFunctionMagic *func1, int16_t magic
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = uint8_t(JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	res.def_type = JS_DEF_CFUNC;
	res.magic = magic;
	res.u.func.length = length;
	res.u.func.cproto = JS_CFUNC_generic_magic;
	res.u.func.cfunc.generic_magic = func1;
	return res;
}

constexpr JSCFunctionListEntry cFuncSpecialDef(
	const char *name, uint8_t length, JSCFunctionEnum cproto,
	JSCFunctionType cfunc
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = uint8_t(JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	res.def_type = JS_DEF_CFUNC;
	res.magic = 0;
	res.u.func.length = length;
	res.u.func.cproto = uint8_t(cproto);
	res.u.func.cfunc = cfunc;
	return res;
}

constexpr JSCFunctionListEntry cFuncIteratorNextDef(
	const char *name, uint8_t length,
	JSValue (*func1)(
		JSContext *, JSValueConst, int, JSValueConst *, int *, int
	),
	int16_t magic
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = uint8_t(JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	res.def_type = JS_DEF_CFUNC;
	res.magic = magic;
	res.u.func.length = length;
	res.u.func.cproto = JS_CFUNC_iterator_next;
	res.u.func.cfunc.iterator_next = func1;
	return res;
}

constexpr JSCFunctionListEntry cGetSetDef(
	const char *name, JSValue (*fgetter)(JSContext *, JSValueConst),
	JSValue (*fsetter)(JSContext *, JSValueConst, JSValueConst)
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = JS_PROP_CONFIGURABLE;
	res.def_type = JS_DEF_CGETSET;
	res.magic = 0;
	res.u.getset.get.getter = fgetter;
	res.u.getset.set.setter = fsetter;
	return res;
}

constexpr JSCFunctionListEntry cGetSetDef(
	const char *name, JSValue (*fgetter)(JSContext *, JSValueConst),
	JSValue (*fsetter)(JSContext *, JSValueConst, JSValueConst),
	uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_CGETSET;
	res.magic = 0;
	res.u.getset.get.getter = fgetter;
	res.u.getset.set.setter = fsetter;
	return res;
}

constexpr JSCFunctionListEntry cGetSetMagicDef(
	const char *name, JSValue (*fgetter)(JSContext *, JSValueConst, int),
	JSValue (*fsetter)(JSContext *, JSValueConst, JSValueConst, int),
	int16_t magic
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = JS_PROP_CONFIGURABLE;
	res.def_type = JS_DEF_CGETSET_MAGIC;
	res.magic = magic;
	res.u.getset.get.getter_magic = fgetter;
	res.u.getset.set.setter_magic = fsetter;
	return res;
}

constexpr JSCFunctionListEntry propStringDef(
	const char *name, const char *cstr, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_STRING;
	res.magic = 0;
	res.u.str = cstr;
	return res;
}

constexpr JSCFunctionListEntry propInt32Def(
	const char *name, int32_t val, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_INT32;
	res.magic = 0;
	res.u.i32 = val;
	return res;
}

constexpr JSCFunctionListEntry propInt64Def(
	const char *name, int64_t val, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_INT64;
	res.magic = 0;
	res.u.i64 = val;
	return res;
}

constexpr JSCFunctionListEntry propDoubleDef(
	const char *name, double val, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_DOUBLE;
	res.magic = 0;
	res.u.f64 = val;
	return res;
}

constexpr JSCFunctionListEntry propU2DDef(
	const char *name, uint64_t val, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_DOUBLE;
	res.magic = 0;
	res.u.u64 = val;
	return res;
}

constexpr JSCFunctionListEntry propUndefinedDef(
	const char *name, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_UNDEFINED;
	res.magic = 0;
	res.u.i32 = 0;
	return res;
}

constexpr JSCFunctionListEntry propBoolDef(
	const char *name, int32_t val, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_BOOL;
	res.magic = 0;
	res.u.i32 = val;
	return res;
}

constexpr JSCFunctionListEntry propSymbolDef(
	const char *name, int32_t val, uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_PROP_SYMBOL;
	res.magic = 0;
	res.u.i32 = val;
	return res;
}

constexpr JSCFunctionListEntry objectDef(
	const char *name, const JSCFunctionListEntry *tab, int len,
	uint8_t prop_flags
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = prop_flags;
	res.def_type = JS_DEF_OBJECT;
	res.magic = 0;
	res.u.prop_list.tab = tab;
	res.u.prop_list.len = len;
	return res;
}

constexpr JSCFunctionListEntry aliasDef(
	const char *name, const char *from
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = uint8_t(JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	res.def_type = JS_DEF_ALIAS;
	res.magic = 0;
	res.u.alias.name = from;
	res.u.alias.base = -1;
	return res;
}

constexpr JSCFunctionListEntry aliasBaseDef(
	const char *name, const char *from, int base
) noexcept {
	JSCFunctionListEntry res{};
	res.name = name;
	res.prop_flags = uint8_t(JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	res.def_type = JS_DEF_ALIAS;
	res.magic = 0;
	res.u.alias.name = from;
	res.u.alias.base = base;
	return res;
}

} // namespace wf::js

#endif // WFORGE_RUNTIME_HACKS_H
