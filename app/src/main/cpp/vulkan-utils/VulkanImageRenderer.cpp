/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <android/trace.h>
#include <chrono>
#include <unistd.h>
#include <cmath>
#include "VulkanImageRenderer.h"
#include "vulkan_utils.h"

// For validation we should use FOREIGN here, but EXTERNAL can be faster.
 const uint32_t VULKAN_QUEUE_FAMILY = VK_QUEUE_FAMILY_EXTERNAL_KHR;
//const uint32_t VULKAN_QUEUE_FAMILY = VK_QUEUE_FAMILY_FOREIGN_EXT;

VulkanImageRenderer::VulkanImageRenderer(VulkanInstance *init,  uint32_t num_displays,
        uint32_t width, uint32_t height,
                                         VkFormat format, VkColorSpaceKHR colorSpace)
        :   mInstance(init), VULKAN_RENDERER_NUM_DISPLAYS(num_displays),
            mImageReaderWidth(width), mImageReaderHeight(height), mFormat(format), mColorSpace(colorSpace) {

    // Set up vectors to be the size of the number of displays
    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {
        mSurfaces.push_back(VulkanSurface(mInstance, &mFormat, &mColorSpace));
        mPipelines.push_back(VK_NULL_HANDLE);
        mCmdBuffers.push_back(VK_NULL_HANDLE);
        mDescriptorPools.push_back(VK_NULL_HANDLE);
    }

    mDescriptorSets.resize(VULKAN_RENDERER_NUM_DISPLAYS);
}

