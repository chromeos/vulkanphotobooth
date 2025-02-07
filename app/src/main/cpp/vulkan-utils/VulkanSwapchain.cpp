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

#include "VulkanSwapchain.h"
#include "vulkan_utils.h"

uint32_t VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH = 500;
uint32_t VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT = 500;


/**
 * Set up all the uniform memory buffers for the shaders. One for each swapchain image.
 */
void VulkanSwapchain::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(ShaderVars);

    shaderVarsBuffers.resize(mSwapchainLength);
    shaderVarsMemory.resize(mSwapchainLength);

    for (size_t i = 0; i < mSwapchainLength; i++) {
        createBuffer(mInstance, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &shaderVarsBuffers[i], &shaderVarsMemory[i]);
    }
}

VulkanSwapchain::VulkanSwapchain(VulkanInstance *instance, VkCommandPool *cmdPool) {
    mInstance = instance;
    mCmdPool = cmdPool;
}

VulkanSwapchain::~VulkanSwapchain() {
    for (int i = 0; i < mSwapchainLength; i++) {
        if (mSwapchainImages[i].framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(mInstance->device(), mSwapchainImages[i].framebuffer, nullptr);
            mSwapchainImages[i].framebuffer = VK_NULL_HANDLE;
        }
        if (mSwapchainImages[i].imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(mInstance->device(), mSwapchainImages[i].imageView, nullptr);
            mSwapchainImages[i].imageView = VK_NULL_HANDLE;
        }
        if (mSwapchainImages[i].imageCopy != VK_NULL_HANDLE) {
            vkDestroyImage(mInstance->device(), mSwapchainImages[i].imageCopy, nullptr);
            mSwapchainImages[i].imageCopy = VK_NULL_HANDLE;
        }
        if (mSwapchainImages[i].presentSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(mInstance->device(), mSwapchainImages[i].presentSemaphore, nullptr);
            mSwapchainImages[i].presentSemaphore = VK_NULL_HANDLE;
        }
        if (mSwapchainImages[i].copySemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(mInstance->device(), mSwapchainImages[i].copySemaphore, nullptr);
            mSwapchainImages[i].copySemaphore = VK_NULL_HANDLE;
        }
        if (mSwapchainImages[i].imageFence != VK_NULL_HANDLE) {
            vkDestroyFence(mInstance->device(), mSwapchainImages[i].imageFence, nullptr);
            mSwapchainImages[i].imageFence = VK_NULL_HANDLE;
        }
        if (mSwapchainFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(mInstance->device(), mSwapchainFences[i], nullptr);
            mSwapchainFences[i] = VK_NULL_HANDLE;
        }
        if (mSwapchainImages[i].cmdBuffer != VK_NULL_HANDLE) {
//            vkFreeCommandBuffers(mInstance->device(), *mCmdPool, 1, &mSwapchainImages[i].cmdBuffer);
            mSwapchainImages[i].cmdBuffer = VK_NULL_HANDLE;
        }
        if (shaderVarsBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(mInstance->device(), shaderVarsBuffers[i], nullptr);
            shaderVarsBuffers[i] = VK_NULL_HANDLE;
        }
        if (shaderVarsMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(mInstance->device(), shaderVarsMemory[i], nullptr);
            shaderVarsMemory[i] = VK_NULL_HANDLE;
        }
        if (i >= (mSwapchainLength - 1)) {
            if (mSwapchainImages[i].old_aimage != nullptr) {
                AImage_delete(mSwapchainImages[i].old_aimage);
                mSwapchainImages[i].old_aimage = nullptr;
            }
        }
    } // For all swapchains
}

