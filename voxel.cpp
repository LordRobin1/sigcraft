#include "voxel.h"
#include <algorithm>
#include <iostream>
#include <span>

extern "C" {
#include "enklume/block_data.h"
}

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

inline bool isSet(const uint16_t mask, const int n) {
    return mask & (1 << n);
}

inline void setBit(uint16_t& mask, const int n) {
    if (!isSet(mask, n)) {
        int x = 1;
    }
    mask |= (1u << n);
}

bool occluded(const ChunkData* chunk, ChunkNeighbors& neighbors, const int x, const int y, const int z) {
    struct n_idx {
        int x, y, z;
    };

    const std::array<n_idx, 6> n_idxs{{
        {x, y+ 1, z},
        {x, y- 1, z},
        {x + 1, y, z},
        {x - 1, y, z},
        {x, y, z + 1},
        {x, y, z - 1}
    }};

    bool occluded = true;
    for (const auto &[x, y, z] : n_idxs) {
        if (access_safe(chunk, neighbors, x, y, z) == BlockAir) {
            occluded = false;
            break;
        }
    }

    return occluded;
}

void greedy_chunk_voxels(const ChunkData* chunk, const ivec2& chunkPos, ChunkNeighbors& neighbours, std::vector<uint8_t>& voxel_buffer,  size_t* num_voxels) {
    GreedyVoxel v{}; // we need to keep track of the greedy voxel we are currently merging
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
        for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
            // we use a bitmask to check if we have already merged a block together.
            // we only merge slices along the x z plane
            uint16_t merged[CUNK_CHUNK_SIZE] = {};

            for (int x = 0; x < CUNK_CHUNK_SIZE; x++) {
                BlockData previous = BlockInvalid;

                for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                    //std::cout << "x: " << x << ", y: " << y << " z: " << z << std::endl;
                    // block has been merged already so we can skip
                    if (isSet(merged[x], z)) continue;
                    int world_y = y + section * CUNK_CHUNK_SIZE;
                    const BlockData block_data = access_safe(chunk, neighbours, x, world_y, z);

                    if (block_data != BlockAir && !occluded(chunk, neighbours, x, world_y, z)) {
                        // Greedy meshing
                        // we encountered same type, we can merge :D
                        if (block_data == previous) {
                            v.end.z = (z + chunkPos.y * CUNK_CHUNK_SIZE) + 1; // y is our z here

                            // do x-axis merging here, not optimal but will refactor later if needed
                            if (z + 1 >= CUNK_CHUNK_SIZE || access_safe(chunk, neighbours, x, world_y, z + 1) != previous) {
                                for (int localZ = v.start.z - chunkPos.y * CUNK_CHUNK_SIZE;
                                     localZ < v.end.z - chunkPos.y * CUNK_CHUNK_SIZE;
                                     localZ++) {
                                    setBit(merged[x], localZ);
                                }


                                const int localX = v.start.x - chunkPos.x * CUNK_CHUNK_SIZE;
                                int localZ0 = v.start.z - chunkPos.y * CUNK_CHUNK_SIZE;
                                int localZ1 = v.end.z   - chunkPos.y * CUNK_CHUNK_SIZE;
                                int mergeX = localX + 1;

                                while (mergeX < CUNK_CHUNK_SIZE) {
                                    bool ok = true;
                                    for (int mergeZ = localZ0; mergeZ < localZ1; mergeZ++) {
                                        int worldX = mergeX + chunkPos.x * CUNK_CHUNK_SIZE;
                                        int worldZ = mergeZ + chunkPos.y * CUNK_CHUNK_SIZE;

                                        if (isSet(merged[mergeX], mergeZ)) { ok = false; break; }
                                        if (access_safe(chunk, neighbours, worldX, world_y, worldZ) != previous) { ok = false; break; }
                                        if (occluded(chunk, neighbours, worldX, world_y, worldZ)) { ok = false; break; }
                                    }
                                    if (!ok) break;
                                    for (int mergeZ = localZ0; mergeZ < localZ1; mergeZ++) {
                                        setBit(merged[mergeX], mergeZ);
                                    }
                                    v.end.x = v.end.x + 1;
                                    mergeX++;
                                }
                            }
                        } else {
                            if (previous != BlockInvalid) {
                                v.copy_to(voxel_buffer);
                                *num_voxels += 1;
                            }

                            v.start.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
                            v.start.y = world_y;
                            v.start.z = z + chunkPos.y * CUNK_CHUNK_SIZE; // y is our z here
                            v.end = v.start + 1;
                            v.color.x = block_colors[block_data].r;
                            v.color.y = block_colors[block_data].g;
                            v.color.z = block_colors[block_data].b;
                            previous = block_data;
                        }
                    } else if (previous != BlockInvalid) {
                        v.copy_to(voxel_buffer);
                        *num_voxels += 1;
                        previous = BlockInvalid;
                    }
                }
                if (previous != BlockInvalid) {
                    v.copy_to(voxel_buffer);
                    *num_voxels += 1;
                }
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
        if (greedyMeshing) {
            greedy_voxel_buf = std::make_unique<imr::Buffer>(device, voxel_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            greedy_voxel_buf->uploadDataSync(0, voxel_buffer_size, voxel_buffer.data());
        } else {
            voxel_buf = std::make_unique<imr::Buffer>(device, voxel_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            voxel_buf->uploadDataSync(0, voxel_buffer_size, voxel_buffer.data());
        }
    }
}