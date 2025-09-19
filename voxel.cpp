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

inline bool canExpandX(
    const std::array<BitMask, CUNK_CHUNK_SIZE>& maskArray,
    const int xPos,
    const int yPos,
    const int zPos,
    const int xDist,
    const int yDist,
    const int zDist
) {
    for (int y = yPos; y < yPos + yDist; y++) {
        const BitMask& mask = maskArray[y];
        for (int x = xPos + xDist; x < xPos + xDist + 1; x++) {
            for (int z = zPos; z < zPos + zDist; z++) {
                if (x >= CUNK_CHUNK_SIZE || (mask.mask[x] & (1u << z)) == 0)
                    return false;
            }
        }
    }
    return true;
}

inline bool canExpandZ(
    const std::array<BitMask, CUNK_CHUNK_SIZE>& maskArray,
    const int xPos,
    const int yPos,
    const int zPos,
    const int xDist,
    const int yDist,
    const int zDist
) {
    for (int y = yPos; y < yPos + yDist; y++) {
        const BitMask& mask = maskArray[y];
        for (int x = xPos; x < xPos + xDist; x++) {
            if ((mask.mask[x] & (1u << (zPos + zDist))) == 0)
                return false;
        }
    }
    return true;
}

inline bool canExpandY(
    const std::array<BitMask, CUNK_CHUNK_SIZE>& maskArray,
    const int xPos,
    const int yPos,
    const int zPos,
    const int xDist,
    const int yDist,
    const int zDist
) {
    if (yPos + yDist >= CUNK_CHUNK_SIZE) return false;

    const BitMask& maskAbove = maskArray[yPos + yDist];
    for (int x = xPos; x < xPos + xDist; x++) {
        for (int z = zPos; z < zPos + zDist; z++) {
            if ((maskAbove.mask[x] & (1u << z)) == 0)
                return false;
        }
    }
    return true;
}

void tryExpandX() {

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
    for (int blockType = 1; blockType < BlockCount; blockType++) {
        for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
                maskArray[y] = BitMask(chunk, neighbours, toWorldY(section, y), static_cast<BlockId>(blockType));
            }

            for (int y = 0; y < CUNK_CHUNK_SIZE; y++) {
                for (int x = 0; x < CUNK_CHUNK_SIZE; x++) {
                    for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                        if ((maskArray[y].mask[x] & (1u << z)) == 0) continue;

                        int xDist = 1, yDist = 1, zDist = 1;

                        bool expanded = true;
                        while (expanded) {
                            expanded = false;
                            if (canExpandX(maskArray, x, y, z, xDist, yDist, zDist)) {
                                xDist++;
                                expanded = true;
                            }
                            if (canExpandZ(maskArray, x, y, z, xDist, yDist, zDist)) {
                                zDist++;
                                expanded = true;
                            }
                            if (canExpandY(maskArray, x, y, z, xDist, yDist, zDist)) {
                                yDist++;
                                expanded = true;
                            }
                        }

                        GreedyVoxel gv;
                        gv.start.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
                        gv.start.y = toWorldY(section, y);
                        gv.start.z = z + chunkPos.y * CUNK_CHUNK_SIZE;

                        gv.end.x = x + xDist + chunkPos.x * CUNK_CHUNK_SIZE;
                        gv.end.y = toWorldY(section, y) + yDist;
                        gv.end.z = z + zDist + chunkPos.y * CUNK_CHUNK_SIZE;

                        gv.color.x = block_colors[static_cast<BlockId>(blockType)].r;
                        gv.color.y = block_colors[static_cast<BlockId>(blockType)].g;
                        gv.color.z = block_colors[static_cast<BlockId>(blockType)].b;

                        gv.textureIndex = idToIdx.contains(static_cast<BlockId>(blockType)) ?
                                          idToIdx.at(static_cast<BlockId>(blockType)) :
                                          idToIdx.at(BlockUnknown);

                        gv.copy_to(voxel_buffer);
                        (*num_voxels)++;

                        // consume useb bits
                        for (int yy = y; yy < y + yDist; yy++)
                            for (int xx = x; xx < x + xDist; xx++)
                                for (int zz = z; zz < z + zDist; zz++)
                                    maskArray[yy].mask[xx] &= ~(1u << zz);
                    }
                }
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
