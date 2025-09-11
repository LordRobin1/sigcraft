#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

layout(location = 0) in Box box;
layout(location = 6) in vec3 color;
layout(location = 7) in vec3 cameraPosition;
layout(location = 8) in vec2 screenSize;
layout(location = 9) in mat4 inverseProjViewMatrix;
layout(location = 14) flat in uint voxelTextureIndex;

layout(location = 0) out vec4 colorOut;

layout(set = 0, binding = 0) uniform sampler2DArray textures;

float maxComponent(vec3 v) { return max (max(v.x, v.y), v.z); }

bool ourHitAABox(vec3 boxCenter, vec3 boxRadius, vec3 rayOrigin, vec3 rayDirection, vec3 invRayDirection) {
    rayOrigin -= boxCenter;
    vec3 distanceToPlane = (-boxRadius * sign(rayDirection) - rayOrigin) * invRayDirection;

    #define TEST(U, V,W)\
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
    #undef TEST
}

// vec3 box.radius:       independent half-length along the X, Y, and Z axes
// mat3 box.rotation:     box-to-world rotation (orthonormal 3x3 matrix) transformation
// bool rayCanStartInBox: if true, assume the origin is never in a box. GLSL optimizes this at compile time
// bool oriented:         if false, ignore box.rotation
bool ourIntersectBoxCommon(Box box, Ray ray, out float distance, out vec3 normal, const bool rayCanStartInBox, const in bool oriented, in vec3 _invRayDirection) {

    // Move to the box's reference frame. This is unavoidable and un-optimizable.
    ray.origin = box.rotation * (ray.origin - box.center);
    if (oriented) {
        ray.direction = ray.direction * box.rotation;
    }

    // This "rayCanStartInBox" branch is evaluated at compile time because `const` in GLSL
    // means compile-time constant. The multiplication by 1.0 will likewise be compiled out
    // when rayCanStartInBox = false.
    float winding;
    if (rayCanStartInBox) {
        // Winding direction: -1 if the ray starts inside of the box (i.e., and is leaving), +1 if it is starting outside of the box
        winding = (maxComponent(abs(ray.origin) * box.invRadius) < 1.0) ? -1.0 : 1.0;
    } else {
        winding = 1.0;
    }

    // We'll use the negated sign of the ray direction in several places, so precompute it.
    // The sign() instruction is fast...but surprisingly not so fast that storing the result
    // temporarily isn't an advantage.
    vec3 sgn = -sign(ray.direction);

    // Ray-plane intersection. For each pair of planes, choose the one that is front-facing
    // to the ray and compute the distance to it.
    vec3 distanceToPlane = box.radius * winding * sgn - ray.origin;
    if (oriented) {
        distanceToPlane /= ray.direction;
    } else {
        distanceToPlane *= _invRayDirection;
    }

    // Perform all three ray-box tests and cast to 0 or 1 on each axis.
    // Use a macro to eliminate the redundant code (no efficiency boost from doing so, of course!)
    // Could be written with
    #define TEST(U, VW)\
         /* Is there a hit on this axis in front of the origin? Use multiplication instead of && for a small speedup */\
         (distanceToPlane.U >= 0.0) && \
         /* Is that hit within the face of the box? */\
         all(lessThan(abs(ray.origin.VW + ray.direction.VW * distanceToPlane.U), box.radius.VW))

    bvec3 test = bvec3(TEST(x, yz), TEST(y, zx), TEST(z, xy));

    // CMOV chain that guarantees exactly one element of sgn is preserved and that the value has the right sign
    sgn = test.x ? vec3(sgn.x, 0.0, 0.0) : (test.y ? vec3(0.0, sgn.y, 0.0) : vec3(0.0, 0.0, test.z ? sgn.z : 0.0));
    #undef TEST

    // At most one element of sgn is non-zero now. That element carries the negative sign of the
    // ray direction as well. Notice that we were able to drop storage of the test vector from registers,
    // because it will never be used again.

    // Mask the distance by the non-zero axis
    // Dot product is faster than this CMOV chain, but doesn't work when distanceToPlane contains nans or infs.
    //
    distance = (sgn.x != 0.0) ? distanceToPlane.x : ((sgn.y != 0.0) ? distanceToPlane.y : distanceToPlane.z);

    // Normal must face back along the ray. If you need
    // to know whether we're entering or leaving the box,
    // then just look at the value of winding. If you need
    // texture coordinates, then use box.invDirection * hitPoint.

    if (oriented) {
        normal = box.rotation * sgn;
    } else {
        normal = sgn;
    }

    return (sgn.x != 0) || (sgn.y != 0) || (sgn.z != 0);
}

void main() {
    // fragPosition in world space (formula from https://stackoverflow.com/a/38960050)
    vec3 ndc = vec3(gl_FragCoord.xy / screenSize, gl_FragCoord.z) * 2.0 - 1.0;
    vec4 clip  = inverseProjViewMatrix * vec4(ndc, 1.0);
    vec3 fragPosition = clip.xyz / clip.w;

    vec3 rayOrigin = cameraPosition;
    vec3 rayDirection = normalize(fragPosition - rayOrigin);
    Ray ray = Ray(rayOrigin, rayDirection);

    const bool rayCanStartInBox = true;
    const bool oriented = false;
    float distance;
    vec3 normal;

//    if (ourHitAABox(box.center, box.radius, rayOrigin, rayDirection, 1 / rayDirection))
    if (ourIntersectBoxCommon(box, ray, distance, normal, rayCanStartInBox, oriented, 1.0 / rayDirection)) {
        vec3 intersectionPoint = rayOrigin + rayDirection * distance;

        vec3 localPos = (intersectionPoint - box.center) * box.invRadius;

        vec2 uv;
        if (abs(normal.x) > 0.9) {
            uv = localPos.yz;
        } else if (abs(normal.y) > 0.9) {
            uv = localPos.xz;
        } else {
            uv = localPos.xy;
        }
        // uv = fract(uv);

        colorOut = texture(textures, vec3(uv, voxelTextureIndex));

        const float near = 0.1;
        const float far = 1000.0;
        gl_FragDepth = clamp((distance - near) / (far - near), 0.0, 1.0);
    } else {
        discard;
    }
}
