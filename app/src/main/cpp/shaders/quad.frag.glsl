#version 310 es
#pragma shader_stage(fragment)

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

precision highp float;

#include "filter_beeeye.glsl"
#include "filter_drawing4.glsl"
#include "../third_party/filter_shapes/filter_shapes.glsl"
#include "filter_colour_blast.glsl"
#include "filter_lighting.glsl"
#include "filter_watercolour_blur.glsl"

struct vulkanShaderVars {
    int panel_id;
    int width;
    int height;
    int windowWidth;
    int windowHeight;
    int rotation;
    int seek_value1;
    int seek_value2;
    int seek_value3;
    int seek_value4;
    int seek_value5;
    int seek_value6;
    int seek_value7;
    int seek_value8;
    int seek_value9;
    int seek_value10;
    int time_value;
    float distortion_correction_normal;
    float distortion_correction_rotated;
    int use_filter;
};

bool isRotated(int rotation) {
    // Handle rotation
    switch (rotation) {
        case -270:
        case 90:
        case 270:
        case -90:
            return true;

        case -180:
        case 180:
        case 360:
        case 0:
        default:
            return false;
    }

    return false;
}

layout (set = 0, binding = 2) uniform bufferVals {
    vulkanShaderVars shader_vars;
} buffer_vals;

layout(binding=0) uniform sampler2D sampler2d;
layout(binding=1) uniform sampler2D prevSampler2d;
layout(location=0) in vec2 tex_coord;
layout(location=0) out vec4 color;

/**
 * 1 - Bee eye filter
 * 2 - Drawing filter
 * 3 - Glyphs
 * 4 - Colour mod
 * 5 - Lighting mod
 * 6 - Blur (0-50%), Watercolour (51-100%)
 */

void main() {

    // Depth field
    //if ((buffer_vals.shader_vars.use_filter & 0x01) != 0) { // 0b 0000 0001
    //    if ((buffer_vals.shader_vars.use_filter & 0x02) == 0) { // Don't use if other sampling based filters are engaged
    //        color = filterHeightField(sampler2d, tex_coord, float(buffer_vals.shader_vars.width), float(buffer_vals.shader_vars.height), buffer_vals.shader_vars.time_value, buffer_vals.shader_vars.seek_value1, buffer_vals.shader_vars.rotation);
    //    }
    //}

    // Bee-eye
    if ((buffer_vals.shader_vars.use_filter & 0x01) != 0) { // 0b 0000 0001
        if ((buffer_vals.shader_vars.use_filter & 0x02) == 0) { // Don't use if other sampling based filters are engaged
            color = filterBeeEye(sampler2d, tex_coord, float(buffer_vals.shader_vars.width), float(buffer_vals.shader_vars.height), float(buffer_vals.shader_vars.seek_value1), false, float(buffer_vals.shader_vars.seek_value2));
        }
    }

    // Drawing
    if ((buffer_vals.shader_vars.use_filter & 0x02) != 0) { // 0b 0000 0010
        if ((buffer_vals.shader_vars.use_filter & (0x01)) == 0) { // Depth field not engaged
            color = filterDrawing4(sampler2d, tex_coord, float(buffer_vals.shader_vars.width), float(buffer_vals.shader_vars.height), float(buffer_vals.shader_vars.seek_value2));

        } else { // Drawing and BeeEye are both engaged
            color = filterBeeEye(sampler2d, tex_coord, float(buffer_vals.shader_vars.width), float(buffer_vals.shader_vars.height), float(buffer_vals.shader_vars.seek_value1), true, float(buffer_vals.shader_vars.seek_value2));
        }
    }

    // If none of the multi-sampling-based shaders are selected, do a basic colour sample
    if ((buffer_vals.shader_vars.use_filter & (0x01 | 0x02 )) == 0) {
        color = texture (sampler2d, tex_coord);
    }

    // Glyphs
    if ((buffer_vals.shader_vars.use_filter & 0x04) != 0) { // 0b 0000 0100
        color = filterShapes(color, tex_coord, buffer_vals.shader_vars.width, buffer_vals.shader_vars.height, buffer_vals.shader_vars.time_value, buffer_vals.shader_vars.seek_value3);
    }

    // Color messer upper
    if ((buffer_vals.shader_vars.use_filter & 0x08) != 0) { // 0b 0000 1000
        color = filterColourBlast(color, buffer_vals.shader_vars.time_value, buffer_vals.shader_vars.seek_value4);
    }

    // Lighting
    if ((buffer_vals.shader_vars.use_filter & 0x10) != 0) { // 0b 0001 0000
        color = filterLighting(color, tex_coord, buffer_vals.shader_vars.width, buffer_vals.shader_vars.height, buffer_vals.shader_vars.time_value, buffer_vals.shader_vars.seek_value5);
    }

    // Motion Blur / Watercolour
    if ((buffer_vals.shader_vars.use_filter & 0x20) != 0) { // 0b 0010 0000
        color = filterWatercolourBlur(color, sampler2d, prevSampler2d, tex_coord, buffer_vals.shader_vars.width, buffer_vals.shader_vars.height, buffer_vals.shader_vars.windowWidth, buffer_vals.shader_vars.windowHeight, buffer_vals.shader_vars.distortion_correction_normal, buffer_vals.shader_vars.distortion_correction_rotated, buffer_vals.shader_vars.rotation, buffer_vals.shader_vars.panel_id, buffer_vals.shader_vars.seek_value6);
    }
}