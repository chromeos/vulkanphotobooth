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

#include <android/trace.h>
#include <media/NdkImageReader.h>
#include "ImageReaderListener.h"
#include "vulkan-utils/vulkan_utils.h"
#include "native-lib.h"
#include "ring_buffer.h"

/**
 * Controlling flags
 *
 * ImageReaderListener manages the global flow of everything in native because receiving camera
 * frames is the driving function of the app.
 */
bool ImageReaderListener::surface_ready = false;
bool ImageReaderListener::surface_ready_left = false;
bool ImageReaderListener::surface_ready_right = false;
bool ImageReaderListener::native_draw_to_display = false;
bool ImageReaderListener::vulkan_queue_empty = true;
bool ImageReaderListener::gif_being_encoded = false;
bool ImageReaderListener::gif_requested = false;
int ImageReaderListener::gif_frames_captured = 0;

ImageReaderListener::ImageReaderListener(VulkanInstance *instance, VulkanImageRenderer *renderer, FilterParams *filterParams, ANativeWindow *outputWindow) {
    mInstance = instance;
    mRenderer = renderer;
    mFilterParams = filterParams;
    mMainOutputWindow = outputWindow;

    // Create ring buffer for GIF creation
    gifRingBuffer = new RingBuffer<uint32_t *>(NUM_GIF_FRAMES);
}

/**
 * Static method so onImageAvailable can be used as a callback
 */
void ImageReaderListener::onImageAvailableCallback(void* obj, AImageReader* reader) {
    auto listener = static_cast<ImageReaderListener *>( obj );
    listener->onImageAvailable(obj, reader);
}


/**
 * When a new image is available, acquire it, render it, and free it
 *
 * @param obj This listener
 * @param reader The ImageReader for the camera
 */
