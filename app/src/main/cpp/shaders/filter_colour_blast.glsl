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

// Simple colour messer-upper that morphs over time
vec4 filterColourBlast(vec4 color, int time_value, int color_value) {

    float time = float (time_value) / 40.0;
    float mix_level = float (color_value) / 100.0;

    vec4 modded_color = color;
    modded_color.rgb += sin(modded_color.rgb * (10.0) + time + vec3(3.0 , 1.5 , 0.5 * modded_color.r));
//    modded_color.rgb *= 0.5;
//    modded_color.rgb += 0.5;
//    modded_color.gb = clamp(color.gb, 0.3, 0.8);
    return mix(color, modded_color, mix_level);
}