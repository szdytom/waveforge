#ifndef WFORGE_RUNTIME_H
#define WFORGE_RUNTIME_H

#include "ctti.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Event.hpp>
#include <concepts>
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

namespace wf::js {

// Minimal replacement for quickjs-libc's js_std_dump_error.
// Prints the current exception and its stack trace to stderr.
void dumpJSError(JSContext *ctx);

struct RuntimeDeleter {
	void operator()(JSRuntime *rt) const noexcept;
};
using RuntimePtr = std::unique_ptr<JSRuntime, RuntimeDeleter>;

struct ContextDeleter {
	void operator()(JSContext *ctx) const noexcept;
};
using ContextPtr = std::unique_ptr<JSContext, ContextDeleter>;

class Engine {
public:
	Engine();

	Engine(Engine &&) noexcept = default;
	Engine &operator=(Engine &&) noexcept = default;

	JSContext *createContext();
	void destroyContext(JSContext *ctx) noexcept;

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
	std::vector<ContextPtr> _contexts;
};

// RAII wrapper for JSValue to ensure JS_FreeValue is called
// Use as function local variable only, do not return or store in class fields
class ValueGuard {
public:
	ValueGuard() noexcept: _ctx(nullptr), _value(JS_UNDEFINED) {}
	ValueGuard(JSContext *ctx, JSValue value) noexcept
		: _ctx(ctx), _value(value) {}

	~ValueGuard() noexcept {
		if (_ctx) {
			JS_FreeValue(_ctx, _value);
		}
	}

	JSValue get() const noexcept {
		return _value;
	}

	JSValue release() noexcept {
		auto tmp = _value;
		_value = JS_UNDEFINED;
		return tmp;
	}

	ValueGuard(const ValueGuard &) = delete;
	ValueGuard &operator=(const ValueGuard &) = delete;
	ValueGuard(ValueGuard &&other) = delete;
	ValueGuard &operator=(ValueGuard &&other) = delete;

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

	static JSValue parentCtor(JSContext *ctx) noexcept {
		return JS_UNDEFINED;
	}

	static constexpr std::size_t typeHash() noexcept {
		return fnv1a(Derived::CLASS_NAME);
	}

	static JSClassID clsId(JSRuntime *rt) noexcept {
		return static_cast<Engine *>(JS_GetRuntimeOpaque(rt))
			->template clsId<Derived>();
	}

	static Derived *unwrap(JSRuntime *rt, JSValueConst val) noexcept {
		return static_cast<Derived *>(JS_GetOpaque(val, clsId(rt)));
	}

	static Derived *unwrap(JSContext *ctx, JSValueConst val) noexcept {
		return unwrap(JS_GetRuntime(ctx), val);
	}

	static void finalize(JSRuntime *rt, JSValue val) noexcept {
		delete unwrap(rt, val);
	}

	static JSClassGCMark *gcMarkFunc() noexcept {
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

	static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	Texture(sf::Texture *tex, std::string id) noexcept;

	int width() const noexcept;
	int height() const noexcept;

	sf::Texture *texture; // not owned, managed by AssetsManager
	std::string id;
};

struct Color final : BindingBase<Color> {
	static constexpr const char *CLASS_NAME = "Color";
	static constexpr int CTOR_LENGTH = 1;
	static const CFunctionList PROTO_FIELDS;

	static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	Color(sf::Color color) noexcept;

	sf::Color color;

	static std::expected<sf::Color, const char *> interpret(
		JSContext *ctx, JSValueConst val
	);
};

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemDrawRender, render);

} // namespace _dispatch

/* clang-format off */
struct DrawCmdFacade : pro::facade_builder
	::add_convention<_dispatch::MemDrawRender, void(sf::RenderTarget&, int) const>
	::support_relocation<pro::constraint_level::nontrivial>
	::build {};
/* clang-format on */

struct DrawTextCmd final : BindingBase<DrawTextCmd> {
	static constexpr const char *CLASS_NAME = "DrawTextCmd";
	static constexpr int CTOR_LENGTH = 3;
	static const CFunctionList PROTO_FIELDS;

	static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	std::string text;
	int x;
	int y;
	int size;
	sf::Color color;

	DrawTextCmd(std::string text, int x, int y) noexcept;

	void render(sf::RenderTarget &target, int scale) const;
};

struct DrawSpriteCmd final : BindingBase<DrawSpriteCmd> {
	static constexpr const char *CLASS_NAME = "DrawSpriteCmd";
	static constexpr int CTOR_LENGTH = 3;
	static const CFunctionList PROTO_FIELDS;

	static JSValue ctor(
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

	void render(sf::RenderTarget &target, int scale) const;
};

struct DrawRectCmd final : BindingBase<DrawRectCmd> {
	static constexpr const char *CLASS_NAME = "DrawRectCmd";
	static constexpr int CTOR_LENGTH = 4;
	static const CFunctionList PROTO_FIELDS;

	static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	int x;
	int y;
	int width;
	int height;
	sf::Color color;

	DrawRectCmd(int x, int y, int width, int height) noexcept;

	void render(sf::RenderTarget &target, int scale) const;
};

struct DrawCmdList final : BindingBase<DrawCmdList> {
	static constexpr const char *CLASS_NAME = "DrawCmdList";
	static constexpr int CTOR_LENGTH = 0;
	static const CFunctionList PROTO_FIELDS;

	static JSValue ctor(
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

	void render(sf::RenderTarget &target, int scale) const;

	std::vector<DrawCmdEntry> cmds;
};

struct DrawCmdListIter final : BindingBase<DrawCmdListIter> {
	static constexpr const char *CLASS_NAME = "DrawCmdListIter";
	static const CFunctionList PROTO_FIELDS;

	static JSValue parentProto(JSContext *ctx) noexcept;

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

	static JSValue parentProto(JSContext *ctx) noexcept;

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

	static JSValue from(
		JSContext *ctx, const sf::Event::KeyPressed &evt
	) noexcept;
	static JSValue from(
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

	static JSValue parentProto(JSContext *ctx) noexcept;

	Type type;
	int button;
	int x;
	int y;

	MouseButtonEvent(Type type, int button, int x, int y) noexcept;

	static JSValue from(
		JSContext *ctx, const sf::Event::MouseButtonPressed &evt
	) noexcept;
	static JSValue from(
		JSContext *ctx, const sf::Event::MouseButtonReleased &evt
	) noexcept;
};

struct MouseMoveEvent final : BindingBase<MouseMoveEvent> {
	enum class Type {
		MouseMove
	};

	static constexpr const char *CLASS_NAME = "MouseMoveEvent";
	static const CFunctionList PROTO_FIELDS;

	static JSValue parentProto(JSContext *ctx) noexcept;

	Type type;
	int x;
	int y;

	MouseMoveEvent(Type type, int x, int y) noexcept;

	static JSValue from(
		JSContext *ctx, const sf::Event::MouseMoved &evt
	) noexcept;
};

} // namespace wf::js

#endif // WFORGE_RUNTIME_H
