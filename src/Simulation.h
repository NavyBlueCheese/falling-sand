// Simulation.h
// ----------------------------------------------------------------------------
// The cellular-automaton world: the grid, the per-step update, and all the
// material interaction rules. This is the heart of the project.
//
// KEY DESIGN DECISIONS (explained where they appear in the .cpp):
//   * Bottom-to-top row order so a particle that falls doesn't get processed
//     again in the same frame.
//   * Alternating left<->right horizontal scan each frame to cancel the
//     directional bias an automaton otherwise develops (piles leaning one way).
//   * A per-cell FLAG_UPDATED so each cell moves at most once per step.
//   * Dirty chunks (ChunkGrid) so settled regions cost nothing.
// ----------------------------------------------------------------------------
#pragma once

#include "Cell.h"
#include "Chunk.h"
#include <vector>
#include <cstdint>

class Simulation {
public:
    static constexpr int WIDTH  = 320;
    static constexpr int HEIGHT = 180;

    Simulation();

    // Advance the world by one fixed timestep (called 60x/sec, scaled by speed).
    void update();

    // ---- grid access -------------------------------------------------------
    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < WIDTH && y < HEIGHT;
    }
    Cell&       at(int x, int y)       { return grid_[size_t(y) * WIDTH + x]; }
    const Cell& at(int x, int y) const { return grid_[size_t(y) * WIDTH + x]; }
    Cell*       data()                 { return grid_.data(); }
    const Cell* data() const           { return grid_.data(); }

    // ---- editing -----------------------------------------------------------
    void clear();                                  // everything -> AIR
    void paintCircle(int cx, int cy, int radius, MaterialType m);
    void spawnCell(int x, int y, MaterialType m);  // place one cell with jitter

    // ---- explosions (also used by gunpowder internally) --------------------
    void explode(int cx, int cy, int radius);

    // ---- stats -------------------------------------------------------------
    int  activeCellCount() const { return activeCells_; }
    int  activeChunkCount() const { return chunks_.activeChunkCount(); }
    uint64_t frameCount() const  { return frame_; }

    ChunkGrid& chunks() { return chunks_; }

    // Fast deterministic-ish RNG (xorshift32). Public so explosion code reuses it.
    uint32_t rng() {
        rngState_ ^= rngState_ << 13;
        rngState_ ^= rngState_ >> 17;
        rngState_ ^= rngState_ << 5;
        return rngState_;
    }
    // Returns true with probability percent/100.
    bool chance(int percent) { return int(rng() % 100) < percent; }

private:
    std::vector<Cell> grid_;
    ChunkGrid         chunks_;
    uint32_t          rngState_ = 0x1234567u;
    uint64_t          frame_    = 0;
    int               activeCells_ = 0;

    // --- movement primitives ------------------------------------------------
    int  density(MaterialType m) const { return materialProps(m).density; }
    bool canDisplace(MaterialType src, int nx, int ny) const;
    void moveCell(int x, int y, int nx, int ny);   // move or swap, with flags+chunks
    void setCell(int x, int y, MaterialType m, uint8_t lifetime = 0);

    // --- per-material update routines ---------------------------------------
    void updatePowder(int x, int y);   // sand, gunpowder (movement only)
    void updateLiquid(int x, int y);   // water, lava, acid (movement only)
    void updateGas(int x, int y);      // smoke, steam (movement only)
    void updateFire(int x, int y);
    void updateWater(int x, int y);
    void updateLava(int x, int y);
    void updateAcid(int x, int y);
    void updateWood(int x, int y);
    void updateGunpowder(int x, int y);
    void applyVelocity(int x, int y);  // explosion knockback ballistics

    int  liquidReach(int x, int y) const;  // pressure-based horizontal flow limit
    bool hasNeighbor(int x, int y, MaterialType m) const;
};
