# laststory-native

A native PC port of **The Last Story** (Wii, Mistwalker / AQ Interactive) for the **USA disc**
(`SLSEXJ`), built by static recompilation: the game's PowerPC code translated to C++ and run on
a replacement for the Wii's hardware. No emulator at run time.

Goals beyond the original: 4K, a locked 60 fps, a bloom toggle, shaders compiled before play,
the Classic Controller layout on an Xbox pad with Xbox button icons in game, and a SETUP launcher
in the style of the title screen that walks the player through adding their disc.

## Built on

- **[pc-thelaststory](https://github.com/vs-sr-dev/pc-thelaststory)** by Samuele Voltan (MIT):
  the port's names, hooks and game layer in `tools/`, and its notes in `docs/upstream/`.
  It reaches Chapter 1 on the European disc.
- **[wiikit](https://github.com/vs-sr-dev/wiikit)** (MIT), the game-agnostic Wii toolkit,
  as a submodule at `wiikit/`.

## Where it stands

The USA disc boots and plays into Chapter 1 (`docs/usa.md`), with the engine set to 60 frames a
second (`docs/60fps.md`, still to be measured on a GPU). Next:
the rest of the goals above.

## BYOA: Bring Your Own Assets

No game code, data or assets are in this repository. You need your own disc image.

## Building

Python 3.8+, CMake, Ninja, clang and SDL3; OpenGL 4.5 to run.

```sh
git clone --recursive https://github.com/thebradshaw/laststory-native
cd laststory-native
tools/build.sh "The Last Story.wbfs" <Dolphin>/Sys
cd build/run1 && ../recomp-build/wiiboot ../extract --symbols ../recomp/symbols.tsv --nand ../nand --fonts ../fonts
```

`<Dolphin>/Sys` is any Dolphin install's `Sys` folder: `totaldb.dsy` names the SDK's functions
and `GC/` has free boot fonts and `dsp_coef.bin`. With SDL3 in a non-standard place, set
`CMAKE_ARGS=-DCMAKE_PREFIX_PATH=/opt/sdl3`.

Keys: F11 fullscreen, F12 a GX trace; `build/keys.txt` maps the keyboard.

## Licence

MIT, as upstream. It covers this repository's tools and documentation only, not The Last Story,
which belongs to its rights holders.
