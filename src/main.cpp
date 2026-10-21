#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <time.h>

#include "secrets.h"

WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);
WiFiUDP udp;

// Separate connection that stays open to ntfy.sh for the Android widget.
WiFiClientSecure ntfyClient;

const uint8_t targetMac[6] = {
    0x88, 0xAE, 0xDD, 0x12, 0x33, 0x2A
};

const uint16_t wolPort = 9;

const unsigned long wifiRetryInterval = 10000UL;
const unsigned long telegramPollInterval = 2000UL;

const char* const ntfyHost = "ntfy.sh";
const unsigned long ntfyRetryInterval = 15000UL;
// ntfy sends a keepalive every 45 s, so silence this long means a dead link.
const unsigned long ntfyIdleTimeout = 120000UL;
const size_t ntfyMaxLine = 1024;
// How old a signed widget message may be, in seconds.
const long widgetMaxAge = 120;

unsigned long lastWifiAttempt = 0;
unsigned long lastTelegramPoll = 0;
unsigned long lastNtfyAttempt = 0;
unsigned long lastNtfyData = 0;

bool ntfyHeadersDone = false;
String ntfyLine;
String ntfyLastId;
long long lastWidgetTime = 0;

bool wifiWasConnected = false;
bool udpReady = false;
bool commandsRegistered = false;

const char* const wakeButton = "⚡ Wake PC";
const char* const statusButton = "📊 Status";

// Persistent keyboard under the message box.
const char* const mainKeyboard =
    "[[\"⚡ Wake PC\"],[\"📊 Status\"]]";

// Commands shown in Telegram's "Menu" button.
const char* const botCommands =
    "[{\"command\":\"wake\",\"description\":\"Turn on the PC\"},"
    "{\"command\":\"status\",\"description\":\"Check the bot\"},"
    "{\"command\":\"help\",\"description\":\"What I can do\"}]";

bool intervalPassed(
    unsigned long now,
    unsigned long previous,
    unsigned long interval
) {
    return static_cast<unsigned long>(now - previous) >= interval;
}

IPAddress getBroadcastAddress() {
    IPAddress ip = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    IPAddress broadcast;

    for (int i = 0; i < 4; i++) {
        broadcast[i] = ip[i] | static_cast<uint8_t>(~mask[i]);
    }

    return broadcast;
}

void initializeUdp() {
    udp.stop();
    delay(50);

    udpReady = udp.begin(wolPort);

    Serial.print("UDP initialization: ");
    Serial.println(udpReady ? "OK" : "FAILED");
}

void beginWifiConnection() {
    Serial.println();
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(WIFI_SSID);

    WiFi.disconnect(false, false);
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    lastWifiAttempt = millis();
}

void configureWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);

    beginWifiConnection();
}

void handleWifi() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    const unsigned long now = millis();

    if (connected && !wifiWasConnected) {
        wifiWasConnected = true;

        Serial.println();
        Serial.println("Wi-Fi connected.");

        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        Serial.print("Gateway: ");
        Serial.println(WiFi.gatewayIP());

        Serial.print("Broadcast: ");
        Serial.println(getBroadcastAddress());

        Serial.print("Signal: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");

        secureClient.stop();
        ntfyClient.stop();
        initializeUdp();

        // The widget messages are checked against real time.
        configTime(0, 0, "pool.ntp.org", "time.google.com");

        lastTelegramPoll = 0;
        lastNtfyAttempt = millis() - ntfyRetryInterval;
        return;
    }

    if (!connected && wifiWasConnected) {
        wifiWasConnected = false;
        udpReady = false;

        udp.stop();
        secureClient.stop();
        ntfyClient.stop();

        Serial.println();
        Serial.println("Wi-Fi connection lost.");
    }

    if (
        !connected &&
        intervalPassed(now, lastWifiAttempt, wifiRetryInterval)
    ) {
        Serial.println("Retrying Wi-Fi connection...");
        beginWifiConnection();
    }
}

