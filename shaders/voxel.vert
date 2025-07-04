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
    Voxel voxel = push_constants.voxel_buffer.voxels[gl_InstanceIndex];
    vec2 corner = fullscreenVerts[gl_VertexIndex];

    vec3 right = vec3(push_constants.inverse_proj_view_matrix[0].xyz);
    vec3 up    = vec3(push_constants.inverse_proj_view_matrix[1].xyz);
    float scale = 5.0; // this should be dynamic somehow based on distance to camera
    vec3 worldPos = vec3(voxel.position)
    + right * corner.x * scale
    + up    * corner.y * scale;

    color = voxel.color;
    cameraPosition = push_constants.camera_position;
    inverseProjViewMatrix = push_constants.inverse_proj_view_matrix;
    screenSize = push_constants.screen_size;
    box = Box(voxel.position, vec3(1), vec3(1), mat3(1.0));

    gl_Position = push_constants.proj_view_mat * vec4(worldPos, 1.0);
}
