// Explosion.cpp — see Explosion.h for the two-zone model.
#include "Explosion.h"
#include "Simulation.h"
#include <cmath>
#include <algorithm>

using M = MaterialType;

namespace Explosion {

void detonate(Simulation& sim, int cx, int cy, int radius) {
    const int kick = 6;                 // shell thickness that gets thrown
    const int maxR = radius + kick;
    const float impulse = 28.0f;        // peak knockback speed at the core edge

    for (int dy = -maxR; dy <= maxR; ++dy) {
        for (int dx = -maxR; dx <= maxR; ++dx) {
            int x = cx + dx, y = cy + dy;
            if (!sim.inBounds(x, y)) continue;

            float dist = std::sqrt(float(dx * dx + dy * dy));
            if (dist > maxR) continue;

            Cell& cell = sim.at(x, y);

            if (dist <= radius) {
                // Core: stone shrugs it off; everything else becomes fire. Air
                // included, for the bright initial flash that quickly burns out.
                if (cell.material != M::STONE) {
                    sim.spawnCell(x, y, M::FIRE);
                    // Short, hot lifetime so the fireball dissipates fast.
                    sim.at(x, y).lifetime = uint8_t(20 + sim.rng() % 25);
                }
            } else {
                // Shell: only loose materials get flung; solids stay put.
                MoveClass mv = materialProps(cell.material).move;
                bool loose = (mv == MoveClass::POWDER || mv == MoveClass::LIQUID);
                if (!loose) continue;

                // Velocity falls off linearly from the core edge to the shell rim.
                float falloff = 1.0f - (dist - radius) / float(kick);
                float mag = impulse * std::max(0.0f, falloff);
                float inv = (dist > 0.001f) ? (1.0f / dist) : 0.0f;
                int vx = int(std::lround(dx * inv * mag));
                int vy = int(std::lround(dy * inv * mag));
                cell.velX = int8_t(std::clamp(vx, -127, 127));
                cell.velY = int8_t(std::clamp(vy, -127, 127));
                sim.chunks().wake(x, y);
            }
        }
    }
    // Make sure the whole blast region simulates next frame.
    sim.chunks().wake(cx, cy);
}

} // namespace Explosion
