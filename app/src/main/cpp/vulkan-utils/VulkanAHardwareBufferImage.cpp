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

#include <android/hardware_buffer.h>
#include "VulkanAHardwareBufferImage.h"
#include "vulkan_utils.h"

VulkanAHardwareBufferImage::VulkanAHardwareBufferImage(VulkanInstance *init) : mInstance(init) {
}

bool VulkanAHardwareBufferImage::init(AHardwareBuffer *buffer, bool useExternalFormat, int syncFd) {
    AHardwareBuffer_Desc bufferDesc;
    AHardwareBuffer_describe(buffer, &bufferDesc);
    ASSERT(bufferDesc.layers == 1);

    // Get the AHB propertoes
    VkAndroidHardwareBufferFormatPropertiesANDROID formatInfo = {
            .sType =
            VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
            .pNext = nullptr,
    };
    VkAndroidHardwareBufferPropertiesANDROID properties = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
            .pNext = &formatInfo,
    };
    VK_CALL(mInstance->getHardwareBufferPropertiesFn()(mInstance->device(), buffer,
                                                   &properties));
    ASSERT(useExternalFormat || formatInfo.format != VK_FORMAT_UNDEFINED);

    // Create a VkImage to bind to the AHardwareBuffer.
    VkExternalFormatANDROID externalFormat{
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
            .pNext = nullptr,
            .externalFormat = formatInfo.externalFormat,
    };
    VkExternalMemoryImageCreateInfo externalCreateInfo{
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .pNext = useExternalFormat ? &externalFormat : nullptr,
            .handleTypes =
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };
    VkImageCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = &externalCreateInfo,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = useExternalFormat ? VK_FORMAT_UNDEFINED : formatInfo.format,
            .extent =
                    {
                            bufferDesc.width, bufferDesc.height, 1,
                    },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VK_CALL(vkCreateImage(mInstance->device(), &createInfo, nullptr, &mImage));

    // Set up and allocate the memory in the GPU
    VkImportAndroidHardwareBufferInfoANDROID androidHardwareBufferInfo{
            .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
            .pNext = nullptr,
            .buffer = buffer,
    };
    VkMemoryDedicatedAllocateInfo memoryAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .pNext = &androidHardwareBufferInfo,
            .image = mImage,
            .buffer = VK_NULL_HANDLE,
    };
    VkMemoryAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &memoryAllocateInfo,
            .allocationSize = properties.allocationSize,
            .memoryTypeIndex = mInstance->findMemoryType(
                    properties.memoryTypeBits, 0u),
    };
    VK_CALL(vkAllocateMemory(mInstance->device(), &allocateInfo, nullptr, &mMemory));

    // Bind the GPU memory to the AHB
    VkBindImageMemoryInfo bindImageInfo;
    bindImageInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
    bindImageInfo.pNext = nullptr;
    bindImageInfo.image = mImage;
    bindImageInfo.memory = mMemory;
    bindImageInfo.memoryOffset = 0;

    PFN_vkBindImageMemory2KHR bindImageMemory =
            (PFN_vkBindImageMemory2KHR)vkGetDeviceProcAddr(mInstance->device(),
                                                           "vkBindImageMemory2KHR");
    ASSERT(bindImageMemory);
    VK_CALL(bindImageMemory(mInstance->device(), 1, &bindImageInfo));

    // Check memory requirements for device/image
    VkImageMemoryRequirementsInfo2 memReqsInfo;
    memReqsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
    memReqsInfo.pNext = nullptr;
    memReqsInfo.image = mImage;

    VkMemoryDedicatedRequirements dedicatedMemReqs;
    dedicatedMemReqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
    dedicatedMemReqs.pNext = nullptr;

    VkMemoryRequirements2 memReqs;
    memReqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memReqs.pNext = &dedicatedMemReqs;

    PFN_vkGetImageMemoryRequirements2KHR getImageMemoryRequirements =
            (PFN_vkGetImageMemoryRequirements2KHR)vkGetDeviceProcAddr(
                    mInstance->device(), "vkGetImageMemoryRequirements2KHR");
    ASSERT(getImageMemoryRequirements);
    getImageMemoryRequirements(mInstance->device(), &memReqsInfo, &memReqs);
    ASSERT(VK_TRUE == dedicatedMemReqs.prefersDedicatedAllocation);
    ASSERT(VK_TRUE == dedicatedMemReqs.requiresDedicatedAllocation);

    // Setup sampler to convert YUV -> RGB
    if (useExternalFormat) {
        VkSamplerYcbcrConversionCreateInfo conversionCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
                .pNext = &externalFormat,
                .format = useExternalFormat ? VK_FORMAT_UNDEFINED : formatInfo.format,
                .ycbcrModel = formatInfo.suggestedYcbcrModel,
                .ycbcrRange = formatInfo.suggestedYcbcrRange,
                .components = formatInfo.samplerYcbcrConversionComponents,
                .xChromaOffset = formatInfo.suggestedXChromaOffset,
                .yChromaOffset = formatInfo.suggestedYChromaOffset,
                .chromaFilter = VK_FILTER_NEAREST,
                .forceExplicitReconstruction = VK_FALSE,
        };
        PFN_vkCreateSamplerYcbcrConversionKHR createSamplerYcbcrConversion =
                (PFN_vkCreateSamplerYcbcrConversionKHR)vkGetDeviceProcAddr(
                        mInstance->device(), "vkCreateSamplerYcbcrConversionKHR");
        ASSERT(createSamplerYcbcrConversion);
        VK_CALL(createSamplerYcbcrConversion(mInstance->device(), &conversionCreateInfo,
                                             nullptr, &mConversion));
    }
    VkSamplerYcbcrConversionInfo samplerConversionInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
            .pNext = &externalFormat,
            .conversion = mConversion,
    };

    // Configure the sampler parameters. Mirrored repeat adds to "funhouse" mirror effect
    VkSamplerCreateInfo samplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext =
            (mConversion == VK_NULL_HANDLE) ? nullptr : &samplerConversionInfo,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            // Address modes must be VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CALL(vkCreateSampler(mInstance->device(), &samplerCreateInfo, nullptr, &mSampler));


    // Set up the VkImageView for the VkImage
    VkImageViewCreateInfo viewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext =
            (mConversion == VK_NULL_HANDLE) ? nullptr : &samplerConversionInfo,
            .image = mImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = useExternalFormat ? VK_FORMAT_UNDEFINED : formatInfo.format,
            .components =
                    {
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .flags = 0,
    };
    VK_CALL(vkCreateImageView(mInstance->device(), &viewCreateInfo, nullptr, &mView));

    // Create semaphore if necessary.
    if (syncFd != -1) {
        VkSemaphoreCreateInfo semaphoreCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
        };
        VK_CALL(vkCreateSemaphore(mInstance->device(), &semaphoreCreateInfo, nullptr,
                                  &mSemaphore));

        // Import the fd into a semaphore.
        VkImportSemaphoreFdInfoKHR importSemaphoreInfo{
                .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
                .pNext = nullptr,
                .semaphore = mSemaphore,
                .flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT,
                .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                .fd = syncFd,
        };

        PFN_vkImportSemaphoreFdKHR importSemaphoreFd =
                (PFN_vkImportSemaphoreFdKHR)vkGetDeviceProcAddr(
                        mInstance->device(), "vkImportSemaphoreFdKHR");
        ASSERT(importSemaphoreFd);
        VK_CALL(importSemaphoreFd(mInstance->device(), &importSemaphoreInfo));
    }

    return true;
}

VulkanAHardwareBufferImage::~VulkanAHardwareBufferImage() {
    if (mSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(mInstance->device(), mSemaphore, nullptr);
        mSemaphore = VK_NULL_HANDLE;
    }
    if (mView != VK_NULL_HANDLE) {
        vkDestroyImageView(mInstance->device(), mView, nullptr);
        mView = VK_NULL_HANDLE;
    }
    if (mSampler != VK_NULL_HANDLE) {
        vkDestroySampler(mInstance->device(), mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }
    if (mConversion != VK_NULL_HANDLE) {
        PFN_vkDestroySamplerYcbcrConversionKHR destroySamplerYcbcrConversion =
                (PFN_vkDestroySamplerYcbcrConversionKHR)vkGetDeviceProcAddr(
                        mInstance->device(), "vkDestroySamplerYcbcrConversionKHR");
        destroySamplerYcbcrConversion(mInstance->device(), mConversion, nullptr);
    }
    if (mImage != VK_NULL_HANDLE) {
        vkDestroyImage(mInstance->device(), mImage, nullptr);
        mImage = VK_NULL_HANDLE;
    }
    if (mMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mInstance->device(), mMemory, nullptr);
        mMemory = VK_NULL_HANDLE;
    }
}