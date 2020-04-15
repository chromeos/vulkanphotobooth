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

package dev.hadrosaur.vulkanphotobooth.cameracontroller

import android.content.Context
import android.hardware.camera2.*
import android.view.Surface
import dev.hadrosaur.vulkanphotobooth.*
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.vulkanViewModel
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.logd
import java.lang.Thread.sleep
import java.util.*

/** State of the camera during an image capture - */
internal enum class CameraState {
    UNINITIALIZED,
    PREVIEW_RUNNING,
    WAITING_FOCUS_LOCK,
    WAITING_EXPOSURE_LOCK,
    IMAGE_REQUESTED
}

/**
 * Opens the camera using the Camera 2 API. The open call will complete
 * in the DeviceStateCallback asynchronously.
 */
fun camera2OpenCamera(activity: MainActivity, params: CameraParams?) {
    if (null == params)
        return

    // The camera has not finished closing, wait for it to finish before re-opening
    // Possible orientation/resize event
    while (params.isClosing) {
        sleep(20)
    }

    if (params.isPreviewing) {
        return
    }

    // If camera is already opened (after onPause/rotation), just start a preview session
    if (params.isOpen) {
        camera2StartPreview(activity, params);
        return
    }

    // Don't double up open request
    if (params.cameraOpenRequested) {
        return
    } else {
        params.cameraOpenRequested = true
    }

    val manager = activity.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    try {
        logd("About to open camera: " + params.id)

        // Might be a new session, update callbacks to make sure they match the needed config
        params.camera2DeviceStateCallback =
            Camera2DeviceStateCallback(params, activity)
        params.camera2CaptureSessionCallback =
            Camera2CaptureSessionCallback(activity, params)

        manager.openCamera(params.id, params.camera2DeviceStateCallback!!, params.backgroundHandler)
    } catch (e: CameraAccessException) {
        logd("openCamera CameraAccessException: " + params.id)
        e.printStackTrace()
    } catch (e: SecurityException) {
        logd("openCamera SecurityException: " + params.id)
        e.printStackTrace()
    }
}

/**
 * Setup the camera preview session and output surface.
 */
fun camera2StartPreview(activity: MainActivity, params: CameraParams) {
    // If camera is not open, return
    if (!params.isOpen || params.isClosing) {
        return
    }

    // If preview is already showing, no need to recreate it
    if (params.isPreviewing) {
        return
    }

    // Don't double up preview request
    if (params.previewStartRequested) {
        return
    } else {
        params.previewStartRequested = true;
    }

    try {
        val surface = params.previewSurfaceView?.holder?.surface
        if (null == surface)
            return

        params.previewImageReaderSurface = getImagePreviewSurface()

        if (null == params.previewImageReaderSurface) {
            logd("Preview imagereader surface is NULL, not starting preview")
            return
        }

        params.captureRequestBuilder =
            params.device?.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
//        params.captureRequestBuilder =
//            params.device?.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE)

        params.captureRequestBuilder?.addTarget(params.previewImageReaderSurface)

        logd("In camera2StartPreview. Added request.")
        logd("Preview SurfaceView width: " + params.previewSurfaceView?.width + ", height: " + params.previewSurfaceView?.height)

        // Now try to hook up the native rendering/filtering engine.
        if (params.previewImageReaderSurface != null) {
            if (activity.isChromebox()) {
//                connectPreviewToOutput(surface, vulkanViewModel.getFilterParams().rotation, 1920, 1080)
                connectPreviewToOutput(surface, vulkanViewModel.getFilterParams().rotation, 960, 540)
//                connectPreviewToOutput(surface, vulkanViewModel.getFilterParams().rotation, 1200, 675)
//                connectPreviewToOutput(surface, vulkanViewModel.getFilterParams().rotation, 960, 540)
            } else {
                connectPreviewToOutput(surface, vulkanViewModel.getFilterParams().rotation, 0,0)
            }
        }

        params.device?.createCaptureSession(
            Arrays.asList(params.previewImageReaderSurface),
            Camera2PreviewSessionStateCallback(activity, params), null)
    } catch (e: CameraAccessException) {
        MainActivity.logd("camera2StartPreview CameraAccessException: " + e.message)
        e.printStackTrace()
    } catch (e: IllegalStateException) {
        MainActivity.logd("camera2StartPreview IllegalStateException: " + e.message)

        e.printStackTrace()
    }
}

/**
 * Set up for a still capture.
 */
fun initializeStillCapture(activity: MainActivity, params: CameraParams) {
    logd("TakePicture: capture start.")

    if (!params.isOpen) {
        return
    }

    lockFocus(activity, params)
}

/**
 * Initiate the auto-focus routine if required.
 */
