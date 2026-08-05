# AI Firmware Guide

이 문서는 다음 AI 또는 개발자가 `updated_IoTglove` 펌웨어를 수정하기 전에 확인해야 하는 운영 규칙이다.

## 기본 원칙

- 대상 보드는 TTGO T8 v1.7.1 / ESP32 계열이며, 현재 빌드 기준 보드는 `esp32dev`다.
- Nextion HMI는 `glove.HMI` 기준으로 동작한다.
- `nextion_tft_upload.ino`의 `Serial.*`은 PC-Nextion TFT 업로드 프로토콜이므로 Telnet debug로 바꾸지 않는다.
- 일반 디버그 로그는 `__DEBUG__`가 켜졌을 때 Telnet debug 계층으로 출력한다.
- 실제 WiFi/HMAC 값은 `secrets.h`에만 둔다. `secrets.h`는 git에 올리지 않는다.
- 변경 후에는 가능한 한 Arduino CLI 컴파일과 PlatformIO Docker 빌드를 모두 확인한다.

## Version and OTA

GitHub tag는 SemVer를 사용한다.

```text
prd: v1.2.4
dev: v1.2.4-dev.1
rc:  v1.2.4-rc.1
```

펌웨어는 SemVer 문자열을 직접 비교하지 않는다. OTA 비교는 `version_code` 정수만 사용한다.

```text
version_code =
  major * 10000000
+ minor * 100000
+ patch * 1000
+ release_type * 100
+ build_number

release_type:
dev = 1
rc  = 2
prd = 3
```

예시:

```text
v1.2.4-dev.1 -> 10204101
v1.2.4-dev.2 -> 10204102
v1.2.4-dev.3 -> 10204103
v1.2.4-dev.4 -> 10204104
v1.2.4-dev.5 -> 10204105
v1.2.4-rc.1  -> 10204201
v1.2.4       -> 10204300
```

펌웨어 버전 상수는 `library_and_pin.h`에 둔다.

```cpp
#define FIRMWARE_VERSION "1.2.4-dev.5"
#define FIRMWARE_VERSION_CODE 10204105
```

OTA는 `manifest.version_code > FIRMWARE_VERSION_CODE`일 때만 실행한다.

## Release Manifest

하나의 Release에는 Wi-Fi 프로필별 asset을 올린다. `{profile}`은
`store2-badland`, `store2-city`, `store3-error` 중 하나다.

```text
update-{profile}.bin
update-{profile}.sig
manifest-{channel}-{profile}.json
build-info-{profile}.json
beetle-update-{profile}.bin
beetle-update-{profile}.sig
beetle-manifest-{channel}-{profile}.json
beetle-build-info-{profile}.json
```

manifest 예시:

```json
{
  "channel": "dev",
  "version": "1.2.4-dev.5",
  "version_code": 10204105,
  "firmware_url": "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/v1.2.4-dev.5/update-store2-badland.bin",
  "signature_url": "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/v1.2.4-dev.5/update-store2-badland.sig",
  "build_info_url": "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/v1.2.4-dev.5/build-info-store2-badland.json",
  "size": 1217716,
  "firmware_sha256": "...",
  "signature_sha256": "...",
  "build_info_sha256": "..."
}
```

`update.sig`는 hex 문자열이 아니라 raw 32바이트 HMAC-SHA256 파일이다.

## Reproducible Build

공식 Release 빌드는 GitHub Actions가 만든 결과물만 사용한다. 로컬 Docker 빌드는 검증용이다.

빌드 기준:

- Docker base image: `python:3.11.9-slim-bookworm`
- PlatformIO Core: `6.1.19`
- PlatformIO platform: `https://github.com/pioarduino/platform-espressif32.git#55.03.38`
- ESP32 Arduino core: `3.3.8`
- Board: `esp32dev`
- Framework: `arduino`

로컬 Docker 검증 예시:

```powershell
docker build -t iotglove-builder .
docker run --rm -v ${PWD}:/work iotglove-builder pio run -e ttgo-t8-v171
```

private HAS2 라이브러리를 받아야 하므로 로컬 Docker에서 완전 빌드하려면 GitHub 인증 설정이 필요하다.

## Library Policy

