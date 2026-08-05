# Beetle C3 Location Firmware

This sketch is the Beetle ESP32-C3 companion firmware for HAS2 IoT Glove.

It scans `HAS3:<device_id>` BLE Complete Local Names, maps the first device-id
letter to a compile-time store room, and reports `ROOM:<room>` to the TTGO over
UART1.

## Profile Settings

The Beetle and TTGO paired together must be built from the same `secrets.h`.

| Profile | SSID | Profile ID | Rooms |
| --- | --- | ---: | --- |
| `store2-badland` | `badland_ruins` | 1 | prison, ruins, checkpoint, shoot, warehouse, academy |
| `store2-city` | `bar` | 2 | house, office, bar, gunshop, foodcourt, academy |
| `store3-error` | `badland_shoot` | 3 | bamboo, toilet, sleep, underground, hallway, crack |

Create the ignored local file from `secrets.example.h` and set all six values:

```cpp
#define HMAC_SECRET "..."
#define GLOVE_WIFI_PROFILE "store2-city"
#define GLOVE_WIFI_SSID "bar"
#define GLOVE_WIFI_PASS "..."
#define GLOVE_SERVER_HOST "http://172.30.1.44"
#define GLOVE_PROFILE_ID 2
```

The Beetle does not contact `GLOVE_SERVER_HOST` directly, but the shared header
keeps both board builds on the same profile. Never commit `secrets.h`.

## PlatformIO Build

The canonical release-compatible environment is:

```powershell
pio run -e beetle-c3-location
```

Outputs:

```text
.pio/build/beetle-c3-location/firmware.bin
.pio/build/beetle-c3-location/firmware.factory.bin
```

Use `firmware.bin` for signed GitHub OTA artifacts. Use
`firmware.factory.bin` for a USB baseline; the Windows package renames it to
`beetle-factory.bin` and writes it at address `0x0`.

## Arduino IDE Build Settings

Use these board settings when compiling `wifi_location.ino` in Arduino IDE:

- Board: `DFRobot Beetle ESP32-C3`
- USB CDC On Boot: `Enabled`
- CPU Frequency: `160MHz (WiFi)`
- Flash Mode: `QIO`
- Flash Frequency: `80MHz`
- Flash Size: `4MB (32Mb)`
- Partition Scheme: `Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)`
- Core Debug Level: `None`

The partition scheme is required. The default partition gives only about
1.25MB for the app and this firmware exceeds that size because it includes BLE,
WiFi, HTTPS, JSON, and OTA support.

Do not use `No OTA` or `Huge APP` for release firmware. Those options can make a
large sketch compile, but they remove the OTA slot that this Beetle firmware
needs for GitHub Release OTA updates.

## Arduino CLI Equivalent

```powershell
arduino-cli compile `
  --fqbn "esp32:esp32:dfrobot_beetle_esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs,CPUFreq=160,FlashMode=qio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,DebugLevel=none" `
  ".\wifi_location"
```

The Arduino CLI/IDE build also requires the repository headers, vendored
libraries, and a valid `secrets.h`. PlatformIO is the release reference.

## Serial and UART

| Interface | Baud | Purpose |
| --- | ---: | --- |
| Beetle USB `Serial` | 115200 | build ID, BLE scores, Wi-Fi/OTA logs |
| Beetle `MySerial1` | 115200 | commands from TTGO and `ROOM:*`/OTA results |

Pins are RX GPIO 6 and TX GPIO 5. The TTGO UART1 side also uses 115200.

## BLE Location Stabilization

- Passive BLE scan segments run continuously while state is `activate`.
- Only valid Complete Local Names in `HAS3:<device_id>` format are accepted.
- A device becomes active after at least two samples in the 1.5-second window.
  Its RSSI uses the window median followed by a 0.5/0.5 EMA with the previous
  value.
- A room score is the average of its two strongest devices, or its one device
  when only one is active.
- While the current room still has an active score, a candidate must lead it by
  at least 5dB for 1.2 seconds. If the current room has no active score, the
  candidate only needs the 1.2-second hold.
- No valid HAS3 beacon for 5 seconds produces `ROOM:unknown`; the first valid
  best room recovers from unknown immediately.
- The stable room (including unknown) is repeated once per second.

## OTA Flow

The TTGO forwards the same `device_state` command to the Beetle first, for
example:

```text
github_dev
github_dev@v1.2.4-dev.29
```

The Beetle selects `beetle-manifest-<channel>-<GLOVE_WIFI_PROFILE>.json`, checks
channel/profile/version, verifies the raw 32-byte HMAC-SHA256 signature, installs
the image, and sends one of:

```text
beetle_ota_start
beetle_ota_done
beetle_ota_skip
beetle_ota_error
```

`beetle_ota_start` is not terminal: TTGO continues waiting. TTGO starts its own
OTA after done/skip/error or after the 180-second safety timeout. See the
[Windows USB and OTA test guide](../docs/USB_OTA_TEST_GUIDE.md) for the complete
test sequence.
