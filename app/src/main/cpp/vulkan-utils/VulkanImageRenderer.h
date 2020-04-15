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

#ifndef VULKAN_PHOTO_BOOTH_VULKANIMAGERENDERER_H
#define VULKAN_PHOTO_BOOTH_VULKANIMAGERENDERER_H

#include <media/NdkImage.h>
#include "VulkanInstance.h"
#include "VulkanAHardwareBufferImage.h"
#include "FilterParams.h"
#include "VulkanSwapchain.h"
#include "VulkanSurface.h"


enum RENDERER_RETURN_CODE { RENDER_STATE_NOT_SET, RENDER_FRAME_SENT, RENDER_QUEUE_NOT_EMPTY, RENDER_QUEUE_EMPTY };


/**
 * Given an image, apply the desired effects and render to the screen
 *
 * The main workhorse of the VulkanPipeline, responsible for managing blit-outs for GIF generation
 * and multi-frame effects as well as regular single frame filters
 */
class VulkanImageRenderer {
public:
    /**
     * Constructor
     *
     * @param instance Pre-initialized VulkanInstance
     * @param num_displays Between 1-3
     * @param width
     * @param height
     * @param format
     * @param colorSpace
     */
    VulkanImageRenderer(VulkanInstance *instance, uint32_t num_displays,
                        uint32_t width, uint32_t height,
                        VkFormat format, VkColorSpaceKHR);
    ~VulkanImageRenderer();

    /**
     * Set up the RenderPass and connects it to the given displays
     *
     * Sets up the rendering engine as much as possible without having an AHB
     *
     * @param output_window Native window for centre display
     * @param output_window_left Native window for left display
     * @param output_window_right Native window for right display
     * @param rendererCopyWidth Desired width of blit outs for GIF generation
     * @param rendererCopyHeight Desired height of blit outs for GIF generation
     * @return If initialization was successful
     */
    bool init(ANativeWindow *output_window, ANativeWindow *output_window_left, ANativeWindow *output_window_right,  uint32_t rendererCopyWidth = 0, uint32_t rendererCopyHeight = 0);

    /**
     * Set up the pipeline: shader setup, VkGraphicsPipeline, and VkCommandBuffer allocation
     *
     * This should be called after init and after the first AHB is received, but before the first
     * frame is processed and rendered.
     *
     * @param sampler The immutable sampler to use, if required (required for YUV->RGB)
     * @param useImmutableSampler Whether to use the provided immutable sampler or not
     * @return If pipeline was created successfully
     */
    bool createPipeline(VkSampler sampler, bool useImmutableSampler);

    /**
     * Take an image, send it to Vulkan, copy for GIFs or multi-frame effects if necessary.
     *
     * This is the main workhorse of the pipeline that processes each new frame image.
     *
     * @param new_vkAHB VulkanAHardwareBufferImage ready to be used with the new AImaged
     * @param filter_params Current filter paramters
     * @param new_aimage New AImage from the ImageReader
     * @param draw_to_screen Is Vulkan allowed to draw to the screen
     * @param surface_ready_left Is the left surface of 3 ready for drawing
     * @param surface_ready_right Is the right surface of 3 ready for drawing
     * @param render_state Current state (RENDER_STATE_NOT_SET, RENDER_FRAME_SENT, RENDER_QUEUE_NOT_EMPTY, RENDER_QUEUE_EMPTY)
     * @param image_copy_data If not null, the frame should be copied into the given, pre-allocated, memory
     * @return Time (in ms) that frame render took. NOTE: this does not work currently
     */
    double renderImageAndReadback(VulkanAHardwareBufferImage *new_vkAHB,
                                  FilterParams *filter_params, AImage *new_aimage,
                                  bool draw_to_screen, bool surface_ready_left, bool surface_ready_right,
                                  RENDERER_RETURN_CODE &render_state,
                                  uint32_t *image_copy_data);

    bool isPipelineInitialized = false;

    VkSampler mRgbSampler = VK_NULL_HANDLE; // Used for multi-frame effects

    const uint32_t VULKAN_RENDERER_NUM_DISPLAYS;
    const uint32_t mImageReaderWidth;
    const uint32_t mImageReaderHeight;

private:
    void cleanUpPipelineTemporaries();

    VulkanInstance *const mInstance;
    VkFormat mFormat;
    VkColorSpaceKHR mColorSpace;

    VkCommandPool mCmdPool = VK_NULL_HANDLE;
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkBuffer mVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mVertexBufferMemory = VK_NULL_HANDLE;
    VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
    VkShaderModule mVertModule = VK_NULL_HANDLE;
    VkShaderModule mFragModule = VK_NULL_HANDLE;

    std::vector<VulkanSwapchain> mSwapchains;
    std::vector<VulkanSurface> mSurfaces;
    std::vector<VkPipeline> mPipelines;
    std::vector<VkCommandBuffer> mCmdBuffers;
    std::vector<std::vector<VkDescriptorSet>> mDescriptorSets; // One per surface and one per swapchain image
    std::vector<VkDescriptorPool> mDescriptorPools;

    // Used for shader "time" - actually just a simple frame counter that always increases
    uint32_t mTimeValue = 0;

    // Previous frame's swapchain index (for use with multi-frame effects)
    int mPrevFrameSwapchainIndex = 0;

    // Temporary variables used during renderImageAndReadback.
    VkPipelineCache mCache = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDescriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout mLayout = VK_NULL_HANDLE;
};

#endif //VULKAN_PHOTO_BOOTH_VULKANIMAGERENDERER_H