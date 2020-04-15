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

#ifndef FILTER_DRAWING
#define FILTER_DRAWING

bool isEven(float number) {
    return (0.0 == mod(number, 2.0));
}

// Naive colour quantizer
vec3 color_quantize(vec3 color_in, float steps) {
    return ceil(color_in * steps) / steps;
}

// Color dither
// color_step is the step distance per colour
vec3 color_dither(vec3 color_in, vec2 abs_pos, vec3 color_step) {

    // 9-factor dither
    vec3 dither_9 = mod(color_in / color_step, 3.0);

    float len = length(dither_9); // 0 - 6

    // If "black", no dithering needed
    if(len == 0.0) {
        return color_in;

        // If relatively dark, relaxed dither
    } else if(len == 1.) {
        if(isEven(abs_pos.x + abs_pos.y)) {
            return color_in + color_step;
        } else {
            return color_in - color_step;
        }

        // If a lighter colour, more agressive dither
    } else {
        if(isEven(abs_pos.x + 0.5) && isEven(abs_pos.y + 0.5)) {
            return color_in - color_step;
        } else {
            return color_in + color_step;
        }
    }
}

// Whatever the dominant colour is (RGB), pump it up to make everything more cartoony
vec3 primary_colors(vec3 color_in) {
    if ((color_in.r >= color_in.g)
        && (color_in.r >= color_in.b)) {
        // Red is biggest, pump it up
        color_in.r = sqrt(sqrt(color_in.r));
    } else {
        if ((color_in.g >= color_in.r)
        && (color_in.g >= color_in.b)) {
            // Green is biggest, pump it up
            color_in.g = sqrt(sqrt(color_in.g));
        } else {
            // Must be blue
            color_in.b = sqrt(sqrt(color_in.b));
        }
    }

    return color_in;
}

vec3 saturation(vec3 color, float level) {
    // Relative luminance values for human vision: https://en.wikipedia.org/wiki/Relative_luminance
    const vec3 W = vec3(0.2126, 0.7152, 0.0722);
    vec3 intensity = vec3(dot(color, W));
    return mix(intensity, color, level);
}

vec3 cartoonify_colors(vec3 color, float saturation_level, vec2 abs_pos, float steps, vec3 color_step) {
    vec3 staturized = saturation(color, saturation_level);
    vec3 c = color_quantize(staturized, steps);
    color = color_dither(c, abs_pos, color_step);

    // This only works for some lighting/background situations
    //color = primary_colors(color);

    return color;
}

// Generic convolution function
float convolve(mat3 kernel, mat3 pixels) {
    float result = 0.0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result += kernel[i][j] * pixels[i][j];
        }
    }
    return result;
}

// Basic sobel edge filter. See http://en.wikipedia.org/wiki/Sobel_operator
float sobel(sampler2D sampler_2d, vec2 tex_coord, vec2 sobel_distance) {
    const mat3 SOBEL_KERNEL_X = mat3(
        1.0, 0.0, -1.0,
        2.0, 0.0, -2.0,
        1.0, 0.0, -1.0);

    const mat3 SOBEL_KERNEL_Y = mat3(
        1.0, 2.0, 1.0,
        0.0, 0.0, 0.0,
        -1.0, -2.0, -1.0);

    // Sample given coord and 8 pixels surrounding it
    mat3 pixels = mat3(
        length(texture(sampler_2d, tex_coord + vec2(-sobel_distance.x, sobel_distance.y)).rgb),
        length(texture(sampler_2d, tex_coord + vec2(0, sobel_distance.y)).rgb),
        length(texture(sampler_2d, tex_coord + sobel_distance).rgb),
        length(texture(sampler_2d, tex_coord + vec2(-sobel_distance.x, 0)).rgb),
        length(texture(sampler_2d, tex_coord).rgb),
        length(texture(sampler_2d, tex_coord + vec2(sobel_distance.x, 0)).rgb),
        length(texture(sampler_2d, tex_coord + -sobel_distance).rgb),
        length(texture(sampler_2d, tex_coord + vec2(0,-sobel_distance.y)).rgb),
        length(texture(sampler_2d, tex_coord + vec2(sobel_distance.x,-sobel_distance.y)).rgb));

    vec2 result = vec2(convolve(SOBEL_KERNEL_X, pixels), convolve(SOBEL_KERNEL_Y, pixels));
    float level = clamp(length(result), 0.0, 1.0);

    return level;
}

