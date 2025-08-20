#include "voxel.h"

#include <iostream>

extern "C" {
#include "enklume/block_data.h"
}

inline int toWorldY(const int section, const int y) { return y + section * CUNK_CHUNK_SIZE; }

void chunk_voxels(const ChunkData* chunk, const ivec2& chunkPos, ChunkNeighbors& neighbours, std::vector<uint8_t>& voxel_buffer,  size_t* num_voxels) {
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

                    bool occluded = true;
                    for (const auto &[x, y, z] : n_idxs) {
                        if (access_safe(chunk, neighbours, x, y, z) == BlockAir) {
                            occluded = false;
                            break;
                        }
                    }

                    if (block_data != BlockAir && !occluded) {
                        Voxel v;
                        // TODO adding section * CUNK_CHUNK_SIZE doesn't make any sense
                        v.position.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
                        v.position.y = world_y;
                        v.position.z = z + chunkPos.y * CUNK_CHUNK_SIZE; // y is our z here
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

void greedyMeshSlice(BitMask& mask, const ChunkData* data, const ivec2& chunkPos, const int worldY, ChunkNeighbors& neighbors, std::vector<uint8_t>& voxelBuffer, size_t* numVoxels) {
    for (int x = 0; x < CUNK_CHUNK_SIZE; x++) {
        // keep greedily meshing until this row has none of this block type left
        while (mask.mask[x] != 0) {
            const int zStart = trailingZeros(mask.mask[x]);
            const uint16_t shifted = mask.mask[x] >> zStart; // push ones down to LSB
            const int zLength = trailingOnes(shifted);
            const int zEnd = zStart + zLength;

            // expand along x-axis
            int xEnd = x + 1;
            const uint16_t pattern = ((1u << zLength) - 1) << zStart;
            while (xEnd < CUNK_CHUNK_SIZE) {
                // check if all blocks to the side are also valid
                if ((pattern & mask.mask[xEnd]) != pattern) break;
                // clear those bits
                mask.mask[xEnd] &= ~pattern;
                xEnd++;
            }

            GreedyVoxel gv;
            gv.start.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
            gv.start.y = worldY;
            gv.start.z = zStart + chunkPos.y * CUNK_CHUNK_SIZE;

            gv.end.x = xEnd + chunkPos.x * CUNK_CHUNK_SIZE;
            gv.end.y = worldY + 1;
            gv.end.z = zEnd + chunkPos.y * CUNK_CHUNK_SIZE;

            gv.color.x = block_colors[mask.type].r;
            gv.color.y = block_colors[mask.type].g;
            gv.color.z = block_colors[mask.type].b;

            gv.copy_to(voxelBuffer);
            *numVoxels += 1;

            // clear the used bits
            mask.mask[x] &= ~(((1u << zLength) - 1) << zStart);
        }
    }
}

void greedy_chunk_voxels(const ChunkData* chunk, const ivec2& chunkPos, ChunkNeighbors& neighbours, std::vector<uint8_t>& voxel_buffer,  size_t* num_voxels) {
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
        for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
            const int worldY = toWorldY(section, y);
            // generate a BitMask for each block type for this vertical slice, but skip air obv
            for (int i = 1; i < BlockCount; i++) {
                BitMask mask{chunk, neighbours, worldY, static_cast<BlockId>(i)};
                // std::cout << "Processing BlockId: " << i << std::endl;
                greedyMeshSlice(mask, chunk, chunkPos, worldY, neighbours, voxel_buffer, num_voxels);
            }
        }
    }
}


ChunkVoxels::ChunkVoxels(imr::Device& device, ChunkNeighbors& neighbors, const ivec2& chunkPos, const bool greedyMeshing) {
    std::vector<uint8_t> voxel_buffer;
    num_voxels = 0;
    if (greedyMeshing) {
        greedy_chunk_voxels(neighbors.neighbours[1][1], chunkPos, neighbors, voxel_buffer, &num_voxels);
    } else {
        chunk_voxels(neighbors.neighbours[1][1], chunkPos, neighbors, voxel_buffer, &num_voxels);
    }
    size_t voxel_buffer_size = voxel_buffer.size();
    if (voxel_buffer_size > 0) {
        voxel_buf = std::make_unique<imr::Buffer>(device, voxel_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        voxel_buf->uploadDataSync(0, voxel_buffer_size, voxel_buffer.data());
    }
}