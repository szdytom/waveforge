#ifndef WFORGE_RUNTIME_H
#define WFORGE_RUNTIME_H

#include "ctti.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Event.hpp>
#include <chrono>
#include <concepts>
#include <cstdlib>
#include <exception>
#include <expected>
#include <format>
#include <memory>
#include <proxy/proxy.h>
#include <quickjs.h>
#include <span>
#include <type_traits>
#include <vector>

namespace sf {
class Texture;
class RenderTarget;
} // namespace sf

class PixelFont;

namespace wf::js {

// Minimal replacement for quickjs-libc's js_std_dump_error.
// Prints the current exception and its stack trace to stderr.
void dumpJSError(JSContext *ctx);

struct RuntimeDeleter {
	void operator()(JSRuntime *rt) const noexcept;
};
using RuntimePtr = std::unique_ptr<JSRuntime, RuntimeDeleter>;

class EngineContext;

class Engine {
public:
	Engine();

	Engine(Engine &&) noexcept = default;
	Engine &operator=(Engine &&) noexcept = default;

	EngineContext createContext();

	JSRuntime *runtime() const noexcept {
		return _runtime.get();
	}

	template<typename T>
	JSClassID clsId() const noexcept {
		constexpr auto T_hash = T::typeHash();
		for (const auto [type_hash, class_id] : _cls) {
			if (type_hash == T_hash) {
				return class_id;
			}
		}
		return JS_INVALID_CLASS_ID;
	};

	template<typename T>
	void registerClass() noexcept {
		constexpr auto T_hash = T::typeHash();

		for (const auto &[type_hash, _] : _cls) {
			if (type_hash == T_hash) {
				return;
			}
		}

		JSClassID new_id = 0;
		JS_NewClassID(_runtime.get(), &new_id);
		_cls.push_back({T_hash, new_id});
		T::registerClass(runtime(), new_id);
	}

private:
	struct ClassEntry {
		std::size_t type_hash;
		JSClassID class_id;
	};

	std::vector<ClassEntry> _cls;
	RuntimePtr _runtime;
};

class EngineContext {
public:
	EngineContext() noexcept;
	~EngineContext();

	EngineContext(EngineContext &&other) noexcept;
	EngineContext &operator=(EngineContext &&other) noexcept;

	EngineContext(const EngineContext &) = delete;
	EngineContext &operator=(const EngineContext &) = delete;

	explicit operator bool() const noexcept;

	JSContext *ctx() const noexcept;

	void processTimers();
	void drainPromises();
	void bindTimerGlobals();

	// Type-safe opaque (setOpaque/opaqueFrom must use the same T)
	template<typename T>
	void setOpaque(T *ptr) noexcept {
		_setOpaque(ptr, typeHash<T>());
	}

	template<typename T>
	static T *opaqueFrom(JSContext *ctx) noexcept {
		auto e = _opaqueFrom(ctx);
#ifndef NDEBUG
		if (e.typeHash != typeHash<T>()) {
			std::abort();
		}
#endif
		return static_cast<T *>(e.ptr);
	}

	struct Impl;

private:
	friend class Engine;
	EngineContext(JSRuntime *rt);

	struct OpaqueEntry {
		void *ptr;
		std::size_t typeHash;
	};
	void _setOpaque(void *ptr, std::size_t typeHash) noexcept;
	static OpaqueEntry _opaqueFrom(JSContext *ctx) noexcept;

	std::unique_ptr<Impl> _impl;
};

// RAII wrapper for JSValue to ensure JS_FreeValue is called
class Value {
public:
	Value() noexcept;
	Value(JSContext *ctx, JSValue value) noexcept;
	~Value() noexcept;

	Value(Value &&other) noexcept;
	Value &operator=(Value &&other) noexcept;

	Value(const Value &) = delete;
	Value &operator=(const Value &) = delete;

	[[nodiscard]] Value dup() const;

