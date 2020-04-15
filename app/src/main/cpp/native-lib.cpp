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

#include <jni.h>
#include <string>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vulkan/vulkan_core.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <media/NdkImageReader.h>
#include <android/hardware_buffer_jni.h>
#include <android/hardware_buffer.h>
#include <unistd.h>
#include <android/log.h>
#include <android/trace.h>
#include <dlfcn.h>

#include "vulkan-utils/FilterParams.h"
#include "vulkan-utils/vulkan_utils.h"
#include "ImageReaderListener.h"

#include "native-lib.h"
#include "third_party/androidndkgif/GCTGifEncoder.h"
#include "ring_buffer.h"
#include "third_party/androidndkgif/FastGifEncoder.h"

#define LOG_TAG2 "VulkanPhoto"

// JNI variables
JavaVM *jvm = nullptr;
static jobject class_loader;
static jmethodID findClassMethod;

// ImageReader
AImageReader *preview_reader = nullptr;
ImageReaderListener *listener = nullptr;

// Displays
ANativeWindow *output_window = nullptr;
ANativeWindow *output_window_left = nullptr;
ANativeWindow *output_window_right = nullptr;
uint32_t NUM_DISPLAYS = 0;


// Vulkan globals
VulkanInstance *vulkan_instance = nullptr;
VulkanImageRenderer *renderer = nullptr;

// Current set of filter paramters
FilterParams *filter_params = nullptr;

GCTGifEncoder* gifEncoder = nullptr;
//FastGifEncoder* gifEncoder = nullptr;

// Default GIF width/height
uint32_t rendererCopyWidth = 500;
uint32_t rendererCopyHeight = 500;

JNIEnv* getEnv() {
    JNIEnv *env;
    int status = jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if(status < 0) {
        status = jvm->AttachCurrentThread(&env, nullptr);
        if(status < 0) {
            return nullptr;
        }
    }
    return env;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *onload_jvm, void *reserved) {
    jvm = onload_jvm;  // cache the JavaVM pointer
    auto env = getEnv();

    auto myClass = env->FindClass("dev/hadrosaur/vulkanphotobooth/MainActivity");
    jclass classClass = env->GetObjectClass(myClass);
    auto classLoaderClass = env->FindClass("java/lang/ClassLoader");
    auto getClassLoaderMethod = env->GetMethodID(classClass, "getClassLoader",
                                                 "()Ljava/lang/ClassLoader;");
    // Free'd in cleanup()
    class_loader = env->NewGlobalRef(env->CallObjectMethod(myClass, getClassLoaderMethod));
    findClassMethod = env->GetMethodID(classLoaderClass, "findClass",
                                       "(Ljava/lang/String;)Ljava/lang/Class;");

    return JNI_VERSION_1_6;
}

jclass findClass(const char* name) {
    // Caller is responsible for free'ing returned jstring
    return static_cast<jclass>(getEnv()->CallObjectMethod(class_loader, findClassMethod, getEnv()->NewStringUTF(name)));
}

bool InitializeVulkan() {
    // Set up Vulkan.
    // Instance free'd in cleanup()
    vulkan_instance = new VulkanInstance();
    if (!vulkan_instance->init()) {
        // Could not initialize Vulkan due to lack of device support
        return false;
    }

    ImageReaderListener::native_draw_to_display = true;
    return true;
}

