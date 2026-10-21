package com.pumpkin.wolwidget

import java.net.HttpURLConnection
import java.net.URL
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Publishes a signed "wake" message to the ntfy.sh topic the ESP listens to.
 * The message is "wake <unix time> <hex HMAC-SHA256 of "wake:<unix time>">",
 * so a stranger who learns the topic still can't wake the PC or replay old
 * messages.
 */
object WakeSender {
    suspend fun send(): Boolean = withContext(Dispatchers.IO) {
        val time = System.currentTimeMillis() / 1000
        val body = "wake $time ${sign("wake:$time")}"

        runCatching {
            val url = URL("https://ntfy.sh/${BuildConfig.NTFY_TOPIC}")
            val connection = url.openConnection() as HttpURLConnection

            try {
                connection.requestMethod = "POST"
                connection.connectTimeout = 10_000
                connection.readTimeout = 10_000
                connection.doOutput = true
                // Don't forward the message to ntfy's Firebase push.
                connection.setRequestProperty("Firebase", "no")
                connection.outputStream.use { it.write(body.toByteArray()) }

                connection.responseCode in 200..299
            } finally {
                connection.disconnect()
            }
        }.getOrDefault(false)
    }

    private fun sign(data: String): String {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(BuildConfig.WIDGET_KEY.toByteArray(), "HmacSHA256"))

        return mac.doFinal(data.toByteArray()).joinToString("") { "%02x".format(it) }
    }
}
