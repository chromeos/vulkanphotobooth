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

#ifndef ANDROID_FILTERPARAMS_H
#define ANDROID_FILTERPARAMS_H

/**
 * Struct for filter parameters passed down from Kotlin
 */

#define NUM_FILTERS 6

struct FilterParams {
    int rotation = 0;
    int32_t seek_values[NUM_FILTERS];
    int8_t use_filter[NUM_FILTERS];
};

#endif //ANDROID_FILTERPARAMS_H