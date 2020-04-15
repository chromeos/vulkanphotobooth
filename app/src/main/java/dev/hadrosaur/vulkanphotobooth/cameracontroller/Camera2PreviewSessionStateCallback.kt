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

import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CaptureRequest
import androidx.annotation.NonNull
import dev.hadrosaur.vulkanphotobooth.CameraParams
import dev.hadrosaur.vulkanphotobooth.FocusMode
import dev.hadrosaur.vulkanphotobooth.MainActivity
import dev.hadrosaur.vulkanphotobooth.setAutoFlash

/**
 * Callbacks that track the state of a preview capture session.
 */
class Camera2PreviewSessionStateCallback(
    internal val activity: MainActivity,
    internal val params: CameraParams
) : CameraCaptureSession.StateCallback() {

    // Preview session is open and frames are coming through.
    override fun onActive(session: CameraCaptureSession?) {
        if (!params.isOpen || params.state != CameraState.PREVIEW_RUNNING) {
            return
        }

        params.previewStartRequested = false
        params.isPreviewing = true

        // initializeStillCapture(activity, params, testConfig)

        if (session != null)
            super.onActive(session)
    }

    /**
     * Preview session has been configured, set up preview parameters and request that the preview
     * capture begin.
     */
    override fun onConfigured(@NonNull cameraCaptureSession: CameraCaptureSession) {
        if (!params.isOpen) {
            return
        }

        try {
            when (params.focusMode) {
                FocusMode.AUTO -> {
                    params.captureRequestBuilder?.set(
                        CaptureRequest.CONTROL_AF_MODE,
                        CaptureRequest.CONTROL_AF_MODE_AUTO)
                }
                FocusMode.CONTINUOUS -> {
                    params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_MODE,
                        CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                }
                FocusMode.FIXED -> {
                    params.captureRequestBuilder?.set(CaptureRequest.CONTROL_AF_MODE,
                        CaptureRequest.CONTROL_AF_MODE_AUTO)
                }
            }

            // Enable flash automatically when necessary.
            setAutoFlash(params, params.captureRequestBuilder)

            params.camera2CaptureSession = cameraCaptureSession
            params.state = CameraState.PREVIEW_RUNNING

            // Request that the camera preview begins
            cameraCaptureSession.setRepeatingRequest(params.captureRequestBuilder?.build()!!,
                params.camera2CaptureSessionCallback, params.backgroundHandler)
        } catch (e: CameraAccessException) {
            MainActivity.logd("Create Capture Session error: " + params.id)
            e.printStackTrace()
        } catch (e: IllegalStateException) {
            MainActivity.logd("camera2StartPreview onConfigured, IllegalStateException," +
                " aborting: " + e)
        }
    }

    /**
     * Configuration of the preview stream failed, try again.
     */
    override fun onConfigureFailed(@NonNull cameraCaptureSession: CameraCaptureSession) {
        if (!params.isOpen) {
            return
        }

        MainActivity.logd("Camera preview initialization failed. Trying again")
        camera2StartPreview(activity, params)
    }

    /**
     * Preview session has been closed. Proceed to close camera if needed.
     */
    override fun onClosed(session: CameraCaptureSession) {
        params.isPreviewing = false
        params.previewStartRequested = false
        params.previewStopRequested = false

        if (!params.isOpen || params.isClosing) {
            return
        }

        disconnectPreviewFromOutput()

        if (params.cameraCloseRequested) {
            params.isClosing = true;
            params.device?.close()
            params.cameraCloseRequested = false
        }
        super.onClosed(session)
    }
}