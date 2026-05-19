#include "wforge/layout.h"
#include "hacks.h"
#include "helper.h"
#include <charconv>
#include <cmath>
#include <string_view>

namespace wf::js {

namespace {

// Resolve a dimension under a Yoga measure constraint.
//   intrinsic: the content's natural size (or 0 if undefined)
//   avail:     the available size from the parent
//   mode:      the constraint mode from Yoga
float resolveDim(float intrinsic, float avail, YGMeasureMode mode) noexcept {
	switch (mode) {
	case YGMeasureModeExactly:
		return avail;
	case YGMeasureModeAtMost:
		return std::min(avail, intrinsic);
	case YGMeasureModeUndefined:
	default:
		return intrinsic;
	}
}

[[nodiscard]] JSValue ygValueToJS(JSContext *ctx, YGValue v) noexcept {
	switch (v.unit) {
	case YGUnitUndefined:
		return JS_UNDEFINED;
	case YGUnitAuto:
		return JS_NewString(ctx, "auto");
	case YGUnitPoint:
		if (YGFloatIsUndefined(v.value)) {
			return JS_UNDEFINED;
		}
		return JS_NewFloat64(ctx, v.value);
	case YGUnitPercent: {
		char buf[64];
		auto [ptr, ec] = std::to_chars(
			buf, buf + sizeof(buf), v.value, std::chars_format::fixed
		);
		if (ec == std::errc()) {
			*ptr++ = '%';
			return JS_NewStringLen(ctx, buf, ptr - buf);
		}
		return JS_UNDEFINED;
	}
	case YGUnitMaxContent:
		return JS_NewString(ctx, "max-content");
	case YGUnitFitContent:
		return JS_NewString(ctx, "fit-content");
	case YGUnitStretch:
		return JS_NewString(ctx, "stretch");
	}
	return JS_UNDEFINED;
}

std::pair<int, int> computeContentOffset(
	int contentW, int contentH, const LayoutParameters &lp
) noexcept {
	int dx = 0;
	switch (lp.align.h) {
	case ContentAlignH::Center:
		dx = (lp.w - contentW) / 2;
		break;
	case ContentAlignH::Right:
		dx = lp.w - contentW;
		break;
	default:
		break;
	}
	int dy = 0;
	switch (lp.align.v) {
	case ContentAlignV::Horizon:
		dy = (lp.h - contentH) / 2;
		break;
	case ContentAlignV::Bottom:
		dy = lp.h - contentH;
		break;
	default:
		break;
	}
	return {dx, dy};
}

YGConfigRef pixelConfig() noexcept {
	static YGConfigRef config = []() noexcept {
		YGConfigRef cfg = YGConfigNew();
		YGConfigSetPointScaleFactor(cfg, 1.0f);
		return cfg;
	}();
	return config;
}

// Word-wrapping helper: splits text into lines using greedy word-level
// wrapping. Each returned pair is (start, end) character index in the original
// text.
struct WrappedLine {
	size_t start;
	size_t end;
};

[[nodiscard]] static std::vector<WrappedLine> computeWrappedLines(
	std::string_view text, int maxPixels, int charW
) noexcept {
	std::vector<WrappedLine> lines;
	if (text.empty()) {
		return lines;
	}

	int maxPerLine = std::max(1, maxPixels / charW);

	size_t pos = 0;
	size_t lineStart = 0;
	int curLen = 0;
	size_t lastWordEnd = 0;

	while (true) {
		// skip leading whitespace
		while (pos < text.size()
		       && std::isspace(static_cast<unsigned char>(text[pos]))) {
			pos++;
		}
		if (pos >= text.size()) {
			break;
		}

		size_t wordStart = pos;
		while (pos < text.size()
		       && !std::isspace(static_cast<unsigned char>(text[pos]))) {
			pos++;
		}
		size_t wordEnd = pos;
		int wordLen = static_cast<int>(wordEnd - wordStart);

		if (curLen == 0) {
			lineStart = wordStart;
			lastWordEnd = wordEnd;
			curLen = wordLen;
		} else if ((curLen + 1 + wordLen) <= maxPerLine) {
			lastWordEnd = wordEnd;
			curLen += 1 + wordLen;
		} else {
			lines.push_back({lineStart, lastWordEnd});
			lineStart = wordStart;
			lastWordEnd = wordEnd;
			curLen = wordLen;
		}
	}

	if (curLen > 0) {
		lines.push_back({lineStart, lastWordEnd});
	}

	return lines;
}

} // namespace

// ===== Content::measure/render implementations =====

int TextContent::charWidth() const noexcept {
	return _font->charWidth(size);
}

int TextContent::charHeight() const noexcept {
	return _font->charHeight(size);
}

YGSize TextContent::measure(
	YGMeasureMode width_mode, float width, YGMeasureMode height_mode,
	float height
) const noexcept {
	if (text.empty()) {
		return {
			resolveDim(0, width, width_mode),
			resolveDim(static_cast<float>(charHeight()), height, height_mode),
		};
	}

	float measuredW;
	int lineCount;

	if (width_mode == YGMeasureModeUndefined) {
		measuredW = static_cast<float>(text.length() * charWidth());
		lineCount = 1;
	} else {
		auto lines = computeWrappedLines(
			text, static_cast<int>(width), charWidth()
		);
		lineCount = static_cast<int>(lines.size());
		if (lines.empty()) {
			return {
				resolveDim(0, width, width_mode),
				resolveDim(
					static_cast<float>(charHeight()), height, height_mode
				),
			};
		}
		measuredW = 0;
		for (auto &l : lines) {
			float lw = static_cast<float>((l.end - l.start) * charWidth());
			if (lw > measuredW) {
				measuredW = lw;
			}
		}
		measuredW = std::min(width, measuredW);
	}

	float measuredH = static_cast<float>(lineCount * charHeight());

	return {
		resolveDim(measuredW, width, width_mode),
		resolveDim(measuredH, height, height_mode),
	};
}

void TextContent::render(
	sf::RenderTarget &target, JSContext *ctx, LayoutParameters lp
) const {
	int cw = charWidth();
	int ch = charHeight();

	auto lines = computeWrappedLines(text, lp.w, cw);
	if (lines.empty()) {
		return;
	}

	// total text block dimensions for alignment
	int textW = 0;
	for (auto &l : lines) {
		int lw = static_cast<int>(l.end - l.start) * cw;
		if (lw > textW) {
			textW = lw;
		}
	}
	int textH = static_cast<int>(lines.size()) * ch;

	auto [dx, dy] = computeContentOffset(textW, textH, lp);

	int y = lp.y + dy;
	for (auto &l : lines) {
		_font->renderText(
			target, std::string_view(text.data() + l.start, l.end - l.start),
			nativeColor(ctx), lp.x + dx, y, lp.scale, size
		);
		y += ch;
	}
}

sf::Color TextContent::nativeColor(JSContext *ctx) const noexcept {
	return Color::fromValue(ctx, color).value_or(sf::Color::Black);
}

YGSize SpriteContent::measure(
	YGMeasureMode width_mode, float width, YGMeasureMode height_mode,
	float height
) const noexcept {
	float texW = texture->getSize().x * size;
	float texH = texture->getSize().y * size;
	return {
		resolveDim(texW, width, width_mode),
		resolveDim(texH, height, height_mode),
	};
}

void SpriteContent::render(
	sf::RenderTarget &target, JSContext * /*ctx*/, LayoutParameters lp
) const {
	int spriteW = static_cast<int>(texture->getSize().x) * size;
	int spriteH = static_cast<int>(texture->getSize().y) * size;

	auto [dx, dy] = computeContentOffset(spriteW, spriteH, lp);

	sf::Sprite sprite(*texture);
	sprite.setPosition(
		sf::Vector2f((lp.x + dx) * lp.scale, (lp.y + dy) * lp.scale)
	);
	sprite.setScale(sf::Vector2f(size * lp.scale, size * lp.scale));
	target.draw(sprite);
}

// ===== LayoutNode =====

LayoutNode::LayoutNode() {
	YGNodeSetConfig(&yoga_node, pixelConfig());
	YGNodeSetContext(&yoga_node, this);
}

void LayoutNode::appendChild(JSContext *ctx, LayoutNode *child) {
	if (child->parent) {
		child->parent->removeChild(ctx, child);
	}
	JS_DupValue(ctx, child->self_val);
	child->parent = this;
	if (!first_child) {
		first_child = last_child = child;
	} else {
		last_child->next_sibling = child;
		last_child = child;
	}
	YGNodeInsertChild(
		&yoga_node, &child->yoga_node, YGNodeGetChildCount(&yoga_node)
	);
}

void LayoutNode::removeChild(JSContext *ctx, LayoutNode *child) {
	LayoutNode *prev = nullptr;
	bool found = false;
	for (auto *cur = first_child; cur; prev = cur, cur = cur->next_sibling) {
		if (cur == child) {
			found = true;
			if (prev) {
				prev->next_sibling = child->next_sibling;
			} else {
				first_child = child->next_sibling;
			}
			if (child == last_child) {
				last_child = prev;
			}
			child->parent = nullptr;
			child->next_sibling = nullptr;
			JS_FreeValue(ctx, child->self_val);
			break;
		}
	}
	if (found) {
		YGNodeRemoveChild(&yoga_node, &child->yoga_node);
	}
}

void LayoutNode::insertBefore(
	JSContext *ctx, LayoutNode *newChild, LayoutNode *referenceChild
) {
	if (!referenceChild) {
		appendChild(ctx, newChild);
		return;
	}

	// First verify referenceChild exists
	bool found = false;
	for (auto *cur = first_child; cur; cur = cur->next_sibling) {
		if (cur == referenceChild) {
			found = true;
			break;
		}
	}
	if (!found) {
		JS_ThrowTypeError(ctx, "referenceChild is not a child of this node");
		return;
	}

	// Detach newChild from old parent
	if (newChild->parent) {
		newChild->parent->removeChild(ctx, newChild);
	}

	// Find referenceChild and insert newChild before it
	size_t index = 0;
	LayoutNode *prev = nullptr;
	for (auto *cur = first_child; cur;
	     prev = cur, cur = cur->next_sibling, index++) {
		if (cur == referenceChild) {
			JS_DupValue(ctx, newChild->self_val);
			newChild->parent = this;
			if (prev) {
				prev->next_sibling = newChild;
			} else {
				first_child = newChild;
			}
			newChild->next_sibling = referenceChild;
			YGNodeInsertChild(&yoga_node, &newChild->yoga_node, index);
			return;
		}
	}
}

void LayoutNode::replaceChild(
	JSContext *ctx, LayoutNode *newChild, LayoutNode *oldChild
) {
	if (newChild == oldChild) {
		return;
	}

	// First verify oldChild exists
	bool found = false;
	for (auto *cur = first_child; cur; cur = cur->next_sibling) {
		if (cur == oldChild) {
			found = true;
			break;
		}
	}
	if (!found) {
		JS_ThrowTypeError(ctx, "oldChild is not a child of this node");
		return;
	}

	// Detach newChild from old parent (may modify this list if same parent)
	if (newChild->parent) {
		newChild->parent->removeChild(ctx, newChild);
	}

	// Re-derive oldChild position after potential list modification
	size_t index = 0;
	LayoutNode *prev = nullptr;
	for (auto *cur = first_child; cur;
	     prev = cur, cur = cur->next_sibling, index++) {
		if (cur == oldChild) {
			JS_DupValue(ctx, newChild->self_val);
			JS_FreeValue(ctx, oldChild->self_val);
			newChild->parent = this;
			if (prev) {
				prev->next_sibling = newChild;
			} else {
				first_child = newChild;
			}
			newChild->next_sibling = oldChild->next_sibling;
			if (oldChild == last_child) {
				last_child = newChild;
			}
			oldChild->parent = nullptr;
			oldChild->next_sibling = nullptr;
			YGNodeRemoveChild(&yoga_node, &oldChild->yoga_node);
			YGNodeInsertChild(&yoga_node, &newChild->yoga_node, index);
			return;
		}
	}
}

void LayoutNode::calculateLayout(float availWidth, float availHeight) {
	YGNodeCalculateLayout(&yoga_node, availWidth, availHeight, YGDirectionLTR);
}

void LayoutNode::relayout() noexcept {
	YGNodeMarkDirty(&yoga_node);
}

void LayoutNode::render(
	sf::RenderTarget &target, JSContext *ctx, int scale
) const {
	LayoutParameters lp{
		.scale = scale,
		.x = static_cast<int>(std::round(YGNodeLayoutGetLeft(&yoga_node))),
		.y = static_cast<int>(std::round(YGNodeLayoutGetTop(&yoga_node))),
		.w = static_cast<int>(std::round(YGNodeLayoutGetWidth(&yoga_node))),
		.h = static_cast<int>(std::round(YGNodeLayoutGetHeight(&yoga_node))),
	};
	render(target, ctx, lp);
}

void LayoutNode::_drawBackground(
	sf::RenderTarget &target, JSContext *ctx, const LayoutParameters &lp
) const {
	auto bg_color = Color::fromValue(ctx, background_color);
	if (!bg_color || bg_color->a == 0) {
		return;
	}
	sf::RectangleShape bg(sf::Vector2f(lp.w * lp.scale, lp.h * lp.scale));
	bg.setPosition(sf::Vector2f(lp.x * lp.scale, lp.y * lp.scale));
	bg.setFillColor(*bg_color);
	target.draw(bg);
}

void LayoutNode::_drawBorders(
	sf::RenderTarget &target, JSContext *ctx, const LayoutParameters &lp
) const {
	auto scale = lp.scale;
	auto abs_x = lp.x;
	auto abs_y = lp.y;
	auto w = lp.w;
	auto h = lp.h;

	static constexpr YGEdge BORDER_EDGES[4] = {
		YGEdgeLeft,
		YGEdgeTop,
		YGEdgeRight,
		YGEdgeBottom,
	};
	for (auto edge : BORDER_EDGES) {
		float bw = YGNodeStyleGetBorder(&yoga_node, edge);
		if (bw <= 0) {
			continue;
		}

		auto color = Color::fromValue(ctx, border_color[edge]);
		if (!color || color->a == 0) {
			continue;
		}

		float bx = abs_x, by = abs_y, bw2 = w, bh2 = h;
		switch (edge) {
		case YGEdgeLeft:
			bw2 = bw;
			break;
		case YGEdgeRight:
			bx = abs_x + w - bw;
			bw2 = bw;
			break;
		case YGEdgeTop:
			bh2 = bw;
			break;
		case YGEdgeBottom:
			by = abs_y + h - bw;
			bh2 = bw;
			break;
		default:
			break;
		}

		sf::RectangleShape borderRect(sf::Vector2f(bw2 * scale, bh2 * scale));
		borderRect.setPosition(sf::Vector2f(bx * scale, by * scale));
		borderRect.setFillColor(*color);
		target.draw(borderRect);
	}
}

void LayoutNode::render(
	sf::RenderTarget &target, JSContext *ctx, LayoutParameters lp
) const {
	_drawBackground(target, ctx, lp);
	_drawBorders(target, ctx, lp);

	if (content.has_value()) {
		auto padLeft = static_cast<int>(
			std::round(YGNodeLayoutGetPadding(&yoga_node, YGEdgeLeft))
		);
		auto padTop = static_cast<int>(
			std::round(YGNodeLayoutGetPadding(&yoga_node, YGEdgeTop))
		);
		auto padRight = static_cast<int>(
			std::round(YGNodeLayoutGetPadding(&yoga_node, YGEdgeRight))
		);
		auto padBottom = static_cast<int>(
			std::round(YGNodeLayoutGetPadding(&yoga_node, YGEdgeBottom))
		);
		LayoutParameters content_lp{
			.scale = lp.scale,
			.x = lp.x + padLeft,
			.y = lp.y + padTop,
			.w = std::max(0, lp.w - padLeft - padRight),
			.h = std::max(0, lp.h - padTop - padBottom),
			.align = content_align,
		};
		content->render(target, ctx, content_lp);
	}

	for (size_t i = 0, n = YGNodeGetChildCount(&yoga_node); i < n; i++) {
		auto *child = static_cast<LayoutNode *>(YGNodeGetContext(
			YGNodeGetChild(const_cast<facebook::yoga::Node *>(&yoga_node), i)
		));
		LayoutParameters child_frame{
			.scale = lp.scale,
			.x = lp.x
				+ static_cast<int>(
					 std::round(YGNodeLayoutGetLeft(&child->yoga_node))
				),
			.y = lp.y
				+ static_cast<int>(
					 std::round(YGNodeLayoutGetTop(&child->yoga_node))
				),
			.w = static_cast<int>(
				std::round(YGNodeLayoutGetWidth(&child->yoga_node))
			),
			.h = static_cast<int>(
				std::round(YGNodeLayoutGetHeight(&child->yoga_node))
			),
		};
		child->render(target, ctx, child_frame);
	}
}

// ===== Yoga measure callback =====

namespace {

YGSize yogaMeasureFunc(
	YGNodeConstRef node, float width, YGMeasureMode width_mode, float height,
	YGMeasureMode height_mode
) noexcept {
	auto *self = static_cast<LayoutNode *>(YGNodeGetContext(node));
	if (self->content.has_value()) {
		return self->content->measure(width_mode, width, height_mode, height);
	}
	return YGSize{0, 0};
}

// ===== JS getters/setters =====

// -- TextContent bindings --
WF_JS_DEF_GETTER_STR(TextContent, getText, self->text.c_str())
WF_JS_DEF_SETTER_STR(TextContent, setText, text)

WF_JS_DEF_GETTER_I32(TextContent, getSize, self->size)
WF_JS_DEF_SETTER(TextContent, setSize, {
	int32_t v;
	JS_ToInt32(ctx, &v, val);
	self->size = std::max(1, v);
})

JSValue TextContent_getColor(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = TextContent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->color);
}

JSValue TextContent_setColor(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = TextContent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto color_object = Color::interpretAsValue(ctx, val);
	if (!color_object) {
		return JS_ThrowTypeError(ctx, "%s", color_object.error());
	}

	JS_FreeValue(ctx, self->color);
	self->color = *color_object;
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry TEXT_CONTENT_PROTO[] = {
	cGetSetDef("text", TextContent_getText, TextContent_setText),
	cGetSetDef("size", TextContent_getSize, TextContent_setSize),
	cGetSetDef("color", TextContent_getColor, TextContent_setColor),
	propStringDef("[Symbol.toStringTag]", "TextContent", JS_PROP_CONFIGURABLE),
};

} // anonymous namespace

const CFunctionList TextContent::PROTO_FIELDS{TEXT_CONTENT_PROTO};

JSValue TextContent::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto self = std::make_unique<TextContent>();
	self->_font = &AssetsManager::instance().getAsset<PixelFont>("font");
	if (argc > 0) {
		const char *s = JS_ToCString(ctx, argv[0]);
		if (s) {
			self->text = s;
			JS_FreeCString(ctx, s);
		}
	}
	if (argc > 1) {
		int32_t v;
		JS_ToInt32(ctx, &v, argv[1]);
		self->size = std::max(1, v);
	}
	self->color = Color::toValue(ctx, sf::Color::Black);
	if (argc > 2) {
		auto cv = Color::interpretAsValue(ctx, argv[2]);
		if (!cv) {
			return JS_ThrowTypeError(ctx, "%s", cv.error());
		}
		JS_FreeValue(ctx, self->color);
		self->color = *cv;
	}
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		JS_FreeValue(ctx, self->color);
		return obj;
	}
	JS_SetOpaque(obj, self.release());
	return obj;
}

