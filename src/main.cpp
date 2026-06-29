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
//
// PORTABILITY: native builds spin a normal while-loop; web (Emscripten/WASM)
// builds can't block the browser, so we hand one frame at a time to
// emscripten_set_main_loop_arg. The per-frame work is identical either way and
// lives in frameStep().
// ----------------------------------------------------------------------------
#include "Simulation.h"
#include "Renderer.h"
#include "UI.h"
#include <SDL.h>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Everything one frame needs, so the loop body works as a standalone callback.
struct App {
    Renderer*  renderer = nullptr;
    Simulation* sim     = nullptr;
    UI*        ui       = nullptr;

    double baseStep   = 1.0 / 60.0;   // seconds per sim step at 1x speed
    Uint64 perfFreq   = 1;
    Uint64 prev       = 0;
    double accumulator = 0.0;

    // Rolling stats.
    double fpsTimer = 0.0;
    int    frameCounter = 0, stepCounter = 0;
    double displayFps = 0.0, displaySps = 0.0;

    bool running = true;
};

// One iteration of the loop: input -> fixed-step sim -> render -> stats.
static void frameStep(void* arg) {
    App* a = static_cast<App*>(arg);

    // --- timing -------------------------------------------------------------
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = double(now - a->prev) / double(a->perfFreq);
    a->prev = now;
    if (dt > 0.25) dt = 0.25; // clamp huge stalls (window drag, tab switch)

    // --- input --------------------------------------------------------------
    SDL_Event e;
    while (SDL_PollEvent(&e)) a->ui->handleEvent(e, a->running);
    a->ui->applyBrush();

    // --- simulation (fixed step, scaled by speed) ---------------------------
    if (!a->ui->paused()) {
        double step = a->baseStep / double(a->ui->speed()); // smaller => faster
        a->accumulator += dt;
        int steps = 0;
        // Cap steps/frame so a hitch can't trigger a spiral of death.
        while (a->accumulator >= step && steps < 8) {
            a->sim->update();
            a->accumulator -= step;
            ++steps; ++a->stepCounter;
        }
    } else {
        a->accumulator = 0.0;
    }

    // --- render -------------------------------------------------------------
    a->renderer->render(*a->sim, *a->ui, a->displayFps, a->displaySps);
    ++a->frameCounter;

    // --- stats (refresh ~twice per second) ----------------------------------
    a->fpsTimer += dt;
    if (a->fpsTimer >= 0.5) {
        a->displayFps = a->frameCounter / a->fpsTimer;
        a->displaySps = a->stepCounter / a->fpsTimer;
        a->frameCounter = a->stepCounter = 0;
        a->fpsTimer = 0.0;
    }

#ifdef __EMSCRIPTEN__
    if (!a->running) emscripten_cancel_main_loop();
#endif
}

int main(int /*argc*/, char* /*argv*/[]) {
    const int WIN_W = 1280, WIN_H = 720;

    // IMPORTANT: heap-allocate everything the loop touches. On the web,
    // emscripten_set_main_loop unwinds main()'s stack frame (it never really
    // "returns"), which would destruct any locals while the frame callback is
    // still using them. Putting these on the heap keeps them alive for the life
    // of the program. (On native we clean them up after the loop.)
    Renderer* renderer = new Renderer();
    if (!renderer->init("Falling Sand — C++/SDL2", WIN_W, WIN_H)) {
        std::fprintf(stderr, "Renderer init failed.\n");
        return 1;
    }

    Simulation* sim = new Simulation();
    UI*  ui  = new UI(sim);
    ui->onResize(WIN_W, WIN_H);

    std::printf(
        "Falling Sand controls:\n"
        "  L-drag place   R-drag erase   wheel brush size\n"
        "  1 Sand 2 Water 3 Fire 4 Wood 5 Stone 6 Lava 7 Gunpowder 8 Acid  0 Erase\n"
        "  Space pause  C/R clear  S save  L load  G grid  F fps  +/- speed  Esc quit\n");

    App* app = new App();
    app->renderer = renderer;
    app->sim      = sim;
    app->ui       = ui;
    app->perfFreq = SDL_GetPerformanceFrequency();
    app->prev     = SDL_GetPerformanceCounter();

#ifdef __EMSCRIPTEN__
    // 0 fps => drive from requestAnimationFrame (matches the display refresh);
    // simulate_infinite_loop=1 => the call below never returns to main().
    emscripten_set_main_loop_arg(frameStep, app, 0, 1);
#else
    while (app->running) frameStep(app);
    renderer->shutdown();
    delete app; delete ui; delete sim; delete renderer;
#endif
    return 0;
}
