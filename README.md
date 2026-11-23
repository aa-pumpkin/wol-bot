# Wake-on-LAN Bot

ESP32 firmware that boots a PC on the local network, from a Telegram chat or from an Android home screen widget. No open ports, no VPN and no server of your own needed.

## Features

- **Telegram bot** with buttons: wake the PC, check Wi-Fi signal, uptime and IP
- **Single user:** only the configured chat ID is served, everyone else is ignored
- **Android widget** (Kotlin, Material You): one tap wakes the PC from anywhere
- **Signed messages:** widget requests carry an HMAC-SHA256 signature, expire after two minutes and are protected against replay
- **Self-healing:** reconnects to Wi-Fi and to the message stream on its own

## How it works

```text
Telegram ──► Bot API ───────┐
                            ├──► ESP32 ──► magic packet (UDP broadcast) ──► PC
Android widget ──► ntfy.sh ─┘
```

The widget posts a signed message to a private [ntfy.sh](https://ntfy.sh) topic. The ESP32 keeps a connection to that topic open, verifies the message and sends the magic packet. Every wake from the widget is also reported in Telegram.

## Stack

C++ (Arduino framework), ESP32, PlatformIO, Telegram Bot API, mbedTLS, Kotlin

## Commands

| Button | Command | What it does |
| --- | --- | --- |
| ⚡ Wake PC | `/wake` | Sends a magic packet, the PC boots in about a minute |
| 📊 Status | `/status` | Wi-Fi signal, uptime, IP |
| | `/help` | Shows what the bot can do |

## Setup

1. Turn on Wake-on-LAN in the PC's BIOS and network card settings.
2. Copy `include/secrets.h.example` to `include/secrets.h` and fill in the Wi-Fi credentials, bot token, chat ID and the PC's MAC address.
3. Adjust `upload_port` in `platformio.ini` if needed.

```sh
pio run -t upload
pio device monitor
```

### Android widget

Needs an Android SDK (`sdk.dir` in `android/local.properties`) and JDK 17+.

```sh
python3 tools/widget_secret.py   # creates the shared key
pio run -t upload                # flash the ESP32 with it
cd android && ./gradlew installDebug
```