void TextContent::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_MarkValue(rt, self->color, mark_func);
}

void TextContent::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_FreeValueRT(rt, self->color);
	delete self;
}

// -- SpriteContent bindings --
namespace {

JSValue SpriteContent_getTexture(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = SpriteContent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->textureVal);
}

JSValue SpriteContent_setTexture(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = SpriteContent::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	auto *texObj = Texture::unwrap(ctx, val);
	if (!texObj) {
		return JS_ThrowTypeError(ctx, "Expected a Texture");
	}
	JS_FreeValue(ctx, self->textureVal);
	self->textureVal = JS_DupValue(ctx, val);
	self->texture = texObj->texture;
	return JS_UNDEFINED;
}

WF_JS_DEF_GETTER_I32(SpriteContent, getSize, self->size)
WF_JS_DEF_SETTER(SpriteContent, setSize, {
	int32_t v;
	JS_ToInt32(ctx, &v, val);
	self->size = std::max(1, v);
})

static const JSCFunctionListEntry SPRITE_CONTENT_PROTO[] = {
	cGetSetDef("texture", SpriteContent_getTexture, SpriteContent_setTexture),
	cGetSetDef("size", SpriteContent_getSize, SpriteContent_setSize),
	propStringDef(
		"[Symbol.toStringTag]", "SpriteContent", JS_PROP_CONFIGURABLE
	),
};

} // anonymous namespace

