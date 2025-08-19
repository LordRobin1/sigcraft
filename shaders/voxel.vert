#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

struct Voxel { ivec3 position; vec3 color; };

//layout(location = 0) in ivec3 vertexIn;
//layout(location = 1) in vec3 normalIn;
//layout(location = 2) in vec3 colorIn;

layout(scalar, buffer_reference) readonly buffer VoxelBuffer {
    Voxel voxels[];
};
layout(scalar, push_constant) uniform T {
    mat4 proj_view_mat;
    mat4 inverse_proj_view_matrix;
    vec3 camera_position;
    VoxelBuffer voxel_buffer;
    vec2 screen_size;
} push_constants;

layout(location = 0) out Box box;
layout(location = 6) out vec3 color;
layout(location = 7) out vec3 cameraPosition;
layout(location = 8) out vec2 screenSize;
layout(location = 9) out mat4 inverseProjViewMatrix;
layout(location = 13) out vec2 quad;

// gl_InstanceIndex => AABB-corner
// 0 => bottom-left
// 1 => bottom-right
// 2 => top-right
// 3 => bottom-left
// 4 => top-right
// 5 => top-left
const vec2 fullscreenVerts[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void quadricProj(in vec3 osPosition, in mat4 objectToScreenMatrix, in vec2 halfScreenSize, inout vec4 position, inout float pointSize) {
    const vec4 quadricMat = vec4(1.0, 1.0, 1.0, -1.0); // x^2 + y^2 + z^2 - r^2 = 0
    // float sphereRadius = voxelSize * 1.732051; // uncomment if size becomes variable
    float sphereRadius = 0.86602540378; // 1.732051;
    vec4 sphereCenter = vec4(osPosition.xyz, 1.0);
    mat4 modelViewProj = transpose(objectToScreenMatrix);

    mat3x4 matT = mat3x4(mat3(modelViewProj[0].xyz, modelViewProj[1].xyz, modelViewProj[3].xyz) * sphereRadius);
    matT[0].w = dot(sphereCenter, modelViewProj[0]);
    matT[1].w = dot(sphereCenter, modelViewProj[1]);
    matT[2].w = dot(sphereCenter, modelViewProj[3]);

    mat3x4 matD = mat3x4(matT[0] * quadricMat, matT[1] * quadricMat, matT[2] * quadricMat);
    vec4 eqCoefs =
        vec4(dot(matD[0], matT[2]), dot(matD[1], matT[2]), dot(matD[0], matT[0]), dot(matD[1], matT[1]))
        / dot(matD[2], matT[2]);

    vec4 outPosition = vec4(eqCoefs.x, eqCoefs.y, 0.0, 1.0);
    vec2 AABB = sqrt(eqCoefs.xy * eqCoefs.xy - eqCoefs.zw);
    AABB *= halfScreenSize * 2.0f;

    position.xy = outPosition.xy * position.w;
    pointSize = max(AABB.x, AABB.y);
}


// Signed distance field to represent the planes (clip space)
// f(p) <= 0 is inside for each plane
float distRight(vec4 p) { return  p.x - p.w; }  // x <= w
float distLeft(vec4 p) { return -p.x - p.w; }   // x >= -w
float distTop(vec4 p) { return  p.y - p.w; }    // y <= w
float distBottom(vec4 p) { return -p.y - p.w; } // y >= -w
float distNear(vec4 p) { return -p.z - p.w; }   // z >= -w

bool insideAllPlanes(vec4 p) {
    // Epsilon for numerical robustness, otherwise flickering occurs
    // Relative epsilon (scales with |w|), constant epsilon actually does not work
    float eps = 1e-6 * max(abs(p.w), 1.0);

    if ( distLeft(p) > eps) return false;
    if ( distRight(p) > eps) return false;
    if ( distBottom(p) > eps) return false;
    if ( distTop(p) > eps) return false;
    if ( distNear(p) > eps) return false;
    return true;
}

// See https://www.cs.ucr.edu/~shinar/courses/cs130-winter-2021/content/clipping.pdf
// and the above distance functions as explanation for this
// Edge: a--b, dA = dist(a), dB = dist(b)
vec4 intersectPlane(vec4 a, vec4 b, float dA, float dB) {
    float t = dA / (dA - dB);
    return mix(a, b, clamp(t, 0.0, 1.0));
}

// Plane distance function array (function pointers not allowed; use switch)
float planeDist(int pid, vec4 p) {
    if (pid == 0) return distLeft(p);
    if (pid == 1) return distRight(p);
    if (pid == 2) return distBottom(p);
    if (pid == 3) return distTop(p);
    if (pid == 4) return distNear(p);
    return -1; // unreachable
}

// Compute AABB of the voxel clipped by the frustum
void computeClippedAABB(
    vec3 wsCenter,
    vec3 radius,
    mat4 projView,
    out vec2 ndcMin,
    out vec2 ndcMax
) {
    // Corner points in clip space
    vec4 C[8];
    {
        vec3 mn = wsCenter - radius;
        vec3 mx = wsCenter + radius;
        C[0] = projView * vec4(mn.x, mn.y, mn.z, 1.0);
        C[1] = projView * vec4(mn.x, mn.y, mx.z, 1.0);
        C[2] = projView * vec4(mn.x, mx.y, mn.z, 1.0);
        C[3] = projView * vec4(mn.x, mx.y, mx.z, 1.0);
        C[4] = projView * vec4(mx.x, mn.y, mn.z, 1.0);
        C[5] = projView * vec4(mx.x, mn.y, mx.z, 1.0);
        C[6] = projView * vec4(mx.x, mx.y, mn.z, 1.0);
        C[7] = projView * vec4(mx.x, mx.y, mx.z, 1.0);
    }
    // 12 edges by corner indices
    const ivec2 BOX_EDGES[12] = ivec2[12](
        ivec2(0,1), ivec2(0,2), ivec2(0,4),
        ivec2(1,3), ivec2(1,5),
        ivec2(2,3), ivec2(2,6),
        ivec2(3,7),
        ivec2(4,5), ivec2(4,6),
        ivec2(5,7),
        ivec2(6,7)
    );

    // upper bound on candidates: 8 corners + (12 edges * 5 planes) => at most 68;
    const int MAX_CAND = 72;
    vec4 cand[MAX_CAND];
    int  count = 0;

    // add corners that are inside all planes
    for (int i = 0; i < 8; ++i) {
        if (insideAllPlanes(C[i])) {
            cand[count++] = C[i];
        }
    }

    // add edge/plane intersections
    const int PLANES = 5; // left, right, bottom, top, near
    for (int edge = 0; edge < 12; edge++) {
        vec4 A = C[BOX_EDGES[edge].x];
        vec4 B = C[BOX_EDGES[edge].y];

        for (int plane = 0; plane < PLANES; plane++) {
            float dA = planeDist(plane, A);
            float dB = planeDist(plane, B);

            const float eps = 1e-7;
            // const bool oppositeSides = (dA < -eps && dB > eps) || (dA > eps && dB < -eps);
            if (dA * dB < 0) {
                vec4 its = intersectPlane(A, B, dA, dB);
                if (insideAllPlanes(its)) {
                    cand[count++] = its;
                }
            }
        }
    }

    // if completely clipped, return empty aabb
    if (count == 0) {
        ndcMin = vec2(1e9);
        ndcMax = vec2(-1e9);
        return;
    }

    vec2 mn = vec2(1e9);
    vec2 mx = vec2(-1e9);
    for (int i = 0; i < count; i++) {
        vec2 p = cand[i].xy / cand[i].w; // perspective divide to NDC
        mn = min(mn, p);
        mx = max(mx, p);
    }

    ndcMin = mn;
    ndcMax = mx;
}


void main() {
    const float radius = 0.5;
    const float invRadius = 2;
    const float CLIPPING_THRESHOLD = 200.0;

    Voxel voxel = push_constants.voxel_buffer.voxels[gl_InstanceIndex];
    vec2 corner = fullscreenVerts[gl_VertexIndex];

    vec4 position = push_constants.proj_view_mat * vec4(voxel.position, 1.0);
    float pointSize;
    quadricProj(vec3(voxel.position), push_constants.proj_view_mat, push_constants.screen_size * 0.5, position, pointSize);

    vec2 screenOffset = corner * (pointSize / push_constants.screen_size);
    position.xy += screenOffset * position.w;

    // check if we need to compute the AABB greedily
    if (pointSize * 2.0 > CLIPPING_THRESHOLD) {
        vec2 ndcMin, ndcMax;
        computeClippedAABB(vec3(voxel.position), vec3(radius), push_constants.proj_view_mat, ndcMin, ndcMax);

        // If completely clipped, return early
        // This should only be the case, if we do chunk-only/no frustum culling, so some voxels might not be visible
        // If you do voxel frustum culling (maybe as hierarchy), this should never happen
        if (ndcMin.x > ndcMax.x || ndcMin.y > ndcMax.y) {
            gl_Position = vec4(-1.0, -1.0, -1.0, -1.0);
            return;
        }

        // Place the quad corners in NDC
        vec2 uv = clamp(corner * 0.5 + 0.5, 0.0, 1.0);   // [-1,1] -> [0,1]
        vec2 ndcXY = mix(ndcMin, ndcMax, uv);

        // Convert ndc back to clip space
        position.xy = ndcXY * position.w;
    }

    color = voxel.color;
    cameraPosition = push_constants.camera_position;
    inverseProjViewMatrix = push_constants.inverse_proj_view_matrix;
    screenSize = push_constants.screen_size;
    box = Box(voxel.position, vec3(radius), vec3(invRadius), mat3(1.0));
    quad = (corner * 0.5) + 0.5;

    float stochasticCoverage = pointSize * pointSize;
    if (stochasticCoverage < 0.8 && (gl_InstanceIndex & 0xffff) > stochasticCoverage * (0xffff / 0.8)) {
        position = vec4(-1, -1, -1, -1);
    }

    gl_Position = position;
}
