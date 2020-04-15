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

#ifndef FUNHOUSE_PHOTO_BOOTH_VULKANSWAPCHAIN_H
#define FUNHOUSE_PHOTO_BOOTH_VULKANSWAPCHAIN_H

#include <vulkan/vulkan.h>
#include <media/NdkImage.h>
#include "VulkanAHardwareBufferImage.h"

/**
 * Data associated with a single frame in the swapchain
 */
struct SwapchainImage {
    uint32_t index;

    // Framebuffer variables for presentation
    VkImage image;
    VkImageView imageView;
    VkSemaphore presentSemaphore;
    VkFramebuffer framebuffer;
    bool imageFenceSet;
    VkFence imageFence;

    // Framebuffer variables that will copied out to the ring_buffer to be saved
    VkImage imageCopy;
    VkImageView imageViewCopy;
    VkDeviceMemory imageCopyMemory;
    VkSemaphore copySemaphore;
    VkFence imageCopyFence;

    // For multi-pass effects
    VkImage imagePrevious;
    VkImageView imageViewPrevious;
    VkDeviceMemory imagePreviousMemory;
    VkFence imagePreviousFence;

    VkCommandBuffer cmdBuffer;
    AImage *old_aimage;
    VulkanAHardwareBufferImage *old_vkAHB;
};

/**
 * Shader parameters
 */
struct ShaderVars {
    int panel_id = 0; // 0 == center, 1 == left, 2 == right
    int imageWidth = 0;
    int imageHeight = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    int rotation = 0; // degrees clockwise from right-side up
    int seek_value1 = 50;
    int seek_value2 = 50;
    int seek_value3 = 50;
    int seek_value4 = 50;
    int seek_value5 = 50;
    int seek_value6 = 50;
    int seek_value7 = 50;
    int seek_value8 = 50;
    int seek_value9 = 50;
    int seek_value10 = 50;
    int time_value = 0;
    float distortion_correction_normal = 0.0;
    float distortion_correction_rotated = 0.0;
    uint use_filter;
};

/**
 * Convenience class to work with each VkSwapchain
 *
 * Note, each display needs it's own swapchain
 */
class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanInstance *instance, VkCommandPool *cmdPool);
    ~VulkanSwapchain();

    /**
     * Initialize the swapchain for this surface
     *
     * @param vkSurface
     * @param surfaceCaps
     * @param format
     * @param windowWidth
     * @param windowHeight
     * @param imageReaderWidth
     * @param imageReaderHeight
     * @param queueIndex
     * @param alphaFlags
     * @param renderPass
     * @param rendererCopyWidth
     * @param rendererCopyHeight
     * @return
     */
    bool init(VkSurfaceKHR *vkSurface, VkSurfaceCapabilitiesKHR *surfaceCaps, VkSurfaceFormatKHR *format,
              uint32_t windowWidth, uint32_t windowHeight, uint32_t imageReaderWidth, uint32_t imageReaderHeight,
              uint32_t queueIndex, VkCompositeAlphaFlagBitsKHR *alphaFlags, VkRenderPass *renderPass,
              uint32_t rendererCopyWidth = 500, uint32_t rendererCopyHeight = 0);

    void createUniformBuffers();

    static uint32_t RENDERER_COPY_IMAGE_WIDTH;
    static uint32_t RENDERER_COPY_IMAGE_HEIGHT;

    VulkanInstance *mInstance;
    ShaderVars shaderVars = {};
    std::vector<VkBuffer> shaderVarsBuffers;
    std::vector<VkDeviceMemory> shaderVarsMemory;

    VkCommandPool *mCmdPool = VK_NULL_HANDLE;
    VkSwapchainKHR mVkSwapchain;
    uint32_t mSwapchainLength = 0;
    uint32_t mSwapchainIndex = 0;
    SwapchainImage *mSwapchainImages = VK_NULL_HANDLE;
    VkFence *mSwapchainFences = VK_NULL_HANDLE;
    uint32_t mSwapchainFenceIndex = 0;
};

#endif //FUNHOUSE_PHOTO_BOOTH_VULKANSWAPCHAIN_H