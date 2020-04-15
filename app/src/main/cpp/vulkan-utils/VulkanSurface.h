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

#ifndef VULKAN_PHOTO_BOOTH_VULKANSURFACE_H
#define VULKAN_PHOTO_BOOTH_VULKANSURFACE_H


#include <vulkan/vulkan.h>
#include "VulkanInstance.h"

/**
 * Helper class to bind a native window to a VkSurface
 */
class VulkanSurface {
public:
    VulkanSurface(VulkanInstance *instance, VkFormat *format, VkColorSpaceKHR *colorSpace);
    ~VulkanSurface();
    bool init(ANativeWindow *output_window);

    ANativeWindow *mOutputWindow;
    VulkanInstance *mInstance = nullptr;
    VkFormat *mFormat = VK_NULL_HANDLE;
    VkColorSpaceKHR *mColorSpace = VK_NULL_HANDLE;
    VkSurfaceKHR mVkSurface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR *mFormats = VK_NULL_HANDLE;
    VkSurfaceFormatKHR mSurfaceFormat;
    VkSurfaceCapabilitiesKHR mSurfaceCaps;
    VkCompositeAlphaFlagBitsKHR mAlphaFlags;
    uint32_t mOutputWidth;
    uint32_t mOutputHeight;

};


#endif //VULKAN_PHOTO_BOOTH_VULKANSURFACE_H
