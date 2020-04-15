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

#ifndef FUNHOUSE_PHOTO_BOOTH_IMAGEREADERLISTENER_H
#define FUNHOUSE_PHOTO_BOOTH_IMAGEREADERLISTENER_H


#include <media/NdkImageReader.h>
#include "vulkan-utils/VulkanImageRenderer.h"
#include "ring_buffer.h"

/**
 * Listener for each new frame received from the camera
 */
class ImageReaderListener {
public:
    // Indicate if each of the surfaces is ready to be written too
    static bool surface_ready;
    static bool surface_ready_left;
    static bool surface_ready_right;

    // State flags
    static bool native_draw_to_display; // Can Vulkan write to the screen?
    static bool vulkan_queue_empty; // Are there any frames propagating through Vulkan?
    static bool gif_being_encoded; // Is a gif currently being encoded
    static bool gif_requested; // Has a gif been requested

    // GIF generator info
    static const uint16_t NUM_GIF_FRAMES = 7;
    RingBuffer<uint32_t *> *gifRingBuffer;
    static int gif_frames_captured; // Counter for # frames captured for GIF so far

    ImageReaderListener(VulkanInstance *instance, VulkanImageRenderer *renderer, FilterParams *filterParams, ANativeWindow *outputWindow);

    static void onImageAvailableCallback(void* obj, AImageReader* reader);
    void onImageAvailable(void* obj, AImageReader* reader);
    int onImageAvailableCount();

private:
    VulkanInstance *mInstance = nullptr;
    VulkanImageRenderer *mRenderer = nullptr;
    FilterParams *mFilterParams = nullptr;
    ANativeWindow *mMainOutputWindow = nullptr; // The output surface to grab images from for gif
    int mOnImageAvailableCount = 0;

    // Frame counter
    int frame_count = 0;
    double last_time = 0;
    double current_fps = 0;
    double start_render_time = 0;
    double vulkan_render_time = 0;
};

#endif //FUNHOUSE_PHOTO_BOOTH_IMAGEREADERLISTENER_H
