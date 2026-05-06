/** WaveForge JS Scene API type declarations. */

declare global {
	const waveforge: WaveForge;
}

/** Pre-loaded texture handle. Obtain via `new waveforge.Texture(id)`. */
interface Texture {
	/** Asset ID used for draw sprite commands. */
	get id(): string;

	/** Texture width in pixels. */
	get width(): number;

	/** Texture height in pixels. */
	get height(): number;
}

/** Pre-loaded sound handle. Obtain via `new waveforge.Sound(id)`. */
interface Sound {
	/** Asset ID used for playback. */
	get id(): string;

	/** Duration in milliseconds. */
	get duration(): number;

	/** Play the sound (one-shot via ActiveSoundManager). */
	play(): void;
}

/** A single text draw command. Created via `new waveforge.DrawText(...)`. */
interface DrawText {
	readonly type: "text";
	readonly x: number;
	readonly y: number;
	readonly text: string;
	readonly size: number;
	readonly r: number;
	readonly g: number;
	readonly b: number;
}

/** A single sprite draw command. Created via `new waveforge.DrawSprite(...)`. */
interface DrawSprite {
	readonly type: "sprite";
	readonly x: number;
	readonly y: number;
	readonly textureId: string;
}

/** A single filled-rectangle draw command. Created via `new waveforge.DrawRect(...)`. */
interface DrawRect {
	readonly type: "rect";
	readonly x: number;
	readonly y: number;
	readonly w: number;
	readonly h: number;
	readonly r: number;
	readonly g: number;
	readonly b: number;
}

/** Accumulates draw commands and commits them to the render pipeline. */
interface DrawCmdBuffer {
	/** Append a draw command (DrawText, DrawSprite, or DrawRect). */
	add(cmd: DrawText | DrawSprite | DrawRect): void;

	/** Remove all commands from the buffer. */
	clear(): void;

	/** Iterate over buffered commands. */
	[Symbol.iterator](): Iterator<DrawText | DrawSprite | DrawRect>;
}

interface WaveForge {
	/** Create a pre-loaded texture handle (avoids repeated asset lookups). */
	Texture: { new(id: string): Texture };

	/** Create a pre-loaded sound handle. */
	Sound: { new(id: string): Sound };

	/** Create a text draw command. */
	DrawText: { new(x: number, y: number, text: string, size: number, r: number, g: number, b: number): DrawText };

	/** Create a sprite draw command (accepts Texture object or string asset ID). */
	DrawSprite: { new(x: number, y: number, texture: string | Texture): DrawSprite };

	/** Create a filled-rectangle draw command. */
	DrawRect: { new(x: number, y: number, w: number, h: number, r: number, g: number, b: number): DrawRect };

	/** Create a command buffer for accumulating draw commands. */
	DrawCmdBuffer: { new(): DrawCmdBuffer };

	/** Print arguments to stderr. */
	log(...args: unknown[]): void;

	/**
	 * Register scene lifecycle callbacks.
	 * Must be called exactly once during module body execution.
	 */
	setupScene(api: SceneAPI): void;

	/**
	 * Request a scene change.
	 * The change is buffered and applied after step() returns.
	 *
	 * Built-in IDs:
	 *   "main-menu" | "settings" | "help" | "credits"
	 *   "level-selection" | "__exit__"
	 *   "js:<name>"  — another JS scene
	 */
	changeScene(sceneId: string): void;

	/**
	 * Commit a DrawCmdBuffer to the render pipeline.
	 * Can only be called once per tick (throws TypeError otherwise).
	 * After commit, the buffer is emptied and can be reused.
	 */
	commitDrawCmds(buffer: DrawCmdBuffer): void;
}

export interface SceneAPI {
	/** Return the scene size in logical pixels. */
	size(): [number, number];

	/** Called once when the scene becomes active. */
	setup?(): void;

	/** Called per SFML event. */
	handleEvent?(event: SceneEvent): void;

	/** Called per frame, before render(). Update state here. */
	step?(): void;

	/** Called per frame. Create draw commands and commit them. */
	render?(): void;
}

export interface SceneEvent {
	type:
		| "keyPressed" | "keyReleased"
		| "mouseMoved" | "mousePressed" | "mouseReleased"
		| "closed" | "unknown";

	/** Key name for keyboard events (e.g. "Escape", "Space", "A"). */
	key?: string;

	/** SFML key code for keyboard events. */
	code?: number;

	/** Mouse / event coordinates (logical pixels). */
	x?: number;
	y?: number;

	/** Mouse button index. */
	button?: number;

	/** Modifier keys. */
	alt?: boolean;
	ctrl?: boolean;
	shift?: boolean;
}
