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

#ifndef VULKAN_PHOTO_BOOTH_NATIVE_LIB_H
#define VULKAN_PHOTO_BOOTH_NATIVE_LIB_H

#include <jni.h>
#include "third_party/androidndkgif/GCTGifEncoder.h"

JNIEnv* getEnv();
jclass findClass(const char* name);

void updateFramerateUI(double current_fps, double vulkan_render_time);
bool InitializeVulkan();
void cleanup();
void updateGifProgress();
void gifReadyToEncode();

/**
 * Initialize the native/vulkan setup
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_initializeNative(
        JNIEnv* env, jobject jMainActivity, jint jNumDisplays);

/**
 * Teardown vulkan and free native memory allocations
 */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_cleanupNative(
        JNIEnv* env, jobject);

/**
 * Setup the ImageReader for the camera device
 */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_CameraUtilsKt_createPreviewImageReader(
        JNIEnv* env, jclass, jint width, jint height, jint format, jint max_images);

/**
 * Get the preview Surface from the ImageReader
 */
extern "C" JNIEXPORT jobject JNICALL
Java_dev_hadrosaur_vulkanphotobooth_cameracontroller_Camera2ControllerKt_getImagePreviewSurface(
        JNIEnv* env, jclass);

/**
 * Set up outputSurface to be the center display Vulkan output surface
 */
extern "C" JNIEXPORT jboolean JNICALL
        Java_dev_hadrosaur_vulkanphotobooth_cameracontroller_Camera2ControllerKt_connectPreviewToOutput(
        JNIEnv* env, jobject,
        jobject outputSurface, jint rotation, jint jRendererCopyWidth, jint jRendererCopyHeight);

/**
 * Set up outputSurface to be the left-hand display Vulkan output surface
 */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_LeftPanelActivity_setupVulkanSurfaceLeft(
        JNIEnv* env, jobject, jobject outputSurface);

/**
 * Set up outputSurface to be the right-hand display Vulkan output surface
 */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_RightPanelActivity_setupVulkanSurfaceRight(
        JNIEnv* env, jobject, jobject outputSurface);

/**
 * Removes ImageReader listener and frees ANativeWindows associated with the displays
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_cameracontroller_Camera2ControllerKt_disconnectPreviewFromOutput(
        JNIEnv* env, jclass);

/** Set flag indicating if vulkan can draw to the screen */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_setNativeDrawToDisplay(
        JNIEnv* env, jobject, jboolean jDrawToDisplay);

/** Set flag indicating that centre display surface is ready */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_surfaceReady(
        JNIEnv* env, jobject, jboolean is_ready);

/** Set flag indicating that left display surface is ready */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_LeftPanelActivity_leftSurfaceReady(
        JNIEnv* env, jobject, jboolean is_ready);

/** Set flag indicating that right display surface is ready */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_RightPanelActivity_rightSurfaceReady(
        JNIEnv* env, jobject, jboolean is_ready);

/** Retrieve flag indiciating if Vulkan is finished rendering to screen */
extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_isVulkanQueueEmpty(
        JNIEnv* env, jobject);

/** Update filter parameters from Kotlin */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_updateFilterParams(
        JNIEnv* env, jobject, jint jrotation,
        jintArray jseek_values,
        jbooleanArray juse_filter);

/** Tell the rendered to begin copying out frames for GIF generation */
extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_createGif(
        JNIEnv* env, jobject, jstring filepath);

/** Frames have been copied out, begin encoding and saving GIF file */
extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_encodeAndSaveGif(JNIEnv* env, jobject);

#endif //VULKAN_PHOTO_BOOTH_NATIVE_LIB_H
