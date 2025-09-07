#pragma once
#include <enklume/block_data.h>


struct ChunkNeighborsUnsafe {
    ChunkData* neighbours[3][3];
};

inline BlockData access_safe(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbours, int x, int y, int z) {
    unsigned int i, k;
    if (x < 0) {
        i = 0;
    } else if (x < CUNK_CHUNK_SIZE) {
        i = 1;
    } else {
        i = 2;
    }

    if (y < 0 || y >= CUNK_CHUNK_MAX_HEIGHT)
        return BlockAir;

    if (z < 0) {
        k = 0;
    } else if (z < CUNK_CHUNK_SIZE) {
        k = 1;
    } else {
        k = 2;
    }
    //assert(!neighbours || neighbours[1][1][1] == chunk);
    if (i == 1 && k == 1) {
        return chunk_get_block_data(chunk, x, y, z);
    } else {
        if (neighbours.neighbours[i][k])
            return chunk_get_block_data(neighbours.neighbours[i][k], x & 15, y, z & 15);
    }

    return BlockAir;
}
