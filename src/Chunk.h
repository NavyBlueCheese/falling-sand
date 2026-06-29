// Chunk.h
// ----------------------------------------------------------------------------
// Dirty-region bookkeeping. The grid is divided into fixed CHUNK_SIZE blocks.
// A chunk is "active" for a frame if anything inside it (or a neighbour) moved
// recently. The simulation skips cells whose chunk is inactive.
//
// WHY: in a typical scene most of the screen is settled sand/stone/empty. Those
// regions do not need to be revisited every step. Tracking activity per chunk
// turns an O(W*H) sweep into "only the lively bits", which is what keeps a full
// 320x180 grid at 60 FPS.
//
// Double buffering: we read `current` activity to decide what to simulate this
// frame, and write `next` activity as cells change. At end of frame we swap. A
// changed cell also wakes its 8 neighbouring chunks so motion can cross chunk
// borders without stalling for a frame.
// ----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <cstdint>

class ChunkGrid {
public:
    static constexpr int CHUNK_SIZE = 16;

    ChunkGrid(int gridW, int gridH);

    int chunksX() const { return cx_; }
    int chunksY() const { return cy_; }

    // Is the chunk containing cell (x,y) scheduled to be simulated this frame?
    bool isActiveAt(int x, int y) const;

    // Mark the chunk containing (x,y) — and its neighbours — active next frame.
    void wake(int x, int y);

    // Wake everything (used after load / clear so the next frame settles).
    void wakeAll();

    // Called once per frame: promote `next` activity to `current`, clear `next`.
    void commit();

    int activeChunkCount() const;

private:
    int gridW_, gridH_;
    int cx_, cy_;
    std::vector<uint8_t> current_; // simulate these this frame
    std::vector<uint8_t> next_;    // accumulate wake requests for next frame

    int idx(int chunkX, int chunkY) const { return chunkY * cx_ + chunkX; }
    void wakeChunk(int chunkX, int chunkY, std::vector<uint8_t>& buf);
};
