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

#include "filter_drawing4.glsl"

// Checks if given the coordinate is inside a hexagon
bool isInsideHexagon(vec2 center, vec2 point, float radius) {
    // Normalize points to 1
    vec2 centerNorm = center / radius;
    vec2 pointNorm = point / radius;

    // Make point relative to 0,0
    vec2 pointRelative = pointNorm - centerNorm;

    // Check length (squared) against inner and outer radius
    float lengthSquared = pointRelative.x * pointRelative.x + pointRelative.y * pointRelative.y;
    if (lengthSquared > 1.0f) return false; // Outside widest point
    if (lengthSquared < 0.75f) return true; // Inside narrowest point (sqrt(3)/2)² = 3/4

    // Check projected hexagon against borders
    float px = pointRelative.x * 1.15470053838f; // 2/sqrt(3)
    if (px > 1.0f || px < -1.0f) return false;

    float py = 0.5f * px + pointRelative.y;
    if (py > 1.0f || py < -1.0f) return false;

    if (px - py > 1.0f || px - py < -1.0f) return false;

    return true;
}

// Checks if a given coordinate is inside a circle
bool isInsideCircle(vec2 center, vec2 point, float radius) {
    float distance = length(center - point);
    return distance < radius;
}

vec3 calculateRefractVector(vec2 position, float radius, float distortion) {
    float z = (radius - position.x * position.x - position.y * position.y); // r-x²-y²
    vec3 normal = normalize(vec3(position.x, position.y, z));
    vec3 refractVector = refract(vec3(0.0, 0.0, -1.0), normal, distortion);

    return refractVector;
}

/**
 * Make a "Bee's Eye" view of the image.
 * bee_level is the extent to offset each second row and simultaneous scale down the image
 * Also has a pass through to layer this filter with the drawing filter
 */
vec4 filterBeeEye(sampler2D sampler_2d, vec2 tex_coord, float width, float height, float bee_level, bool combine_with_drawing, float drawing_level) {
    vec4 color;

    bee_level *= 0.5;
    float distortion_variable = bee_level * 0.008; // Converts 0->100 to 0.0->0.4

    float cellScaleWidth = 0.06;
    float cellScaleHeight = 0.10;
    float distortion = 1.0 + distortion_variable; // Default is 1.2
    vec4 cellColour = vec4(0.6, 0.6, 0.6, 1.0); // Grey cell walls
//    vec4 cellColour = vec4(0.9, 0.5, 0.5, 1.0); // Pink cell walls

    // Lock x and y centers to rows and columns
    float yCenter = floor(tex_coord.y / cellScaleHeight) * cellScaleHeight;
    float yIndex = floor(mod(tex_coord.y / cellScaleHeight, 2.0));

    // Shift every second row up by 1/2 a cell
    float xOffset = (yIndex < 1.0) ? (cellScaleWidth / 2.0) : 0.0;
    float xCenter = floor((tex_coord.x + xOffset) / cellScaleWidth) * cellScaleWidth;

    vec2 cellCenter = vec2(xCenter + (cellScaleWidth / 2.0), yCenter + (cellScaleHeight / 2.0));
    vec2 outputCoord = vec2(tex_coord.x + xOffset, tex_coord.y); // Note: outputCoord is just tex_coord with each 2nd row shifted

    float cellWallScale = 1.2;
    float radiusMod = (bee_level / 150.0) + 1.2; // 0-100 -> 1.2-1.7
    float hexRadius = cellScaleHeight / radiusMod;

    // If the position is outside of an "eye", add a background colour
    if (!isInsideHexagon(cellCenter, outputCoord, hexRadius)) {
        return cellColour;
/*
        // Add bevelled "mirror" edges
        if (mod(outputCoord.x, cellScaleWidth) < (cellScaleWidth / 2.0)) {
            outputCoord.x += cellScaleWidth / 2.0;
//            outputCoord.x -= xOffset;
        } else {
            outputCoord.x -= cellScaleWidth / 2.0;
            outputCoord.x += xOffset;
        }

        if (mod(outputCoord.y, cellScaleHeight) < (cellScaleHeight / 2.0)) {
            outputCoord.y += cellScaleHeight / 2.0;
        } else {
            outputCoord.y -= cellScaleHeight / 2.0;
        }
*/
    }

    // Add distortion using spherical refraction
    vec2 position = cellCenter - outputCoord;
    vec3 refractVector = calculateRefractVector(position, hexRadius, distortion);
    vec2 samplePos = outputCoord - refractVector.xy;

    if (samplePos.x < 0.0) {
        samplePos.x *= -1.0;
    }
    if (samplePos.x > 1.0) {
        samplePos.x = 1.0 - (samplePos.x - 1.0);
    }
    if (samplePos.y < 0.0) {
        samplePos.y *= -1.0;
    }
    if (samplePos.y > 1.0) {
        samplePos.y = 1.0 - (samplePos.y - 1.0);
    }

    samplePos = clamp(samplePos, 0.0, 1.0);

    // If the drawing filter is engaged, also pass through to that filter, otherwise sample directly
    if (combine_with_drawing) {
        color = filterDrawing4(sampler_2d, samplePos, width, height, drawing_level);
    } else {
        color = texture(sampler_2d, samplePos);
    }

    return color;
}