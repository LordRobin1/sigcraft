#ifndef VOXEL_H
#define VOXEL_H

#include "nasl/nasl.h"
#include <vector>
#include <cstdint>
#include "BitMask.hpp"

#include "chunk_mesh.h"
#include "nasl/nasl_mat.h"

using namespace nasl;

struct Voxel {
    ivec3 position = ivec3{0, 0, 0};
    vec3 color = vec3{1.0f, 0.0f, 1.0f};
    uint32_t textureIndex = -1;

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
    uint32_t textureIndex;

    void copy_to(std::vector<uint8_t>& buf) const {
        uint8_t tmp[sizeof(GreedyVoxel)];
        memcpy(tmp, this, sizeof(GreedyVoxel));
        for (auto b : tmp)
            buf.push_back(b);
    }
};


struct ChunkVoxels {
    std::unique_ptr<imr::Buffer> gpu_buffer;
    size_t num_voxels;

    /// how much we want to rotate in total
    static constexpr float angle_target = M_PI * 2.0f;
    /// initial -angle_goal rotation, so we are at identity_mat4 when the animation is done
    mat4 rotation = rotate_axis_mat4(1, -angle_target);
    /// initial radius
    float radius = 0.35f;
    /// how far we want to expand in total
    static constexpr float radius_target = 0.5f;
    /// 0.0 to 1.0, how far we are through the animation
    float animation_progress = 0.0f;
    /// animation speed multiplier; animation will take roughly 1/speed seconds
    static constexpr float speed = 1.5f;
    /// whether we are still playing the loading animation
    bool is_playing_loading_animation = true;
    /// where the blocks start in the Y axis
    static constexpr float height_adjust_start = 20.0f;
    /// Gives current adjusted height based on animation progress
    float height_adjust = height_adjust_start;

    float loop_height_amplitude = 3.0f;
    float loop_radius_amplitude = 0.05f;
    double sine_time = 0.0;

    ChunkVoxels(
        imr::Device& device,
        ChunkNeighbors& neighbors,
        const ivec2& chunkPos,
        bool greedyMeshing,
        const std::unordered_map<BlockId, uint32_t>& idToIdx
    );

    [[nodiscard]] VkDeviceAddress voxel_buffer_device_address() const {
        return gpu_buffer->device_address();
    }

    void update(float delta);

    void loop_update(float delta, vec2 chunk_pos);
};


#endif
