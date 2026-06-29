// Simulation.cpp
// ----------------------------------------------------------------------------
// All material behaviour lives here. Read Simulation.h first for the high-level
// design notes. Inline comments below explain the *why* of each rule.
// ----------------------------------------------------------------------------
#include "Simulation.h"
#include "Explosion.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <utility>

using M = MaterialType;

Simulation::Simulation()
    : grid_(size_t(WIDTH) * HEIGHT), chunks_(WIDTH, HEIGHT) {
    // Seed the RNG from the clock so each run looks a little different.
    rngState_ = uint32_t(std::time(nullptr)) | 1u;
}

// ----------------------------------------------------------------------------
// Editing helpers
// ----------------------------------------------------------------------------
void Simulation::clear() {
    std::fill(grid_.begin(), grid_.end(), Cell{});
    chunks_.wakeAll();
}

void Simulation::setCell(int x, int y, M m, uint8_t lifetime) {
    Cell& c = at(x, y);
    c.material = m;
    c.lifetime = lifetime;
    c.flags    = 0;
    c.velX = c.velY = 0;
    c.variant = uint8_t(rng()); // fresh jitter seed
    chunks_.wake(x, y);
}

void Simulation::spawnCell(int x, int y, M m) {
    if (!inBounds(x, y)) return;
    // Give transient materials a randomized starting lifetime.
    uint8_t life = 0;
    switch (m) {
        case M::FIRE:  life = uint8_t(40 + rng() % 80);  break; // 40..119
        case M::SMOKE: life = uint8_t(80 + rng() % 120); break; // 80..199
        case M::STEAM: life = uint8_t(60 + rng() % 40);  break; // 60..99
        default: break;
    }
    setCell(x, y, m, life);
}

void Simulation::paintCircle(int cx, int cy, int radius, M m) {
    // Erasing or solid placement is a filled disc. For fluids/powders we leave
    // gaps (scatter) so the player doesn't dump a perfect solid block of sand.
    bool scatter = (m == M::SAND || m == M::WATER || m == M::GUNPOWDER ||
                    m == M::ACID);
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > r2) continue;
            int x = cx + dx, y = cy + dy;
            if (!inBounds(x, y)) continue;
            if (scatter && (rng() & 3) == 0) continue; // ~25% gaps
            spawnCell(x, y, m);
        }
    }
}

// ----------------------------------------------------------------------------
// Movement primitives
// ----------------------------------------------------------------------------
// A cell of material `src` can move into (nx,ny) if that cell is air, or holds
// a lighter mobile material it can sink through (sand through water, etc.).
bool Simulation::canDisplace(M src, int nx, int ny) const {
    if (!inBounds(nx, ny)) return false;
    const Cell& dst = at(nx, ny);
    if (dst.material == M::AIR) return true;
    // Don't fight a cell that already moved this frame — prevents jitter/dupes.
    if (dst.isUpdated()) return false;
    MoveClass dm = materialProps(dst.material).move;
    if (dm == MoveClass::LIQUID || dm == MoveClass::GAS) {
        return density(src) > density(dst.material);
    }
    return false;
}

// Move (or swap) the cell at (x,y) into (nx,ny). Both cells get woken and the
// mover is flagged updated so it can't move again this frame.
void Simulation::moveCell(int x, int y, int nx, int ny) {
    Cell& a = at(x, y);
    Cell& b = at(nx, ny);
    std::swap(a, b);
    b.markUpdated();        // b now holds the material that just moved
    chunks_.wake(x, y);
    chunks_.wake(nx, ny);
}

