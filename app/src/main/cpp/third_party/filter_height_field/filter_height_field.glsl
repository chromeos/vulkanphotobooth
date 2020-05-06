/*
 * Copyright 2020 Simon Green (@simesgreen)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Code modified from https://www.shadertoy.com/view/Xss3zr by Simon Green

#include "../../shaders/filter_drawing4.glsl"

const int HEIGHT_FIELD_STEPS = 64;
//const int HEIGHT_FIELD_STEPS = 32;

// Calculate luminance for a given RGB value
float luminance(vec3 color) {
//    return color.r;
    return dot(color, vec3(0.333, 0.333, 0.333));
}

// Rotation transforms
// Rotate around X axis
vec3 heightFieldRotateX(vec3 p, float angle) {
    float sin_angle = sin(angle);
    float cos_angle = cos(angle);
    vec3 rotated;
    rotated.x = p.x;
    rotated.y = cos_angle*p.y - sin_angle*p.z;
    rotated.z = sin_angle*p.y + cos_angle*p.z;
    return rotated;
}

// Rotate around Y axis
vec3 heightFieldRotateY(vec3 p, float angle) {
    float sin_angle = sin(angle);
    float cos_angle = cos(angle);
    vec3 rotated;
    rotated.x = cos_angle*p.x + sin_angle*p.z;
    rotated.y = p.y;
    rotated.z = -sin_angle*p.x + cos_angle*p.z;
    return rotated;
}

// Rotate around Z axis
vec3 heightFieldRotateZ(vec3 p, float angle) {
    float sin_angle = sin(angle);
    float cos_angle = cos(angle);
    vec3 rotated;
    rotated.x = cos_angle*p.x - sin_angle*p.y;
    rotated.y = sin_angle*p.x + cos_angle*p.y;
    rotated.z = p.z;
    return rotated;
}

bool intersectBox(vec3 ro, vec3 rd, vec3 boxmin, vec3 boxmax, out float tnear, out float tfar) {
    // compute intersection of ray with all six bbox planes
    vec3 invR = 1.0 / rd;
    vec3 tbot = invR * (boxmin - ro);
    vec3 ttop = invR * (boxmax - ro);

    // re-order intersections to find smallest and largest on each axis
    vec3 tmin = min (ttop, tbot);
    vec3 tmax = max (ttop, tbot);

    // find the largest tmin and the smallest tmax
    vec2 t0 = max (tmin.xx, tmin.yz);
    tnear = max (t0.x, t0.y);
    t0 = min (tmax.xx, tmax.yz);
    tfar = min (t0.x, t0.y);

    // check for hit
    if ((tnear > tfar)) {
        return false;
    } else {
        return true;
    }
}

// Background gradient
vec3 background(vec3 rd) {
    vec3 TOP_COLOUR = vec3(0.5, 0.5, 0.5); // Grey
    vec3 BOTTOM_COLOUR = vec3(0.0, 0.0, 0.0); // Black
    return mix(TOP_COLOUR, BOTTOM_COLOUR, abs(rd.y));
}

// Background gradient
vec3 background_y(float y) {
    vec3 TOP_COLOUR = vec3(0.5, 0.5, 0.5); // Grey
    vec3 BOTTOM_COLOUR = vec3(0.0, 0.0, 0.0); // Black
    return mix(TOP_COLOUR, BOTTOM_COLOUR, y);
}

// Convert world point to sample point
vec2 worldToText(vec3 world_point) {
    vec2 sample_point = (world_point.xz * 0.5) + 0.5;
    sample_point.y = 1.0 - sample_point.y; // y-flip
    return sample_point;
}

// Sample the 2D texture from the 3D world point
vec4 sampleFromWorldPoint(sampler2D sampler_2d, vec3 world_point) {
    vec2 sample_point = worldToText(world_point);

    if (sample_point.x < 0.0 || sample_point.x > 1.0
        || sample_point.y < 0.0 || sample_point.y > 1.0) {
        return vec4(0.0, 0.0, 0.0, 1.0); // If out of bounds, return black
        //        return vec4(background_y(world_point.x), 1.0); // If out of bounds, return background
    }

    return texture (sampler_2d, sample_point);
}

// Sample the 2D texture - through the drawing filter - from the 3D world point
vec4 sampleDrawingFromWorldPoint(sampler2D sampler2d, vec3 world_point, float width, float height, int drawing_level) {
    vec2 sample_point = worldToText(world_point);

    if (sample_point.x < 0.0 || sample_point.x > 1.0
        || sample_point.y < 0.0 || sample_point.y > 1.0) {
        return vec4(0.0, 0.0, 0.0, 1.0); // If out of bounds, return black
    }
    return filterDrawing4(sampler2d, sample_point, width, height, float(drawing_level));
}

// Get the height field (0.0-1.0) based on luminance
float heightField(sampler2D sampler_2d, vec3 p) {
    vec3 rgb = sampleFromWorldPoint(sampler_2d, p).rgb;
    return luminance(rgb) * luminance(rgb);
//    vec2 sample_point = worldToText(p);
//    return length(fwidth(sample_point));
}


// Draw the height field for the given position
bool traceHeightField(sampler2D sampler_2d, vec3 ro, vec3 rayStep, out vec3 hitPos, int height_field_steps) {
    vec3 p = ro;
    bool hit = false;
    float prev_h = 0.0;
    vec3 prev_p = p;

    if (height_field_steps < 1)
        height_field_steps = 1;

    for(int i = 0; i < height_field_steps; i++) {
        float h = heightField(sampler_2d, p);
        if ((p.y < h) && !hit) {
            hit = true;
            //hitPos = p;
            // interpolate based on height
            hitPos = mix(prev_p, p, (prev_h - prev_p.y) / ((p.y - prev_p.y) - (h - prev_h)));
        }
        prev_h = h;
        prev_p = p;
        p += rayStep;
    }
    return hit;
}


// This doesn't work, don't use it.
bool binaryHeightField(sampler2D sampler_2d, vec3 ro, vec3 rd, float stepRange, out vec3 hitPos)
{
    vec3 p = ro;
    bool hit = false;
    float prev_h = 0.0;
    vec3 prev_p = p;

    vec3 stepRangeVector = rd;
//    float fuzz = 0.5 * length(ro);
    float fuzz = 0.2;

    float next_step = 1.0; //next step percentage

    while (!hit) {

        float h = heightField(sampler_2d, p);

        if (((p.y + fuzz) > h) && ((p.y - fuzz) < h)) {
            hit = true;
            hitPos = p;
//            hitPos = mix(prev_p, p, (prev_h - prev_p.y) / ((p.y - prev_p.y) - (h - prev_h)));
            return true;

            // Missed, step up or down
        } else {
            prev_p = p;
            prev_h = h;

            // Binary search
            next_step *= 0.5;

            // Too low, step up
            if (p.y < h) {
                p += rd * next_step * stepRange;

                // Too high step down
            } else {
                p -= rd * next_step * stepRange;
            }
        }
    }

    return false;
}

vec4 filterHeightField(sampler2D sampler_2d, vec2 tex_coord, float width, float height, int time_value, int rotation, int height_field_effect_level, int drawing_effect_level, bool is_drawing_engaged) {
    vec2 resolution = vec2(width, height);
    vec2 abs_pos = tex_coord * resolution;

    float effect_scale_amount = float(height_field_effect_level) / 100.0; // 0 - 1
    vec2 pixel_position = (tex_coord * (1.0 + 1.0 * effect_scale_amount)) - (1.0 * effect_scale_amount); // Shrink to 1/4 size at full effect level
//    vec2 pixel_position = tex_coord;

    // compute ray origin and direction
    float aspect_ratio = height / width;
    vec3 rd = normalize(vec3(aspect_ratio * pixel_position.x, pixel_position.y, -1.0 - (0.25 * effect_scale_amount))); // Z controls the scaling
    vec3 ro = vec3(-1.0 + effect_scale_amount, -1.0 + effect_scale_amount, 3.0); // Camera position: centered and zoomed out

    if (0 == rotation) {
        aspect_ratio = width / height;
        pixel_position = (tex_coord * (1.0 + 1.0 * effect_scale_amount)) - (1.0 * effect_scale_amount); // Shrink to 1/4 size at full effect level
        rd = normalize(vec3(pixel_position.x, pixel_position.y, (-1.4) - (0.6 * effect_scale_amount))); // Z controls the scaling
        ro = vec3(-1.0 + effect_scale_amount, -1.0 + effect_scale_amount, 3.0); // Camera position: centered and zoomed out
    }


    // Rotate angle of view around X axis so height fields are obvious
//    float ax = -0.85;
//    float ax = float(effect_level) / 8.0;
    float effect_rotation_amount = float(height_field_effect_level) / 100.0; // 0 - 1

    float ax = -1.57 + effect_rotation_amount * 0.585;
    85; // PI / 2 + PI/12 * effect
    if (rotation == 0) {
//        ax = 3.14 + 1.57 + (effect_rotation_amount * 0.585); // PI + Pi / 12
        ax = 3.14159 + 1.5708 - effect_rotation_amount * 0.141; // PI + PI / 2
    }

   rd = heightFieldRotateX(rd, ax);
   ro = heightFieldRotateX(ro, ax);

    // Rotate around Y axis over time to show off different angles
//    float ay = sin(float(time_value)*0.01) / 2.0;
//    float ay = float(effect_level) / 8.0;
//    rd = heightFieldRotateY(rd, ay);
//    ro = heightFieldRotateY(ro, ay);

    // On phones, tilt up about PI / 6
    if (rotation == 0) {

                  float az = 0.2618 * effect_rotation_amount; // PI / 12
//        float az = 1.57; // PI/2
//        float az = 0.785; // PI/4
//        float az = 3.14 + 0.785;
        rd = heightFieldRotateZ(rd, az);
        ro = heightFieldRotateZ(ro, az);
    }

    // Intersect with bounding box
    bool hit = false;
    const vec3 boxMin = vec3(-2.0, -0.01, -2.0);
    const vec3 boxMax = vec3(2.0, 0.5, 2.0);
    float tnear, tfar;
    hit = intersectBox(ro, rd, boxMin, boxMax, tnear, tfar);

    tnear -= 0.1;
    vec3 pnear = ro + rd*tnear;
    vec3 pfar = ro + rd*tfar;

    int height_field_steps = height_field_effect_level / 2;
    if (height_field_steps < 3)
        height_field_steps = 3;

    if (rotation == 0) {
        height_field_steps *= 2;
    }

    float stepSize = length(pfar - pnear) / float(height_field_steps);
    float stepRange = length(pfar - pnear);

    vec3 rgb = background(rd); // Default to background gradient
    if(hit) {
        // rgb = sampleFromWorldPoint(sampler_2d, pnear).rgb; // Sample flat image

        // now intersect with heightfield
        ro = pnear;
        vec3 hitPos;
        hit = traceHeightField(sampler_2d, ro, rd * stepSize, hitPos, height_field_steps);
        if (hit) {
            if (is_drawing_engaged) {
                rgb = sampleDrawingFromWorldPoint(sampler_2d, hitPos, width, height, drawing_effect_level).rgb;
            } else {
                rgb = sampleFromWorldPoint(sampler_2d, hitPos).rgb;
            }
        }
    }

    return vec4(rgb, 1.0);
}