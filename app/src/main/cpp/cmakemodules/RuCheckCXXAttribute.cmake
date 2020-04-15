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

include(CheckCXXSourceCompiles)
include(RuAddCXXFlag)

function(ru_get_cxx_attribute_test_name result attrib)
    string(TOUPPER "HAVE_CXX_ATTRIBUTE_${attrib}" name)
    set("${result}" "${name}" PARENT_SCOPE)
endfunction()

# Check if the C++ compiler supports a given attribute.
#
#   ru_check_cxx_attribute(<attrib> CODE <code>)
#
# Test if the C++ compiler supports <attrib>, and store
# the test result in internal cache variable HAVE_CXX_ATTRIBUTE_${attrib_name},
# where attrib_name is the result of converting "${attrib}"
# to uppercase and sanitizing it into a legal CMake variable name.
#
# The test respects the same global variables as check_cxx_source_compiles.
#
# Before checking if the compiler supports the attribute, the function
# first checks if the compiler supports the flags
# "-Werror=ignored-attribute" and "-Werror=unknown-attributes" with
# ru_check_cxx_flag, and adds each supported flag to
# CMAKE_REQUIRED_FLAGS at function-scope.
#
# Example:
#
#     If the compiler supports the "alloc_size" attribute, then
#
#         ru_check_cxx_attribute(alloc_size
#             CODE
#                 [=[
#                 void *f(int n) __attribute__((alloc_size(1)));
#                 void *g(int m, int n) __attribute__((alloc_size(1, 2)));
#                 int main(void) { return 0; }
#                 ]=]
#         )
#
#     will set the cache variable
#
#         HAVE_CXX_ATTRIBUTE_ALLOC_SIZE:INTERNAL=1
#
#     Otherwise, it will set
#
#         HAVE_CXX_ATTRIBUTE_ALLOC_SIZE:INTERNAL=
#
function(ru_check_cxx_attribute attrib)
    cmake_parse_arguments(
        PARSE_ARGV 1
        "ARG" # prefix
        "" # options
        "CODE" # one-value-keywords
        "" # multi-value-keywords
    )

    if(NOT ARG_CODE)
        message(FATAL_ERROR "in ru_check_cxx_attribute: CODE keyword argument is missing")
    endif()

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "in ru_check_cxx_attribute: found unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    ru_check_cxx_flag("-Werror=ignored-attributes")
    if(HAVE_CXX_FLAG_WERROR_IGNORED_ATTRIBUTES)
        string(APPEND CMAKE_REQUIRED_FLAGS " -Werror=ignored-attributes")
    endif()

    ru_check_cxx_flag("-Werror=unknown-attributes")
    if(HAVE_CXX_FLAG_WERROR_UNKNOWN_ATTRIBUTES)
        string(APPEND CMAKE_REQUIRED_FLAGS " -Werror=unknown-attributes")
    endif()

    ru_get_cxx_attribute_test_name(test_name "${attrib}")

    # Replace the messages from check_cxx_source_compiles with custom,
    # more helpful messages. But mimic the style of the original messages.
    set(CMAKE_REQUIRED_QUIET ON)
    set(msg "Performing Test ${test_name} for C++ attribute \"${attrib}\"")

    message(STATUS "${msg}")
    check_cxx_source_compiles("${ARG_CODE}" ${test_name})

    if(${test_name})
        message(STATUS "${msg} - Success")
    else()
        message(STATUS "${msg} - Failed")
    endif()
endfunction()
