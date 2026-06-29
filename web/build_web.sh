#!/usr/bin/env bash
# build_web.sh — compile the simulator to WebAssembly and emit the GitHub Pages
# site into docs/. Requires the Emscripten SDK (emcc on PATH; run via emsdk's
# `source emsdk_env.sh` first, or use a system emscripten).
#
# Usage (from repo root):
#   ./web/build_web.sh
set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$PROJ/docs"

# USE_SDL=2 + USE_SDL_TTF=2 pull in Emscripten's SDL2/SDL2_ttf ports.
# --preload-file bakes the bundled font into the .data image so the HUD works.
emcc "$PROJ"/src/*.cpp \
  -std=c++17 -O2 \
  -sUSE_SDL=2 -sUSE_SDL_TTF=2 \
  -sALLOW_MEMORY_GROWTH=1 \
  --preload-file "$PROJ/assets@/assets" \
  --shell-file "$PROJ/web/shell.html" \
  -o "$PROJ/docs/index.html"

# Stop GitHub Pages' Jekyll from touching the emscripten output.
touch "$PROJ/docs/.nojekyll"
echo "Web build complete -> docs/index.html"
