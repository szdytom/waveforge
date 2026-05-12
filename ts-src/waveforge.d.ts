declare namespace waveforge {
	function log(message: string): void;

	function setupScene(scene: SceneConfig): void;

	function commitDraw(cmds: DrawCmdList): void;

	class Texture {
		readonly width: number;
		readonly height: number;
		readonly id: string;

		constructor(id: string);
	}

	class Color {
		r: number;
		g: number;
		b: number;
		a: number;

		constructor(color: string | number | [number, number, number] | [number, number, number, number]);
		constructor(r: number, g: number, b: number, a?: number);

		toString(): string;
		valueOf(): number;
	}

	class DrawTextCmd {
		text: string;
		x: number;
		y: number;
		size: number;
		r: number;
		g: number;
		b: number;
		a: number;

		constructor(text: string, x: number, y: number, size?: number, color?: Color);
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
		r: number;
		g: number;
		b: number;
		a: number;

		constructor(x: number, y: number, width: number, height: number, color?: Color);
	}

	class DrawCmdList {
		push(cmd: DrawTextCmd | DrawSpriteCmd | DrawRectCmd): void;
		[Symbol.iterator](): Iterator<DrawTextCmd | DrawSpriteCmd | DrawRectCmd>;
	}

	type KeyCode =
		| "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j"
		| "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t"
		| "u" | "v" | "w" | "x" | "y" | "z"
		| "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
		| "F1" | "F2" | "F3" | "F4" | "F5" | "F6" | "F7" | "F8" | "F9"
		| "F10" | "F11" | "F12" | "F13" | "F14" | "F15"
		| "Control" | "Shift" | "Alt" | "Meta"
		| "ArrowLeft" | "ArrowRight" | "ArrowUp" | "ArrowDown"
		| "PageUp" | "PageDown" | "Home" | "End" | "Insert" | "Delete"
		| "Escape" | "Space" | "Enter" | "Backspace" | "Tab" | "Pause"
		| "ContextMenu"
		| "[" | "]" | ";" | "," | "." | "'" | "/" | "\\" | "`" | "="
		| "-" | "+" | "*"
		| "Unknown";

	class KeyEvent {
		readonly type: "keydown" | "keyup";
		readonly code: KeyCode;
		readonly alt: boolean;
		readonly control: boolean;
		readonly shift: boolean;
		readonly system: boolean;
	}

	class MouseButtonEvent {
		readonly type: "mousedown" | "mouseup";
		readonly button: number;
		readonly x: number;
		readonly y: number;
	}

	class MouseMoveEvent {
		readonly type: "mousemove";
		readonly x: number;
		readonly y: number;
	}

	type SceneEvent = KeyEvent | MouseButtonEvent | MouseMoveEvent;

	interface SceneConfig {
		size(): [number, number];
		setup?(): void;
		step?(): void;
		render?(): void;
		handleEvent?(event: SceneEvent): void;
	}
}
