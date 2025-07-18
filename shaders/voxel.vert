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
    float sphereRadius = 1.732051;
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


void main() {
    Voxel voxel = push_constants.voxel_buffer.voxels[gl_InstanceIndex];
    vec2 corner = fullscreenVerts[gl_VertexIndex];

    vec4 position = push_constants.proj_view_mat * vec4(voxel.position, 1.0);
    float pointSize;
    quadricProj(vec3(voxel.position), push_constants.proj_view_mat, push_constants.screen_size * 0.5, position, pointSize);

    vec2 screenOffset = corner * (pointSize / push_constants.screen_size);
    position.xy += screenOffset * position.w;

    color = voxel.color;
    cameraPosition = push_constants.camera_position;
    inverseProjViewMatrix = push_constants.inverse_proj_view_matrix;
    screenSize = push_constants.screen_size;
    box = Box(voxel.position, vec3(1), vec3(1), mat3(1.0));

    gl_Position = position;
}
