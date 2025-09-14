#include "game.h"

GameVoxels::GameVoxels(imr::Device &device, GLFWwindow *window, imr::Swapchain &swapchain, World *world, Camera &camera,
                       const bool greedyVoxels)
    : Game(device, window, swapchain, VoxelShaders(device, swapchain, {
                                                       greedyVoxels ? "greedyVoxel.vert.spv" : "voxel.vert.spv",
                                                       "voxel.frag.spv"
                                                   }), world, camera),
      greedyVoxels(greedyVoxels) {
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto* game = static_cast<GameVoxels*>(glfwGetWindowUserPointer(window));
        if (!game->world) return;

        if (key == GLFW_KEY_R && mods & GLFW_MOD_CONTROL) {
            game->reload_shaders = true;
        } else if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
            game->debugShader = (game->debugShader + game->fragmentShaders.size() - 1) % game->fragmentShaders.size();
            game->reload_shaders = true;
        } else if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
            game->debugShader = (game->debugShader + 1) % game->fragmentShaders.size();
            game->reload_shaders = true;
        } else if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
            game->greedyVoxels = !game->greedyVoxels;
            game->reload_shaders = true;
            game->toggleGreedy = true;
        } else if (key == GLFW_KEY_F10 && action == GLFW_PRESS) {
            game->toggleMode = true;
        }
    });
}

void GameVoxels::renderFrame() {
    if (toggleGreedy) {
        swapchain.drain();
        for (const auto chunk : world->loaded_chunks()) {
            if (std::unique_ptr<ChunkVoxels> stolen = std::move(chunk->voxels)) {
                delete stolen.release();
            }
            world->unload_chunk(chunk);
        }
        toggleGreedy = false;
    }
    if (reload_shaders) {
        vkDeviceWaitIdle(device.device);
        const std::vector<std::string> shaderFiles = {
            greedyVoxels ? "greedyVoxel.vert.spv" : "voxel.vert.spv",
            fragmentShaders[debugShader]
        };
        shaders = VoxelShaders(device, swapchain, shaderFiles);
        textureManager.onShaderReload(*shaders.pipeline);
        reload_shaders = false;
        textureManager.m_blockTextures->bindHelper->set_combined_image_sampler(0, 0, *textureManager.m_blockTextures->textures, sampler.sampler);
        std::cout << "Vertex shader: " << shaderFiles[0] << std::endl;
        std::cout << "Pixel shader: " << shaderFiles[1] << std::endl;
    }

    swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
        camera_update(window, &camera_input);
        camera_move_freelook(&camera, &camera_input, &camera_state, delta);

        auto& image = context.image();
        auto cmdbuf = context.cmdbuf();

        textureManager.m_blockTextures->bindHelper->commit_frame(cmdbuf);

        if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
            auto depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_D32_SFLOAT, depthBufferFlags);

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = 0,
                    .srcAccessMask = 0,
                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = depthBuffer->handle(),
                    .subresourceRange = depthBuffer->whole_image_subresource_range()
                })
            }));
        }

        vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
            // blueish color for sky
            .float32 = { 0.294f, 0.498f, 0.875f, 1.0f },
        }), 1, tmpPtr(image.whole_image_subresource_range()));

        vk.cmdClearDepthStencilImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearDepthStencilValue) {
            .depth = 1.0f,
            .stencil = 0,
        }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

        // This barrier ensures that the clear is finished before we run the dispatch.
        // before: all writes from the "transfer" stage (to which the clear command belongs)
        // after: all writes from the "compute" stage
        vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            })
        }));

        // update the push constant data on the host...
        mat4 m = identity_mat4;
        mat4 flip_y = identity_mat4;
        flip_y.rows[1][1] = -1;
        m = m * flip_y;
        mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height); // has perspective already
        m = m * view_mat;

        auto& pipeline = shaders.pipeline;
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());

        push_constants.matrix = m;
        push_constants.inverse_matrix = invert_mat4(m);
        push_constants.camera_position = camera.position;
        push_constants.screen_size = vec2(context.image().size().width, context.image().size().height);

        context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]{

            auto load_chunk = [&](const int cx, const int cz) {
                Chunk* loaded = world->get_loaded_chunk(cx, cz);
                if (!loaded)
                    world->load_chunk(cx, cz);
                else {
                    if (loaded->voxels) return;

                    bool all_neighbours_loaded = true;
                    ChunkNeighbors n = {};
                    for (int dx = -1; dx < 2; dx++) {
                        for (int dz = -1; dz < 2; dz++) {
                            const int nx = cx + dx;
                            const int nz = cz + dz;

                            const auto neighborChunk = world->get_loaded_chunk(nx, nz);
                            if (neighborChunk)
                                n.neighbours[dx + 1][dz + 1] = &neighborChunk->data;
                            else
                                all_neighbours_loaded = false;
                        }
                    }
                    if (all_neighbours_loaded)
                        loaded->voxels = std::make_unique<ChunkVoxels>(device, n, ivec2{cx, cz}, greedyVoxels, textureManager.m_idToIndex);
                }
            };

            const int player_chunk_x = camera.position.x / 16;
            const int player_chunk_z = camera.position.z / 16;

            constexpr int radius = RENDER_DISTANCE;
            for (int dx = -radius; dx <= radius; dx++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                }
            }

             for (const auto chunk : world->loaded_chunks()) {
                 // unload
                 if (abs(chunk->cx - player_chunk_x) > radius || abs(chunk->cz - player_chunk_z) > radius) {
                     if (std::unique_ptr<ChunkVoxels> stolen = std::move(chunk->voxels)) {
                         const ChunkVoxels* released = stolen.release();
                         context.frame().addCleanupAction([=]{
                             delete released;
                         });
                     }
                     world->unload_chunk(chunk);
                     continue;
                 }

                 const auto& voxels = chunk->voxels;
                 if (!voxels || voxels->num_voxels == 0)
                     continue;

                 push_constants.voxel_buffer = voxels->voxel_buffer_device_address(greedyVoxels);
                 vkCmdPushConstants(
                     cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT,
                     0, sizeof(push_constants), &push_constants);
                 vkCmdDraw(cmdbuf, 6, voxels->num_voxels, 0, 0);
             }
        });

        auto now = imr_get_time_nano();
        delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
        prev_frame = now;

        glfwPollEvents();
    });
}