bool sendTelegramMessage(const String& chatId, const String& text) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Telegram skipped: Wi-Fi disconnected.");
        return false;
    }

    const bool result = bot.sendMessageWithReplyKeyboard(
        chatId,
        text,
        "HTML",
        mainKeyboard,
        true
    );

    Serial.print("Telegram send: ");
    Serial.println(result ? "OK" : "FAILED");

    if (!result) {
        secureClient.stop();
    }

    return result;
}

bool sendMagicPacket() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot send WoL: Wi-Fi disconnected.");
        return false;
    }

    if (!udpReady) {
        initializeUdp();
    }

    if (!udpReady) {
        Serial.println("Cannot send WoL: UDP unavailable.");
        return false;
    }

    uint8_t packet[102];

    for (int i = 0; i < 6; i++) {
        packet[i] = 0xFF;
    }

    for (int repeat = 0; repeat < 16; repeat++) {
        for (int i = 0; i < 6; i++) {
            packet[6 + repeat * 6 + i] = targetMac[i];
        }
    }

    const IPAddress broadcast = getBroadcastAddress();
    bool sentAtLeastOnce = false;

    for (int attempt = 0; attempt < 5; attempt++) {
        const bool packetStarted =
            udp.beginPacket(broadcast, wolPort);

        if (packetStarted) {
            udp.write(packet, sizeof(packet));
        }

        const int result =
            packetStarted ? udp.endPacket() : 0;

        Serial.print("Magic packet attempt ");
        Serial.print(attempt + 1);
        Serial.print(": ");
        Serial.println(result == 1 ? "sent" : "failed");

        if (result == 1) {
            sentAtLeastOnce = true;
        }

        delay(250);
    }

    return sentAtLeastOnce;
}

String signalQuality(long rssi) {
    if (rssi >= -60) {
        return "excellent";
    }

    if (rssi >= -70) {
        return "good";
    }

    if (rssi >= -80) {
        return "fair";
    }

    return "weak";
}

String formatUptime(unsigned long ms) {
    const unsigned long minutes = ms / 60000UL;
    const unsigned long days = minutes / 1440UL;
    const unsigned long hours = (minutes / 60UL) % 24UL;

    if (days > 0) {
        return String(days) + " d " + String(hours) + " h";
    }

    if (hours > 0) {
        return String(hours) + " h " + String(minutes % 60UL) + " min";
    }

    return String(minutes) + " min";
}

void sendHelp(const String& chatId) {
    sendTelegramMessage(
        chatId,
        "👋 <b>Hi! I turn on your PC.</b>\n"
        "\n"
        "⚡ <b>Wake PC</b> — turn it on\n"
        "📊 <b>Status</b> — check that I'm online\n"
        "\n"
        "Use the buttons below or the Menu."
    );
}

void sendWake(const String& chatId) {
    const bool sent = sendMagicPacket();

    sendTelegramMessage(
        chatId,
        sent
            ? "⚡ <b>Wake-up signal sent.</b>\n"
              "The PC should be ready in about a minute."
            : "❌ <b>Couldn't send the wake-up signal.</b>\n"
              "Try again in a few seconds."
    );
}

void sendStatus(const String& chatId) {
    const long rssi = WiFi.RSSI();

    String status = "🟢 <b>Bot is online</b>\n\n";

    status += "📶 Wi-Fi: ";
    status += signalQuality(rssi);
    status += " (";
    status += String(rssi);
    status += " dBm)\n";

    status += "⏱ Up for: ";
    status += formatUptime(millis());
    status += "\n";

    status += "🌐 IP: <code>";
    status += WiFi.localIP().toString();
    status += "</code>";

    sendTelegramMessage(chatId, status);
}

