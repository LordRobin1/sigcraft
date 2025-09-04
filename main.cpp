#include "imr/imr.h"
#include "imr/util.h"

#include "world.h"

#include "nasl/nasl.h"

#include "camera.h"
#include "game.h"

using namespace nasl;

int main(int argc, char** argv) {
    if (argc < 2) return 0;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;
    auto world = World(argv[1]);
    Camera camera = {{30, 141, -12}, {0, 0}, 90};
    std::unique_ptr<Game> game = std::make_unique<GameVoxels>(device, window, swapchain, &world, camera);
    bool voxels = true;

    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        if (game->toggleMode) {
            swapchain.drain();
            delete game.release();
            if (voxels) {
                game = std::make_unique<GameMesh>(device, window, swapchain, &world, camera);
                voxels = false;
                std::cout << "Switched to mesh mode" << std::endl;
            } else {
                game = std::make_unique<GameVoxels>(device, window, swapchain, &world, camera);
                voxels = true;
                std::cout << "Switched to voxel mode" << std::endl;
            }
        }
        game->renderFrame();
    }

    swapchain.drain();
    return 0;
}
