// Cell.h
// ----------------------------------------------------------------------------
// The single grid unit. Deliberately a small POD (8 bytes) so the whole
// 320x180 grid is ~460 KB and stays comfortably in L2 cache. No pointers, no
// virtuals, no per-cell heap.
// ----------------------------------------------------------------------------
#pragma once

#include "Materials.h"
#include <cstdint>

struct Cell {
    MaterialType material = MaterialType::AIR;
    uint8_t      lifetime = 0;  // countdown for transient cells (fire/smoke/steam)
    uint8_t      variant  = 0;  // fixed per-particle brightness jitter seed
    uint8_t      flags    = 0;  // CellFlags bitmask (reset/maintained per frame)
    int8_t       velX     = 0;  // explosion knockback, horizontal (cells/frame-ish)
    int8_t       velY     = 0;  // explosion knockback, vertical
    uint8_t      pad0     = 0;  // keep struct a tidy 8 bytes
    uint8_t      pad1     = 0;

    bool isAir()     const { return material == MaterialType::AIR; }
    bool isUpdated() const { return flags & FLAG_UPDATED; }
    void markUpdated()     { flags |= FLAG_UPDATED; }
    void clearUpdated()    { flags &= ~FLAG_UPDATED; }
};
