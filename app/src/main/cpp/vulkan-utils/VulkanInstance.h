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

#ifndef VULKAN_PHOTO_BOOTH_VULKANINSTANCE_H
#define VULKAN_PHOTO_BOOTH_VULKANINSTANCE_H

#include <vulkan/vulkan.h>
#include <vector>

//#define DEBUG_BUILD
#ifdef DEBUG_BUILD
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
};

class VulkanInstance {
    public:
        ~VulkanInstance();

        bool init();
        VkDevice device() { return mDevice; }
        VkQueue queue() { return mQueue; }
        VkPhysicalDevice gpu() { return mGpu; }
        VkInstance instance() { return mInstance; }
        uint32_t queueFamilyIndex() { return mQueueFamilyIndex; }
        PFN_vkGetAndroidHardwareBufferPropertiesANDROID
        getHardwareBufferPropertiesFn() {
            return mPfnGetAndroidHardwareBufferPropertiesANDROID;
        }

        uint32_t findMemoryType(uint32_t memoryTypeBitsRequirement,
                                VkFlags requirementsMask);
        VkDebugReportCallbackEXT mDebugReportCallback;


    private:
        VkInstance mInstance = VK_NULL_HANDLE;
        VkPhysicalDevice mGpu = VK_NULL_HANDLE;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkQueue mQueue = VK_NULL_HANDLE;
        uint32_t mQueueFamilyIndex = 0;
        VkPhysicalDeviceMemoryProperties mMemoryProperties = {};
        PFN_vkGetAndroidHardwareBufferPropertiesANDROID
                mPfnGetAndroidHardwareBufferPropertiesANDROID = nullptr;
};


#endif //VULKAN_PHOTO_BOOTH_VULKANINSTANCE_H
