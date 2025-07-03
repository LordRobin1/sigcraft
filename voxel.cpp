#include "voxel.h"
#include <algorithm>
#include <span>

extern "C" {
#include "enklume/block_data.h"
}

void chunk_voxels(const ChunkData* chunk, ChunkNeighbors& neighbours, std::vector<uint8_t>& voxel_buffer, size_t* num_voxels) {
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
        for (int x = 0; x < CUNK_CHUNK_SIZE; x++) {
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
                for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                    int world_y = y + section * CUNK_CHUNK_SIZE;
                    const BlockData block_data = access_safe(chunk, neighbours, x, world_y, z);
                    struct n_idx {
                        int x, y, z;
                    };
                    const std::array<n_idx, 6> n_idxs{{
                        {x, world_y + 1, z},
                        {x, world_y - 1, z},
                        {x + 1, world_y, z},
                        {x - 1, world_y, z},
                        {x, world_y, z + 1},
                        {x, world_y, z - 1}
                    }};

                    const bool occluded = std::ranges::any_of(n_idxs.begin(), n_idxs.end(), [&](const auto& n) {
                        return access_safe(chunk, neighbours, n.x, n.y, n.z) != BlockAir;
                    });

                    if (block_data != BlockAir && !occluded) {
                        Voxel v;
                        // TODO adding section * CUNK_CHUNK_SIZE doesn't make any sense
                        v.position.x = x + section * CUNK_CHUNK_SIZE;
                        v.position.y = world_y;
                        v.position.z = z + section * CUNK_CHUNK_SIZE;
                        v.color.x = block_colors[block_data].r;
                        v.color.y = block_colors[block_data].g;
                        v.color.z = block_colors[block_data].b;
                        v.copy_to(voxel_buffer);
                        *num_voxels += 1;
                    }
                }
            }
        }
    }
}

ChunkVoxels::ChunkVoxels(imr::Device& device, ChunkNeighbors& neighbors) {
    std::vector<uint8_t> voxel_buffer;
    num_voxels = 0;
    chunk_voxels(neighbors.neighbours[1][1], neighbors, voxel_buffer, &num_voxels);

    size_t voxel_buffer_size = voxel_buffer.size();

    if (voxel_buffer_size > 0) {
        voxel_buf = std::make_unique<imr::Buffer>(device, voxel_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        voxel_buf->uploadDataSync(0, voxel_buffer_size, voxel_buffer.data());
    }
}
