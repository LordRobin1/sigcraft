#ifndef FRUSTUM_H
#define FRUSTUM_H
#include "nasl/nasl_mat.h"
#include "nasl/nasl_vec.h"
#include "aabb.h"

using namespace nasl;

// https://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
// https://zeux.io/2009/01/31/view-frustum-culling-optimization-introduction/
// https://fgiesen.wordpress.com/2010/10/17/view-frustum-culling/
// https://bruop.github.io/frustum_culling/
// https://iquilezles.org/articles/frustumcorrect/
struct Frustum {
    vec4 planes[5]{}; // left, right, bottom, top, near
    vec3 corners[8]{};

    explicit Frustum(const mat4& perspective, mat4& invPerspective) {
        planes[0] = perspective.rows[3] + perspective.rows[0]; // left
        planes[1] = perspective.rows[3] - perspective.rows[0]; // right
        planes[2] = perspective.rows[3] + perspective.rows[1]; // bottom
        planes[3] = perspective.rows[3] - perspective.rows[1]; // top
        planes[4] = perspective.rows[3] + perspective.rows[2]; // near

        constexpr vec3 ndc[8] = {
            {-1, -1, -1},
            {1, -1, -1},
            {-1, 1, -1},
            {1, 1, -1},
            {-1, -1, 1},
            {1, -1, 1},
            {-1, 1, 1},
            {1, 1, 1}
        };

        for (int i = 0; i < 8; i++) {
            corners[i] = (invPerspective * vec4{ndc[i], 1.0f}).xyz;
        }
    }

private:
    // Signed distance field to represent the planes (clip space)
    // f(p) <= 0 is inside for each plane
    static float distRight(const vec4& p) { return  p.x - p.w; }  // x <= w
    static float distLeft(const vec4& p) { return -p.x - p.w; }   // x >= -w
    static float distTop(const vec4& p) { return  p.y - p.w; }    // y <= w
    static float distBottom(const vec4& p) { return -p.y - p.w; } // y >= -w
    static float distNear(const vec4& p) { return -p.z - p.w; }   // z >= -w

    static bool insideAllPlanes(const vec4& p) {
        // Epsilon for numerical robustness, otherwise flickering occurs
        // Relative epsilon (scales with |w|), constant epsilon actually does not work
        const float eps = 1e-6f * std::max(std::abs(p.w), 1.0f);

        if (distLeft(p) > eps) return false;
        if (distRight(p) > eps) return false;
        if (distBottom(p) > eps) return false;
        if (distTop(p) > eps) return false;
        if (distNear(p) > eps) return false;
        return true;
    }

public:
    [[nodiscard]] bool isInside(const AABB& aabb, mat4& perspective) const {
        // get aabb points
        vec4 cpPoints[8] =
        {
            perspective * vec4{ aabb.min.x, aabb.min.y, aabb.min.z, 1.0 },
            perspective * vec4{ aabb.max.x, aabb.min.y, aabb.min.z, 1.0 },
            perspective * vec4{ aabb.min.x, aabb.max.y, aabb.min.z, 1.0 },
            perspective * vec4{ aabb.max.x, aabb.max.y, aabb.min.z, 1.0 },
            perspective * vec4{ aabb.min.x, aabb.min.y, aabb.max.z, 1.0 },
            perspective * vec4{ aabb.max.x, aabb.min.y, aabb.max.z, 1.0 },
            perspective * vec4{ aabb.min.x, aabb.max.y, aabb.max.z, 1.0 },
            perspective * vec4{ aabb.max.x, aabb.max.y, aabb.max.z, 1.0 }
        };

        // check if aabb is outside/inside frustum
        bool inside = false;
        for (auto point: cpPoints) {
            inside |= insideAllPlanes(point);
        }

        if (inside) {
            return true;
        }

        // check if frustum is outside/inside aabb (see link 5 above)
        int out;
        out = 0; for (auto corner: corners) out += ((corner.x > aabb.max.x) ? 1 : 0); if(out == 8) return false;
        out = 0; for (auto corner: corners) out += ((corner.x < aabb.min.x) ? 1 : 0); if(out == 8) return false;
        out = 0; for (auto corner: corners) out += ((corner.y > aabb.max.y) ? 1 : 0); if(out == 8) return false;
        out = 0; for (auto corner: corners) out += ((corner.y < aabb.min.y) ? 1 : 0); if(out == 8) return false;
        out = 0; for (auto corner: corners) out += ((corner.z > aabb.max.z) ? 1 : 0); if(out == 8) return false;
        out = 0; for (auto corner: corners) out += ((corner.z < aabb.min.z) ? 1 : 0); if(out == 8) return false;

        return true;
    }
};

#endif //FRUSTUM_H
