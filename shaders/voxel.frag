#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

layout(location = 0) in Box box;
layout(location = 6) in vec3 color;
layout(location = 7) in vec3 cameraPosition;
layout(location = 8) in vec3 fragPosition;

layout(location = 0) out vec4 colorOut;

bool ourHitAABox(vec3 boxCenter, vec3 boxRadius, vec3 rayOrigin, vec3 rayDirection, vec3 invRayDirection) {
    rayOrigin -= boxCenter;
    vec3 distanceToPlane = (-boxRadius * sign(rayDirection) - rayOrigin) * invRayDirection;

#   define TEST(U, V,W)\
         (float(distanceToPlane.U >= 0.0) * \
          float(abs(rayOrigin.V + rayDirection.V * distanceToPlane.U) < boxRadius.V) * \
          float(abs(rayOrigin.W + rayDirection.W * distanceToPlane.U) < boxRadius.W))

    // If the ray is in the box or there is a hit along any axis, then there is a hit
    return bool(float(abs(rayOrigin.x) < boxRadius.x) *
        float(abs(rayOrigin.y) < boxRadius.y) *
        float(abs(rayOrigin.z) < boxRadius.z) +
        TEST(x, y, z) +
        TEST(y, z, x) +
        TEST(z, x, y));
#   undef TEST
}

void main() {
    vec3 rayOrigin = cameraPosition;
    vec3 rayDirection = normalize(fragPosition - rayOrigin);

    if (ourHitAABox(box.center, box.radius, rayOrigin, rayDirection, 1 / rayDirection))
        colorOut = vec4(color, 1.0);
    else
        discard;
}
