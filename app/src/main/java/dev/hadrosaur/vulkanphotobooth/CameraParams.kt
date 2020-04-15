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

import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CaptureRequest
import android.os.Handler
import android.os.HandlerThread
import android.util.Size
import android.view.Surface
import android.view.SurfaceView
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.FIXED_FOCUS_DISTANCE
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.INVALID_FOCAL_LENGTH
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.NO_APERTURE
import dev.hadrosaur.vulkanphotobooth.cameracontroller.Camera2CaptureSessionCallback
import dev.hadrosaur.vulkanphotobooth.cameracontroller.Camera2DeviceStateCallback
import dev.hadrosaur.vulkanphotobooth.cameracontroller.CameraState

/**
 * CameraParams contains a list of device characteristics for a given camera device.
 *
 * These are populated on app startup using initializeCameras in CameraUtils to prevent multiple
 * calls to the CameraManager.
 *
 * In addition, some convenience variables for camera API callbacks, UI surfaces, and ImageReaders
 * are included. The calling Activity is responsible to make sure these
 * convenience variables are coordinated with the active camera device.
 */
class CameraParams {
    // Intrinsic device characteristics
    internal var id: String = ""
    internal var device: CameraDevice? = null
    internal var isFront: Boolean = false
    internal var isExternal: Boolean = false
    internal var hasFlash: Boolean = false
    internal var hasMulti: Boolean = false
    internal var hasManualControl: Boolean = false
    internal var physicalCameras: Set<String> = HashSet<String>()
    internal var focalLengths: FloatArray = FloatArray(0)
    internal var apertures: FloatArray = FloatArray(0)
    internal var smallestFocalLength: Float = INVALID_FOCAL_LENGTH
    internal var minDeltaFromNormal: Float = INVALID_FOCAL_LENGTH
    internal var minFocusDistance: Float = FIXED_FOCUS_DISTANCE
    internal var largestAperture: Float = NO_APERTURE
    internal var hasAF: Boolean = false
    internal var megapixels: Int = 0
    internal var isLegacy: Boolean = false
    internal var characteristics: CameraCharacteristics? = null
    internal var orientation: Int = 0

    internal var cam2MinSize: Size = Size(0, 0)
    internal var cam2MaxSize: Size = Size(0, 0)

    // Current state
    internal var state = CameraState.UNINITIALIZED
    internal var isOpen: Boolean = false
    internal var isPreviewing: Boolean = false
    internal var cameraOpenRequested: Boolean = false
    internal var previewStartRequested: Boolean = false
    internal var cameraCloseRequested: Boolean = false
    internal var previewStopRequested: Boolean = false
    internal var isClosing: Boolean = false

    // Thread to use for this device
    internal var backgroundThread: HandlerThread? = null
    internal var backgroundHandler: Handler? = null

    // Convenience UI references
    internal var previewImageReaderSurface: Surface? = null
    internal var previewSurfaceView: SurfaceView? = null

    // Camera 2 API callback references
    internal var captureRequestBuilder: CaptureRequest.Builder? = null

    internal var camera2CaptureSession: CameraCaptureSession? = null
    internal var camera2CaptureSessionCallback: Camera2CaptureSessionCallback? = null
    internal var camera2DeviceStateCallback: Camera2DeviceStateCallback? = null

    internal var focusMode: FocusMode = FocusMode.AUTO
    internal var autoFocusStuckCounter = 0
}