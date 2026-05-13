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
	YGMeasureMode widthMode, float width, YGMeasureMode heightMode, float height
) const noexcept {
	if (text.empty()) {
		return {
			resolveDim(0, width, widthMode),
			resolveDim(static_cast<float>(charHeight()), height, heightMode),
		};
	}

	float measuredW;
	int lineCount;

	if (widthMode == YGMeasureModeUndefined) {
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
		resolveDim(measuredW, width, widthMode),
		resolveDim(measuredH, height, heightMode),
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
	YGMeasureMode widthMode, float width, YGMeasureMode heightMode, float height
) const noexcept {
	float texW = static_cast<float>(texture->getSize().x) * size;
	float texH = static_cast<float>(texture->getSize().y) * size;
	return {
		resolveDim(texW, width, widthMode),
		resolveDim(texH, height, heightMode),
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
	yogaNode.setContext(this);
}

void LayoutNode::appendChild(
	JSContext *ctx, LayoutNode *child, JSValue childVal
) {
	children.push_back({JS_DupValue(ctx, childVal), child});
	yogaNode.insertChild(&child->yogaNode, yogaNode.getChildCount());
	child->yogaNode.setOwner(&yogaNode);
}

void LayoutNode::removeChild(JSContext *ctx, LayoutNode *child) {
	for (size_t i = 0; i < children.size(); i++) {
		if (children[i].node == child) {
			JS_FreeValue(ctx, children[i].val);
			children.erase(children.begin() + static_cast<ssize_t>(i));
			break;
		}
	}
	yogaNode.removeChild(&child->yogaNode);
}

void LayoutNode::calculateLayout(float availWidth, float availHeight) {
	YGNodeCalculateLayout(&yogaNode, availWidth, availHeight, YGDirectionLTR);
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
	using namespace facebook::yoga;
	auto &layout = yogaNode.getLayout();
	float absX = parentX + layout.position(PhysicalEdge::Left);
	float absY = parentY + layout.position(PhysicalEdge::Top);
	float w = layout.dimension(Dimension::Width);
	float h = layout.dimension(Dimension::Height);

	if (auto bg_color = Color::fromValue(ctx, backgroundColor);
	    bg_color && bg_color->a > 0) {
		sf::RectangleShape bg(sf::Vector2f(w * scale, h * scale));
		bg.setPosition(sf::Vector2f(absX * scale, absY * scale));
		bg.setFillColor(*bg_color);
		target.draw(bg);
	}

	if (!JS_IsNull(contentVal)) {
		content->render(target, ctx, scale, absX, absY, w, h);
	}

	for (auto *childNode : yogaNode.getChildren()) {
		auto *child = static_cast<LayoutNode *>(childNode->getContext());
		child->render(target, ctx, scale, absX, absY);
	}
}

// ===== Yoga measure callback =====

namespace {

YGSize yogaMeasureFunc(
	YGNodeConstRef node, float width, YGMeasureMode widthMode, float height,
	YGMeasureMode heightMode
) noexcept {
	auto *yogaNode = facebook::yoga::resolveRef(node);
	auto *self = static_cast<LayoutNode *>(yogaNode->getContext());
	if (!JS_IsNull(self->contentVal)) {
		return self->content->measure(widthMode, width, heightMode, height);
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
		v = YGNodeStyleGetWidth(&self->yogaNode);
		break;
	case LayoutDim::Height:
		v = YGNodeStyleGetHeight(&self->yogaNode);
		break;
	case LayoutDim::MinWidth:
		v = YGNodeStyleGetMinWidth(&self->yogaNode);
		break;
	case LayoutDim::MaxWidth:
		v = YGNodeStyleGetMaxWidth(&self->yogaNode);
		break;
	case LayoutDim::MinHeight:
		v = YGNodeStyleGetMinHeight(&self->yogaNode);
		break;
	case LayoutDim::MaxHeight:
		v = YGNodeStyleGetMaxHeight(&self->yogaNode);
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

	// undefined / null → set undefined (NaN with Point unit)
	if (JS_IsUndefined(val) || JS_IsNull(val)) {
		switch (dim) {
		case LayoutDim::Width:
			YGNodeStyleSetWidth(&self->yogaNode, YGUndefined);
			break;
		case LayoutDim::Height:
			YGNodeStyleSetHeight(&self->yogaNode, YGUndefined);
			break;
		case LayoutDim::MinWidth:
			YGNodeStyleSetMinWidth(&self->yogaNode, YGUndefined);
			break;
		case LayoutDim::MaxWidth:
			YGNodeStyleSetMaxWidth(&self->yogaNode, YGUndefined);
			break;
		case LayoutDim::MinHeight:
			YGNodeStyleSetMinHeight(&self->yogaNode, YGUndefined);
			break;
		case LayoutDim::MaxHeight:
			YGNodeStyleSetMaxHeight(&self->yogaNode, YGUndefined);
			break;
		}
		return JS_UNDEFINED;
	}

	// number → point
	double v;
	if (JS_ToFloat64(ctx, &v, val) == 0) {
		switch (dim) {
		case LayoutDim::Width:
			YGNodeStyleSetWidth(&self->yogaNode, static_cast<float>(v));
			break;
		case LayoutDim::Height:
			YGNodeStyleSetHeight(&self->yogaNode, static_cast<float>(v));
			break;
		case LayoutDim::MinWidth:
			YGNodeStyleSetMinWidth(&self->yogaNode, static_cast<float>(v));
			break;
		case LayoutDim::MaxWidth:
			YGNodeStyleSetMaxWidth(&self->yogaNode, static_cast<float>(v));
			break;
		case LayoutDim::MinHeight:
			YGNodeStyleSetMinHeight(&self->yogaNode, static_cast<float>(v));
			break;
		case LayoutDim::MaxHeight:
			YGNodeStyleSetMaxHeight(&self->yogaNode, static_cast<float>(v));
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
			YGNodeStyleSetWidthAuto(&self->yogaNode);
			break;
		case LayoutDim::Height:
			YGNodeStyleSetHeightAuto(&self->yogaNode);
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
				YGNodeStyleSetWidthPercent(&self->yogaNode, pct);
				break;
			case LayoutDim::Height:
				YGNodeStyleSetHeightPercent(&self->yogaNode, pct);
				break;
			case LayoutDim::MinWidth:
				YGNodeStyleSetMinWidthPercent(&self->yogaNode, pct);
				break;
			case LayoutDim::MaxWidth:
				YGNodeStyleSetMaxWidthPercent(&self->yogaNode, pct);
				break;
			case LayoutDim::MinHeight:
				YGNodeStyleSetMinHeightPercent(&self->yogaNode, pct);
				break;
			case LayoutDim::MaxHeight:
				YGNodeStyleSetMaxHeightPercent(&self->yogaNode, pct);
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
			ctx, directionToString(YGNodeStyleGetDirection(&self->yogaNode))
		);
	case LayoutProp::FlexDirection:
		return JS_NewString(
			ctx,
			flexDirectionToString(YGNodeStyleGetFlexDirection(&self->yogaNode))
		);
	case LayoutProp::JustifyContent:
		return JS_NewString(
			ctx, justifyToString(YGNodeStyleGetJustifyContent(&self->yogaNode))
		);
	case LayoutProp::AlignItems:
		return JS_NewString(
			ctx, alignToString(YGNodeStyleGetAlignItems(&self->yogaNode))
		);
	case LayoutProp::AlignSelf:
		return JS_NewString(
			ctx, alignToString(YGNodeStyleGetAlignSelf(&self->yogaNode))
		);
	case LayoutProp::AlignContent:
		return JS_NewString(
			ctx, alignToString(YGNodeStyleGetAlignContent(&self->yogaNode))
		);
	case LayoutProp::FlexWrap:
		return JS_NewString(
			ctx, wrapToString(YGNodeStyleGetFlexWrap(&self->yogaNode))
		);
	case LayoutProp::Overflow:
		return JS_NewString(
			ctx, overflowToString(YGNodeStyleGetOverflow(&self->yogaNode))
		);
	case LayoutProp::Display:
		return JS_NewString(
			ctx, displayToString(YGNodeStyleGetDisplay(&self->yogaNode))
		);
	case LayoutProp::PositionType:
		return JS_NewString(
			ctx,
			positionTypeToString(YGNodeStyleGetPositionType(&self->yogaNode))
		);
	case LayoutProp::Flex:
		return JS_NewFloat64(ctx, YGNodeStyleGetFlex(&self->yogaNode));
	case LayoutProp::FlexGrow:
		return JS_NewFloat64(ctx, YGNodeStyleGetFlexGrow(&self->yogaNode));
	case LayoutProp::FlexShrink:
		return JS_NewFloat64(ctx, YGNodeStyleGetFlexShrink(&self->yogaNode));
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
		YGNodeStyleSetDirection(&self->yogaNode, stringToDirection(sv));
		break;
	case LayoutProp::FlexDirection:
		YGNodeStyleSetFlexDirection(&self->yogaNode, stringToFlexDirection(sv));
		break;
	case LayoutProp::JustifyContent:
		YGNodeStyleSetJustifyContent(&self->yogaNode, stringToJustify(sv));
		break;
	case LayoutProp::AlignItems:
		YGNodeStyleSetAlignItems(&self->yogaNode, stringToAlign(sv));
		break;
	case LayoutProp::AlignSelf:
		YGNodeStyleSetAlignSelf(&self->yogaNode, stringToAlign(sv));
		break;
	case LayoutProp::AlignContent:
		YGNodeStyleSetAlignContent(&self->yogaNode, stringToAlign(sv));
		break;
	case LayoutProp::FlexWrap:
		YGNodeStyleSetFlexWrap(&self->yogaNode, stringToWrap(sv));
		break;
	case LayoutProp::Overflow:
		YGNodeStyleSetOverflow(&self->yogaNode, stringToOverflow(sv));
		break;
	case LayoutProp::Display:
		YGNodeStyleSetDisplay(&self->yogaNode, stringToDisplay(sv));
		break;
	case LayoutProp::PositionType:
		YGNodeStyleSetPositionType(&self->yogaNode, stringToPositionType(sv));
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
		YGNodeStyleSetFlex(&self->yogaNode, static_cast<float>(v));
		break;
	case LayoutProp::FlexGrow:
		YGNodeStyleSetFlexGrow(&self->yogaNode, static_cast<float>(v));
		break;
	case LayoutProp::FlexShrink:
		YGNodeStyleSetFlexShrink(&self->yogaNode, static_cast<float>(v));
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
	if (JS_IsNull(self->contentVal)) {
		return JS_NULL;
	}
	return JS_DupValue(ctx, self->contentVal);
}

JSValue LayoutNode_setContent(
	JSContext *ctx, JSValueConst this_val, JSValueConst val
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}

	JS_FreeValue(ctx, self->contentVal);

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
		self->contentVal = JS_DupValue(ctx, val);
	} else {
		self->contentVal = JS_NULL;
	}

	if (canMeasure) {
		YGNodeSetMeasureFunc(&self->yogaNode, yogaMeasureFunc);
		YGNodeMarkDirty(&self->yogaNode);
	} else {
		YGNodeSetMeasureFunc(&self->yogaNode, nullptr);
	}

	return JS_UNDEFINED;
}

// -- background color --
JSValue LayoutNode_getBgColor(JSContext *ctx, JSValueConst this_val) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	return JS_DupValue(ctx, self->backgroundColor);
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

	JS_FreeValue(ctx, self->backgroundColor);
	self->backgroundColor = *color_object;
	return JS_UNDEFINED;
}

// -- margin/padding shorthand --
enum class EdgeProp {
	Margin,
	Padding,
};

JSValue LayoutNode_getUndefined(
	JSContext *ctx, JSValueConst this_val, int magic
) noexcept {
	(void)ctx;
	(void)this_val;
	(void)magic;
	return JS_UNDEFINED;
}

JSValue LayoutNode_setEdgeProp(
	JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic
) noexcept {
	auto *self = LayoutNode::unwrap(ctx, this_val);
	if (!self) {
		return JS_UNDEFINED;
	}
	auto setEdge = static_cast<EdgeProp>(magic) == EdgeProp::Margin
		? YGNodeStyleSetMargin
		: YGNodeStyleSetPadding;
	if (JS_IsObject(val)) {
		auto toLen = [&](const char *prop) {
			JSValue v = JS_GetPropertyStr(ctx, val, prop);
			double result = std::numeric_limits<double>::quiet_NaN();
			if (JS_IsNumber(v)) {
				JS_ToFloat64(ctx, &result, v);
			}
			JS_FreeValue(ctx, v);
			return result;
		};
		setEdge(&self->yogaNode, YGEdgeTop, static_cast<float>(toLen("top")));
		setEdge(
			&self->yogaNode, YGEdgeRight, static_cast<float>(toLen("right"))
		);
		setEdge(
			&self->yogaNode, YGEdgeBottom, static_cast<float>(toLen("bottom"))
		);
		setEdge(&self->yogaNode, YGEdgeLeft, static_cast<float>(toLen("left")));
	} else if (JS_IsNumber(val)) {
		double v;
		JS_ToFloat64(ctx, &v, val);
		float fv = static_cast<float>(v);
		setEdge(&self->yogaNode, YGEdgeTop, fv);
		setEdge(&self->yogaNode, YGEdgeRight, fv);
		setEdge(&self->yogaNode, YGEdgeBottom, fv);
		setEdge(&self->yogaNode, YGEdgeLeft, fv);
	}
	return JS_UNDEFINED;
}

// -- tree --
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
		"margin", LayoutNode_getUndefined, LayoutNode_setEdgeProp,
		static_cast<int16_t>(EdgeProp::Margin)
	),

	cGetSetMagicDef(
		"padding", LayoutNode_getUndefined, LayoutNode_setEdgeProp,
		static_cast<int16_t>(EdgeProp::Padding)
	),

	cFuncDef("appendChild", 1, LayoutNode_appendChild),
	cFuncDef("removeChild", 1, LayoutNode_removeChild),

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
	JS_MarkValue(rt, self->contentVal, mark_func);
	JS_MarkValue(rt, self->backgroundColor, mark_func);
	for (const auto &child : self->children) {
		JS_MarkValue(rt, child.val, mark_func);
	}
}

void LayoutNode::finalize(JSRuntime *rt, JSValue val) noexcept {
	auto *self = unwrap(rt, val);
	if (!self) {
		return;
	}
	JS_FreeValueRT(rt, self->contentVal);
	JS_FreeValueRT(rt, self->backgroundColor);
	for (auto &child : self->children) {
		JS_FreeValueRT(rt, child.val);
	}
	delete self;
}

} // namespace wf::js
