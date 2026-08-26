# Scripted UI Scenes — Best Practices & Common Pitfalls

For the complete API listing of all classes, methods, and types exposed to JavaScript, see `types/waveforge/index.d.ts`.

## Overview

It scripted UI system is built on top of native C++ code and some Typescript utilities. This is a monorepo with both the engine code and all the Typescript utilities (renderer, styles, components) in one place.

The UI system is very new. Please feel free to extend the utilities as needed. The engine code is open to modification as well, please refer to [Creating New Bindings](bindings.md) for instructions on how to add new APIs.

## Scene Writing Styles

There are two approaches to writing a scene:

| Approach | When to use |
|---|---|
| **Immediate mode** (manual `commitLayout` + `commitDraw` each frame) | You need full control over draw order, overlay immediate-mode graphics on top of layouts, or don't want React's overhead |
| **React declarative** (JSX via `@waveforge/renderer`) | You want state-driven reactive updates, reusable components, and don't need custom draw commands |

React scenes use the `@waveforge/renderer` package, which internally calls `commitLayout` each frame. In most cases, React is the recommended approach for UI scenes due to its declarative nature and powerful state management.

## Immediate Mode Scenes

### Structure

```typescript
const root = new waveforge.LayoutNode();
root.flexDirection = 'column';
// ... build tree ...

const cmds = new waveforge.DrawCmdList();
cmds.push(new waveforge.DrawSpriteCmd(texture, x, y));
cmds.push(new waveforge.DrawTextCmd('hello', 10, 10, 1, '#fff'));

waveforge.addEventListener('step', () => {
    // mutate state, update nodes
    waveforge.commitLayout(root);
    waveforge.commitDraw(cmds);
});

export {};
```

### Must `export {}`

Scene files are ES modules. Always add `export {};` at the end to make TypeScript/esbuild treat the file as a module (prevents global scope collisions).

### `commitLayout` vs `commitDraw`

- `commitLayout(root)` — renders the Yoga layout tree. Nodes whose `content` is set are drawn as part of the layout pass. Overlapping content is determined by tree order.
- `commitDraw(cmds)` — renders immediate-mode commands **on top of** the layout. Draw commands are drawn in order and may overlay layout content.

Both can be called independently. You can commit a layout without draw commands, or draw commands without a layout.

### LayoutNode Tree Mutations

Mutate node properties directly between frames. The Yoga tree is re-laid-out on every `commitLayout` call, so changing `width`, `flexDirection`, `backgroundColor`, etc. and then calling `commitLayout` is the correct pattern. Avoid recreating the entire tree each frame.

Adding / removing nodes, changing style properties (like `flexDirection`), will cause the layout to be recalculated automatically. However, changing properties inside `content` (e.g. `TextContent.text`) does not trigger a layout recalculation, so you must call `node.relayout()` manually.

Relayout will not occur immediately — the engine guarantees that the layout will be updated before the next render, but if you need to read layout results with `node.getComputedBounds()`, it will return `null` if new layout results are not available yet, and other functions that depend on layout (e.g. `node.hitTest()`) may give incorrect results until the layout is updated. Calling `relayout()` or `commitLayout()` will not update the layout immediately as well. The only reliable way to get updated layout results is to wait for the next `step` event after mutating layout properties.

### `content` vs Draw Commands

Prefer `LayoutNode.content` (TextContent / SpriteContent) for UI elements that participate in layout. Use `DrawCmdList` only for elements that need to bypass layout (e.g. overlays, particle effects, debugging visualizations).

## React Declarative Scenes

### Entry Point

```tsx
import { render, View, Text } from '@waveforge/renderer';
import { style, column, center, bg } from '@waveforge/styles';
import React from 'react';

function App() {
    return (
        <View style={style({ width: 256, height: 192 }, column(), center(), bg('#222'))}>
            <Text>Hello</Text>
        </View>
    );
}

render(<App />);
```

No `export {}` needed — JSX files are automatically treated as modules.

### `render()` Sets Up the Engine

Calling `render(<App />)` on the first scene registers the engine listeners used for layout, pointer, keyboard, focus, and teardown handling. This means:

- The reconciler already owns the internal step handler that drives layout; you **should not** call `waveforge.commitLayout` manually — it does this automatically.
- You **may** register additional `step` listeners (e.g. via `useEffect`) for animation or polling — multiple step listeners are supported.
- Input listeners are managed by the renderer and dispatch synthetic events to hit-tested or focused nodes.
- Scene teardown synchronously unmounts the React root, so `useEffect` cleanup functions run before the QuickJS context is destroyed.

### Reactive State

Use `React.useState` and `React.useEffect` normally:

```tsx
function Counter() {
    const [count, setCount] = React.useState(0);

    // Subscribe to frame ticks for animation
    React.useEffect(() => {
        const handler = () => setCount(c => c + 1);
        waveforge.addEventListener('step', handler);
        return () => waveforge.removeEventListener('step', handler);
    }, []);

    return <View style={column()}><Text>{`count: ${count}`}</Text></View>;
}
```

Note: different with immediate mode, React scenes do not need to call `relayout()` in any situation.

### Event Handling

React synthetic events are dispatched by the renderer:

```tsx
<View onClick={(event) => console.log(event.x, event.y)}>
<View onPointerEnter={() => setHovered(true)}>
<View onPointerLeave={() => setHovered(false)}>
```

Pointer, keyboard, and focus events bubble. Call `event.stopPropagation()` to stop the current event before it reaches an ancestor, or `event.preventDefault()` to suppress renderer defaults such as Tab focus traversal. Synthetic events expose `target`, `currentTarget`, `nativeEvent`, `propagationStopped`, and `defaultPrevented`.

