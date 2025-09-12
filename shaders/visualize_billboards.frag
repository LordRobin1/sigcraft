#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

layout(set = 0, binding = 0) uniform sampler2DArray textures;

layout(location = 0) in Box box;
layout(location = 6) in vec3 color;
layout(location = 7) in vec3 cameraPosition;
layout(location = 8) in vec2 screenSize;
layout(location = 9) in mat4 inverseProjViewMatrix;
layout(location = 14) flat in uint voxelTextureIndex;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(color, 1.0);
}
