/*
 * Copyright 2021 Google LLC
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

#include "VulkanAHBManager.h"
#include "vulkan_utils.h"

VulkanAHBManager::~VulkanAHBManager() {
    // Call destructor for all the created VulkanAHardwareBufferImages
    for (auto item : ahb_map) {
        item.second->~VulkanAHardwareBufferImage();
    }
}

VulkanAHardwareBufferImage *VulkanAHBManager::getVulkanAHB(AHardwareBuffer *ahb) {
    // Not available until SDK 31. Once available we should use the AHB's id, not its memory address
    // uint64_t ahb_id;
    // AHardwareBuffer_getId(ahb, &ahb_id);

    // Fetch existing vahb from map or get a nullptr if not in the map
    VulkanAHardwareBufferImage *vahb = ahb_map[ahb];

    // If this is the first time seeing this ahb, set it up and add to map
    if (nullptr == vahb) {
        vahb = new VulkanAHardwareBufferImage(instance);
        ASSERT_FORMATTED(vahb->init(ahb, true),
                         "Could not init VulkanAHardwareBufferImage.");
        ahb_map[ahb] = vahb;
    }
    return vahb;
}

void VulkanAHBManager::setVulkanInstance(VulkanInstance *pInstance) {
    this->instance = pInstance;
}