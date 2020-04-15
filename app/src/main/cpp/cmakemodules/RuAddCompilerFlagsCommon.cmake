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

if(__ADD_COMPILER_FLAGS_COMMON)
    return()
endif()
set(__ADD_COMPILER_FLAGS_COMMON 1)

function(ru_get_compiler_flag_name result lang flag)
    set(flag_name "${flag}")
    string(REGEX REPLACE "^-" "" flag_name "${flag_name}")
    set(flag_name "${lang}_FLAG_${flag_name}")
    string(TOUPPER "${flag_name}" flag_name)
    string(REGEX REPLACE "[^0-9A-Z]" "_" flag_name "${flag_name}")
    set(${result} "${flag_name}" PARENT_SCOPE)
endfunction()

function(ru_get_compiler_flag_test_name result lang flag)
    ru_get_compiler_flag_name(flag_name "${lang}" "${flag}")
    set(${result} "HAVE_${flag_name}" PARENT_SCOPE)
endfunction()
