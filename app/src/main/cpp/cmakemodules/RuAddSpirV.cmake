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

if(CMAKE_VERSION VERSION_LESS "3.7")
    message(WARNING
        [=[
        In CMake versions < 3.7, command `add_custom_command` does not support the DEPFILE argument.
        Therefore command `ru_add_spvnum` cannot track dependencies for GLSL `#include` directives.
        ]=]
    )
endif()

function(__ru_add_spirv_join_paths result a b)
    string(REGEX MATCH "^/" has_root "${b}")

    if(has_root)
        set(${result} "${b}" PARENT_SCOPE)
    else()
        set(${result} "${a}/${b}" PARENT_SCOPE)
    endif()
endfunction()

function(ru_add_spvnum out src)
    __ru_add_spirv_join_paths(abs_src "${CMAKE_CURRENT_SOURCE_DIR}" "${src}")
    file(RELATIVE_PATH rel_src "${CMAKE_BINARY_DIR}" "${abs_src}")

    __ru_add_spirv_join_paths(abs_out "${CMAKE_CURRENT_BINARY_DIR}" "${out}")
    file(RELATIVE_PATH rel_out "${CMAKE_BINARY_DIR}" "${abs_out}")

    if(CMAKE_VERSION VERSION_LESS "3.7")
        add_custom_command(
            DEPENDS ${src}
            OUTPUT ${out}
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMAND ${GLSLC} -MD -o "${rel_out}" -c -mfmt=num "${rel_src}"
        )
    else()
        add_custom_command(
            DEPENDS ${src}
            OUTPUT ${out}
            DEPFILE ${rel_out}.d
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMAND ${GLSLC} -MD -o "${rel_out}" -c -mfmt=num "${rel_src}"
        )
    endif()
endfunction()
