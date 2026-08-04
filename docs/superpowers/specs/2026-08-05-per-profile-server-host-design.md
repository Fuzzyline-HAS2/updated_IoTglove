# 프로필별 서버 주소 분리 설계

- 날짜: 2026-08-05
- 대상 브랜치: `codex/release-v1.2.4-dev.26`

## 배경

TTGO 펌웨어는 HAS2 서버 주소 `http://172.30.1.43`을 소스에 직접 박아 쓴다.
`store2-city`(SSID `bar`) 매장은 서버가 `http://172.30.1.44`이므로 프로필별로
주소가 달라져야 한다.

Wi-Fi 프로필은 이미 컴파일 타임 상수(`GLOVE_WIFI_PROFILE`, `GLOVE_WIFI_SSID`)로
관리되고, 릴리즈 워크플로가 `scripts/release_profiles.py`를 통해 프로필별
`secrets.h`를 생성한다. 서버 주소도 같은 경로를 타게 한다.

로컬에서 소스만 고치면 다음 릴리즈에서 CI가 `secrets.h`를 재생성하며 되돌아가므로,
CI와 문서까지 같은 변경에 포함한다.

## 데이터 흐름

```
PROFILE_SERVERS (scripts/release_profiles.py)
  -> write_secrets_header() -> secrets.h: #define GLOVE_SERVER_HOST
    -> updated_IoTglove.h (HAS2_Wifi 생성자)
    -> sensor.ino (location 전송 URL)
    -> updated_IoTglove.ino (부팅 로그)
```

서버 IP는 비밀이 아니므로 GitHub Actions secret이나 repository variable을
추가하지 않는다. 매핑 테이블을 `release_profiles.py`에 직접 둔다. SSID는
repository variable로 주입되고 테이블과 일치하는지 검증되지만, 서버 주소는
검증할 외부 입력이 없으므로 테이블이 곧 단일 출처다.

## 프로필 매핑

| profile | SSID | server |
| --- | --- | --- |
| `store2-badland` | `badland_ruins` | `http://172.30.1.43` |
| `store2-city` | `bar` | `http://172.30.1.44` |
| `store3-error` | `badland_shoot` | `http://172.30.1.43` |

## 변경 파일

| 파일 | 변경 |
| --- | --- |
| `scripts/release_profiles.py` | `PROFILE_SERVERS` 테이블 추가, `write_secrets_header()`가 `GLOVE_SERVER_HOST`를 출력 |
| `updated_IoTglove.h` | `GLOVE_SERVER_HOST` 누락 시 `#error` 가드 추가 (기존 3개 가드와 동일 패턴), `HAS2_Wifi has2wifi(GLOVE_SERVER_HOST)` |
| `sensor.ino` | location 전송 URL을 `GLOVE_SERVER_HOST` 리터럴 연결로 변경 |
| `updated_IoTglove.ino` | 부팅 로그 `[BOOT] server=`를 `GLOVE_SERVER_HOST` 기반으로 변경 |
| `secrets.example.h` | `GLOVE_SERVER_HOST` 추가 및 형식 제약 주석 |
| `tests/test_release_profiles.py` | `PROFILE_SERVERS` 매핑 정확성, 헤더 출력 assert |
| `AI_FIRMWARE_GUIDE.md` | 프로필 표에 server 열 추가 |
| `secrets.h` (gitignore) | 로컬 store2-city 빌드용 `GLOVE_SERVER_HOST "http://172.30.1.44"` |

## 형식 제약

`GLOVE_SERVER_HOST`는 반드시 `http://` 스킴을 포함하고 후행 슬래시가 없어야 한다.
`lib/vendor/HAS2_Wifi/HAS2_Wifi.cpp:507`이 `HOST_NAME.substring(7)`로 앞 7자를
잘라 IP를 얻으므로, `https://`(8자)를 넣으면 조용히 잘못된 주소가 된다.
`secrets.example.h` 주석에 이 제약을 명시한다.

## 범위에서 제외

벤더 라이브러리(`lib/vendor/HAS2_Wifi/`) 안의 `172.30.1.43` 기본값 4곳은 그대로 둔다.

- `HAS2_Wifi.cpp:15` `_activeHost` 초기값 — 호스트 지정 생성자가 `:95`에서 덮어쓴다
- `HAS2_Wifi.cpp:62` 기본 생성자 — 펌웨어가 호출하지 않는다
- `HAS2_Wifi.cpp:76` 주석
- `HAS2_Wifi.h:96` `FirmwareUpdate` 기본 파라미터 — 유일한 호출부(`:507`)가
  `HOST_NAME.substring(7)`을 명시적으로 넘기므로 기본값이 쓰이지 않는다

현재 도달 불가능한 값들이고, 수정하면 벤더 라이브러리가 `secrets.h`에 더 얽힌다.
다만 나중에 기본 생성자를 쓰면 `.43`으로 붙는 위험은 남는다.

Beetle(`beetle-c3-location`)은 HAS2 서버에 접속하지 않고 GitHub만 사용하므로
`GLOVE_SERVER_HOST`가 필요 없다. `secrets.h`를 공유하는 구조상 양쪽 env에
정의되지만 Beetle 코드에서 참조하지 않아 무해하다.

## 검증 기준

1. `python3 -m unittest tests.test_release_profiles` 통과
2. `pio run -e ttgo-t8-v171 -e beetle-c3-location` 성공
3. `strings .pio/build/ttgo-t8-v171/firmware.bin | grep '172\.30\.1\.'`에
   `172.30.1.44`가 존재. `.43`은 벤더 라이브러리 리터럴로 남을 수 있으므로
   "`.43`이 0개"를 기준으로 삼지 않는다
4. `GLOVE_SERVER_HOST`를 뺀 `secrets.h`로 빌드하면 `#error`로 실패
5. 실제 동작 확인은 부팅 시리얼의 `[BOOT] server=http://172.30.1.44/has2.php`