전역 Arduino libraries 폴더에 의존하지 않는다.

현재 펌웨어에서 사용하는 라이브러리:

- `HAS2_Wifi`: private Git repo `Fuzzyline-HAS2/HAS2_Wifi`, tag `v1.0.0`
- `Adafruit_NeoPixel`: PlatformIO registry exact version `1.15.5`
- `ArduinoJson`: PlatformIO registry exact version `7.4.3`
- `IRremoteESP8266`: PlatformIO registry exact version `2.9.0`
- `Pangodream_18650_CL`: upstream Git commit `e1be2aa402bd19473aac2a843c9aeb4f274418c7`
- `Nextion`: repo 내부 `lib/vendor/Nextion`
- `SimpleTimer`: repo 내부 `lib/vendor/SimpleTimer`
- `SecureOTA`: 펌웨어 repo 내부 local module

개인 라이브러리 관리 규칙:

- `HAS2_Wifi`, `HAS2_MQTT`, `HAS2_Vibration`은 각각 private Git repo로 관리한다.
- 초기 tag는 모두 `v1.0.0`.
- 현재 펌웨어의 `platformio.ini`에는 실제 사용 중인 `HAS2_Wifi#v1.0.0`만 넣는다.
- `HAS2_MQTT`, `HAS2_Vibration`은 사용 전까지 펌웨어 의존성에 넣지 않는다.
- Wi-Fi 비밀번호 또는 실제 운영용 `secrets.h`를 소스에 하드코딩하거나 commit하지 않는다.

전역 Arduino libraries 폴더의 현재 `HAS2_*` 라이브러리를 repo용 폴더로 준비하려면:

```powershell
python scripts\prepare_has2_libraries.py --out build\has2-libraries
```

생성된 각 폴더를 private GitHub repo에 commit한 뒤 `v1.0.0` tag를 만든다.

## GitHub Actions Secrets

Release workflow에는 다음 secrets가 필요하다.

```text
HMAC_SECRET
GLOVE_WIFI_PASS
```

SSID는 다음 GitHub Actions repository variable로 관리한다.

| profile | repository variable | value |
| --- | --- | --- |
| `store2-badland` | `GLOVE_WIFI_SSID_STORE2_BADLAND` | `badland_ruins` |
| `store2-city` | `GLOVE_WIFI_SSID_STORE2_CITY` | `bar` |
| `store3-error` | `GLOVE_WIFI_SSID_STORE3_ERROR` | `badland_shoot` |

세 프로필은 같은 `GLOVE_WIFI_PASS` secret을 공유한다.
릴리즈는 repository variable 값이 위 표의 SSID와 정확히 일치하지 않으면 실패한다.

HAS2 서버 주소는 비밀이 아니므로 repository variable 없이
`scripts/release_profiles.py`의 `PROFILE_SERVERS` 테이블이 단일 출처다.
이 값은 `GLOVE_SERVER_HOST`로 `secrets.h`에 기록된다.

| profile | `GLOVE_SERVER_HOST` |
| --- | --- |
| `store2-badland` | `http://172.30.1.43` |
| `store2-city` | `http://172.30.1.44` |
| `store3-error` | `http://172.30.1.43` |

`GLOVE_SERVER_HOST`는 `http://` 스킴을 포함하고 후행 슬래시가 없어야 한다.
`HAS2_Wifi`가 `HOST_NAME.substring(7)`로 IP를 잘라내므로 `https://`를 쓰면
조용히 잘못된 주소가 된다.

## BLE Location Rooms

매장마다 방 이름이 다르므로 `location_protocol.h`의 `HAS3_ROOMS` 테이블을
컴파일 타임에 선택한다. `GLOVE_WIFI_PROFILE`은 문자열 리터럴이라 전처리기로
비교할 수 없어, `release_profiles.py`가 숫자 `GLOVE_PROFILE_ID`를 함께 쓴다.

| profile | `GLOVE_PROFILE_ID` | rooms (HAS3_ROOMS 순서) |
| --- | --- | --- |
| `store2-badland` | 1 | prison, ruins, checkpoint, shoot, warehouse, academy |
| `store2-city` | 2 | house, office, bar, gunshop, foodcourt, academy |
| `store3-error` | 3 | bamboo, toilet, sleep, underground, hallway, crack |

