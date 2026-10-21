package com.pumpkin.wolwidget

import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            SystemTheme {
                WakeScreen()
            }
        }
    }
}

/** Material You colors from the wallpaper on Android 12+, plain Material 3 before that. */
@Composable
private fun SystemTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    val context = LocalContext.current

    val colors = when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ->
            if (dark) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        dark -> darkColorScheme()
        else -> lightColorScheme()
    }

    MaterialTheme(colorScheme = colors, content = content)
}

@Composable
private fun WakeScreen() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val widgetManager = remember { AppWidgetManager.getInstance(context) }

    var sending by remember { mutableStateOf(false) }
    var result by remember { mutableStateOf<String?>(null) }

    Surface(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .systemBarsPadding()
                .padding(24.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_power),
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(64.dp),
            )
            Spacer(Modifier.height(16.dp))
            Text("Wake PC", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.height(8.dp))
            Text(
                "Add the widget to your home screen and turn on the PC with one tap, from anywhere.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = TextAlign.Center,
            )
            Spacer(Modifier.height(32.dp))

            Button(
                onClick = {
                    sending = true
                    scope.launch {
                        result = if (WakeSender.send()) {
                            "⚡ Sent! The PC should be ready in about a minute."
                        } else {
                            "❌ Couldn't reach the server. Check your internet."
                        }
                        sending = false
                    }
                },
                enabled = !sending,
                modifier = Modifier.fillMaxWidth().height(56.dp),
            ) {
                Text(if (sending) "Sending…" else "Wake PC now")
            }

            if (widgetManager.isRequestPinAppWidgetSupported) {
                Spacer(Modifier.height(12.dp))
                FilledTonalButton(
                    onClick = {
                        widgetManager.requestPinAppWidget(
                            ComponentName(context, WakeWidgetReceiver::class.java),
                            null,
                            null,
                        )
                    },
                    modifier = Modifier.fillMaxWidth().height(56.dp),
                ) {
                    Text("Add widget to home screen")
                }
            }

            result?.let {
                Spacer(Modifier.height(24.dp))
                Text(it, textAlign = TextAlign.Center)
            }
        }
    }
}