bool VulkanImageRenderer::init(ANativeWindow *output_window, ANativeWindow *output_window_left, ANativeWindow *output_window_right, uint32_t rendererCopyWidth, uint32_t rendererCopyHeight) {
    // Create render pass
    {
        VkAttachmentDescription attachmentDescs[1] {
                {       .flags = 0u,
                        .format = mFormat,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
        };

        VkAttachmentReference attachmentRefs[1]{
                {.attachment = 0u, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        };

        VkSubpassDescription subpassDesc{
                .flags = 0u,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 0u,
                .pInputAttachments = nullptr,
                .colorAttachmentCount = 1u,
                .pColorAttachments = attachmentRefs,
                .pResolveAttachments = nullptr,
                .pDepthStencilAttachment = nullptr,
                .preserveAttachmentCount = 0u,
                .pPreserveAttachments = nullptr,
        };
        VkRenderPassCreateInfo renderPassCreateInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .attachmentCount = 1u,
                .pAttachments = attachmentDescs,
                .subpassCount = 1u,
                .pSubpasses = &subpassDesc,
                .dependencyCount = 0u,
                .pDependencies = nullptr,
        };
        VK_CALL(vkCreateRenderPass(mInstance->device(), &renderPassCreateInfo, nullptr,
                                   &mRenderPass));
    }

    // Create vertex buffer.
    {
        const float vertexData[] = {
                -1.0F, -1.0F, 0.0F, 0.0F, 1.0F,  -1.0F, 1.0F, 0.0F,
                1.0F,  1.0F,  1.0F, 1.0F, -1.0F, -1.0F, 0.0F, 0.0F,
                1.0F,  1.0F,  1.0F, 1.0F, -1.0F, 1.0F,  0.0F, 1.0F,
        };
        VkBufferCreateInfo createBufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .size = sizeof(vertexData),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .flags = 0,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr,
        };
        VK_CALL(vkCreateBuffer(mInstance->device(), &createBufferInfo, nullptr,
                               &mVertexBuffer));

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(mInstance->device(), mVertexBuffer, &memReq);
        VkMemoryAllocateInfo allocInfo{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = nullptr,
                .allocationSize = memReq.size,
                .memoryTypeIndex = mInstance->findMemoryType(
                        memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
        };
        VK_CALL(vkAllocateMemory(mInstance->device(), &allocInfo, nullptr,
                                 &mVertexBufferMemory));

        void *mappedData;
        VK_CALL(vkMapMemory(mInstance->device(), mVertexBufferMemory, 0,
                            sizeof(vertexData), 0, &mappedData));
        memcpy(mappedData, vertexData, sizeof(vertexData));
        vkUnmapMemory(mInstance->device(), mVertexBufferMemory);

        VK_CALL(vkBindBufferMemory(mInstance->device(), mVertexBuffer,
                                   mVertexBufferMemory, 0));
    }

    // Create the output surfaces
    {
        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(mInstance->gpu(), &queue_family_count,
                                                 NULL);

        auto *queue_family_props = new VkQueueFamilyProperties[queue_family_count];
        vkGetPhysicalDeviceQueueFamilyProperties(mInstance->gpu(), &queue_family_count, queue_family_props);

        uint32_t family_queue_index = 0;

        // From the Vulkan 1.1.11 spec:
        //
        //   On Android, all physical devices and queue families must be capable of
        //   presentation with any native window.
        //
        // Therefore we simply choose the first graphics queue.
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                family_queue_index = i;
//                logd("Found graphics family queue. Index: %d", family_queue_index);
                break;
            }
        }

        // Create command pool.
        {
            VkCommandPoolCreateInfo cmdPoolCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = 0,
            };
            VK_CALL(vkCreateCommandPool(mInstance->device(), &cmdPoolCreateInfo, nullptr,
                                        &mCmdPool));
        }

        // Set up each output surface. 1 == Center. 2 == Left. 3 == Right
        mSurfaces[0].init(output_window);
        if (2 <= VULKAN_RENDERER_NUM_DISPLAYS)
            mSurfaces[1].init(output_window_left);
        if (3 <= VULKAN_RENDERER_NUM_DISPLAYS)
            mSurfaces[2].init(output_window_right);

        // Setup a swapchain for each output surface
        mSwapchains = std::vector<VulkanSwapchain>(VULKAN_RENDERER_NUM_DISPLAYS, VulkanSwapchain(mInstance, &mCmdPool));
        for (int i = 0; i < VULKAN_RENDERER_NUM_DISPLAYS; i++) {
            if ((rendererCopyWidth > 0) && (rendererCopyHeight > 0)) {
                mSwapchains[i].init(&mSurfaces[i].mVkSurface, &mSurfaces[i].mSurfaceCaps, &mSurfaces[i].mSurfaceFormat,
                                    mSurfaces[i].mOutputWidth, mSurfaces[i].mOutputHeight, mImageReaderWidth, mImageReaderHeight,
                                    family_queue_index, &mSurfaces[i].mAlphaFlags, &mRenderPass, rendererCopyWidth, rendererCopyHeight);
            } else {
                mSwapchains[i].init(&mSurfaces[i].mVkSurface, &mSurfaces[i].mSurfaceCaps, &mSurfaces[i].mSurfaceFormat,
                                    mSurfaces[i].mOutputWidth, mSurfaces[i].mOutputHeight, mImageReaderWidth, mImageReaderHeight,
                                    family_queue_index, &mSurfaces[i].mAlphaFlags, &mRenderPass);
            }
        }
    }

    // Create Vertex and Fragment shaders
    {
        static const uint32_t vert_spirv[] = {
            #include "quad.vert.spvnum"
        };

        VkShaderModuleCreateInfo vertexShaderInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .codeSize = sizeof(vert_spirv),
                .pCode = vert_spirv,
        };
        VK_CALL(vkCreateShaderModule(mInstance->device(), &vertexShaderInfo, nullptr,
                                     &mVertModule));



        static const uint32_t frag_spirv[] = {
            #include "quad.frag.spvnum"
        };

        VkShaderModuleCreateInfo fragmentShaderInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .codeSize = sizeof(frag_spirv),
                .pCode = frag_spirv,
        };
        VK_CALL(vkCreateShaderModule(mInstance->device(), &fragmentShaderInfo, nullptr,
                                     &mFragModule));
    }


    VkPipelineCacheCreateInfo pipelineCacheInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .initialDataSize = 0,
            .pInitialData = nullptr,
            .flags = 0, // reserved, must be 0
    };
    VK_CALL(vkCreatePipelineCache(mInstance->device(), &pipelineCacheInfo, nullptr, &mCache));

    // Create Descriptor Pools
    {
        for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {

            const VkDescriptorPoolSize descriptorPoolSizes[3] = {
                    // New image sampler
                    {
                            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .descriptorCount = mSwapchains[surface_i].mSwapchainLength,
                    },

                    // Previous image sampler
                    {
                            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .descriptorCount = mSwapchains[surface_i].mSwapchainLength,
                    },

                    // ShaderVars
                    {
                            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .descriptorCount = mSwapchains[surface_i].mSwapchainLength,
                    },
            };
            const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .maxSets = mSwapchains[surface_i].mSwapchainLength,
                    .poolSizeCount = 3,
                    .pPoolSizes = descriptorPoolSizes,
            };

            VK_CALL(vkCreateDescriptorPool(mInstance->device(), &descriptorPoolCreateInfo,
                                           nullptr, &mDescriptorPools[surface_i]));
            }
    }

    // Create RGB sampler for multi-frame effects
    VkSamplerCreateInfo samplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE, // range 0.0->1.0
    };
    VK_CALL(vkCreateSampler(mInstance->device(), &samplerCreateInfo, nullptr, &mRgbSampler));


    return true;
}

VulkanImageRenderer::~VulkanImageRenderer() {
    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {
        if (mDescriptorPools[surface_i] != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(mInstance->device(), mDescriptorPools[surface_i], nullptr);
            mDescriptorPools[surface_i] = VK_NULL_HANDLE;
        }
    }

    if (mCmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(mInstance->device(), mCmdPool, nullptr);
        mCmdPool = VK_NULL_HANDLE;
    }
    if (mRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(mInstance->device(), mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }
    if (mVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mInstance->device(), mVertexBuffer, nullptr);
        mVertexBuffer = VK_NULL_HANDLE;
    }
    if (mVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mInstance->device(), mVertexBufferMemory, nullptr);
        mVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (mFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(mInstance->device(), mFramebuffer, nullptr);
        mFramebuffer = VK_NULL_HANDLE;
    }
    if (mCache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(mInstance->device(), mCache, nullptr);
        mCache = VK_NULL_HANDLE;
    }
    if (mVertModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(mInstance->device(), mVertModule, nullptr);
        mVertModule = VK_NULL_HANDLE;
    }
    if (mFragModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(mInstance->device(), mFragModule, nullptr);
        mFragModule = VK_NULL_HANDLE;
    }
}


