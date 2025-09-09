#pragma once

extern "C" {
#include <enklume/block_data.h>
}

struct ChunkNeighborsUnsafe {
    ChunkData* neighbours[3][3];
};

BlockData access_safe(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbours, int x, int y, int z);
