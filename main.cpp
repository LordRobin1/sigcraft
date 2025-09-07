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
    std::mutex device_mutex;
    ThreadPool tp(std::thread::hardware_concurrency());
    auto world = World(argv[1]);
    Camera camera = {{30, 141, -12}, {0, 0}, 90};
    bool voxels = true;
    bool greedyVoxels = false;
    std::unique_ptr<Game> game = std::make_unique<GameVoxels>(device, window, swapchain, &world, camera, greedyVoxels, device_mutex, tp);

    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        if (game->toggleMode) {
            swapchain.drain();
            if (voxels) {
                greedyVoxels = static_cast<GameVoxels*>(&*game)->greedyVoxels;
                delete game.release();
                game = std::make_unique<GameMesh>(device, window, swapchain, &world, camera, device_mutex, tp);
                voxels = false;
                std::cout << "Switched to mesh mode" << std::endl;
            } else {
                delete game.release();
                game = std::make_unique<GameVoxels>(device, window, swapchain, &world, camera, greedyVoxels, device_mutex, tp);
                voxels = true;
                std::cout << "Switched to voxel mode" << std::endl;
            }
        }
        std::scoped_lock<std::mutex> device_lock(device_mutex);
        game->renderFrame();
    }

    swapchain.drain();
    return 0;
}
