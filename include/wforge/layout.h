#ifndef WFORGE_LAYOUT_H
#define WFORGE_LAYOUT_H

#include "wforge/assets.h"
#include "wforge/runtime.h"
#include <array>
#include <memory>
#include <proxy/proxy.h>
#include <string>
#include <vector>
#include <yoga/Yoga.h>
#include <yoga/node/Node.h>
#include <yoga/style/Style.h>

namespace wf::js {

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemContentMeasure, measure);
PRO_DEF_MEM_DISPATCH(MemContentRender, render);

} // namespace _dispatch

/* clang-format off */
struct ContentFacade : pro::facade_builder
	::add_convention<
		_dispatch::MemContentMeasure,
		YGSize(YGMeasureMode, float, YGMeasureMode, float) const
	>
	::add_convention<
		_dispatch::MemContentRender,
		void(sf::RenderTarget&, JSContext*, int, float, float, float, float) const
	>
	::support_relocation<pro::constraint_level::nontrivial>
	::build {};
/* clang-format on */

struct TextContent final : BindingBase<TextContent> {
	static constexpr const char *CLASS_NAME = "TextContent";
	static constexpr int CTOR_LENGTH = 1;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;
	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	std::string text;
	int size = 1;
	JSValue color = JS_NULL;
	sf::Color _nativeColor{0, 0, 0};

	// ContentFacade implementation
	[[nodiscard]] int charWidth() const noexcept;
	[[nodiscard]] int charHeight() const noexcept;

	[[nodiscard]] sf::Color nativeColor(JSContext *ctx) const noexcept;
	YGSize measure(YGMeasureMode, float, YGMeasureMode, float) const noexcept;
	void render(
		sf::RenderTarget &, JSContext *, int, float, float, float, float
	) const;
};

struct SpriteContent final : BindingBase<SpriteContent> {
	static constexpr const char *CLASS_NAME = "SpriteContent";
	static constexpr int CTOR_LENGTH = 2;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
	) noexcept;

	static void gcMark(
		JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
	) noexcept;
	static void finalize(JSRuntime *rt, JSValue val) noexcept;

	JSValue textureVal = JS_NULL;
	sf::Texture *texture = nullptr; // not owned, managed by AssetsManager
	int size = 1;                   // scale multiplier

	// ContentFacade implementation
	YGSize measure(YGMeasureMode, float, YGMeasureMode, float) const noexcept;
	void render(
		sf::RenderTarget &, JSContext *, int, float, float, float, float
	) const;
};

struct LayoutNode final : BindingBase<LayoutNode> {
	struct ChildEntry {
		JSValue val;
		LayoutNode *node;
	};

	static constexpr const char *CLASS_NAME = "LayoutNode";
	static constexpr int CTOR_LENGTH = 0;
	static const CFunctionList PROTO_FIELDS;

	[[nodiscard]] static JSValue ctor(
		JSContext *, JSValueConst, int, JSValueConst *
	) noexcept;
	static void gcMark(JSRuntime *, JSValueConst, JS_MarkFunc *) noexcept;
	static void finalize(JSRuntime *, JSValue) noexcept;

	LayoutNode();

	void appendChild(JSContext *, LayoutNode *, JSValue);
	void removeChild(JSContext *, LayoutNode *);

	void calculateLayout(float avail_width, float avail_height);
	void render(sf::RenderTarget &, JSContext *, int) const;
	void render(sf::RenderTarget &, JSContext *, int, float, float) const;

	// -- data members --
	facebook::yoga::Node yoga_node;

	pro::proxy_view<ContentFacade> content;
	JSValue content_val = JS_NULL;
	JSValue background_color = JS_NULL;
	// Indexed by YGEdge (Left=0, Top=1, Right=2, Bottom=3)
	std::array<JSValue, 4> border_color = {JS_NULL, JS_NULL, JS_NULL, JS_NULL};
	std::vector<ChildEntry> children;
};

} // namespace wf::js

#endif // WFORGE_LAYOUT_H
