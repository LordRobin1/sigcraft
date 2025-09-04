#ifndef AABB_H
#define AABB_H
#include "nasl/nasl_vec.h"
using namespace nasl;
// Axis-aligned bounding box for Box-Plane intersection
struct AABB {
    // Empty by default
    const vec3 min = vec3(1);
    const vec3 max = vec3(-1);

    AABB(const vec3& min, const vec3& max) : min(min), max(max) {}
};
#endif //AABB_H