// Check if all 3 surfaces are ready, if so, init the renderer and start the imagereader callback
// TODO: This should probably be a proper bool function
void areSurfacesReady() {
    // logd("In areSurfacesReady, ow: %d, owl %d, owr %d.", output_window == nullptr, output_window_left == nullptr, output_window_right == nullptr);

    // No displays
    if (NUM_DISPLAYS < 1)
        return;

    // Check 1st display if needed
    if (NUM_DISPLAYS >= 1 && output_window == nullptr)
        return;

    // Check 2nd display if needed
    if (NUM_DISPLAYS >= 2 && output_window_left == nullptr)
        return;

    // Check 3rd display if needed
    if (NUM_DISPLAYS >= 3 && output_window_right == nullptr)
        return;

    // All required surfaces are now ready, initialize renderer
    logd("Calling init on render output surfaces");
    ASSERT_FORMATTED(renderer->init(output_window, output_window_left, output_window_right, rendererCopyWidth, rendererCopyHeight), "Could not init VulkanImageRenderer.");
//    ASSERT_FORMATTED(renderer->init(output_window, output_window, output_window), "Could not init VulkanImageRenderer.");

    // Set up ImageReader
    listener = new ImageReaderListener(vulkan_instance, renderer, filter_params, output_window);
    AImageReader_ImageListener preview_image_listener { listener, ImageReaderListener::onImageAvailableCallback };
    AImageReader_setImageListener(preview_reader, &preview_image_listener);
}


void cleanup() {
    logd("Cleaning up Vulkan memory.");
//    getEnv()->DeleteGlobalRef(class_loader);
//    getEnv()->DeleteGlobalRef(mainActivity);
    if (nullptr != renderer)
        delete(renderer);

    delete(vulkan_instance);
    delete(filter_params);
    delete(listener);
}

