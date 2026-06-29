// Explosion.h
// ----------------------------------------------------------------------------
// Standalone blast geometry, kept out of Simulation.cpp so the detonation math
// is easy to read and tweak. An explosion has two zones:
//   * Core (dist <= R): everything non-stone is flashed to Fire.
//   * Shell (R < dist <= R+kick): loose debris (powders/liquids) is given an
//     outward velocity impulse, which Simulation::applyVelocity then integrates
//     into a satisfying spray of flying sand and water.
// ----------------------------------------------------------------------------
#pragma once

class Simulation;

namespace Explosion {
    void detonate(Simulation& sim, int cx, int cy, int radius);
}
