# Waveforge

Waveforge is the game I'm developing for [GitHub Game Off 2025](https://itch.io/jam/game-off-2025).

## The concept

Waveforge is a 2D platformer where the player needs to manipulate water waves (and other physical phenomena) to somehow move a rubber duck to the checkpoint in a pixel emulated world with physics simulation (of it's own style, not always realistic though).

For more details about the game physics system, please refer to [Physics System Documentation](docs/physics-system.md).

## Current status

The game is finished to a playable state with 18 levels, published as version 0.4 on [Itch.io](https://fang-erj.itch.io/waveforge). Try it out and let me know what you think!

## Build instructions

You need to have CMake and a C++23 compatible compiler installed (e.g. GCC 14, Clang 20, MSVC 19.44.35219.0). Then run the following commands in the project root directory:

```bash
# Make sure to enter MSVC environment when on Windows
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo .
cmake --build build --config RelWithDebInfo
```

The direct dependencies will be automatically downloaded and built. Find the executable in `build` directory. The built program can be found at `build/waveforge` (or `build/waveforge.exe` on Windows).

For Linux systems, SFML might requires some additional system libraries. The simplest way is to install SFML via your package manager, so that all those internal dependencies are automatically handled. For example:

```bash
# Debian/Ubuntu
sudo apt install libsfml-dev

# Arch Linux and dirivatives
sudo pacman -S sfml
```

You can also install those dependencies manually if you prefer not to install SFML system-wide. Please refer to SFML's official documentation for more details.

Besides the executable, you all need to build the Typescript scripts as well, which are located in `ts` directory. You need to have `make` and `esbuild` installed for this. Then run the following command in the project root directory:

```bash
make -C ts -j$(nproc)
```

The makefile assumes that a executable named `esbuild` is available in your PATH. If it's not the case, you can set the `ESBUILD` environment variable to point to the esbuild executable, for example, using `pnpm`:

```bash
make -C ts -j$(nproc) ESBUILD="pnpm exec esbuild"
```

## Team Members

- Code: [fang_erj](https://github.com/szdytom)
- Music & SFX: [stevvven](https://github.com/Stevvven777)
- Pixel Art: [ZTL-UwU](https://github.com/ZTL-UwU/)

## Acknowledgements

We would like to thank zurry for thier inspiring thoughts on game play, mechanics and level design. We would also like to thank [RitaRossweisse301](https://github.com/RitaRossweisse301) for early testing and feedback.

We would also like to thank the following open source projects for making this game possible:

- [proxy](https://github.com/ngcpp/proxy): A C++ library for "Next Generation Polymorphism".

- [SFML](https://www.sfml-dev.org/): Simple and Fast Multimedia Library for graphics and audio.

- [cpptrace](https://github.com/jeremy-rifkin/cpptrace): A simple & self-contained C++ stack trace library.

- [QuickJS (NG fork)](https://github.com/quickjs-ng/quickjs): A community fork of [Fabrice Bellard's original QuickJS](https://bellard.org/quickjs/), a small and embeddable JavaScript engine.

- [Aseprite](https://www.aseprite.org/): A pixel art tool used for creating pixel art assets.

Additionally, we would like to acknowledge GitHub and [Lee Reilly](https://leereilly.net/) for organizing the [GitHub Game Off 2025](https://itch.io/jam/game-off-2025) game jam, which provided the inspiration and motivation for this project.
