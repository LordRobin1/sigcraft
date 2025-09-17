#include "imr/imr.h"
#include "imr/util.h"

#include "world.h"

#include "nasl/nasl.h"

#include "camera.h"
#include "game.h"

using namespace nasl;

int main(int argc, char** argv) {
    if (argc < 2) return 0;

    const bool staticWorld = true;
    const int staticRenderDistance = 12;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    auto world = World(argv[1]);
    Camera camera = {{30, 141, -12}, {0, 0}, 90};
    bool voxels = true;
    bool greedyVoxels = false;
    std::unique_ptr<Game> game = std::make_unique<GameVoxels>(device, window, swapchain, &world, camera, greedyVoxels);
    if (staticWorld) game->loadChunksInDistance(staticRenderDistance);

    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        if (game->toggleMode) {
            swapchain.drain();
            if (voxels) {
                greedyVoxels = static_cast<GameVoxels*>(&*game)->greedyVoxels;
                delete game.release();
                game = std::make_unique<GameMesh>(device, window, swapchain, &world, camera);
                if (staticWorld) game->loadChunksInDistance(staticRenderDistance);
                voxels = false;
                std::cout << "Switched to mesh mode" << std::endl;
            } else {
                delete game.release();
                game = std::make_unique<GameVoxels>(device, window, swapchain, &world, camera, greedyVoxels);
                if (staticWorld) game->loadChunksInDistance(staticRenderDistance);
                voxels = true;
                std::cout << "Switched to voxel mode" << std::endl;
            }
        }
        game->renderFrame(staticWorld);
    }

    swapchain.drain();
    return 0;
}
