#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
    mat3 rotation;
};

layout(location = 0) in ivec3 vertexIn;
//layout(location = 1) in vec3 normalIn;
//layout(location = 2) in vec3 colorIn;

struct Voxel { ivec3 position; vec3 color; };

layout(scalar, buffer_reference) readonly buffer VoxelBuffer {
    Voxel voxels[];
};
layout(scalar, push_constant) uniform T {
    mat4 matrix;
    mat4 inverse_matrix;
    ivec3 camera_position;
    VoxelBuffer voxel_buffer;
    vec2 screen_size;
} push_constants;

layout(location = 0) out Box box;
layout(location = 6) out vec3 color;
layout(location = 7) out vec3 cameraPosition;
layout(location = 8) out vec3 fragPosition;

void main() {
    Voxel voxel = push_constants.voxel_buffer.voxels[gl_VertexIndex / 6];
    ivec3 voxel_position = voxel.position;

    color = voxel.color;

    cameraPosition = push_constants.camera_position;

    vec2 ndc = (vertexIn.xy / push_constants.screen_size) * 2.0 - 1.0;
    vec4 farPoint  = push_constants.inverse_matrix * vec4(ndc, 1.0, 1.0);
    vec3 worldFar  = farPoint.xyz / farPoint.w;
    fragPosition = normalize(worldFar - cameraPosition);

    box = Box(voxel_position, vec3(1), vec3(1), mat3(1.0));

    gl_Position = vec4(vertexIn, 1.0);
}
