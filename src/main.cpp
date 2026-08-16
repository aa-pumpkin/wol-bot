#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <UniversalTelegramBot.h>

#include "secrets.h"

WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);
WiFiUDP udp;

const uint8_t targetMac[6] = {
    0x88, 0xAE, 0xDD, 0x12, 0x33, 0x2A
};

const uint16_t wolPort = 9;

const unsigned long wifiRetryInterval = 10000UL;
const unsigned long telegramPollInterval = 2000UL;

unsigned long lastWifiAttempt = 0;
unsigned long lastTelegramPoll = 0;

bool wifiWasConnected = false;
bool udpReady = false;

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
        initializeUdp();

        lastTelegramPoll = 0;
        return;
    }

    if (!connected && wifiWasConnected) {
        wifiWasConnected = false;
        udpReady = false;

        udp.stop();
        secureClient.stop();

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

    const bool result = bot.sendMessage(chatId, text, "");

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

void sendStatus(const String& chatId) {
    String status = "ESP32 is online.";

    status += "\nIP: ";
    status += WiFi.localIP().toString();

    status += "\nWi-Fi signal: ";
    status += String(WiFi.RSSI());
    status += " dBm";

    status += "\nUptime: ";
    status += String(millis() / 3600000UL);
    status += " h";

    status += "\nBroadcast: ";
    status += getBroadcastAddress().toString();

    sendTelegramMessage(chatId, status);
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

        if (text == "/wake") {
            const bool sent = sendMagicPacket();

            sendTelegramMessage(
                chatId,
                sent
                    ? "Magic packet sent to the PC."
                    : "Failed to send the magic packet."
            );
        } else if (text == "/status") {
            sendStatus(chatId);
        } else if (text == "/start" || text == "/help") {
            sendTelegramMessage(
                chatId,
                "Commands:\n"
                "/wake - turn on the PC\n"
                "/status - show ESP32 status\n"
                "/help - show commands"
            );
        } else {
            Serial.println("Unknown command ignored.");
        }
    }
}

void checkTelegram() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
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

    delay(20);
}
