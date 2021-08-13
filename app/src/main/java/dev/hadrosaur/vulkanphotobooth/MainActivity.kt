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

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Color
import android.hardware.display.DisplayManager
import android.media.midi.*
import android.net.Uri
import android.os.*
import androidx.appcompat.app.AppCompatActivity
import android.util.Log
import android.view.SurfaceHolder
import android.view.View.*
import android.widget.ImageView
import android.widget.SeekBar
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import androidx.swiperefreshlayout.widget.CircularProgressDrawable
import kotlinx.android.synthetic.main.activity_main.*
import dev.hadrosaur.vulkanphotobooth.cameracontroller.camera2CloseCamera
import dev.hadrosaur.vulkanphotobooth.cameracontroller.camera2OpenCamera
import dev.hadrosaur.vulkanphotobooth.cameracontroller.camera2StartPreview
import dev.hadrosaur.vulkanphotobooth.cameracontroller.camera2StopPreview
import kotlinx.coroutines.launch
import java.io.File
import java.lang.Exception
import java.lang.Thread.sleep
import kotlin.concurrent.thread
import android.content.pm.ConfigurationInfo

import android.app.ActivityManager




// Arbitrary ids to keep track of permission requests
private const val REQUEST_CAMERA_PERMISSION = 1
private const val REQUEST_FILE_WRITE_PERMISSION = 2

// TODO: Make a more comprehensive MIDI handling class

// MIDI max and min values
private const val MIDI_ID_SLIDER_MIN = 0x00;
private const val MIDI_ID_SLIDER_MAX = 0x7F;

// MIDI constants for custom MIDI controllers
private const val MIDI_CONTROLLER_GROUP = 0xB2;
private const val MIDI_ID_SLIDER_1 = 0x00;
private const val MIDI_ID_SLIDER_2 = 0x01;
private const val MIDI_ID_SLIDER_3 = 0x02;
private const val MIDI_ID_SLIDER_4 = 0x03;
private const val MIDI_ID_SLIDER_5 = 0x04;
private const val MIDI_ID_SLIDER_6 = 0x05;
private const val MIDI_ID_BUTTON = 0x33;
private const val MIDI_ID_BUTTON_ON_VALUE = 0x41;
private const val MIDI_ID_BUTTON_OFF_VALUE = 0x00;

// MIDI constants for M-Audio Keystation Mini
private const val MIDI_CONTROLLER_GROUP_KEYSTATION = 0xB0;
private const val MIDI_ID_KEYSTATION_SLIDER = 0x07;
private const val MIDI_ID_KEYSTATION_BUTTON = 0x40;
private const val MIDI_ID_KEYSTATION_BUTTON_ON_VALUE = 0x64;
private const val MIDI_ID_KEYSTATION_BUTTON_OFF_VALUE = 0x00;

