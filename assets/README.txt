Drop a monospace TrueType font here named `font.ttf` for the nicest HUD.

A good free choice: JetBrains Mono (https://www.jetbrains.com/lp/mono/) — download
the .ttf and rename it to font.ttf, or symlink it.

If no font.ttf is present, the program automatically falls back to a common
system monospace font (Consolas on Windows, Menlo on macOS, DejaVu Sans Mono on
Linux). If none of those exist either, the simulation still runs perfectly — the
HUD just renders its colored palette boxes without text labels.
