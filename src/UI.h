// UI.h
// ----------------------------------------------------------------------------
// Input handling and user-facing state: material selection, brush, the toggles
// behind the keyboard shortcuts, and translating window-space mouse coordinates
// into simulation grid cells. The Renderer reads this state to draw the HUD.
// ----------------------------------------------------------------------------
#pragma once

#include "Materials.h"
#include <SDL.h>
#include <array>

class Simulation;

class UI {
public:
    explicit UI(Simulation* sim);

    // Process a single SDL event. Sets `running=false` on quit/Esc.
    void handleEvent(const SDL_Event& e, bool& running);
    // Per-frame: apply held mouse buttons (paint/erase) to the simulation.
    void applyBrush();

    void onResize(int w, int h) { winW_ = w; winH_ = h; }

    // ---- state queried by the renderer ------------------------------------
    MaterialType material() const { return palette_[selected_]; }
    const char*  materialName() const { return materialProps(material()).name; }
    int   brushRadius() const { return brushRadius_; }
    bool  paused() const      { return paused_; }
    bool  showGrid() const    { return showGrid_; }
    bool  showFPS() const     { return showFPS_; }
    float speed() const       { return speed_; }
    int   mouseX() const      { return mouseX_; }
    int   mouseY() const      { return mouseY_; }
    int   selectedIndex() const { return selected_; }
    static constexpr int PALETTE_SIZE = 9;
    MaterialType paletteAt(int i) const { return palette_[i]; }

    // Map a window pixel to a grid cell (accounts for window resizing).
    void windowToGrid(int wx, int wy, int& gx, int& gy) const;

private:
    Simulation* sim_;
    int   winW_ = 1280, winH_ = 720;
    int   mouseX_ = 0, mouseY_ = 0;
    bool  leftDown_ = false, rightDown_ = false;
    int   selected_ = 0;             // index into palette_
    int   brushRadius_ = 4;
    bool  paused_ = false;
    bool  showGrid_ = false;
    bool  showFPS_ = true;
    float speed_ = 1.0f;             // 0.25x .. 4x

    // Number-key palette. Index 0..8 -> keys 1..9. Key 0 erases (handled apart).
    std::array<MaterialType, PALETTE_SIZE> palette_ = {{
        MaterialType::SAND, MaterialType::WATER, MaterialType::FIRE,
        MaterialType::WOOD, MaterialType::STONE, MaterialType::LAVA,
        MaterialType::GUNPOWDER, MaterialType::ACID, MaterialType::STONE
    }};
    bool eraseMode_ = false; // set when key 0 chosen
};
