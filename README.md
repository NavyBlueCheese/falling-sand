# Falling Sand 🟡🔵🔥

A real-time, pixel-based falling-sand & fluid sandbox in C++ with SDL2, think
*The Powder Toy* / *Noita*, with clean code you can actually read and extend.
Pour water on sand, set wood on fire, drop gunpowder next to lava and watch it
go. A 320×180 cellular grid simulated at a fixed 60 steps/second, rendered to a
resizable 1280×720 window.

[**live site →**](https://navybluecheese.github.io/falling-sand/)

(the same C++ is compiled to WebAssembly and runs entirely
client-side.)

---

## Features

- **10 materials**, each with distinct physics & animated visuals:
  Sand, Water, Fire, Smoke, Wood, Stone, Lava, Gunpowder, Steam, Acid.
- **Rich interactions**: water+lava → stone & steam, fire spreads through wood,
  gunpowder explodes (with real outward knockback that flings sand & water),
  acid dissolves things, water evaporates near lava.
- **Performance**: dirty-chunk system so settled regions cost nothing; runs the
  full grid at 60 FPS on a laptop. No heap allocation in the simulation loop.
- **Tactile UI**: scalable circular brush, material palette HUD, pause, save/load,
  grid overlay, FPS readout, adjustable sim speed.

---

## Controls

| Input | Action |
|-------|--------|
| **Left-click + drag** | Place the selected material |
| **Right-click + drag** | Erase (place Air) |
| **Scroll wheel** | Brush size (1–30) |
| `1`–`8` | Select material (1 Sand, 2 Water, 3 Fire, 4 Wood, 5 Stone, 6 Lava, 7 Gunpowder, 8 Acid) |
| `0` | Erase brush |
| `Space` | Pause / unpause |
| `C` / `R` | Clear the grid |
| `S` / `L` | Save / load `save.bin` |
| `G` | Toggle grid overlay |
| `F` | Toggle FPS display |
| `+` / `-` | Sim speed 0.25×–4× |
| `Esc` | Quit |

---

## Building

Requires **CMake ≥ 3.16**, a **C++17** compiler, **SDL2**, and **SDL2_ttf**.

### Linux
```bash
sudo apt install build-essential cmake libsdl2-dev libsdl2-ttf-dev   # Debian/Ubuntu
# or: sudo dnf install cmake gcc-c++ SDL2-devel SDL2_ttf-devel        # Fedora
cmake -B build && cmake --build build -j
./build/sand_sim
```

### macOS
```bash
brew install cmake sdl2 sdl2_ttf
cmake -B build && cmake --build build -j
./build/sand_sim
```

### Windows (vcpkg + Visual Studio)
```powershell
vcpkg install sdl2 sdl2-ttf
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\sand_sim.exe
```

### Windows (MSYS2 / MinGW)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf
cmake -B build -G "MinGW Makefiles" && cmake --build build -j
./build/sand_sim.exe
```

> **Font:** the HUD uses the bundled `assets/font.ttf` (JetBrains Mono). If it's
> missing the program falls back to a system monospace font automatically.

### Web (WebAssembly via Emscripten)

The browser build at the link above is produced from the *same* source, the
main loop is wrapped in `emscripten_set_main_loop` (see `src/main.cpp`) and SDL2 /
SDL2_ttf come from Emscripten's built-in ports. The compiled site lives in
`docs/` and is served by GitHub Pages.

```bash
# install + activate the Emscripten SDK (https://emscripten.org), then:
./web/build_web.sh                 # macOS/Linux
# or on Windows:
powershell -ExecutionPolicy Bypass -File web\build_web.ps1
# output: docs/index.{html,js,wasm,data}
```

---

## How it works (architecture)

```
src/
  main.cpp        entry point, fixed-timestep loop, FPS/SPS stats
  Materials.h     MaterialType enum + property/color table + color helpers
  Cell.h          the 8-byte POD grid cell
  Simulation.*    the grid, update order, and every material's physics rules
  Chunk.*         dirty-region (active chunk) bookkeeping
  Explosion.*     blast geometry + knockback impulses
  Renderer.*      ARGB pixel buffer, animated colors, SDL texture, HUD
  UI.*            input handling, brush, palette, keyboard shortcuts
  SaveLoad.*      binary snapshot format
```

A few design notes worth calling out (also commented inline):

- **Update order is bottom-to-top.** Gravity moves particles down into rows
  we've already processed, so a falling grain is never simulated twice in one
  step.
- **Horizontal scan direction alternates each frame.** A fixed scan direction
  makes piles lean and fluids drift one way; flipping it every frame cancels the
  bias.
- **One move per cell per step** via a `FLAG_UPDATED` bit, reset each frame.
- **Dirty chunks**: a 16×16 block is only simulated if something in it (or a
  neighbour) recently changed; that's what keeps mostly-settled scenes cheap.
- **Fixed 60 Hz simulation, free render rate**: physics is frame-count based, so
  decoupling it from the render rate keeps behavior identical across machines.

## Save format

```
[4]  "SAND"
[4]  width  (uint32)
[4]  height (uint32)
[w*h * sizeof(Cell)]  raw cells
```

## License

MIT — do whatever you like with it.
