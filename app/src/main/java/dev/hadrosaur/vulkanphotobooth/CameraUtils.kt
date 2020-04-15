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

package dev.hadrosaur.vulkanphotobooth

import android.graphics.ImageFormat
import android.graphics.Rect
import android.hardware.camera2.*
import android.hardware.camera2.CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE
import android.os.Build
import android.view.Surface
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_main.*
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.FIXED_FOCUS_DISTANCE
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.allCameraParams
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.logd
import dev.hadrosaur.vulkanphotobooth.cameracontroller.Camera2CaptureSessionCallback
import dev.hadrosaur.vulkanphotobooth.cameracontroller.Camera2DeviceStateCallback
import java.util.*

/** The focus mechanism to request for captures */
enum class FocusMode(private val mode: String) {
    /** Auto-focus */
    AUTO("Auto"),
    /** Continous auto-focus */
    CONTINUOUS("Continuous"),
    /** For fixed-focus lenses */
    FIXED("Fixed")
}

/**
 * For all the cameras associated with the device, populate the CameraParams values and add them to
 * the companion object for the activity.
 */
fun initializeCameras(activity: MainActivity) {
    val manager = activity.getSystemService(AppCompatActivity.CAMERA_SERVICE) as CameraManager
    try {
        val numCameras = manager.cameraIdList.size

        for (cameraId in manager.cameraIdList) {
            val tempCameraParams = CameraParams().apply {

                logd("Camera " + cameraId + " of " + numCameras)

                // Get camera characteristics and capabilities
                val cameraChars = manager.getCameraCharacteristics(cameraId)
                val cameraCapabilities =
                    cameraChars.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES)
                        ?: IntArray(0)

                // Multi-camera?
                for (capability in cameraCapabilities) {
                    if (capability ==
                        CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) {
                        hasMulti = true
                    } else if (capability ==
                        CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR) {
                        hasManualControl = true
                    }
                }

                id = cameraId
                isOpen = false
                hasFlash = cameraChars.get(CameraCharacteristics.FLASH_INFO_AVAILABLE) ?: false
                isFront = CameraCharacteristics.LENS_FACING_FRONT ==
                        cameraChars.get(CameraCharacteristics.LENS_FACING)

                isExternal = (Build.VERSION.SDK_INT >= 23 &&
                        CameraCharacteristics.LENS_FACING_EXTERNAL ==
                        cameraChars.get(CameraCharacteristics.LENS_FACING))

                characteristics = cameraChars
                orientation = cameraChars.get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 0
                focalLengths =
                    cameraChars.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS)
                        ?: FloatArray(0)
                smallestFocalLength = smallestFocalLength(focalLengths)
                minDeltaFromNormal = focalLengthMinDeltaFromNormal(focalLengths)

                apertures = cameraChars.get(CameraCharacteristics.LENS_INFO_AVAILABLE_APERTURES)
                    ?: FloatArray(0)
                largestAperture = largestAperture(apertures)
                minFocusDistance =
                    cameraChars.get(CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE)
                        ?: MainActivity.FIXED_FOCUS_DISTANCE

                hasAF = minFocusDistance != FIXED_FOCUS_DISTANCE // If camera is fixed focus, no AF

                isLegacy = cameraChars.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL) ==
                        CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;

                val activeSensorRect: Rect = cameraChars.get(SENSOR_INFO_ACTIVE_ARRAY_SIZE)
                    ?: Rect(0, 0, 640, 480)
                megapixels = (activeSensorRect.width() * activeSensorRect.height()) / 1000000

                // Set up camera2 callbacks
                camera2DeviceStateCallback =
                    Camera2DeviceStateCallback(this, activity)
                camera2CaptureSessionCallback =
                    Camera2CaptureSessionCallback(activity, this)

                previewSurfaceView = activity.surface_mirror

                if (Build.VERSION.SDK_INT >= 28) {
                    physicalCameras = cameraChars.physicalCameraIds
                }

                // Get Camera2 and CameraX image capture sizes
                val map =
                    characteristics?.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
                if (map != null) {
                    cam2MaxSize = Collections.max(
                        Arrays.asList(*map.getOutputSizes(ImageFormat.JPEG)),
                        CompareSizesByArea()
                    )
                    cam2MinSize = Collections.min(
                        Arrays.asList(*map.getOutputSizes(ImageFormat.JPEG)),
                        CompareSizesByArea()
                    )
                }
            }

            allCameraParams.put(cameraId, tempCameraParams)
        } // For all camera devices
    } catch (accessError: CameraAccessException) {
        accessError.printStackTrace()
    }
}

