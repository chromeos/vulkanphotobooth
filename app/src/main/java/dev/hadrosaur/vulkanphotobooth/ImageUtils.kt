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

import android.content.Intent
import android.hardware.camera2.CameraCharacteristics
import android.net.Uri
import android.os.Environment
import android.util.SparseIntArray
import android.view.Surface
import android.widget.Toast
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.PHOTOS_DIR
import dev.hadrosaur.vulkanphotobooth.MainActivity.Companion.logd
import java.io.File
import java.text.SimpleDateFormat
import java.util.*

/**
 * Various image and ImageReader utilities
 */

/**
 * Generate a timestamp to append to saved filenames.
 */
fun generateTimestamp(): String {
    val sdf = SimpleDateFormat("yyyy_MM_dd_HH_mm_ss_SSS", Locale.US)
    return sdf.format(Date())
}

/**
 * Ensure photos directory exists and return path
 */
fun checkPhotosDirectory() : String {
    val photosDir = File(
        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM),
        PHOTOS_DIR)

    if (!photosDir.exists()) {
        val createSuccess = photosDir.mkdir()
        if (!createSuccess) {
            logd("Photo storage directory DCIM/" + PHOTOS_DIR + " creation failed!!")
        } else {
            logd("Photo storage directory DCIM/" + PHOTOS_DIR + " did not exist. Created.")
        }
    }

    return photosDir.path
}

/**
 * Generate a file path for a GIF file made up of VulkanPhotoBooth + timestamp + .gif
 */
fun generateGifFilepath() : String {
    // Create photos directory if needed
    checkPhotosDirectory()

    val gifFilepath = File(
        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM),
        File.separatorChar + PHOTOS_DIR + File.separatorChar +
                "VulkanPhotoBooth" + generateTimestamp() + ".gif")

    return gifFilepath.path
}


/**
 * Delete all the photos in the default PHOTOS_DIR
 */
fun deleteAllPhotos(activity: MainActivity) {
    val photosDir = File(
        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM),
        PHOTOS_DIR)

    if (photosDir.exists()) {

        for (photo in photosDir.listFiles())
            photo.delete()

        // Files are deleted, let media scanner know
        val scannerIntent = Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE)
        scannerIntent.data = Uri.fromFile(photosDir)
        activity.sendBroadcast(scannerIntent)

        activity.runOnUiThread {
            Toast.makeText(activity, "All photos deleted", Toast.LENGTH_SHORT).show()
        }
        logd("All photos in storage directory DCIM/" + PHOTOS_DIR + " deleted.")
    }
}

/**
 * Calculate rotation needed to save jpg file in the correct orientation
 */
fun getOrientation(params: CameraParams, rotation: Int): Int {
    val orientations = SparseIntArray()
    orientations.append(Surface.ROTATION_0, 90)
    orientations.append(Surface.ROTATION_90, 0)
    orientations.append(Surface.ROTATION_180, 270)
    orientations.append(Surface.ROTATION_270, 180)

    logd("Orientation: sensor: " +
            params.characteristics?.get(CameraCharacteristics.SENSOR_ORIENTATION) +
            " and current rotation: " + orientations.get(rotation))
    val sensorRotation: Int =
        params.characteristics?.get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 0
    return (orientations.get(rotation) + sensorRotation + 270) % 360
}