bool VulkanImageRenderer::createPipeline(VkSampler sampler,
                                         bool useImmutableSampler) {
    isPipelineInitialized = true;
    cleanUpPipelineTemporaries();

    // Create graphics pipeline.
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[3] = {
                // imageSamplerBinding
                {
                        .binding = 0,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .pImmutableSamplers = useImmutableSampler ? &sampler : nullptr,
                },

                // previous frame image sampler
                {
                        .binding = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .pImmutableSamplers = &mRgbSampler,
                },

                // shaderVarsLayoutBinding
                {
                        .binding = 2,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags =  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        .pImmutableSamplers = NULL
                }
        };

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = 3,
                .pBindings = descriptorSetLayoutBindings,
        };
        VK_CALL(vkCreateDescriptorSetLayout(mInstance->device(),
                                            &descriptorSetLayoutCreateInfo, nullptr,
                                            &mDescriptorLayout));


        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .setLayoutCount = 1,
                .pSetLayouts = &mDescriptorLayout,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr,
        };


        VK_CALL(vkCreatePipelineLayout(mInstance->device(), &pipelineLayoutCreateInfo,
                                       nullptr, &mLayout));

        VkPipelineShaderStageCreateInfo shaderStageParams[2] = {
                {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                        .module = mVertModule,
                        .pName = "main",
                        .pSpecializationInfo = nullptr,
                },
                {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module = mFragModule,
                        .pName = "main",
                        .pSpecializationInfo = nullptr,
                }
        };

        VkSampleMask sampleMask = ~0u;
        VkPipelineMultisampleStateCreateInfo multisampleInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .pNext = nullptr,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
                .minSampleShading = 0,
                .pSampleMask = &sampleMask,
                .alphaToCoverageEnable = VK_FALSE,
                .alphaToOneEnable = VK_FALSE,
        };
        VkPipelineColorBlendAttachmentState attachmentStates{
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_FALSE,
        };
        VkPipelineColorBlendStateCreateInfo colorBlendInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = nullptr,
                .logicOpEnable = VK_FALSE,
                .logicOp = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &attachmentStates,
                .flags = 0,
        };
        VkPipelineRasterizationStateCreateInfo rasterInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext = nullptr,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1,
        };
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .pNext = nullptr,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE,
        };

        // Specify vertex input state
        VkVertexInputBindingDescription vertexInputBindingDescription{
                .binding = 0u,
                .stride = 4 * sizeof(float),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription vertex_input_attributes[2]{
                {
                        .binding = 0,
                        .location = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = 0,
                },
                {
                        .binding = 0,
                        .location = 1,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = sizeof(float) * 2,
                }};
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexInputBindingDescription,
                .vertexAttributeDescriptionCount = 2,
                .pVertexAttributeDescriptions = vertex_input_attributes,
        };

        // Create the viewport and pipeline for each surface
        for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {
            VkViewport viewports{
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f,
                    .x = 0,
                    .y = 0,
                    .width = static_cast<float>(mSurfaces[surface_i].mOutputWidth),
                    .height = static_cast<float>(mSurfaces[surface_i].mOutputHeight),
            };
            VkRect2D scissor = {.extent = {mSurfaces[surface_i].mOutputWidth, mSurfaces[surface_i].mOutputHeight}, .offset = {0, 0}};
            VkPipelineViewportStateCreateInfo viewportInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .pNext = nullptr,
                    .viewportCount = 1,
                    .pViewports = &viewports,
                    .scissorCount = 1,
                    .pScissors = &scissor,
            };


            // Create the pipeline
            VkGraphicsPipelineCreateInfo pipelineCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stageCount = 2,
                    .pStages = shaderStageParams,
                    .pVertexInputState = &vertexInputInfo,
                    .pInputAssemblyState = &inputAssemblyInfo,
                    .pTessellationState = nullptr,
                    .pRasterizationState = &rasterInfo,
                    .pMultisampleState = &multisampleInfo,
                    .pDepthStencilState = nullptr,
                    .pColorBlendState = &colorBlendInfo,
//                .pDynamicState = &dynamicStateInfo,
                    .pDynamicState = 0u,
                    .layout = mLayout,
                    .renderPass = mRenderPass,
                    .subpass = 0,
                    .basePipelineHandle = VK_NULL_HANDLE,
                    .basePipelineIndex = 0,

                    // Viewport State is the only per-surface dependent part of the pipeline
                    .pViewportState = &viewportInfo,
            };

            VK_CALL(vkCreateGraphicsPipelines(
                    mInstance->device(), mCache, 1, &pipelineCreateInfo, nullptr, &mPipelines[surface_i]));
        } // For all surfaces
    }

    // Create a command buffers and descriptor sets
    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {

        VkCommandBufferAllocateInfo cmdBufferCreateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = mCmdPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
        };
        VK_CALL(vkAllocateCommandBuffers(mInstance->device(), &cmdBufferCreateInfo,
                                         &mCmdBuffers[surface_i]));

        // One descriptor set for each swapchain image
        std::vector<VkDescriptorSetLayout> layouts(mSwapchains[surface_i].mSwapchainLength, mDescriptorLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPools[surface_i];
        allocInfo.descriptorSetCount = mSwapchains[surface_i].mSwapchainLength;
        allocInfo.pSetLayouts = layouts.data();
        mDescriptorSets[surface_i].resize(mSwapchains[surface_i].mSwapchainLength);
        VK_CALL(vkAllocateDescriptorSets(mInstance->device(), &allocInfo, mDescriptorSets[surface_i].data()));
    }

    return true;
}

