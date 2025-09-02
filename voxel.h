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
    size_t num_voxels;

    ChunkVoxels(imr::Device& device, ChunkNeighbors& neighbors, const ivec2& chunkPos, const bool greedyMeshing);

    [[nodiscard]] VkDeviceAddress voxel_buffer_device_address(const bool greedyMeshing) const {
        return voxel_buf->device_address();
    }
};


#endif