const CFunctionList SpriteContent::PROTO_FIELDS{SPRITE_CONTENT_PROTO};

JSValue SpriteContent::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	if (argc < 1) {
		return JS_ThrowTypeError(ctx, "Expected a Texture argument");
	}
	auto *texObj = Texture::unwrap(ctx, argv[0]);
	if (!texObj) {
		return JS_ThrowTypeError(ctx, "Expected a Texture");
	}
	auto self = std::make_unique<SpriteContent>();
	self->textureVal = JS_DupValue(ctx, argv[0]);
	self->texture = texObj->texture;
	if (argc > 1) {
		int32_t v;
		JS_ToInt32(ctx, &v, argv[1]);
		self->size = std::max(1, v);
	}
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		JS_FreeValue(ctx, self->textureVal);
		return obj;
	}
	JS_SetOpaque(obj, self.release());
	return obj;
}

void SpriteContent::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_MarkValue(rt, self->textureVal, mark_func);
}

void SpriteContent::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_FreeValueRT(rt, self->textureVal);
	delete self;
}

namespace {

// -- dimension properties (dispatched via magic) --
enum class LayoutDim {
	Width,
	Height,
	MinWidth,
	MaxWidth,
	MinHeight,
	MaxHeight,
};

JSValue LayoutNode_getDimProp(
	JSContext *ctx, JSValueConst this_val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	YGValue v;
	switch (static_cast<LayoutDim>(magic)) {
	case LayoutDim::Width:
		v = YGNodeStyleGetWidth(&self->yoga_node);
		break;
	case LayoutDim::Height:
		v = YGNodeStyleGetHeight(&self->yoga_node);
		break;
	case LayoutDim::MinWidth:
		v = YGNodeStyleGetMinWidth(&self->yoga_node);
		break;
	case LayoutDim::MaxWidth:
		v = YGNodeStyleGetMaxWidth(&self->yoga_node);
		break;
	case LayoutDim::MinHeight:
		v = YGNodeStyleGetMinHeight(&self->yoga_node);
		break;
	case LayoutDim::MaxHeight:
		v = YGNodeStyleGetMaxHeight(&self->yoga_node);
		break;
	}

	return ygValueToJS(ctx, v);
}

JSValue LayoutNode_setDimProp(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto dim = static_cast<LayoutDim>(magic);

	// undefined / null → set undefined (YGUndefined)
	if (JS_IsUndefined(val) || JS_IsNull(val)) {
		switch (dim) {
		case LayoutDim::Width:
			YGNodeStyleSetWidth(&self->yoga_node, YGUndefined);
			break;
		case LayoutDim::Height:
			YGNodeStyleSetHeight(&self->yoga_node, YGUndefined);
			break;
		case LayoutDim::MinWidth:
			YGNodeStyleSetMinWidth(&self->yoga_node, YGUndefined);
			break;
		case LayoutDim::MaxWidth:
			YGNodeStyleSetMaxWidth(&self->yoga_node, YGUndefined);
			break;
		case LayoutDim::MinHeight:
			YGNodeStyleSetMinHeight(&self->yoga_node, YGUndefined);
			break;
		case LayoutDim::MaxHeight:
			YGNodeStyleSetMaxHeight(&self->yoga_node, YGUndefined);
			break;
		}
		return JS_UNDEFINED;
	}

	// number → point
	double v;
	if (JS_ToFloat64(ctx, &v, val) == 0) {
		switch (dim) {
		case LayoutDim::Width:
			YGNodeStyleSetWidth(&self->yoga_node, static_cast<float>(v));
			break;
		case LayoutDim::Height:
			YGNodeStyleSetHeight(&self->yoga_node, static_cast<float>(v));
			break;
		case LayoutDim::MinWidth:
			YGNodeStyleSetMinWidth(&self->yoga_node, static_cast<float>(v));
			break;
		case LayoutDim::MaxWidth:
			YGNodeStyleSetMaxWidth(&self->yoga_node, static_cast<float>(v));
			break;
		case LayoutDim::MinHeight:
			YGNodeStyleSetMinHeight(&self->yoga_node, static_cast<float>(v));
			break;
		case LayoutDim::MaxHeight:
			YGNodeStyleSetMaxHeight(&self->yoga_node, static_cast<float>(v));
			break;
		}
		return JS_UNDEFINED;
	}

	// string
	size_t len;
	const char *s = JS_ToCStringLen(ctx, &len, val);
	if (!s) {
		return JS_UNDEFINED;
	}
	std::string_view sv(s, len);

	// "auto"
	if (sv == "auto") {
		switch (dim) {
		case LayoutDim::Width:
			YGNodeStyleSetWidthAuto(&self->yoga_node);
			break;
		case LayoutDim::Height:
			YGNodeStyleSetHeightAuto(&self->yoga_node);
			break;
		default:
			JS_FreeCString(ctx, s);
			return JS_ThrowTypeError(
				ctx, "\"auto\" is only valid for width and height"
			);
		}
		JS_FreeCString(ctx, s);
		return JS_UNDEFINED;
	}

	// "XX%" → percent
	if (!sv.empty() && sv.back() == '%') {
		auto numPart = sv.substr(0, sv.size() - 1);
		float pct;
		auto result = std::from_chars(
			numPart.data(), numPart.data() + numPart.size(), pct
		);
		if (result.ec == std::errc()
		    && result.ptr == numPart.data() + numPart.size()) {
			switch (dim) {
			case LayoutDim::Width:
				YGNodeStyleSetWidthPercent(&self->yoga_node, pct);
				break;
			case LayoutDim::Height:
				YGNodeStyleSetHeightPercent(&self->yoga_node, pct);
				break;
			case LayoutDim::MinWidth:
				YGNodeStyleSetMinWidthPercent(&self->yoga_node, pct);
				break;
			case LayoutDim::MaxWidth:
				YGNodeStyleSetMaxWidthPercent(&self->yoga_node, pct);
				break;
			case LayoutDim::MinHeight:
				YGNodeStyleSetMinHeightPercent(&self->yoga_node, pct);
				break;
			case LayoutDim::MaxHeight:
				YGNodeStyleSetMaxHeightPercent(&self->yoga_node, pct);
				break;
			}
			JS_FreeCString(ctx, s);
			return JS_UNDEFINED;
		}
	}

	JS_FreeCString(ctx, s);
	return JS_ThrowTypeError(
		ctx,
		"Invalid dimension value; expected a number, \"auto\", "
		"\"<number>%%\", or undefined"
	);
}

// -- enum properties (dispatched via magic) --
enum class LayoutProp {
	Direction,
	FlexDirection,
	JustifyContent,
	AlignItems,
	AlignSelf,
	AlignContent,
	FlexWrap,
	Overflow,
	Display,
	PositionType,
	Flex,
	FlexGrow,
	FlexShrink,
	ContentAlignH,
	ContentAlignV,
};

// -- enum string conversion helpers --
template<typename E, size_t N>
[[nodiscard]] constexpr const char *enumToString(
	E value, const char *defaultStr,
	const std::pair<const char *, E> (&table)[N]
) noexcept {
	for (size_t i = 0; i < N; ++i) {
		if (table[i].second == value) {
			return table[i].first;
		}
	}
	return defaultStr;
}

template<typename E, size_t N>
[[nodiscard]] E stringToEnum(
	std::string_view s, E defaultVal,
	const std::pair<const char *, E> (&table)[N]
) noexcept {
	for (size_t i = 0; i < N; ++i) {
		if (s == table[i].first) {
			return table[i].second;
		}
	}
	return defaultVal;
}

static constexpr std::pair<const char *, YGDirection> DIRECTION_TABLE[] = {
	{"inherit", YGDirectionInherit},
	{"ltr", YGDirectionLTR},
	{"rtl", YGDirectionRTL},
};

[[nodiscard]] constexpr const char *directionToString(YGDirection v) noexcept {
	return enumToString(v, "inherit", DIRECTION_TABLE);
}

[[nodiscard]] YGDirection stringToDirection(std::string_view s) noexcept {
	return stringToEnum(s, YGDirectionInherit, DIRECTION_TABLE);
}

static constexpr std::pair<const char *, YGFlexDirection>
	FLEX_DIRECTION_TABLE[] = {
		{"column", YGFlexDirectionColumn},
		{"columnReverse", YGFlexDirectionColumnReverse},
		{"row", YGFlexDirectionRow},
		{"rowReverse", YGFlexDirectionRowReverse},
};

[[nodiscard]] constexpr const char *flexDirectionToString(
	YGFlexDirection v
) noexcept {
	return enumToString(v, "column", FLEX_DIRECTION_TABLE);
}

[[nodiscard]] YGFlexDirection stringToFlexDirection(
	std::string_view s
) noexcept {
	return stringToEnum(s, YGFlexDirectionColumn, FLEX_DIRECTION_TABLE);
}

static constexpr std::pair<const char *, YGJustify> JUSTIFY_TABLE[] = {
	{"flexStart", YGJustifyFlexStart},
	{"center", YGJustifyCenter},
	{"flexEnd", YGJustifyFlexEnd},
	{"spaceBetween", YGJustifySpaceBetween},
	{"spaceAround", YGJustifySpaceAround},
	{"spaceEvenly", YGJustifySpaceEvenly},
};

[[nodiscard]] constexpr const char *justifyToString(YGJustify v) noexcept {
	return enumToString(v, "flexStart", JUSTIFY_TABLE);
}

[[nodiscard]] YGJustify stringToJustify(std::string_view s) noexcept {
	return stringToEnum(s, YGJustifyFlexStart, JUSTIFY_TABLE);
}

static constexpr std::pair<const char *, YGAlign> ALIGN_TABLE[] = {
	{"auto", YGAlignAuto},
	{"flexStart", YGAlignFlexStart},
	{"center", YGAlignCenter},
	{"flexEnd", YGAlignFlexEnd},
	{"stretch", YGAlignStretch},
	{"baseline", YGAlignBaseline},
	{"spaceBetween", YGAlignSpaceBetween},
	{"spaceAround", YGAlignSpaceAround},
	{"spaceEvenly", YGAlignSpaceEvenly},
};

[[nodiscard]] constexpr const char *alignToString(YGAlign v) noexcept {
	return enumToString(v, "auto", ALIGN_TABLE);
}

[[nodiscard]] YGAlign stringToAlign(std::string_view s) noexcept {
	return stringToEnum(s, YGAlignAuto, ALIGN_TABLE);
}

static constexpr std::pair<const char *, YGWrap> WRAP_TABLE[] = {
	{"noWrap", YGWrapNoWrap},
	{"wrap", YGWrapWrap},
	{"wrapReverse", YGWrapWrapReverse},
};

[[nodiscard]] constexpr const char *wrapToString(YGWrap v) noexcept {
	return enumToString(v, "noWrap", WRAP_TABLE);
}

[[nodiscard]] YGWrap stringToWrap(std::string_view s) noexcept {
	return stringToEnum(s, YGWrapNoWrap, WRAP_TABLE);
}

static constexpr std::pair<const char *, YGOverflow> OVERFLOW_TABLE[] = {
	{"visible", YGOverflowVisible},
	{"hidden", YGOverflowHidden},
	{"scroll", YGOverflowScroll},
};

[[nodiscard]] constexpr const char *overflowToString(YGOverflow v) noexcept {
	return enumToString(v, "visible", OVERFLOW_TABLE);
}

[[nodiscard]] YGOverflow stringToOverflow(std::string_view s) noexcept {
	return stringToEnum(s, YGOverflowVisible, OVERFLOW_TABLE);
}

static constexpr std::pair<const char *, YGDisplay> DISPLAY_TABLE[] = {
	{"flex", YGDisplayFlex},
	{"none", YGDisplayNone},
	{"contents", YGDisplayContents},
};

[[nodiscard]] constexpr const char *displayToString(YGDisplay v) noexcept {
	return enumToString(v, "flex", DISPLAY_TABLE);
}

[[nodiscard]] YGDisplay stringToDisplay(std::string_view s) noexcept {
	return stringToEnum(s, YGDisplayFlex, DISPLAY_TABLE);
}

static constexpr std::pair<const char *, YGPositionType>
	POSITION_TYPE_TABLE[] = {
		{"static", YGPositionTypeStatic},
		{"relative", YGPositionTypeRelative},
		{"absolute", YGPositionTypeAbsolute},
};

[[nodiscard]] constexpr const char *positionTypeToString(
	YGPositionType v
) noexcept {
	return enumToString(v, "static", POSITION_TYPE_TABLE);
}

[[nodiscard]] YGPositionType stringToPositionType(std::string_view s) noexcept {
	return stringToEnum(s, YGPositionTypeStatic, POSITION_TYPE_TABLE);
}

static constexpr std::pair<const char *, ContentAlignH>
	CONTENT_ALIGN_H_TABLE[] = {
		{"left", ContentAlignH::Left},
		{"center", ContentAlignH::Center},
		{"right", ContentAlignH::Right},
};

[[nodiscard]] constexpr const char *contentAlignHToString(
	ContentAlignH v
) noexcept {
	return enumToString(v, "left", CONTENT_ALIGN_H_TABLE);
}

[[nodiscard]] ContentAlignH stringToContentAlignH(std::string_view s) noexcept {
	return stringToEnum(s, ContentAlignH::Left, CONTENT_ALIGN_H_TABLE);
}

static constexpr std::pair<const char *, ContentAlignV>
	CONTENT_ALIGN_V_TABLE[] = {
		{"top", ContentAlignV::Top},
		{"horizon", ContentAlignV::Horizon},
		{"bottom", ContentAlignV::Bottom},
};

[[nodiscard]] constexpr const char *contentAlignVToString(
	ContentAlignV v
) noexcept {
	return enumToString(v, "top", CONTENT_ALIGN_V_TABLE);
}

[[nodiscard]] ContentAlignV stringToContentAlignV(std::string_view s) noexcept {
	return stringToEnum(s, ContentAlignV::Top, CONTENT_ALIGN_V_TABLE);
}

JSValue LayoutNode_getEnumProp(
	JSContext *ctx, JSValueConst this_val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	switch (static_cast<LayoutProp>(magic)) {
	case LayoutProp::Direction:
		return JS_NewString(
			ctx, directionToString(YGNodeStyleGetDirection(&self->yoga_node))
		);
	case LayoutProp::FlexDirection:
		return JS_NewString(
			ctx,
			flexDirectionToString(YGNodeStyleGetFlexDirection(&self->yoga_node))
		);
	case LayoutProp::JustifyContent:
		return JS_NewString(
			ctx, justifyToString(YGNodeStyleGetJustifyContent(&self->yoga_node))
		);
	case LayoutProp::AlignItems:
		return JS_NewString(
			ctx, alignToString(YGNodeStyleGetAlignItems(&self->yoga_node))
		);
	case LayoutProp::AlignSelf:
		return JS_NewString(
			ctx, alignToString(YGNodeStyleGetAlignSelf(&self->yoga_node))
		);
	case LayoutProp::AlignContent:
		return JS_NewString(
			ctx, alignToString(YGNodeStyleGetAlignContent(&self->yoga_node))
		);
	case LayoutProp::FlexWrap:
		return JS_NewString(
			ctx, wrapToString(YGNodeStyleGetFlexWrap(&self->yoga_node))
		);
	case LayoutProp::Overflow:
		return JS_NewString(
			ctx, overflowToString(YGNodeStyleGetOverflow(&self->yoga_node))
		);
	case LayoutProp::Display:
		return JS_NewString(
			ctx, displayToString(YGNodeStyleGetDisplay(&self->yoga_node))
		);
	case LayoutProp::PositionType:
		return JS_NewString(
			ctx,
			positionTypeToString(YGNodeStyleGetPositionType(&self->yoga_node))
		);
	case LayoutProp::Flex:
		return JS_NewFloat64(ctx, YGNodeStyleGetFlex(&self->yoga_node));
	case LayoutProp::FlexGrow:
		return JS_NewFloat64(ctx, YGNodeStyleGetFlexGrow(&self->yoga_node));
	case LayoutProp::FlexShrink:
		return JS_NewFloat64(ctx, YGNodeStyleGetFlexShrink(&self->yoga_node));
	case LayoutProp::ContentAlignH:
		return JS_NewString(ctx, contentAlignHToString(self->content_align.h));
	case LayoutProp::ContentAlignV:
		return JS_NewString(ctx, contentAlignVToString(self->content_align.v));
	}
	return JS_UNDEFINED;
}

JSValue LayoutNode_setEnumPropStr(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *s = JS_ToCStringLen(ctx, &len, val);
	if (!s) {
		return JS_ThrowTypeError(ctx, "Expected a string");
	}
	std::string_view sv(s, len);
	switch (static_cast<LayoutProp>(magic)) {
	case LayoutProp::Direction:
		YGNodeStyleSetDirection(&self->yoga_node, stringToDirection(sv));
		break;
	case LayoutProp::FlexDirection:
		YGNodeStyleSetFlexDirection(
			&self->yoga_node, stringToFlexDirection(sv)
		);
		break;
	case LayoutProp::JustifyContent:
		YGNodeStyleSetJustifyContent(&self->yoga_node, stringToJustify(sv));
		break;
	case LayoutProp::AlignItems:
		YGNodeStyleSetAlignItems(&self->yoga_node, stringToAlign(sv));
		break;
	case LayoutProp::AlignSelf:
		YGNodeStyleSetAlignSelf(&self->yoga_node, stringToAlign(sv));
		break;
	case LayoutProp::AlignContent:
		YGNodeStyleSetAlignContent(&self->yoga_node, stringToAlign(sv));
		break;
	case LayoutProp::FlexWrap:
		YGNodeStyleSetFlexWrap(&self->yoga_node, stringToWrap(sv));
		break;
	case LayoutProp::Overflow:
		YGNodeStyleSetOverflow(&self->yoga_node, stringToOverflow(sv));
		break;
	case LayoutProp::Display:
		YGNodeStyleSetDisplay(&self->yoga_node, stringToDisplay(sv));
		break;
	case LayoutProp::PositionType:
		YGNodeStyleSetPositionType(&self->yoga_node, stringToPositionType(sv));
		break;
	case LayoutProp::ContentAlignH:
		self->content_align.h = stringToContentAlignH(sv);
		break;
	case LayoutProp::ContentAlignV:
		self->content_align.v = stringToContentAlignV(sv);
		break;
	default:
		break;
	}
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

JSValue LayoutNode_setEnumPropFloat(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	double v;
	JS_ToFloat64(ctx, &v, val);
	switch (static_cast<LayoutProp>(magic)) {
	case LayoutProp::Flex:
		YGNodeStyleSetFlex(&self->yoga_node, static_cast<float>(v));
		break;
	case LayoutProp::FlexGrow:
		YGNodeStyleSetFlexGrow(&self->yoga_node, static_cast<float>(v));
		break;
	case LayoutProp::FlexShrink:
		YGNodeStyleSetFlexShrink(&self->yoga_node, static_cast<float>(v));
		break;
	default:
		break;
	}
	return JS_UNDEFINED;
}

// -- content --
JSValue LayoutNode_getContent(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	if (JS_IsNull(self->content_val)) {
		return JS_NULL;
	}
	return JS_DupValue(ctx, self->content_val);
}

JSValue LayoutNode_setContent(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	JS_FreeValue(ctx, self->content_val);

	bool canMeasure = true;

	if (auto *tc = TextContent::unwrap(ctx, val)) {
		self->content = tc;
	} else if (auto *sc = SpriteContent::unwrap(ctx, val)) {
		self->content = sc;
	} else {
		self->content.reset();
		canMeasure = false;
	}

	if (self->content.has_value()) {
		self->content_val = JS_DupValue(ctx, val);
	} else {
		self->content_val = JS_NULL;
	}

	if (canMeasure) {
		YGNodeSetMeasureFunc(&self->yoga_node, yogaMeasureFunc);
		YGNodeMarkDirty(&self->yoga_node);
	} else {
		YGNodeSetMeasureFunc(&self->yoga_node, nullptr);
	}

	return JS_UNDEFINED;
}

// -- background color --
JSValue LayoutNode_getBgColor(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->background_color);
}

JSValue LayoutNode_setBgColor(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto color_object = Color::interpretAsValue(ctx, val);
	if (!color_object) {
		return JS_ThrowTypeError(ctx, "%s", color_object.error());
	}

	JS_FreeValue(ctx, self->background_color);
	self->background_color = *color_object;
	return JS_UNDEFINED;
}

// -- border color --
JSValue LayoutNode_getBorderColor(
	JSContext *ctx, JSValueConst this_val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	auto edge = static_cast<YGEdge>(magic);
	return JS_DupValue(ctx, self->border_color[edge]);
}

JSValue LayoutNode_setBorderColor(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto color_object = Color::interpretAsValue(ctx, val);
	if (!color_object) {
		return JS_ThrowTypeError(ctx, "%s", color_object.error());
	}

	auto edge = static_cast<YGEdge>(magic);
	JS_FreeValue(ctx, self->border_color[edge]);
	self->border_color[edge] = *color_object;
	return JS_UNDEFINED;
}

// -- edge properties (margin, padding, position, border) --
// -- YGValue properties (margin, padding, position, border, gap) --
// Each entry in the table interprets the low nibble of magic independently.

enum class YgValueProp : uint8_t {
	Margin,
	Padding,
	Position,
	Border,
	Gap,
};

constexpr int16_t toMagic(YgValueProp type, int value) noexcept {
	return static_cast<int16_t>(
		(static_cast<uint8_t>(type) << 4) | (value & 0xF)
	);
}

struct YgValueFuncs {
	YGValue (*get)(YGNodeConstRef, int16_t magic);
	void (*set)(YGNodeRef, int16_t magic, float);
	void (*set_percent)(YGNodeRef, int16_t magic, float);
	void (*set_auto)(YGNodeRef, int16_t magic);
};

// Border getter returns float directly; wrap as YGValue.
static YGValue getBorder(YGNodeConstRef node, int16_t magic) noexcept {
	auto edge = static_cast<YGEdge>(magic & 0xF);
	return {YGNodeStyleGetBorder(node, edge), YGUnitPoint};
}

static YGValue getGap(YGNodeConstRef node, int16_t magic) noexcept {
	return YGNodeStyleGetGap(node, static_cast<YGGutter>(magic & 0xF));
}
static void setGap(YGNodeRef node, int16_t magic, float value) noexcept {
	YGNodeStyleSetGap(node, static_cast<YGGutter>(magic & 0xF), value);
}
static void setGapPercent(YGNodeRef node, int16_t magic, float value) noexcept {
	YGNodeStyleSetGapPercent(node, static_cast<YGGutter>(magic & 0xF), value);
}

// Template wrappers for YGEdge-based Yoga API functions.
template<auto Fn>
[[nodiscard]] YGValue callGet(YGNodeConstRef node, int16_t magic) noexcept {
	return Fn(node, static_cast<YGEdge>(magic & 0xF));
}
template<auto Fn>
void callSet(YGNodeRef node, int16_t magic, float value) noexcept {
	Fn(node, static_cast<YGEdge>(magic & 0xF), value);
}
template<auto Fn>
void callSetAuto(YGNodeRef node, int16_t magic) noexcept {
	Fn(node, static_cast<YGEdge>(magic & 0xF));
}

/* clang-format off */
static constexpr YgValueFuncs YG_VALUE_FUNCS[] = {
	{
		.get = callGet<YGNodeStyleGetMargin>,
		.set = callSet<YGNodeStyleSetMargin>,
		.set_percent = callSet<YGNodeStyleSetMarginPercent>,
		.set_auto = callSetAuto<YGNodeStyleSetMarginAuto>
	},
	{
		.get = callGet<YGNodeStyleGetPadding>,
		.set = callSet<YGNodeStyleSetPadding>,
		.set_percent = callSet<YGNodeStyleSetPaddingPercent>,
		.set_auto = nullptr
	},
	{
		.get = callGet<YGNodeStyleGetPosition>,
		.set = callSet<YGNodeStyleSetPosition>,
		.set_percent = callSet<YGNodeStyleSetPositionPercent>,
		.set_auto = callSetAuto<YGNodeStyleSetPositionAuto>
	},
	{
		.get = getBorder,
		.set = callSet<YGNodeStyleSetBorder>,
		.set_percent = nullptr,
		.set_auto = nullptr
	},
	{
		.get = getGap,
		.set = setGap,
		.set_percent = setGapPercent,
		.set_auto = nullptr
	},
};
/* clang-format on */

JSValue LayoutNode_getYgValue(
	JSContext *ctx, JSValueConst this_val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto type = static_cast<YgValueProp>(
		(static_cast<uint8_t>(magic) >> 4) & 0xF
	);
	YGValue v = YG_VALUE_FUNCS[static_cast<uint8_t>(type)].get(
		&self->yoga_node, static_cast<int16_t>(magic & 0xF)
	);

	return ygValueToJS(ctx, v);
}

JSValue LayoutNode_setYgValue(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto type = static_cast<YgValueProp>(
		(static_cast<uint8_t>(magic) >> 4) & 0xF
	);
	const auto &funcs = YG_VALUE_FUNCS[static_cast<uint8_t>(type)];
	auto subMagic = static_cast<int16_t>(magic & 0xF);

	// undefined / null → unset
	if (JS_IsUndefined(val) || JS_IsNull(val)) {
		funcs.set(&self->yoga_node, subMagic, YGUndefined);
		return JS_UNDEFINED;
	}

	// number → point
	double v;
	if (JS_ToFloat64(ctx, &v, val) == 0) {
		funcs.set(&self->yoga_node, subMagic, static_cast<float>(v));
		return JS_UNDEFINED;
	}

	// string
	size_t len;
	const char *s = JS_ToCStringLen(ctx, &len, val);
	if (!s) {
		return JS_UNDEFINED;
	}
	std::string_view sv(s, len);

	// "auto"
	if (sv == "auto") {
		if (funcs.set_auto) {
			funcs.set_auto(&self->yoga_node, subMagic);
			JS_FreeCString(ctx, s);
			return JS_UNDEFINED;
		}
		JS_FreeCString(ctx, s);
		return JS_ThrowTypeError(
			ctx, "\"auto\" is not valid for this property"
		);
	}

	// "<n>%" → percent
	if (!sv.empty() && sv.back() == '%') {
		auto numPart = sv.substr(0, sv.size() - 1);
		float pct;
		auto result = std::from_chars(
			numPart.data(), numPart.data() + numPart.size(), pct
		);
		if (result.ec == std::errc()
		    && result.ptr == numPart.data() + numPart.size()) {
			if (funcs.set_percent) {
				funcs.set_percent(&self->yoga_node, subMagic, pct);
				JS_FreeCString(ctx, s);
				return JS_UNDEFINED;
			}
			JS_FreeCString(ctx, s);
			return JS_ThrowTypeError(
				ctx, "Percent is not valid for this property"
			);
		}
	}

	JS_FreeCString(ctx, s);
	return JS_ThrowTypeError(
		ctx,
		"Invalid edge property value; expected a number, \"auto\", "
		"\"<number>%%\", or undefined"
	);
}

// -- tree getters --
JSValue LayoutNode_getChildCount(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	int32_t count = 0;
	for (auto *child = self->first_child; child; child = child->next_sibling) {
		count++;
	}
	return JS_NewInt32(ctx, count);
}

JSValue LayoutNode_getFirstChild(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self || !self->first_child) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->first_child->self_val);
}

