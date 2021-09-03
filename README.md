# Vulkan Photo Booth

## This is not an officially supported Google product
This is currently in alpha status. This code should not be used in production.

## About
A Vulkan-powered Android app targeting Chrome OS devices that showcases their potential to run
high-peformance graphics applications. Should also run on Android phones.

Best experienced with a MIDI controller for a tactile experience, can also be enjoyed using a
touchscreen/keyboard and mouse. 

Explore combinations of glsl filters interacting with the camera
preview stream and then capture a shareable animation sequence.

## Requirements

* Device with Android 8.1 or higher
* Vulkan 1.1 or higher
* Device with a camera or USB webcam

Optional:
* MIDI controller with 6 sliders/knobs and one button

## Building
Currently this uses a staticly built and linked version of shaderc and
a fixed NDK version. There are better ways of doing this and I hope to
update the project to include one soon. In the meantime, to "just get it
to build" please follow the following steps:
* Import the project into Android Studio (2020.3.1 Arctic Fox or later)
  * Update and gradle or library dependencies (automatic links should work)
* Install NDK 22.1.7171670:
  * Go to Tools->SDK manager->SDK Tools tab
  * Click checkbox "show package details" to list all versions
  * Expand "NDK Side-by-Side"
  * Check version 22.1.7171670
  * Click apply to download install
* Update local.properties to have: ndk.dir=/home/YOURHOMEDIR/Android/Sdk/ndk/22.1.7171670
* Build shaderc (replace SDK/NDK directories as needed):
  * Go to the command line
  * cd ~/Android/Sdk/ndk/22.1.7171670/sources/third_party/shaderc
  * ../../../ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk APP_STL:=c++_static APP_ABI=all libshaderc_combined
* Note: In build.gradle, `abiFilters` is set to build ARM32, ARM65, x86_32, and x86_64 ABIs to maximize device support. You may adjust this if required.
* Build project

## LICENSE

***

Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.