	[[nodiscard]] JSValue operator*() const noexcept;
	JSValue release() noexcept;

private:
	JSContext *_ctx;
	JSValue _value;
};

using CFunctionList = std::span<const JSCFunctionListEntry>;

template<typename T>
concept HasGCMark = requires {
	{ T::gcMark } -> std::convertible_to<JSClassGCMark *>;
};

template<typename T>
concept HasCtor = requires {
	{ T::ctor } -> std::convertible_to<JSCFunction *>;
	{ T::CTOR_LENGTH } -> std::convertible_to<int>;
};

template<typename T>
concept HasNoCtor = requires {
	{ T::parentProto } -> std::same_as<JSValue (*)(JSContext *)>;
} && T::CTOR_FIELDS.size() == 0;

template<typename T>
concept Bindable = requires {
	{ T::CLASS_NAME } -> std::same_as<const char *>;
	{ T::PROTO_FIELDS } -> std::same_as<CFunctionList>;
	{ T::CTOR_FIELDS } -> std::same_as<CFunctionList>;
} && ((HasCtor<T> + HasNoCtor<T>) == 1); // Not both, not neither

void bindContextImpl(
	JSContext *ctx, JSValueConst ns, const char *class_name, int class_id,
	JSCFunction *ctor_func, int ctor_length, CFunctionList proto_fields,
	CFunctionList ctor_fields, JSValue parent_ctor
);

void bindContextImplNoCtor(
	JSContext *ctx, const char *class_name, int class_id, JSValue parent_proto,
	CFunctionList fields
);

// CRTP base for native bindings class
template<typename Derived>
struct BindingBase {
	static constexpr CFunctionList CTOR_FIELDS{};
	static constexpr CFunctionList PROTO_FIELDS{};

	[[nodiscard]] static JSValue parentCtor(JSContext *ctx) noexcept {
		return JS_UNDEFINED;
	}

	[[nodiscard]] static constexpr std::size_t typeHash() noexcept {
		return fnv1a(Derived::CLASS_NAME);
	}

	[[nodiscard]] static JSClassID clsId(JSRuntime *rt) noexcept {
		return static_cast<Engine *>(JS_GetRuntimeOpaque(rt))
			->template clsId<Derived>();
	}

	[[nodiscard]] static Derived *unwrap(
		JSRuntime *rt, JSValueConst val
	) noexcept {
		return static_cast<Derived *>(JS_GetOpaque(val, clsId(rt)));
	}

	[[nodiscard]] static Derived *unwrap(
		JSContext *ctx, JSValueConst val
	) noexcept {
		return unwrap(JS_GetRuntime(ctx), val);
	}

	static void finalize(JSRuntime *rt, JSValue val) noexcept {
		delete unwrap(rt, val);
	}

	[[nodiscard]] static JSClassGCMark *gcMarkFunc() noexcept {
		if constexpr (HasGCMark<Derived>) {
			return Derived::gcMark;
		} else {
			return nullptr;
		}
	}

	static void registerClass(JSRuntime *rt, JSClassID cls_id) noexcept {
		JSClassDef def = {
			.class_name = Derived::CLASS_NAME,
			.finalizer = Derived::finalize,
			.gc_mark = gcMarkFunc(),
			.call = nullptr,
			.exotic = nullptr,
		};
		JS_NewClass(rt, cls_id, &def);
	}

	static void bindContext(JSContext *ctx, JSValueConst ns) {
		if constexpr (HasCtor<Derived>) {
			bindContextImpl(
				ctx, ns, Derived::CLASS_NAME, clsId(JS_GetRuntime(ctx)),
				Derived::ctor, Derived::CTOR_LENGTH, Derived::PROTO_FIELDS,
				Derived::CTOR_FIELDS, Derived::parentCtor(ctx)
			);
		} else {
			bindContextImplNoCtor(
				ctx, Derived::CLASS_NAME, clsId(JS_GetRuntime(ctx)),
				Derived::parentProto(ctx), Derived::PROTO_FIELDS
			);
		}
	}
};

struct Texture final : BindingBase<Texture> {
	static constexpr const char *CLASS_NAME = "Texture";
	static constexpr int CTOR_LENGTH = 1;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	Texture(sf::Texture *tex, std::string id) noexcept;

	[[nodiscard]] int width() const noexcept;
	[[nodiscard]] int height() const noexcept;

	sf::Texture *texture; // not owned, managed by AssetsManager
	std::string id;
};

struct Color final : BindingBase<Color> {
	static constexpr const char *CLASS_NAME = "Color";
	static constexpr int CTOR_LENGTH = 1;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	Color(sf::Color color) noexcept;

	sf::Color color;

	[[nodiscard]] static std::expected<sf::Color, const char *> interpret(
		JSContext *ctx, JSValueConst val
	);
	[[nodiscard]] static std::expected<JSValue, const char *> interpretAsValue(
		JSContext *ctx, JSValueConst val
	);

	[[nodiscard]] static std::optional<sf::Color> fromValue(
		JSContext *ctx, JSValueConst val
	);
	[[nodiscard]] static JSValue toValue(JSContext *ctx, sf::Color color);
};

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemDrawRender, render);

} // namespace _dispatch

/* clang-format off */
struct DrawCmdFacade : pro::facade_builder
	::add_convention<_dispatch::MemDrawRender, void(sf::RenderTarget&, JSContext*, int) const>
	::support_relocation<pro::constraint_level::nontrivial>
	::build {};
/* clang-format on */

