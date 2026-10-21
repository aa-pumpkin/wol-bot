# wol-bot

ESP32 + Telegram bot for waking up my PC over the network.

## Using the bot

Send `/start` and two buttons appear under the message box:

| Button | Command | What it does |
| --- | --- | --- |
| ⚡ Wake PC | `/wake` | Sends a magic packet, the PC boots in about a minute |
| 📊 Status | `/status` | Shows that the bot is online: Wi-Fi signal, uptime, IP |
| | `/help` | Shows what the bot can do |

The commands are also in Telegram's **Menu** button. The bot only replies to
the chat ID set in `ALLOWED_CHAT_ID` and ignores everyone else.

## Android widget

The `android/` app adds a one-tap **Wake PC** widget that follows the phone's
system colors (Material You). It works from anywhere: the widget posts a signed
message to a private [ntfy.sh](https://ntfy.sh) topic, the ESP keeps a
connection to that topic open and wakes the PC. The message is signed with a
shared key and expires after two minutes, so knowing the topic isn't enough to
wake the PC. Every wake from the widget is also reported in Telegram.

```sh
python3 tools/widget_secret.py   # adds WIDGET_TOPIC/WIDGET_KEY to secrets.h
pio run -t upload                # flash the ESP with the new secret
cd android && ./gradlew installDebug
```

The build needs an Android SDK (`sdk.dir` in `android/local.properties`) and a
JDK 17+.

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and fill in your
   Wi-Fi credentials, bot token and chat ID.
2. Set your PC's MAC address in `targetMac` in `src/main.cpp`.
3. Turn on Wake-on-LAN in the PC's BIOS and network card settings.
4. Adjust `upload_port` in `platformio.ini` if needed.

```sh
pio run -t upload
pio device monitor
```
