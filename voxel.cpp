#include "voxel.h"

#include <iostream>

extern "C" {
#include "enklume/block_data.h"
}

inline int toWorldY(const int section, const int y) { return y + section * CUNK_CHUNK_SIZE; }

void chunk_voxels(
    const ChunkData* chunk,
    const ivec2& chunkPos,
    ChunkNeighbors& neighbours,
    std::vector<uint8_t>& voxel_buffer,
    size_t* num_voxels,
    const std::unordered_map<BlockId, uint32_t>& idToIdx
) {
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
                        v.position.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
                        v.position.y = world_y;
                        v.position.z = z + chunkPos.y * CUNK_CHUNK_SIZE; // y is our z here
                        v.color.x = block_colors[block_data].r;
                        v.color.y = block_colors[block_data].g;
                        v.color.z = block_colors[block_data].b;
                        v.textureIndex = idToIdx.contains(static_cast<BlockId>(block_data)) ? idToIdx.at(static_cast<BlockId>(block_data)) : idToIdx.at(BlockUnknown);
                        v.copy_to(voxel_buffer);

                        *num_voxels += 1;
                    }
                }
            }
        }
    }
}

void greedyMeshSlice(
    std::array<BitMask, CUNK_CHUNK_SIZE>& maskArray,
    const int y,
    const ChunkData* data,
    const ivec2& chunkPos,
    const int worldY,
    ChunkNeighbors& neighbors,
    std::vector<uint8_t>& voxelBuffer,
    size_t* numVoxels,
    const std::unordered_map<BlockId, uint32_t>& idToIdx
) {
    BitMask& mask = maskArray[y];
    for (int x = 0; x < CUNK_CHUNK_SIZE; x++) {
        // keep greedily meshing until this row has none of this block type left
        while (mask.mask[x] != 0) {
            const int zStart = trailingZeros(mask.mask[x]);
            const uint16_t shifted = mask.mask[x] >> zStart; // push ones down to LSB
            const int zLength = trailingOnes(shifted);
            const int zEnd = zStart + zLength;
            const uint16_t pattern = ((1u << zLength) - 1) << zStart;

            // expand along x-axis
            int xEnd = x + 1;
            while (xEnd < CUNK_CHUNK_SIZE) {
                // check if all blocks to the side are also valid
                if ((pattern & mask.mask[xEnd]) != pattern) break;
                // clear those bits
                mask.mask[xEnd] &= ~pattern;
                xEnd++;
            }

            // expand along y-axis
            int yEnd = y + 1;
            while (yEnd < CUNK_CHUNK_SIZE) {
                BitMask& maskAbove = maskArray[yEnd];
                bool wholeLevelMatches = true;
                // check if all rows on the layer above are a match
                for (int xStart = x; xStart < xEnd; xStart++) {
                    if ((pattern & maskAbove.mask[xStart]) != pattern) {
                        wholeLevelMatches = false;
                        break;
                    }
                }

                // no match => stop checking
                if (!wholeLevelMatches) {
                    break;
                }

                // match, so we clear all used blocks
                for (int xStart = x; xStart < xEnd; xStart++) {
                    maskAbove.mask[xStart] &= ~pattern;
                }
                yEnd++;
            }

            GreedyVoxel gv;
            gv.start.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
            gv.start.y = worldY;
            gv.start.z = zStart + chunkPos.y * CUNK_CHUNK_SIZE;

            gv.end.x = xEnd + chunkPos.x * CUNK_CHUNK_SIZE;
            gv.end.y = worldY + (yEnd - y);
            gv.end.z = zEnd + chunkPos.y * CUNK_CHUNK_SIZE;

            gv.color.x = block_colors[mask.type].r;
            gv.color.y = block_colors[mask.type].g;
            gv.color.z = block_colors[mask.type].b;

            gv.textureIndex = idToIdx.contains(mask.type) ? idToIdx.at(mask.type) : idToIdx.at(BlockUnknown);

            gv.copy_to(voxelBuffer);

            *numVoxels += 1;

            // clear the used bits
            mask.mask[x] &= ~pattern;
        }
    }
}

void greedy_chunk_voxels(
    const ChunkData* chunk,
    const ivec2& chunkPos,
    ChunkNeighbors& neighbours,
    std::vector<uint8_t>& voxel_buffer,
    size_t* num_voxels,
    const std::unordered_map<BlockId, uint32_t>& idToIdx
) {
    std::array<BitMask, CUNK_CHUNK_SIZE> maskArray;
    // generate a BitMask for each block type, and for each vertical slice
    for (int i = 1; i < BlockCount; i++) {
        for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
                maskArray[y] = BitMask(chunk, neighbours, toWorldY(section, y), static_cast<BlockId>(i));
            }
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
                greedyMeshSlice(maskArray, y, chunk, chunkPos, toWorldY(section, y), neighbours, voxel_buffer, num_voxels, idToIdx);
            }
        }
    }
}

ChunkVoxels::ChunkVoxels(
    imr::Device& device,
    ChunkNeighbors& neighbors,
    const ivec2& chunkPos,
    const bool greedyMeshing,
    const std::unordered_map<BlockId, uint32_t>& idToIdx
) {
    std::vector<uint8_t> voxel_buffer;
    num_voxels = 0;
    if (greedyMeshing) {
        greedy_chunk_voxels(neighbors.neighbours[1][1], chunkPos, neighbors, voxel_buffer, &num_voxels,  idToIdx);
    } else {
        chunk_voxels(neighbors.neighbours[1][1], chunkPos, neighbors, voxel_buffer, &num_voxels, idToIdx);
    }
    const size_t voxel_buffer_size = voxel_buffer.size();
    if (voxel_buffer_size > 0) {
        gpu_buffer = std::make_unique<imr::Buffer>(device, voxel_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        gpu_buffer->uploadDataSync(0, voxel_buffer_size, voxel_buffer.data());
    }
}

void ChunkVoxels::update(float delta) {
    loadingAnimation(delta);
    rotateAnimation(delta);
}

void ChunkVoxels::rotateAnimation(float delta) {
    if (is_playing_loading_animation) return;

    const float angle = speed * delta;

    mat4 rotX = rotate_axis_mat4(0, angle * 0.75);
    mat4 rotY = rotate_axis_mat4(1, angle * 0.75f);

    rotation = rotY * rotX * rotation;
}

void ChunkVoxels::loadingAnimation(float delta) {
    if (!is_playing_loading_animation) {
        return;
    }
    animation_progress += delta * speed;
    if (animation_progress >= 1.0f) {
        rotation = identity_mat4;
        radius = 0.5f;
        height_adjust = 0;
        is_playing_loading_animation = false;
        return;
    }
    const float angle = animation_progress * angle_target;
    const float rad = animation_progress * radius_target;
    const float h = height_adjust_start + animation_progress * -height_adjust_start;
    rotation = rotate_axis_mat4(1, angle);
    radius = rad;
    height_adjust = h;
}