struct DrawTextCmd final : BindingBase<DrawTextCmd> {
	static constexpr const char *CLASS_NAME = "DrawTextCmd";
	static constexpr int CTOR_LENGTH = 3;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;
	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	std::string text;
	int x;
	int y;
	int size;
	JSValue color = JS_NULL;
	const PixelFont *_font = nullptr; // not owned, managed by AssetsManager

	DrawTextCmd(std::string text, int x, int y) noexcept;

	[[nodiscard]] sf::Color nativeColor(JSContext *ctx) const noexcept;
	void render(sf::RenderTarget &target, JSContext *ctx, int scale) const;
};

struct DrawSpriteCmd final : BindingBase<DrawSpriteCmd> {
	static constexpr const char *CLASS_NAME = "DrawSpriteCmd";
	static constexpr int CTOR_LENGTH = 3;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	JSValue textureVal;
	sf::Texture *texture; // not owned, managed by AssetsManager
	int x;
	int y;

	DrawSpriteCmd(
		JSValue textureVal, sf::Texture *texture, int x, int y
	) noexcept;

	void render(sf::RenderTarget &target, JSContext *ctx, int scale) const;
};

struct DrawRectCmd final : BindingBase<DrawRectCmd> {
	static constexpr const char *CLASS_NAME = "DrawRectCmd";
	static constexpr int CTOR_LENGTH = 4;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;
	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	int x;
	int y;
	int width;
	int height;
	JSValue color = JS_NULL;

	DrawRectCmd(int x, int y, int width, int height) noexcept;

	[[nodiscard]] sf::Color nativeColor(JSContext *ctx) const noexcept;
	void render(sf::RenderTarget &target, JSContext *ctx, int scale) const;
};

struct DrawCmdList final : BindingBase<DrawCmdList> {
	static constexpr const char *CLASS_NAME = "DrawCmdList";
	static constexpr int CTOR_LENGTH = 0;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	struct DrawCmdEntry {
		JSValue val;
		pro::proxy_view<DrawCmdFacade> cmd;
	};

	void render(sf::RenderTarget &target, JSContext *ctx, int scale) const;

	std::vector<DrawCmdEntry> cmds;
};

struct DrawCmdListIter final : BindingBase<DrawCmdListIter> {
	static constexpr const char *CLASS_NAME = "DrawCmdListIter";
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue parentProto(JSContext *ctx) noexcept;

	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	std::size_t index;
	DrawCmdList *list;
	JSValue list_val;
};

struct KeyEvent final : BindingBase<KeyEvent> {
	enum class Type {
		KeyDown,
		KeyUp
	};

	static constexpr const char *CLASS_NAME = "KeyEvent";
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue parentProto(JSContext *ctx) noexcept;

	Type type;
	std::string code;
	bool alt;
	bool control;
	bool shift;
	bool system;

	KeyEvent(
		Type type, std::string code, bool alt, bool control, bool shift,
		bool system
	) noexcept;

	[[nodiscard]] static JSValue from(
		JSContext *ctx, const sf::Event::KeyPressed &evt
	) noexcept;

	[[nodiscard]] static JSValue from(
		JSContext *ctx, const sf::Event::KeyReleased &evt
	) noexcept;
};

struct MouseButtonEvent final : BindingBase<MouseButtonEvent> {
	enum class Type {
		MouseDown,
		MouseUp
	};

	static constexpr const char *CLASS_NAME = "MouseButtonEvent";
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue parentProto(JSContext *ctx) noexcept;

	Type type;
	int button;
	int x;
	int y;

	MouseButtonEvent(Type type, int button, int x, int y) noexcept;

	[[nodiscard]] static JSValue from(
		JSContext *ctx, const sf::Event::MouseButtonPressed &evt, int scale = 1
	) noexcept;

	[[nodiscard]] static JSValue from(
		JSContext *ctx, const sf::Event::MouseButtonReleased &evt, int scale = 1
	) noexcept;
};

struct MouseMoveEvent final : BindingBase<MouseMoveEvent> {
	enum class Type {
		MouseMove
	};

	static constexpr const char *CLASS_NAME = "MouseMoveEvent";
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue parentProto(JSContext *ctx) noexcept;

	Type type;
	int x;
	int y;

	MouseMoveEvent(Type type, int x, int y) noexcept;

	[[nodiscard]] static JSValue from(
		JSContext *ctx, const sf::Event::MouseMoved &evt, int scale = 1
	) noexcept;
};

template<typename... Ts>
struct BindingList {
	static void registerClass(Engine &engine) noexcept {
		(engine.registerClass<Ts>(), ...);
	}

	static void bindContext(JSContext *ctx, JSValueConst ns) {
		(Ts::bindContext(ctx, ns), ...);
	}
};

} // namespace wf::js

#endif // WFORGE_RUNTIME_H
