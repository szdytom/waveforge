# JS Scene System

This document describes the JavaScript-based scene system in WaveForge.
JS scenes run on the embedded QuickJS engine and provide a scriptable way
to create menus, UI overlays, and gameplay logic.

## Architecture

```
┌───────────────────────────────────────────────────────────-──┐
│                    Game Loop (C++)                           │
│  SceneManager::tick() → JSScene::step() → JSScene::render()│
└──────────────────────┬────────────────────────────────────-──┘
                       │ calls JS exports
                       v
┌─────────────────────────────────────────────────────────────┐
│                    QuickJS Runtime (C++)                    │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  JS Module (User code)                                │  │
│  │  waveforge.setupScene({ size, setup, step, render })  │  │
│  └──────────────────────┬────────────────────────────────┘  │
│                         │ calls native functions            │
│                         v                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  waveforge namespace (C native functions)             │  │
│  │  log, drawText, drawSprite, drawRect, playSound,      │  │
│  │  changeScene, setupScene, getCommands, clearCommands  │  │
│  └───────────────────────────────────────────────────────┘  │
│                         │                                   │
│                         v                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  DrawCmd Buffer (C++)                                 │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────────┘
                           │ flushed per frame
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              SFML RenderTarget (C++)                        │
│  Font::renderText, sf::Sprite, sf::RectangleShape           │
└─────────────────────────────────────────────────────────────┘
```

## API Reference

### Global Namespace: `waveforge`

All native functions are available via the global `waveforge` object,
no import statement required.

```js
waveforge.log("Hello from JS!");
waveforge.drawText(10, 10, "Hello", 1, 255, 255, 255);
```

### `waveforge.log(...args)`

Prints arguments to stderr (visible in terminal output). Accepts any
number of arguments.

### `waveforge.setupScene(api)`

Registers the scene lifecycle callbacks. Must be called exactly once
during module body execution. The `api` object can have these methods:

| Method | Signature | Required | Description |
|--------|-----------|----------|-------------|
| `size` | `() => [number, number]` | Yes | Returns `[width, height]` in logical pixels |
| `setup` | `() => void` | No | Called once when scene starts |
| `handleEvent` | `(event) => void` | No | Called per SFML event |
| `step` | `() => void` | No | Called per frame, before render |
| `render` | `() => void` | No | Called per frame; push draw commands here |

### `waveforge.drawText(x, y, text, size, r, g, b)`

Queues a text draw command. Coordinates are in logical pixels (C++
multiplies by scale factor). Text is rendered using the game's pixel
font. Color is RGB with range 0-255.

### `waveforge.drawSprite(x, y, textureId)`

Queues a sprite draw command. `textureId` is the asset ID of a loaded
texture (e.g., `"ui/main-menu-background"`). The sprite is scaled to
match the current scale factor.

### `waveforge.drawRect(x, y, w, h, r, g, b)`

Queues a filled rectangle draw command.

### `waveforge.playSound(id)`

Plays a one-shot sound effect. `id` is the asset ID of a loaded
`sf::SoundBuffer` (e.g., `"sfx/forward"`).

### `waveforge.changeScene(sceneId)`

Requests a scene change. The scene is NOT changed immediately; the
request is buffered and applied after `step()` returns. Built-in
scene IDs:

| ID | Scene |
|----|-------|
| `"main-menu"` | Main menu |
| `"settings"` | Settings menu |
| `"help"` | Help screen |
| `"credits"` | Credits |
| `"level-selection"` | Level selection |
| `"__exit__"` | Exit the game |
| `"js:<name>"` | JS scene with ID `<name>` |

### `waveforge.getCommands()` / `waveforge.clearCommands()`

Used by existing React-based JS scenes. Returns an array of draw
command objects, or clears the buffer. Each command is an object:
`{ type: "text"|"sprite"|"rect", x, y, ... }`.

## Event Object

The `handleEvent` callback receives an event object with a `type` field:

| Type | Additional Fields |
|------|-------------------|
| `"keyPressed"` | `key` (string), `code` (number), `alt`, `ctrl`, `shift` (boolean) |
| `"keyReleased"` | `key` (string), `code` (number) |
| `"mouseMoved"` | `x`, `y` (number) |
| `"mousePressed"` | `x`, `y`, `button` (number) |
| `"mouseReleased"` | `x`, `y`, `button` (number) |
| `"closed"` | none |

Coordinates are in logical pixels (not multiplied by scale).

## Writing a JS Scene

### Minimal example (direct JS, no bundler)

Create `assets/bundled-js/my-scene.js`:

```js
waveforge.log("Loading my scene...");

waveforge.setupScene({
    size() { return [400, 300]; },

    setup() {
        waveforge.log("Scene ready");
    },

    handleEvent(event) {
        if (event.type === "keyPressed" && event.key === "Escape") {
            waveforge.changeScene("main-menu");
        }
    },

    step() {
        // update logic here
    },

    render() {
        waveforge.drawText(10, 10, "My Scene", 1, 255, 255, 255);
    }
});
```

Add to `assets/manifest.json`:

```json
{
    "id": "js/my-scene/source",
    "type": "load-js",
    "file": "bundled-js/my-scene.js",
    "description": "Loading my scene"
},
{
    "id": "js/my-scene/bytecode",
    "type": "bytecode-from-js",
    "input": "js/my-scene/source",
    "description": "Compiling my scene",
    "module": true,
    "filename": "js/my-scene/source.js"
}
```

Run: `./build/waveforge "js:my-scene" --scale 2`

### TypeScript example (with esbuild)

See `ts/scene-test.ts` and the `ts/Makefile` for the TypeScript
workflow. Write TS in `ts/`, compile with `cd ts && make`, then
run `./build/waveforge "js:scene-test" --scale 2`.

## How It Works

### Lifecycle

1. **Module evaluation**: During `JSScene` construction, the JS source
   is evaluated as an ES module (`JS_Eval` with `JS_EVAL_TYPE_MODULE`).
   The module body must call `waveforge.setupScene()` to register the
   scene callbacks.

2. **`setup()`**: Called once when the scene becomes active, after the
   window is created.

3. **`handleEvent()`**: Called for each SFML event in the event queue.
   The C++ side converts the event to a plain JS object.

4. **`step()`**: Called once per frame, before rendering. Scene state
   (physics, counters, positions) should be updated here.

5. **`render()`**: Called once per frame. JS code pushes draw commands
   via `waveforge.drawText/Sprite/Rect`. After `render()` returns, C++
   iterates the draw command buffer and renders via SFML.

### Draw Command Buffer

Draw commands are accumulated in a `std::vector<DrawCmd>` during
`render()`. After the JS `render()` export returns, C++ iterates the
buffer and renders each command with the current scale factor applied.
The buffer is cleared after each frame.

### Scene Change Buffering

`waveforge.changeScene()` does NOT immediately destroy the current JS
scene. Instead, it stores the request in a deferred buffer. The actual
change happens after `step()` returns, ensuring clean destruction of
the QuickJS runtime.

### QuickJS Integration

- QuickJS 2025-09-13 embedded as a static library
- Each `JSScene` has its own `JSRuntime` and `JSContext` (isolated heap)
- The `waveforge` C module is registered in the module registry for
  bundler compatibility (`import * as wf from 'waveforge'`)
- Scene JS is evaluated from source (not bytecode) for simplicity;
  bytecode compilation is used for asset pipeline validation and
  future pre-compilation workflows
