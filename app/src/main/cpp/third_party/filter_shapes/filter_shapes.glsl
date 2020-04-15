/*
 * Copyright 2020 Justin Gitlin
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


// Adapted from: https://www.shadertoy.com/view/Xd2BRm by Justin Gitin (license above)
// See also: https://cacheflowe.com/

float colorSteps = 8.;
float scale = 0.25;
float timeFactor = 0.003;

float rgbToGray(vec4 rgba) {
    const vec3 W = vec3(0.2125, 0.7154, 0.0721);
    return dot(rgba.xyz, W);
}

vec2 rotateCoord(vec2 uv, float rads) {
    uv *= mat2(cos(rads), sin(rads), -sin(rads), cos(rads));
    return uv;
}

float halftoneDots(vec2 uv, float rows, float curRadius, float curRotation, float invert, vec2 resolution) {
    // update layout params
    // get original coordinate, translate & rotate
    uv = rotateCoord(uv, curRotation);
    // calc row index to offset x of every other row
    float rowIndex = floor(uv.y * rows);
    float oddEven = mod(rowIndex, 2.);
    // create grid coords
    vec2 uvRepeat = fract(uv * rows) - 0.5;
    if(oddEven == 1.) {							// offset x by half
        uvRepeat = fract(vec2(0.5, 0.) + uv * rows) - vec2(0.5, 0.5);
    }
    // adaptive antialiasing, draw, invert
    float aa = resolution.y * rows * 0.00001;
    float col = smoothstep(curRadius - aa, curRadius + aa, length(uvRepeat));
    if(invert == 1.) col = 1. - col;
    return col;
}

float halftoneLines(vec2 uv, float rows, float curThickness, float curRotation, float invert, vec2 resolution) {
    // get original coordinate, translate & rotate
    uv = rotateCoord(uv, curRotation);
    // create grid coords
    vec2 uvRepeat = fract(uv * rows);
    // adaptive antialiasing, draw, invert
    float aa = resolution.y * 0.0003;
    float col = smoothstep(curThickness - aa, curThickness + aa, length(uvRepeat.y - 0.5));
    if(invert == 1.) col = 1. - col;
    return col;
}

vec4 filterShapes(vec4 color, vec2 tex_coord, int width, int height, int time_value, int level) {
    vec2 resolution = vec2(width, height);
    vec2 tex_coord_absolute = tex_coord * resolution;
    vec2 tex_coord_tone = (2. * tex_coord_absolute - resolution.xy) / resolution.y;
//    tex_coord.x = 1. - tex_coord.x; // mirror

    float luma = rgbToGray(color) * 1.;
    float lumaIndex = floor(luma * colorSteps);
    float lumaFloor = lumaIndex / colorSteps;

    // time
    float time = float(time_value) * timeFactor;

    // posterize -> halftone gradient configurations
    float halftoneCol = 0.;
    if(lumaIndex == 0.) {
        halftoneCol = halftoneDots(tex_coord_tone, scale * 50., 0.1, 0.2 + time, 1., resolution);
    } else if(lumaIndex == 1.) {
        halftoneCol = halftoneLines(tex_coord_tone, scale * 84., 0.08, 2. - time, 1., resolution);
    } else if(lumaIndex == 2.) {
        halftoneCol = halftoneDots(tex_coord_tone, scale * 120., 0.45, 0.8 + time, 0., resolution);
    } else if(lumaIndex == 3.) {
        halftoneCol = halftoneDots(tex_coord_tone, scale * 60., 0.37, 0.5 - time, 1., resolution);
    } else if(lumaIndex == 4.) {
        halftoneCol = halftoneLines(tex_coord_tone, scale * 40., 0.18, 2. + time, 0., resolution);
    } else if(lumaIndex == 5.) {
        halftoneCol = halftoneDots(tex_coord_tone, scale * 60., 0.34, 0.5 - time, 0., resolution);
    } else if(lumaIndex == 6.) {
        halftoneCol = halftoneLines(tex_coord_tone, scale * 84., 0.15, 2. + time, 0., resolution);
    } else if(lumaIndex >= 7.) {
        halftoneCol = halftoneDots(tex_coord_tone, scale * 50., 0.1, 0.2 - time, 0., resolution);
    }

    return mix(color, vec4(vec3(halftoneCol), 1.0), float(level) / 100.0);
}