fun lockFocus(activity: MainActivity, params: CameraParams) {
    logd("In lockFocus.")
    if (!params.isOpen) {
        return
    }

    try {
        if (null != params.device) {
            setAutoFlash(params, params.captureRequestBuilder)

            // If this lens can focus, we need to start a focus search and wait for focus lock
            if (params.hasAF &&
                FocusMode.AUTO == params.focusMode) {
                logd("In lockFocus. About to request focus lock and call capture.")

                params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_MODE,
                    CaptureRequest.CONTROL_AF_MODE_AUTO)
                params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_TRIGGER,
                    CameraMetadata.CONTROL_AF_TRIGGER_CANCEL)
                params.camera2CaptureSession?.capture(params.captureRequestBuilder?.build()!!,
                    params.camera2CaptureSessionCallback, params.backgroundHandler)

                params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_MODE,
                    CaptureRequest.CONTROL_AF_MODE_AUTO)
                params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_TRIGGER,
                    CameraMetadata.CONTROL_AF_TRIGGER_START)

                params.state = CameraState.WAITING_FOCUS_LOCK

                params.autoFocusStuckCounter = 0
                params.camera2CaptureSession?.capture(params.captureRequestBuilder?.build()!!,
                    params.camera2CaptureSessionCallback, params.backgroundHandler)
            } else {
                // If no auto-focus requested, go ahead to the still capture routine
                logd("In lockFocus. Fixed focus or continuous focus, calling captureStillPicture.")
                params.state = CameraState.IMAGE_REQUESTED
                captureStillPicture(activity, params)
            }
        }
    } catch (e: CameraAccessException) {
        e.printStackTrace()
    }
}

/**
 * Request pre-capture auto-exposure (AE) metering
 */
fun runPrecaptureSequence(params: CameraParams) {
    if (!params.isOpen) {
        return
    }

    try {
        if (null != params.device) {
            setAutoFlash(params, params.captureRequestBuilder)
            params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START)

            params.state = CameraState.WAITING_EXPOSURE_LOCK
            params.camera2CaptureSession?.capture(params.captureRequestBuilder?.build()!!,
                params.camera2CaptureSessionCallback, params.backgroundHandler)
        }
    } catch (e: CameraAccessException) {
        e.printStackTrace()
    }
}

/**
 * Make a still capture request. At this point, AF and AE should be converged or unnecessary.
 */
fun captureStillPicture(activity: MainActivity, params: CameraParams) {
    if (!params.isOpen) {
        return
    }

    try {
        if (null != params.device) {
            params.captureRequestBuilder =
                params.device?.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE)

            when (params.focusMode) {
                FocusMode.CONTINUOUS -> {
                    params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_MODE,
                        CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                }
                FocusMode.AUTO -> {
                    params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_TRIGGER,
                        CameraMetadata.CONTROL_AF_TRIGGER_IDLE)
                }
                FocusMode.FIXED -> {
                }
            }

            // Disable HDR+ for Pixel devices
            // This is a hack, Pixel devices do not have Sepia mode, but this forces HDR+ off
            if (android.os.Build.MANUFACTURER.equals("Google")) {
                //    params.captureRequestBuilder?.set(CaptureRequest.CONTROL_EFFECT_MODE,
                //        CaptureRequest.CONTROL_EFFECT_MODE_SEPIA)
            }

            // Orientation
            val rotation = activity.windowManager.defaultDisplay.rotation
            val capturedImageRotation = getOrientation(params, rotation)
            params.captureRequestBuilder
                ?.set(CaptureRequest.JPEG_ORIENTATION, capturedImageRotation)

            // Flash
            setAutoFlash(params, params.captureRequestBuilder)

            val captureCallback =
                Camera2CaptureCallback(activity, params)
            params.camera2CaptureSession?.capture(params.captureRequestBuilder?.build()!!,
                captureCallback, params.backgroundHandler)
        }
    } catch (e: CameraAccessException) {
        e.printStackTrace()
    } catch (e: IllegalStateException) {
        logd("captureStillPicture IllegalStateException, aborting: " + e.message)
    }
}

/**
 * Stop preview stream and clean-up native ImageReader/processing
 */
fun camera2StopPreview(params: CameraParams) {
    if (params.isPreviewing && !params.previewStopRequested) {
        logd("camera2StopPreview")
        params.previewStopRequested = true
        params.camera2CaptureSession?.close()
    }
}

/**
 * Close preview stream and camera device.
 */
fun camera2CloseCamera(params: CameraParams?) {
    if (params == null)
        return

    logd("closePreviewAndCamera: " + params.id)
    if (params.isPreviewing && !params.previewStopRequested) {
        params.cameraCloseRequested = true
        params.previewStopRequested = true
        params.camera2CaptureSession?.close()
    } else {
        params.isClosing = true;
        params.device?.close()
    }
}

/**
 * An abort request has been received. Abandon everything
 */
fun camera2Abort(activity: MainActivity, params: CameraParams) {
    params.camera2CaptureSession?.abortCaptures()
    activity.stopBackgroundThread(params)
}

external fun getImagePreviewSurface() : Surface
external fun connectPreviewToOutput(outputSurface: Surface, rotation: Int, rendererCopyWidth: Int, rendererCopyHeight: Int): Boolean
external fun disconnectPreviewFromOutput(): Boolean