# Assets Manifest Format

This document describes the format of `manifest.json`, which defines how assets are loaded and processed in WaveForge.

## Overview

The manifest file is a JSON document that specifies a sequence of operations to load and transform game assets. Assets are loaded in the order specified in the `sequence` array, allowing dependent assets to reference previously loaded ones.

## Structure

```json
{
  "format": 1,
  "sequence": [...]
}
```

### Top-level Fields

- **`format`** (integer, required): Manifest format version. Current version is `1`.
- **`sequence`** (array, required): Ordered list of asset operations to execute.

## Asset Operations

Each entry in the `sequence` array represents an asset operation with the following common fields:

- **`id`** (string, required): Unique identifier for the resulting asset. Used to reference this asset in later operations or in code.
- **`type`** (string, required): Type of operation to perform (see below).
- **`description`** (string, required): Human-readable description of the operation, displayed during loading.

### Operation Types

#### `image`
Load an image file from disk.

**Additional fields:**
- **`file`** (string, required): Path to the image file, relative to the assets directory.

**Example:**
```json
{
  "id": "duck/raw",
  "type": "image",
  "file": "duck.png",
  "description": "Loading duck PNG"
}
```

#### `trim-image`
Remove fully transparent borders from an image.

**Additional fields:**
- **`input`** (string, required): ID of the source image asset.

**Example:**
```json
{
  "id": "duck/image",
  "type": "trim-image",
  "input": "duck/raw",
  "description": "Trimming duck image"
}
```

#### `create-image-of-all-facings`
Create rotated versions of an image for all four facing directions (up, right, down, left).

**Additional fields:**
- **`input`** (string, required): ID of the source image asset.

**Example:**
```json
{
  "id": "duck/image_4facings",
  "type": "create-image-of-all-facings",
  "input": "duck/image",
  "description": "Creating duck images for all facings"
}
```

#### `calculate-shape`
Generate a `PixelShape` from an image, which provides efficient collision detection based on non-transparent pixels.

**Additional fields:**
- **`input`** (string, required): ID of the source image asset.

**Example:**
```json
{
  "id": "duck/shape",
  "type": "calculate-shape",
  "input": "duck/image",
  "description": "Calculating duck shape"
}
```

#### `create-pixel-shape-of-all-facings`
Generate `PixelShape` instances for all four facing directions from a source image.

**Additional fields:**
- **`input`** (string, required): ID of the source image asset.

**Example:**
```json
{
  "id": "duck/shape_4facings",
  "type": "create-pixel-shape-of-all-facings",
  "input": "duck/image",
  "description": "Creating duck shapes for all facings"
}
```

#### `create-texture`
Create a GPU texture from an image for rendering.

**Additional fields:**
- **`input`** (string, required): ID of the source image asset.

**Example:**
```json
{
  "id": "duck/texture",
  "type": "create-texture",
  "input": "duck/image",
  "description": "Creating duck texture"
}
```

#### `create-checkpoint-sprite`
Create the special checkpoint sprite animation. This operation has hardcoded dependencies on `checkpoint/image_1` and `checkpoint/image_2` assets.

**Additional fields:** None (uses hardcoded asset IDs).

**Example:**
```json
{
  "id": "checkpoint/sprite",
  "type": "create-checkpoint-sprite",
  "description": "Creating checkpoint sprite"
}
```

#### `music`
Load a music file from disk.

**Additional fields:**
- **`file`** (string, required): Path to the music file, relative to the assets directory.
- **`collections`** (array of strings, optional): List of collection IDs this music belongs to (for categorization).

**Example:**
```json
{
  "id": "music/Pixel Tide",
  "type": "music",
  "file": "Pixel Tide.mp3",
  "description": "Loading Pixel Tide music",
  "collections": ["background/main-menu-music"]
}
```

### `level-metadata`

Load level metadata from a JSON file.

**Additional fields:**
- **`file`** (string, required): Path to the level metadata JSON file, relative to the assets directory.

**Example:**
```json
{
  "id": "level/simple-intro",
  "type": "level-metadata",
  "file": "levels/simple_intro.json",
  "description": "Loading \"simple intro\" level metadata"
}
```

### `load-js`

Load a JavaScript source file (e.g., output from esbuild or another bundler) as a plain-text string asset.

**Additional fields:**
- **`file`** (string, required): Path to the JS file, relative to the assets directory.

The JS source is evaluated directly at runtime by `JSScene`.

**Example:**
```json
{
  "id": "ui/myscript",
  "type": "load-js",
  "file": "ui/myscript.js",
  "description": "Loading UI script"
}
```

## Asset Loading Process

1. The assets manager searches for `manifest.json` in the following locations (in order):
   - `$WAVEFORGE_ASSETS_PATH` (if environment variable is set)
   - `./assets`
   - `../assets`
   - `<executable_path>/assets`
   - `<executable_path>/../assets`
   - `/usr/share/waveforge/assets` (Linux only)
   - `/usr/local/share/waveforge/assets` (Linux only)

2. Each operation in the `sequence` array is executed in order.

3. Assets are cached by their ID and can be retrieved later using `AssetsManager::getAsset<T>(id)`.

## Best Practices

- Use descriptive IDs with namespacing (e.g., `duck/texture`, `music/theme`)
- Place base assets (files loaded from disk) before derived assets (transformations)
- Provide clear descriptions for each operation to aid debugging
- Ensure all referenced `input` IDs exist in prior sequence entries

## Error Handling

The game will log errors encountered during asset loading and then directly abort. Use debug build (`-DCMAKE_BUILD_TYPE=Debug`) to get more detailed (stack trace) information about the errors.
