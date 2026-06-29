// Materials.h
// ----------------------------------------------------------------------------
// Defines the universe of materials the simulator understands: the MaterialType
// enum, a static table of per-material properties (color, density, flags), and
// small helpers for packing/perturbing ARGB colors.
//
// WHY a property table instead of virtual classes per material?
//   A falling-sand sim touches every cell many times per second. Virtual
//   dispatch and per-cell objects would thrash the cache and allocate. Instead
//   every cell is a tiny POD struct (see Cell.h) and behavior is driven by a
//   compact, branch-friendly lookup in a flat array. This keeps the hot loop
//   allocation-free and cache-friendly.
// ----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <array>

// ---------------------------------------------------------------------------
// Material identifiers. Order matters only for the palette/number-key mapping
// and the property table below; keep COUNT last.
// ---------------------------------------------------------------------------
enum class MaterialType : uint8_t {
    AIR = 0,    // empty cell
    SAND,       // granular powder
    WATER,      // liquid
    FIRE,       // transient, rises, ignites
    SMOKE,      // transient gas, rises + fades
    WOOD,       // static, flammable
    STONE,      // static, inert
    LAVA,       // slow liquid, ignites, hardens in water
    GUNPOWDER,  // powder, explosive
    STEAM,      // transient gas (water + lava)
    ACID,       // liquid, dissolves things
    COUNT
};

// Broad movement family. The simulation picks an update routine from this.
enum class MoveClass : uint8_t {
    NONE = 0,   // never moves on its own (AIR handled specially, STONE, WOOD)
    POWDER,     // falls down, then diagonally (sand, gunpowder)
    LIQUID,     // falls down, then spreads horizontally (water, lava, acid)
    GAS,        // rises, drifts (smoke, steam, fire is special-cased)
};

// Per-cell flag bits (stored in Cell::flags).
enum CellFlags : uint8_t {
    FLAG_UPDATED = 1 << 0,  // already simulated this frame (prevents double-move)
    FLAG_BURNING = 1 << 1,  // wood/gunpowder currently catching fire
};

// ---------------------------------------------------------------------------
// Static, read-only description of a material. One entry per MaterialType.
// ---------------------------------------------------------------------------
struct MaterialProps {
    const char* name;       // shown in the HUD
    uint32_t    baseColor;  // 0xAARRGGBB
    MoveClass   move;
    uint8_t     density;    // for buoyancy/displacement (higher sinks)
    bool        flammable;  // can be ignited by fire/lava
    bool        placeable;  // can the player paint it directly?
    uint8_t     variantRange; // +/- brightness jitter applied per particle
};

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------
constexpr uint32_t makeARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) |
           (uint32_t(g) << 8)  |  uint32_t(b);
}

inline uint8_t clamp8(int v) {
    return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// Add a signed brightness offset to every channel of an ARGB color, preserving
// alpha. Used to give each grain a slightly different, *persistent* shade.
inline uint32_t shadeColor(uint32_t argb, int delta) {
    uint8_t a = (argb >> 24) & 0xFF;
    int r = ((argb >> 16) & 0xFF) + delta;
    int g = ((argb >> 8)  & 0xFF) + delta;
    int b = ( argb        & 0xFF) + delta;
    return makeARGB(a, clamp8(r), clamp8(g), clamp8(b));
}

// Linear blend between two ARGB colors (t in [0,255]). Used for fire age fade.
inline uint32_t lerpColor(uint32_t c0, uint32_t c1, int t) {
    int it = 255 - t;
    uint8_t a = clamp8((((c0 >> 24) & 0xFF) * it + ((c1 >> 24) & 0xFF) * t) / 255);
    uint8_t r = clamp8((((c0 >> 16) & 0xFF) * it + ((c1 >> 16) & 0xFF) * t) / 255);
    uint8_t g = clamp8((((c0 >> 8)  & 0xFF) * it + ((c1 >> 8)  & 0xFF) * t) / 255);
    uint8_t b = clamp8((( c0        & 0xFF) * it + ( c1        & 0xFF) * t) / 255);
    return makeARGB(a, r, g, b);
}

// ---------------------------------------------------------------------------
// The master property table. Indexed by MaterialType.
// Colors taken from the design brief.
// ---------------------------------------------------------------------------
inline const MaterialProps& materialProps(MaterialType m) {
    static const std::array<MaterialProps, size_t(MaterialType::COUNT)> table = {{
        // name          color                       move              dens flam place varJitter
        { "Air",       makeARGB(0,   0,   0,   0  ), MoveClass::NONE,    0,  false, false, 0 },
        { "Sand",      makeARGB(255, 0xE8,0xC8,0x4A), MoveClass::POWDER, 200, false, true, 10 },
        { "Water",     makeARGB(220, 0x3A,0x8F,0xD4), MoveClass::LIQUID,  60, false, true,  8 },
        { "Fire",      makeARGB(255, 0xFF,0x45,0x00), MoveClass::GAS,      5, false, true, 12 },
        { "Smoke",     makeARGB(180, 0x55,0x55,0x55), MoveClass::GAS,      3, false, false, 14 },
        { "Wood",      makeARGB(255, 0x8B,0x45,0x13), MoveClass::NONE,   255, true,  true,  9 },
        { "Stone",     makeARGB(255, 0x88,0x88,0x88), MoveClass::NONE,   255, false, true, 10 },
        { "Lava",      makeARGB(255, 0xCC,0x33,0x00), MoveClass::LIQUID, 220, false, true, 14 },
        { "Gunpowder", makeARGB(255, 0x33,0x33,0x33), MoveClass::POWDER, 190, true,  true, 16 },
        { "Steam",     makeARGB(160, 0xCC,0xDD,0xFF), MoveClass::GAS,      2, false, false, 12 },
        { "Acid",      makeARGB(220, 0x00,0xFF,0x44), MoveClass::LIQUID,  55, false, true, 12 },
    }};
    return table[size_t(m)];
}

// Convenience predicates used throughout the simulation.
inline bool isFlammable(MaterialType m) { return materialProps(m).flammable; }
inline bool isLiquid(MaterialType m)    { return materialProps(m).move == MoveClass::LIQUID; }
inline bool isGas(MaterialType m)       { return materialProps(m).move == MoveClass::GAS; }
inline bool isPowder(MaterialType m)    { return materialProps(m).move == MoveClass::POWDER; }
