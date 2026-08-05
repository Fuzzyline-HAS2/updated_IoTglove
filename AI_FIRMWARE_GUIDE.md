# AI Firmware Guide

이 문서는 다음 AI 또는 개발자가 `updated_IoTglove` 펌웨어를 수정하기 전에 확인해야 하는 운영 규칙이다.

## 기본 원칙

- 메인 보드는 TTGO T8 v1.7.1(`ttgo-t8-v171`, PlatformIO board `ttgo-t1`)이고,
  위치 보드는 DFRobot Beetle ESP32-C3(`beetle-c3-location`)다.
- Nextion HMI는 `glove.HMI` 기준으로 동작한다.
- `nextion_tft_upload.ino`의 `Serial.*`은 PC-Nextion TFT 업로드 프로토콜이므로 Telnet debug로 바꾸지 않는다.
- 일반 디버그 로그는 `__DEBUG__`가 켜졌을 때 Telnet debug 계층으로 출력한다.
- 실제 WiFi/HMAC 값은 `secrets.h`에만 둔다. `secrets.h`는 git에 올리지 않는다.
- TTGO USB Serial은 921600, Beetle USB Serial은 115200, 두 보드의 UART1은
  양쪽 모두 115200이다. TTGO의 `Serial.begin()`은 `NextionTftUploadInit()`에서
  간접 호출된다.
- 변경 후에는 profile unit test와 두 PlatformIO 환경 빌드를 모두 확인한다.

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
v1.2.4-dev.29 -> 10204129
v1.2.4-rc.1  -> 10204201
v1.2.4       -> 10204300
```

펌웨어 버전 상수는 `firmware_version.h`에 둔다.

```cpp
#define FIRMWARE_VERSION "1.2.4-dev.29"
#define FIRMWARE_VERSION_CODE 10204129
```

현재 manifest OTA 호출은 `allow_downgrade=true`다. 같은 `version_code`만 skip하고,
QA rollback을 위해 다른 version code는 현재 값보다 낮아도 설치한다. production에서
downgrade를 막으려면 호출 정책을 별도로 변경해야 한다.

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
  "wifi_profile": "store2-badland",
  "version": "1.2.4-dev.29",
  "version_code": 10204129,
  "firmware_url": "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/v1.2.4-dev.29/update-store2-badland.bin",
  "signature_url": "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/v1.2.4-dev.29/update-store2-badland.sig",
  "build_info_url": "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/v1.2.4-dev.29/build-info-store2-badland.json",
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
- Boards: TTGO `ttgo-t1`, Beetle `dfrobot_beetle_esp32c3`
- Framework: `arduino`

로컬 Docker 검증 예시:

```powershell
docker build -t iotglove-builder .
.\scripts\pio_docker_build.ps1 -Env ttgo-t8-v171,beetle-c3-location
```

현재 `HAS2_Wifi`는 `lib/vendor/HAS2_Wifi`에 있으므로 정상 빌드에 GitHub token이
필요하지 않다.

## Library Policy

전역 Arduino libraries 폴더에 의존하지 않는다.

현재 펌웨어에서 사용하는 라이브러리:

- `HAS2_Wifi`: repo 내부 `lib/vendor/HAS2_Wifi`
- `Adafruit_NeoPixel`: PlatformIO registry exact version `1.15.5`
- `ArduinoJson`: PlatformIO registry exact version `7.4.3`
- `IRremoteESP8266`: PlatformIO registry exact version `2.9.0`
- `Pangodream_18650_CL`: upstream Git commit `e1be2aa402bd19473aac2a843c9aeb4f274418c7`
- `Nextion`: repo 내부 `lib/vendor/Nextion`
- `SimpleTimer`: repo 내부 `lib/vendor/SimpleTimer`
- `SecureOTA`: 펌웨어 repo 내부 local module

`platformio.ini`의 `lib_extra_dirs = lib/vendor`가 vendored HAS2/Nextion/SimpleTimer를
찾는다. 전역 Arduino libraries 폴더나 현재 사용하지 않는 HAS2 라이브러리에
의존성을 추가하지 않는다. Wi-Fi 비밀번호 또는 실제 운영용 `secrets.h`를 소스에
하드코딩하거나 commit하지 않는다.

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

`GLOVE_WIFI_PROFILE`과 `GLOVE_PROFILE_ID`를 교차 검증하는 장치는 없다. CI는 둘을
같은 테이블에서 생성하므로 어긋날 수 없지만, 손으로 고친 `secrets.h`는 city의
SSID/서버에 badland 방 목록을 짝지어도 그대로 빌드된다. 두 값은 항상 함께 고친다.

`location_protocol.h`는 `secrets.h`를 직접 include한다. `wifi_location.h`가
`location_protocol.h`를 `secrets.h`보다 먼저 include하므로, 헤더가 자립하지
않으면 Beetle 빌드에서 `GLOVE_PROFILE_ID` `#error`가 발동한다.

## Server and OTA Contract

OTA용 DB 필드를 추가하지 않는다. 채널과 고정 태그 선택은 `device_state` 한 필드에
인코딩한다. `ota_channel`과 `ota_manifest_url`은 사용하지 않는다.

