#ifndef GAME_H
#define GAME_H
#include <iostream>
#include <GLFW/glfw3.h>

#include "camera.h"
#include "world.h"
#include "shaders.h"
#include "imr/util.h"

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

    virtual void renderFrame() = 0;
};


struct GameVoxels final : Game {
private:
    bool toggleGreedy = false;
    int debugShader = 0;
    std::vector<std::string> vertexShaders = { "voxel.vert.spv", "greedyVoxel.vert.spv" };
    std::vector<std::string> fragmentShaders = {"voxel.frag.spv", "visualize_billboards.frag.spv", "outline_billboards.frag.spv"};
    struct {
        VkDeviceAddress voxel_buffer;
        VkDeviceAddress ubo;
    } push_constants;
    struct UBO {
        struct {
            mat4 projection;
            mat4 inverse_projection;
            vec3 camera_position;
            vec2 screen_size;
        } data = {};
        imr::Buffer buffer;

        explicit UBO(imr::Device &device)
            : buffer(imr::Buffer(device, sizeof(data), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
               {}
        void update(imr::Device& d) {
            buffer.ubo_upload(&data, sizeof(data));
        }
    } ubo;

public:
    GameVoxels(imr::Device &device, GLFWwindow *window, imr::Swapchain &swapchain, World *world, Camera &camera, bool greedyVoxels);
    void renderFrame() override;
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
    void renderFrame() override;
};


#endif //GAME_H