double VulkanImageRenderer::renderImageAndReadback(VulkanAHardwareBufferImage *vkAHB,
                                                    FilterParams *filter_params,
                                                   AImage *new_aimage,
                                                   bool draw_to_screen, bool surface_ready_left, bool surface_ready_right,
                                                   RENDERER_RETURN_CODE &render_state,
                                                   uint32_t *image_copy_data) {

    // Define button for blur / multi-frame effects, if engaged, do an extra blit-out
    const int BLUR_BUTTON = 5;

    // If Kotlin needs the screen, free up all images and return
    // TODO: this should be a seperate function from renderImageAndReadback with boolean check in native-lib.cpp
    if (!draw_to_screen) {
        AImage_delete(new_aimage);

        // Wait for all surfaces to be done rendering and free up resources
        for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {

            SwapchainImage *swapchainImage = &mSwapchains[surface_i].mSwapchainImages[mSwapchains[surface_i].mSwapchainIndex];

            // Wait for any Vulkan rendering to finish
            // Note: these may be out of order from mSwapchainImages. This doesn't matter, wait for them all
            for (int fence_index = 0; fence_index < mSwapchains[surface_i].mSwapchainLength; fence_index++) {
                VK_CALL(vkWaitForFences(mInstance->device(), 1, &mSwapchains[surface_i].mSwapchainFences[fence_index], true, UINT64_MAX));
            }

            //Delete any AImages
            // Each active surface depends on the camera image/ahb, free them when the last surface is done with it.
            for (int swapchain_index = 0; swapchain_index < mSwapchains[surface_i].mSwapchainLength; swapchain_index++) {
                if (swapchainImage->old_aimage != nullptr) {
                    if (surface_i >= (VULKAN_RENDERER_NUM_DISPLAYS - 1)) {
                        AImage_delete(swapchainImage->old_aimage);
                    }
                    swapchainImage->old_aimage = nullptr;
                }
            }
        }

        render_state = RENDER_QUEUE_EMPTY;
        return 0.0;
    }


    // Render image to all surfaces
    ATrace_beginSection("VULKAN_PHOTOBOOTH: render wait for fences");
    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {

        // Grab the next swapchain image for each surface, wait for them to be done presenting
        mSwapchains[surface_i].mSwapchainFenceIndex =
                (mSwapchains[surface_i].mSwapchainFenceIndex + 1) % mSwapchains[surface_i].mSwapchainLength;
        VK_CALL(vkResetFences(mInstance->device(), 1,
                              &mSwapchains[surface_i].mSwapchainFences[mSwapchains[surface_i].mSwapchainFenceIndex]));

        ATrace_beginSection("VULKAN_PHOTOBOOTH: render acquire next image KHR");

        VK_CALL(vkAcquireNextImageKHR(mInstance->device(), mSwapchains[surface_i].mVkSwapchain,
                /*timeout*/ UINT64_MAX,
                /*semaphore*/ VK_NULL_HANDLE,
                mSwapchains[surface_i].mSwapchainFences[mSwapchains[surface_i].mSwapchainFenceIndex],
                &mSwapchains[surface_i].mSwapchainIndex));
        ATrace_endSection();
        ATrace_beginSection("VULKAN_PHOTOBOOTH: render wait for present fences...");

        // TODO: performance: get a semaphore here instead and pass it into VkQueueSubmit

        // Wait until the swapchain image is ready to use
        VK_CALL(vkWaitForFences(mInstance->device(), 1,
                                &mSwapchains[surface_i].mSwapchainFences[mSwapchains[surface_i].mSwapchainFenceIndex],
                                VK_TRUE, UINT64_MAX));
        ATrace_endSection();
        // Wait for any Vulkan rendering to finish

//        SwapchainImage *swapchainImage = &mSwapchains[surface_i].mSwapchainImages[mSwapchains[surface_i].mSwapchainIndex];

        // TODO Shouldn't need the imageFenceSet because we're waiting above

        // Make sure the display is done with the image buffer from the previous time this swapchain image was used
//        if (swapchainImage->imageFenceSet) {
//            VK_CALL(vkWaitForFences(mInstance->device(), 1, &swapchainImage->imageFence, true, UINT64_MAX));
//            swapchainImage->imageFenceSet = false;
//        }
    } // for all surfaces
    ATrace_endSection();

    /**
     * The next section copies the frame in this swapchain if it is required for GIF creation.
     *
     * Adapted from: https://github.com/SaschaWillems/Vulkan/blob/master/examples/screenshot/screenshot.cpp#L230
     */
    if (nullptr != image_copy_data) {
        ATrace_beginSection("VULKAN_PHOTOBOOTH: copy out frame for animated gif buffer");
        SwapchainImage *swapchainImage = &mSwapchains[0].mSwapchainImages[mSwapchains[0].mSwapchainIndex];

        /*
        // Check if blitting is supported
        bool supportsBlit = true;

        // Check blit support for source and destination
        VkFormatProperties formatProps;

         // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
        vkGetPhysicalDeviceFormatProperties(mInstance->gpu(), VK_FORMAT_R5G6B5_UNORM_PACK16, &formatProps);
        if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
            logd("Device does not support blitting from optimal tiled images");
            supportsBlit = false;
        } else {
            logd("Supports blitting FROM");
        }

        // Check if the device supports blitting to linear images
        vkGetPhysicalDeviceFormatProperties(mInstance->gpu(), VK_FORMAT_A8B8G8R8_UINT_PACK32, &formatProps);
        if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
            logd("Device does not support blitting to linear tiled images");
            supportsBlit = false;
        } else {
            logd("Supports blitting TO");
        }
        */

        VkCommandBufferBeginInfo cmdBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                .pInheritanceInfo = nullptr,
        };
        VK_CALL(vkBeginCommandBuffer(swapchainImage->cmdBuffer, &cmdBufferBeginInfo));

        // Do the actual blit from the swapchain image to host visible destination image
        // Transition destination image to transfer destination layout
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->imageCopy,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                mInstance->queueFamilyIndex(), mInstance->queueFamilyIndex());

        // Transition swapchain image from present to transfer source layout
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->image,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Define the region to blit (full size -> render size)
        VkOffset3D blitSizeSource {
            .x = (int32_t) mSurfaces[0].mOutputWidth,
            .y = (int32_t) mSurfaces[0].mOutputHeight,
            .z = 1,
        };
        VkOffset3D blitSizeDestination {
            .x = (int32_t) VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH,
            .y = (int32_t) VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT,
            .z = 1,
        };
        VkImageBlit imageBlitRegion{
            .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .srcSubresource.layerCount = 1,
            .srcOffsets[1] = blitSizeSource,
            .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .dstSubresource.layerCount = 1,
            .dstOffsets[1] = blitSizeDestination,
        };

        // Issue the blit command
        vkCmdBlitImage(swapchainImage->cmdBuffer,
                swapchainImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapchainImage->imageCopy, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &imageBlitRegion, VK_FILTER_NEAREST);

        // Transition destination image to general layout, which is the required layout for mapping the image memory later on
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->imageCopy,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

        // Transition back the swap chain image after the blit is done
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->image,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Submit the transition and blit commands, wait for copy completion
        VK_CALL(vkEndCommandBuffer(swapchainImage->cmdBuffer));
        VkSubmitInfo queueSubmitInfo = {
                .pNext = nullptr,
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &swapchainImage->cmdBuffer,
        };
        VK_CALL(vkQueueSubmit(mInstance->queue(), 1, &queueSubmitInfo, swapchainImage->imageCopyFence));
        VK_CALL(vkWaitForFences(mInstance->device(), 1, &swapchainImage->imageCopyFence, true, UINT64_MAX));

        // Get layout of the image (including row pitch)
        VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(mInstance->device(), swapchainImage->imageCopy, &subResource, &subResourceLayout);

        // Map image memory to allow copying from it
        char* image_data;
        VK_CALL(vkMapMemory(mInstance->device(), swapchainImage->imageCopyMemory, 0,
                            VK_WHOLE_SIZE, 0, (void**) &image_data));
        image_data += subResourceLayout.offset;

        char* image_copy_data_pointer = (char *) image_copy_data;
        double start = now_ms();
        int bytes_per_row = 4 * VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH;
        for (int y = 0; y < VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT; y++) {
            uint32_t *row = (uint32_t*) image_data;
            memcpy(image_copy_data_pointer, row, bytes_per_row);
            image_copy_data_pointer += bytes_per_row;
            image_data += subResourceLayout.rowPitch;
        }
        // logd("Memcpy took: %fms. Offset: %d. Row pitch: %d. Width: %d.", now_ms() - start, (int) subResourceLayout.offset, (int) subResourceLayout.rowPitch, VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH);

        vkUnmapMemory(mInstance->device(), swapchainImage->imageCopyMemory);
        ATrace_endSection();
    }

    /**
     * The next section copies the frame in this swapchain if it is required for multi-pass effects
     *
     * Keep a copy of the previous frame for multi-pass effects.
     *
     * Note: Only use the 1st swapchain or else there will be jiggling from out-of-order frames
     */
    if (filter_params->use_filter[BLUR_BUTTON]) {
        ATrace_beginSection("VULKAN_PHOTOBOOTH: previous frame copy");

        // TODO: add every monitor
        // Copy from the last rendered swapchain into this prev frame VkImage
        SwapchainImage *swapchainImage = &mSwapchains[0].mSwapchainImages[mSwapchains[0].mSwapchainIndex];
        SwapchainImage *prevSwapchainImage = &mSwapchains[0].mSwapchainImages[mPrevFrameSwapchainIndex];

        VkCommandBufferBeginInfo cmdBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                .pInheritanceInfo = nullptr,
        };
        VK_CALL(vkBeginCommandBuffer(swapchainImage->cmdBuffer, &cmdBufferBeginInfo));

        // Transition destination image to transfer destination layout
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->imagePrevious,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Transition N-1 swapchain image from present to transfer source layout
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, prevSwapchainImage->image,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Define the region to blit (full size -> render size)
        VkImageBlit imageBlitRegion{
                .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .srcSubresource.layerCount = 1,
                .srcOffsets[0] = VkOffset3D {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                },
                .srcOffsets[1] = VkOffset3D {
                        .x = (int32_t) mSurfaces[0].mOutputWidth,
                        .y = (int32_t) mSurfaces[0].mOutputHeight,
                        .z = 1,
                },

                .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .dstSubresource.layerCount = 1,
                .dstOffsets[0] = VkOffset3D {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                },
                .dstOffsets[1] = VkOffset3D {
                        .x = (int32_t) mSurfaces[0].mOutputWidth,
                        .y = (int32_t) mSurfaces[0].mOutputHeight,
                        .z = 1,
                },
        };

        // Issue the blit command
        vkCmdBlitImage(swapchainImage->cmdBuffer,
                       prevSwapchainImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapchainImage->imagePrevious, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &imageBlitRegion, VK_FILTER_NEAREST);

        // Transition destination image
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->imagePrevious,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Transition back the N-1 swap chain image after the blit is done
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, prevSwapchainImage->image,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Submit the transition and blit commands, wait for copy completion
        VK_CALL(vkEndCommandBuffer(swapchainImage->cmdBuffer));
        VkSubmitInfo queueSubmitInfo = {
                .pNext = nullptr,
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &swapchainImage->cmdBuffer,
        };
        VK_CALL(vkQueueSubmit(mInstance->queue(), 1, &queueSubmitInfo, swapchainImage->imagePreviousFence));
        VK_CALL(vkWaitForFences(mInstance->device(), 1, &swapchainImage->imagePreviousFence, true, UINT64_MAX));
        VK_CALL(vkResetFences(mInstance->device(), 1, &swapchainImage->imagePreviousFence));

        ATrace_endSection();
    }

    /**
     * The next section sets up the render queue for each frame, to each surface.
     *
     * This where frames are submitted with filter parameters to actually be rendered
     */
    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {
        SwapchainImage *swapchainImage = &mSwapchains[surface_i].mSwapchainImages[mSwapchains[surface_i].mSwapchainIndex];

        if (surface_i > 0) {
            logd("Rendering a non-0 surface...");
        }

        ATrace_beginSection("VULKAN_PHOTOBOOTH: render delete old images");
        // Free up old AImages for this swapchain image
        // Each active surface depends on the same camera image/ahb, free them when the last surface is done with it.
        if (surface_i >= (VULKAN_RENDERER_NUM_DISPLAYS - 1)) {
            if (swapchainImage->old_aimage != nullptr) {
                AImage_delete(swapchainImage->old_aimage);
            }
        }

        swapchainImage->old_aimage = new_aimage;
        render_state = RENDER_FRAME_SENT;
        ATrace_endSection();

        ATrace_beginSection("VULKAN_PHOTOBOOTH: render create descriptor sets");
        {
    //        logd("Time value: %" PRIu64, time_value);
            // Update descriptor set with the ShaderVars uniform buffer
            mSwapchains[surface_i].shaderVars.panel_id = surface_i;
//            mSwapchains[surface_i].shaderVars.imageWidth = mImageReaderWidth;
//            mSwapchains[surface_i].shaderVars.imageHeight = mImageReaderHeight;

            // NOTE: if we need to calculate this every frame, see algorithms in VulkanSwapchain.cpp
//             mSwapchains[surface_i].shaderVars.distortion_correction_normal = mImageReaderHeight * mSwapchains[surface_i].shaderVars.windowHeight /  mImageReaderWidth * mSwapchains[surface_i].shaderVars.windowWidth; // (Ih * Wh) / (Iw * Ww);
//             mSwapchains[surface_i].shaderVars.distortion_correction_rotated = mImageReaderHeight * mSwapchains[surface_i].shaderVars.windowHeight /  mImageReaderWidth * mSwapchains[surface_i].shaderVars.windowWidth; // (Ih * Wh) / (Iw * Ww);
//logd("Image renderer reports rotation is: %d", filter_params->rotation);

            mSwapchains[surface_i].shaderVars.rotation = filter_params->rotation;
            mSwapchains[surface_i].shaderVars.seek_value1 = filter_params->seek_values[0];
            mSwapchains[surface_i].shaderVars.seek_value2 = filter_params->seek_values[1];
            mSwapchains[surface_i].shaderVars.seek_value3 = filter_params->seek_values[2];
            mSwapchains[surface_i].shaderVars.seek_value4 = filter_params->seek_values[3];
            mSwapchains[surface_i].shaderVars.seek_value5 = filter_params->seek_values[4];
            mSwapchains[surface_i].shaderVars.seek_value6 = filter_params->seek_values[5];
//            mSwapchains[surface_i].shaderVars.seek_value7 = filter_params->seek_values[6];
//            mSwapchains[surface_i].shaderVars.seek_value8 = filter_params->seek_values[7];
//            mSwapchains[surface_i].shaderVars.seek_value9 = filter_params->seek_values[8];
//            mSwapchains[surface_i].shaderVars.seek_value10 = filter_params->seek_values[9];

//        logd("checking seek_value8: %d",mSwapchains[surface_i].shaderVars.seek_value8);
//        logd("Surface %d Ima4geReader width: %d, height %d.", surface_i, mImageReaderWidth, mImageReaderHeight);
//        logd("Surface %d Surface width: %d, height %d.", surface_i, mSwapchains[surface_i].shaderVars.windowWidth, mSwapchains[surface_i].shaderVars.windowHeight);

            // Create a bitmask of enabled filters using first NUM_FILTERS of the int
            mSwapchains[surface_i].shaderVars.use_filter = 0;
            for (int i = 0; i < NUM_FILTERS; i++) {
                mSwapchains[surface_i].shaderVars.use_filter <<= 1;
                if (filter_params->use_filter[NUM_FILTERS - 1 - i]) {
                    mSwapchains[surface_i].shaderVars.use_filter += 1;
                }
//            logd("Filter %d is now set to: %d", i, mSwapchains[surface_i].shaderVars.use_filter);
            }

            // Current time value. Actually just a frame counter - shaders only need an increasing value, not real time
            // Reset every 30mins to prevent drift of sin and cos calculations
            mTimeValue++;
            if (mTimeValue >= 3600 * 15)
                mTimeValue = 0;
            mSwapchains[surface_i].shaderVars.time_value = mTimeValue;

            void* mappedShaderVarsData;
            vkMapMemory(mInstance->device(), mSwapchains[surface_i].shaderVarsMemory[mSwapchains[surface_i].mSwapchainIndex], 0, sizeof(mSwapchains[surface_i].shaderVars), 0, &mappedShaderVarsData);
            memcpy(mappedShaderVarsData, &mSwapchains[surface_i].shaderVars, sizeof(mSwapchains[surface_i].shaderVars));
            vkUnmapMemory(mInstance->device(), mSwapchains[surface_i].shaderVarsMemory[mSwapchains[surface_i].mSwapchainIndex]);

            VkDescriptorBufferInfo shaderVarsDescriptorBufferInfo = {
                    .buffer = mSwapchains[surface_i].shaderVarsBuffers[mSwapchains[surface_i].mSwapchainIndex],
                    .offset = 0,
                    .range = sizeof(ShaderVars),
            };

            VkWriteDescriptorSet shaderVarsWrite[1];
            shaderVarsWrite[0] = {};
            shaderVarsWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            shaderVarsWrite[0].pNext = NULL;
            shaderVarsWrite[0].dstSet = mDescriptorSets[surface_i][mSwapchains[surface_i].mSwapchainIndex];
            shaderVarsWrite[0].descriptorCount = 1;
            shaderVarsWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            shaderVarsWrite[0].pBufferInfo = &shaderVarsDescriptorBufferInfo;
            shaderVarsWrite[0].dstArrayElement = 0;
            shaderVarsWrite[0].dstBinding = 2;

            vkUpdateDescriptorSets(mInstance->device(), 1, shaderVarsWrite, 0, NULL);
        }

        // Update the fragment descriptor sets.
        {
            VkDescriptorImageInfo texDesc[2] {
                    {
                        .sampler = vkAHB->sampler(),
                        .imageView = vkAHB->view(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                    {
                        .sampler = mRgbSampler,
                        .imageView = swapchainImage->imageViewPrevious,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
            };

            VkWriteDescriptorSet writeDst[2] {
                // Sample from camera
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = mDescriptorSets[surface_i][mSwapchains[surface_i].mSwapchainIndex],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &texDesc[0],
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr},

                // Sample from previous frame
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = mDescriptorSets[surface_i][mSwapchains[surface_i].mSwapchainIndex],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &texDesc[1],
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr},
            };

            // Only use a previous frame if one has been recorded and blur filter is engaged
            if (filter_params->use_filter[BLUR_BUTTON]) {
                vkUpdateDescriptorSets(mInstance->device(), 2, writeDst, 0, nullptr);
            } else {
                vkUpdateDescriptorSets(mInstance->device(), 1, writeDst, 0, nullptr);
            }
        }
        ATrace_endSection();


        ATrace_beginSection("VULKAN_PHOTOBOOTH: render begin command buffer");

        // Begin Command Buffer (separate buffer for each surface
        {
            VkCommandBufferBeginInfo cmdBufferBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                    .pInheritanceInfo = nullptr,
            };

            VK_CALL(vkBeginCommandBuffer(swapchainImage->cmdBuffer, &cmdBufferBeginInfo));
        }

        ATrace_endSection();

        ATrace_beginSection("VULKAN_PHOTOBOOTH: render transistion barriers");

        // Acquire the AHB image resource so it can be sampled from
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, vkAHB->image(),
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VULKAN_QUEUE_FAMILY, mInstance->queueFamilyIndex());

        if (filter_params->use_filter[BLUR_BUTTON]) {
            // Transition the N-1 frame to read from
            addImageTransitionBarrier(
                    swapchainImage->cmdBuffer, swapchainImage->imagePrevious,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VULKAN_QUEUE_FAMILY, mInstance->queueFamilyIndex());
        }

        // Transition the destination texture for use as a framebuffer.
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->image,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VULKAN_QUEUE_FAMILY, mInstance->queueFamilyIndex());


        ATrace_endSection();
        ATrace_beginSection("VULKAN_PHOTOBOOTH: render Render pass");

        // Begin Render Pass to draw the source resource to the framebuffer.
        {
            const VkClearValue clearValue{
                    .color =
                            {
                                    .float32 = {0.0f, 0.0f, 0.0f, 0.0f},
                            },
            };

           VkRenderPassBeginInfo renderPassBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .pNext = nullptr,
                    .renderPass = mRenderPass,
                    .framebuffer = swapchainImage->framebuffer,
                    .renderArea = {{0, 0}, {mSurfaces[surface_i].mOutputWidth, mSurfaces[surface_i].mOutputHeight}},
                    .clearValueCount = 1u,
                    .pClearValues = &clearValue,
            };
            vkCmdBeginRenderPass(swapchainImage->cmdBuffer, &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);

        }
        ATrace_endSection();
        ATrace_beginSection("VULKAN_PHOTOBOOTH: render draw textures");