/**
 * Return the float 0.0-1.0 level of darkness for this outline
 * 1.0 == no outline, 0.0 == darkest outline
 *
 * Samples given coord using a sobel filter. If not an edge, check if it is to either side of an
 * edge along the path defined by DRAWING_ANGLE. If so, smudge in a line to give a comic-book effect
 */
float drawing_outline(sampler2D sampler_2d, vec2 tex_coord, float width, float height, float effect_level) {
    const float EDGE_CUTOFF = 0.35;
    const float SMUDGE_CENTRE = 0.45; // Level at pixel centre
    const float SMUDGE_BEGIN = 0.95; // Level of beginning of brush stroke
    const float SMUDGE_END = 0.85; // Level of beginning of brush stroke
    const float BRUSH_STROKE_SPACING = 3.0; // Spacing between brush lines

    vec2 DRAWING_ANGLE = normalize(vec2(1.0, 1.0)); // 45 degrees

    vec2 resolution = vec2 (width, height);
    vec2 normalized = (1.0 + tex_coord) / 2.0; // -1.0-1.0 => 0.0-1.0
    vec2 uv = normalized * resolution;

    float sobel_level = effect_level * 8.20 / 100.0;
    vec2 sobel_distance = vec2(sobel_level / width, sobel_level / height);
    float brush_stroke_length = effect_level * 24.0 / 100.0;
    float brush_stroke_thickness = 1.0;

    vec2 brush_stroke_start = tex_coord + vec2(brush_stroke_length/width * DRAWING_ANGLE.x, -1.0 * brush_stroke_length/height * DRAWING_ANGLE.y);
    vec2 brush_stroke_end = tex_coord - vec2(brush_stroke_length/width * DRAWING_ANGLE.x, -1.0 * brush_stroke_length/height * DRAWING_ANGLE.y);

    float level = 0.0;

    // Sobel filter for centre point
    float level_centre = sobel(sampler_2d, tex_coord, sobel_distance);

    // If this pixel is on an edge, the sobel filter will darken it already.
    // If not, check if this pixel is part of brush stroke
    if (level_centre > EDGE_CUTOFF) {
        level = level_centre * SMUDGE_CENTRE;
    } else {
        // A brush stroke on every pixel would just thicken all lines
        // Only include brush stroke every BRUSH_STROKE_SPACING pixels
        if(0.0 == floor(mod(length(uv * DRAWING_ANGLE), BRUSH_STROKE_SPACING))) {
            float level_start = sobel(sampler_2d, brush_stroke_start, sobel_distance);
            if (level_start > EDGE_CUTOFF) {
                level = level_start * SMUDGE_BEGIN;
            } else {
                float level_end = sobel(sampler_2d, brush_stroke_end, sobel_distance);
                if (level_end >EDGE_CUTOFF) {
                    level = level_end * SMUDGE_END;
                }
            }
        }
    }
    return 1.0 - level;
}

vec4 filterDrawing4(sampler2D sampler_2d, vec2 tex_coord, float width, float height,  float effect_level) {
    vec2 resolution = vec2(width, height);
    vec2 abs_pos = tex_coord * resolution;
    vec2 coordStep = (1. / resolution);

    float steps = 5.0 + (100.0 - effect_level) / 8.0;
    vec3 component_mix = normalize(vec3(0.5, 0.5, 0.5));
    vec3 color_step = component_mix / steps;

    float saturation_level = 1.0 + effect_level / 80.0;

    // Get sample
    vec3 tex = texture(sampler_2d, tex_coord).xyz;

    // Cartoonify the colours
    vec3 color = cartoonify_colors(tex, saturation_level, abs_pos, steps, color_step);

    // Brightness/gamma adjust
    float gamma = 0.5;
    color = pow(color, vec3(1.0/gamma));
    float brightness = 0.3;
    color += brightness;

    // Get the outline (1.0 == no outline, 0.0 == darkest outline)
    float outline = drawing_outline(sampler_2d, tex_coord, width, height, effect_level);

    return vec4(color * outline, 1.0);
}

#endif