#ifndef FRUSTUM_H
#define FRUSTUM_H
#include "voxel.h"
#include "nasl/nasl_mat.h"
#include "nasl/nasl_vec.h"

using namespace nasl;

// https://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
// https://zeux.io/2009/01/31/view-frustum-culling-optimization-introduction/
// https://fgiesen.wordpress.com/2010/10/17/view-frustum-culling/
struct Frustum {
    vec4 planes[5]{}; // left, right, bottom, top, near

    explicit Frustum(const mat4& perspective) {
        planes[0] = perspective.rows[3] + perspective.rows[0]; // left
        planes[1] = perspective.rows[3] - perspective.rows[0]; // right
        planes[2] = perspective.rows[3] + perspective.rows[1]; // bottom
        planes[3] = perspective.rows[3] - perspective.rows[1]; // top
        planes[4] = perspective.rows[3] + perspective.rows[2]; // near
    }

    [[nodiscard]] bool isInside(const AABB& aabb) const {
        // get aabb points
        vec4 points[8] =
        {
            { aabb.min.x, aabb.min.y, aabb.min.z, 1.0 },
            { aabb.max.x, aabb.min.y, aabb.min.z, 1.0 },
            { aabb.min.x, aabb.max.y, aabb.min.z, 1.0 },
            { aabb.max.x, aabb.max.y, aabb.min.z, 1.0 },
            { aabb.min.x, aabb.min.y, aabb.max.z, 1.0 },
            { aabb.max.x, aabb.min.y, aabb.max.z, 1.0 },
            { aabb.min.x, aabb.max.y, aabb.max.z, 1.0 },
            { aabb.max.x, aabb.max.y, aabb.max.z, 1.0 }
        };

        // for each plane...
        for (auto& plane: planes) {
            bool inside = false;

            for (auto point: points) {
                if (dot(point, plane) > 0) {
                    inside = true;
                    break;
                }
            }

            if (!inside) {
                return false;
            }
        }

        return true;
    }
};

#endif //FRUSTUM_H
