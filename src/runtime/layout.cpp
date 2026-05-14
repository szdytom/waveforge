#include "wforge/layout.h"
#include "hacks.h"
#include "helper.h"
#include <charconv>
#include <sstream>
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

} // namespace

// ===== Content::measure/render implementations =====

int TextContent::charWidth() const noexcept {
	return AssetsManager::instance().getAsset<PixelFont>("font").charWidth(
		size
	);
}

int TextContent::charHeight() const noexcept {
	return AssetsManager::instance().getAsset<PixelFont>("font").charHeight(
		size
	);
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
		int maxPixels = static_cast<int>(width);
		int maxPerLine = std::max(1, maxPixels / charWidth());
		lineCount = 0;
		int curLen = 0;
		float maxLineW = 0;

		std::istringstream stream(text);
		std::string word;
		while (stream >> word) {
			int wordLen = static_cast<int>(word.length());
			if (curLen == 0) {
				curLen = wordLen;
			} else if ((curLen + 1 + wordLen) <= maxPerLine) {
				curLen += 1 + wordLen;
			} else {
				lineCount++;
				maxLineW = std::max(
					maxLineW, static_cast<float>(curLen * charWidth())
				);
				curLen = wordLen;
			}
		}
		if (curLen > 0) {
			lineCount++;
			maxLineW = std::max(
				maxLineW, static_cast<float>(curLen * charWidth())
			);
		}
		measuredW = std::min(width, maxLineW);
	}

	float measuredH = static_cast<float>(lineCount * charHeight());

	return {
		resolveDim(measuredW, width, width_mode),
		resolveDim(measuredH, height, height_mode),
	};
}

void TextContent::render(
	sf::RenderTarget &target, JSContext *ctx, int scale, float x, float y,
	float w, float h
) const {
	(void)w;
	(void)h;
	auto &font = AssetsManager::instance().getAsset<PixelFont>("font");
	font.renderText(
		target, text, nativeColor(ctx), static_cast<int>(x),
		static_cast<int>(y), scale, size
	);
}

sf::Color TextContent::nativeColor(JSContext *ctx) const noexcept {
	return Color::fromValue(ctx, color).value_or(sf::Color::Black);
}

YGSize SpriteContent::measure(
	YGMeasureMode width_mode, float width, YGMeasureMode height_mode,
	float height
) const noexcept {
	float texW = static_cast<float>(texture->getSize().x) * size;
	float texH = static_cast<float>(texture->getSize().y) * size;
	return {
		resolveDim(texW, width, width_mode),
		resolveDim(texH, height, height_mode),
	};
}

void SpriteContent::render(
	sf::RenderTarget &target, JSContext * /*ctx*/, int scale, float x, float y,
	float w, float h
) const {
	sf::Sprite sprite(*texture);
	sprite.setPosition(sf::Vector2f(x * scale, y * scale));
	sprite.setScale(
		sf::Vector2f(
			w * scale / static_cast<float>(texture->getSize().x),
			h * scale / static_cast<float>(texture->getSize().y)
		)
	);
	target.draw(sprite);
}

// ===== LayoutNode =====

LayoutNode::LayoutNode() {
	YGNodeSetContext(&yoga_node, this);
}

void LayoutNode::appendChild(
	JSContext *ctx, LayoutNode *child, JSValue childVal
) {
	children.push_back({JS_DupValue(ctx, childVal), child});
	YGNodeInsertChild(
		&yoga_node, &child->yoga_node, YGNodeGetChildCount(&yoga_node)
	);
}

void LayoutNode::removeChild(JSContext *ctx, LayoutNode *child) {
	for (size_t i = 0; i < children.size(); i++) {
		if (children[i].node == child) {
			JS_FreeValue(ctx, children[i].val);
			children.erase(children.begin() + static_cast<ssize_t>(i));
			break;
		}
	}
	YGNodeRemoveChild(&yoga_node, &child->yoga_node);
}

void LayoutNode::insertBefore(
	JSContext *ctx, LayoutNode *newChild, JSValue newChildVal,
	LayoutNode *referenceChild
) {
	if (referenceChild) {
		for (size_t i = 0; i < children.size(); i++) {
			if (children[i].node == referenceChild) {
				children.insert(
					children.begin() + static_cast<ssize_t>(i),
					{JS_DupValue(ctx, newChildVal), newChild}
				);
				YGNodeInsertChild(&yoga_node, &newChild->yoga_node, i);
				return;
			}
		}
	}
	// referenceChild not found or null → append
	appendChild(ctx, newChild, newChildVal);
}

