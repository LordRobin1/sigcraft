#ifndef GAME_H
#define GAME_H
#include <iostream>
#include <GLFW/glfw3.h>

#include "camera.h"
#include "world.h"
#include "shaders.h"
#include "imr/util.h"
#include "texture.hpp"

constexpr size_t RENDER_DISTANCE = 16;
using KeyCallback = void(*)(GLFWwindow*, int, int, int, int);

struct Game {
protected:
    imr::Device& device;
    vkb::DispatchTable& vk = device.dispatch;
    GLFWwindow* window;
    imr::Swapchain& swapchain;
    std::unique_ptr<imr::Image> depthBuffer;
    Shaders shaders;
    World* world;
    Camera& camera;
    CameraFreelookState camera_state = {
        .fly_speed = 100.0f,
        .mouse_sensitivity = 1,
    };
    CameraInput camera_input = {};
    bool reload_shaders = false;
    uint64_t prev_frame = imr_get_time_nano();
    float delta = 0;

    Game(imr::Device& device, GLFWwindow* window, imr::Swapchain& swapchain, Shaders shaders, World* world, Camera& camera)
        : device(device), window(window), swapchain(swapchain), shaders(std::move(shaders)), world(world), camera(camera)
    {}

public:
    bool toggleMode = false;
    virtual ~Game() = default;

    virtual void renderFrame(bool staticWorld) = 0;
    virtual void loadChunksInDistance(int renderDistance) = 0;
};


struct GameVoxels final : Game {
private:
    Sampler sampler{device};
    TextureManager textureManager{device, *shaders.pipeline, sampler};
    bool toggleGreedy = false;
    int debugShader = 0;
    bool texturesEnabled = true;
    bool rotateVoxels = false;
    std::vector<std::string> vertexShaders = { "voxel.vert.spv", "greedyVoxel.vert.spv" };
    std::vector<std::string> fragmentShaders = {"voxel.frag.spv", "visualize_billboards.frag.spv", "outline_billboards.frag.spv"};
    struct {
        mat4 matrix;
        mat4 inverse_matrix;
        vec3 camera_position;
        VkDeviceAddress voxel_buffer;
        vec2 screen_size;
        bool textures;
        mat4 rotation;
        float radius;
        float height_adjust;
    } push_constants;

public:
    GameVoxels(imr::Device &device, GLFWwindow *window, imr::Swapchain &swapchain, World *world, Camera &camera, bool greedyVoxels);
    void renderFrame(bool staticWorld) override;
    void loadChunksInDistance(int renderDistance) override;
    bool greedyVoxels;
};

struct GameMesh final : Game {
private:
    struct {
        mat4 matrix;
        ivec3 chunk_position;
        float time;
    } push_constants;

public:
    GameMesh(imr::Device& device, GLFWwindow* window, imr::Swapchain& swapchain, World* world, Camera& camera);
    void renderFrame(bool staticWorld) override;
    void loadChunksInDistance(int renderDistance) override;
};


#endif //GAME_H