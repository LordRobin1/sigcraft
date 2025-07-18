#pragma once

#include <stddef.h>
#include <math.h>

#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

using namespace nasl;

typedef struct {
    vec3 position;
    struct {
        float yaw, pitch;
    } rotation;
    float fov;
} Camera;

vec3 camera_get_forward_vec(const Camera* cam, vec3 forward = vec3(0, 0, -1));
vec3 camera_get_right_vec(const Camera*);
// Returns perspective projection matrix
mat4 camera_get_view_mat4(const Camera*, size_t, size_t);

typedef struct {
    float fly_speed, mouse_sensitivity;
    double last_mouse_x, last_mouse_y;
    unsigned mouse_was_held: 1;
} CameraFreelookState;

typedef struct {
    bool mouse_held;
    bool should_capture;
    double mouse_x, mouse_y;
    struct {
        bool forward, back, left, right;
    } keys;
} CameraInput;

bool camera_move_freelook(Camera*, CameraInput*, CameraFreelookState*, float);

inline vec2 camera_scale_from_hfov(float fov, float aspect) {
    float sw = tanf(fov * 0.5f);
    float sh = sw / aspect;
    return vec2(sw, sh);
}