void updateFramerateUI(double current_fps, double vulkan_render_time) {
    JNIEnv *jni_env = getEnv();


    jclass clazz = findClass("dev/hadrosaur/vulkanphotobooth/MainActivity");
    jmethodID updateFramerateText = jni_env->GetStaticMethodID(clazz, "updateFramerateText", "(II)V");
    jni_env->CallStaticVoidMethod(clazz, updateFramerateText, int(current_fps * 10), int(vulkan_render_time));
    jni_env->DeleteLocalRef(clazz);

}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_initializeNative(
        JNIEnv* env,
        jobject jMainActivity,
        jint jNumDisplays) {

    filter_params = new FilterParams();

    ImageReaderListener::surface_ready = false;
    ImageReaderListener::surface_ready_left = false;
    ImageReaderListener::surface_ready_right = false;
    ImageReaderListener::native_draw_to_display = false;
    ImageReaderListener::vulkan_queue_empty = true;

    ATrace_beginSection("VULKAN_PHOTOBOOTH: INITIALIZING VULKAN_PHOTOBOOTH TEST");
    NUM_DISPLAYS = (uint32_t ) jNumDisplays;

    if (!InitializeVulkan()) {
        loge("Failied initializing Vulkan!");
        return false;
    }

    ATrace_endSection();

    logd("SUCCEEDED Initializing Vulkan!");
    return true;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_cleanupNative(
        JNIEnv* env,
        jobject) {
    cleanup();
}



extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_CameraUtilsKt_createPreviewImageReader(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jint format,
        jint max_images) {
    // logd("Create Preview Image Reader. Width: %d Height: %d", width, height);

    //Free'd in cleanup()
//    AImageReader_new(width, height, format, max_images, &preview_reader);
//    AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_YUV_420_888,
    AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_PRIVATE,
//    AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_JPEG,
//                              AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
//                              AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER |
//                              AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
//                              AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                              AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
            max_images, &preview_reader);

    // Create a VulkanImageRenderer with the actual width/height of the AHardwareBuffer.
    // Free'd in cleanup()
    renderer = new VulkanImageRenderer(vulkan_instance, NUM_DISPLAYS, width, height,
                                   VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_LeftPanelActivity_setupVulkanSurfaceLeft(
        JNIEnv* env,
        jobject,
        jobject outputSurface) {

    if (nullptr != output_window_left) {
        ANativeWindow_release(output_window_left);
    }

    logd("In setupVulkanSurfaceLeft");
    output_window_left = ANativeWindow_fromSurface(env, outputSurface);
    areSurfacesReady();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_RightPanelActivity_setupVulkanSurfaceRight(
        JNIEnv* env,
        jobject,
        jobject outputSurface) {

    if (nullptr != output_window_right) {
        ANativeWindow_release(output_window_right);
    }

    logd("In setupVulkanSurfaceRight");
    output_window_right = ANativeWindow_fromSurface(env, outputSurface);
    areSurfacesReady();
}

extern "C" JNIEXPORT jobject JNICALL
        Java_dev_hadrosaur_vulkanphotobooth_cameracontroller_Camera2ControllerKt_getImagePreviewSurface(
                JNIEnv* env,
                jclass) {

    ANativeWindow* preview_window = nullptr;

    if (nullptr != preview_reader) {
        // AImageReader will delete this window automatically. The native window will be free'd by java automatically.
        if (AMEDIA_OK == AImageReader_getWindow(preview_reader, &preview_window)) {
            return ANativeWindow_toSurface(env, preview_window);
        }
    }
    return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_cameracontroller_Camera2ControllerKt_connectPreviewToOutput(
        JNIEnv* env, jobject,
        jobject outputSurface,
        jint rotation,
        jint jRendererCopyWidth,
        jint jRendererCopyHeight) {

    logd("Setting output to outputSurface");
    filter_params->rotation = rotation;

    if (nullptr != output_window) {
        ANativeWindow_release(output_window);
    }

    output_window = ANativeWindow_fromSurface(env, outputSurface);
    rendererCopyWidth = jRendererCopyWidth;
    rendererCopyHeight = jRendererCopyHeight;
    areSurfacesReady();

    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_cameracontroller_Camera2ControllerKt_disconnectPreviewFromOutput(JNIEnv* env, jclass) {

    //Remove image listenr
    AImageReader_setImageListener(preview_reader, nullptr);

    // Drain pipe
    AImage* image = nullptr;
    media_status_t ret = AImageReader_acquireLatestImage(preview_reader, &image);
    while(AMEDIA_OK == ret) {
        AImage_delete(image);
        ret = AImageReader_acquireLatestImage(preview_reader, &image);
    }

    if (nullptr != preview_reader) {
        AImageReader_delete(preview_reader);
        preview_reader = nullptr;
    }


    if (nullptr != output_window) {
        ANativeWindow_release(output_window);
        output_window = nullptr;
    }
    if (nullptr != output_window_left) {
        ANativeWindow_release(output_window_left);
        output_window_left = nullptr;
    }
    if (nullptr != output_window_right) {
        ANativeWindow_release(output_window_right);
        output_window_right = nullptr;
    }

    return true;
}


extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_surfaceReady(
        JNIEnv* env,
        jobject, /* this */
        jboolean is_ready) {
    ImageReaderListener::surface_ready = is_ready;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_LeftPanelActivity_leftSurfaceReady(
        JNIEnv* env,
        jobject, /* this */
        jboolean is_ready) {
    ImageReaderListener::surface_ready_left = is_ready;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_RightPanelActivity_rightSurfaceReady(
        JNIEnv* env,
        jobject, /* this */
        jboolean is_ready) {
    ImageReaderListener::surface_ready_right = is_ready;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_isVulkanQueueEmpty(
        JNIEnv* env,
        jobject) {
    return ImageReaderListener::vulkan_queue_empty;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_setNativeDrawToDisplay(
        JNIEnv* env,
        jobject,
        jboolean jDrawToDisplay) {
    ImageReaderListener::native_draw_to_display = jDrawToDisplay;

    // Kotlin has requested a callback
    if (!ImageReaderListener::native_draw_to_display) {
        ImageReaderListener::vulkan_queue_empty = false;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_updateFilterParams(
        JNIEnv* env,
        jobject,
        jint jrotation,
        jintArray jseek_values,
        jbooleanArray juse_filter) {

    filter_params->rotation = jrotation;

    // Copy seek values from Kotlin
    jint *jseek_values_elements = env->GetIntArrayElements(jseek_values, nullptr);
    for (int i = 0; i < env->GetArrayLength(jseek_values); i++) {
        filter_params->seek_values[i] = jseek_values_elements[i];
    }

    // Copy filter toggles from Kotlin
    jboolean *juse_filter_elements = env->GetBooleanArrayElements(juse_filter, nullptr);
    for (int i = 0; i < env->GetArrayLength(juse_filter); i++) {
        filter_params->use_filter[i] = juse_filter_elements[i];
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_createGif(
        JNIEnv* env, jobject, jstring filepath) {

    // If a gif is already in flight, just return
    if (ImageReaderListener::gif_requested || ImageReaderListener::gif_being_encoded) {
        return false;
    }

    int width = VulkanSwapchain::RENDERER_COPY_IMAGE_WIDTH;
    int height = VulkanSwapchain::RENDERER_COPY_IMAGE_HEIGHT;

    gifEncoder = new GCTGifEncoder();
    // gifEncoder = new FastGifEncoder();
    gifEncoder->setThreadCount(8); // This does nothing for GCT but will speed up fast encoder

    const char* pathChars = env->GetStringUTFChars(filepath, 0);
    bool result = gifEncoder->init((uint16_t) width,(uint16_t) height, pathChars);
    env->ReleaseStringUTFChars(filepath, pathChars);

    if (result) {
        ImageReaderListener::gif_frames_captured = 0;
        ImageReaderListener::gif_requested = true;
        return true;
    } else {
        logd("Error initializing GIF encoder.");
    }

    return false;
}

void gifReadyToEncode() {
    // GIF ready to encode, notify kotlin
    JNIEnv *jni_env = getEnv();
    jclass clazz = findClass("dev/hadrosaur/vulkanphotobooth/MainActivity");
    jmethodID gifReadyToEncode = jni_env->GetStaticMethodID(clazz, "gifReadyToEncode", "()V");
    jni_env->CallStaticVoidMethod(clazz, gifReadyToEncode);
    jni_env->DeleteLocalRef(clazz);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_hadrosaur_vulkanphotobooth_MainActivity_encodeAndSaveGif(
        JNIEnv* env, jobject) {
    ImageReaderListener::gif_being_encoded = true;

    int num_frames = listener->gifRingBuffer->numItems();
    uint32_t **frames = new uint32_t *[num_frames];

    // Going forward
    for (int n = 0; n < num_frames; n++) {
        uint32_t *temp_frame = listener->gifRingBuffer->get();
        frames[n] = temp_frame;
        //            logd("Encoding gif frame: %d", n);
        gifEncoder->encodeFrame(temp_frame, 250); // 4fps
        //            logd("Done encoding gif frame: %d", n);
        //           delete(temp_frame);
    }

    // Going backward for boomerang effect
    for (int n = num_frames -2; n >= 1; n--) {
        gifEncoder->encodeFrame(frames[n], 250); // 4fps
    }

    logd("About to release gif encoder.");
    gifEncoder->release();
    logd("Gif encoded correctly.");

    ImageReaderListener::gif_being_encoded = false;
}

void updateGifProgress() {
    float percentage = float(ImageReaderListener::gif_frames_captured) / float(ImageReaderListener::NUM_GIF_FRAMES);

    // If no more frames to capture, tell spinner to go indefinite
    if (ImageReaderListener::gif_frames_captured >= ImageReaderListener::NUM_GIF_FRAMES) {
        percentage = -1.0f;
    }

    JNIEnv *jni_env = getEnv();
    jclass clazz = findClass("dev/hadrosaur/vulkanphotobooth/MainActivity");
    jmethodID updateGifProgressSpinner = jni_env->GetStaticMethodID(clazz, "updateGifProgressSpinner", "(F)V");
    jni_env->CallStaticVoidMethod(clazz, updateGifProgressSpinner, percentage);
    jni_env->DeleteLocalRef(clazz);
}