JSValue LayoutNode_getLastChild(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self || !self->last_child) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->last_child->self_val);
}

// -- tree methods --
WF_JS_METHOD(LayoutNode, appendChild, {
	auto *child = LayoutNode::unwrap(ctx, argv[0]);
	if (!child) {
		return JS_ThrowTypeError(ctx, "Expected a LayoutNode");
	}
	self->appendChild(ctx, child);
	return JS_UNDEFINED;
})

WF_JS_METHOD(LayoutNode, removeChild, {
	auto *child = LayoutNode::unwrap(ctx, argv[0]);
	if (!child) {
		return JS_ThrowTypeError(ctx, "Expected a LayoutNode");
	}
	self->removeChild(ctx, child);
	return JS_UNDEFINED;
})

WF_JS_METHOD(LayoutNode, insertBefore, {
	auto *newChild = LayoutNode::unwrap(ctx, argv[0]);
	if (!newChild) {
		return JS_ThrowTypeError(ctx, "Expected a LayoutNode");
	}
	LayoutNode *referenceChild = nullptr;
	if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
		referenceChild = LayoutNode::unwrap(ctx, argv[1]);
		if (!referenceChild) {
			return JS_ThrowTypeError(
				ctx, "Expected a LayoutNode as second argument"
			);
		}
	}
	self->insertBefore(ctx, newChild, referenceChild);
	return JS_UNDEFINED;
})

