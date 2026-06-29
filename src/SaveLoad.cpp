// SaveLoad.cpp — see SaveLoad.h for the format.
#include "SaveLoad.h"
#include "Simulation.h"
#include <cstdio>
#include <cstring>

namespace SaveLoad {

bool save(const Simulation& sim, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    const char magic[4] = { 'S', 'A', 'N', 'D' };
    uint32_t w = Simulation::WIDTH, h = Simulation::HEIGHT;
    std::fwrite(magic, 1, 4, f);
    std::fwrite(&w, sizeof(w), 1, f);
    std::fwrite(&h, sizeof(h), 1, f);
    std::fwrite(sim.data(), sizeof(Cell), size_t(w) * h, f);

    std::fclose(f);
    return true;
}

bool load(Simulation& sim, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    char magic[4] = {};
    uint32_t w = 0, h = 0;
    bool ok = std::fread(magic, 1, 4, f) == 4 &&
              std::fread(&w, sizeof(w), 1, f) == 1 &&
              std::fread(&h, sizeof(h), 1, f) == 1;
    // Validate header before trusting the payload.
    if (!ok || std::memcmp(magic, "SAND", 4) != 0 ||
        w != uint32_t(Simulation::WIDTH) || h != uint32_t(Simulation::HEIGHT)) {
        std::fclose(f);
        return false;
    }

    size_t n = size_t(w) * h;
    size_t read = std::fread(sim.data(), sizeof(Cell), n, f);
    std::fclose(f);
    if (read != n) return false;

    sim.chunks().wakeAll(); // re-settle the loaded scene next frame
    return true;
}

} // namespace SaveLoad
