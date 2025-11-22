package com.hxesp32.fpvgs.ui

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.IBinder
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.hxesp32.fpvgs.data.model.ConnectionState
import com.hxesp32.fpvgs.data.model.RecordingState
import com.hxesp32.fpvgs.osd.OsdOverlay
import com.hxesp32.fpvgs.service.RecordingService
import com.hxesp32.fpvgs.service.UsbCommunicationService
import com.hxesp32.fpvgs.service.VideoDecoderService
import com.hxesp32.fpvgs.ui.theme.HXESP32FPVGSTheme
import com.hxesp32.fpvgs.viewmodel.MainViewModel
import timber.log.Timber

/**
 * Main Activity - Entry point for the application
 * Coordinates USB communication, video decoding, and UI
 */
class MainActivity : ComponentActivity() {

    private val viewModel: MainViewModel by viewModels()

    private var usbService: UsbCommunicationService? = null
    private var videoService: VideoDecoderService? = null
    private var recordingService: RecordingService? = null

    private var surfaceView: SurfaceView? = null

    private val usbServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as UsbCommunicationService.LocalBinder
            usbService = binder.getService()

            // Set up data callback
            usbService?.setDataCallback { data ->
                videoService?.processVideoData(data)
            }

            // Observe connection state
            lifecycleScope.launchWhenStarted {
                usbService?.connectionState?.collect { state ->
                    viewModel.updateConnectionState(state)
                }
            }

            Timber.d("USB service connected")
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            usbService = null
            Timber.d("USB service disconnected")
        }
    }

    private val videoServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as VideoDecoderService.LocalBinder
            videoService = binder.getService()

            // Observe video state
            lifecycleScope.launchWhenStarted {
                videoService?.videoState?.collect { state ->
                    viewModel.updateVideoState(state)
                }
            }

            // Observe statistics
            lifecycleScope.launchWhenStarted {
                videoService?.statistics?.collect { stats ->
                    viewModel.updateStatistics(stats)
                }
            }

            Timber.d("Video service connected")
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            videoService = null
            Timber.d("Video service disconnected")
        }
    }

    private val recordingServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as RecordingService.LocalBinder
            recordingService = binder.getService()

            // Observe recording state
            lifecycleScope.launchWhenStarted {
                recordingService?.recordingState?.collect { state ->
                    viewModel.updateRecordingState(state)
                }
            }

            Timber.d("Recording service connected")
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            recordingService = null
            Timber.d("Recording service disconnected")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep screen on
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Start and bind services
        startAndBindServices()

        setContent {
            HXESP32FPVGSTheme {
                MainScreen(
                    viewModel = viewModel,
                    onConnect = { connectToDevice() },
                    onDisconnect = { disconnectFromDevice() },
                    onStartRecording = { startRecording() },
                    onStopRecording = { stopRecording() },
                    onSurfaceReady = { surface ->
                        // Initialize decoder when surface is ready
                        videoService?.initializeDecoder(
                            width = 640,
                            height = 480,
                            surface = surface
                        )
                    }
                )
            }
        }
    }

    private fun startAndBindServices() {
        // USB Communication Service
        Intent(this, UsbCommunicationService::class.java).also { intent ->
            startForegroundService(intent)
            bindService(intent, usbServiceConnection, Context.BIND_AUTO_CREATE)
        }

        // Video Decoder Service
        Intent(this, VideoDecoderService::class.java).also { intent ->
            startForegroundService(intent)
            bindService(intent, videoServiceConnection, Context.BIND_AUTO_CREATE)
        }

        // Recording Service
        Intent(this, RecordingService::class.java).also { intent ->
            startForegroundService(intent)
            bindService(intent, recordingServiceConnection, Context.BIND_AUTO_CREATE)
        }
    }

    private fun connectToDevice() {
        usbService?.connect()
    }

    private fun disconnectFromDevice() {
        usbService?.disconnect()
    }

    private fun startRecording() {
        recordingService?.startRecording(
            width = 640,
            height = 480,
            bitrate = 5_000_000,
            frameRate = 30
        )
    }

    private fun stopRecording() {
        recordingService?.stopRecording()
    }

    override fun onDestroy() {
        super.onDestroy()
        unbindService(usbServiceConnection)
        unbindService(videoServiceConnection)
        unbindService(recordingServiceConnection)
    }
}

@Composable
fun MainScreen(
    viewModel: MainViewModel,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
    onStartRecording: () -> Unit,
    onStopRecording: () -> Unit,
    onSurfaceReady: (android.view.Surface) -> Unit
) {
    val connectionState by viewModel.connectionState.collectAsStateWithLifecycle()
    val videoState by viewModel.videoState.collectAsStateWithLifecycle()
    val recordingState by viewModel.recordingState.collectAsStateWithLifecycle()
    val statistics by viewModel.statistics.collectAsStateWithLifecycle()
    val osdData by viewModel.osdData.collectAsStateWithLifecycle()
    val osdSettings by viewModel.osdSettings.collectAsStateWithLifecycle()

    Box(modifier = Modifier.fillMaxSize()) {
        // Video surface
        AndroidView(
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            onSurfaceReady(holder.surface)
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                        }
                    })
                }
            },
            modifier = Modifier.fillMaxSize()
        )

        // OSD Overlay
        OsdOverlay(
            statistics = statistics,
            osdData = osdData,
            osdSettings = osdSettings,
            recordingState = recordingState
        )

        // Control buttons (bottom bar)
        ControlButtons(
            connectionState = connectionState,
            recordingState = recordingState,
            onConnect = onConnect,
            onDisconnect = onDisconnect,
            onStartRecording = onStartRecording,
            onStopRecording = onStopRecording
        )
    }
}

@Composable
fun ControlButtons(
    connectionState: ConnectionState,
    recordingState: RecordingState,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
    onStartRecording: () -> Unit,
    onStopRecording: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp),
        horizontalArrangement = Arrangement.SpaceEvenly
    ) {
        // Connect/Disconnect button
        Button(
            onClick = {
                if (connectionState is ConnectionState.Connected) {
                    onDisconnect()
                } else {
                    onConnect()
                }
            }
        ) {
            Text(
                if (connectionState is ConnectionState.Connected) "Disconnect" else "Connect"
            )
        }

        // Recording button
        Button(
            onClick = {
                if (recordingState is RecordingState.Recording) {
                    onStopRecording()
                } else {
                    onStartRecording()
                }
            },
            enabled = connectionState is ConnectionState.Connected
        ) {
            Text(
                if (recordingState is RecordingState.Recording) "Stop Recording" else "Start Recording"
            )
        }
    }
}