bool widgetSignatureValid(const String& data, const String& signature) {
    uint8_t mac[32];

    const int result = mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        reinterpret_cast<const uint8_t*>(WIDGET_KEY),
        strlen(WIDGET_KEY),
        reinterpret_cast<const uint8_t*>(data.c_str()),
        data.length(),
        mac
    );

    if (result != 0 || signature.length() != sizeof(mac) * 2) {
        return false;
    }

    char expected[sizeof(mac) * 2 + 1];

    for (size_t i = 0; i < sizeof(mac); i++) {
        snprintf(expected + i * 2, 3, "%02x", mac[i]);
    }

    // Constant-time compare.
    uint8_t difference = 0;

    for (size_t i = 0; i < sizeof(mac) * 2; i++) {
        difference |= expected[i] ^ signature[i];
    }

    return difference == 0;
}

/*
 * Widget message: "wake <unix time> <hex HMAC-SHA256 of "wake:<unix time>">".
 * Only fresh, correctly signed and never seen before messages wake the PC.
 */
void handleWidgetMessage(const String& message) {
    const int firstSpace = message.indexOf(' ');
    const int secondSpace = message.indexOf(' ', firstSpace + 1);

    if (
        firstSpace < 0 ||
        secondSpace < 0 ||
        message.substring(0, firstSpace) != "wake"
    ) {
        Serial.println("Widget: unknown message ignored.");
        return;
    }

    const String timeText = message.substring(firstSpace + 1, secondSpace);
    const String signature = message.substring(secondSpace + 1);

    if (timeText.length() == 0 || timeText.length() > 12) {
        Serial.println("Widget: bad time ignored.");
        return;
    }

    for (size_t i = 0; i < timeText.length(); i++) {
        if (!isDigit(timeText[i])) {
            Serial.println("Widget: bad time ignored.");
            return;
        }
    }

    if (!widgetSignatureValid("wake:" + timeText, signature)) {
        Serial.println("Widget: bad signature ignored.");
        return;
    }

    const time_t now = time(nullptr);

    if (now < 1700000000) {
        Serial.println("Widget: clock not synced yet, ignored.");
        return;
    }

    const long long sentAt = atoll(timeText.c_str());

    if (llabs(static_cast<long long>(now) - sentAt) > widgetMaxAge) {
        Serial.println("Widget: message too old, ignored.");
        return;
    }

    if (sentAt <= lastWidgetTime) {
        Serial.println("Widget: repeated message ignored.");
        return;
    }

    lastWidgetTime = sentAt;

    Serial.println("Widget: wake request accepted.");

    const bool sent = sendMagicPacket();

    sendTelegramMessage(
        ALLOWED_CHAT_ID,
        sent
            ? "⚡ <b>Wake-up signal sent from the widget.</b>\n"
              "The PC should be ready in about a minute."
            : "❌ <b>The widget asked to wake the PC,</b>\n"
              "but the signal couldn't be sent."
    );
}

void handleNtfyLine(const String& line) {
    JsonDocument doc;

    if (deserializeJson(doc, line)) {
        return;
    }

    const char* event = doc["event"] | "";

    if (strcmp(event, "message") != 0) {
        return;
    }

    ntfyLastId = doc["id"] | "";
    handleWidgetMessage(doc["message"] | "");
}

void connectNtfy() {
    lastNtfyAttempt = millis();
    ntfyClient.stop();

    if (!ntfyClient.connect(ntfyHost, 443)) {
        Serial.println("ntfy: connection failed.");
        return;
    }

    /*
     * HTTP/1.0 keeps the stream free of chunked encoding: one JSON
     * object per line. After a reconnect "since" fetches anything
     * sent while the link was down.
     */
    String path = "/" WIDGET_TOPIC "/json";

    if (ntfyLastId.length() > 0) {
        path += "?since=" + ntfyLastId;
    }

    ntfyClient.print(
        "GET " + path + " HTTP/1.0\r\n"
        "Host: " + String(ntfyHost) + "\r\n"
        "User-Agent: wol-bot\r\n"
        "\r\n"
    );

    ntfyHeadersDone = false;
    ntfyLine = "";
    lastNtfyData = millis();

    Serial.println("ntfy: listening for the widget.");
}

