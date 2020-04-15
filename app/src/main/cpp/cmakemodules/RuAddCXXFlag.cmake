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

include(CheckCXXCompilerFlag)
include(RuAddCompilerFlagsCommon)

function(ru_get_cxx_flag_name result flag)
    ru_get_compiler_flag_name(tmp "CXX" "${flag}")
    set("${result}" "${tmp}" PARENT_SCOPE)
endfunction()

function(ru_get_cxx_flag_test_name result flag)
    ru_get_compiler_flag_test_name(tmp "CXX" "${flag}")
    set("${result}" "${tmp}" PARENT_SCOPE)
endfunction()

#
# Test if the C++ compiler supports <flag>. Store the test's result in
# the internal cache variable HAVE_CXX_FLAG_${flag_name}, where
# flag_name is the result of converting "${flag}" to uppercase and
# sanitizing it into a legal CMake variable name.
#
# If the test succeeds, then set the non-cache variable
# CXX_FLAG_${flag_name} to the string <flag>.  If the test fails,
# then unset CXX_FLAG_${flag_name}.
#
# Example:
#
#   If the C++ compiler supports -Werror=format, then
#
#       ru_add_cxx_flag_checked(CMAKE_CXX_FLAGS "-Werror=format")
#
#   sets the cache variable
#
#       HAVE_CXX_FLAG_WERROR_FORMAT:INTERNAL=1
#
#   and updates the non-cache variables
#
#       set(CXX_FLAG_WERROR_FORMAT "-Werror=format")
#
# Example:
#
#   If the C++ compiler does not support -Werror=format, then
#
#       ru_add_cxx_flag_checked(CMAKE_CXX_FLAGS "-Werror=format")
#
#   sets the cache variable
#
#       HAVE_CXX_FLAG_WERROR_FORMAT:INTERNAL=0
#
#   and updates the the non-cache variable
#
#       unset(CXX_FLAG_WERROR_FORMAT)
#
function(ru_check_cxx_flag flag)
    ru_get_cxx_flag_name(flag_name "${flag}")
    ru_get_cxx_flag_test_name(test_name "${flag}")

    # Replace the messages from check_cxx_compiler_flag with custom,
    # more helpful messages. But mimic the style of the original messages.
    set(CMAKE_REQUIRED_QUIET ON)
    set(msg "Performing Test ${test_name} for C++ compiler flags \"${flag}\"")

    message(STATUS "${msg}")
    check_cxx_compiler_flag("${flag}" ${test_name})

    if(${test_name})
        message(STATUS "${msg} - Success")
        set(${flag_name} "${flag}" PARENT_SCOPE)
    else()
        message(STATUS "${msg} - Failed")
        unset(${flag_name} PARENT_SCOPE)
    endif()
endfunction()

#
# Test if the C++ compiler supports <flag> with
# ru_check_cxx_flag.  If the test succeeds, then add the compiler
# <flag> to <var>, where <var> is a string of C++ compiler flags.
#
# Example:
#
#   If the C++ compiler supports -Werror=format, then
#
#       ru_add_cxx_flag_checked(CMAKE_CXX_FLAGS "-Werror=format")
#
#   sets the cache variable
#
#       HAVE_CXX_FLAG_WERROR_FORMAT:INTERNAL=1
#
#   and updates the non-cache variables
#
#       set(CXX_FLAG_WERROR_FORMAT "-Werror=format")
#       string(APPEND CMAKE_CXX_FLAGS " -Werror=format")
#
# Example:
#
#   If the C++ compiler does not support -Werror=format, then
#
#       ru_add_cxx_flag_checked(CMAKE_CXX_FLAGS "-Werror=format")
#
#   sets the cache variable
#
#       HAVE_CXX_FLAG_WERROR_FORMAT:INTERNAL=0
#
#   and updates the the non-cache variable
#
#       unset(CXX_FLAG_WERROR_FORMAT)
#
#   and does not modify CMAKE_CXX_FLAGS.
#
function(ru_add_cxx_flag_checked var flag)
    ru_get_cxx_flag_test_name(test_name "${flag}")
    ru_check_cxx_flag("${flag}")

    if(${test_name})
        set(${var} "${${var}} ${flag}" PARENT_SCOPE)
    endif()
endfunction()