void LayoutNode::replaceChild(
	JSContext *ctx, LayoutNode *newChild, JSValue newChildVal,
	LayoutNode *oldChild
) {
	for (size_t i = 0; i < children.size(); i++) {
		if (children[i].node == oldChild) {
			JS_FreeValue(ctx, children[i].val);
			children[i] = {JS_DupValue(ctx, newChildVal), newChild};
			YGNodeRemoveChild(&yoga_node, &oldChild->yoga_node);
			YGNodeInsertChild(&yoga_node, &newChild->yoga_node, i);
			return;
		}
	}
}

void LayoutNode::calculateLayout(float availWidth, float availHeight) {
	YGNodeCalculateLayout(&yoga_node, availWidth, availHeight, YGDirectionLTR);
}

void LayoutNode::render(
	sf::RenderTarget &target, JSContext *ctx, int scale
) const {
	render(target, ctx, scale, 0, 0);
}

void LayoutNode::render(
	sf::RenderTarget &target, JSContext *ctx, int scale, float parentX,
	float parentY
) const {
	float abs_x = parentX + YGNodeLayoutGetLeft(&yoga_node);
	float abs_y = parentY + YGNodeLayoutGetTop(&yoga_node);
	float w = YGNodeLayoutGetWidth(&yoga_node);
	float h = YGNodeLayoutGetHeight(&yoga_node);

	if (auto bg_color = Color::fromValue(ctx, background_color);
	    bg_color && bg_color->a > 0) {
		sf::RectangleShape bg(sf::Vector2f(w * scale, h * scale));
		bg.setPosition(sf::Vector2f(abs_x * scale, abs_y * scale));
		bg.setFillColor(*bg_color);
		target.draw(bg);
	}

	// Draw borders
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

	if (content.has_value()) {
		content->render(target, ctx, scale, abs_x, abs_y, w, h);
	}

	for (size_t i = 0, n = YGNodeGetChildCount(&yoga_node); i < n; i++) {
		auto *child = static_cast<LayoutNode *>(YGNodeGetContext(
			YGNodeGetChild(const_cast<facebook::yoga::Node *>(&yoga_node), i)
		));
		child->render(target, ctx, scale, abs_x, abs_y);
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
	auto opt = Color::fromValue(ctx, *color_object);
	self->_nativeColor = opt.value_or(sf::Color::Black);
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
		auto opt = Color::fromValue(ctx, *cv);
		self->_nativeColor = opt.value_or(sf::Color::Black);
	}
	JSValue obj = JS_NewObjectClass(ctx, clsId(JS_GetRuntime(ctx)));
	if (JS_IsException(obj)) {
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
	}
	return JS_UNDEFINED;
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
};

// -- string conversion helpers for enum properties --
constexpr const char *directionToString(YGDirection v) noexcept {
	switch (v) {
	case YGDirectionInherit:
		return "inherit";
	case YGDirectionLTR:
		return "ltr";
	case YGDirectionRTL:
		return "rtl";
	default:
		return "inherit";
	}
}

YGDirection stringToDirection(std::string_view s) noexcept {
	if (s == "ltr") {
		return YGDirectionLTR;
	}
	if (s == "rtl") {
		return YGDirectionRTL;
	}
	return YGDirectionInherit;
}

constexpr const char *flexDirectionToString(YGFlexDirection v) noexcept {
	switch (v) {
	case YGFlexDirectionColumn:
		return "column";
	case YGFlexDirectionColumnReverse:
		return "columnReverse";
	case YGFlexDirectionRow:
		return "row";
	case YGFlexDirectionRowReverse:
		return "rowReverse";
	default:
		return "column";
	}
}

YGFlexDirection stringToFlexDirection(std::string_view s) noexcept {
	if (s == "columnReverse") {
		return YGFlexDirectionColumnReverse;
	}
	if (s == "row") {
		return YGFlexDirectionRow;
	}
	if (s == "rowReverse") {
		return YGFlexDirectionRowReverse;
	}
	return YGFlexDirectionColumn;
}

constexpr const char *justifyToString(YGJustify v) noexcept {
	switch (v) {
	case YGJustifyFlexStart:
		return "flexStart";
	case YGJustifyCenter:
		return "center";
	case YGJustifyFlexEnd:
		return "flexEnd";
	case YGJustifySpaceBetween:
		return "spaceBetween";
	case YGJustifySpaceAround:
		return "spaceAround";
	case YGJustifySpaceEvenly:
		return "spaceEvenly";
	default:
		return "flexStart";
	}
}

YGJustify stringToJustify(std::string_view s) noexcept {
	if (s == "center") {
		return YGJustifyCenter;
	}
	if (s == "flexEnd") {
		return YGJustifyFlexEnd;
	}
	if (s == "spaceBetween") {
		return YGJustifySpaceBetween;
	}
	if (s == "spaceAround") {
		return YGJustifySpaceAround;
	}
	if (s == "spaceEvenly") {
		return YGJustifySpaceEvenly;
	}
	return YGJustifyFlexStart;
}

