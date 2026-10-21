package com.pumpkin.wolwidget

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.glance.ColorFilter
import androidx.glance.GlanceId
import androidx.glance.GlanceModifier
import androidx.glance.GlanceTheme
import androidx.glance.Image
import androidx.glance.ImageProvider
import androidx.glance.action.ActionParameters
import androidx.glance.action.clickable
import androidx.glance.appwidget.GlanceAppWidget
import androidx.glance.appwidget.GlanceAppWidgetReceiver
import androidx.glance.appwidget.action.ActionCallback
import androidx.glance.appwidget.action.actionRunCallback
import androidx.glance.appwidget.appWidgetBackground
import androidx.glance.appwidget.cornerRadius
import androidx.glance.appwidget.provideContent
import androidx.glance.appwidget.state.getAppWidgetState
import androidx.glance.appwidget.state.updateAppWidgetState
import androidx.glance.background
import androidx.glance.currentState
import androidx.glance.layout.Alignment
import androidx.glance.layout.Box
import androidx.glance.layout.Column
import androidx.glance.layout.Spacer
import androidx.glance.layout.fillMaxSize
import androidx.glance.layout.height
import androidx.glance.layout.padding
import androidx.glance.layout.size
import androidx.glance.state.PreferencesGlanceStateDefinition
import androidx.glance.text.FontWeight
import androidx.glance.text.Text
import androidx.glance.text.TextAlign
import androidx.glance.text.TextStyle
import kotlinx.coroutines.delay

enum class WakeStatus(val label: String) {
    IDLE("Wake PC"),
    SENDING("Sending…"),
    SENT("Sent ✓"),
    FAILED("No connection"),
}

private val statusKey = stringPreferencesKey("status")

class WakeWidget : GlanceAppWidget() {
    override suspend fun provideGlance(context: Context, id: GlanceId) {
        provideContent {
            GlanceTheme {
                Content()
            }
        }
    }

    @Composable
    private fun Content() {
        val status = currentState<Preferences>()[statusKey]
            ?.let(WakeStatus::valueOf)
            ?: WakeStatus.IDLE

        val failed = status == WakeStatus.FAILED
        val background = if (failed) {
            GlanceTheme.colors.errorContainer
        } else {
            GlanceTheme.colors.primaryContainer
        }
        val foreground = if (failed) {
            GlanceTheme.colors.onErrorContainer
        } else {
            GlanceTheme.colors.onPrimaryContainer
        }

        Box(
            modifier = GlanceModifier
                .fillMaxSize()
                .appWidgetBackground()
                .cornerRadius(24.dp)
                .background(background)
                .clickable(actionRunCallback<WakeAction>())
                .padding(8.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Image(
                    provider = ImageProvider(R.drawable.ic_power),
                    contentDescription = null,
                    colorFilter = ColorFilter.tint(foreground),
                    modifier = GlanceModifier.size(28.dp),
                )
                Spacer(GlanceModifier.height(4.dp))
                Text(
                    text = status.label,
                    style = TextStyle(
                        color = foreground,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Medium,
                        textAlign = TextAlign.Center,
                    ),
                    maxLines = 1,
                )
            }
        }
    }
}

class WakeAction : ActionCallback {
    override suspend fun onAction(
        context: Context,
        glanceId: GlanceId,
        parameters: ActionParameters,
    ) {
        val state = getAppWidgetState(context, PreferencesGlanceStateDefinition, glanceId)
        if (state[statusKey] == WakeStatus.SENDING.name) {
            return
        }

        setStatus(context, glanceId, WakeStatus.SENDING)
        val sent = WakeSender.send()
        setStatus(context, glanceId, if (sent) WakeStatus.SENT else WakeStatus.FAILED)

        delay(4_000)
        setStatus(context, glanceId, WakeStatus.IDLE)
    }

    private suspend fun setStatus(context: Context, glanceId: GlanceId, status: WakeStatus) {
        updateAppWidgetState(context, glanceId) { it[statusKey] = status.name }
        WakeWidget().update(context, glanceId)
    }
}

class WakeWidgetReceiver : GlanceAppWidgetReceiver() {
    override val glanceAppWidget: GlanceAppWidget = WakeWidget()
}
