# Windows USB 설치와 GitHub OTA 테스트

이 문서는 TTGO와 Beetle을 USB factory 이미지로 초기화한 다음, 같은 매장
프로필의 GitHub Release로 OTA하는 QA 절차다.

## 패키지 선택

TTGO와 Beetle에 반드시 같은 ZIP의 이미지를 설치한다.

| ZIP 이름 형식 | 프로필 | SSID | 서버 |
| --- | --- | --- | --- |
| `IoTglove-<version>-store2-city-windows.zip` | `store2-city` | `bar` | `http://172.30.1.44` |
| `IoTglove-<version>-store2-badland-windows.zip` | `store2-badland` | `badland_ruins` | `http://172.30.1.43` |
| `IoTglove-<version>-store3-error-windows.zip` | `store3-error` | `badland_shoot` | `http://172.30.1.43` |

현재 dev28 QA 패키지:

| ZIP | SHA-256 |
| --- | --- |
| `IoTglove-dev.28-store2-city-windows.zip` | `791278c4bb592519d9751e15cdb2793de4a1778923c7a82b937bfcd0f731d3b4` |
| `IoTglove-dev.28-store2-badland-windows.zip` | `f818a95f12ac606379572e33e9fad6a208949e5471249b75750ef468a9b89557` |
| `IoTglove-dev.28-store3-error-windows.zip` | `6d932a4accb8d366a497c96d18736342af3a306146924d65d210f304175b6b7f` |

각 ZIP은 다음 파일을 포함한다.

```text
IoTglove-<version>-<profile>/
├── WINDOWS_FLASH_README.txt
├── scripts/
│   ├── auto_flash_beetle.bat
│   ├── auto_flash_beetle.ps1
│   ├── auto_flash_ttgo.bat
│   └── auto_flash_ttgo.ps1
└── dist/
    ├── tools/esptool/
    │   ├── esptool.exe
    │   ├── LICENSE
    │   ├── README.md
    │   └── SOURCE.txt
    └── wired-upload/
        ├── beetle-c3-location/beetle-factory.bin
        └── ttgo-t8-v171/ttgo-factory.bin
```

Python과 pip은 필요 없다. ZIP 전체를 풀고 `dist`와 `scripts`의 상대 경로를
유지해야 한다.

## USB 설치

1. 매장에 맞는 ZIP 하나를 Windows PC로 복사해 전체 압축을 푼다.
2. `scripts\auto_flash_beetle.bat`를 실행한다.
3. Beetle ESP32-C3를 연결하고 `[OK]`가 나올 때까지 기다린다.
4. `Ctrl+C`로 Beetle 감시기를 종료한다.
5. `scripts\auto_flash_ttgo.bat`를 실행한다.
6. TTGO T8 v1.7.1을 연결하고 `[OK]`가 나올 때까지 기다린다.
7. 감시기를 종료하고 두 보드를 정상 배선/전원 상태로 다시 연결한다.

스크립트는 기본적으로 다음 USB 장치를 찾는다.

| 보드 | 기본 VID/PID | chip | baud |
| --- | --- | --- | ---: |
| TTGO CH9102 | `VID_1A86.*PID_55D4` | `esp32` | 921600 |
| Beetle native USB | `VID_303A.*PID_1001` | `esp32c3` | 921600 |

CP2104 또는 CH340 TTGO라면 BAT에 인자를 전달한다.

```powershell
.\scripts\auto_flash_ttgo.bat -VidPid "VID_10C4.*PID_EA60"
.\scripts\auto_flash_ttgo.bat -VidPid "VID_1A86.*PID_7523"
```

factory 이미지는 주소 `0x0`에서 전체 플래시를 쓰므로 기존 부트로더, 파티션,
OTA 상태와 해당 범위의 NVS가 초기화된다. USB 설치 전 필요한 로컬 상태가 있으면
별도로 기록한다.

## USB 설치 확인

TTGO 부팅 로그 또는 서버 값에서 다음을 확인한다.

```text
[BOOT] build=firmware=<설치한 버전>;wifi_profile=<선택한 프로필>;code=<version code>;...
esp_version=<설치한 버전>
```

Beetle USB Serial은 115200에서 build ID와 프로필을 확인할 수 있다. TTGO USB
Serial은 921600이다.

## 고정 태그 OTA

실제 업그레이드 검증은 rolling alias보다 고정 태그를 먼저 사용한다.

```text
device_state=github_dev@v1.2.4-dev.29
```

프로필 이름을 명령에 붙이지 않는다. TTGO와 Beetle이 컴파일된
`GLOVE_WIFI_PROFILE`을 이용해 다음 manifest를 자동 선택한다.

```text
manifest-dev-<profile>.json
beetle-manifest-dev-<profile>.json
```

정상 로그 흐름:

```text
[OTA] request Beetle OTA
beetle_ota_start
beetle_ota_done
[OTA] Beetle wait finished: beetle_ota_done
[OTA] manifest <target version> code <target code> / current <current code>
[OTA] update complete
```

TTGO 재부팅 후:

```text
device_state=setting
esp_version=<target version>
```

## rolling alias 확인

고정 태그 업그레이드가 성공한 뒤 아래 명령으로 alias 경로를 확인한다.

```text
device_state=github_dev
```

이미 같은 버전이면 TTGO와 Beetle 모두 정상적으로 `already installed`/skip하는
것이 맞다. 실제 재설치 테스트가 필요하면 더 낮은 USB 버전을 다시 넣거나 다음
dev 버전을 발행한다.

## 오류 판단

| 결과 | 의미 / 확인할 항목 |
| --- | --- |
| `beetle_ota_error` | Beetle Wi-Fi, manifest, 프로필, HMAC 또는 다운로드 실패 |
| `beetle_ota_skip` | Beetle에 같은 version code가 이미 설치됨 |
| `WiFi profile mismatch` | USB 패키지와 Release 프로필이 다름 |
| `signature verification failed` | 로컬 USB 빌드와 GitHub Release의 HMAC 설정 불일치 가능성 |
| `device_state=ota_error` | TTGO manifest/채널/프로필/서명/다운로드 실패 |

## 전송 파일 해시 확인

Windows에서 ZIP이 손상되지 않았는지 확인하려면 이 문서의 현재 QA 패키지 표
또는 로컬 `dist/README.md`에 기록된 ZIP SHA-256과 비교한다.

```powershell
Get-FileHash .\IoTglove-dev.28-store2-city-windows.zip -Algorithm SHA256
```

각 ZIP 안의 `WINDOWS_FLASH_README.txt`에도 TTGO·Beetle factory 이미지 해시가
기록되어 있다.
