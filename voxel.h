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

    std::vector<uint8_t> generate_bb_verts() const;
};

struct ChunkVoxels {
    std::unique_ptr<imr::Buffer> voxel_buf;
    std::unique_ptr<imr::Buffer> vert_buf;
    size_t num_voxels;
    size_t num_verts;

    // I don't think we need neighbors since that's for mesh gen
    ChunkVoxels(imr::Device& device, ChunkData* data, const ivec2& chunkPos);

    [[nodiscard]] VkDeviceAddress voxel_buffer_device_address() const {
        return voxel_buf->device_address();
    }

    struct Vertex {
        int16_t vx, vy, vz;

        void copy_to(std::vector<uint8_t>& buf) const {
            uint8_t tmp[sizeof(Vertex)];
            memcpy(tmp, this, sizeof(Vertex));
            for (auto b : tmp)
                buf.push_back(b);
        }
    };
};


#endif
