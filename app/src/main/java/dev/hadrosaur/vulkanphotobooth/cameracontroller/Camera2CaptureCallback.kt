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

import android.hardware.camera2.*
import android.view.Surface
import dev.hadrosaur.vulkanphotobooth.CameraParams
import dev.hadrosaur.vulkanphotobooth.MainActivity

/**
 * Image capture callback for Camera 2 API. Tracks state of an image capture request.
 */
class Camera2CaptureCallback(
    internal val activity: MainActivity,
    internal val params: CameraParams
    ) : CameraCaptureSession.CaptureCallback() {

    override fun onCaptureSequenceAborted(session: CameraCaptureSession, sequenceId: Int) {
        if (session != null)
            super.onCaptureSequenceAborted(session, sequenceId)
    }

    override fun onCaptureFailed(
        session: CameraCaptureSession,
        request: CaptureRequest,
        failure: CaptureFailure
    ) {

        if (!params.isOpen) {
            return
        }

        MainActivity.logd("captureStillPicture captureCallback: Capture Failed. Failure: " +
            failure?.reason)

        // The session failed. Let's just try again (yay infinite loops)
        camera2CloseCamera(params)
        camera2OpenCamera(activity, params)

        if (session != null && request != null && failure != null)
            super.onCaptureFailed(session, request, failure)
    }

    override fun onCaptureStarted(
        session: CameraCaptureSession,
        request: CaptureRequest,
        timestamp: Long,
        frameNumber: Long
    ) {
        if (session != null && request != null)
            super.onCaptureStarted(session, request, timestamp, frameNumber)
    }

    override fun onCaptureProgressed(
        session: CameraCaptureSession,
        request: CaptureRequest,
        partialResult: CaptureResult
    ) {
        if (session != null && request != null && partialResult != null)
            super.onCaptureProgressed(session, request, partialResult)
    }

    override fun onCaptureBufferLost(
        session: CameraCaptureSession,
        request: CaptureRequest,
        target: Surface,
        frameNumber: Long
    ) {
        MainActivity.logd("captureStillPicture captureCallback: Buffer lost")

        if (session != null && request != null && target != null)
            super.onCaptureBufferLost(session, request, target, frameNumber)
    }

    override fun onCaptureCompleted(
        session: CameraCaptureSession,
        request: CaptureRequest,
        result: TotalCaptureResult
    ) {
        if (!params.isOpen) {
            return
        }
    }
}