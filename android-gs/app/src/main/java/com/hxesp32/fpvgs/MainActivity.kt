package com.hxesp32.fpvgs

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.hxesp32.fpvgs.ui.MainScreen
import com.hxesp32.fpvgs.ui.SettingsScreen
import com.hxesp32.fpvgs.ui.StatisticsScreen
import com.hxesp32.fpvgs.ui.theme.HxEsp32FpvTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            HxEsp32FpvTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    FpvApp()
                }
            }
        }
    }
}

@Composable
fun FpvApp() {
    val navController = rememberNavController()
    val viewModel: FpvViewModel = viewModel()

    val telemetryState by viewModel.telemetryState.collectAsState()
    val osdConfig by viewModel.osdConfig.collectAsState()
    val isRecording by viewModel.isRecording.collectAsState()

    NavHost(
        navController = navController,
        startDestination = "main"
    ) {
        composable("main") {
            MainScreen(
                telemetryState = telemetryState,
                osdConfig = osdConfig,
                isRecording = isRecording,
                onRecordingToggle = { viewModel.toggleRecording() },
                onSettingsClick = { navController.navigate("settings") },
                onStatsClick = { navController.navigate("statistics") }
            )
        }

        composable("settings") {
            SettingsScreen(
                osdConfig = osdConfig,
                onOsdConfigChange = { viewModel.updateOsdConfig(it) },
                onBackClick = { navController.popBackStack() }
            )
        }

        composable("statistics") {
            StatisticsScreen(
                telemetryState = telemetryState,
                onBackClick = { navController.popBackStack() }
            )
        }
    }
}