Set `tabIndex={0}` on an element to include it in Tab/Shift+Tab navigation. `tabIndex={-1}` allows pointer or programmatic focus but skips sequential keyboard navigation. Focused nodes receive `onKeyDown`, `onKeyUp`, `onFocus`, and `onBlur`. The renderer also exports `focus(node)` and `blur()` for imperative focus changes.

### `<Sprite>` Component

```tsx
const duck = new waveforge.Texture('duck/texture');

<Sprite texture={duck} scale={2} />
```

The `texture` must be a `waveforge.Texture` instance created **outside** the component to avoid recreating it on every render. The `scale` defaults to 1.

### `<Text>` Color

Text color can be set via the `color` prop or `style.color`:

```tsx
<Text color="#ff0">yellow</Text>
<Text style={{ color: '#ff0' }}>also yellow</Text>
<Text style={textColor('#ff0')}>also yellow</Text>
```

Default text color if unspecified: `#f0e6ff`.

Use the `size` prop for integer pixel-font scaling:

```tsx
<Text size={2}>Heading</Text>
```

### Props Are Diffed on Every Render

The reconciler compares the previous and new effective layout props on every re-render. Removed properties are reset to the defaults of a fresh native `LayoutNode`; this applies equally to direct props and properties inside `style`. Direct props take precedence over values in `style` when both specify the same layout property.

### Use `React.useCallback` for Event Handlers

To avoid unnecessary re-renders (the reconciler always calls `commitUpdate` even if props didn't change, but `useCallback` still helps keep the closure stable):

```tsx
const handleClick = React.useCallback(() => {
    console.log('clicked');
}, []);
```

---

## Style Helpers (`@waveforge/styles`)

Style helpers return plain objects and compose with `style()`:

```tsx
style(
    size(256, 192),  // { width: 256, height: 192 }
    column(),         // { flexDirection: 'column' }
    center(),         // { justifyContent: 'center', alignItems: 'center' }
    bg('#333'),       // { backgroundColor: '#333' }
    padding(4, 12),   // { paddingTop: 4, paddingBottom: 4, paddingRight: 12, paddingLeft: 12 }
)
```

Falsy values are skipped, enabling conditional styles:

```tsx
style(column(), isActive && bg('#4ecdc4'))
```

### Layout Props: `style` vs Direct Props

Host components accept layout properties both inside the `style` prop and as direct props — both paths end up in the same `applyProps` call:

```tsx
{/* These three are equivalent: */}
<View backgroundColor="#333" flexDirection="column" />
<View style={{ backgroundColor: '#333', flexDirection: 'column' }} />
<View style={style(bg('#333'), column())} />
```

No need to choose one over the other. The `style` prop is useful for merging conditional styles. It is suggested to keep all styles in the `style` prop for consistency.

### More style helpers

The `@waveforge/styles` package is open to new style helpers. Create new ones as needed.

## Component Libraries

We build reusable components on top of the renderer. The `@waveforge/components` package provides common UI elements like `Button`.

### The `Button` Component (`@waveforge/components`)

```tsx
import { Button } from '@waveforge/components';

<Button label="Click" variant="primary" onClick={handler} />
<Button label="Delete" variant="secondary" onClick={handler} />
<Button label="Save" variant="accent" onClick={handler} />
```

Buttons use hover state internally (`onPointerEnter`/`onPointerLeave`). If you need a custom clickable element, replicate the pattern from the Button source rather than trying to extend Button.

### Common widgets and hooks

The component package also exports reusable layout and navigation building blocks:

- `Screen`, `Row`, `Column`, `Center`, and `Panel` for scene structure.
- `Label`, `Title`, and `Hint` for consistent typography and colors.
- `MenuItem` and `useMenuNavigation` for keyboard and pointer menus.
- `useEngineEvent`, `useKeyDown`, and `useStep` for lifecycle-safe engine subscriptions.
- `playUiSound` and the shared `UI_COLORS` / `UI_SPACING` tokens.

Prefer extending these primitives when multiple scenes need the same behavior.

## Scene Lifecycle

1. C++ `ModuleRegistry` loads the bundled JS file.
2. The script executes top-to-bottom (imports, top-level `console.log`, `render()` call, `addEventListener` calls).
3. The engine enters the event loop: `step` fires each frame, `commitLayout` renders the tree.
4. `navigateTo(id)` queues a scene transition. After the active callback returns, the engine emits `teardown`, the renderer unmounts its React root, and `React.useEffect` cleanup functions run before the old JS context is destroyed.

Immediate-mode scenes can register `waveforge.addEventListener('teardown', callback)` when they need explicit cleanup. Scene transitions are deferred until the current event or step callback returns, so code after `navigateTo()` can finish safely.

## Platform Services

Scripted scenes access persistent settings through `waveforge.storage`, background music through `waveforge.music`, and the read-only level catalog through `waveforge.levels`. `waveforge.version` and `waveforge.debug` expose build metadata, while `waveforge.close()` requests application shutdown.

Keep scene code declarative by wrapping repeated service interactions in component hooks rather than reading storage during every render.

## Manifest Registration

Register the scene in `assets/manifest.json`:

```json
{
    "id": "scripts/my_scene",
    "type": "ui-scene",
    "width": 256,
    "height": 192,
    "description": "Loading scripts/my_scene"
}
```

The `id` must match the file stem (no extension). The `width` and `height` are available as `waveforge.width` / `waveforge.height`.

## Performance Considerations

Follow the regular React performance best practices: memoize components, avoid unnecessary state updates, and keep the component tree shallow if possible. Further more, avoid allocating new objects/arrays/functions inside the render loop, as GC cuycles can cause frame drops (the QuickJS GC is not generational, and the engine don't have complex optimizations like running partial GC on a separate thread or between frames like browsers do, so GC pauses can be more noticeable).
