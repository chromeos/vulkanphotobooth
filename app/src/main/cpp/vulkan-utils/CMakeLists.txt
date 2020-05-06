# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include_directories(
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        ${GLMINC_PREFIX}
        ${SHADERC_SRC}/third_party/spirv-tools/include
        ${SHADERC_SRC}/third_party/spirv-tools/include/spirv-tools
        "${CMAKE_CURRENT_BINARY_DIR}" # for generated spvnum files
)

ru_add_spvnum(quad.vert.spvnum ../shaders/quad.vert.glsl)
ru_add_spvnum(quad.frag.spvnum ../shaders/quad.frag.glsl)

add_library(vulkan-utils SHARED
        ../third_party/vulkan_debug/vulkan_debug.cpp
        vulkan_utils.h
        vulkan_utils.cpp
        VulkanInstance.cpp
        VulkanAHardwareBufferImage.cpp
        VulkanImageRenderer.cpp
        VulkanSwapchain.cpp
        VulkanSurface.cpp
        quad.vert.spvnum
        quad.frag.spvnum
        )

target_link_libraries(vulkan-utils
        android
        mediandk
        log
        shaderc_lib
        vulkan
        )