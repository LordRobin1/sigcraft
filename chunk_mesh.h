#ifndef SIGCRAFT_CHUNK_MESH_H
#define SIGCRAFT_CHUNK_MESH_H

#include "imr/imr.h"

#include <cstddef>
#include <limits>

#include "nasl/nasl_vec.h"
#include "aabb.h"

extern "C" {
#include "enklume/block_data.h"
}

using namespace nasl;

struct ChunkNeighbors {
    const ChunkData* neighbours[3][3];
};

BlockData access_safe(const ChunkData* chunk, ChunkNeighbors& neighbours, int x, int y, int z);

struct ChunkMesh {
    std::unique_ptr<imr::Buffer> buf;
    size_t num_verts;
    int min_height = std::numeric_limits<int>::max(), max_height = std::numeric_limits<int>::min();

    ChunkMesh(imr::Device&, ChunkNeighbors& n);

    [[nodiscard]] AABB getBoundingBox(const ivec2& chunk_pos) const {
        return {
            vec3(chunk_pos.x * CUNK_CHUNK_SIZE, min_height, chunk_pos.y * CUNK_CHUNK_SIZE),
             vec3(chunk_pos.x * CUNK_CHUNK_SIZE + CUNK_CHUNK_SIZE, max_height, chunk_pos.y * CUNK_CHUNK_SIZE + CUNK_CHUNK_SIZE)
        };
    }

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
