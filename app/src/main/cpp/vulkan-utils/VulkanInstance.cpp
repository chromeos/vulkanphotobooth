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

#include "VulkanInstance.h"
#include "vulkan_utils.h"
#include "../third_party/vulkan_debug/vulkan_debug.h"

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        //logd("Layer name: %s", layerName);
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

bool VulkanInstance::init() {
    // Validation layers
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .apiVersion = VK_MAKE_VERSION(1, 1, 0),
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .pApplicationName = "VulkanPhoto",
            .pEngineName = "VulkanPhotoEngine",
    };

    std::vector<const char *> instanceExt, deviceExt;
    instanceExt.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    instanceExt.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    instanceExt.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
    instanceExt.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    instanceExt.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    instanceExt.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
    deviceExt.push_back(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
    deviceExt.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
//    deviceExt.push_back(VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME); // Doesn't exist yet

    VkInstanceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = static_cast<uint32_t>(instanceExt.size()),
            .ppEnabledExtensionNames = instanceExt.data(),
    };

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }
    VK_CALL(vkCreateInstance(&createInfo, nullptr, &mInstance));

    vks::debug::setupDebugging(mInstance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, VK_NULL_HANDLE);
//    vks::debug::setupDebugging(mInstance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, VK_NULL_HANDLE);
//    vks::debug::setupDebugging(mInstance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, VK_NULL_HANDLE);
//    vks::debug::setupDebugging(mInstance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, VK_NULL_HANDLE);
//    vks::debug::setupDebugging(mInstance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, VK_NULL_HANDLE);
//    vks::debug::setupDebugging(mInstance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, VK_NULL_HANDLE);

    // Find a GPU to use.
    uint32_t gpuCount = 0;
    int status = vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr);
    if (0 < gpuCount)
        status = vkEnumeratePhysicalDevices(mInstance, &gpuCount, &mGpu);

    ASSERT(status == VK_SUCCESS || status == VK_INCOMPLETE);
    ASSERT(gpuCount > 0);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mGpu, &queueFamilyCount, nullptr);
    ASSERT(queueFamilyCount != 0);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mGpu, &queueFamilyCount,
                                             queueFamilyProperties.data());

    uint32_t queueFamilyIndex;
    for (queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount;
         ++queueFamilyIndex) {
        if (queueFamilyProperties[queueFamilyIndex].queueFlags &
            VK_QUEUE_GRAPHICS_BIT)
            break;
    }
    ASSERT(queueFamilyIndex < queueFamilyCount);
    mQueueFamilyIndex = queueFamilyIndex;

    // Enable YUV sampler
    VkPhysicalDeviceSamplerYcbcrConversionFeaturesKHR ycbcrFeatures{
            .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES_KHR,
            .pNext = nullptr,
    };
    VkPhysicalDeviceFeatures2KHR physicalDeviceFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
            .pNext = &ycbcrFeatures,
    };
    PFN_vkGetPhysicalDeviceFeatures2KHR getFeatures =
            (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(
                    mInstance, "vkGetPhysicalDeviceFeatures2KHR");
    ASSERT(getFeatures);
    getFeatures(mGpu, &physicalDeviceFeatures);
    ASSERT(ycbcrFeatures.samplerYcbcrConversion == VK_TRUE);

    float priorities[] = {1.0f};
    VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCount = 1,
            .queueFamilyIndex = queueFamilyIndex,
            .pQueuePriorities = priorities,
    };

    VkDeviceCreateInfo deviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &ycbcrFeatures,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExt.size()),
            .ppEnabledExtensionNames = deviceExt.data(),
            .pEnabledFeatures = nullptr,
    };

    VK_CALL(vkCreateDevice(mGpu, &deviceCreateInfo, nullptr, &mDevice));

    mPfnGetAndroidHardwareBufferPropertiesANDROID =
            (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)vkGetDeviceProcAddr(
                    mDevice, "vkGetAndroidHardwareBufferPropertiesANDROID");
    ASSERT(mPfnGetAndroidHardwareBufferPropertiesANDROID);

    logd("VulkanInstance Init, all good with AHB");

    vkGetDeviceQueue(mDevice, 0, 0, &mQueue);
    vkGetPhysicalDeviceMemoryProperties(mGpu, &mMemoryProperties);


    return true;
}

VulkanInstance::~VulkanInstance() {
    loge("Danger VulkanInstance is being destroyed!!!");

    if (mQueue != VK_NULL_HANDLE) {
        // Queues are implicitly destroyed with the device.
        mQueue = VK_NULL_HANDLE;
    }

    vks::debug::freeDebugCallback(mInstance);

    if (mDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }
    if (mInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }
}

uint32_t VulkanInstance::findMemoryType(uint32_t memoryTypeBitsRequirement,
                                        VkFlags requirementsMask) {
    for (uint32_t memoryIndex = 0; memoryIndex < VK_MAX_MEMORY_TYPES;
         ++memoryIndex) {
        const uint32_t memoryTypeBits = (1 << memoryIndex);
        const bool isRequiredMemoryType =
                memoryTypeBitsRequirement & memoryTypeBits;
        const bool satisfiesFlags =
                (mMemoryProperties.memoryTypes[memoryIndex].propertyFlags &
                 requirementsMask) == requirementsMask;
        if (isRequiredMemoryType && satisfiesFlags)
            return memoryIndex;
    }

    // failed to find memory type.
    ALOGE("Couldn't find required memory type.");
    return 0;
}