bool VulkanSwapchain::init(VkSurfaceKHR *vkSurface, VkSurfaceCapabilitiesKHR *surfaceCaps, VkSurfaceFormatKHR *format,
                           uint32_t windowWidth, uint32_t windowHeight,  uint32_t imageReaderWidth, uint32_t imageReaderHeight,
                           uint32_t queueIndex, VkCompositeAlphaFlagBitsKHR *alphaFlags,
                           VkRenderPass *renderPass,
                           uint32_t rendererCopyWidth, uint32_t rendererCopyHeight) {

    // Set render copy width/height accounting for aspect ratio
    VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH = rendererCopyWidth;

    if (0 == rendererCopyHeight) {
        VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT = RENDERER_COPY_IMAGE_WIDTH * (float(windowHeight) / float(windowWidth));
    } else {
        VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT = rendererCopyHeight;
    }

    const int NUM_VKIMAGES_DESIRED_PER_SWAPCHAIN = 6;
    // We'd like 6 (or more) images in the swapchain, if the surface can handle it
    uint32_t numImagesDesired = surfaceCaps->minImageCount;
    if (NUM_VKIMAGES_DESIRED_PER_SWAPCHAIN <= surfaceCaps->maxImageCount
        && NUM_VKIMAGES_DESIRED_PER_SWAPCHAIN > surfaceCaps->minImageCount) {
        numImagesDesired = NUM_VKIMAGES_DESIRED_PER_SWAPCHAIN;
    }

    VkSwapchainCreateInfoKHR swapchainCreateinfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = *vkSurface,
//            .minImageCount = surfaceCaps->minImageCount,
            .minImageCount = numImagesDesired,
            .imageFormat = format->format,
            .imageColorSpace = format->colorSpace,
            .imageExtent = VkExtent2D {
                    .width = windowWidth,
                    .height = windowHeight,
            },
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queueIndex,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = *alphaFlags,

            // The Vulkan spec requires that all surfaces support fifo mode.
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = false,
            .oldSwapchain = VK_NULL_HANDLE,
    };
    VK_CALL(vkCreateSwapchainKHR(mInstance->device(), &swapchainCreateinfo, nullptr, &mVkSwapchain));

    VK_CALL(vkGetSwapchainImagesKHR(mInstance->device(), mVkSwapchain, &mSwapchainLength, nullptr));
    auto *swapchainImages = new VkImage[mSwapchainLength];
    VK_CALL(vkGetSwapchainImagesKHR(mInstance->device(), mVkSwapchain, &mSwapchainLength, swapchainImages));

    logd("Min image count: %d", surfaceCaps->minImageCount);
    logd("Max image count: %d", surfaceCaps->maxImageCount);
    logd("Swapchain length: %d", mSwapchainLength);


    mSwapchainImages = new SwapchainImage[mSwapchainLength];
    mSwapchainFences = new VkFence[mSwapchainLength];

    // Allocate VkImageViews and Framebuffers
    for (uint32_t i = 0; i < mSwapchainLength; ++i) {
        VkImage image = swapchainImages[i];

        // Set of framebuffer objects for presentation
        VkImageView imageView;
        VkImageViewCreateInfo imageViewCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format->format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = (VkImageSubresourceRange) {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
        };
        VK_CALL(vkCreateImageView(mInstance->device(), &imageViewCreateInfo, nullptr, &imageView));


        VkSemaphore presentSemaphore;
        VkSemaphoreCreateInfo semaphoreCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VK_CALL(vkCreateSemaphore(mInstance->device(), &semaphoreCreateInfo, nullptr, &presentSemaphore));

        VkFence imageFence;
        VkFenceCreateInfo fenceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
        };
        VK_CALL(vkCreateFence(mInstance->device(), &fenceCreateInfo, nullptr, &imageFence));
        VK_CALL(vkResetFences(mInstance->device(), 1, &imageFence));



        // Set of framebuffer objects for copying out images
        VkImage imageCopy;
        VkImageCreateInfo imageCopyCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
