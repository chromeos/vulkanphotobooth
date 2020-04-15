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

#ifndef VULKAN_PHOTO_BOOTH_VULKAN_UTILS_H
#define VULKAN_PHOTO_BOOTH_VULKAN_UTILS_H

#include <android/log.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_android.h>
#include "VulkanInstance.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convenience method for creating VkBuffers
 *
 * @param instance
 * @param size
 * @param usage
 * @param properties
 * @param buffer
 * @param bufferMemory
 * @return
 */
bool createBuffer(VulkanInstance *instance, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory);

/**
 * Get current time in ms
 *
 * @return current time in ms
 */
double now_ms(void);

/**
 * Convenience method for setting up command buffer image synchronization
 *
 * @param commandBuffer
 * @param image
 * @param srcStageMask
 * @param dstStageMask
 * @param srcAccessMask
 * @param dstAccessMask
 * @param oldLayout
 * @param newLayout
 * @param srcQueue
 * @param dstQueue
 */
void addImageTransitionBarrier(VkCommandBuffer commandBuffer, VkImage image,
                               VkPipelineStageFlags srcStageMask,
                               VkPipelineStageFlags dstStageMask,
                               VkAccessFlags srcAccessMask,
                               VkAccessFlags dstAccessMask,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t srcQueue = VK_QUEUE_FAMILY_IGNORED,
                               uint32_t dstQueue = VK_QUEUE_FAMILY_IGNORED);



/**
 * Standard logging functions
 */
#ifdef ANDROID
#define logv(fmt, ...) __android_log_print(ANDROID_LOG_VERBOSE, "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define logd(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define logi(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define logw(fmt, ...) __android_log_print(ANDROID_LOG_WARN, "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define loge(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define logf(fmt, ...) __android_log_print(ANDROID_LOG_FATAL, "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define log_assert(cond, fmt, ...) __android_log_assert((cond), "VulkanPhoto", (fmt), ##__VA_ARGS__)
#define log_loc_assert(cond, fmt, ...) log_assert((cond), "%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define logv(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define logd(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define logi(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define logw(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define loge(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define logf(fmt, ...) printf((fmt), ##__VA_ARGS__)
#endif

#define logv_loc(fmt, ...) logv("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define logd_loc(fmt, ...) logd("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define logi_loc(fmt, ...) logi("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define logw_loc(fmt, ...) logw("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define loge_loc(fmt, ...) loge("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define logf_loc(fmt, ...) logf("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#define LOG_TAG2 "VulkanPhoto"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG2, __VA_ARGS__)

#define ASSERT(a)                                                              \
  if (!(a)) {                                                                  \
    ALOGE("Failure: " #a " at " __FILE__ ":%d", __LINE__);                     \
    return false;                                                              \
  }

#define VK_CALL(a) ASSERT_W_RETURN((a))

#define ASSERT_W_RETURN(a)                                                              \
   do { int __ret = (a); \
  if (VK_SUCCESS != __ret) {                                                                  \
    ALOGE("Failure: %d: " #a " at " __FILE__ ":%d", __ret, __LINE__);                     \
    abort(); \
    return false;                                                              \
  }} while (0);


#define ASSERT_FORMATTED(condition, format, args...)                                     \
  if (!(condition)) {                                                          \
  }

#define ASSERT_TRUE(a) ASSERT((a), "assert failed on (" #a ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_FALSE(a) ASSERT(!(a), "assert failed on (!" #a ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_EQ(a, b) \
        ASSERT((a) == (b), "assert failed on (" #a " == " #b ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_NE(a, b) \
        ASSERT((a) != (b), "assert failed on (" #a " != " #b ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_GT(a, b) \
        ASSERT((a) > (b), "assert failed on (" #a " > " #b ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_GE(a, b) \
        ASSERT((a) >= (b), "assert failed on (" #a " >= " #b ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_LT(a, b) \
        ASSERT((a) < (b), "assert failed on (" #a " < " #b ") at " __FILE__ ":%d", __LINE__)
#define ASSERT_LE(a, b) \
        ASSERT((a) <= (b), "assert failed on (" #a " <= " #b ") at " __FILE__ ":%d", __LINE__)

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG2, __VA_ARGS__)

#define ALIGN(x, mask) ( ((x) + (mask) - 1) & ~((mask) - 1) )

#endif //VULKAN_PHOTO_BOOTH_VULKAN_UTILS_H