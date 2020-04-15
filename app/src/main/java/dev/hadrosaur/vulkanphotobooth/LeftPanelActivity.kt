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

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.View.*
import kotlinx.android.synthetic.main.activity_left_panel.*

/**
 * Activity that manages the left-hand display of 3 displays
 */
class LeftPanelActivity : AppCompatActivity(), SurfaceHolder.Callback {
    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder?, format: Int, width: Int, height: Int) {
    }

    override fun surfaceDestroyed(holder: SurfaceHolder?) {
        leftSurfaceReady(false)
    }

    override fun surfaceCreated(holder: SurfaceHolder?) {
        leftSurfaceReady(true)
        if (holder?.surface != null)
            setupVulkanSurfaceLeft(holder.surface)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_left_panel)
        surface_mirror_left.holder.addCallback(this)
        window.decorView.systemUiVisibility = SYSTEM_UI_FLAG_IMMERSIVE or SYSTEM_UI_FLAG_FULLSCREEN or
                SYSTEM_UI_FLAG_HIDE_NAVIGATION
    }

    /**
     * Signal to native that this display is ready
     */
    external fun leftSurfaceReady(isReady: Boolean)

    /**
     * Configure this display in native
     */
    external fun setupVulkanSurfaceLeft(outputSurface: Surface)
}