WF_JS_METHOD(LayoutNode, replaceChild, {
	auto *newChild = LayoutNode::unwrap(ctx, argv[0]);
	if (!newChild) {
		return JS_ThrowTypeError(ctx, "Expected a LayoutNode");
	}
	auto *oldChild = LayoutNode::unwrap(ctx, argv[1]);
	if (!oldChild) {
		return JS_ThrowTypeError(ctx, "Expected a LayoutNode");
	}
	self->replaceChild(ctx, newChild, oldChild);
	return JS_UNDEFINED;
})

WF_JS_METHOD(LayoutNode, hasChildNodes, {
	return JS_NewBool(ctx, self->first_child != nullptr);
})

WF_JS_METHOD(LayoutNode, getRootNode, {
	auto *root = self;
	while (root->parent) {
		root = root->parent;
	}
	return JS_DupValue(ctx, root->self_val);
})

WF_JS_METHOD(LayoutNode, relayout, {
	self->relayout();
	return JS_UNDEFINED;
})

WF_JS_METHOD(LayoutNode, childItem, {
	int32_t index;
	if (JS_ToInt32(ctx, &index, argv[0]) != 0) {
		return JS_ThrowTypeError(ctx, "Expected a number");
	}
	if (index < 0) {
		return JS_NULL;
	}
	auto *child = self->first_child;
	for (int32_t i = 0; child && i < index; i++) {
		child = child->next_sibling;
	}
	if (!child) {
		return JS_NULL;
	}
	return JS_DupValue(ctx, child->self_val);
})