| `device_state` | 동작 |
| --- | --- |
| `github` | compile-time 기본 채널 `OTA_CHANNEL` |
| `github_dev` | `dev-latest`의 현재 프로필 manifest |
| `github_rc` | `rc-latest`의 현재 프로필 manifest |
| `github_prd` | GitHub production latest |
| `github_dev@v1.2.4-dev.29` | 정확한 dev 태그의 현재 프로필 manifest |
| `github_rc@v1.2.4-rc.1` | 정확한 rc 태그의 현재 프로필 manifest |
| `github_prd@v1.2.4` | 정확한 production 태그의 현재 프로필 manifest |

서버 명령에는 프로필 이름을 붙이지 않는다. TTGO와 Beetle이 각자의 compile-time
`GLOVE_WIFI_PROFILE`을 사용해 프로필별 manifest를 선택하며, manifest의
`wifi_profile`이 다르면 설치를 거부한다. `@` 뒤에 태그가 있으면 compile-time
latest URL보다 고정 태그 URL을 우선한다.

부팅 후 Wi-Fi/서버 동기화가 끝나면 TTGO가 다음 값을 보고한다.

- `esp_version`: 현재 `FIRMWARE_VERSION`
- `nextion_version`: `pgSetting.vVersion.val`; 읽기 실패 시 `-1`

OTA 성공 시 TTGO는 `device_state=setting`을 보내고 재부팅한다. manifest 다운로드,
채널/프로필 검증, HMAC 서명 또는 설치가 실패하면 `device_state=ota_error`를 보낸다.
Beetle에는 서버 version 필드가 없으므로 TTGO 로그의 `beetle_ota_done`과 Beetle
USB Serial build ID로 확인한다.

## Verification Checklist

- `python3 -m unittest discover -s tests -v` 통과
- Docker/PlatformIO `ttgo-t8-v171`, `beetle-c3-location` 모두 통과
- PlatformIO 빌드가 보조 스케치 폴더를 컴파일하지 않는지 확인
- `.bin` 안에 `FIRMWARE_VERSION`, `BUILD_GIT_COMMIT`, `BUILD_ESP32_CORE`, `BUILD_PLATFORMIO_CORE` 문자열이 들어있는지 확인
- Release asset의 manifest/build-info/hash/size 일치 확인
- 같은 `version_code`이면 OTA skip 확인
- 낮은 고정 태그가 QA rollback으로 설치되는지 확인
- channel mismatch이면 OTA 중단 및 `ota_error` 확인
- signature/bin HMAC mismatch이면 OTA 실패 확인

## Beetle Location Firmware

- `wifi_location/wifi_location.ino` is the Beetle ESP32-C3 firmware connected to TTGO over UART1.
- TTGO sends `setting `, `ready `, `activate `, and OTA commands such as `github_dev ` or `github_prd@v1.2.4 ` to Beetle.
- Beetle scans BLE advertisers, not WiFi AP RSSI.
- BLE advertiser Local Name은 `HAS3:<device_id>` 형식이어야 하고, Complete Local
  Name AD field와 프로필 방 prefix 검증을 모두 통과해야 한다.
- Beetle은 1.5초 표본 창에 같은 장치 표본이 2개 이상 있을 때 RSSI median을
  구하고 기존 값과 0.5/0.5 EMA로 합친다. 방별 점수는 가장 강한 장치 두 개의
  평균(한 개면 그 값)이다.
- 현재 방 점수가 살아 있으면 새 방이 5dB 이상 강한 상태로 1.2초 유지되어야
  전환한다. 현재 방 점수가 사라진 경우에는 5dB 조건 없이 새 후보를 1.2초
  유지하면 전환한다.
- 유효 HAS3 beacon이 5초 없으면 `ROOM:unknown`을 보낸다. unknown 상태에서
  유효한 최상위 방이 다시 생기면 즉시 그 방으로 복귀한다.
- Beetle sends `reset ` on boot. TTGO responds with the current `device_state`.
- TTGO는 OTA 명령을 Beetle에 먼저 전달한다. `beetle_ota_start`에서는 기다리고,
  `beetle_ota_done`, `beetle_ota_skip`, `beetle_ota_error` 또는 180초 timeout 뒤
  TTGO OTA를 시작한다.
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

1. `firmware_version.h`와 일치하는 SemVer tag(예: `v1.2.4-dev.30`)가 push된다.
2. `.github/workflows/release.yml` runs on a GitHub-hosted Ubuntu runner.
3. The workflow runs a three-profile matrix and creates `secrets.h` from each
   profile's GitHub Actions variable plus the shared secrets.
4. Docker builds the fixed PlatformIO environment.
5. PlatformIO builds TTGO and Beetle firmware.
6. The workflow signs each `.bin`, creates collision-free profile manifests and
   build-info files, and uploads all 24 assets to one GitHub Release.
7. dev/rc는 같은 commit으로 `dev-latest`/`rc-latest` tag를 이동하고 같은 24개
   프로필 자산을 pointer release에 `--clobber` 업로드한다.

`gh release upload --clobber`는 같은 이름만 교체하고 과거의 다른 이름 자산은
삭제하지 않는다. immutable 버전 Release는 정확히 24개여야 한다. pointer
release에 legacy 무프로필 자산이 남아도 현재 펌웨어는 프로필별 이름만 요청하지만,
정리가 필요하면 대상 자산을 확인한 뒤 명시적으로 삭제한다.

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

현재 vendored 라이브러리만으로 빌드되므로 `HAS2_LIB_TOKEN`은 필요하지 않다.
향후 private 의존성을 다시 추가한 경우에만 현재 PowerShell session에 token을 두고
`-ForwardHas2Token`을 명시한다. token을 소스나 `secrets.h`에 넣지 않는다.
