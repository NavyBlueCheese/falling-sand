// main.cpp
// ----------------------------------------------------------------------------
// Entry point: SDL setup (via Renderer), the fixed-timestep simulation loop,
// and FPS/SPS measurement.
//
// WHY a fixed timestep? Cellular-automaton physics is frame-count based (a grain
// falls one cell per step). If we tied steps to render time, the sim would run
// faster on fast machines and the physics would feel different. Instead we run
// the world at a constant 60 steps/sec (scaled by the user's speed setting) and
// render as fast as vsync allows, decoupling look from logic.
// ----------------------------------------------------------------------------
#include "Simulation.h"
#include "Renderer.h"
#include "UI.h"
#include <SDL.h>
#include <cstdio>

int main(int /*argc*/, char* /*argv*/[]) {
    const int WIN_W = 1280, WIN_H = 720;

    Renderer renderer;
    if (!renderer.init("Falling Sand — C++/SDL2", WIN_W, WIN_H)) {
        std::fprintf(stderr, "Renderer init failed.\n");
        return 1;
    }

    Simulation sim;
    UI ui(&sim);
    ui.onResize(WIN_W, WIN_H);

    std::printf(
        "Falling Sand controls:\n"
        "  L-drag place   R-drag erase   wheel brush size\n"
        "  1 Sand 2 Water 3 Fire 4 Wood 5 Stone 6 Lava 7 Gunpowder 8 Acid  0 Erase\n"
        "  Space pause  C/R clear  S save  L load  G grid  F fps  +/- speed  Esc quit\n");

    const double SIM_HZ = 60.0;
    const double baseStep = 1.0 / SIM_HZ;     // seconds per sim step at 1x

    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 prev = SDL_GetPerformanceCounter();
    double accumulator = 0.0;

    // Rolling stats.
    double fpsTimer = 0.0;
    int    frameCounter = 0, stepCounter = 0;
    double displayFps = 0.0, displaySps = 0.0;

    bool running = true;
    while (running) {
        // --- timing ---------------------------------------------------------
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = double(now - prev) / double(perfFreq);
        prev = now;
        if (dt > 0.25) dt = 0.25; // clamp huge stalls (e.g. window drag)

        // --- input ----------------------------------------------------------
        SDL_Event e;
        while (SDL_PollEvent(&e)) ui.handleEvent(e, running);
        ui.applyBrush();

        // --- simulation (fixed step, scaled by speed) -----------------------
        if (!ui.paused()) {
            double step = baseStep / double(ui.speed()); // smaller step => faster
            accumulator += dt;
            int steps = 0;
            // Cap steps/frame so a hitch can't trigger a spiral of death.
            while (accumulator >= step && steps < 8) {
                sim.update();
                accumulator -= step;
                ++steps; ++stepCounter;
            }
        } else {
            accumulator = 0.0;
        }

        // --- render ---------------------------------------------------------
        renderer.render(sim, ui, displayFps, displaySps);
        ++frameCounter;

        // --- stats (update ~ twice per second) ------------------------------
        fpsTimer += dt;
        if (fpsTimer >= 0.5) {
            displayFps = frameCounter / fpsTimer;
            displaySps = stepCounter / fpsTimer;
            frameCounter = stepCounter = 0;
            fpsTimer = 0.0;
        }
    }

    renderer.shutdown();
    return 0;
}
