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

CMAKE_MODULES := \
    RuAddCFlag.cmake \
    RuAddCXXFlag.cmake \
    RuCheckCAttribute.cmake \
    RuCheckCXXAttribute.cmake \
    $(NULL)

.PHONY: all
all: $(CMAKE_MODULES)
	@

.PHONY: clean
clean:
	$(RM) $(CMAKE_MODULES)

RuAddCFlag.cmake: RuAddCompilerFlagsGen.py
	./$< --lang=c > $@

RuAddCXXFlag.cmake: RuAddCompilerFlagsGen.py
	./$< --lang=c++ > $@

RuCheckCAttribute.cmake: RuCheckCompilerAttributeGen.py
	./$< --lang=c > $@

RuCheckCXXAttribute.cmake: RuCheckCompilerAttributeGen.py
	./$< --lang=c++ > $@
