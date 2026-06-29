# build_web.ps1 — compile the simulator to WebAssembly and emit the GitHub Pages
# site into docs/. Requires the Emscripten SDK (emsdk) installed and activated.
#
# Usage (from anywhere):
#   powershell -ExecutionPolicy Bypass -File web\build_web.ps1
#
# Set $EMSDK below to your emsdk location if it differs.
$ErrorActionPreference = 'Stop'

$EMSDK = $env:EMSDK_DIR; if (-not $EMSDK) { $EMSDK = 'C:\Users\HP\tools\emsdk' }
$PROJ  = Split-Path -Parent $PSScriptRoot   # repo root (web/.. )

# Pull emcc/node onto PATH for this shell.
. "$EMSDK\emsdk_env.ps1" | Out-Null

New-Item -ItemType Directory -Force "$PROJ\docs" | Out-Null
$src = (Get-ChildItem "$PROJ\src\*.cpp").FullName

# USE_SDL=2 + USE_SDL_TTF=2 pull in Emscripten's SDL2/SDL2_ttf ports.
# --preload-file bakes the bundled font into the .data image so the HUD works.
& emcc @src `
  -std=c++17 -O2 `
  -sUSE_SDL=2 -sUSE_SDL_TTF=2 `
  -sALLOW_MEMORY_GROWTH=1 `
  --preload-file "$PROJ\assets@/assets" `
  --shell-file "$PROJ\web\shell.html" `
  -o "$PROJ\docs\index.html"

# Stop GitHub Pages' Jekyll from touching the emscripten output.
New-Item -ItemType File -Force "$PROJ\docs\.nojekyll" | Out-Null
Write-Host "`nWeb build complete -> docs\index.html"
