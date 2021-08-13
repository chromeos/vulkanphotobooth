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

#ifndef VULKAN_PHOTO_BOOTH_VULKANAHBMANAGER_H
#define VULKAN_PHOTO_BOOTH_VULKANAHBMANAGER_H

#include <unordered_map>
#include <media/NdkImageReader.h>
#include "VulkanAHardwareBufferImage.h"

/**
 * Manager class for AHardwareBuffers (ahbs) / VulkanAHardwareBufferImages (vhbs)
 *
 * The ImageReader will only use a fixed number of ahbs. As these are seen, create a corresponding
 * vahb and store it in a hashmap. This avoids needing to recreate and reinitalize vahbs every
 * frame.
 */
class VulkanAHBManager {
    public:
        ~VulkanAHBManager();
        /**
         * If the AHardwareBuffer has already been added to the map, return the
         * VulkanAHardwareBufferImage associated with it. Otherwise initialize a new
         * VulkanAHardwareBufferImage, add it to the map, and return it.
         *
         * @return the VulkanAHardwareBufferImage associated with the given AHardwareBuffer
         */
        VulkanAHardwareBufferImage *getVulkanAHB(AHardwareBuffer *);
        void setVulkanInstance(VulkanInstance *pInstance);

    private:
        // Hashmap indexed by pointers to AHBs
        std::unordered_map<AHardwareBuffer *, VulkanAHardwareBufferImage *> ahb_map;
        VulkanInstance *instance;
};


#endif //VULKAN_PHOTO_BOOTH_VULKANAHBMANAGER_H
