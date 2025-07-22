#ifndef SIGCRAFT_CHUNK_MESH_H
#define SIGCRAFT_CHUNK_MESH_H

#include "imr/imr.h"

#include <cstddef>

extern "C" {
#include "enklume/block_data.h"
}

struct ChunkNeighbors {
    const ChunkData* neighbours[3][3];
};

BlockData access_safe(const ChunkData* chunk, ChunkNeighbors& neighbours, int x, int y, int z);

struct ChunkMesh {
    std::unique_ptr<imr::Buffer> buf;
    size_t num_verts;

    ChunkMesh(imr::Device&, ChunkNeighbors& n);

    struct Vertex {
        int16_t vx, vy, vz;
        uint8_t tt;
        uint8_t ss;
        uint8_t nnx, nny, nnz;
        uint8_t pad;
        uint8_t br, bg, bb, pad2;
    };

    static_assert(sizeof(Vertex) == sizeof(uint8_t) * 16);
};

#endif