constexpr const char *alignToString(YGAlign v) noexcept {
	switch (v) {
	case YGAlignAuto:
		return "auto";
	case YGAlignFlexStart:
		return "flexStart";
	case YGAlignCenter:
		return "center";
	case YGAlignFlexEnd:
		return "flexEnd";
	case YGAlignStretch:
		return "stretch";
	case YGAlignBaseline:
		return "baseline";
	case YGAlignSpaceBetween:
		return "spaceBetween";
	case YGAlignSpaceAround:
		return "spaceAround";
	case YGAlignSpaceEvenly:
		return "spaceEvenly";
	default:
		return "auto";
	}
}

YGAlign stringToAlign(std::string_view s) noexcept {
	if (s == "flexStart") {
		return YGAlignFlexStart;
	}
	if (s == "center") {
		return YGAlignCenter;
	}
	if (s == "flexEnd") {
		return YGAlignFlexEnd;
	}
	if (s == "stretch") {
		return YGAlignStretch;
	}
	if (s == "baseline") {
		return YGAlignBaseline;
	}
	if (s == "spaceBetween") {
		return YGAlignSpaceBetween;
	}
	if (s == "spaceAround") {
		return YGAlignSpaceAround;
	}
	if (s == "spaceEvenly") {
		return YGAlignSpaceEvenly;
	}
	return YGAlignAuto;
}

constexpr const char *wrapToString(YGWrap v) noexcept {
	switch (v) {
	case YGWrapNoWrap:
		return "noWrap";
	case YGWrapWrap:
		return "wrap";
	case YGWrapWrapReverse:
		return "wrapReverse";
	default:
		return "noWrap";
	}
}

YGWrap stringToWrap(std::string_view s) noexcept {
	if (s == "wrap") {
		return YGWrapWrap;
	}
	if (s == "wrapReverse") {
		return YGWrapWrapReverse;
	}
	return YGWrapNoWrap;
}

constexpr const char *overflowToString(YGOverflow v) noexcept {
	switch (v) {
	case YGOverflowVisible:
		return "visible";
	case YGOverflowHidden:
		return "hidden";
	case YGOverflowScroll:
		return "scroll";
	default:
		return "visible";
	}
}

YGOverflow stringToOverflow(std::string_view s) noexcept {
	if (s == "hidden") {
		return YGOverflowHidden;
	}
	if (s == "scroll") {
		return YGOverflowScroll;
	}
	return YGOverflowVisible;
}

constexpr const char *displayToString(YGDisplay v) noexcept {
	switch (v) {
	case YGDisplayFlex:
		return "flex";
	case YGDisplayNone:
		return "none";
	case YGDisplayContents:
		return "contents";
	default:
		return "flex";
	}
}

YGDisplay stringToDisplay(std::string_view s) noexcept {
	if (s == "none") {
		return YGDisplayNone;
	}
	if (s == "contents") {
		return YGDisplayContents;
	}
	return YGDisplayFlex;
}

constexpr const char *positionTypeToString(YGPositionType v) noexcept {
	switch (v) {
	case YGPositionTypeStatic:
		return "static";
	case YGPositionTypeRelative:
		return "relative";
	case YGPositionTypeAbsolute:
		return "absolute";
	default:
		return "static";
	}
}

YGPositionType stringToPositionType(std::string_view s) noexcept {
	if (s == "relative") {
		return YGPositionTypeRelative;
	}
	if (s == "absolute") {
		return YGPositionTypeAbsolute;
	}
	return YGPositionTypeStatic;
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
enum class EdgeType : uint8_t {
	Margin,
	Padding,
	Position,
	Border,
};

struct EdgeProp {
	EdgeType type : 4;
	YGEdge edge : 4;

	static constexpr EdgeProp fromMagic(int16_t magic) noexcept {
		return {
			static_cast<EdgeType>((static_cast<uint8_t>(magic) >> 4) & 0xF),
			static_cast<YGEdge>(static_cast<uint8_t>(magic) & 0xF),
		};
	}
};

constexpr int16_t toMagic(EdgeType type, YGEdge edge) noexcept {
	return static_cast<int16_t>(
		(static_cast<uint8_t>(type) << 4) | static_cast<uint8_t>(edge)
	);
}

struct EdgeFuncs {
	YGValue (*get)(YGNodeConstRef, YGEdge);
	void (*set)(YGNodeRef, YGEdge, float);
	void (*set_percent)(YGNodeRef, YGEdge, float);
	void (*set_auto)(YGNodeRef, YGEdge);
};

// Border getter returns float directly; wrap as YGValue for the unified getter.
static YGValue getBorder(YGNodeConstRef node, YGEdge edge) noexcept {
	return {YGNodeStyleGetBorder(node, edge), YGUnitPoint};
}

static constexpr EdgeFuncs EDGE_FUNCS[] = {
	{
		YGNodeStyleGetMargin,
		YGNodeStyleSetMargin,
		YGNodeStyleSetMarginPercent,
		YGNodeStyleSetMarginAuto,
	},
	{
		YGNodeStyleGetPadding,
		YGNodeStyleSetPadding,
		YGNodeStyleSetPaddingPercent,
		nullptr,
	},
	{
		YGNodeStyleGetPosition,
		YGNodeStyleSetPosition,
		YGNodeStyleSetPositionPercent,
		YGNodeStyleSetPositionAuto,
	},
	{
		getBorder,
		YGNodeStyleSetBorder,
		nullptr,
		nullptr,
	},
};

JSValue LayoutNode_getEdgeProp(
	JSContext *ctx, JSValueConst this_val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto [type, edge] = EdgeProp::fromMagic(magic);
	YGValue v = EDGE_FUNCS[static_cast<uint8_t>(type)].get(
		&self->yoga_node, edge
	);

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
	}
	return JS_UNDEFINED;
}

