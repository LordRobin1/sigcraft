#ifndef VOXEL_H
#define VOXEL_H

#include "nasl/nasl.h"
#include <vector>
#include <cstdint>
#include <span>

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

struct ChunkVoxels {
    std::unique_ptr<imr::Buffer> voxel_buf;
    size_t num_voxels;

    ChunkVoxels(imr::Device&, ChunkNeighbors& neighbors, const ivec2& chunkPos);

    [[nodiscard]] VkDeviceAddress voxel_buffer_device_address() const {
        return voxel_buf->device_address();
    }
};


#endif