// MIDI constants for AKAI MPK Mini
// TODO: Test and remade AKAI MIDI controller ids for intuitive experience
private const val MIDI_CONTROLLER_GROUP_AKAI = 0xE0;
private const val MIDI_ID_AKAI_SLIDER_1 = 0x01;
private const val MIDI_ID_AKAI_SLIDER_2 = 0x02;
private const val MIDI_ID_AKAI_SLIDER_3 = 0x03;
private const val MIDI_ID_AKAI_SLIDER_4 = 0x04;
private const val MIDI_ID_AKAI_SLIDER_5 = 0x05;
private const val MIDI_ID_AKAI_SLIDER_6 = 0x06;
private const val MIDI_ID_AKAI_BUTTON = 0x40;
private const val MIDI_ID_AKAI_BUTTON_ON_VALUE = 0x7F;
private const val MIDI_ID_AKAI_BUTTON_OFF_VALUE = 0x00;

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {
    override fun onPause() {
        // If camera permissions are granted, stop the preview stream
        if (checkPermissions()) {
            camera2StopPreview(cameraParams)
            cameraParams.isPreviewing = false
        }
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        if (checkPermissions()) {
            onResumeWithPermission()
        }
    }

    // Continuation of "onResume" with appropriate permissions
    fun onResumeWithPermission() {
        if (!isVulkanInitialized) {
            // Initialize native engine, force 1 display for now
            val initSuccess = initializeNative(1)
            setupImageReader(this, cameraParams)

            // If Vulkan cannot initialize, show a warning dialog and do not start the cameras.
            if (!initSuccess) {
                logd("Failure initializing Vulkan")
                isVulkanInitialized = false
                noVulkanDialog().show()
                return
            } else {
                isVulkanInitialized = true
            }
        }

        if (cameraWaitingOnPermissions) {
            cameraWaitingOnPermissions = false
            camera2OpenCamera(this, cameraParams)
        }

        // Restart the preview stream
        // Note, if camera is not open or preview is already running, this will simply return
        vulkanViewModel = ViewModelProvider(this).get(VulkanViewModel::class.java)
        allCameraParams = vulkanViewModel.getAllCameraParams()
        camera2StartPreview(this, cameraParams)
        updateFilterParams()
    }

    override fun onStop() {
        // If no camera permissions, don't try to stop the camera
        if (checkPermissions()) {
            camera2CloseCamera(cameraParams)
            cleanupNative()
            isVulkanInitialized = false
        }

        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()

        // Disconnect MIDI devices
        for (deviceInfo in midiManager.devices) {
            if (deviceInfo.id == vulkanViewModel.currentMidiDevice) {
                disconnectMidiDeviceFromSliders(deviceInfo)
            }
        }
    }

    companion object {
        /** Directory to save image files under sdcard/DCIM */
        const val PHOTOS_DIR: String = "VulkanPhotoBooth"
        /** Tag to include when using the logd function */
        val LOG_TAG = "VulkanPhoto"
        /** Define "normal" focal length as 50.0mm */
        const val NORMAL_FOCAL_LENGTH: Float = 50f
        /** No aperture reference */
        const val NO_APERTURE: Float = 0f
        /** Fixed-focus lenses have a value of 0 */
        const val FIXED_FOCUS_DISTANCE: Float = 0f
        /** Constant for invalid focal length */
        val INVALID_FOCAL_LENGTH: Float = Float.MAX_VALUE
        /** Shutter semaphore to prevent multiple shutter hits */
        var shutter_engaged: Boolean = false
        /** Debounce timeout to prevent multiple button clicks, in ms */
        const val BUTTON_DEBOUNCE: Int = 250
        /** Debounce timer **/
        var lastButtonClickTime: Long = 0L
        /** Check if camera open was delayed due to permissions **/
        var cameraWaitingOnPermissions = false
        /** Did Vulkan initialize? **/
        var isVulkanInitialized = false
        /** Does this device support MIDI? **/
        var hasMidi = false
        /** MIDI Manager */
        lateinit var midiManager: MidiManager

        /** View model that contains state data for the application */
        lateinit var vulkanViewModel: VulkanViewModel

        /** Hashmap of CameraParams for all cameras on the device */
        lateinit var allCameraParams: HashMap<String, CameraParams>
        /** CameraParams for chosen camera */
        lateinit var cameraParams: CameraParams

        /** Convenience access to device information, OS build, etc. */
        lateinit var deviceInfo: DeviceInfo

        /** Handler for framerate updates from native */
        lateinit var nativeUpdateFpsHandler: Handler
        /** Handler for gif creation callback from native */
        lateinit var nativeGifSavedCallbackHandler: Handler
        /** Handler to co-ordinate spinning a new thread to handle GIF encoding */
        lateinit var nativeGifReadyToEncodeHandler: Handler
        /** Handler to handle updating gif creation progress spinner */
        lateinit var nativeUpdateGifProgressHandler: Handler

        /** Spinner for gif creation */
        lateinit var gifSpinner: CircularProgressDrawable
        var gifFilepath: String = ""

        /** Convenience wrapper for Log.d that can be toggled on/off */
        fun logd(message: String) {
            if (vulkanViewModel.getShouldOutputLog().value ?: false)
                Log.d(LOG_TAG, message)
        }

        /** Connect to native */
        init {
            System.loadLibrary("native-lib")
        }

        /** Method for native to update framerate values */
        @JvmStatic
        fun updateFramerateText(fps: Int, renderTime: Int) {
            val message = Message()
            message.arg1 = fps
            message.arg2 = renderTime
            nativeUpdateFpsHandler.sendMessage(message)
        }

        /** Static function to allow handler to update the gif progress UI */
        @JvmStatic
        fun updateGifProgressSpinner(progress: Float) {
            val message = Message()
            message.arg1 = (progress * 100).toInt()
            nativeUpdateGifProgressHandler.sendMessage(message)
        }

        /** Static function to allow handler to indicate gif is ready to encode */
        @JvmStatic
        fun gifReadyToEncode() {
            val message = Message()
            nativeGifReadyToEncodeHandler.sendMessage(message)
        }

        /** Static function to allow native to indicate the GIF has been saved correctly */
        @JvmStatic
        fun gifSavedCallback() {
            val message = Message()
            nativeGifSavedCallbackHandler.sendMessage(message)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_main)
        vulkanViewModel = ViewModelProvider(this).get(VulkanViewModel::class.java)
        allCameraParams = vulkanViewModel.getAllCameraParams()

        gifSpinner = CircularProgressDrawable(this)

        // Handler to receive fps messages and send them to the UI
        nativeUpdateFpsHandler = @SuppressLint("HandlerLeak")
        object : Handler() {
            override fun handleMessage(msg: Message) {
                if (null != msg) {
                    updateFpsText(msg.arg1.toFloat() / 10, msg.arg2)
                }
            }
        }

        // Handler to receive GIF progress messages and send them to the UI
        nativeUpdateGifProgressHandler = @SuppressLint("HandlerLeak")
        object : Handler() {
            override fun handleMessage(msg: Message) {
                if (null != msg) {
                    val progress = msg.arg1.toFloat() / 100

                    runOnUiThread {
                        if (progress < 0f) {
                            progress_gif.isIndeterminate = true
                            gifSpinner.start()
                        } else if (progress > 1f) {
                            gifSpinner.setStartEndTrim(0f, 1f)
                            progress_gif.isIndeterminate = false
                            progress_gif.setProgress(100, false)
//                            gifSpinner.progressRotation = 1f
                        } else {
                            gifSpinner.setStartEndTrim(0f, progress)
                            progress_gif.isIndeterminate = false
                            progress_gif.setProgress((progress * 100).toInt(), false)
//                            gifSpinner.progressRotation = progress
                        }
                    }
                }
            }
        }

        // Handler to indicate to Kotlin the GIF is ready to be encoded
        // This indication comes from native, Kotlin starts a new thread, and then tells native
        // to encode on this new thread. This way Kotlin handles the thread management
        nativeGifReadyToEncodeHandler = @SuppressLint("HandlerLeak")
        object : Handler() {
            override fun handleMessage(msg: Message) {
                if (null != msg) {
                    thread(start = true, name = "VulkanEncodeGif") {
                        runOnUiThread {
                            gifSpinner.start()
                        }

                        val startTime = System.currentTimeMillis()
                        encodeAndSaveGif()
                        val encodeTime = System.currentTimeMillis() - startTime
                        logd("GIF Encode completed in: " + encodeTime / 1000 + " seconds.")

                        runOnUiThread {
                            // Save complete, restore shutter button
                            button_shutter.setImageResource(R.drawable.filter_button_enabled)
                            progress_gif.visibility = GONE
                            progress_gif.isIndeterminate = false;
                            progress_gif.progress = 0
                            gifSpinner.stop()

                            // TODO: this hack assumes chromeboxes do not have on-screen controls &
                            // phones do. Phones may have MIDI controllers and chromeboxes not.
                            // Handles this properly (maintain current on-screen button state).
                            if (!isChromebox()) {
                                seek_input1.visibility = VISIBLE
                                seek_input2.visibility = VISIBLE
                                seek_input3.visibility = VISIBLE
                                seek_input4.visibility = VISIBLE
                                seek_input5.visibility = VISIBLE
                                seek_input6.visibility = VISIBLE
                                button_shutter.visibility = VISIBLE
                            }
                            lockRotation(false)
                        }

                        // File is written, let media scanner know
                        val gifFile = File(gifFilepath)
                        val gifURI = Uri.fromFile(gifFile)
                        val scannerIntent = Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE)
                        scannerIntent.data = gifURI
                        sendBroadcast(scannerIntent)

                        // Re-engage the shutter
                        shutter_engaged = false;
                    }
                }
            }
        }

        /**
         * The onCreate and onResume methods are doubled to facilitate handling permissions. If
         * permissions have not been granted for the camera, try to gracefully handle the situation.
         * If they have, continue opening the camera, and initialize native.
         */
        if (checkPermissions()) {
            onCreateWithPermission()
        }
    }

    /**
     * Continue with onCreate, knowing that Camera permissions have now been granted.
     */
    fun onCreateWithPermission() {

        // Note: Chromebox detection and multi-display code is for facilitating demo setup
        // Currently only 1 display is used and all devices are treated equally (except for GIF
        // output size, see ImageUtils.
        // TODO: Handle demo setup code in a proper class

        // Check out many displays are connected to handle 1, 2, or 3 displays
        val displayManager = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        val displays = displayManager.displays
        logd("I detect " + displays.size + " display(s) connected")

        if (isChromebox()) {
            logd("This is a Chromebox!")
        }

//        val isArc = isArc()
        val isArc = false
//        val initSuccess = if (isArc) initializeNative(3) else initializeNative(displays.size)
        val initSuccess = initializeNative(1) // force only 1 display for now

        // If Vulkan cannot initialize, show a warning dialog and do not start the cameras.
        if (!initSuccess) {
            logd("Failure initializing Vulkan")
            isVulkanInitialized = false
            noVulkanDialog().show()
            return
        } else {
            isVulkanInitialized = true
        }

        if (allCameraParams.isEmpty()) {
            initializeCameras(this)
        }
        cameraParams =
            chooseBestCamera(allCameraParams)

        vulkanViewModel.getFilterParams().rotation = getCameraImageRotation(this, cameraParams)

        cameraParams.previewSurfaceView = surface_mirror
        setupImageReader(this, cameraParams)

        // Add callbacks to the mirror surfaceview, when it is ready, open the camera
        surface_mirror.holder.addCallback(this)

        // Permissions dialog hack: dialogs can prevent the surface from being created. Once we are done with the
        // permissions dialog, set the surface visibility to true
        surface_mirror.visibility = VISIBLE

        // Check for MIDI support and set up connected MIDI devices
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            hasMidi = true;
            midiManager = getSystemService(Context.MIDI_SERVICE) as MidiManager
            initializeMidiControls(this)
        } else {
            logd("This device does not support MIDI.")
        }

        // Setup on-screen UI
        setupShutterButton()
        setupFilterButtons()
        setupFilterSliders()
        updateFilterParams()

        // Make full-screen Immersive
        window.decorView.systemUiVisibility = SYSTEM_UI_FLAG_IMMERSIVE or
            SYSTEM_UI_FLAG_FULLSCREEN or SYSTEM_UI_FLAG_HIDE_NAVIGATION

        // TODO Parameterize SeekBars

        // If this is a phone, allow the user to turn on the controls. Otherwise always hide.
