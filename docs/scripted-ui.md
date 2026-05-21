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

Calling `render(<App />)` on the first scene internally calls `waveforge.addEventListener('step', ...)` and `waveforge.addEventListener('mousebutton', ...)`. This means:

- The reconciler already owns the internal step handler that drives layout; you **should not** call `waveforge.commitLayout` manually — it does this automatically.
- You **may** register additional `step` listeners (e.g. via `useEffect`) for animation or polling — multiple step listeners are supported.
- The `mousebutton` listener is managed by the reconciler and dispatches `onClick` events to hit-tested nodes with event bubbling.

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

React synthetic events are dispatched by the reconciler:

```tsx
<View onClick={() => console.log('clicked')}>
<View onPointerEnter={() => setHovered(true)}>
<View onPointerLeave={() => setHovered(false)}>
```

Events bubble: clicking a child triggers the child's `onClick`, then the parent's, and so on up the tree. **The first handler found during bubble-up stops propagation** (no `stopPropagation` API). If more complex event handling is needed, consider extending the `@waveforge/renderer` source, which is open to modification.

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

### Props Are Sent to `applyProps` Every Render

The reconciler calls `commitUpdate` with the new props on every re-render. The `applyProps` function iterates all props and overwrites them on the `LayoutNode`. This means:

- If a prop is **not** set in the new render, it does **not** get cleared — `applyProps` only writes props that exist.
- To "remove" a prop, explicitly set it to `undefined` or to its default value.
- Style obects are merged via `Object.assign`, so only the keys present in the new style object are overwritten.

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

### More components

The `@waveforge/components` package welcomes changes. Create new reusable components or extend existing ones as needed.

## Scene Lifecycle

1. C++ `ModuleRegistry` loads the bundled JS file.
2. The script executes top-to-bottom (imports, top-level `console.log`, `render()` call, `addEventListener` calls).
3. The engine enters the event loop: `step` fires each frame, `commitLayout` renders the tree.
4. `navigateTo(id)` creates a new scene. The current scene's listeners and state are discarded (no explicit cleanup — `React.useEffect` cleanup runs if the component unmounts, but the engine does not call them on navigation).

**Important:** There is no lifecycle hook for scene teardown as of now, but it can be supported with minimal changes to the engine if needed, and this feature is planned. If your scene needs to clean up resources on navigation, consider work on the engine to add a teardown hook first. `React.useEffect` cleanup functions won't run on navigation because the entire scene is hard-replaced, not unmounted.

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
