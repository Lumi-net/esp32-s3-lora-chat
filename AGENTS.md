# esp32-s3-lora-chat

ESP-IDF 5.5.4 project for ESP32-S3. LoRa peer-to-peer chat with LCD + LVGL 9.4.0 GUI, keyboard input, external flash storage, WiFi provisioning, and light sleep power management.

## Build & Flash

Standard ESP-IDF CMake build. No monorepo tooling.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

- Flash size: 16MB (QIO 80MHz)
- Partition: custom `partitions_user.csv` (factory app at 0x10000, 4MB)

## Project Map

All source is in `main/main.c` + 12 local components under `components/`.

| Component | Role |
|-----------|------|
| `APP/` | Shared types (`types.h`), globals (`va.h`) |
| `UART/` | LoRa radio via UART, frame build/parse, ACK |
| `KEY/` | Matrix keyboard scan task |
| `UI/` | LVGL screens (menu, chat, settings, home) |
| `LCD/` | SPI display init + backlight |
| `SPI/` | SPI bus init |
| `PWM/` | Backlight PWM (LEDC) |
| `FLASH/` | External SPI flash chat history (A/B block wear-leveling) |
| `MYNVS/` | NVS for aliases, colors, sleep timeout, brightness |
| `WIFI/` | AP (web config portal) → STA → SNTP sync |
| `SLEEP/` | Light sleep with GPIO wakeup (LoRa AUX pin) |
| `I2C/` | I2C master (INA219 battery monitor, currently disabled) |
| `FONTS/` | Custom bitmap fonts (jbm10/12/14) |
| `IMAGES/` | LOCK/SHIFT bitmaps |
`managed_components/lvgl__lvgl/` — LVGL 9.4.0 (fetched via `idf.py` component manager).

## Architecture

- **`app_main()`** (core 0): initializes display pipeline (PWM → SPI → LCD), creates boot screen, spawns tasks
- **`peripheral_init_task`** (core 0): sequential init of KEY → UART → FLASH → NVS, then sets `INIT_DONE` bit
- **`app_main_task`** (core 1): event loop — receives from `appQueue` (UIEvent), processes KEY/UART/WIFI events
- **LVGL mutex**: `lvgl_mutex` is a recursive mutex; always take it before calling LVGL API from non-timer-task contexts

## Key Behaviors

- **SEND**: builds LoRa frame (`buildLoRaFrame`), transmits via UART, enters `SEND_STATE_WAITING_ACK` with 3s timeout. On ACK receipt, saves to flash + updates UI. Blocks repeat send until ACK/timeout.
- **Sleep**: light sleep via `enter_light_sleep()`. Wake triggers: GPIO (keyboard or LoRa AUX pin). Auto-sleep timeout configurable via NVS. AUX wake-up → 2s quick-sleep deadline (5s fallback if no UART data).
- **WiFi provisioning**: button triggers AP mode + web config server. On form submit → STA connect → SNTP sync → WiFi off.
- **Flash storage**: A/B block wear-leveling for chat history. `chat_storage_scan()` rebuilds write offset at boot.

## Important Conventions

- LVGL calls from non-timer tasks **must** hold `lvgl_mutex` (recursive)
- `appQueue` (size 10) is the single event queue for KEY/UART/WIFI events
- `chat_list[256]` indexed by peer ID (0–255)
- Frame CRC8 is computed over all fields through `data_str[data_len]`
- Logging uses `ESP_LOGx` (30 calls in main.c)
- ESP-IDF sdkconfig checked in; `sdkconfig.old` gitignored

## VSCode / Dev Environment

- `.vscode/settings.json`: COM7, esp-idf at `D:\esp\v5.5.4\v5.5.4\esp-idf`, clangd with compile-commands in `build/`
- `.devcontainer/Dockerfile`: `espressif/idf:latest`, privileged mode
- `.clangd`: strips `-f*` and `-m*` flags from compile commands
- Expected toolchain: `xtensa-esp-elf-gcc` 14.2.0, `esp-clang` 19.1.2
- Language server: clangd with `--background-index --query-driver=**`
