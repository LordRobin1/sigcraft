#ifndef Box_glsl
#define Box_glsl

mat3 quat2mat(vec4 q) {
    q *= 1.41421356; //sqrt(2)
    return mat3(1.0 - q.y*q.y - q.z*q.z,  q.x*q.y + q.w*q.z,         q.x*q.z - q.w*q.y,
                q.x*q.y - q.w*q.z,        1.0 - q.x*q.x - q.z*q.z,   q.y*q.z + q.w*q.x,
                q.x*q.z + q.w*q.y,        q.y*q.z - q.w*q.x,         1.0 - q.x*q.x - q.y*q.y);
}

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

float safeInverse(float x) { return (x == 0.0) ? 1e12 : (1.0 / x); }
vec3 safeInverse(vec3 v) { return vec3(safeInverse(v.x), safeInverse(v.y), safeInverse(v.z)); }

#endif
