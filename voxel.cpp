#include "voxel.h"
#include <algorithm>
#include <span>

extern "C" {
#include "enklume/block_data.h"
}

void chunk_voxels(const ChunkData* chunk, const ivec2& chunkPos, ChunkNeighbors& neighbours, std::vector<uint8_t>& voxel_buffer, std::vector<uint8_t>& vert_buffer, size_t* num_voxels, size_t* num_verts) {
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
                        v.position.x = x + chunkPos.x * CUNK_CHUNK_SIZE;
                        v.position.y = world_y;
                        v.position.z = z + chunkPos.y * CUNK_CHUNK_SIZE; // y is our z here
                        v.color.x = block_colors[block_data].r;
                        v.color.y = block_colors[block_data].g;
                        v.color.z = block_colors[block_data].b;
                        v.copy_to(voxel_buffer);
                        std::vector<uint8_t> bb_verts = v.generate_bb_verts();
                        vert_buffer.insert(vert_buffer.end(), bb_verts.begin(), bb_verts.end());
                        *num_voxels += 1;
                        *num_verts += bb_verts.size() / sizeof(ChunkVoxels::Vertex);
                    }
                }
            }
        }
    }
}

ChunkVoxels::ChunkVoxels(imr::Device& device, ChunkNeighbors& neighbors, const ivec2& chunkPos) {
    std::vector<uint8_t> voxel_buffer;
    std::vector<uint8_t> vert_buffer;
    num_voxels = 0, num_verts = 0;
    chunk_voxels(neighbors.neighbours[1][1], chunkPos, neighbors, voxel_buffer, vert_buffer, &num_voxels, &num_verts);

    size_t voxel_buffer_size = voxel_buffer.size();// * sizeof(uint8_t);
    size_t vert_buffer_size = vert_buffer.size();// * sizeof(uint8_t);

    if (voxel_buffer_size > 0) {
        voxel_buf = std::make_unique<imr::Buffer>(device, voxel_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        vert_buf = std::make_unique<imr::Buffer>(device, vert_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        voxel_buf->uploadDataSync(0, voxel_buffer_size, voxel_buffer.data());
        vert_buf->uploadDataSync(0, vert_buffer_size, vert_buffer.data());
    }
}

std::vector<uint8_t> Voxel::generate_bb_verts() const {
    int16_t z = 1;
    // bounding box vertices span the whole screen
    std::array<std::tuple<int16_t, int16_t, int16_t>, 6> bb_verts{{
        {-1, -1, z}, {1, -1, z}, {1, 1, z},
        {-1, -1, z}, {1, 1, z}, {-1, 1, z},
    }};

    std::vector<uint8_t> buf;
    for (auto [x, y, z] : bb_verts) {
        ChunkVoxels::Vertex v = { x, y, z };
        v.copy_to(buf);
    }
    return buf;
}
