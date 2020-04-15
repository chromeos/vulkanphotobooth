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

#ifndef VULKAN_PHOTO_BOOTH_VULKANAHARDWAREBUFFERIMAGE_H
#define VULKAN_PHOTO_BOOTH_VULKANAHARDWAREBUFFERIMAGE_H

#include "VulkanInstance.h"

/**
 * Class to allow importing an AHardwareBuffer into Vulkan
 */
class VulkanAHardwareBufferImage {
public:
    VulkanAHardwareBufferImage(VulkanInstance *init);
    ~VulkanAHardwareBufferImage();

    /**
     * Setup the Vulkan AHB
     *
     * @param buffer The Android Hardware Buffer
     * @param useExternalFormat Whether to use an external format or a format pulled from the AHB
     * @param syncFd The AHBs sync file descriptor (-1 for none)
     * @return
     */
    bool init(AHardwareBuffer *buffer, bool useExternalFormat, int syncFd = -1);
    VkImage image() { return mImage; }
    VkSampler sampler() { return mSampler; }
    VkImageView view() { return mView; }
    VkSemaphore semaphore() { return mSemaphore; }
    bool isSamplerImmutable() { return mConversion != VK_NULL_HANDLE; }

private:
    VulkanInstance *const mInstance;
    VkImage mImage = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    VkSampler mSampler = VK_NULL_HANDLE;
    VkImageView mView = VK_NULL_HANDLE;
    VkSamplerYcbcrConversion mConversion = VK_NULL_HANDLE;
    VkSemaphore mSemaphore = VK_NULL_HANDLE;
};

#endif //VULKAN_PHOTO_BOOTH_VULKANAHARDWAREBUFFERIMAGE_H