JSValue LayoutNode_setEdgeProp(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	auto [type, edge] = EdgeProp::fromMagic(magic);
	const auto &funcs = EDGE_FUNCS[static_cast<uint8_t>(type)];

	// undefined / null → unset
	if (JS_IsUndefined(val) || JS_IsNull(val)) {
		funcs.set(&self->yoga_node, edge, YGUndefined);
		return JS_UNDEFINED;
	}

	// number → point
	double v;
	if (JS_ToFloat64(ctx, &v, val) == 0) {
		funcs.set(&self->yoga_node, edge, static_cast<float>(v));
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
			funcs.set_auto(&self->yoga_node, edge);
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
				funcs.set_percent(&self->yoga_node, edge, pct);
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
WF_JS_DEF_GETTER_I32(
	LayoutNode, getChildCount, static_cast<int32_t>(self->children.size())
)

JSValue LayoutNode_getFirstChild(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self || self->children.empty()) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->children.front().val);
}

JSValue LayoutNode_getLastChild(
	JSContext *ctx, JSValueConst this_val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self || self->children.empty()) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->children.back().val);
}

// -- tree methods --
WF_JS_METHOD(LayoutNode, appendChild, {
	auto *child = LayoutNode::unwrap(ctx, argv[0]);
	if (!child) {
		return JS_ThrowTypeError(ctx, "Expected a LayoutNode");
	}
	self->appendChild(ctx, child, argv[0]);
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
	self->insertBefore(ctx, newChild, argv[0], referenceChild);
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
	self->replaceChild(ctx, newChild, argv[0], oldChild);
	return JS_UNDEFINED;
})

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
		"marginLeft", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Margin, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"marginRight", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Margin, YGEdgeRight)
	),
	cGetSetMagicDef(
		"marginTop", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Margin, YGEdgeTop)
	),
	cGetSetMagicDef(
		"marginBottom", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Margin, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"paddingLeft", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Padding, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"paddingRight", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Padding, YGEdgeRight)
	),
	cGetSetMagicDef(
		"paddingTop", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Padding, YGEdgeTop)
	),
	cGetSetMagicDef(
		"paddingBottom", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Padding, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"left", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Position, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"right", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Position, YGEdgeRight)
	),
	cGetSetMagicDef(
		"top", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Position, YGEdgeTop)
	),
	cGetSetMagicDef(
		"bottom", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Position, YGEdgeBottom)
	),

	cGetSetMagicDef(
		"borderLeft", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Border, YGEdgeLeft)
	),
	cGetSetMagicDef(
		"borderRight", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Border, YGEdgeRight)
	),
	cGetSetMagicDef(
		"borderTop", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Border, YGEdgeTop)
	),
	cGetSetMagicDef(
		"borderBottom", LayoutNode_getEdgeProp, LayoutNode_setEdgeProp,
		toMagic(EdgeType::Border, YGEdgeBottom)
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

	cFuncDef("appendChild", 1, LayoutNode_appendChild),
	cFuncDef("removeChild", 1, LayoutNode_removeChild),

	// tree getters (read-only, no setter)
	cGetSetDef("childCount", LayoutNode_getChildCount, nullptr),
	cGetSetDef("firstChild", LayoutNode_getFirstChild, nullptr),
	cGetSetDef("lastChild", LayoutNode_getLastChild, nullptr),

	// tree methods
	cFuncDef("insertBefore", 2, LayoutNode_insertBefore),
	cFuncDef("replaceChild", 2, LayoutNode_replaceChild),

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
	for (const auto &child : self->children) {
		JS_MarkValue(rt, child.val, mark_func);
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
	for (auto &child : self->children) {
		JS_FreeValueRT(rt, child.val);
	}
	delete self;
}

} // namespace wf::js
