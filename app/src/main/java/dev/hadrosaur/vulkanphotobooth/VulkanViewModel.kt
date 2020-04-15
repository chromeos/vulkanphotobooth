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

import android.media.midi.MidiDevice
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

/**
 * ViewModel for keeping track of application state across resizes and configuration changes.
 * This includes:
 *  - Camera parameters
 *  - MIDI device state
 *  - Current filter parameters
 */
class VulkanViewModel : ViewModel() {
    private var allCameraParams: HashMap<String, CameraParams> = HashMap<String, CameraParams>()
    private var cameraParams = CameraParams()
    private val shouldOutputLog = MutableLiveData<Boolean>().apply { value = true }
    private val filterParams = MutableLiveData<FilterParams>().apply { value = FilterParams() }

    /** Current MIDI device id connected, -1 for none **/
    var currentMidiDevice = -1
    var currentOpenMidiDevice: MidiDevice? = null

    /** Hashmap of the CameraParams associated with all the cameras on the device */
    fun getAllCameraParams(): HashMap<String, CameraParams> {
        return allCameraParams
    }

    /** CameraParams of main camera */
    fun getCameraParams(): CameraParams {
        return cameraParams
    }

    /** Current FilterParams */
    fun getFilterParams(): FilterParams {
        return filterParams.value!!
    }

    /** Current FilterParams */
    fun getFilterParamsLiveData(): MutableLiveData<FilterParams> {
        return filterParams
    }

    /** If the user has asked to output the debugging log */
    fun getShouldOutputLog(): MutableLiveData<Boolean> {
        return shouldOutputLog
    }
}