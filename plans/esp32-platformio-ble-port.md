## ESP32 PlatformIO BLE port (AxonCadabra)

### Goal
Port the Android app’s BLE logic to an ESP32 firmware built with **PlatformIO + ESP-IDF (NimBLE)**:
- **Scan** for nearby devices whose public address string starts with **`00:25:DF`** (Axon OUI) and report RSSI.
- **Advertise** a **Service Data** AD structure for **UUID `0xFE6C`** with the same payload used by the Android app.
- Implement **fuzz mode**: every **500ms**, stop advertising, mutate specific payload bytes, restart advertising.

### What the Android app does (source of truth)
Android implementation: `app/src/main/java/com/axon/blecontroller/MainActivity.kt`
- **OUI filter**: `00:25:DF`
- **Service UUID**: `0000FE6C-0000-1000-8000-00805F9B34FB` (i.e., UUID16 `0xFE6C`)
- **Base payload** (`24B`):
  - `01 58 38 37 30 30 32 46 50 34 01 02 00 00 00 00 CE 1B 33 00 00 02 00 00`
- **Fuzz interval**: `500ms`
- **Fuzz mutation**:
  - `payload[10] = (fuzzValue >> 8) & 0xFF`
  - `payload[11] = fuzzValue & 0xFF`
  - `payload[20] = (fuzzValue >> 4) & 0xFF`
  - `payload[21] = (fuzzValue << 4) & 0xFF`

### On-air encoding (ESP32 target behavior)
The ESP32 shall advertise a **Service Data (AD type 0x16)** structure:
- `uuid16_le = 0x6C 0xFE`
- followed by the **24-byte** payload above (mutated in fuzz mode).

### Constraints / rules we must follow
- Repo `.cursor/rules/` require: atomic commits, upstream sync before commits, push after commits, strict C warnings, return-code error handling, and custom logging levels between INFO and DEBUG.
- Embedded firmware has no `-v` CLI flags; we’ll satisfy verbosity mapping via UART commands and a compile-time default.

### Questions (at least 10)
1. Which PlatformIO framework? **Answer: ESP-IDF**
2. Which features to port? **Answer: scan + advertise**
3. Should scan run continuously or in periodic windows (power vs latency)?
4. Should advertising be connectable like Android (`connectable=true`) or non-connectable to reduce connection churn?
5. If a central connects, should we immediately disconnect (no GATT), or keep connection open briefly?
6. Do you want the advertising interval to be configurable (and if so, preferred default)?
7. For the OUI filter: do you want strict MAC prefix match only, or also accept randomized addresses where OUI isn’t meaningful?
8. What UART command surface is preferred (minimal set vs richer diagnostics)?
9. How should the “custom log levels” rule be satisfied on firmware? **Answer: compile-time default + UART runtime override**
10. Should fuzz mutate only the four indices the Android app mutates, or should we add optional modes later?
11. Which ESP32 board target should be the default PlatformIO env (e.g., `esp32dev`, `esp32-s3-devkitc-1`)?
12. Do you want scanning results persisted (top-N by RSSI) or just streamed logs?

### Implementation TODOs
- [x] Save this plan in `plans/` at repo root.
- [x] Add repo-root `.clang-format` and `Makefile` wrappers (Android + ESP32).
- [x] Create `firmware/esp32/` PlatformIO ESP-IDF project skeleton.
- [x] Implement NimBLE advertiser: service-data UUID16 `0xFE6C` + payload, start/stop, handle connect events.
- [x] Implement NimBLE scanner: OUI filter `00:25:DF`, log RSSI and address.
- [x] Implement fuzz loop: 500ms stop/mutate/restart with the exact Android mutation.
- [x] Implement UART commands: `scan`, `tx`, `fuzz`, `log`, `status`.
