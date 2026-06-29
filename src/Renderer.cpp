// Renderer.cpp — pixel buffer construction, material color animation, and HUD.
#include "Renderer.h"
#include "UI.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using M = MaterialType;

// ----------------------------------------------------------------------------
// Small per-(x,y,frame) hash for cheap, stable-per-frame noise used by animated
// materials (fire flicker, lava glow, water shimmer). No allocations, no std::rand.
// ----------------------------------------------------------------------------
static inline uint32_t hash3(int x, int y, uint64_t f) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u
               + uint32_t(f) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Multiply an ARGB color's RGB channels by num/den (used to fade smoke/steam).
static inline uint32_t scaleRGB(uint32_t argb, int num, int den) {
    if (den <= 0) den = 1;
    uint8_t a = (argb >> 24) & 0xFF;
    int r = ((argb >> 16) & 0xFF) * num / den;
    int g = ((argb >> 8)  & 0xFF) * num / den;
    int b = ( argb        & 0xFF) * num / den;
    return makeARGB(a, clamp8(r), clamp8(g), clamp8(b));
}

// ----------------------------------------------------------------------------
// The color of a single cell, including per-particle jitter and time-based
// animation for the "alive" materials.
// ----------------------------------------------------------------------------
static uint32_t cellColor(const Cell& c, int x, int y, uint64_t frame) {
    const MaterialProps& p = materialProps(c.material);
    int range = p.variantRange;
    int jitter = range ? (int(c.variant % (2 * range + 1)) - range) : 0;
    uint32_t base = p.baseColor;

    switch (c.material) {
        case M::FIRE: {
            // Age ramp: hot+young (high lifetime) is yellow-white; old is dark red.
            // lifetime spans ~0..120; normalize to t in [0,1].
            int life = c.lifetime;
            float t = std::min(1.0f, life / 110.0f);
            uint32_t darkRed = makeARGB(255, 0x80, 0x10, 0x00);
            uint32_t orange  = makeARGB(255, 0xFF, 0x8C, 0x00);
            uint32_t yellow  = makeARGB(255, 0xFF, 0xE0, 0x60);
            uint32_t col = (t > 0.5f)
                ? lerpColor(orange, yellow, int((t - 0.5f) * 2 * 255))
                : lerpColor(darkRed, orange, int(t * 2 * 255));
            int flick = int(hash3(x, y, frame) % 31) - 15; // lively flicker
            return shadeColor(col, flick);
        }
        case M::LAVA: {
            // Molten texture: random bright spots each frame plus a slow pulse.
            uint32_t hsh = hash3(x, y, frame);
            int glow = (hsh & 7) == 0 ? 50 : (int(hsh % 25));
            int pulse = int(20 * std::sin((frame + c.variant) * 0.15));
            return shadeColor(base, jitter + glow + pulse);
        }
        case M::WATER: {
            int shimmer = int(hash3(x, y, frame / 2) % 11) - 5; // gentle ripple
            return shadeColor(base, jitter + shimmer);
        }
        case M::ACID: {
            int bubble = int(hash3(x, y, frame) % 25) - 8; // toxic fizz
            return shadeColor(base, jitter + bubble);
        }
        case M::SMOKE: {
            // Fade toward black as it dies (we render over a black background, so
            // darkening reads as transparency without needing real alpha blend).
            int s = std::min(60, int(c.lifetime));
            return scaleRGB(shadeColor(base, jitter), s, 60);
        }
        case M::STEAM: {
            int s = std::min(40, int(c.lifetime));
            return scaleRGB(shadeColor(base, jitter), s, 40);
        }
        default:
            // Static/granular materials: fixed per-particle shade only.
            return shadeColor(base, jitter);
    }
}

// ----------------------------------------------------------------------------
// Init / shutdown
// ----------------------------------------------------------------------------
bool Renderer::init(const char* title, int winW, int winH) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        // Non-fatal: we can run without text.
    }

    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, winW, winH,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window_) { std::fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); return false; }

    renderer_ = SDL_CreateRenderer(window_, -1,
                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) { std::fprintf(stderr, "CreateRenderer: %s\n", SDL_GetError()); return false; }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                    SDL_TEXTUREACCESS_STREAMING,
                    Simulation::WIDTH, Simulation::HEIGHT);
    if (!texture_) { std::fprintf(stderr, "CreateTexture: %s\n", SDL_GetError()); return false; }

    loadFont();
    return true;
}

