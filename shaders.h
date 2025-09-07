#ifndef SHADERS_H
#define SHADERS_H

#include "chunk_mesh.h"

struct Shaders {
    std::vector<std::unique_ptr<imr::ShaderModule>> modules;
    std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_points;
    std::unique_ptr<imr::GraphicsPipeline> pipeline;

    Shaders(imr::Device& d, imr::Swapchain& swapchain, const std::vector<std::string>& shaderFiles,
            VkVertexInputBindingDescription* bindings, uint32_t bindings_num,
            VkVertexInputAttributeDescription* attributes, uint32_t attributes_num
    ) {
        imr::GraphicsPipeline::RenderTargetsState rts;
        rts.color.push_back((imr::GraphicsPipeline::RenderTarget) {
            .format = swapchain.format(),
            .blending = {
                .blendEnable = false,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            }
        });
        imr::GraphicsPipeline::RenderTarget depth = {
            .format = VK_FORMAT_D32_SFLOAT
        };
        rts.depth = depth;

        VkPipelineVertexInputStateCreateInfo vertex_input {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = bindings_num,
            .pVertexBindingDescriptions = bindings,
            .vertexAttributeDescriptionCount = attributes_num,
            .pVertexAttributeDescriptions = attributes,
        };

        VkPipelineRasterizationStateCreateInfo rasterization {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,

            .lineWidth = 1.0f,
        };

        imr::GraphicsPipeline::StateBuilder stateBuilder = {
            .vertexInputState = vertex_input,
            .inputAssemblyState = imr::GraphicsPipeline::simple_triangle_input_assembly(),
            .viewportState = imr::GraphicsPipeline::one_dynamically_sized_viewport(),
            .rasterizationState = rasterization,
            .multisampleState = imr::GraphicsPipeline::one_spp(),
            .depthStencilState = imr::GraphicsPipeline::simple_depth_testing(),
        };

        std::vector<imr::ShaderEntryPoint*> entry_point_ptrs;
        for (auto filename : shaderFiles) {
            VkShaderStageFlagBits stage;
            if (filename.ends_with("vert.spv"))
                stage = VK_SHADER_STAGE_VERTEX_BIT;
            else if (filename.ends_with("frag.spv"))
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            else
                throw std::runtime_error("Unknown suffix");
            modules.push_back(std::make_unique<imr::ShaderModule>(d, std::move(filename)));
            entry_points.push_back(std::make_unique<imr::ShaderEntryPoint>(*modules.back(), stage, "main"));
            entry_point_ptrs.push_back(entry_points.back().get());
        }
        pipeline = std::make_unique<imr::GraphicsPipeline>(d, std::move(entry_point_ptrs), rts, stateBuilder);
    }
};

struct VoxelShaders : Shaders {
    VoxelShaders(imr::Device& d, imr::Swapchain& swapchain, const std::vector<std::string>& shaderFiles)
        : Shaders(d, swapchain, shaderFiles, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0)
    {}
};

struct MeshShaders : Shaders {
    MeshShaders(imr::Device& d, imr::Swapchain& swapchain)
        : Shaders(
            d,
            swapchain,
            { "basic.vert.spv", "basic.frag.spv" },
            std::array<VkVertexInputBindingDescription,1>{{
                {
                    .binding = 0,
                    .stride = sizeof(ChunkMesh::Vertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                },
            }}.data(),
            1,
            std::array<VkVertexInputAttributeDescription,3>{{
                {
                    .location = 0,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16_SINT,
                    .offset = 0,
                },
                {
                    .location = 1,
                    .binding = 0,
                    .format = VK_FORMAT_R8G8B8_SNORM,
                    .offset = offsetof(ChunkMesh::Vertex, nnx),
                },
                {
                    .location = 2,
                    .binding = 0,
                    .format = VK_FORMAT_R8G8B8_UNORM,
                    .offset = offsetof(ChunkMesh::Vertex, br),
                },
            }}.data(),
            3
        )
    {}

};

#endif //SHADERS_H
