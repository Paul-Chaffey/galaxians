# Galaxians

A remake of the 1979 arcade shooter, written in C++20 on Vulkan 1.3 and SDL3.
It runs as a self-playing attract-mode demo: title screen, score table, then an
AI pilot playing a real game. Press Start at any point to take over.

All artwork and sound is original: the sprites are generated from ASCII art by
`tools/make_atlas.py`, and the sound effects are synthesised in code at startup.
The sound models the original board's circuits as documented by MAME's
emulation: a monophonic tone generator stepped once per video frame, a 555
"fire" oscillator, LFSR noise, and three LFO-swept 555s for the swarm drone.

## Features

- Renders at the arcade's native 224×256, then scales up by whole pixels, with
  an optional CRT effect (curvature, glow, scanlines, aperture grille, vignette)
- The 46-alien formation sways and bobs. Aliens peel off into diving attacks
  and fire at the player, and flagships dive with red escorts.
- Arcade scoring, including the flagship escort bonus of up to 800 points
- Attract mode with an AI demo pilot that dodges bullets and leads its shots
- Deterministic 60 Hz game logic with render interpolation, smooth on 144 Hz displays
- Keyboard and hot-pluggable gamepads, borderless fullscreen, saved high score

## Requirements

- A GPU and driver with Vulkan 1.3 (dynamic rendering, synchronization2)
- CMake 3.25+, a C++20 compiler, SDL 3.4+, Vulkan headers and loader, `glslc`
- Network access on the first configure: CMake downloads
  [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)

On Arch Linux:

```sh
sudo pacman -S cmake ninja sdl3 vulkan-headers vulkan-icd-loader shaderc vulkan-validation-layers
```

`vulkan-validation-layers` is optional, but Debug builds use it automatically when it's installed.

## Build and run

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/galaxians
```

The default build type is Debug, which enables Vulkan validation. For a release build:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

## Controls

| Action      | Keyboard                    | Gamepad              |
|-------------|-----------------------------|----------------------|
| Move        | ← → or A D                  | D-pad or left stick  |
| Fire        | Space, Z or Left Ctrl       | Any face button      |
| Start       | Enter or 1 (Fire works too) | Start                |
| Mute        | M                           |                      |
| CRT effect  | C                           |                      |
| Fullscreen  | F11 or Alt+Enter            |                      |
| Frame stats | F3                          |                      |
| Quit        | Esc                         |                      |

The high score is saved under SDL's per-user preferences folder
(`~/.local/share/galaxians-remake/galaxians/highscore.txt` on Linux).

## Tests

The game logic runs without a window or GPU, so it is tested headlessly:

```sh
ctest --test-dir build --output-on-failure
```

- `world_test` covers scoring, diving, lives, game flow, determinism and the
  demo pilot's performance.
- `synth_test` checks the generated sound effects.

To check rendering, run with synchronization validation and the frame-pacing log:

```sh
VK_LAYER_VALIDATE_SYNC=true GALAXIANS_STATS=1 ./build/galaxians
```

## Project layout

```
src/
  main.cpp        window, main loop (fixed 60 Hz ticks), hotkeys, high score file
  app/            keyboard and gamepad input, frame-pacing statistics
  gfx/            Vulkan: device, swapchain, sprite batching, starfield, scale-up and CRT pass
  game/           World (game rules), Game (attract and game states), DemoPilot, HUD, text
  audio/          sound synthesis and an SDL3 audio-stream mixer
shaders/          GLSL, compiled to SPIR-V and embedded in the executable at build time
tools/            make_atlas.py: builds assets/atlas.png and src/game/Atlas.h
                  sound_preview.cpp: writes every sound effect to WAV files
tests/            headless tests
```

To change the sprites or font, edit `tools/make_atlas.py` and run
`python tools/make_atlas.py`, then rebuild.

To listen to the sound effects outside the game, edit `src/audio/Synth.cpp`, rebuild, then:

```sh
mkdir -p sounds && ./build/sound_preview sounds && pw-play sounds/4-dive.wav
```

## Disclaimer

This is an unofficial fan remake for learning and fun. It is not affiliated with
or endorsed by Bandai Namco, which owns the original Galaxian game and trademark.
No original game code, graphics or sound is used.

## License

The code in this repository is released under the [MIT License](LICENSE).
