#ifndef VOXEL_H
#define VOXEL_H

#include "nasl/nasl.h"
#include <vector>
#include <cstdint>
#include "BitMask.hpp"

#include "chunk_mesh.h"

using namespace nasl;

struct Voxel {
    ivec3 position;
    vec3 color;
    // mat4 orientation; // for later

    void copy_to(std::vector<uint8_t>& buf) const {
        uint8_t tmp[sizeof(Voxel)];
        memcpy(tmp, this, sizeof(Voxel));
        for (auto b : tmp)
            buf.push_back(b);
    }
};

struct AABB {
    vec3 min;
    vec3 max;

    AABB(const vec3& min, const vec3& max) : min(min), max(max) {}
};

struct GreedyVoxel {
    ivec3 start;
    ivec3 end;
    vec3 color;

    void copy_to(std::vector<uint8_t>& buf) const {
        uint8_t tmp[sizeof(GreedyVoxel)];
        memcpy(tmp, this, sizeof(GreedyVoxel));
        for (auto b : tmp)
            buf.push_back(b);
    }
};


struct ChunkVoxels {
    std::unique_ptr<imr::Buffer> voxel_buf;
    std::unique_ptr<imr::Buffer> greedy_voxel_buf; // for greedy meshing
    size_t num_voxels = 0;
    // https://www.badlion.net/minecraft-blog/what-minecraft-height-limit-2024
    int min_height = -64, max_height = 320;

    ChunkVoxels(imr::Device& device, ChunkNeighbors& neighbors, const ivec2& chunkPos, const bool greedyMeshing);

    AABB getBoundingBox(ivec2& chunk_pos) const {
        return {
            vec3(chunk_pos.x * CUNK_CHUNK_SIZE, min_height, chunk_pos.y * CUNK_CHUNK_SIZE),
             vec3(chunk_pos.x * CUNK_CHUNK_SIZE + CUNK_CHUNK_SIZE, max_height, chunk_pos.y * CUNK_CHUNK_SIZE + CUNK_CHUNK_SIZE)
        };
    }

    [[nodiscard]] VkDeviceAddress voxel_buffer_device_address(const bool greedyMeshing) const {
        return greedyMeshing ? greedy_voxel_buf->device_address() : voxel_buf->device_address();
    }
};


#endif
