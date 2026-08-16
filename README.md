# wol-bot

ESP32 + Telegram bot for waking up my PC over the network.

## Commands

- `/wake` - send a magic packet to the target PC
- `/status` - ESP32 IP, Wi-Fi signal, uptime
- `/help` - list commands

Only replies to the chat ID set in `ALLOWED_CHAT_ID`.

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and fill in your
   Wi-Fi credentials, bot token and chat ID.
2. Set your PC's MAC address in `targetMac` in `src/main.cpp`.
3. Adjust `upload_port` in `platformio.ini` if needed.

```sh
pio run -t upload
pio device monitor
```
