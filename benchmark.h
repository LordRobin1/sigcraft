#ifndef SIGCRAFT_BENCHMARK_H
#define SIGCRAFT_BENCHMARK_H
#include <memory>
#include <vector>

#include "camera.h"
#include "game.h"


class Benchmark {
    Camera& camera;
    std::shared_ptr<CameraInput> camera_input = std::make_shared<CameraInput>();
    std::shared_ptr<CameraFreelookState> camera_state = std::make_shared<CameraFreelookState>(50.0f, 1);

    std::vector<std::pair<vec3, vec2>> position_yaw_pitch;
    size_t current_playback_frame = 0;

    GLFWwindow* window;
    std::unique_ptr<Game> game = nullptr;

    bool mousemove_done = true;
    bool saved_last_move = false;

    bool record = false;

    void record_frame(bool overrule = false);

    void play_next_frame();

    void read_from_file();

    void write_to_file();
public:
    Benchmark(
        Camera& camera,
        imr::Device& device,
        GLFWwindow* window,
        imr::Swapchain& swapchain,
        World* world
    );

    void recording_session();
    void playback_session();
};


#endif //SIGCRAFT_BENCHMARK_H