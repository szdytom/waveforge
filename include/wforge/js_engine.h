#ifndef WFORGE_JS_ENGINE_H
#define WFORGE_JS_ENGINE_H

#include <memory>
#include <quickjs.h>

namespace wf {

struct JSRuntimeDeleter {
	void operator()(JSRuntime *rt) const noexcept;
};
using JSRuntimeUnique = std::unique_ptr<JSRuntime, JSRuntimeDeleter>;

struct JSContextDeleter {
	void operator()(JSContext *ctx) const noexcept;
};
using JSContextUnique = std::unique_ptr<JSContext, JSContextDeleter>;

class QuickJSEngine {
public:
	QuickJSEngine();
	~QuickJSEngine() = default;
	QuickJSEngine(QuickJSEngine &&) noexcept = default;
	QuickJSEngine &operator=(QuickJSEngine &&) noexcept = default;

	JSRuntime *runtime() const noexcept {
		return _rt.get();
	}
	JSContext *context() const noexcept {
		return _ctx.get();
	}

private:
	// _ctx declared after _rt so it's destroyed before _rt
	JSRuntimeUnique _rt;
	JSContextUnique _ctx;
};

// RAII wrapper that calls JS_FreeValue on destruction.
class JSValueGuard {
public:
	JSValueGuard() noexcept: JSValueGuard(nullptr, JS_UNDEFINED) {}
	JSValueGuard(JSContext *ctx, JSValue val) noexcept: _ctx(ctx), _val(val) {}

	~JSValueGuard() {
		if (_ctx) {
			JS_FreeValue(_ctx, _val);
		}
	}

	JSValueGuard(JSValueGuard &&other) noexcept
		: _ctx(other._ctx), _val(other._val) {
		other._ctx = nullptr;
		other._val = JS_UNDEFINED;
	}

	JSValueGuard &operator=(JSValueGuard &&other) noexcept {
		if (this != &other) {
			if (_ctx) {
				JS_FreeValue(_ctx, _val);
			}
			_ctx = other._ctx;
			_val = other._val;
			other._ctx = nullptr;
			other._val = JS_UNDEFINED;
		}
		return *this;
	}

	JSValueGuard(const JSValueGuard &) = delete;
	JSValueGuard &operator=(const JSValueGuard &) = delete;

	JSValue get() const {
		return _val;
	}

	// Release ownership without freeing — caller must manage the value.
	JSValue release() {
		JSValue v = _val;
		_val = JS_UNDEFINED;
		return v;
	}

	explicit operator bool() const noexcept {
		return !JS_IsException(_val) && !JS_IsUndefined(_val);
	}

	static JSValueGuard fromString(JSContext *ctx, const char *str) {
		return JSValueGuard(ctx, JS_NewString(ctx, str));
	}

	static JSValueGuard fromString(JSContext *ctx, const std::string &str) {
		return fromString(ctx, str.c_str());
	}

	static JSValueGuard fromCFunction(
		JSContext *ctx, JSCFunction *func, const char *name, int length,
		JSCFunctionEnum cproto = JS_CFUNC_generic, int magic = 0
	) {
		return JSValueGuard(
			ctx, JS_NewCFunction2(ctx, func, name, length, cproto, magic)
		);
	}

private:
	JSContext *_ctx;
	JSValue _val;
};

class JSAtomGuard {
public:
	JSAtomGuard() noexcept: _ctx(nullptr), _atom(JS_ATOM_NULL) {}
	JSAtomGuard(JSContext *ctx, JSAtom atom) noexcept: _ctx(ctx), _atom(atom) {}
	JSAtomGuard(JSContext *ctx, const char *str) noexcept
		: JSAtomGuard(ctx, JS_NewAtom(ctx, str)) {}
	JSAtomGuard(JSContext *ctx, const std::string_view str) noexcept
		: JSAtomGuard(ctx, JS_NewAtomLen(ctx, str.data(), str.size())) {}

	~JSAtomGuard() {
		if (_ctx) {
			JS_FreeAtom(_ctx, _atom);
		}
	}

	JSAtomGuard(const JSAtomGuard &) = delete;
	JSAtomGuard &operator=(const JSAtomGuard &) = delete;

	JSAtomGuard(JSAtomGuard &&other) noexcept
		: _ctx(other._ctx), _atom(other._atom) {
		other._ctx = nullptr;
		other._atom = JS_ATOM_NULL;
	}

	JSAtomGuard &operator=(JSAtomGuard &&other) noexcept {
		if (this != &other) {
			if (_ctx) {
				JS_FreeAtom(_ctx, _atom);
			}
			_ctx = other._ctx;
			_atom = other._atom;
			other._ctx = nullptr;
			other._atom = JS_ATOM_NULL;
		}
		return *this;
	}

	JSAtom get() const {
		return _atom;
	}

	JSAtom release() {
		JSAtom a = _atom;
		_atom = JS_ATOM_NULL;
		return a;
	}

private:
	JSContext *_ctx;
	JSAtom _atom;
};

} // namespace wf

#endif // WFORGE_JS_ENGINE_H
