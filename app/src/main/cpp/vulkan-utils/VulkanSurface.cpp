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

#include <android/native_window.h>
#include "VulkanSurface.h"
#include "vulkan_utils.h"

VulkanSurface::VulkanSurface(VulkanInstance *instance, VkFormat *format, VkColorSpaceKHR *colorSpace)
    : mInstance(instance), mFormat(format), mColorSpace(colorSpace)
{}

VulkanSurface::~VulkanSurface() {
    if (mVkSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(mInstance->instance(), mVkSurface, nullptr);
        mVkSurface = VK_NULL_HANDLE;
    }
}

bool VulkanSurface::init(ANativeWindow *output_window) {
    mOutputWindow = output_window;
    mOutputWidth = uint32_t (ANativeWindow_getWidth(output_window));
    mOutputHeight = uint32_t (ANativeWindow_getHeight(output_window));

    logd("Surface Output Window Width: %d, Height %d.", mOutputWidth, mOutputHeight);

    // Main surface
    VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .flags = 0,
            .window = output_window,
    };
    VK_CALL(vkCreateAndroidSurfaceKHR(mInstance->instance(), &androidSurfaceCreateInfo, nullptr, &mVkSurface));

    VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mInstance->gpu(), mVkSurface, &mSurfaceCaps));

    uint32_t format_count = 0;
    VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(mInstance->gpu(), mVkSurface, &format_count, NULL));

    mFormats = new VkSurfaceFormatKHR[format_count];
    VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(mInstance->gpu(), mVkSurface, &format_count, mFormats));

    // Find the needed surface format
    uint32_t format_index = UINT32_MAX;
    for (uint32_t i = 0; i < format_count; ++i) {
        if ((mFormats[i].format == *mFormat )
            && (mFormats[i].colorSpace == *mColorSpace)) {
            format_index = i;
            break;
        }
    }
    if (format_index == UINT32_MAX) {
        logd("ERROR: VkSurface does not support VkSurfaceFormat{%d, %d}",
             *mFormat,
             *mColorSpace);
    } else {
        logd("Hooray: VkSurface supports VkSurfaceFormat{%d, %d}",
             *mFormat,
             *mColorSpace);
    }

    mSurfaceFormat = mFormats[format_index];

    //        logd("Caps min image count: %d. max image count: %d", mSurfaceCaps.minImageCount, mSurfaceCaps.maxImageCount);
    //        logd("Format count: %d. Queue family count: %d", format_count, queue_family_count);
    //        logd("Caps min image count: %d. max image count: %d", mSurfaceCaps.minImageCount, mSurfaceCaps.maxImageCount);
    //        logd("Caps comp alpha: %d", mSurfaceCaps.supportedCompositeAlpha);
    //        logd("Caps current width: %d current height: %d", mSurfaceCaps.currentExtent.width, mSurfaceCaps.currentExtent.height);

    mAlphaFlags = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
    switch (mSurfaceCaps.supportedCompositeAlpha) {
        case VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:
            mAlphaFlags = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            break;
        case VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR:
            mAlphaFlags = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
            break;
        case VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR:
            mAlphaFlags = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
            break;
        case VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR:
            mAlphaFlags = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
            break;
    }

    return true;
}
