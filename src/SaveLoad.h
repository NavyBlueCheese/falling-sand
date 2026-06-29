// SaveLoad.h
// ----------------------------------------------------------------------------
// Dead-simple binary snapshot of the grid. Format:
//   [4]  magic  "SAND"
//   [4]  width  (uint32, little-endian native)
//   [4]  height (uint32)
//   [w*h*sizeof(Cell)] raw cell data
// No compression — a 320x180 grid is ~460 KB, which writes/reads instantly.
// ----------------------------------------------------------------------------
#pragma once

#include <string>
class Simulation;

namespace SaveLoad {
    bool save(const Simulation& sim, const std::string& path);
    bool load(Simulation& sim, const std::string& path);
}