BLE 장치 이름은 방 이름의 첫 글자를 대문자로 접두에 갖는다. `bar itembox 1`은
`HAS3:BI1`로 광고한다. 따라서 prefix는 `HAS3_ROOMS[i][0]`에서 파생되고, 방을
추가하거나 바꿀 때 고칠 곳은 이 테이블 하나뿐이다.

제약:

- 한 프로필 안에서 방 이름 첫 글자가 겹치면 안 된다. 매핑이 모호해진다.
- 세 프로필의 방 개수가 같아야 한다. `HAS3_ROOM_COUNT`는 두 바이너리가 공유하는
  컴파일 타임 배열 크기다.
- 프로필 간에는 같은 prefix가 다른 방을 뜻한다(`B`는 city의 `bar`,
  store3의 `bamboo`). 따라서 **잘못된 프로필로 구운 장비는 방 이름을 조용히
  틀리게 보고한다.** 서버가 이를 검출할 방법은 없다.

위 표는 `tests/test_release_profiles.py`가 `location_protocol.h`를 파싱해
`PROFILE_ROOMS`와 일치하는지 검증한다.

## Server Contract

Current OTA policy: the server DB only changes `device_state` to `github`. Ignore any older references to `ota_channel` or `ota_manifest_url`; those are now firmware compile-time constants.

On boot, after WiFi/server sync, TTGO reports version fields to the server:

- `esp_version`: current firmware `FIRMWARE_VERSION`.
- `nextion_version`: numeric value read from `pgSetting.vVersion.val`.
- If the Nextion value cannot be read, firmware sends `nextion_version=-1`.

OTA 트리거는 기존처럼 `device_state=github`를 사용한다.

서버 DB 필드:

| field | value | description |
| --- | --- | --- |
| `ota_channel` | `dev`, `rc`, `prd` | 장갑이 받을 OTA 채널 |
| `ota_manifest_url` | URL or empty | 특정 manifest 직접 지정 |

OTA 실패 시 펌웨어는 `device_state=ota_error`를 서버로 전송한다.

## Verification Checklist

- Arduino CLI compile 통과
- Docker `pio run -e ttgo-t8-v171` 통과
- PlatformIO 빌드가 보조 스케치 폴더를 컴파일하지 않는지 확인
- `.bin` 안에 `FIRMWARE_VERSION`, `BUILD_GIT_COMMIT`, `BUILD_ESP32_CORE`, `BUILD_PLATFORMIO_CORE` 문자열이 들어있는지 확인
- Release asset의 manifest/build-info/hash/size 일치 확인
- `version_code <= FIRMWARE_VERSION_CODE`이면 OTA skip 확인
- channel mismatch이면 OTA 중단 및 `ota_error` 확인
- signature/bin HMAC mismatch이면 OTA 실패 확인
## OTA DB Contract Update

- Do not add OTA DB fields. OTA selection is encoded in `device_state`.
- Supported values:
  - `github`: legacy default channel from `OTA_CHANNEL`.
  - `github_dev`: latest dev manifest from `dev-latest`.
  - `github_rc`: latest rc manifest from `rc-latest`.
  - `github_prd`: GitHub production latest release.
  - `github_dev@v1.2.4-dev.5`: exact dev release tag.
  - `github_rc@v1.2.4-rc.1`: exact rc release tag.
  - `github_prd@v1.2.4`: exact production release tag.
- If a tag is present after `@`, the tag manifest wins over compile-time default URLs.
- 서버 명령에는 공간명을 붙이지 않는다. TTGO와 Beetle은 compile-time
  `GLOVE_WIFI_PROFILE`에 따라 같은 채널/태그의 프로필별 manifest를 자동으로 선택한다.
- OTA 설치 전 manifest의 `wifi_profile`이 장비의 compile-time 프로필과 일치하는지 검증한다.
- Manifest OTA allows downgrade for QA rollback. Same `version_code` is still skipped.
- `device_state=ota_error` is sent when manifest download, channel check, signature check, or OTA execution fails.
- Release workflow updates `dev-latest` and `rc-latest` pointer releases for prerelease tags.
- GitHub `releases/latest` remains reserved for production `prd`.
- Do not store `HAS2_LIB_TOKEN` in firmware. That token is only for GitHub Actions to fetch private libraries during build.

