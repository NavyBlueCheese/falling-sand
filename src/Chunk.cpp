// Chunk.cpp — see Chunk.h for the rationale behind dirty chunks.
#include "Chunk.h"
#include <algorithm>

ChunkGrid::ChunkGrid(int gridW, int gridH)
    : gridW_(gridW), gridH_(gridH) {
    // Round up so partial chunks at the right/bottom edge are still covered.
    cx_ = (gridW_ + CHUNK_SIZE - 1) / CHUNK_SIZE;
    cy_ = (gridH_ + CHUNK_SIZE - 1) / CHUNK_SIZE;
    current_.assign(size_t(cx_) * cy_, 1); // start fully active so the first
    next_.assign(size_t(cx_) * cy_, 0);    // frame can settle whatever was loaded
}

bool ChunkGrid::isActiveAt(int x, int y) const {
    int chunkX = x / CHUNK_SIZE;
    int chunkY = y / CHUNK_SIZE;
    return current_[idx(chunkX, chunkY)] != 0;
}

void ChunkGrid::wakeChunk(int chunkX, int chunkY, std::vector<uint8_t>& buf) {
    if (chunkX < 0 || chunkY < 0 || chunkX >= cx_ || chunkY >= cy_) return;
    buf[idx(chunkX, chunkY)] = 1;
}

void ChunkGrid::wake(int x, int y) {
    int chunkX = x / CHUNK_SIZE;
    int chunkY = y / CHUNK_SIZE;
    // Wake the chunk and all 8 neighbours so cross-border motion is not delayed.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            wakeChunk(chunkX + dx, chunkY + dy, next_);
}

void ChunkGrid::wakeAll() {
    std::fill(current_.begin(), current_.end(), 1);
    std::fill(next_.begin(),    next_.end(),    1);
}

void ChunkGrid::commit() {
    current_.swap(next_);
    std::fill(next_.begin(), next_.end(), 0);
}

int ChunkGrid::activeChunkCount() const {
    int n = 0;
    for (uint8_t v : current_) n += (v != 0);
    return n;
}
