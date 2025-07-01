#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

//layout(location = 0) in ivec3 vertexIn;
//layout(location = 1) in vec3 normalIn;
//layout(location = 2) in vec3 colorIn;

struct Voxel { ivec3 position; vec3 color; };

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

const vec2 fullscreenVerts[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void main() {
    Voxel voxel = push_constants.voxel_buffer.voxels[gl_VertexIndex / 6];
    ivec3 voxel_position = voxel.position;
    vec2 ndc = fullscreenVerts[gl_VertexIndex % 6];

    color = voxel.color;

    cameraPosition = push_constants.camera_position;
    inverseProjViewMatrix = push_constants.inverse_proj_view_matrix;
    box = Box(voxel_position, vec3(1), vec3(1), mat3(1.0));
    screenSize = push_constants.screen_size;

    // emulate billboard for debugging
    //vec4 position = push_constants.proj_view_mat * vec4(vec3(voxel_position) + vec3(ndc, 0.0), 1.0);

    gl_Position = vec4(ndc, 0.0, 1.0);
}