void Renderer::shutdown() {
    if (font_)     TTF_CloseFont(font_);
    if (texture_)  SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
    TTF_Quit();
    SDL_Quit();
}

bool Renderer::loadFont() {
    // Try the bundled font first, then a few common system monospace fonts so
    // the HUD still works even if assets/font.ttf wasn't provided.
    const char* candidates[] = {
        "assets/font.ttf", "font.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/Library/Fonts/Andale Mono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    };
    for (const char* path : candidates) {
        font_ = TTF_OpenFont(path, 16);
        if (font_) return true;
    }
    std::fprintf(stderr, "[font] none found; HUD text disabled "
                         "(drop a TTF at assets/font.ttf)\n");
    return false;
}

// ----------------------------------------------------------------------------
// Pixel buffer
// ----------------------------------------------------------------------------
void Renderer::buildPixelBuffer(const Simulation& sim) {
    const uint64_t frame = sim.frameCount();
    for (int y = 0; y < Simulation::HEIGHT; ++y) {
        for (int x = 0; x < Simulation::WIDTH; ++x) {
            const Cell& c = sim.at(x, y);
            uint32_t color;
            if (c.material == M::AIR) {
                color = makeARGB(255, 0, 0, 0); // pure black background
            } else {
                color = cellColor(c, x, y, frame);
                // Depth cue: if the cell below is empty air, darken this cell a
                // touch so the bottom edges of piles read as having form. Skip
                // for gases/fire where it would look wrong.
                MoveClass mv = materialProps(c.material).move;
                if (mv != MoveClass::GAS && c.material != M::FIRE &&
                    y + 1 < Simulation::HEIGHT && sim.at(x, y + 1).isAir()) {
                    color = shadeColor(color, -28);
                }
            }
            pixels_[y * Simulation::WIDTH + x] = color;
        }
    }
}

// ----------------------------------------------------------------------------
// Frame
// ----------------------------------------------------------------------------
void Renderer::render(const Simulation& sim, const UI& ui, double fps, double sps) {
    buildPixelBuffer(sim);
    SDL_UpdateTexture(texture_, nullptr, pixels_,
                      Simulation::WIDTH * int(sizeof(uint32_t)));

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    // Stretch the 320x180 sim to fill the window (NULL dst = whole target).
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

    int winW, winH;
    SDL_GetRendererOutputSize(renderer_, &winW, &winH);

    if (ui.showGrid()) drawGridOverlay(winW, winH);
    drawBrushCursor(ui, winW, winH);
    drawHUD(sim, ui, fps, sps);

    SDL_RenderPresent(renderer_);
}

// ----------------------------------------------------------------------------
// Text helpers
// ----------------------------------------------------------------------------
void Renderer::textSize(const std::string& s, int& w, int& h) {
    w = h = 0;
    if (font_) TTF_SizeUTF8(font_, s.c_str(), &w, &h);
}

void Renderer::drawText(int x, int y, const std::string& s, SDL_Color color) {
    if (!font_ || s.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, s.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_Rect dst{ x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if (tex) {
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
}

// ----------------------------------------------------------------------------
// HUD: stats (top-left), material+brush (top-right), palette bar (bottom).
// ----------------------------------------------------------------------------
void Renderer::drawHUD(const Simulation& sim, const UI& ui, double fps, double sps) {
    const SDL_Color white{ 230, 230, 230, 255 };
    const SDL_Color dim  { 150, 150, 150, 255 };
    char buf[128];

    // Top-left stats.
    int ty = 8;
    if (ui.showFPS()) {
        std::snprintf(buf, sizeof(buf), "FPS %.0f", fps);
        drawText(8, ty, buf, white); ty += 18;
        std::snprintf(buf, sizeof(buf), "SPS %.0f", sps);
        drawText(8, ty, buf, dim); ty += 18;
    }
    std::snprintf(buf, sizeof(buf), "active %d", sim.activeCellCount());
    drawText(8, ty, buf, dim); ty += 18;
    std::snprintf(buf, sizeof(buf), "chunks %d", sim.activeChunkCount());
    drawText(8, ty, buf, dim); ty += 18;
    if (ui.paused()) drawText(8, ty, "PAUSED", SDL_Color{ 255, 200, 80, 255 });

    int winW, winH;
    SDL_GetRendererOutputSize(renderer_, &winW, &winH);

    // Top-right: current material + brush size + sim speed.
    std::snprintf(buf, sizeof(buf), "%s", ui.materialName());
    int tw, th; textSize(buf, tw, th);
    drawText(winW - tw - 10, 8, buf, SDL_Color{ 255, 255, 255, 255 });
    std::snprintf(buf, sizeof(buf), "brush %d   speed %.2fx", ui.brushRadius(), ui.speed());
    textSize(buf, tw, th);
    drawText(winW - tw - 10, 28, buf, dim);

    // Bottom palette bar.
    const int n = UI::PALETTE_SIZE + 1; // +1 for the erase slot
    const int box = 34, gap = 8;
    int totalW = n * box + (n - 1) * gap;
    int startX = (winW - totalW) / 2;
    int by = winH - box - 10;

    for (int i = 0; i < n; ++i) {
        int bx = startX + i * (box + gap);
        bool isErase = (i == UI::PALETTE_SIZE);
        bool selected = isErase ? false : (i == ui.selectedIndex());

        SDL_Rect r{ bx, by, box, box };
        if (isErase) {
            SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 255);
            SDL_RenderFillRect(renderer_, &r);
            SDL_SetRenderDrawColor(renderer_, 120, 120, 120, 255);
            SDL_RenderDrawRect(renderer_, &r);
            drawText(bx + 4, by + 8, "ERS", dim);
        } else {
            uint32_t col = materialProps(ui.paletteAt(i)).baseColor;
            SDL_SetRenderDrawColor(renderer_, (col >> 16) & 0xFF,
                (col >> 8) & 0xFF, col & 0xFF, 255);
            SDL_RenderFillRect(renderer_, &r);
        }
        // Highlight the selected slot with a bright, slightly larger border.
        if (selected) {
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_Rect hl{ bx - 3, by - 3, box + 6, box + 6 };
            SDL_RenderDrawRect(renderer_, &hl);
            SDL_RenderDrawRect(renderer_, &r);
        }
        // Number key label.
        char key[4];
        std::snprintf(key, sizeof(key), "%d", isErase ? 0 : i + 1);
        drawText(bx + 2, by + box - 16, key, SDL_Color{ 0, 0, 0, 200 });
    }
}

void Renderer::drawGridOverlay(int winW, int winH) {
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 30); // faint
    float sx = winW / float(Simulation::WIDTH);
    float sy = winH / float(Simulation::HEIGHT);
    for (int x = 0; x <= Simulation::WIDTH; x += 1) {
        int px = int(x * sx);
        SDL_RenderDrawLine(renderer_, px, 0, px, winH);
    }
    for (int y = 0; y <= Simulation::HEIGHT; y += 1) {
        int py = int(y * sy);
        SDL_RenderDrawLine(renderer_, 0, py, winW, py);
    }
}

void Renderer::drawBrushCursor(const UI& ui, int winW, int winH) {
    // Brush radius is in grid cells; convert to window pixels for the ring.
    float sx = winW / float(Simulation::WIDTH);
    float rpx = ui.brushRadius() * sx;
    int cx = ui.mouseX(), cy = ui.mouseY();
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 160);
    const int segs = 48;
    int prevX = cx + int(rpx), prevY = cy;
    for (int i = 1; i <= segs; ++i) {
        float a = (i / float(segs)) * 2.0f * 3.14159265f;
        int nx = cx + int(std::cos(a) * rpx);
        int ny = cy + int(std::sin(a) * rpx);
        SDL_RenderDrawLine(renderer_, prevX, prevY, nx, ny);
        prevX = nx; prevY = ny;
    }
}
