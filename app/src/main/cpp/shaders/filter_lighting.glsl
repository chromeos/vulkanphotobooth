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

#define PI2 6.28318530717959

vec3 saturation2(vec3 color, float level) {
    // Relative luminance values for human vision: https://en.wikipedia.org/wiki/Relative_luminance
    const vec3 W = vec3(0.2126, 0.7152, 0.0722);
    vec3 intensity = vec3(dot(color, W));
    return mix(intensity, color, level);
}

float dist_to_line(vec2 origin, vec2 direction, vec2 point) {
    vec2 intersect = origin + direction * dot(point - origin, direction);
    return length(point - intersect);
}


/**
 * Add a linear light burst affect that rotates slowly with time
 *
 */
vec4 filterLighting(vec4 color, vec2 tex_coord, int width, int height, int time_value, int effect_level) {
    float timeStretchFactor = 100.0;
    float time = float(time_value) / timeStretchFactor;


    vec2 light_pos = vec2(0.5, 0.5); // Centre light at the origin
    float light_angle = PI2 * (float(effect_level) / 100.0) + 0.02 + time; // 0.02 fudge factor to make sure angle is never exactly PI
    vec2 light_vector = normalize (vec2(1.0, tan(light_angle) * 1.0)); // tan 0 = o / a. Can be optimized as only vector is needed.

    float distance_to_light_vector = dist_to_line(light_pos, light_vector, tex_coord);
    float dist_to_light = length(tex_coord - light_pos);


    float brightness = 5.0;
    float light_intensity = 1.0 - sqrt(distance_to_light_vector);
    brightness *= ((float(effect_level) / 100.0)) * light_intensity;

    color.rgb *= 1.0 - (0.3 * (float(effect_level) / 100.0)); // Darken everything
    color.rgb *= 1.0 + (brightness);

    return color;
}