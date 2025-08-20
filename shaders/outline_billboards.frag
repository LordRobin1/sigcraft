#version 450

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

layout(location = 0) in Box box;
layout(location = 6) in vec3 color;
layout(location = 7) in vec3 cameraPosition;
layout(location = 8) in vec2 screenSize;
layout(location = 9) in mat4 inverseProjViewMatrix;
layout(location = 13) in vec2 quadUV;

layout(location = 0) out vec4 outColor;

void main() {
    float edgeThickness = 0.02;
    bool onEdge = quadUV.x < edgeThickness ||
                  quadUV.x > 1.0 - edgeThickness ||
                  quadUV.y < edgeThickness ||
                  quadUV.y > 1.0 - edgeThickness;

    if (onEdge) {
        outColor = vec4(color, 1.0);
    } else {
        discard;
    }
}
