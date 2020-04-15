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

#include "../third_party/filter_watercolour/filter_watercolour.glsl"

// Fix vertices due to rotation discrepancies between saved frame and new frame from camera
vec2 vertexFix(vec2 texcoord, float distortion_correction_normal, float distortion_correction_rotated, int rotation, int panel_id) {
    float x_scale = 1.0;
    float y_scale = 1.0;
    float x_displacement = 0.0;
    float y_displacement = 0.0;

    // Handle rotation
    switch (rotation) {
        case -270:
        case 90:
        y_scale *= distortion_correction_rotated;
        y_displacement -= (distortion_correction_rotated - 1.0) / 2.0;
        texcoord = vec2(y_scale * texcoord.y + y_displacement, x_scale * texcoord.x + x_displacement);
        texcoord = 1.0 - texcoord;
        break;

        case -180:
        case 180:
        x_scale *= distortion_correction_normal;
        x_displacement -= (distortion_correction_normal - 1.0) / 2.0;
        texcoord = vec2(x_scale * texcoord.x  + x_displacement, y_scale * texcoord.y + y_displacement);
        texcoord.y = 1.0 - texcoord.y;
        break;

        case 270:
        case -90:
        y_scale *= distortion_correction_rotated;
        y_displacement -= (distortion_correction_rotated - 1.0) / 2.0;
        texcoord = vec2(y_scale * texcoord.y + y_displacement, x_scale * texcoord.x + x_displacement);
        break;

        case 360:
        case 0:
        default:
        x_scale *= distortion_correction_normal;
        x_displacement -= (distortion_correction_normal - 1.0) / 2.0;
        texcoord = vec2(x_scale * texcoord.x  + x_displacement, y_scale * texcoord.y + y_displacement);
        texcoord.x = 1.0 - texcoord.x;
        break;
    }

    return texcoord;
}

/**
 * Add a blur effect by fading current frame with previous frame.
 * If effect level is >50 also engage a runny "watercolour" effect
 */
vec4 filterWatercolourBlur(vec4 color, sampler2D sampler_2d, sampler2D sampler_prev_2d, vec2 tex_coord,
        int imageWidth, int imageHeight, int windowWidth, int windowHeight,
        float distortion_correction_normal, float distortion_correction_rotated,
        int rotation, int panel_id, int blur_amount) {

    // Get previous frame buffer
    vec2 texcoord_old = vertexFix(tex_coord, distortion_correction_normal, distortion_correction_rotated, rotation, panel_id);

    // If effect level <= 50, just add straight blur
    if (blur_amount <= 50) {
        vec4 old_frame = texture(sampler_prev_2d, texcoord_old);
        float blur = 0.55 + (float(blur_amount) * 0.003); // 0-100 -> 0.55-0.95
        blur = sqrt(blur); // Get more blur for your value. sqrt(0.5) = .707
        return mix(color, old_frame, blur);
    }

    // Else, engage water colour effect. Square input to provide smoother transition
    blur_amount = int(pow(float(blur_amount) / 100.0, 2.0) * float(blur_amount));
    return fitlerWatercolour(color, sampler_2d, sampler_prev_2d, tex_coord, texcoord_old,
        blur_amount, abs(rotation));
}