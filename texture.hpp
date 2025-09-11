#pragma once

#include "stb_image.h"
#include <imr/imr.h>
#include <stdexcept>
#include <string>
#include <assert.h>
#include <filesystem>
#include "slog.hpp"
#include <enklume/block_data.h>

/// max size of a texture array
constexpr size_t TEXTURE_ARRAY_MAX = 1024;

/**
 * The Sampler tells the GPU how to read from a texture.
 */
struct Sampler {
    VkSampler sampler;

    Sampler(const imr::Device& device);
};

/**
 * Holds raw texture data. Used for loading in textures.
 */
struct TextureData {
    std::vector<stbi_uc*> raw{};
    int width;
    int height;
    int channels;
};

/**
 * Holds many textures inside of one imr::Image.
 * Different textures are accessible through layers.
 */
struct TextureArray {
    std::unique_ptr<imr::Image> textures;
    std::unique_ptr<imr::DescriptorBindHelper> bindHelper;
};

/**
 * Holds all textures in the game. When instantiated, will load all images in the
 * textures directory.
 *
 * references:
 * - https://kylehalladay.com/blog/tutorial/vulkan/2018/01/28/Textue-Arrays-Vulkan.html
 * - https://github.com/SaschaWillems/Vulkan/blob/master/examples/texturearray/texturearray.cpp?utm_source=chatgpt.com
 */
class TextureManager {
public:
    // loads all textures and uploads to the gpu such that shaders can access them
    TextureManager(imr::Device &device, imr::GraphicsPipeline &pipeline, Sampler &sampler);
    ~TextureManager() = default;

    std::unique_ptr<TextureArray> m_blockTextures{};
    std::unique_ptr<TextureArray> m_liquidTextures{};

    // Maps each block to its texture index.
    std::unordered_map<BlockId, uint32_t> m_idToIndex{};

private:
    /// loads all files from given dir
    TextureData loadTextureData(const std::string& dirPathStr);
};
