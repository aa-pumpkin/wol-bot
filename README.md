# wol-bot

ESP32-контроллер Wake-on-LAN, управляемый через Telegram-бота. Прошивка держит
Wi-Fi соединение, слушает команды в Telegram и по запросу шлёт magic packet
для включения ПК по сети.

## Команды бота

- `/wake` — отправить magic packet на целевой ПК
- `/status` — показать IP, уровень сигнала Wi-Fi, аптайм и broadcast-адрес ESP32
- `/start`, `/help` — список команд

Бот отвечает только пользователю с `ALLOWED_CHAT_ID`, сообщения от остальных
игнорируются.

## Требования

- [PlatformIO](https://platformio.org/) (CLI или расширение для VS Code)
- Плата на базе ESP32
- ПК с включённым Wake-on-LAN в BIOS/UEFI и в настройках сетевой карты

## Настройка

1. Скопируй `include/secrets.h.example` в `include/secrets.h` и заполни своими
   данными:
   - `WIFI_SSID`, `WIFI_PASSWORD` — данные Wi-Fi сети
   - `BOT_TOKEN` — токен Telegram-бота от [@BotFather](https://t.me/BotFather)
   - `ALLOWED_CHAT_ID` — твой chat ID в Telegram (например, через [@userinfobot](https://t.me/userinfobot))

   Файл `secrets.h` в `.gitignore` и не попадает в репозиторий.

2. В `src/main.cpp` замени `targetMac` на MAC-адрес сетевой карты целевого ПК:

   ```cpp
   const uint8_t targetMac[6] = {
       0x88, 0xAE, 0xDD, 0x12, 0x33, 0x2A
   };
   ```

3. При необходимости поправь `upload_port` / `monitor_port` в `platformio.ini`
   под свой порт ESP32 (по умолчанию `/dev/ttyUSB0`).

## Сборка и прошивка

```sh
pio run                # собрать
pio run -t upload      # прошить плату
pio device monitor      # смотреть логи по serial
```

## Как это работает

- ESP32 подключается к Wi-Fi и переподключается автоматически при обрыве связи.
- Раз в 2 секунды опрашивает Telegram на новые сообщения (long polling выключен,
  используется short polling через `bot.getUpdates`).
- По команде `/wake` формирует и рассылает magic packet (6 байт `0xFF` +
  16 повторов MAC-адреса) на broadcast-адрес текущей подсети, порт 9, с
  несколькими повторными попытками для надёжности.
