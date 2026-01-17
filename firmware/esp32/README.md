## ESP32 firmware (PlatformIO + ESP-IDF)

### What it does
- **Scans** for nearby BLE devices whose displayed address starts with **`00:25:DF`** and logs RSSI.
- **Advertises** a legacy (31-byte) BLE advertisement containing **Service Data** for **UUID16 `0xFE6C`** with the same 24-byte payload used by the Android app.
- **Fuzz mode**: when enabled, every 500ms (or configured interval) it mutates payload bytes `[10]`, `[11]`, `[20]`, `[21]` and restarts advertising.

### Build

```bash
pio run -d firmware/esp32
```

### Flash (optional)

```bash
pio run -d firmware/esp32 -t upload
```

### Monitor (UART commands)

```bash
pio device monitor -d firmware/esp32
```

If your board uses a **CH343P** USB-to-serial bridge, the port may appear as `/dev/ttyUSB*` (Linux) or similar.

Commands:
- `status`
- `scan on|off`
- `tx on|off`
- `fuzz on|off`
- `fuzz interval <ms>`
- `log <0-5>` (verbosity mapping required by repo rules)
- `help`
