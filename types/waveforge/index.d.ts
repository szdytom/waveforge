// ── *.txt module declaration ──
declare module '*.txt' {
	const content: string;
	export default content;
}

// ── waveforge runtime namespace (exposed by C++ host) ──
declare namespace waveforge {
	const width: number;
	const height: number;
	function setTitle(title: string): void;

	function addEventListener(type: 'step', callback: () => void): void;
	function addEventListener(type: 'key', callback: (event: KeyEvent) => void): void;
	function addEventListener(type: 'mousebutton', callback: (event: MouseButtonEvent) => void): void;
	function addEventListener(type: 'mousemove', callback: (event: MouseMoveEvent) => void): void;
	// biome-ignore lint/complexity/noBannedTypes: waveforge C++ host accepts any callback
	function removeEventListener(type: 'step' | 'key' | 'mousebutton' | 'mousemove', callback: Function): void;

	function commitDraw(cmds: DrawCmdList): void;

	function commitLayout(root: LayoutNode): void;

	/**
	 * Navigate to a scene by route ID, optionally passing data.
	 * Data is available on the target scene via `waveforge.getRouteData()`.
	 */
	function navigateTo(id: string, data?: string): void;

	/**
	 * The route data string that was passed to `navigateTo`
	 * when this scene was opened, or null if no data was passed.
	 */
	const routeData: string | null;

	class Texture {
		readonly width: number;
		readonly height: number;
		readonly id: string;

		constructor(id: string);
	}

	type ColorLike = Color | string | number | [number, number, number] | [number, number, number, number];

	class LayoutNode {
		width: number | string | undefined;
		height: number | string | undefined;
		minWidth: number | string | undefined;
		maxWidth: number | string | undefined;
		minHeight: number | string | undefined;
		maxHeight: number | string | undefined;

		direction: 'inherit' | 'ltr' | 'rtl';
		flexDirection: 'column' | 'columnReverse' | 'row' | 'rowReverse';
		justifyContent: 'flexStart' | 'center' | 'flexEnd' | 'spaceBetween' | 'spaceAround' | 'spaceEvenly';
		alignItems:
			| 'auto'
			| 'flexStart'
			| 'center'
			| 'flexEnd'
			| 'stretch'
			| 'baseline'
			| 'spaceBetween'
			| 'spaceAround'
			| 'spaceEvenly';
		alignSelf:
			| 'auto'
			| 'flexStart'
			| 'center'
			| 'flexEnd'
			| 'stretch'
			| 'baseline'
			| 'spaceBetween'
			| 'spaceAround'
			| 'spaceEvenly';
		alignContent:
			| 'auto'
			| 'flexStart'
			| 'center'
			| 'flexEnd'
			| 'stretch'
			| 'baseline'
			| 'spaceBetween'
			| 'spaceAround'
			| 'spaceEvenly';
		flexWrap: 'noWrap' | 'wrap' | 'wrapReverse';
		overflow: 'visible' | 'hidden' | 'scroll';
		display: 'flex' | 'none' | 'contents';
		positionType: 'static' | 'relative' | 'absolute';

		flex: number;
		flexGrow: number;
		flexShrink: number;

		content: TextContent | SpriteContent | null;
		contentAlignH: 'left' | 'center' | 'right';
		contentAlignV: 'top' | 'horizon' | 'bottom';
		marginLeft: number | string | undefined;
		marginRight: number | string | undefined;
		marginTop: number | string | undefined;
		marginBottom: number | string | undefined;
		paddingLeft: number | string | undefined;
		paddingRight: number | string | undefined;
		paddingTop: number | string | undefined;
		paddingBottom: number | string | undefined;

		gap: number | string | undefined;
		rowGap: number | string | undefined;
		columnGap: number | string | undefined;

		left: number | string | undefined;
		right: number | string | undefined;
		top: number | string | undefined;
		bottom: number | string | undefined;

		borderLeft: number | undefined;
		borderRight: number | undefined;
		borderTop: number | undefined;
		borderBottom: number | undefined;

		get borderLeftColor(): Color | undefined;
		set borderLeftColor(value: ColorLike | undefined);
		get borderRightColor(): Color | undefined;
		set borderRightColor(value: ColorLike | undefined);
		get borderTopColor(): Color | undefined;
		set borderTopColor(value: ColorLike | undefined);
		get borderBottomColor(): Color | undefined;
		set borderBottomColor(value: ColorLike | undefined);

		get backgroundColor(): Color | undefined;
		set backgroundColor(value: ColorLike | undefined);