void ImageReaderListener::onImageAvailable(void* obj, AImageReader* reader) {

    // If the SurfaceView (Kotlin) is not yet created or has been destroyed, do not try to write to it
    if (!surface_ready) { return;}

    // If the listener is null, something is wrong
    if (obj == nullptr) { return; }

    start_render_time = now_ms();
    auto thiz = reinterpret_cast<ImageReaderListener*>(obj);
    thiz->mOnImageAvailableCount++;
    AImage* image = nullptr;

    // Get latest AImage from the reader. NOTE: this will be freed by the renderer
    int fenceFd = -1;
    media_status_t ret = AImageReader_acquireLatestImageAsync(reader, &image, &fenceFd);

    if (ret != AMEDIA_OK || image == nullptr) {
        loge("%s: acquire image from reader %p failed! ret: %d, img %p",
             __FUNCTION__, reader, ret, image);
        return;
    }

    // Get the image format
    int32_t format = -1;
    ret = AImage_getFormat(image, &format);
    if (ret != AMEDIA_OK || format == -1) {
        loge("%s: get format for image %p failed! ret: %d, format %d",
             __FUNCTION__, image, ret, format);
        AImage_delete(image);
        return;
    } else {
        // logd("%s: get format for image %p Succeeded! ret: %d, format %d",
        // __FUNCTION__, image, ret, format);
    }


    //    int32_t check_height;
    //    AImage_getHeight(image, &check_height);
    //    int32_t check_width;
    //    AImage_getWidth(image, &check_width);
    //    logd("Image in, w: %d, h: %d", check_width, check_height);

    // Get the Android Hardware Buffer from the image
    ATrace_beginSection("VULKAN_PHOTOBOOTH: Get AHB buffer.");
    AHardwareBuffer *ahb;
    ret = AImage_getHardwareBuffer(image, &ahb);
    if (ret)
        loge("AImage_getHardwareBuffer failed: error: %d", ret);
    ATrace_endSection();

    // Set up the Vulkan AHB and connect the Android AHB to it
    ATrace_beginSection("VULKAN_PHOTOBOOTH: vkAHB creation.");
    // Import the AHardwareBuffer into Vulkan. NOTE: the vkAHB will be freed by the renderer
    auto vkAHB = new VulkanAHardwareBufferImage(mInstance);
    ASSERT_FORMATTED(vkAHB->init(ahb, true ),
                     "Could not init VulkanAHardwareBufferImage.");
    ATrace_endSection();

    // The first time an image is received, create the render pipeline, afterward re-use the same pipeline
    if (!mRenderer->isPipelineInitialized) {
        ATrace_beginSection("VULKAN_PHOTOBOOTH: Create Vulkan pipeline.");
        //TODO: do something if assert fails and die.
        ASSERT_FORMATTED(mRenderer->createPipeline(vkAHB->sampler(), vkAHB->isSamplerImmutable()),
                         "creation of Render pipeline failed.");
        ATrace_endSection();
    }

    //      logd("About to render: %d", frame_count);
    // Send the image to the renderer so it will passed into the Vulkan pipeline for effects and display
    ATrace_beginSection("VULKAN_PHOTOBOOTH: renderImageAndReadback call from native-lib.");

    // Only copy out every 12th frame, and only if a gif is not currently being encoded
    // if ringbuf_data is null, no copy will be made in renderImageAndReadback.
    uint32_t *ringbuf_data = nullptr;
    if (1 == frame_count % 12
        && gif_requested
        && !gif_being_encoded) {
            ringbuf_data = new uint32_t[VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH * VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT];
        if (gifRingBuffer->isFull()) {
            delete gifRingBuffer->get();
        }
    }

    RENDERER_RETURN_CODE render_state = RENDER_STATE_NOT_SET;

    // Send the frame to the renderer
    double fence_delay = mRenderer->renderImageAndReadback(
            vkAHB, mFilterParams, image, native_draw_to_display, surface_ready_left, surface_ready_right, render_state,
            ringbuf_data);
    ATrace_endSection(); // renderImageAndReadback

    // Save to the ring buffer
    if (nullptr != ringbuf_data) {
        gifRingBuffer->put(ringbuf_data);
        gif_frames_captured++;
        updateGifProgress();

        if (gif_frames_captured >= NUM_GIF_FRAMES) {
            gif_requested = false;
            gifReadyToEncode();
        }
    }

    thiz->mOnImageAvailableCount--;

    // Signal to Kotlin that the vulkan queue has been drained
    vulkan_queue_empty = !native_draw_to_display && (render_state == RENDER_QUEUE_EMPTY);

    // Frame latency statistics
    // current_fps: should be relatively accurate as it measures the time it takes to submit
    // a frame to the pipeline. If rendering is faster than image reader is feeding, this should be
    // ~ the speed of the camera. Normally 30fps/60fps. If the pipeline has to wait for a previous
    // swapchain to complete rendering, before it can submit this will be lower.
    //
    // vulkan_render_time: is less precise. It's difficult to measure actual shader performance due
    // to the asynchronous nature of the pipeline. However, it is easy to measure how long it took
    // to submit the last frame to the pipeline. This is a "black-box" way of checking the
    // total time spent doing blit outs, prepping the pipeline, and waiting for previous frames in
    // the swapchain to be presented. Anything value less than 1 / camera_fps (ie. 33ms for 30fps)
    // is not that useful and means the pipeline is running smoothly. Anything greater than that
    // indicates that the shaders are too slow for the camera speed, the current_fps is likely less
    // less than the camera framerate, and optimizations should be made. Probably a shader is doing
    // sampling too many pixels or has sampling-dependencies (a sample depends on the result of a
    // previous sample).
    if (native_draw_to_display) {
        frame_count++;
        // Only update every 50 frames
        if (0 == frame_count % 50) {
            current_fps = 1000 / (now_ms() - last_time);
            vulkan_render_time = now_ms() - start_render_time;
            updateFramerateUI(current_fps, vulkan_render_time);
        }
    }
    // Record time the last time a frame was successfully submitted to the Vulkan pipeline
    last_time = now_ms();
}

int ImageReaderListener::onImageAvailableCount() {
    return mOnImageAvailableCount;
}