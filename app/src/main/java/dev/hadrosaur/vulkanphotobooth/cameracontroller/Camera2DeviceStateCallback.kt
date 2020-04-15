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

import android.hardware.camera2.CameraDevice
import androidx.annotation.NonNull
import dev.hadrosaur.vulkanphotobooth.CameraParams
import dev.hadrosaur.vulkanphotobooth.MainActivity
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.logd

/**
 * Callbacks that track the state of the camera device using the Camera 2 API.
 */
class Camera2DeviceStateCallback(
    internal var params: CameraParams,
    internal var activity: MainActivity
) : CameraDevice.StateCallback() {

    /**
     * Camera device has opened successfully
     */
    override fun onOpened(cameraDevice: CameraDevice) {
        params.isOpen = true
        params.device = cameraDevice
        params.cameraOpenRequested = false

        camera2StartPreview(activity, params)
    }

    /**
     * Camera device has been closed.
     */
    override fun onClosed(camera: CameraDevice?) {

        params.isOpen = false
        params.isClosing = false;
        params.isPreviewing = false;
        params.cameraOpenRequested = false

        if (camera != null)
            super.onClosed(camera)
    }

    /**
     * Camera has been disconnected. Whatever was happening, it won't work now.
     */
    override fun onDisconnected(@NonNull cameraDevice: CameraDevice) {
        if (!params.isOpen) {
            return
        }

        camera2CloseCamera(params)
    }

    /**
     * Camera device has thrown an error. Try to recover or fail gracefully.
     */
    override fun onError(@NonNull cameraDevice: CameraDevice, error: Int) {
        logd("In CameraStateCallback onError: " + cameraDevice.id + " error: " + error)

        if (!params.isOpen) {
            return
        }

        when (error) {
            ERROR_MAX_CAMERAS_IN_USE -> {
                // Try to close an open camera and re-open this one
                logd("In CameraStateCallback too many cameras open, closing one...")
                camera2CloseCamera(params)
                camera2OpenCamera(activity, params)
            }

            ERROR_CAMERA_DEVICE -> {
                logd("Fatal camera error, close and try to re-initialize...")
                camera2CloseCamera(params)
                camera2OpenCamera(activity, params)
            }

            ERROR_CAMERA_IN_USE -> {
                logd("This camera is already open... doing nothing")
            }

            else -> {
                camera2CloseCamera(params)
            }
        }
    }
}