void handleNtfy() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!ntfyClient.connected()) {
        if (intervalPassed(millis(), lastNtfyAttempt, ntfyRetryInterval)) {
            connectNtfy();
        }

        return;
    }

    while (ntfyClient.available() > 0) {
        const char c = static_cast<char>(ntfyClient.read());
        lastNtfyData = millis();

        if (c == '\r') {
            continue;
        }

        if (c != '\n') {
            if (ntfyLine.length() < ntfyMaxLine) {
                ntfyLine += c;
            }

            continue;
        }

        if (!ntfyHeadersDone) {
            if (ntfyLine.startsWith("HTTP/") && ntfyLine.indexOf(" 200") < 0) {
                Serial.print("ntfy: unexpected response: ");
                Serial.println(ntfyLine);
                ntfyClient.stop();
                return;
            }

            if (ntfyLine.length() == 0) {
                ntfyHeadersDone = true;
            }
        } else {
            handleNtfyLine(ntfyLine);
        }

        ntfyLine = "";
    }

    if (intervalPassed(millis(), lastNtfyData, ntfyIdleTimeout)) {
        Serial.println("ntfy: link went quiet, reconnecting.");
        ntfyClient.stop();
    }
}

void handleMessages(int messageCount) {
    for (int i = 0; i < messageCount; i++) {
        String chatId = bot.messages[i].chat_id;
        String text = bot.messages[i].text;

        text.trim();

        Serial.println();
        Serial.print("Message from chat ID: ");
        Serial.println(chatId);

        Serial.print("Command: ");
        Serial.println(text);

        if (chatId != ALLOWED_CHAT_ID) {
            Serial.println("Unauthorized chat ignored.");
            continue;
        }

        if (text == "/wake" || text == wakeButton) {
            sendWake(chatId);
        } else if (text == "/status" || text == statusButton) {
            sendStatus(chatId);
        } else if (text == "/start" || text == "/help") {
            sendHelp(chatId);
        } else {
            sendTelegramMessage(
                chatId,
                "🤔 I don't know that one. Use the buttons below."
            );
        }
    }
}

void checkTelegram() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!commandsRegistered) {
        commandsRegistered = bot.setMyCommands(botCommands);

        Serial.print("Telegram commands: ");
        Serial.println(commandsRegistered ? "OK" : "FAILED");
    }

    int messageCount =
        bot.getUpdates(bot.last_message_received + 1);

    if (messageCount < 0) {
        Serial.println("Telegram request failed.");
        secureClient.stop();
        return;
    }

    int batchesProcessed = 0;

    while (messageCount > 0 && batchesProcessed < 5) {
        handleMessages(messageCount);
        batchesProcessed++;

        /*
         * Этот следующий запрос подтверждает уже обработанные
         * сообщения Telegram. Старый /restart после прошивки
         * будет проигнорирован и затем удалён из очереди.
         */
        messageCount =
            bot.getUpdates(bot.last_message_received + 1);

        if (messageCount < 0) {
            Serial.println(
                "Telegram request failed while reading messages."
            );

            secureClient.stop();
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("ESP32 Wake-on-LAN controller starting...");

    secureClient.setInsecure();
    secureClient.setTimeout(10000);

    ntfyClient.setInsecure();
    ntfyClient.setTimeout(10000);

    bot.longPoll = 0;

    configureWifi();
}

void loop() {
    handleWifi();

    const unsigned long now = millis();

    if (
        WiFi.status() == WL_CONNECTED &&
        intervalPassed(
            now,
            lastTelegramPoll,
            telegramPollInterval
        )
    ) {
        checkTelegram();
        lastTelegramPoll = millis();
    }

    handleNtfy();

    delay(20);
}
