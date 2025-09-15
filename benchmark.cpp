#include "benchmark.h"
#include "slog.hpp"

#include <iostream>
#include <filesystem>

Benchmark::Benchmark(
    Camera& camera,
    imr::Device& device,
    GLFWwindow* window,
    imr::Swapchain& swapchain,
    World* world
) : camera(camera),
    window(window),
    game(std::make_unique<GameVoxels>(device, window, swapchain, world, camera, camera_input, camera_state)) {
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* benchmark = static_cast<Benchmark*>(glfwGetWindowUserPointer(window));
        if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
            benchmark->record = !benchmark->record;
            if (benchmark->record) {
                nfo("Benchmark recording started");
            } else {
                nfo("Benchmark recording stopped");
            }
        } else if (key == GLFW_KEY_F8 && action == GLFW_PRESS) {
            benchmark->read_from_file();
            nfo("Benchmark playback started");
        }
    });
}

void Benchmark::recording_session() {
    imr::FpsCounter fps_counter;

    bool started = false;
    // record initial position
    record_frame(true);
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        game->renderFrame();
        if (record) {
            record_frame(!started);
            started = true;
        }
        if (!record && started) {
            record_frame(true);
            started = false;
        }
    }
    write_to_file();
}

void Benchmark::record_frame(const bool overrule) {
    // record a new position + rotation if the direction was changed
    const bool mouse_moved = camera_input->mouse_held && camera_state->mouse_was_held;
    mousemove_done = !mouse_moved;
    saved_last_move = mousemove_done ? saved_last_move : false;

    if ((mousemove_done && !saved_last_move) || overrule) {
        saved_last_move = true;
        position_yaw_pitch.emplace_back(camera.position, vec2(camera.rotation.yaw, camera.rotation.pitch));
    }
}

void Benchmark::playback_session() {
    imr::FpsCounter fps_counter;

    read_from_file();
    if (position_yaw_pitch.empty()) {
        err("No benchmark data to play back");
        return;
    }
    nfo("Loaded {} benchmark frames", position_yaw_pitch.size());

    // calculate total distance
    std::vector<float> distances;
    float total_distance = 0.0f;
    std::vector<float> yaw_diffs;
    float abs_yaw_diff = 0.0f;
    std::vector<float> pitch_diffs;
    float abs_pitch_diff = 0.0f;
    for (size_t i = 1; i < position_yaw_pitch.size(); i++) {
        vec3 dir = vec3_sub(position_yaw_pitch[i].first, position_yaw_pitch[i - 1].first);
        float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        distances.push_back(dist);
        total_distance += dist;
        float yaw_diff = position_yaw_pitch[i].second.x - position_yaw_pitch[i - 1].second.x;
        yaw_diffs.push_back(yaw_diff);
        abs_yaw_diff += std::abs(yaw_diff);
        float pitch_diff = position_yaw_pitch[i].second.y - position_yaw_pitch[i - 1].second.y;
        pitch_diffs.push_back(pitch_diff);
        abs_pitch_diff += std::abs(pitch_diff);
    }

    // very wrong impl ahead
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        play_next_frame();
        game->renderFrame();
    }
}

void Benchmark::play_next_frame() {
    if (current_playback_frame >= position_yaw_pitch.size()) {
        nfo("Benchmark playback finished");
        return;
    }

    camera.position = position_yaw_pitch[current_playback_frame].first;
    camera.rotation.yaw = position_yaw_pitch[current_playback_frame].second.x;
    camera.rotation.pitch = position_yaw_pitch[current_playback_frame].second.y;

    current_playback_frame = (current_playback_frame + 1) % position_yaw_pitch.size();
}

void Benchmark::read_from_file()  {
    const auto dirpath = std::filesystem::path("../benchmarks");
    if (!std::filesystem::exists(dirpath)) {
        std::filesystem::create_directory(dirpath);
    }
    const auto filepath = dirpath / "benchmark.txt";
    FILE* file = std::fopen(filepath.c_str(), "r");
    if (!file) {
        err("Could not open {}", filepath.string());
        return;
    }
    float x, y, z, yaw, pitch;
    if (std::fscanf(file, " %f %f %f %f %f", &x, &y, &z, &yaw, &pitch) == 5) {
        position_yaw_pitch.emplace_back(vec3{x, y, z}, vec2{yaw, pitch});
    }
    std::fclose(file);
}


void Benchmark::write_to_file() {
    const auto dirpath = std::filesystem::path("../benchmarks");
    if (!std::filesystem::exists(dirpath)) {
        std::filesystem::create_directory(dirpath);
    }
    const auto filepath = dirpath / "benchmark.txt";
    FILE* file = std::fopen(filepath.c_str(), "w");
    if (!file) {
        err("Could not open {}", filepath.string());
        return;
    }
    for (const auto& [pos, rot]: position_yaw_pitch) {
        dbg("Writing: {} {} {} {} {}", static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z),
            static_cast<float>(rot.x), static_cast<float>(rot.y));
        std::fprintf(file, "%f %f %f %f %f\n", static_cast<float>(pos.x), static_cast<float>(pos.y),
                     static_cast<float>(pos.z), static_cast<float>(rot.x), static_cast<float>(rot.y));
    }
    std::fclose(file);
    nfo("Successfully written benchmark file at {}", filepath.string());
}