JSValue LayoutNode_getParent(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self || !self->parent) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->parent->self_val);
}

JSValue LayoutNode_getComputedBounds(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	(void)argc;
	(void)argv;
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	auto x = static_cast<int>(
		std::round(YGNodeLayoutGetLeft(&self->yoga_node))
	);
	auto y = static_cast<int>(std::round(YGNodeLayoutGetTop(&self->yoga_node)));
	auto w = static_cast<int>(
		std::round(YGNodeLayoutGetWidth(&self->yoga_node))
	);
	auto h = static_cast<int>(
		std::round(YGNodeLayoutGetHeight(&self->yoga_node))
	);
	if (w == 0 && h == 0) {
		return JS_NULL;
	}

	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, x));
	JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, y));
	JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, w));
	JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, h));
	return obj;
}

namespace {

LayoutNode *hitTestImpl(
	LayoutNode *node, int x, int y, int off_x, int off_y
) noexcept {
	// Check children in reverse order (last child rendered = topmost)
	auto count = YGNodeGetChildCount(&node->yoga_node);
	for (size_t i = count; i > 0; i--) {
		auto *child = static_cast<LayoutNode *>(
			YGNodeGetContext(YGNodeGetChild(&node->yoga_node, i - 1))
		);
		auto cx = off_x
			+ static_cast<int>(
					  std::round(YGNodeLayoutGetLeft(&child->yoga_node))
			);
		auto cy = off_y
			+ static_cast<int>(
					  std::round(YGNodeLayoutGetTop(&child->yoga_node))
			);
		auto *result = hitTestImpl(child, x, y, cx, cy);
		if (result) {
			return result;
		}
	}

	// Check self
	auto nw = static_cast<int>(
		std::round(YGNodeLayoutGetWidth(&node->yoga_node))
	);
	auto nh = static_cast<int>(
		std::round(YGNodeLayoutGetHeight(&node->yoga_node))
	);
	if (x >= off_x && x < off_x + nw && y >= off_y && y < off_y + nh) {
		return node;
	}
	return nullptr;
}

} // namespace