/** Adds automatic flash to the given CaptureRequest.Builder */
fun setAutoFlash(params: CameraParams, requestBuilder: CaptureRequest.Builder?) {
    try {
        if (params.hasFlash) {
            requestBuilder?.set(
                CaptureRequest.CONTROL_AE_MODE,
                CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH)

            // Force flash always on
//            requestBuilder?.set(CaptureRequest.CONTROL_AE_MODE,
//                    CaptureRequest.CONTROL_AE_MODE_ON_ALWAYS_FLASH)
        }
    } catch (e: Exception) {
        // Do nothing
    }
}

/**
 * Configure the ImageReader
 *
 * Note: format is currently overridden to PRIVATE in native for speed
 *
 * If device is a Chromebox, assume it can handle 1920x1080 and has enough memory for a large buffer
 */
fun setupImageReader(activity: MainActivity, params: CameraParams) {
    val useLargest = true

    val size = if (useLargest)
        params.cam2MaxSize
    else
        params.cam2MinSize

    if (activity.isChromebox()) {
        createPreviewImageReader(
            1920, 1080,
            ImageFormat.YUV_420_888, 20
        )
    } else {
        createPreviewImageReader(size.width, size.height,
           ImageFormat.YUV_420_888, 10)
    }
}

/** Finds the smallest focal length in the given array, useful for finding the widest angle lens */
fun smallestFocalLength(focalLengths: FloatArray): Float = focalLengths.min()
    ?: MainActivity.INVALID_FOCAL_LENGTH

/** Finds the largest aperture in the array of focal lengths */
fun largestAperture(apertures: FloatArray): Float = apertures.max()
    ?: MainActivity.NO_APERTURE

/** Finds the most "normal" focal length in the array of focal lengths */
fun focalLengthMinDeltaFromNormal(focalLengths: FloatArray): Float =
    focalLengths.minBy { Math.abs(it - MainActivity.NORMAL_FOCAL_LENGTH) }
        ?: Float.MAX_VALUE

/**
 * Return the "best" camera's CameraParams
 *
 * Best in order of preference: first external camera, first front camera, fallback to first camera
 */
fun chooseBestCamera(allCameraParams: HashMap<String, CameraParams>) : CameraParams {
    if (allCameraParams.isEmpty()) {
        return CameraParams()
    }

    var bestParams = allCameraParams.entries.iterator().next().value // Start with the first camera
    for (cameraParams in allCameraParams) {
        // Is external?
        if (cameraParams.value.isExternal) {
            bestParams = cameraParams.value
            break
        }

        // If we have not found a front camera and this one is a front-facing
        if (!bestParams.isFront && cameraParams.value.isFront) {
            bestParams = cameraParams.value
        }
    }

    return bestParams
}

/**
 * Get degrees (multiple of 90) that output camera image is rotated from "up" relative to screen display
 */
fun getCameraImageRotation(activity: MainActivity, params: CameraParams) : Int {
    var rotation = 0

    if (true) { // front-facing
        rotation = (params.orientation + getDisplayRotation(activity)) % 360;
        rotation = (360 - rotation) % 360;  // make front-facing like a mirror
    } else {  // back-facing
        rotation = (params.orientation - getDisplayRotation(activity) + 360) % 360;
    }

    return rotation
}

/**
 * Get current display rotation
 */
fun getDisplayRotation(activity: MainActivity) : Int {
    return when(activity.windowManager.defaultDisplay.rotation) {
        Surface.ROTATION_0 -> 0
        Surface.ROTATION_90 -> 90
        Surface.ROTATION_180 -> 180
        Surface.ROTATION_270 -> 270
        else -> 0
    }
}

/**
 * Native call to create the desired ImageReader
 *
 * The reader should be created in Native to avoid costly image copies across JNI
 */
external fun createPreviewImageReader(width: Int, height: Int, format: Int, maxImages: Int)
