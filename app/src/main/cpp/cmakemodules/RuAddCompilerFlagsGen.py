#!/usr/bin/env python3

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

import argparse
import sys
from textwrap import dedent

template = dedent("""\
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

    include(Check{lang:u}CompilerFlag)
    include(RuAddCompilerFlagsCommon)

    function(ru_get_{lang:l}_flag_name result flag)
        ru_get_compiler_flag_name(tmp "{lang:u}" "${{flag}}")
        set("${{result}}" "${{tmp}}" PARENT_SCOPE)
    endfunction()

    function(ru_get_{lang:l}_flag_test_name result flag)
        ru_get_compiler_flag_test_name(tmp "{lang:u}" "${{flag}}")
        set("${{result}}" "${{tmp}}" PARENT_SCOPE)
    endfunction()

    #
    # Test if the {lang:h} compiler supports <flag>. Store the test's result in
    # the internal cache variable HAVE_{lang:u}_FLAG_${{flag_name}}, where
    # flag_name is the result of converting "${{flag}}" to uppercase and
    # sanitizing it into a legal CMake variable name.
    #
    # If the test succeeds, then set the non-cache variable
    # {lang:u}_FLAG_${{flag_name}} to the string <flag>.  If the test fails,
    # then unset {lang:u}_FLAG_${{flag_name}}.
    #
    # Example:
    #
    #   If the {lang:h} compiler supports -Werror=format, then
    #
    #       ru_add_{lang:l}_flag_checked(CMAKE_{lang:u}_FLAGS "-Werror=format")
    #
    #   sets the cache variable
    #
    #       HAVE_{lang:u}_FLAG_WERROR_FORMAT:INTERNAL=1
    #
    #   and updates the non-cache variables
    #
    #       set({lang:u}_FLAG_WERROR_FORMAT "-Werror=format")
    #
    # Example:
    #
    #   If the {lang:h} compiler does not support -Werror=format, then
    #
    #       ru_add_{lang:l}_flag_checked(CMAKE_{lang:u}_FLAGS "-Werror=format")
    #
    #   sets the cache variable
    #
    #       HAVE_{lang:u}_FLAG_WERROR_FORMAT:INTERNAL=0
    #
    #   and updates the the non-cache variable
    #
    #       unset({lang:u}_FLAG_WERROR_FORMAT)
    #
    function(ru_check_{lang:l}_flag flag)
        ru_get_{lang:l}_flag_name(flag_name "${{flag}}")
        ru_get_{lang:l}_flag_test_name(test_name "${{flag}}")

        # Replace the messages from check_{lang:l}_compiler_flag with custom,
        # more helpful messages. But mimic the style of the original messages.
        set(CMAKE_REQUIRED_QUIET ON)
        set(msg "Performing Test ${{test_name}} for {lang:h} compiler flags \\"${{flag}}\\"")

        message(STATUS "${{msg}}")
        check_{lang:l}_compiler_flag("${{flag}}" ${{test_name}})

        if(${{test_name}})
            message(STATUS "${{msg}} - Success")
            set(${{flag_name}} "${{flag}}" PARENT_SCOPE)
        else()
            message(STATUS "${{msg}} - Failed")
            unset(${{flag_name}} PARENT_SCOPE)
        endif()
    endfunction()

    #
    # Test if the {lang:h} compiler supports <flag> with
    # ru_check_{lang:l}_flag.  If the test succeeds, then add the compiler
    # <flag> to <var>, where <var> is a string of {lang:h} compiler flags.
    #
    # Example:
    #
    #   If the {lang:h} compiler supports -Werror=format, then
    #
    #       ru_add_{lang:l}_flag_checked(CMAKE_{lang:u}_FLAGS "-Werror=format")
    #
    #   sets the cache variable
    #
    #       HAVE_{lang:u}_FLAG_WERROR_FORMAT:INTERNAL=1
    #
    #   and updates the non-cache variables
    #
    #       set({lang:u}_FLAG_WERROR_FORMAT "-Werror=format")
    #       string(APPEND CMAKE_{lang:u}_FLAGS " -Werror=format")
    #
    # Example:
    #
    #   If the {lang:h} compiler does not support -Werror=format, then
    #
    #       ru_add_{lang:l}_flag_checked(CMAKE_{lang:u}_FLAGS "-Werror=format")
    #
    #   sets the cache variable
    #
    #       HAVE_{lang:u}_FLAG_WERROR_FORMAT:INTERNAL=0
    #
    #   and updates the the non-cache variable
    #
    #       unset({lang:u}_FLAG_WERROR_FORMAT)
    #
    #   and does not modify CMAKE_{lang:u}_FLAGS.
    #
    function(ru_add_{lang:l}_flag_checked var flag)
        ru_get_{lang:l}_flag_test_name(test_name "${{flag}}")
        ru_check_{lang:l}_flag("${{flag}}")

        if(${{test_name}})
            set(${{var}} "${{${{var}}}} ${{flag}}" PARENT_SCOPE)
        endif()
    endfunction()
    """)

class LangName:

    __slots__ = ('human', 'cmake')

    def __init__(self, *, human, cmake):
        self.human = human
        self.cmake = cmake

    def __format__(self, format_spec):
        if format_spec == 'h':
            return self.human
        elif format_spec == 'l':
            return self.cmake.lower()
        elif format_spec == 'u':
            return self.cmake.upper()
        else:
            assert False

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lang', choices=('c', 'c++'))
    return ap.parse_args()

def main():
    pargs = parse_args()

    if pargs.lang == 'c':
        lang = LangName(human='C', cmake='C')
    elif pargs.lang == 'c++':
        lang = LangName(human='C++', cmake='CXX')
    else:
        assert False

    sys.stdout.write(template.format(lang=lang))

if __name__== '__main__':
    main()
