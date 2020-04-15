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

package dev.hadrosaur.vulkanphotobooth

/**
 *  Parameters from Kotlin / UI to pass down to the Native layer
 *
 *  Ensure NUM_FILTERS matches with native implementation
 */
class FilterParams {
    val NUM_FILTERS = 10;
    var rotation: Int = 0;
    val seek_values = IntArray(NUM_FILTERS)
    var use_filter = BooleanArray(NUM_FILTERS)
}