bool Simulation::hasNeighbor(int x, int y, M m) const {
    static const int ox[4] = { -1, 1, 0, 0 };
    static const int oy[4] = { 0, 0, -1, 1 };
    for (int i = 0; i < 4; ++i) {
        int nx = x + ox[i], ny = y + oy[i];
        if (inBounds(nx, ny) && at(nx, ny).material == m) return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// Main step
// ----------------------------------------------------------------------------
void Simulation::update() {
    ++frame_;

    // 1) Clear the per-cell "updated" flag. A flat sweep is cheap (~58k bytes)
    //    and far simpler than a parity scheme; the real cost is in step 2.
    for (Cell& c : grid_) c.clearUpdated();

    // 2) Bottom-to-top is essential: gravity-driven materials move *down*, into
    //    rows we've already finished, so they're never revisited this frame.
    //    Each frame we flip the horizontal scan direction; an automaton that
    //    always scans the same way builds a subtle left/right lean in its piles.
    bool leftToRight = (frame_ & 1) == 0;

    activeCells_ = 0;
    for (int y = HEIGHT - 1; y >= 0; --y) {
        int xStart = leftToRight ? 0 : WIDTH - 1;
        int xEnd   = leftToRight ? WIDTH : -1;
        int xStep  = leftToRight ? 1 : -1;
        for (int x = xStart; x != xEnd; x += xStep) {
            Cell& c = at(x, y);
            if (c.material == M::AIR) continue;
            if (c.isUpdated()) continue;
            // Skip cells in settled (inactive) chunks — the performance win.
            if (!chunks_.isActiveAt(x, y)) continue;

            ++activeCells_;

            // Explosion knockback overrides normal physics while it lasts.
            if (c.velX != 0 || c.velY != 0) { applyVelocity(x, y); continue; }

            switch (c.material) {
                case M::SAND:      updatePowder(x, y);    break;
                case M::GUNPOWDER: updateGunpowder(x, y); break;
                case M::WATER:     updateWater(x, y);     break;
                case M::ACID:      updateAcid(x, y);      break;
                case M::LAVA:      updateLava(x, y);      break;
                case M::FIRE:      updateFire(x, y);      break;
                case M::SMOKE:
                case M::STEAM:     updateGas(x, y);       break;
                case M::WOOD:      updateWood(x, y);      break;
                case M::STONE:     /* inert */            break;
                default: break;
            }
        }
    }

    // 3) Promote this frame's wake requests to next frame's active set.
    chunks_.commit();
}

// ----------------------------------------------------------------------------
// POWDERS — sand & (movement part of) gunpowder
// ----------------------------------------------------------------------------
void Simulation::updatePowder(int x, int y) {
    M m = at(x, y).material;
    // Straight down first.
    if (canDisplace(m, x, y + 1)) { moveCell(x, y, x, y + 1); return; }
    // Then diagonally. Randomize which diagonal we try first each time so piles
    // don't consistently slump toward one side.
    int dir = (rng() & 1) ? 1 : -1;
    if (canDisplace(m, x + dir, y + 1)) { moveCell(x, y, x + dir, y + 1); return; }
    if (canDisplace(m, x - dir, y + 1)) { moveCell(x, y, x - dir, y + 1); return; }
}

void Simulation::updateGunpowder(int x, int y) {
    // Ignition check first: very flammable (80%) next to fire or lava.
    if (hasNeighbor(x, y, M::FIRE) || hasNeighbor(x, y, M::LAVA)) {
        if (chance(80)) { explode(x, y, 8); return; }
    }
    updatePowder(x, y); // otherwise behaves like sand
}

// ----------------------------------------------------------------------------
// LIQUIDS — generic spreading used by water/acid; lava reuses it slowed down.
// ----------------------------------------------------------------------------
// Pressure model: deeper water flows further sideways. We approximate "depth"
// by how many same-liquid cells are stacked directly above us (cheap, capped).
int Simulation::liquidReach(int x, int y) const {
    M m = at(x, y).material;
    int depth = 0;
    for (int ny = y - 1; ny >= 0 && depth < 5; --ny) {
        if (at(x, ny).material != m) break;
        ++depth;
    }
    return std::min(5, 2 + depth); // shallow:2  ... deep:5
}

void Simulation::updateLiquid(int x, int y) {
    M m = at(x, y).material;
    // Fall straight down.
    if (canDisplace(m, x, y + 1)) { moveCell(x, y, x, y + 1); return; }
    // Diagonal down (lets a column drain into a pool).
    int dir = (rng() & 1) ? 1 : -1;
    if (canDisplace(m, x + dir, y + 1)) { moveCell(x, y, x + dir, y + 1); return; }
    if (canDisplace(m, x - dir, y + 1)) { moveCell(x, y, x - dir, y + 1); return; }

    // Blocked below: spread horizontally. Walk up to `reach` cells in the chosen
    // direction through air and hop to the furthest reachable cell — this makes
    // pools level out quickly instead of crawling one cell per frame.
    int reach = liquidReach(x, y);
    int firstDir = (rng() & 1) ? 1 : -1;
    for (int s = 0; s < 2; ++s) {
        int d = (s == 0) ? firstDir : -firstDir;
        int target = x;
        for (int step = 1; step <= reach; ++step) {
            int nx = x + d * step;
            if (canDisplace(m, nx, y)) target = nx; else break;
        }
        if (target != x) { moveCell(x, y, target, y); return; }
    }
}

// ----------------------------------------------------------------------------
// WATER — liquid movement plus evaporation near lava.
// ----------------------------------------------------------------------------
void Simulation::updateWater(int x, int y) {
    // Evaporate slowly when touching lava: 1% per adjacent lava cell.
    static const int ox[4] = { -1, 1, 0, 0 };
    static const int oy[4] = { 0, 0, -1, 1 };
    int lavaNeighbors = 0;
    for (int i = 0; i < 4; ++i) {
        int nx = x + ox[i], ny = y + oy[i];
        if (inBounds(nx, ny) && at(nx, ny).material == M::LAVA) ++lavaNeighbors;
    }
    if (lavaNeighbors > 0 && chance(lavaNeighbors)) { // ~1% per lava cell
        setCell(x, y, M::STEAM, uint8_t(60 + rng() % 40));
        return;
    }
    updateLiquid(x, y);
}

// ----------------------------------------------------------------------------
// LAVA — slow liquid that ignites things, hardens in water, and spits embers.
// ----------------------------------------------------------------------------
void Simulation::updateLava(int x, int y) {
    static const int ox[4] = { -1, 1, 0, 0 };
    static const int oy[4] = { 0, 0, -1, 1 };

    // Water contact: lava quenches to stone and flashes the water to steam.
    bool quenched = false;
    for (int i = 0; i < 4; ++i) {
        int nx = x + ox[i], ny = y + oy[i];
        if (inBounds(nx, ny) && at(nx, ny).material == M::WATER) {
            setCell(nx, ny, M::STEAM, uint8_t(60 + rng() % 40));
            quenched = true;
        }
    }
    if (quenched) { setCell(x, y, M::STONE); return; }

    // Ignite adjacent flammable solids/powders by converting them to fire.
    for (int i = 0; i < 4; ++i) {
        int nx = x + ox[i], ny = y + oy[i];
        if (inBounds(nx, ny) && isFlammable(at(nx, ny).material) && chance(25)) {
            setCell(nx, ny, M::FIRE, uint8_t(40 + rng() % 80));
        }
    }

    // Occasionally spit fire/smoke from the surface (only into empty space).
    if (y > 0 && at(x, y - 1).isAir() && chance(5)) {
        setCell(x, y - 1, (rng() & 1) ? M::FIRE : M::SMOKE,
                uint8_t(40 + rng() % 60));
    }

    // Movement: lava is viscous — it only flows on ~1 in 3 frames.
    if (frame_ % 3 == 0) updateLiquid(x, y);
    else chunks_.wake(x, y); // keep its chunk awake so reactions keep ticking
}

// ----------------------------------------------------------------------------
// ACID — flows like water, eats most things, gets consumed doing it.
// ----------------------------------------------------------------------------
void Simulation::updateAcid(int x, int y) {
    static const int ox[4] = { -1, 1, 0, 0 };
    static const int oy[4] = { 0, 0, -1, 1 };
    for (int i = 0; i < 4; ++i) {
        int nx = x + ox[i], ny = y + oy[i];
        if (!inBounds(nx, ny)) continue;
        M nm = at(nx, ny).material;
        bool dissolves = (nm == M::SAND || nm == M::WOOD || nm == M::GUNPOWDER);
        if (dissolves && chance(5)) {
            setCell(nx, ny, M::AIR); // dissolve the victim
            setCell(x, y, M::AIR);   // ...and use up the acid
            return;
        }
    }
    updateLiquid(x, y);
}

// ----------------------------------------------------------------------------
// FIRE — rises, ages through a color ramp (handled in Renderer), spreads via
// the flammable materials' own update, and decays into smoke.
// ----------------------------------------------------------------------------
void Simulation::updateFire(int x, int y) {
    Cell& c = at(x, y);

    // Age. When it burns out, sometimes leave a puff of smoke.
    if (c.lifetime == 0) {
        if (chance(40)) setCell(x, y, M::SMOKE, uint8_t(80 + rng() % 120));
        else            setCell(x, y, M::AIR);
        return;
    }
    --c.lifetime;
    chunks_.wake(x, y); // fire is always "lively"

    // Emit smoke upward as it ages.
    if (y > 0 && at(x, y - 1).isAir() && chance(8)) {
        setCell(x, y - 1, M::SMOKE, uint8_t(80 + rng() % 120));
        return;
    }

    // Rise into empty space or through smoke, with a little horizontal flicker.
    if (canDisplace(M::FIRE, x, y - 1)) { moveCell(x, y, x, y - 1); return; }
    int dir = (rng() & 1) ? 1 : -1;
    if (canDisplace(M::FIRE, x + dir, y - 1)) { moveCell(x, y, x + dir, y - 1); return; }
}

// ----------------------------------------------------------------------------
// WOOD — static but flammable. It checks its own neighbours for fire/lava so
// the ignition logic lives in one place.
// ----------------------------------------------------------------------------
void Simulation::updateWood(int x, int y) {
    if (hasNeighbor(x, y, M::FIRE) || hasNeighbor(x, y, M::LAVA)) {
        if (chance(20)) setCell(x, y, M::FIRE, uint8_t(40 + rng() % 80));
        else chunks_.wake(x, y); // stay awake while a neighbour burns
    }
}

// ----------------------------------------------------------------------------
// GASES — smoke & steam: rise, drift, and fade out.
// ----------------------------------------------------------------------------
void Simulation::updateGas(int x, int y) {
    Cell& c = at(x, y);
    if (c.lifetime == 0) { setCell(x, y, M::AIR); return; }
    --c.lifetime;
    chunks_.wake(x, y);

    // Steam rises faster (tries to jump two cells) than smoke.
    bool fast = (c.material == M::STEAM);
    int rise = (fast && canDisplace(c.material, x, y - 2)) ? 2 : 1;

    // Drift: bias upward, occasionally sideways for a natural plume.
    int r = rng() % 4;
    int dx = (r == 0) ? -1 : (r == 1 ? 1 : 0);
    if (canDisplace(c.material, x + dx, y - rise)) { moveCell(x, y, x + dx, y - rise); return; }
    if (canDisplace(c.material, x, y - 1))         { moveCell(x, y, x, y - 1);         return; }
    // If capped below by something, slide sideways so plumes spread under ceilings.
    if (canDisplace(c.material, x + dx, y))         { moveCell(x, y, x + dx, y);         return; }
}

// ----------------------------------------------------------------------------
// EXPLOSION KNOCKBACK — ballistic motion for cells flung by a blast.
// velX/velY are an impulse budget; each frame we step along it, lose energy to
// friction, and gain downward velocity from gravity so debris arcs and lands.
// ----------------------------------------------------------------------------
void Simulation::applyVelocity(int x, int y) {
    // Read the impulse off the starting cell. The material itself swaps along as
    // it moves (moveCell swaps whole Cells), so velX/velY travel with it.
    int vx = at(x, y).velX;
    int vy = at(x, y).velY;
    int sx = (vx > 0) - (vx < 0);
    int sy = (vy > 0) - (vy < 0);

    // Micro-steps this frame scale with remaining speed (cap 4 for stability).
    int speed = std::max(std::abs(vx), std::abs(vy));
    int steps = std::max(1, std::min(4, speed / 3));

    int cx = x, cy = y;
    bool collided = false;
    for (int i = 0; i < steps && (sx != 0 || sy != 0); ++i) {
        int nx = cx + sx, ny = cy + sy;
        if (canDisplace(at(cx, cy).material, nx, ny)) {
            moveCell(cx, cy, nx, ny);
            cx = nx; cy = ny;
        } else {
            collided = true;
            break;
        }
    }

    // Update the impulse on the particle's *current* cell.
    Cell& moved = at(cx, cy);
    if (collided) {
        moved.velX = 0;                       // lose horizontal momentum on impact
        moved.velY = int8_t(moved.velY / 2);  // bleed off the vertical
    } else {
        moved.velX = int8_t(vx - sx);         // friction toward 0
        moved.velY = int8_t(vy + 1);          // gravity pulls the arc down
    }
    if (moved.velX == 0 && moved.velY >= 0 && moved.velY <= 1) {
        moved.velX = moved.velY = 0;          // at rest; normal physics resumes
    } else {
        chunks_.wake(cx, cy);                 // still in flight: keep simulating
    }
    moved.markUpdated();
}

// ----------------------------------------------------------------------------
// Explosion entry point — delegates the geometry to Explosion.cpp.
// ----------------------------------------------------------------------------
void Simulation::explode(int cx, int cy, int radius) {
    Explosion::detonate(*this, cx, cy, radius);
}