//        logd("surface: %d, sets size: %u, swapchain index: %d", surface_i, (int) mDescriptorSets[surface_i].size(), mSwapchains[surface_i].mSwapchainIndex);

        /// Draw texture to renderpass.
        vkCmdBindPipeline(swapchainImage->cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[surface_i]);
        vkCmdBindDescriptorSets(swapchainImage->cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mLayout,
                0, 1,
                &mDescriptorSets[surface_i][mSwapchains[surface_i].mSwapchainIndex], 0, nullptr);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(swapchainImage->cmdBuffer, 0, 1, &mVertexBuffer, &offset);
        vkCmdDraw(swapchainImage->cmdBuffer, 6, 1, 0, 0);
        vkCmdEndRenderPass(swapchainImage->cmdBuffer);

        ATrace_endSection();
        ATrace_beginSection("VULKAN_PHOTOBOOTH: render queue swapchain");

        // Finished reading the AHB
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, vkAHB->image(),
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VULKAN_QUEUE_FAMILY, mInstance->queueFamilyIndex());

        // Finished reading from the N-1 frame
        if (filter_params->use_filter[BLUR_BUTTON]) {
            addImageTransitionBarrier(
                    swapchainImage->cmdBuffer, swapchainImage->imagePrevious,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VULKAN_QUEUE_FAMILY, mInstance->queueFamilyIndex());
        }

        // Finished writing to the frame buffer
        addImageTransitionBarrier(
                swapchainImage->cmdBuffer, swapchainImage->image,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VULKAN_QUEUE_FAMILY, mInstance->queueFamilyIndex());

        ATrace_beginSection("VULKAN_PHOTOBOOTH: render end buffer");
        VK_CALL(vkEndCommandBuffer(swapchainImage->cmdBuffer));
        ATrace_endSection();

        // Set up present semaphore
        VkSemaphore semaphore = vkAHB->semaphore();
        VkPipelineStageFlags semaphoreWaitFlags =
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        VkSubmitInfo queueSubmitInfo = {
                .pNext = nullptr,
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = semaphore != VK_NULL_HANDLE ? 1u : 0u,
                .pWaitSemaphores = semaphore != VK_NULL_HANDLE ? &semaphore : nullptr,
                .pWaitDstStageMask = semaphore != VK_NULL_HANDLE ? &semaphoreWaitFlags : nullptr,
                .commandBufferCount = 1,
                .pCommandBuffers = &swapchainImage->cmdBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &swapchainImage->presentSemaphore,
        };

        // Keep track of which swapchain being rendered to correct N-1 frame can be retrieved
        mPrevFrameSwapchainIndex = mSwapchains[0].mSwapchainIndex;

        swapchainImage->imageFenceSet = true;
        VK_CALL(vkQueueSubmit(mInstance->queue(), 1, &queueSubmitInfo, swapchainImage->imageFence));
    } // For all surfaces

    // Queues have been set up and submitted. Now set up presentation semaphores
    VkSemaphore *waitSemaphores = new VkSemaphore[VULKAN_RENDERER_NUM_DISPLAYS];
    VkSwapchainKHR *presentSwapchains = new VkSwapchainKHR[VULKAN_RENDERER_NUM_DISPLAYS];
    uint32_t *presentSwapchainIndices = new uint32_t[VULKAN_RENDERER_NUM_DISPLAYS];

    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {
        SwapchainImage *swapchainImage = &mSwapchains[surface_i].mSwapchainImages[mSwapchains[surface_i].mSwapchainIndex];
        waitSemaphores[surface_i] = swapchainImage->presentSemaphore;
        presentSwapchains[surface_i] = mSwapchains[surface_i].mVkSwapchain;
        presentSwapchainIndices[surface_i] = mSwapchains[surface_i].mSwapchainIndex;
    }

    VkResult swapchain_result = VK_SUCCESS;
    VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = VULKAN_RENDERER_NUM_DISPLAYS,
            .pWaitSemaphores = waitSemaphores,
            .swapchainCount = VULKAN_RENDERER_NUM_DISPLAYS,
            .pSwapchains = presentSwapchains,
            .pImageIndices = presentSwapchainIndices,
            &swapchain_result,
    };
    swapchain_result = vkQueuePresentKHR (mInstance->queue(), &presentInfo);

    switch (swapchain_result) {
        case VK_SUCCESS:
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            break;
        default:
            logd("vkQueuePresent FAILED and returned:: %d", swapchain_result);
    }

    ATrace_endSection();

    // TODO: measure this correctly
    double fence_delay = 0;
    return fence_delay;
}

void VulkanImageRenderer::cleanUpPipelineTemporaries() {
    if (mDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mInstance->device(), mDescriptorLayout, nullptr);
        mDescriptorLayout = VK_NULL_HANDLE;
    }
    if (mLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(mInstance->device(), mLayout, nullptr);
        mLayout = VK_NULL_HANDLE;
    }

    for (int surface_i = 0; surface_i < VULKAN_RENDERER_NUM_DISPLAYS; surface_i++) {
        if (mPipelines[surface_i] != VK_NULL_HANDLE) {
            vkDestroyPipeline(mInstance->device(), mPipelines[surface_i], nullptr);
            mPipelines[surface_i] = VK_NULL_HANDLE;
        }
        if (mCmdBuffers[surface_i] != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(mInstance->device(), mCmdPool, 1, &mCmdBuffers[surface_i]);
            mCmdBuffers[surface_i] = VK_NULL_HANDLE;
        }
    }
}