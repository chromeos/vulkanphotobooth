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

cmake_minimum_required(VERSION 3.7)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmakemodules")

include(RuAddSpirV)
include(RuAddCFlag)
include(RuCheckCAttribute)

add_definitions("-D_POSIX_C_SOURCE=200809L")
add_definitions("-D_GNU_SOURCE")
add_definitions("-DLOG_TAG=VulkanPhoto")

string(APPEND CMAKE_C_FLAGS " -std=c11")
#string(APPEND CMAKE_C_FLAGS " -Wall")
#string(APPEND CMAKE_C_FLAGS " -Wextra -pedantic")
#string(APPEND CMAKE_CXX_FLAGS " -Wextra")
string(APPEND CMAKE_C_FLAGS " -fpermissive")
string(APPEND CMAKE_CXX_FLAGS " -fpermissive")
string(APPEND CMAKE_C_FLAGS " -fstack-protector -fno-omit-frame-pointer -O3 -ggdb3")
string(APPEND CMAKE_CXX_FLAGS " -fstack-protector -fno-omit-frame-pointer -O3 -ggdb3")

ru_add_c_flag_checked(CMAKE_C_FLAGS "-Wignored-attributes")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Wunknown-attributes")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Wno-initializer-overrides")

ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=discarded-qualifiers")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=format")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=gnu-designator")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=implicit-function-declaration")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=implicit-int")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=incompatible-pointer-types")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=int-conversion")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=missing-parameter-type")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=return-type")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=shadow")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=sentinel")
ru_add_c_flag_checked(CMAKE_C_FLAGS "-Werror=writable-strings")

ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Wno-initializer-overrides")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Wignored-attributes")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Wunknown-attributes")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=discarded-qualifiers")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=format")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=gnu-designator")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=implicit-function-declaration")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=implicit-int")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=incompatible-pointer-types")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=int-conversion")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=missing-parameter-type")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=return-type")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=shadow")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=sentinel")
ru_add_c_flag_checked(CMAKE_CXX_FLAGS "-Werror=writable-strings")

string(APPEND CMAKE_C_FLAGS_DEBUG " -g3")
string(APPEND CMAKE_C_FLAGS_DEBUG " -O0")
string(APPEND CMAKE_C_FLAGS_DEBUG " -DDEBUG")

ru_check_c_attribute(alloc_size
        CODE
        [=[
        void *f(int n) __attribute__((alloc_size(1))) \\;
        void *g(int m, int n) __attribute__((alloc_size(1, 2))) \\;
        int main(void) { return 0 \\; }
        ]=]
        )

configure_file(config.h.in config.h @ONLY)
string(APPEND CMAKE_C_FLAGS " -include \"${CMAKE_CURRENT_BINARY_DIR}/config.h\"")

get_filename_component(SHADERC_SRC
        ${ANDROID_NDK}/sources/third_party/shaderc
        ABSOLUTE)

set(GLSLANG_SPIRV_INCLUDE_DIR ${SHADERC_SRC}/third_party/glslang)
get_filename_component(PROJECT_SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/"
        ABSOLUTE)

# Import shaderc lib in NDK
add_library(shaderc_lib STATIC IMPORTED)
set_target_properties(shaderc_lib PROPERTIES IMPORTED_LOCATION
        ${SHADERC_SRC}/libs/${ANDROID_STL}/${ANDROID_ABI}/libshaderc.a)

# Build application's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fpermissive -D VK_USE_PLATFORM_ANDROID_KHR")

# Force export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED
             # Provides a relative path to your source file(s).
             native-lib.cpp
             ImageReaderListener.cpp
             ring_buffer.h
        )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log
        )


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_include_directories(native-lib PUBLIC
        ${CMAKE_SOURCE_DIR}/data
        ${GLMINC_PREFIX}
        ${SHADERC_SRC}/third_party/spirv-tools/include
        ${SHADERC_SRC}/third_party/spirv-tools/include/spirv-tools
        )

add_subdirectory(vulkan-utils)
add_subdirectory(third_party/androidndkgif)

target_link_libraries( # Specifies the target library.
                       native-lib

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
        ${UTILS_NAME}
        shaderc_lib
        androidndkgif
        vulkan-utils
        android
        mediandk
        nativewindow
        vulkan
        )