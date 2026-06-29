// Renderer.h
// ----------------------------------------------------------------------------
// Owns the SDL window/renderer, the CPU-side ARGB pixel buffer, the streaming
// GPU texture it's uploaded to each frame, and all HUD drawing (SDL2_ttf text +
// the material palette + brush cursor).
//
// Pipeline per frame:
//   simulation grid -> per-cell color (with animation) -> uint32 pixel buffer
//   -> SDL_UpdateTexture -> SDL_RenderCopy (stretched to the window)
//   -> HUD drawn on top in window space.
// ----------------------------------------------------------------------------
#pragma once

#include "Simulation.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdint>
#include <string>

class UI;

class Renderer {
public:
    bool init(const char* title, int winW, int winH);
    void shutdown();

    // Draw one full frame. `fps`/`sps` are display stats (frames & sim-steps/sec).
    void render(const Simulation& sim, const UI& ui, double fps, double sps);

    SDL_Window* window() const { return window_; }

private:
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  texture_  = nullptr;   // streaming, Simulation::WIDTH x HEIGHT
    TTF_Font*     font_     = nullptr;

    // CPU pixel buffer, one uint32 (ARGB8888) per grid cell.
    uint32_t pixels_[Simulation::WIDTH * Simulation::HEIGHT];

    bool loadFont();
    void buildPixelBuffer(const Simulation& sim);
    void drawHUD(const Simulation& sim, const UI& ui, double fps, double sps);
    void drawGridOverlay(int winW, int winH);
    void drawBrushCursor(const UI& ui, int winW, int winH);
    void drawText(int x, int y, const std::string& s, SDL_Color color);
    void textSize(const std::string& s, int& w, int& h);
};
