/** WaveForge JS Scene API type declarations. */

declare global {
	const waveforge: WaveForge;
}

/** Pre-loaded texture handle. Obtain via `new waveforge.Texture(id)`. */
interface Texture {
	/** @internal Asset ID used for serialization in getCommands(). */
	get id(): string;

	/** Texture width in pixels. */
	get width(): number;

	/** Texture height in pixels. */
	get height(): number;
}

interface WaveForge {
	/** Create a pre-loaded texture handle (avoids repeated asset lookups). */
	Texture: { new(id: string): Texture };

	/** Print arguments to stderr. */
	log(...args: unknown[]): void;

	/**
	 * Register scene lifecycle callbacks.
	 * Must be called exactly once during module body execution.
	 */
	setupScene(api: SceneAPI): void;

	/**
	 * Queue a text draw command.
	 * Coordinates are in logical pixels (C++ multiplies by scale).
	 */
	drawText(
		x: number, y: number, text: string,
		size: number, r: number, g: number, b: number
	): void;

	/**
	 * Queue a sprite draw command.
	 * @param texture Texture instance (from `new waveforge.Texture(id)`)
	 *   or a string asset ID (e.g. "ui/main-menu-background").
	 *   Preloading via `new waveforge.Texture(id)` avoids repeated
	 *   lookups in the assets manager.
	 */
	drawSprite(x: number, y: number, texture: string | Texture): void;

	/**
	 * Queue a filled rectangle draw command.
	 */
	drawRect(
		x: number, y: number, w: number, h: number,
		r: number, g: number, b: number
	): void;

	/**
	 * Play a one-shot sound effect.
	 * @param id Asset ID of a loaded sf::SoundBuffer
	 *   (e.g. "sfx/forward")
	 */
	playSound(id: string): void;

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
	 * Get accumulated draw commands as an array of plain objects.
	 * Each entry has: { type: "text"|"sprite"|"rect", x, y, ... }
	 */
	getCommands(): DrawCommand[];

	/** Clear the draw command buffer. */
	clearCommands(): void;
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

	/** Called per frame. Push draw commands via waveforge.draw*. */
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

export type DrawCommand =
	| DrawTextCommand
	| DrawSpriteCommand
	| DrawRectCommand;

export interface DrawTextCommand {
	readonly type: "text";
	readonly x: number;
	readonly y: number;
	readonly text: string;
	readonly size: number;
	readonly r: number;
	readonly g: number;
	readonly b: number;
}

export interface DrawSpriteCommand {
	readonly type: "sprite";
	readonly x: number;
	readonly y: number;
	readonly textureId: string;
}

export interface DrawRectCommand {
	readonly type: "rect";
	readonly x: number;
	readonly y: number;
	readonly w: number;
	readonly h: number;
	readonly r: number;
	readonly g: number;
	readonly b: number;
}