		readonly childCount: number;
		readonly firstChild: LayoutNode | undefined;
		readonly lastChild: LayoutNode | undefined;
		readonly parent: LayoutNode | undefined;
		appendChild(child: LayoutNode): void;
		removeChild(child: LayoutNode): void;
		insertBefore(newChild: LayoutNode, referenceChild?: LayoutNode): void;
		replaceChild(newChild: LayoutNode, oldChild: LayoutNode): void;
		hasChildNodes(): boolean;
		getRootNode(): LayoutNode;
		childItem(index: number): LayoutNode | null;

		getComputedBounds(): { x: number; y: number; width: number; height: number } | null;
		hitTest(x: number, y: number): LayoutNode | null;
		relayout(): void;

		constructor();
	}

	class Color {
		r: number;
		g: number;
		b: number;
		a: number;

		constructor(color: ColorLike);
		constructor(r: number, g: number, b: number, a?: number);

		toString(): string;
		valueOf(): number;
	}

	class Sound {
		constructor(id: string);
		play(): void;
	}

	class TextContent {
		text: string;
		size: number;
		get color(): Color | undefined;
		set color(value: ColorLike | undefined);

		constructor(text: string, size?: number, color?: ColorLike);
	}

	class SpriteContent {
		texture: Texture;
		size: number;

		constructor(texture: Texture, size?: number);
	}

	class DrawTextCmd {
		text: string;
		x: number;
		y: number;
		size: number;
		get color(): Color | undefined;
		set color(value: ColorLike | undefined);

		constructor(text: string, x: number, y: number, size?: number, color?: ColorLike);
	}

	class DrawSpriteCmd {
		texture: Texture;
		x: number;
		y: number;

		constructor(texture: Texture, x: number, y: number);
	}

	class DrawRectCmd {
		x: number;
		y: number;
		width: number;
		height: number;
		get color(): Color | undefined;
		set color(value: ColorLike | undefined);

		constructor(x: number, y: number, width: number, height: number, color?: ColorLike);
	}

	class DrawCmdList {
		push(cmd: DrawTextCmd | DrawSpriteCmd | DrawRectCmd): void;
		[Symbol.iterator](): Iterator<DrawTextCmd | DrawSpriteCmd | DrawRectCmd>;
	}

	type KeyCode =
		| 'a'
		| 'b'
		| 'c'
		| 'd'
		| 'e'
		| 'f'
		| 'g'
		| 'h'
		| 'i'
		| 'j'
		| 'k'
		| 'l'
		| 'm'
		| 'n'
		| 'o'
		| 'p'
		| 'q'
		| 'r'
		| 's'
		| 't'
		| 'u'
		| 'v'
		| 'w'
		| 'x'
		| 'y'
		| 'z'
		| '0'
		| '1'
		| '2'
		| '3'
		| '4'
		| '5'
		| '6'
		| '7'
		| '8'
		| '9'
		| 'F1'
		| 'F2'
		| 'F3'
		| 'F4'
		| 'F5'
		| 'F6'
		| 'F7'
		| 'F8'
		| 'F9'
		| 'F10'
		| 'F11'
		| 'F12'
		| 'F13'
		| 'F14'
		| 'F15'
		| 'Control'
		| 'Shift'
		| 'Alt'
		| 'Meta'
		| 'ArrowLeft'
		| 'ArrowRight'
		| 'ArrowUp'
		| 'ArrowDown'
		| 'PageUp'
		| 'PageDown'
		| 'Home'
		| 'End'
		| 'Insert'
		| 'Delete'
		| 'Escape'
		| 'Space'
		| 'Enter'
		| 'Backspace'
		| 'Tab'
		| 'Pause'
		| 'ContextMenu'
		| '['
		| ']'
		| ';'
		| ','
		| '.'
		| "'"
		| '/'
		| '\\'
		| '`'
		| '='
		| '-'
		| '+'
		| '*'
		| 'Unknown';

	class KeyEvent {
		readonly type: 'keydown' | 'keyup';
		readonly code: KeyCode;
		readonly alt: boolean;
		readonly control: boolean;
		readonly shift: boolean;
		readonly system: boolean;
	}

	class MouseButtonEvent {
		readonly type: 'mousedown' | 'mouseup';
		readonly button: number;
		readonly x: number;
		readonly y: number;
	}

	class MouseMoveEvent {
		readonly type: 'mousemove';
		readonly x: number;
		readonly y: number;
	}

	type SceneEvent = KeyEvent | MouseButtonEvent | MouseMoveEvent;
}

// ── performance API ──
interface Performance {
	now(): number;
}
declare var performance: Performance;

// ── rAF (global, same as browser API) ──
declare function requestAnimationFrame(callback: (timestamp: number) => void): number;
declare function cancelAnimationFrame(id: number): void;
