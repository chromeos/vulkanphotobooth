/*
 * The MIT License
 * Copyright © 2017 Michael Schuresko
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions: The above copyright notice and this
 * permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Adapted and Modified by Google LLC. Original code found at: https://www.shadertoy.com/view/4lV3RG
 */
vec4 fitlerWatercolour(vec4 color, sampler2D sampler_2d, sampler2D sampler_prev_2d,
    vec2 tex_coord, vec2 texcoord_old, int blur_amount, int rotation) {

    float delta = 0.75 * float(blur_amount) * 0.000064;// Distance from each pixel to sample past frame
    //    float delta = (50.0 * 16.0) / 100000.0;
    //    float delta = 0.008;

    // Things get interesting around 0.05. Try a range of 0.1 - 0.001
    float intensity_component = 0.1001 - (float(blur_amount) * 0.66 * 0.001);
    float intensity_remainder = 1.0 - intensity_component;

    vec3 current_frame_color = color.rgb;
    vec3 intensity = 1.0 - current_frame_color;

    float vidSample = dot(vec3(1.0), current_frame_color);
    float vidSampleDx = dot(vec3(1.0), texture(sampler_2d, clamp(tex_coord + vec2(delta, 0.0), 0.0, 1.0)).rgb);
    float vidSampleDy = dot(vec3(1.0), texture(sampler_2d, clamp(tex_coord + vec2(0.0, delta), 0.0, 1.0)).rgb);

    vec2 flow = delta * vec2 (vidSampleDy - vidSample, vidSample - vidSampleDx);
    vec2 flow_direction;

    // Account for camera rotation
    if (rotation == 0 || rotation == 180 || rotation == 360) {
        flow_direction = vec2(1.0, -1.0);
    } else {
        flow_direction = vec2(-1.0, 1.0);
    }
    intensity = intensity_component * intensity + intensity_remainder * (1.0 - texture(sampler_prev_2d, clamp(texcoord_old +  flow_direction * flow, 0.0, 1.0)).rgb);
    return vec4(1.0 - intensity, 1.0);
}