//                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .format = VK_FORMAT_R5G6B5_UNORM_PACK16,
//                .format = VK_FORMAT_A8B8G8R8_UINT_PACK32,
                .extent =
                        {
                            RENDERER_COPY_IMAGE_WIDTH, RENDERER_COPY_IMAGE_HEIGHT, 1,
                        },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_LINEAR,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VK_CALL(vkCreateImage(mInstance->device(), &imageCopyCreateInfo, nullptr, &imageCopy));

        VkMemoryRequirements imageCopyMemRequirements;
        vkGetImageMemoryRequirements(mInstance->device(), imageCopy, &imageCopyMemRequirements);

        VkMemoryAllocateInfo imageCopyAllocInfo = {};
        imageCopyAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        imageCopyAllocInfo.allocationSize = imageCopyMemRequirements.size;
        imageCopyAllocInfo.memoryTypeIndex = mInstance->findMemoryType(imageCopyMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDeviceMemory imageCopyMemory;
        VK_CALL(vkAllocateMemory(mInstance->device(), &imageCopyAllocInfo, nullptr, &imageCopyMemory) != VK_SUCCESS);
        vkBindImageMemory(mInstance->device(), imageCopy, imageCopyMemory, 0);

        VkSemaphore copySemaphore;
        VkSemaphoreCreateInfo copySemaphoreCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VK_CALL(vkCreateSemaphore(mInstance->device(), &copySemaphoreCreateInfo, nullptr, &copySemaphore));

        VkFence imageCopyFence;
        VkFenceCreateInfo copyFenceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
        };
        VK_CALL(vkCreateFence(mInstance->device(), &copyFenceCreateInfo, nullptr, &imageCopyFence));
        VK_CALL(vkResetFences(mInstance->device(), 1, &imageCopyFence));


        // For storing N-1 frame
        VkImage imagePrevious;
        VkImageCreateInfo imagePreviousCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format->format,
                .extent = { windowWidth, windowHeight, 1, },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VK_CALL(vkCreateImage(mInstance->device(), &imagePreviousCreateInfo, nullptr, &imagePrevious));

        VkMemoryRequirements imagePreviousMemRequirements;
        vkGetImageMemoryRequirements(mInstance->device(), imagePrevious, &imagePreviousMemRequirements);

        VkMemoryAllocateInfo imagePreviousAllocInfo = {};
        imagePreviousAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        imagePreviousAllocInfo.allocationSize = imagePreviousMemRequirements.size;
        imagePreviousAllocInfo.memoryTypeIndex = mInstance->findMemoryType(imagePreviousMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory imagePreviousMemory;
        VK_CALL(vkAllocateMemory(mInstance->device(), &imagePreviousAllocInfo, nullptr, &imagePreviousMemory) != VK_SUCCESS);
        vkBindImageMemory(mInstance->device(), imagePrevious, imagePreviousMemory, 0);

        VkImageView imageViewPrevious;
        VkImageViewCreateInfo imageViewPreviousCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .image = imagePrevious,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format->format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = (VkImageSubresourceRange) {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
        };
        VK_CALL(vkCreateImageView(mInstance->device(), &imageViewPreviousCreateInfo, nullptr, &imageViewPrevious));

        VkFence imagePreviousFence;
        VkFenceCreateInfo imagePreviousFenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
        };
        VK_CALL(vkCreateFence(mInstance->device(), &imagePreviousFenceInfo, nullptr, &imagePreviousFence));
        VK_CALL(vkResetFences(mInstance->device(), 1, &imagePreviousFence));



        VkImageView framebufferImageViews[1] = {
                imageView,
        };

        VkFramebuffer framebuffer;
        VkFramebufferCreateInfo framebufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = *renderPass,
                .attachmentCount = 1,
                .pAttachments = framebufferImageViews,
                .width = windowWidth,
                .height = windowHeight,
                .layers = 1,
        };
        VK_CALL(vkCreateFramebuffer(mInstance->device(), &framebufferCreateInfo, nullptr, &framebuffer));


        VkCommandBuffer cmdBuffer;
        VkCommandBufferAllocateInfo cmdBufferCreateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = *mCmdPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
        };
        VK_CALL(vkAllocateCommandBuffers(mInstance->device(), &cmdBufferCreateInfo,
                                         &cmdBuffer));

        // Swapchain fun!
        VkFenceCreateInfo swapchainFenceInfo =  {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
        };
        VK_CALL(vkCreateFence(mInstance->device(), &swapchainFenceInfo, nullptr, &mSwapchainFences[i]));

        mSwapchainImages[i] = SwapchainImage {
                .index = i,

                .image = image,
                .imageView = imageView,
                .presentSemaphore = presentSemaphore,
                .framebuffer = framebuffer,
                .imageFence = imageFence,
                .imageFenceSet = false,

                .imageCopy = imageCopy,
                .imageCopyMemory = imageCopyMemory,
                .copySemaphore = copySemaphore,
                .imageCopyFence = imageCopyFence,

                .imagePrevious = imagePrevious,
                .imageViewPrevious = imageViewPrevious,
                .imagePreviousMemory = imagePreviousMemory,
                .imagePreviousFence = imagePreviousFence,

                .cmdBuffer = cmdBuffer,
                .old_aimage = nullptr,
        };
    }

    // Create shader variables
    createUniformBuffers();

    shaderVars.imageWidth = imageReaderWidth;
    shaderVars.imageHeight = imageReaderHeight;
    shaderVars.windowWidth = windowWidth;
    shaderVars.windowHeight = windowHeight;

    shaderVars.rotation = 90; // Default to portrait screen with landscape sensor, update each frame

    // Calculate these once rather than recalculating every frame
    shaderVars.distortion_correction_normal = (float(imageReaderWidth) * float(windowHeight)) /  (float(imageReaderHeight) * float(windowWidth)); // (Iw * Wh) / (Ih * Ww);
    shaderVars.distortion_correction_rotated = (float(imageReaderHeight) * float(windowHeight)) /  (float(imageReaderWidth) * float(windowWidth)); // (Ih * Wh) / (Iw * Ww);

    // TODO: the above corrections work for portrait phones and landscape chrome os, but there are other possibilities
    // TODO: instead calculate ratio w/h IR and w/h screen. If IR ratio is bigger, above should work. Otherwise need to inverse something

    logd("Distortion Correction normal: %f, rotated: %f", shaderVars.distortion_correction_normal, shaderVars.distortion_correction_rotated);

    return true;
}