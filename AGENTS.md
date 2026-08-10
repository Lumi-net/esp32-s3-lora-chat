# esp32-s3-lora-chat

ESP-IDF 6.0.2 project for ESP32-S3. LoRa peer-to-peer chat with LCD + LVGL 9.4.0 GUI, keyboard input, external flash storage, WiFi provisioning, and light sleep power management.

## Build & Flash

Standard ESP-IDF CMake build. No monorepo tooling.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

- Flash size: 16MB (QIO 80MHz)
- Partition: custom `partitions_user.csv` (factory app at 0x10000, 4MB)
- `sdkconfig`/`sdkconfig.old` and `managed_components/` are gitignored/untracked; `dependencies.lock` pins the IDF version to 6.0.2 and LVGL to 9.5.0
- As a AI agent, you shouldn't try to build and flash, because there isn't idf.py in the AI environment.

## Project Map

All source is in `main/main.c` + 14 local components under `components/`.

| Component | Role |
|-----------|------|
| `APP/` | Shared types (`types.h`), globals (`va.h`) |
| `UART/` | LoRa radio via UART, frame build/parse, ACK |
| `KEY/` | Matrix keyboard scan task |
| `UI/` | LVGL screens (menu, chat, settings, home) |
| `LCD/` | ST7789 panel init + backlight |
| `SPI/` | SPI bus init |
| `PWM/` | Backlight PWM (LEDC) |
| `FLASH/` | External SPI flash chat history (A/B block wear-leveling) |
| `MYNVS/` | NVS for aliases, colors, sleep timeout, brightness |
| `WIFI/` | AP (web config portal) → STA → SNTP sync |
| `SLEEP/` | Light sleep with GPIO wakeup (LoRa AUX pin) |
| `I2C/` | I2C master (INA219 battery monitor, currently disabled) |
| `FONTS/` | Custom bitmap fonts (jbm10/12/14) |
| `IMAGES/` | LOCK/SHIFT bitmaps |

## Architecture

- **`app_main()`** (core 0): initializes display pipeline (PWM → SPI → LCD → UI), creates boot screen, spawns tasks
- **`peripheral_init_task`** (core 0): sequential init of KEY → UART → FLASH → NVS (I2C init commented out), then sets `INIT_DONE` bit
- **`app_main_task`** (core 1): event loop — receives from `appQueue` (UIEvent), processes KEY/UART/WIFI events
- **LVGL driver**: no dedicated LVGL task. A 1ms `esp_timer` calls `lv_tick_inc()`; `app_main_task` calls `lv_timer_handler()` every loop iteration, all guarded by `lvgl_mutex`
- **LVGL mutex**: `lvgl_mutex` is a recursive mutex; always take it before calling LVGL API from any context other than `app_main_task`

## Key Behaviors

- **SEND**: builds LoRa frame (`buildLoRaFrame`), transmits via UART, enters `SEND_STATE_WAITING_ACK` with 3s timeout (`ACK_TIMEOUT_MS`). On ACK receipt, saves to flash + updates UI. Blocks repeat send until ACK/timeout.
- **Sleep**: light sleep via `enter_light_sleep()`. Wake triggers: timer (heartbeat deadline) or GPIO. Keypad columns GPIO16/17/18/8 + rows GPIO5/6/7/4; LoRa AUX is GPIO21 (EXT0, low-level). After an AUX/GPI wake a 5s quick-sleep deadline is armed; a received message re-arms it.
- **WiFi provisioning**: settings page (UI) triggers AP mode + web config portal. On form submit → switch to STA → connect → SNTP sync → WiFi off.
- **Flash storage**: A/B block wear-leveling for chat history. `chat_storage_scan()` rebuilds the write offset at boot.

## Host-side tooling

- `sim_lora.py` simulates a second LoRa peer over a USB-UART module in transparent mode, mirroring the firmware frame format exactly (CRC8 poly x^8+x^5+x^4+1 init 0, `AA 55` header, 3-byte routing prefix). Useful for testing message/ACK/heartbeat logic without a second device. `pip install pyserial`, run `python sim_lora.py COMx [--self-id 0x..]`.

## Important Conventions

- LVGL calls from outside `app_main_task` **must** hold `lvgl_mutex` (recursive)
- `appQueue` (size 10) is the single event queue for KEY/UART/WIFI events
- `chat_list[256]` (`chat_item_t`) indexed by peer ID (0–255); slot 0 is the built-in "ALL" broadcast contact (sends target `0xFF`, no ACK); `id == 0xFF` marks empty slots
- Frame CRC8 is computed over all fields through `data_str[data_len]`
- Logging uses `ESP_LOGx` with per-component TAGs
- Repo-local agent skills live in `.agents/skills/` (gitignored)

## VSCode / Dev Environment

- `.vscode/settings.json`: COM7, esp-idf at `D:\esp\v6.0.2\esp-idf`, clangd with compile-commands in `build/`
- `.devcontainer/Dockerfile`: `espressif/idf:latest`, privileged mode
- `.clangd`: strips `-f*` and `-m*` flags from compile commands
- Language server: esp-clang 19.1.2 clangd with `--background-index --query-driver=**`
