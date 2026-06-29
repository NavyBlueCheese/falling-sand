// UI.cpp — input handling. See UI.h.
#include "UI.h"
#include "Simulation.h"
#include "SaveLoad.h"
#include <algorithm>
#include <cstdio>

UI::UI(Simulation* sim) : sim_(sim) {}

void UI::windowToGrid(int wx, int wy, int& gx, int& gy) const {
    // The simulation texture is stretched to fill the whole window, so mapping
    // is a straight proportional scale from window pixels to grid cells.
    gx = int(int64_t(wx) * Simulation::WIDTH  / std::max(1, winW_));
    gy = int(int64_t(wy) * Simulation::HEIGHT / std::max(1, winH_));
}

void UI::handleEvent(const SDL_Event& e, bool& running) {
    switch (e.type) {
        case SDL_QUIT:
            running = false;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT)  leftDown_  = true;
            if (e.button.button == SDL_BUTTON_RIGHT) rightDown_ = true;
            mouseX_ = e.button.x; mouseY_ = e.button.y;
            break;

        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT)  leftDown_  = false;
            if (e.button.button == SDL_BUTTON_RIGHT) rightDown_ = false;
            break;

        case SDL_MOUSEMOTION:
            mouseX_ = e.motion.x; mouseY_ = e.motion.y;
            break;

        case SDL_MOUSEWHEEL:
            // Scroll changes brush radius (1..30).
            brushRadius_ = std::clamp(brushRadius_ + (e.wheel.y > 0 ? 1 : -1), 1, 30);
            break;

        case SDL_KEYDOWN: {
            SDL_Keycode k = e.key.keysym.sym;
            // Material selection 1..9.
            if (k >= SDLK_1 && k <= SDLK_9) {
                selected_ = int(k - SDLK_1);
                eraseMode_ = false;
            } else if (k == SDLK_0) {
                eraseMode_ = true; // erase brush
            }
            switch (k) {
                case SDLK_SPACE: paused_ = !paused_; break;
                case SDLK_c:     sim_->clear();      break;
                case SDLK_r:     sim_->clear();      break;
                case SDLK_g:     showGrid_ = !showGrid_; break;
                case SDLK_f:     showFPS_  = !showFPS_;  break;
                case SDLK_s:
                    std::printf("[save] %s\n",
                        SaveLoad::save(*sim_, "save.bin") ? "ok" : "FAILED");
                    break;
                case SDLK_l:
                    std::printf("[load] %s\n",
                        SaveLoad::load(*sim_, "save.bin") ? "ok" : "FAILED");
                    break;
                case SDLK_EQUALS:   // '=' / '+'
                case SDLK_PLUS:
                case SDLK_KP_PLUS:
                    speed_ = std::min(4.0f, speed_ * 2.0f); break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    speed_ = std::max(0.25f, speed_ * 0.5f); break;
                case SDLK_ESCAPE:   running = false; break;
                default: break;
            }
            break;
        }

        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                e.window.event == SDL_WINDOWEVENT_RESIZED) {
                onResize(e.window.data1, e.window.data2);
            }
            break;

        default: break;
    }
}

void UI::applyBrush() {
    if (!leftDown_ && !rightDown_) return;
    int gx, gy;
    windowToGrid(mouseX_, mouseY_, gx, gy);
    if (!sim_->inBounds(gx, gy)) return;

    if (rightDown_) {
        sim_->paintCircle(gx, gy, brushRadius_, MaterialType::AIR);
    } else if (leftDown_) {
        MaterialType m = eraseMode_ ? MaterialType::AIR : material();
        sim_->paintCircle(gx, gy, brushRadius_, m);
    }
}