## Beetle Location Firmware

- `wifi_location/wifi_location.ino` is the Beetle ESP32-C3 firmware connected to TTGO over UART1.
- TTGO sends `setting `, `ready `, `activate `, and OTA commands such as `github_dev ` or `github_prd@v1.2.4 ` to Beetle.
- Beetle scans BLE advertisers, not WiFi AP RSSI.
- BLE device ID is the advertiser Local Name, and only names starting with `HAS2` are valid location candidates.
- Beetle averages RSSI for 3 seconds and sends the nearest stable ID as `<device_id> ` after the same ID wins 2 scan windows in a row.
- Beetle sends `reset ` on boot. TTGO responds with the current `device_state`.
- If TTGO receives an OTA `device_state`, it sends the same OTA command to Beetle first, then starts TTGO OTA after `beetle_ota_start`, `beetle_ota_skip`, `beetle_ota_error`, or a 10 second timeout.
- Beetle performs its own GitHub manifest OTA over WiFi using the same `GLOVE_WIFI_PROFILE`, `GLOVE_WIFI_SSID`, `GLOVE_WIFI_PASS`, and `HMAC_SECRET` build settings.
- Beetle must use the `min_spiffs` partition because BLE + HTTPS OTA is larger than the default 1.2MB OTA app slot.
  In Arduino IDE, select `Partition Scheme = Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)`.
  Do not use `No OTA` or `Huge APP` for release firmware because Beetle GitHub OTA needs an OTA slot.
- Release assets include one Beetle artifact set per Wi-Fi profile in the same tag as TTGO:
  - `beetle-update-{profile}.bin`
  - `beetle-update-{profile}.sig`
  - `beetle-manifest-{channel}-{profile}.json`
  - `beetle-build-info-{profile}.json`

## Build Location and Cache Policy

Release binaries are built by GitHub Actions, not by the developer's local PC.
Local builds are for verification and USB flashing. The official OTA assets are
the files attached to the GitHub Release.

GitHub Actions flow for this repository:

1. A SemVer tag such as `v1.2.4-dev.5` is pushed.
2. `.github/workflows/release.yml` runs on a GitHub-hosted Ubuntu runner.
3. The workflow runs a three-profile matrix and creates `secrets.h` from each
   profile's GitHub Actions variable plus the shared secrets.
4. Docker builds the fixed PlatformIO environment.
5. PlatformIO builds TTGO and Beetle firmware.
6. The workflow signs each `.bin`, creates collision-free profile manifests and
   build-info files, and uploads all 24 assets to one GitHub Release.

GitHub secrets are encrypted repository settings. They are injected only into
the Actions job environment and are not committed to source code. Changing a
GitHub secret affects the next GitHub Actions release build only; it does not
change already-built release assets or local Arduino IDE builds.

The release workflow caches PlatformIO packages in `.platformio-cache` using
`actions/cache`. This avoids downloading the ESP32 platform, toolchain, and
libraries again on every GitHub Actions run when `platformio.ini` and the
Dockerfile have not changed.

For local Docker builds, use the helper script instead of typing long Docker
commands:

```powershell
.\scripts\pio_docker_build.ps1 -BuildImage
.\scripts\pio_docker_build.ps1
.\scripts\pio_docker_build.ps1 -Env ttgo-t8-v171
.\scripts\pio_docker_build.ps1 -Env beetle-c3-location
.\scripts\pio_docker_build.ps1 -Env ttgo-t8-v171,beetle-c3-location
```

Do not use raw `docker run ... pio run ...` for normal verification builds.
The helper script is the canonical local build entry point because it always
mounts the cache volume and keeps token forwarding consistent.

The local script mounts the named Docker volume
`iotglove-platformio-cache:/opt/platformio`, so PlatformIO tools survive
`docker run --rm`. The first run can still be slow because it fills the cache;
later runs should be much faster.

If a clean local Docker build must fetch private HAS2 libraries, set
`HAS2_LIB_TOKEN` in the current PowerShell session and pass
`-ForwardHas2Token` explicitly. Do not put the token in firmware source code.
