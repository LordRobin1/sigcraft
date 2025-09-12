#define STB_IMAGE_IMPLEMENTATION
#include "texture.hpp"

const std::string TEXTURE_DIR = "../assets/textures/";
const std::string BLOCKS = "blocks/";
const std::string LIQUID = "liquid/";
const std::string PNG = ".png";

const std::string SIDE = "_side";
const std::string BOTTOM = "_bottom";
const std::string TOP = "_top";

const std::unordered_map<std::string, BlockId> nameToId {
    {"stone", BlockStone},
    {"dirt", BlockDirt},
    {"grass_block", BlockGrass},
    {"sand", BlockSand},
    {"gravel", BlockGravel},
    {"planks", BlockPlanks},
    {"water", BlockWater},
    {"leaves", BlockLeaves},
    {"log", BlockWood},
    {"snow", BlockSnow},
    {"lava", BlockLava},
    {"bedrock", BlockBedrock},
    {"sandstone", BlockSandStone},
    {"unknown", BlockUnknown},
};

inline std::string cleanName(const std::string& name) {
    if (name.ends_with(SIDE)) return name.substr(0, name.length() - SIDE.length());
    if (name.ends_with(BOTTOM)) return name.substr(0, name.length() - BOTTOM.length());
    if (name.ends_with(TOP)) return name.substr(0, name.length() - TOP.length());
    return name;
}

namespace fs = std::filesystem;

Sampler::Sampler(const imr::Device& device) {
    VkSamplerCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST, // when texel bigger than 1 pixel, pick nearest
        .minFilter= VK_FILTER_NEAREST, // when texel smaller pick nearest
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // we want to tile when outside [0, 1]
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .minLod = 0.0f, // no LOD for now...
        .maxLod = 0.0f,
    };

    vkCreateSampler(device.device, &createInfo, nullptr, &sampler);
}


TextureManager::TextureManager(imr::Device &device, imr::GraphicsPipeline& pipeline, Sampler& sampler) {
    // TODO: Add liquid support
    TextureData blockData = loadTextureData(TEXTURE_DIR + BLOCKS);

    VkImageUsageFlagBits usage = static_cast<VkImageUsageFlagBits>(
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT
    );

    VkBufferImageCopy copyRegion = {};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.depth = 1;
    copyRegion.bufferOffset = 0;


    const uint32_t layerCount = blockData.raw.size();
    const VkExtent3D sizeVk = {
        static_cast<uint32_t>(blockData.width * TEXTURES_PER_BLOCK),
        static_cast<uint32_t>(blockData.height), 1
    };
    const size_t singleImageSize = blockData.width * blockData.height * 4;
    const size_t size = singleImageSize * TEXTURES_PER_BLOCK;

    m_blockTextures = std::make_unique<TextureArray>(
        std::make_unique<imr::Image>(
            device,
            VK_IMAGE_TYPE_2D,
            sizeVk,
            VK_FORMAT_R8G8B8A8_UNORM,
            usage,
            layerCount
        ),
        std::unique_ptr<imr::DescriptorBindHelper>(pipeline.create_bind_helper())
    );

    assert(m_blockTextures->textures->handle());

    imr::Buffer stagingBuffer{
        device,
        size * layerCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    for (const auto& id : m_blockOrder) {
        uint32_t idx = m_idToIndex[id];
        auto &[side, top, bottom] = blockData.raw[id];

        assert(side && "Side texture must always be present.");
        stagingBuffer.uploadDataSync(idx * size, singleImageSize, side);
        if (top) stagingBuffer.uploadDataSync(idx * size + singleImageSize, singleImageSize, top);
        else stagingBuffer.uploadDataSync(idx * size + singleImageSize, singleImageSize, side);
        if (bottom) {
            assert(side && top && "Side and Top textures must be present if Bottom is also present");
            stagingBuffer.uploadDataSync(idx * size + singleImageSize * 2, singleImageSize, bottom);
        } else if (top) stagingBuffer.uploadDataSync(idx * size + singleImageSize * 2, singleImageSize, top);
        else stagingBuffer.uploadDataSync(idx * size + singleImageSize * 2, singleImageSize, side);

        stbi_image_free(side);
        stbi_image_free(bottom);
        stbi_image_free(top);
    }

    copyRegion.imageExtent.width = static_cast<uint32_t>(blockData.width * TEXTURES_PER_BLOCK);
    copyRegion.imageExtent.height = static_cast<uint32_t>(blockData.height);

    device.executeCommandsSync([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier2 preCopy {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = m_blockTextures->textures->handle(),
            .subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                layerCount
            }
        };

        VkDependencyInfo depPre {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &preCopy
        };

        device.dispatch.cmdPipelineBarrier2(cmd, &depPre);

        for (int layer = 0; layer < layerCount; layer++) {
            copyRegion.bufferOffset = layer * size;
            copyRegion.imageSubresource.baseArrayLayer = layer;

            vkCmdCopyBufferToImage(
                cmd,
                stagingBuffer.handle,
                m_blockTextures->textures->handle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion
            );
        }

        VkImageMemoryBarrier2 postCopy {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = m_blockTextures->textures->handle(),
            .subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0, 1,
                0, layerCount
            }
        };

        VkDependencyInfo depPost {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &postCopy
        };

        device.dispatch.cmdPipelineBarrier2(cmd, &depPost);

        // make accessible to shaders
        m_blockTextures->bindHelper->set_combined_image_sampler(0, 0, *m_blockTextures->textures, sampler.sampler);

        m_blockTextures->bindHelper->commit(cmd);
    });

}

TextureManager::TextureData TextureManager::loadTextureData(const std::string& dirPathStr) {
    TextureData textureData{};
    fs::path dirPath(dirPathStr);
    int width = 0, height = 0, channels = 0;
    uint32_t index = 0;

    for (const auto& textureFile : fs::directory_iterator(dirPath)) {
        assert(textureFile.path().extension() == PNG && ("Encountered non .png file at " + textureFile.path().string()).data());

        int w, h, c;
        const std::string filePath = textureFile.path().string();
        const std::string name = textureFile.path().stem().string();
        const BlockId id = nameToId.at(cleanName(name));

        stbi_uc* data = stbi_load(filePath.c_str(), &w, &h, &c, STBI_rgb_alpha);
        assert(data && "Could not load image data from file");

        if (textureData.raw.empty()) {
            width = w; height = h; channels = c;
        } else {
            assert((w == width && h == height) && "All textures loaded into the texture array must have the same dimensions.");
            // nfo("{} has {} channels", name, c);
        }

        if (!m_idToIndex.contains(id)) {
            m_idToIndex[id] = index;
            m_blockOrder.push_back(id);
            nfo("Mapped {} to idx {}", name, index);
            index++;
        }

        if (name.ends_with(BOTTOM)) textureData.raw[id].bottom = data;
        else if (name.ends_with(TOP)) textureData.raw[id].top = data;
        else textureData.raw[id].side = data;
    }

    textureData.height = height;
    textureData.width = width;
    textureData.channels = channels;

    return textureData;
}