JSValue LayoutNode_hitTest(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	if (argc < 2) {
		return JS_ThrowTypeError(ctx, "Expected two arguments");
	}
	int32_t x;
	int32_t y;
	if (JS_ToInt32(ctx, &x, argv[0]) != 0
	    || JS_ToInt32(ctx, &y, argv[1]) != 0) {
		return JS_ThrowTypeError(ctx, "Expected two numbers");
	}
	auto *result = hitTestImpl(self, x, y, 0, 0);
	if (!result) {
		return JS_NULL;
	}
	return JS_DupValue(ctx, result->self_val);
}

} // namespace

// ===== Proto table =====

static const JSCFunctionListEntry LAYOUT_NODE_PROTO[] = {
	cGetSetMagicDef(
		"width", LayoutNode_getDimProp, LayoutNode_setDimProp,
		static_cast<int16_t>(LayoutDim::Width)
	),
	cGetSetMagicDef(
		"height", LayoutNode_getDimProp, LayoutNode_setDimProp,
		static_cast<int16_t>(LayoutDim::Height)
	),
	cGetSetMagicDef(
		"minWidth", LayoutNode_getDimProp, LayoutNode_setDimProp,
		static_cast<int16_t>(LayoutDim::MinWidth)
	),
	cGetSetMagicDef(
		"maxWidth", LayoutNode_getDimProp, LayoutNode_setDimProp,
		static_cast<int16_t>(LayoutDim::MaxWidth)
	),
	cGetSetMagicDef(
		"minHeight", LayoutNode_getDimProp, LayoutNode_setDimProp,
		static_cast<int16_t>(LayoutDim::MinHeight)
	),
	cGetSetMagicDef(
		"maxHeight", LayoutNode_getDimProp, LayoutNode_setDimProp,
		static_cast<int16_t>(LayoutDim::MaxHeight)
	),

	cGetSetMagicDef(
		"direction", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::Direction)
	),
	cGetSetMagicDef(
		"flexDirection", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::FlexDirection)
	),
	cGetSetMagicDef(
		"justifyContent", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::JustifyContent)
	),
	cGetSetMagicDef(
		"alignItems", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::AlignItems)
	),
	cGetSetMagicDef(
		"alignSelf", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::AlignSelf)
	),
	cGetSetMagicDef(
		"alignContent", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::AlignContent)
	),
	cGetSetMagicDef(
		"flexWrap", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::FlexWrap)
	),
	cGetSetMagicDef(
		"overflow", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::Overflow)
	),
	cGetSetMagicDef(
		"display", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::Display)
	),
	cGetSetMagicDef(
		"positionType", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::PositionType)
	),
	cGetSetMagicDef(
		"flex", LayoutNode_getEnumProp, LayoutNode_setEnumPropFloat,
		static_cast<int16_t>(LayoutProp::Flex)
	),
	cGetSetMagicDef(
		"flexGrow", LayoutNode_getEnumProp, LayoutNode_setEnumPropFloat,
		static_cast<int16_t>(LayoutProp::FlexGrow)
	),
	cGetSetMagicDef(
		"flexShrink", LayoutNode_getEnumProp, LayoutNode_setEnumPropFloat,
		static_cast<int16_t>(LayoutProp::FlexShrink)
	),

	cGetSetDef("content", LayoutNode_getContent, LayoutNode_setContent),

	cGetSetDef("backgroundColor", LayoutNode_getBgColor, LayoutNode_setBgColor),

	cGetSetMagicDef(
		"contentAlignH", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::ContentAlignH)
	),
	cGetSetMagicDef(
		"contentAlignV", LayoutNode_getEnumProp, LayoutNode_setEnumPropStr,
		static_cast<int16_t>(LayoutProp::ContentAlignV)
	),

	cGetSetMagicDef(
		"marginLeft", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Margin, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"marginRight", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Margin, YGEdgeRight)
	),
	cGetSetMagicDef(
		"marginTop", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Margin, YGEdgeTop)
	),
	cGetSetMagicDef(
		"marginBottom", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Margin, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"paddingLeft", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Padding, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"paddingRight", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Padding, YGEdgeRight)
	),
	cGetSetMagicDef(
		"paddingTop", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Padding, YGEdgeTop)
	),
	cGetSetMagicDef(
		"paddingBottom", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Padding, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"left", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Position, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"right", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Position, YGEdgeRight)
	),
	cGetSetMagicDef(
		"top", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Position, YGEdgeTop)
	),
	cGetSetMagicDef(
		"bottom", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Position, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"borderLeft", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Border, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"borderRight", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Border, YGEdgeRight)
	),
	cGetSetMagicDef(
		"borderTop", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Border, YGEdgeTop)
	),
	cGetSetMagicDef(
		"borderBottom", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Border, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"borderLeftColor", LayoutNode_getBorderColor, LayoutNode_setBorderColor,
		YGEdgeLeft
	),
	cGetSetMagicDef(
		"borderRightColor", LayoutNode_getBorderColor,
		LayoutNode_setBorderColor, YGEdgeRight
	),
	cGetSetMagicDef(
		"borderTopColor", LayoutNode_getBorderColor, LayoutNode_setBorderColor,
		YGEdgeTop
	),
	cGetSetMagicDef(
		"borderBottomColor", LayoutNode_getBorderColor,
		LayoutNode_setBorderColor, YGEdgeBottom
	),

	cGetSetMagicDef(
		"gap", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Gap, YGGutterAll)
	),
	cGetSetMagicDef(
		"rowGap", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Gap, YGGutterRow)
	),
	cGetSetMagicDef(
		"columnGap", LayoutNode_getYgValue, LayoutNode_setYgValue,
		toMagic(YgValueProp::Gap, YGGutterColumn)
	),

	// tree getters (read-only, no setter)
	cGetSetDef("childCount", LayoutNode_getChildCount, nullptr),
	cGetSetDef("firstChild", LayoutNode_getFirstChild, nullptr),
	cGetSetDef("lastChild", LayoutNode_getLastChild, nullptr),
	cGetSetDef("parent", LayoutNode_getParent, nullptr),

	// tree methods
	cFuncDef("appendChild", 1, LayoutNode_appendChild),
	cFuncDef("removeChild", 1, LayoutNode_removeChild),
	cFuncDef("insertBefore", 2, LayoutNode_insertBefore),
	cFuncDef("replaceChild", 2, LayoutNode_replaceChild),
	cFuncDef("hasChildNodes", 0, LayoutNode_hasChildNodes),
	cFuncDef("getRootNode", 0, LayoutNode_getRootNode),
	cFuncDef("relayout", 0, LayoutNode_relayout),
	cFuncDef("childItem", 1, LayoutNode_childItem),
	cFuncDef("getComputedBounds", 0, LayoutNode_getComputedBounds),
	cFuncDef("hitTest", 2, LayoutNode_hitTest),

	propStringDef("[Symbol.toStringTag]", "LayoutNode", JS_PROP_CONFIGURABLE),
};

const CFunctionList LayoutNode::PROTO_FIELDS{LAYOUT_NODE_PROTO};

JSValue LayoutNode::ctor(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv
) noexcept {
	auto self = std::make_unique<LayoutNode>();
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
		return obj;
	}
	JS_SetOpaque(obj, self.get());
	self->self_val = obj;
	self.release();
	return obj;
}

void LayoutNode::gcMark(
	JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func
) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_MarkValue(rt, self->content_val, mark_func);
	JS_MarkValue(rt, self->background_color, mark_func);
	for (auto &c : self->border_color) {
		JS_MarkValue(rt, c, mark_func);
	}
	for (auto *child = self->first_child; child; child = child->next_sibling) {
		JS_MarkValue(rt, child->self_val, mark_func);
	}
}

void LayoutNode::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_FreeValueRT(rt, self->content_val);
	JS_FreeValueRT(rt, self->background_color);
	for (auto &c : self->border_color) {
		JS_FreeValueRT(rt, c);
	}
	for (auto *child = self->first_child; child; child = child->next_sibling) {
		JS_FreeValueRT(rt, child->self_val);
		child->parent = nullptr;
	}
	// self_val is stored without ref count.
	delete self;
}

} // namespace wf::js