//        if (!isChromebox()) {
            surface_mirror.setOnClickListener {
                if (button_shutter.visibility == GONE) {
                    seek_input1.visibility = VISIBLE
                    seek_input2.visibility = VISIBLE
                    seek_input3.visibility = VISIBLE
                    seek_input4.visibility = VISIBLE
                    seek_input5.visibility = VISIBLE
                    seek_input6.visibility = VISIBLE
                    button_shutter.visibility = VISIBLE
                } else {
                    seek_input1.visibility = GONE
                    seek_input2.visibility = GONE
                    seek_input3.visibility = GONE
                    seek_input4.visibility = GONE
                    seek_input5.visibility = GONE
                    seek_input6.visibility = GONE
                    button_shutter.visibility = GONE
                }
            }
//        }
        /*

        // If 2+ displays, enable left panel
        if (displays.size >= 2 || isArc) {
            val leftPanelIntent = Intent(this, LeftPanelActivity::class.java)
            leftPanelIntent.flags = Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT or Intent.FLAG_ACTIVITY_NEW_TASK
            val options = ActivityOptions.makeBasic()
            if (!isArc)
                options.launchDisplayId = displays[1].displayId // Second display

            if (isArc) {
                val metrics = DisplayMetrics()
                displays[0].getRealMetrics(metrics)
                val leftPanelRect = Rect(0, 0, metrics.widthPixels / 3, metrics.heightPixels)
                options.launchBounds = leftPanelRect
            }
            startActivity(leftPanelIntent, options.toBundle())
        }

        // If 3+ displays, enable right panel
        if (displays.size >= 3 || isArc) {
            val rightPanelIntent = Intent(this, RightPanelActivity::class.java)
            rightPanelIntent.flags = Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT or Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED
            val options = ActivityOptions.makeBasic()
            if (!isArc)
                options.launchDisplayId = displays[2].displayId // Second display

            if (isArc) {
                val metrics = DisplayMetrics()
                displays[0].getRealMetrics(metrics)
                val rightPanelRect = Rect(metrics.widthPixels - metrics.widthPixels / 3, 0, metrics.widthPixels, metrics.heightPixels)
                options.launchBounds = rightPanelRect
            }
            startActivity(rightPanelIntent, options.toBundle())
        }
        */
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (!cameraParams.id.equals("") && checkPermissions()) {
            camera2OpenCamera(this, cameraParams)
        } else {
            cameraWaitingOnPermissions = true
        }

        surfaceReady(true)
        setNativeDrawToDisplay(true)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        surfaceReady(false)

        // Stop the camera outputting frames to the screen
        setNativeDrawToDisplay(false)
        if (!isVulkanQueueEmpty()) {
            sleep(10)
        }

        if (!cameraParams.id.equals("") && checkPermissions()) {
            camera2StopPreview(cameraParams)
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
    }

    /**
     * Act on the result of a permissions request. If permission granted, launch the app
     */
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray) {
        when (requestCode) {
            REQUEST_CAMERA_PERMISSION -> {
                if (grantResults.size > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    if (checkPermissions()) {
                        onCreateWithPermission()
                    }
                } else {
                    // Do nothing
                }
                return
            }
            REQUEST_FILE_WRITE_PERMISSION -> {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.size > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    if (checkPermissions()) {
                        onCreateWithPermission()
                    }
                } else {
                    // Do nothing
                }
                return
            }
        }
    }

    /**
     * Check if we have been granted the needed camera and file-system permissions
     */
    fun checkPermissions(): Boolean {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
            != PackageManager.PERMISSION_GRANTED) {

            // No explanation needed; request the permission
            ActivityCompat.requestPermissions(this,
                arrayOf(Manifest.permission.CAMERA),
                REQUEST_CAMERA_PERMISSION
            )
            return false
        } else if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED) {
            // No explanation needed; request the permission
            ActivityCompat.requestPermissions(this,
                arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE),
                REQUEST_FILE_WRITE_PERMISSION
            )
            return false
        }

        return true
    }

    /** Lock orientation during gif creation so the native pipe-line doesn't get re-initialized mid-process */
    @SuppressLint("SourceLockedOrientationActivity")
    fun lockRotation(lockRotation: Boolean = true) {
        if (lockRotation) {
            val currentOrientation = resources.configuration.orientation
            if (currentOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
            } else {
                requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
            }
        } else {
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
        }
    }

    /** Start the background threads associated with the given camera device/params */
    fun startBackgroundThread(params: CameraParams) {
        if (params.backgroundThread == null) {
            params.backgroundThread = HandlerThread(LOG_TAG).apply {
                this.start()
                params.backgroundHandler = Handler(this.looper)
            }
        }
    }

    /** Stop the background threads associated with the given camera device/params */
    fun stopBackgroundThread(params: CameraParams) {
        params.backgroundThread?.quitSafely()
        try {
            params.backgroundThread?.join()
            params.backgroundThread = null
            params.backgroundHandler = null
        } catch (e: InterruptedException) {
            logd("Interrupted while shutting background thread down: " + e)
        }
    }

    /**
     * Set up the shutter button to kick off GIF creation
     */
    fun setupShutterButton() {
        button_shutter.setOnClickListener {
            if (!shutter_engaged) {
                shutter_engaged = true;

                // Setup countdown
                runOnUiThread {
                    lockRotation(true)

                    image_countdown_background.visibility = VISIBLE
                    image_countdown_text.visibility = VISIBLE
                    image_countdown_text.setImageResource(R.drawable.digit_3_blue)
                    progress_gif.visibility = VISIBLE

                    seek_input1.visibility = GONE
                    seek_input2.visibility = GONE
                    seek_input3.visibility = GONE
                    seek_input4.visibility = GONE
                    seek_input5.visibility = GONE
                    seek_input6.visibility = GONE
                    button_shutter.visibility = GONE
                }

                // Start the countdown, updating the UI
                val timer: CountDownTimer = object: CountDownTimer(3000, 1000) {
                    override fun onTick(millisUntilFinished: Long) {
                        val ms: Int = (millisUntilFinished.toInt() / 1000) * 1000 // multiple of 1000

                        runOnUiThread {
                            when (ms) {
                                2000 -> {
                                    image_countdown_text.setImageResource(R.drawable.digit_3_blue)
                                }
                                1000 -> {
                                    image_countdown_text.setImageResource(R.drawable.digit_2_red)
                                }
                                0 -> {
                                    image_countdown_text.setImageResource(R.drawable.digit_1_yellow)
                                }
                            }
                        }
                    }

                    // Countdown finished, start taking photos
                    override fun onFinish() {
                        runOnUiThread {
                            image_countdown_text.visibility = GONE
                            image_countdown_background.visibility = GONE
                        }

                        gifFilepath = generateGifFilepath()
                        thread(start = true, name = "VulkanSetupGIFEncoder") {
                            runOnUiThread {
                                gifSpinner.backgroundColor = getColor(R.color.sliderControlEnd)
                                gifSpinner.setColorSchemeColors(Color.GREEN)
                                gifSpinner.strokeWidth = 10f
                                gifSpinner.stop()
                                gifSpinner.setStartEndTrim(0f, 0f)
                                progress_gif.visibility = VISIBLE
                                progress_gif.setProgress(0)

                                button_shutter.setImageDrawable(gifSpinner)
                            }
                            createGif(gifFilepath) // Tell native to start taking photos
                        }
                    }
                }.start()
            }
        }
    }

    // Match UI buttons to desired filters
    fun setupFilterButtons() {
        setupFilterButton(button_filter_one, 0)
        setupFilterButton(button_filter_two, 1)
        setupFilterButton(button_filter_three, 2)
        setupFilterButton(button_filter_four, 3)
        setupFilterButton(button_filter_five, 4)
        setupFilterButton(button_filter_six, 5)
    }

    // Setup button listners to engaged the appropriate filter
    fun setupFilterButton(view: ImageView, index: Int) {

        view.setOnClickListener {
            // Check for rapid button clicks
            // Note: the click timer is global for all buttons
            if (SystemClock.elapsedRealtime() - lastButtonClickTime < BUTTON_DEBOUNCE) {
                return@setOnClickListener
            }

            lastButtonClickTime = SystemClock.elapsedRealtime()

            // TODO: Verify if this is necessary.
            // Tell Vulkan it can no longer write to the display, wait until the queue is empty
            // Update the UI in Kotlin, then tell Vulkan it can write to the screen again
            setNativeDrawToDisplay(false)
            vulkanViewModel.viewModelScope.launch {

                // Wait for Vulkan surface to be finished
                if (!isVulkanQueueEmpty()) {
                    sleep(10)
                }

                val use_filter = vulkanViewModel.getFilterParams().use_filter
                use_filter[index] = !use_filter[index]
                toggleFilterButtonState(view, use_filter, index)

                view.post {
                    setNativeDrawToDisplay(true)
                }
            }
        }

        // Initial state
        // TODO: Verify if this is necessary.
        // Tell Vulkan it can no longer write to the display, wait until the queue is empty
        // Update the UI in Kotlin, then tell Vulkan it can write to the screen again
        setNativeDrawToDisplay(false)
        vulkanViewModel.viewModelScope.launch {
            // Wait for Vulkan surface to be finished
            if (!isVulkanQueueEmpty()) {
                sleep(10)
            }
            toggleFilterButtonState(view, vulkanViewModel.getFilterParams().use_filter, index)

            // Post will wait for View to be drawn, and then signal to Vulkan to continue
            view.post {
                setNativeDrawToDisplay(true)
            }
        }
    }

    /**
     * Flip button state
     */
    suspend fun toggleFilterButtonState(view: ImageView, use_filter: BooleanArray, index: Int) {
        if (use_filter[index]) {
            try {
                view.setImageResource(R.drawable.filter_button_enabled)
                updateFilterParams();
            } catch (ex: Exception) {
                logd("Unable to set button enabled: " + ex.message)
            }
        } else {
            try {
                view.setImageResource(R.drawable.filter_button_disabled)
                updateFilterParams();
            } catch (ex: Exception) {
                logd("Unable to set button disabled: " + ex.message)
            }
        }
    }

    /**
     * Update the current on-screen FPS and Render text
     */
    fun updateFpsText(fps: Float, renderTime: Int) {
        text_fps.text = "fps: " + fps
        text_render.text = "render: " + renderTime + "ms"
    }

    /**
     * Setup filter sliders to also act as on/off switches
     */
    fun setupFilterSliders() {
        // TODO: parameterize this so an array with slider # -> filter # can be used
        seek_input1.setOnSeekBarChangeListener(FilterSeekBarListener(this, 0, button_filter_one))
        seek_input2.setOnSeekBarChangeListener(FilterSeekBarListener(this, 1, button_filter_two))
        seek_input3.setOnSeekBarChangeListener(FilterSeekBarListener(this, 2, button_filter_three))
        seek_input4.setOnSeekBarChangeListener(FilterSeekBarListener(this, 3, button_filter_four))
        seek_input5.setOnSeekBarChangeListener(FilterSeekBarListener(this, 4, button_filter_five))
        seek_input6.setOnSeekBarChangeListener(FilterSeekBarListener(this, 5, button_filter_six))

        // Add livedata observers
        vulkanViewModel.getFilterParamsLiveData().observe(this, androidx.lifecycle.Observer {
                updatedFilterParams ->
            runOnUiThread {
                seek_input1.setProgress(updatedFilterParams.seek_values[0], true)
                seek_input2.setProgress(updatedFilterParams.seek_values[1], true)
                seek_input3.setProgress(updatedFilterParams.seek_values[2], true)
                seek_input4.setProgress(updatedFilterParams.seek_values[3], true)
                seek_input5.setProgress(updatedFilterParams.seek_values[4], true)
                seek_input6.setProgress(updatedFilterParams.seek_values[5], true)
            }
        })

        // Restore previous values
        seek_input1.setProgress(vulkanViewModel.getFilterParams().seek_values[0], true)
        seek_input2.setProgress(vulkanViewModel.getFilterParams().seek_values[1], true)
        seek_input3.setProgress(vulkanViewModel.getFilterParams().seek_values[2], true)
        seek_input4.setProgress(vulkanViewModel.getFilterParams().seek_values[3], true)
        seek_input5.setProgress(vulkanViewModel.getFilterParams().seek_values[4], true)
        seek_input6.setProgress(vulkanViewModel.getFilterParams().seek_values[5], true)
        updateFilterParams();
    }

    /** Update sliders with current ViewModel values */
    fun updateFilterParams() {
        vulkanViewModel.getFilterParams().apply {
            updateFilterParams(rotation, seek_values, use_filter)
        }
    }

    class FilterSeekBarListener(val mainActivity: MainActivity, val seekbarNumber: Int, val button_filter: ImageView): SeekBar.OnSeekBarChangeListener {
        override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
            // Use slider to turn filter on or off
            val use_filter = vulkanViewModel.getFilterParams().use_filter
            use_filter[seekbarNumber] = (progress != 0)
            setFilterButton(button_filter, use_filter, seekbarNumber)

            vulkanViewModel.getFilterParams().seek_values[seekbarNumber] = progress
            mainActivity.updateFilterParams();
        }

        override fun onStartTrackingTouch(seekBar: SeekBar?) {
        }

        override fun onStopTrackingTouch(seekBar: SeekBar?) {
        }

        /**
         * Set filter button resource to current filter state
         */
        fun setFilterButton(view: ImageView, use_filter: BooleanArray, index: Int) {
            if (use_filter[index]) {
                view.setImageResource(R.drawable.filter_button_enabled)
            } else {
                view.setImageResource(R.drawable.filter_button_disabled)
            }
        }
    }

    // TODO: move the MIDI logic into a separate class
    // Note: currently a bit "debuggy" as this gets flushed out and solidified

    /**
     * Setup connected MIDI devices
     */
    fun initializeMidiControls(activity: MainActivity) {
        if (!hasMidi) return

        val midiDeviceInfos = midiManager.devices
        logd("MIDI: Number of connected devices: " + midiDeviceInfos.size)
        for (info: MidiDeviceInfo in midiDeviceInfos) {
            logd("A MIDI device is plugged in: " + info.id + ", output ports: " + info.outputPortCount)
            val props: Bundle = info.getProperties()
            logd("Props for " + info.id + ": manufacturer: " + props.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) +
                    ", product: " + props.getString(MidiDeviceInfo.PROPERTY_PRODUCT))

            // Use the last device with midi signals
            // TODO: This assumes Android registers external midi devices as the last device.
            // TODO: find a better way to do this.
            if (info.outputPortCount > 0) {
                connectMidiDeviceToSliders(activity, info)
                //break // Uncomment to use first MIDI device
            }
        }

        midiManager.registerDeviceCallback(object: MidiManager.DeviceCallback() {
            override fun onDeviceAdded(info: MidiDeviceInfo?) {
                if (null == info) { return }

                logd("A MIDI device has been connected: " + info.id + ", output ports: " + info.outputPortCount)
                val props: Bundle = info.getProperties()
                logd("Props for " + info.id + ": manufacturer: " + props.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) +
                        ", product: " + props.getString(MidiDeviceInfo.PROPERTY_PRODUCT))

                // If a new device is connected, and it has ports, connect it to the sliders
                if (info.outputPortCount > 0) {
                    connectMidiDeviceToSliders(activity, info)
                }
            }

            override fun onDeviceRemoved(info: MidiDeviceInfo?) {
                logd("A MIDI device has been removed: " + info?.id + ", output ports: " + info?.outputPortCount)

                if (vulkanViewModel.currentMidiDevice >= 0
                    && info != null) {
                    if (vulkanViewModel.currentMidiDevice == info.id) {
                        disconnectMidiDeviceFromSliders(info)
                    }
                }
            }
        }, Handler(mainLooper))
    }

    fun connectMidiDeviceToSliders(activity: MainActivity, info: MidiDeviceInfo) {
        logd("MIDI: In connectMidiDeviceToSliders")
        if (vulkanViewModel.currentMidiDevice >= 0
            && vulkanViewModel.currentMidiDevice != info.id) {
            disconnectMidiDeviceFromSliders(info)
        }

        // Open the device
        logd("MIDI: proceeding to open MIDI device")
        midiManager.openDevice(info, object: MidiManager.OnDeviceOpenedListener {
            override fun onDeviceOpened(device: MidiDevice?) {
                logd("MIDI: device opened.")
                vulkanViewModel.currentOpenMidiDevice = device

                if (null != device) {
                    vulkanViewModel.currentMidiDevice = info.id
                    val portIndex = getFirstMidiOutputPortIndex(info)
                    logd("MIDI: First output port index: " + portIndex)
                    if (portIndex != -1) {
                        logd("MIDI: opening output port")
                        val outputPort: MidiOutputPort = device.openOutputPort(portIndex)
                        logd("MIDI: connecting slider receiver!")
                        outputPort.onConnect(MidiSliderReceiver(activity));
                    }
                }
            }
        }, Handler(mainLooper))
    }

    fun disconnectMidiDeviceFromSliders(info: MidiDeviceInfo) {
        vulkanViewModel.currentOpenMidiDevice?.close()
        vulkanViewModel.currentOpenMidiDevice = null;
        vulkanViewModel.currentMidiDevice = -1
    }

    fun getFirstMidiOutputPortIndex(info: MidiDeviceInfo) : Int {
        info.ports.forEachIndexed { index, portInfo ->
            if (portInfo.type == MidiDeviceInfo.PortInfo.TYPE_OUTPUT) {
                return portInfo.portNumber
            }
        }
        return -1
    }

    /**
     * Dialog for if this device was unable to initialize the Vulkan engine
     */
    fun noVulkanDialog() : AlertDialog {
        val alertDialog: AlertDialog = this.let {
            val builder = AlertDialog.Builder(it)
            builder.apply {
                setNegativeButton(R.string.ok,
                    DialogInterface.OnClickListener { dialog, id ->
                        dialog.dismiss()
                    })
            }
            builder.setMessage(R.string.no_vulkan)

            // Create the AlertDialog
            builder.create()
        }
        return alertDialog
    }

    /**
     * Check if this device is a Chrome OS device
     */
    fun isArc() : Boolean {
        return packageManager.hasSystemFeature("org.chromium.arc")
    }

    /**
     * Check if this device is a Chromebox (codename: fizz)
     */
    fun isChromebox() : Boolean {
        logd("BUILD says the board is: " + Build.BOARD)
        logd("BUILD says the product is: " + Build.PRODUCT)
        return (Build.PRODUCT == "fizz")
    }

    /**
     * Handle MIDI messages
     */
    class MidiSliderReceiver(val activity: MainActivity): MidiReceiver() {
        override fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
            if (null == msg) {
                logd("NULL MIDI message received")
            } else {

//                logd("Received a MIDI message.")

                var i = offset
                var systemMessage = false
                while (i < count) {
                    val currentByte = (msg[i])
                    val currentInt: Int = (currentByte.toInt() and 0xFF) // Make sure this is just the byte

                    // If it's a system exclusive message, skip over this byte
                    if (systemMessage &&
                        !((currentInt and 0b11110000) != 0)) {
                        i++
                        continue
                    }

                    // If it's a control
                    if ((currentInt and 0b10000000) != 0){

                        if (currentInt == 0b11110000) {
                            logd("System message start, let's ignore it...")
                            systemMessage = true
                            i++
                            continue
                        }
                        if (currentInt == 0b11110111) {
                            logd("System message end.")
                            systemMessage = false
                            i++
                            continue
                        }

                        /*
                        val controller = (currentInt shr 4)
                        val channel = (currentInt and 0b00001111)
                        // Only pay attention to the controllers
                        if (controller != 0xB) continue

                        // TODO differentiate between controllers, for now only care about any controller
//                        logd("Controller: " + controller  + ", channel: " + channel)
                        */

                        if (currentInt != MIDI_CONTROLLER_GROUP
                            && currentInt != MIDI_CONTROLLER_GROUP_KEYSTATION
                            && currentInt != MIDI_CONTROLLER_GROUP_AKAI) {
//                            logd("About to disqualify controller: " + currentInt)
                            i++
                            continue
                        }

                        val controllerIdByte = (msg[i+1])
                        val controllerIdInt: Int = (controllerIdByte.toInt() and 0xFF) // Make sure this is just the byte

                        val levelByte = (msg[i+2])
                        var levelInt: Int = (levelByte.toInt() and 0xFF) // Make sure this is just the byte
//                        logd("MIDI Controller ID: " + controllerIdInt + ", new level: " + levelInt)

                        // For sliders convert 0-127 -> 0-100
                        if (controllerIdInt != MIDI_ID_BUTTON
                            && controllerIdInt != MIDI_ID_KEYSTATION_BUTTON) {
                            levelInt = (levelInt * 100) / MIDI_ID_SLIDER_MAX
                        }

                        // AKAI Controller
                        if (currentInt == MIDI_CONTROLLER_GROUP_AKAI
                            || currentInt == MIDI_CONTROLLER_GROUP_KEYSTATION) {
  //                          logd("AKAI Control... " + currentInt + " controller: " + controllerIdInt);
                            when (controllerIdInt) {
                                MIDI_ID_AKAI_SLIDER_1 -> {
                                    vulkanViewModel.getFilterParams().seek_values[0] = levelInt
                                }
                                MIDI_ID_AKAI_SLIDER_2 -> {
                                    vulkanViewModel.getFilterParams().seek_values[1] = levelInt
                                }
                                MIDI_ID_AKAI_SLIDER_3 -> {
                                    vulkanViewModel.getFilterParams().seek_values[2] = levelInt
                                }
                                MIDI_ID_AKAI_SLIDER_4 -> {
                                    vulkanViewModel.getFilterParams().seek_values[3] = levelInt
                                }
                                MIDI_ID_AKAI_SLIDER_5 -> {
                                    vulkanViewModel.getFilterParams().seek_values[4] = levelInt
                                }
                                MIDI_ID_AKAI_SLIDER_6 -> {
                                    vulkanViewModel.getFilterParams().seek_values[5] = levelInt
                                }
                                MIDI_ID_AKAI_BUTTON -> {
                                    if (levelInt == MIDI_ID_AKAI_BUTTON_ON_VALUE) {
                                        activity.runOnUiThread {
                                            logd("MIDI BUTTON CLICK")
//                                            activity.button_shutter.callOnClick()
                                        }
                                    }
                                }
                               else -> {
                                    logd("Midi controller: " + controllerIdInt + " ajusted to value: " + levelInt)
                                }
                            }
                        } else {
//                            logd("NON AKAI Control... " + currentInt + " (instead of " + MIDI_CONTROLLER_GROUP_KEYSTATION + "), controller: " + controllerIdInt)
                            when (controllerIdInt) {
                                MIDI_ID_SLIDER_1,
                                MIDI_ID_KEYSTATION_SLIDER -> {
                                    vulkanViewModel.getFilterParams().seek_values[0] = levelInt
                                }
                                MIDI_ID_SLIDER_2 -> {
                                    vulkanViewModel.getFilterParams().seek_values[1] = levelInt
                                }
                                MIDI_ID_SLIDER_3 -> {
                                    vulkanViewModel.getFilterParams().seek_values[2] = levelInt
                                }
                                MIDI_ID_SLIDER_4 -> {
                                    vulkanViewModel.getFilterParams().seek_values[3] = levelInt
                                }
                                MIDI_ID_SLIDER_5 -> {
                                    vulkanViewModel.getFilterParams().seek_values[4] = levelInt
                                }
                                MIDI_ID_SLIDER_6 -> {
                                    vulkanViewModel.getFilterParams().seek_values[5] = levelInt
                                }
                                MIDI_ID_BUTTON -> {
                                    if (levelInt == MIDI_ID_BUTTON_OFF_VALUE) {
                                        activity.runOnUiThread {
                                            activity.button_shutter.callOnClick()
                                        }
                                    }
                                }
                                MIDI_ID_KEYSTATION_BUTTON -> {
                                    if (levelInt == MIDI_ID_KEYSTATION_BUTTON_OFF_VALUE) {
                                        activity.runOnUiThread {
                                            activity.button_shutter.callOnClick()
                                        }
                                    }
                                }
                                else -> {
                                    logd("Midi controller: " + controllerIdInt + " ajusted to value: " + levelInt)
                                }

                            }
                        }

                        // Update value and UI slider
//                        vulkanViewModel.getFilterParamsLiveData().postValue(vulkanViewModel.getFilterParams())
                        activity.runOnUiThread {
                            vulkanViewModel.getFilterParamsLiveData().value  = vulkanViewModel.getFilterParams()
                        }


                        // A random data byte
                    } else {
//                        logd("Random Data: " + currentInt)
                    }

                    i++
                }
                //val message = ByteArray(count)
                //msg.copyInto(message, 0, offset, offset + count)
                //logd("MIDI Message received at " + timestamp + ": " + message)
            }
        }
    }

    /**
     * Native functions
     */
    /** Set up the native pipeline with the given number of displays (1-3) */
    external fun initializeNative(numDisplays: Int): Boolean
    /** Indicate that this Activity's display (center) is ready */
    external fun surfaceReady(isReady: Boolean)
    /** Clean-up memory and native pipeline */
    external fun cleanupNative()

    /**
     * Tell Vulkan it can write to the surface. This is used to make sure the surface exists and can
     * be drawn to and that Kotlin does not need to make UI updates.
     */
    external fun setNativeDrawToDisplay(drawToDisplay: Boolean)
    /** Asks native if the Vulkan pipeline has emptied its queue and won't be drawing to the screen */
    external fun isVulkanQueueEmpty() : Boolean
    /** Passes updated filter values from the UI down to native */
    external fun updateFilterParams(rotation: Int, seek_values: IntArray, use_filter: BooleanArray)
    /** Tells native to start capturing photos for GIF creation */
    external fun createGif(filepath: String) : Boolean
    /** Tells native to start encoding and saving gif - should be run on a separate thread */
    external fun encodeAndSaveGif()
}