GameMesh::GameMesh(imr::Device& device, GLFWwindow* window, imr::Swapchain& swapchain, World* world, Camera& camera)
    : Game(device, window, swapchain, MeshShaders(device, swapchain), world, camera) {
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* game = static_cast<GameMesh*>(glfwGetWindowUserPointer(window));
        if (key == GLFW_KEY_R && mods & GLFW_MOD_CONTROL) {
            game->reload_shaders = true;
        } else if (key == GLFW_KEY_F10 && action == GLFW_PRESS) {
            game->toggleMode = true;
        }
    });
}

void GameMesh::renderFrame() {
    swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
        camera_update(window, &camera_input);
        camera_move_freelook(&camera, &camera_input, &camera_state, delta);

        if (reload_shaders) {
            swapchain.drain();
            shaders = MeshShaders(device, swapchain);
            reload_shaders = false;
        }

        auto& image = context.image();
        auto cmdbuf = context.cmdbuf();

        if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
            auto depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_D32_SFLOAT, depthBufferFlags);

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = 0,
                    .srcAccessMask = 0,
                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = depthBuffer->handle(),
                    .subresourceRange = depthBuffer->whole_image_subresource_range()
                })
            }));
        }

        vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
            .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
        }), 1, tmpPtr(image.whole_image_subresource_range()));

        vk.cmdClearDepthStencilImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearDepthStencilValue) {
            .depth = 1.0f,
            .stencil = 0,
        }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

        // This barrier ensures that the clear is finished before we run the dispatch.
        // before: all writes from the "transfer" stage (to which the clear command belongs)
        // after: all writes from the "compute" stage
        vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = 0,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            })
        }));

        // update the push constant data on the host...
        mat4 m = identity_mat4;
        mat4 flip_y = identity_mat4;
        flip_y.rows[1][1] = -1;
        m = m * flip_y;
        mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
        m = m * view_mat;
        m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

        auto& pipeline = shaders.pipeline;
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());

        push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;

        context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
            push_constants.matrix = m;

            auto load_chunk = [&](int cx, int cz) {
                auto loaded = world->get_loaded_chunk(cx, cz);
                if (!loaded)
                    world->load_chunk(cx, cz);
                else {
                    if (loaded->mesh)
                        return;

                    bool all_neighbours_loaded = true;
                    ChunkNeighbors n = {};
                    for (int dx = -1; dx < 2; dx++) {
                        for (int dz = -1; dz < 2; dz++) {
                            int nx = cx + dx;
                            int nz = cz + dz;

                            auto neighborChunk = world->get_loaded_chunk(nx, nz);
                            if (neighborChunk)
                                n.neighbours[dx + 1][dz + 1] = &neighborChunk->data;
                            else
                                all_neighbours_loaded = false;
                        }
                    }
                    if (all_neighbours_loaded)
                        loaded->mesh = std::make_unique<ChunkMesh>(device, n);
                }
            };

            int player_chunk_x = camera.position.x / 16;
            int player_chunk_z = camera.position.z / 16;

            int radius = RENDER_DISTANCE;
            for (int dx = -radius; dx <= radius; dx++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                }
            }

            for (auto chunk : world->loaded_chunks()) {
                if (abs(chunk->cx - player_chunk_x) > radius || abs(chunk->cz - player_chunk_z) > radius) {
                    std::unique_ptr<ChunkMesh> stolen = std::move(chunk->mesh);
                    if (stolen) {
                        ChunkMesh* released = stolen.release();
                        context.frame().addCleanupAction([=]() {
                            delete released;
                        });
                    }
                    world->unload_chunk(chunk);
                    continue;
                }

                auto& mesh = chunk->mesh;
                if (!mesh || mesh->num_verts == 0)
                    continue;

                push_constants.chunk_position = { chunk->cx, 0, chunk->cz };
                vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);

                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mesh->buf->handle, tmpPtr((VkDeviceSize) 0));

                assert(mesh->num_verts > 0);
                vkCmdDraw(cmdbuf, mesh->num_verts, 1, 0, 0);
            }
        });

        auto now = imr_get_time_nano();
        delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
        prev_frame = now;

        glfwPollEvents();
    });
}
