declare namespace waveforge {
	function log(message: string): void;

	function setupScene(scene: SceneConfig): void;

	function commitDraw(cmds: DrawCmdBuffer): void;

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

	class DrawCmdBuffer {
		push(cmd: DrawTextCmd | DrawSpriteCmd | DrawRectCmd): void;
		[Symbol.iterator](): Iterator<DrawTextCmd | DrawSpriteCmd | DrawRectCmd>;
	}

	interface SceneConfig {
		size(): [number, number];
		setup?(): void;
		step?(): void;
		render?(): void;
	}
}
