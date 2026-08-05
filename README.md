# IoT Glove Firmware

HAS2 IoT Glove의 TTGO T8 v1.7.1 메인 펌웨어와 Beetle ESP32-C3 BLE 위치
펌웨어를 함께 관리하는 저장소다. TTGO가 게임/HMI/서버 통신을 담당하고,
Beetle은 BLE 광고를 스캔해 위치를 TTGO에 UART로 전달한다.

## 문서

- [USB 설치와 GitHub OTA 테스트](docs/USB_OTA_TEST_GUIDE.md)
- [펌웨어·릴리스 운영 규칙](AI_FIRMWARE_GUIDE.md)
- [Beetle 빌드 설정](wifi_location/README.md)
- [서버/DB 연동 사항](SERVER_DB_MIGRATION_NOTES.md)
- [Nextion TFT 업로더](nextion_tft_uploader/README.md)

## 하드웨어와 통신

| 역할 | 보드 / PlatformIO 환경 | USB Serial | 보드 간 UART1 |
| --- | --- | ---: | ---: |
| 메인 | TTGO T8 v1.7.1 / `ttgo-t8-v171` | 921600 | 115200 |
| 위치 | DFRobot Beetle ESP32-C3 / `beetle-c3-location` | 115200 | 115200 |

TTGO의 `Serial.begin(921600)`은 `setup()`에서 직접 호출하지 않고
`NextionTftUploadInit()`을 통해 설정된다. 두 보드의 USB Serial baud가 다른 것은
정상이며, TTGO-Beetle UART1은 양쪽 모두 115200이다.

## 매장 프로필

TTGO와 Beetle은 반드시 같은 프로필로 빌드하고 설치해야 한다.

| 프로필 | SSID | HAS2 서버 | ID | BLE 방 순서 |
| --- | --- | --- | ---: | --- |
| `store2-badland` | `badland_ruins` | `http://172.30.1.43` | 1 | prison, ruins, checkpoint, shoot, warehouse, academy |
| `store2-city` | `bar` | `http://172.30.1.44` | 2 | house, office, bar, gunshop, foodcourt, academy |
| `store3-error` | `badland_shoot` | `http://172.30.1.43` | 3 | bamboo, toilet, sleep, underground, hallway, crack |

릴리스 프로필 매핑은 `scripts/release_profiles.py`에서 관리한다. 실제 펌웨어의
컴파일 타임 방 테이블은 `location_protocol.h`에도 있으며,
`tests/test_release_profiles.py`가 두 파일의 ID·방 목록이 같은지 검사한다. 릴리스
manifest는 장비의 컴파일 프로필과 다른 `wifi_profile`을 거부한다.

## 로컬 secrets.h

로컬 빌드는 추적되지 않는 `secrets.h`를 사용한다.

```powershell
Copy-Item secrets.example.h secrets.h
```

다음 항목을 모두 설정한다.

```cpp
#define HMAC_SECRET "..."
#define GLOVE_WIFI_PROFILE "store2-city"
#define GLOVE_WIFI_SSID "bar"
#define GLOVE_WIFI_PASS "..."
#define GLOVE_SERVER_HOST "http://172.30.1.44"
#define GLOVE_PROFILE_ID 2
```

`GLOVE_WIFI_PROFILE`, SSID, 서버, `GLOVE_PROFILE_ID`를 한 세트로 맞춰야 한다.
`secrets.h`와 실제 비밀번호/HMAC은 commit하지 않는다.

## 빌드

두 보드를 한 번에 빌드하려면:

```powershell
pio run -e ttgo-t8-v171 -e beetle-c3-location
```

Docker 검증은 캐시와 실행 방식을 통일하는 helper를 사용한다.

```powershell
docker build -t iotglove-builder .
.\scripts\pio_docker_build.ps1
```

주요 산출물:

```text
.pio/build/ttgo-t8-v171/firmware.bin
.pio/build/ttgo-t8-v171/firmware.factory.bin
.pio/build/beetle-c3-location/firmware.bin
.pio/build/beetle-c3-location/firmware.factory.bin
```

- `firmware.bin`: OTA용 앱 이미지
- `firmware.factory.bin`: 부트로더·파티션·앱을 합친 USB 초기 설치 이미지

공식 OTA 파일은 로컬 산출물이 아니라 GitHub Actions가 서명해 Release에 올린
프로필별 파일을 사용한다.

## OTA 명령

서버는 별도 `ota_channel`/`ota_manifest_url` 필드를 만들지 않고 `device_state`에
OTA 선택을 인코딩한다.

| `device_state` | 동작 |
| --- | --- |
| `github` | 펌웨어 기본 채널(`OTA_CHANNEL`, 현재 dev) |
| `github_dev` | `dev-latest`의 현재 프로필 manifest |
| `github_rc` | `rc-latest`의 현재 프로필 manifest |
| `github_prd` | GitHub production latest |
| `github_dev@v1.2.4-dev.29` | 고정 dev 태그의 현재 프로필 manifest |
| `github_rc@v1.2.4-rc.1` | 고정 rc 태그의 현재 프로필 manifest |
| `github_prd@v1.2.4` | 고정 production 태그의 현재 프로필 manifest |

TTGO는 같은 명령을 Beetle에 먼저 전달한다. Beetle이
`beetle_ota_done`, `beetle_ota_skip`, `beetle_ota_error` 중 하나를 보내거나
180초가 지나면 TTGO OTA를 시작한다. 같은 `version_code`는 skip하며, QA용 고정
태그 롤백을 위해 다른 버전 코드는 이전 버전이어도 설치할 수 있다.

성공 후 TTGO는 `device_state=setting`을 보내고 재부팅한 다음
`esp_version`을 서버에 다시 보고한다. TTGO OTA가 실패하면
`device_state=ota_error`를 보낸다. Beetle 결과는 TTGO 로그의
`beetle_ota_done`/`skip`/`error`로 별도 확인한다.

## GitHub Release 자동화

태그를 push하면 `.github/workflows/release.yml`이 세 프로필의 TTGO·Beetle을
빌드하고 HMAC-SHA256으로 서명한 뒤 총 24개 자산을 생성한다.

Repository secrets:

```text
HMAC_SECRET
GLOVE_WIFI_PASS
```

Repository variables:

```text
GLOVE_WIFI_SSID_STORE2_BADLAND=badland_ruins
GLOVE_WIFI_SSID_STORE2_CITY=bar
GLOVE_WIFI_SSID_STORE3_ERROR=badland_shoot
```

예시:

```powershell
git tag -a v1.2.4-dev.30 -m "Release v1.2.4-dev.30"
git push origin v1.2.4-dev.30
```

태그와 `firmware_version.h`의 버전/코드가 다르면 workflow가 실패한다. dev/rc
릴리스는 각각 `dev-latest`/`rc-latest` 태그와 pointer release도 갱신한다.

## Nextion TFT 업로드와의 차이

Windows 펌웨어 USB 패키지는 번들 `esptool.exe`를 사용하므로 Python/pip이
필요 없다. `nextion_tft_uploader`는 별도 도구이며 TFT 전송에는 Python이
필요하다. 두 절차를 혼동하지 않는다.
