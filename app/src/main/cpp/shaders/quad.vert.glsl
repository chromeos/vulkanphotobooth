#version 400
#pragma shader_stage(vertex)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

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

struct vulkanShaderVars {
    int panel_id;
    int imageWidth;
    int imageHeight;
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

layout (set = 0, binding = 2) uniform bufferVals {
    vulkanShaderVars shader_vars;
} myBufferVals;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 attr;
layout (location = 0) out vec2 texcoord;

void main() {
    texcoord = attr;
    float y_scale = 1;
    float x_scale = 1;
    float x_displacement = 0;
    float y_displacement = 0;
    bool invert_xy = false;

    vec2 position = pos.xy;

    // Handle rotation
    switch (myBufferVals.shader_vars.rotation) {
        case -270:
        case 90:
            y_scale = -1;
            x_scale = -1;
            invert_xy = true;

             // Scale up squashed axis to deal with window/image reader mix-match
             y_scale *= myBufferVals.shader_vars.distortion_correction_rotated;
//             y_displacement -= (myBufferVals.shader_vars.distortion_correction - 1.0);
             break;

        case -180:
        case 180:
            y_scale = -1;
            x_scale = 1;
            invert_xy = false;
            x_scale *= myBufferVals.shader_vars.distortion_correction_normal;


            switch (myBufferVals.shader_vars.panel_id) {
                case 1:
                x_displacement = 2.0; // Left
                break;
                case 2:
                x_displacement = -2.0; // Right
                break;

                case 0:
                default:
                x_displacement = 0;
                break;
            }
            break;


        case 270:
        case -90:
            y_scale = 1;
            x_scale = 1;
            invert_xy = true;

            // Scale up squashed axis to deal with window/image reader mix-match
            y_scale *= myBufferVals.shader_vars.distortion_correction_rotated;
            break;

        case 360:
        case 0:
        default:
            y_scale = 1;
            x_scale = -1; // Horizontal flip so it's a mirror
            invert_xy = false;
            x_scale *= myBufferVals.shader_vars.distortion_correction_normal;

            switch (myBufferVals.shader_vars.panel_id) {
                case 1:
                      x_displacement = 2.0; // Left
                    break;
                case 2:
                    x_displacement = -2.0; // Right
                    break;

                case 0:
                default:
                x_displacement = 0;
                break;
            }
            break;
    }

    if (invert_xy) {
        gl_Position = vec4(y_scale * position.y + y_displacement, x_scale * position.x + x_displacement, pos.z, pos.w);
    } else {
        gl_Position = vec4(x_scale * position.x + x_displacement, y_scale * position.y + y_displacement, pos.z, pos.w);
    }
}