# glove.HMI 전환 서버/DB 수정 정리

이 문서는 `C:\Users\teamh\OneDrive\바탕 화면\CODE\iotglove` legacy 펌웨어와 현재 `updated_IoTglove` 펌웨어 흐름을 비교해서, 펌웨어를 제외한 서버/DB 쪽에서 맞춰야 하는 항목을 정리한 것이다.

## 핵심 결론

- 기존 서버는 `life_chip`, `role`, `shift_machine`, `taken_chip`, `battery_pack`, `lv` 같은 기본 필드를 이미 내려주는 구조로 보인다.
- 새 펌웨어는 `life_chip < 1`이면 펌웨어가 서버에 `role=revival` 변경 요청을 보낸다.
- 서버는 그 요청 이후 `shift_machine`을 변경해서 장갑이 `my` 전체 데이터를 다시 읽게 하고, 그때 DB의 `role=revival`이 내려오도록 해야 한다.
- 부활 쿨다운은 HMI 터치가 아니라 `revival IR 송신 -> player 수신 -> revival_cooldown 전송 -> 서버 count 증가 -> revival 장갑이 count 증가분 반영` 구조다.

## DB 필드

### 기존 유지 필드

| 필드 | 용도 |
| --- | --- |
| `role` | `player`, `tagger`, `revival`, `ghost` 등 현재 역할 |
| `life_chip` | 생존자 생명 수. `0` 이하가 되면 revival 전환 대상 |
| `taken_chip` | 술래가 획득한 생명 수 |
| `battery_pack` | 배터리/아이템 표시용 |
| `lv` | 레벨 표시용 |
| `device_state` | `setting`, `ready`, `activate`, `photo`, `github_*`, `ota_error` 등 화면/OTA 상태 |
| `game_state` | 게임 진행 상태 |
| `shift_machine` | 펌웨어가 전체 `my` 데이터를 다시 읽도록 하는 변경 트리거 |
| `esp_version` | TTGO가 부팅 후 보고하는 현재 펌웨어 SemVer 문자열 |
| `nextion_version` | TTGO가 `pgSetting.vVersion.val`에서 읽어 보고하는 값; 실패 시 `-1` |

### 새로 필요하거나 확정해야 하는 필드

| 필드 | 타입 권장 | 필요 여부 | 설명 |
| --- | --- | --- | --- |
| `revival_cooldown_count` | int | 필수 | player가 revival 장갑을 도와준 횟수. revival 진입 시 `0`으로 초기화 |
| `revival_reduce_time` | int | 필수 | 도움 1회당 줄어드는 부활 시간, 초 단위 |
| `lives_lost` | int | 필수 | 결과 화면에서 생존자가 잃은 생명 수 |
| `batteries_gained` | int | 필수 | 결과 화면에서 생존자가 획득한 배터리 수 |
| `round_taken_chip` | int | 필수 | 결과 화면에서 술래가 해당 라운드에 획득한 생명 수 |
| `max_chip` | int | 권장 | 술래 화면 `pgTagFull` 전환 기준. 없으면 펌웨어 기본값 사용 필요 |
| `revival_help_log` | table/log | 선택 | 서버에서 중복 도움 이벤트까지 막고 싶을 때 사용 |

## OTA 서버 계약

OTA 채널이나 manifest URL을 위한 별도 DB 필드를 추가하지 않는다. 서버는 대상
장갑의 `device_state`만 변경한다.

| `device_state` 값 | 동작 |
| --- | --- |
| `github` | 펌웨어 compile-time 기본 채널 |
| `github_dev` | `dev-latest`의 현재 매장 프로필 |
| `github_rc` | `rc-latest`의 현재 매장 프로필 |
| `github_prd` | GitHub production latest |
| `github_dev@v1.2.4-dev.29` | 고정 dev 태그의 현재 매장 프로필 |
| `github_rc@v1.2.4-rc.1` | 고정 rc 태그의 현재 매장 프로필 |
| `github_prd@v1.2.4` | 고정 production 태그의 현재 매장 프로필 |

명령에 city/badland/error를 붙이지 않는다. USB로 설치된 TTGO와 Beetle의
compile-time `GLOVE_WIFI_PROFILE`이 프로필별 manifest를 자동 선택하며, 다른
프로필 manifest는 펌웨어가 거부한다.

OTA 진행 순서:

1. TTGO가 명령을 Beetle에 전달한다.
2. Beetle이 `beetle_ota_done`, `beetle_ota_skip`, `beetle_ota_error` 중 하나를
   TTGO에 보낸다. 응답이 없으면 TTGO는 180초 후 자체 OTA로 진행한다.
3. TTGO가 자신의 프로필별 manifest와 HMAC 서명을 검증해 설치한다.
4. 성공하면 `device_state=setting`을 서버에 보내고 재부팅한다.
5. 재부팅 후 `esp_version`과 `nextion_version`을 다시 보고한다.

manifest/채널/프로필/서명/다운로드가 실패하면 TTGO가
`device_state=ota_error`를 보낸다. Beetle 버전은 별도 DB 필드로 보고하지 않으므로
TTGO Telnet의 `beetle_ota_done` 또는 Beetle USB Serial build ID로 확인한다.

같은 `version_code`는 skip한다. 현재 QA 정책은 고정 태그 rollback을 위해 이전
version code도 허용한다.

## 서버 동작 변경

### 1. `taken` Situation 처리

현재 펌웨어는 tagger가 player IR을 일정 횟수 감지하면 `taken` Situation을 한 번 보낸다.

서버는 `taken`을 받으면 다음을 처리해야 한다.

- 대상 player의 `life_chip` 감소
- tagger의 `taken_chip` 또는 라운드 획득 수 증가
- player의 `life_chip`이 `0` 이하가 되면 `role=revival`로 변경
- `role=revival`로 들어가는 순간 `revival_cooldown_count=0` 초기화
- 관련 장갑들이 새 값을 읽도록 `shift_machine` 갱신

### 2. 펌웨어의 `role=revival` 요청 처리

새 펌웨어는 백업 흐름으로 `life_chip < 1`을 감지하면 직접 `role=revival` 요청을 보낸다.

서버는 이 요청을 받으면 다음을 보장해야 한다.

- 해당 장갑의 DB `role`을 `revival`로 변경
- `revival_cooldown_count=0` 초기화
- `shift_machine` 갱신
- 이후 `my` 조회에서 `role=revival`이 내려오도록 처리

즉, 펌웨어가 명령을 보낸 즉시 내부 변수만 바꾸는 것이 아니라, 서버 DB 값이 바뀌고 `shift_machine` 변화로 다시 읽혀야 한다.

### 3. OS fallback 부활 타이머 처리

펌웨어도 revival 상태에서 로컬 부활 타이머를 돌리지만, 펌웨어가 멈추거나 WiFi 문제로 `role=player` 복귀 요청을 못 보낼 수 있다.

그래서 OS/서버도 revival 진입 시점부터 fallback 타이머를 별도로 관리해야 한다.

서버는 `role=revival`로 바뀌는 순간 다음 값을 기록하는 것을 권장한다.

| 필드 | 타입 권장 | 설명 |
| --- | --- | --- |
| `revival_started_at` | datetime/timestamp | revival 상태에 들어간 서버 기준 시각 |
| `revival_due_at` | datetime/timestamp | 서버가 fallback으로 player 전환해야 하는 예정 시각 |
| `revival_finished_at` | datetime/timestamp/null | 실제 부활 완료 처리 시각 |

`revival_due_at` 계산 기준은 다음과 같다.

```text
revival_due_at = revival_started_at + revival_time
```

`revival_cooldown_count`가 증가하면 OS도 남은 시간을 줄여야 한다.

```text
revival_due_at = revival_due_at - revival_reduce_time
```

또는 매번 재계산 방식으로 처리해도 된다.

```text
revival_due_at = revival_started_at + revival_time - (revival_cooldown_count * revival_reduce_time)
```

서버 fallback job은 주기적으로 다음 조건을 확인한다.

- `role`이 `revival` 또는 `ghost`
- 현재 시각이 `revival_due_at` 이상
- 아직 `revival_finished_at`이 없음

조건이 맞으면 서버가 직접 다음을 처리한다.

- `role=player`로 변경
- 부활 후 필요한 `life_chip` 값 회복
- `revival_finished_at` 기록
- `shift_machine` 갱신

펌웨어가 정상적으로 먼저 `role=player` 요청을 보낸 경우에는 OS fallback이 중복 처리하지 않도록 `role` 또는 `revival_finished_at` 기준으로 막아야 한다.

### 4. `revival_cooldown` Situation 처리

player가 revival IR을 받으면 `revival_cooldown` Situation을 보낸다.

서버는 다음을 처리해야 한다.

- 도움받은 revival 장갑의 `revival_cooldown_count`를 `+1`
- 변경된 count가 revival 장갑에 내려가도록 `shift_machine` 갱신
- 같은 player가 같은 revival 대상에게 여러 번 도움을 주는 중복 처리는 현재 펌웨어에서 1차 방지한다.
- 서버에서도 중복 방지를 강화하려면 `revival_help_log` 같은 별도 기록을 둔다.

### 5. `role=player` 복귀 요청 처리

revival 로컬 타이머가 끝나면 펌웨어가 `pgRevival`을 잠시 보여준 뒤 `role=player` 요청을 보낸다.

서버는 다음을 처리해야 한다.

- 해당 장갑의 `role`을 `player`로 변경
- 필요하면 `life_chip`을 부활 후 기본값으로 회복
- `shift_machine` 갱신
- 네트워크 실패 대비로 서버 fallback 타이머를 둘 수 있음
- OS fallback 타이머가 이미 처리한 경우에는 중복 처리하지 않음

## 쿨다운 계산 기준

- `revival_time`은 남은 시간이 아니라 최대 부활 시간 기준값으로 사용한다.
- 펌웨어는 `revival_cooldown_count` 증가분만큼만 반영한다.
- 반영량은 `revival_reduce_time * 1 tick`이다.
- HMI `pgGhost.vTick.val`은 1 tick/sec 기준으로 증가한다(`pgGhost.tScreenRefresh.tim=1000ms`).
- 예: `revival_reduce_time=3`이면 도움 1회마다 `3 tick`, 즉 3초가 줄어든다.

## HMI 결과 화면용 서버 데이터

`device_state=photo`가 내려오면 펌웨어는 role별 결과 화면으로 이동한다.

서버는 다음 값을 내려줘야 한다.

- 생존자 결과: `lives_lost`, `batteries_gained`, `lv`
- 술래 결과: `round_taken_chip`, `lv`
- 배터리 그림: `battery_pack`

펌웨어에서 표시 범위는 다음처럼 clamp된다.

- battery: `0-4`
- lives lost / taken: `0-30`
- batteries gained: `0-20`
- level: `0-99`

## 더 이상 서버에서 의존하지 않아도 되는 흐름

새 `glove.HMI` 전환 후에는 아래 legacy HMI 터치 생명칩 전달 흐름을 사용하지 않는다.

- `lifechip_send`
- `lifechip_rece`
- `H_lifesend`
- `H_lifechip_receive`
- `H_revival`
- legacy hacking/message 화면 흐름

## 검증 체크리스트

- `taken` 수신 시 대상 player의 `life_chip`이 감소하는지 확인
- `life_chip`이 `0` 이하가 되면 서버 DB의 `role`이 `revival`로 바뀌는지 확인
- `role=revival` 진입 시 `revival_cooldown_count`가 `0`으로 초기화되는지 확인
- `shift_machine` 변경 후 장갑이 `my` 전체 데이터를 다시 읽고 role 변경을 반영하는지 확인
- player가 revival IR을 받으면 `revival_cooldown`이 서버에 도착하는지 확인
- `revival_cooldown`마다 대상 revival 장갑의 `revival_cooldown_count`가 증가하는지 확인
- count 증가분이 펌웨어에 내려가고 부활 시간이 `revival_reduce_time`만큼 줄어드는지 확인
- revival 완료 후 `role=player` 요청을 서버가 처리하고 `shift_machine`을 갱신하는지 확인
- `device_state=photo`에서 결과 화면 필드가 정상 표시되는지 확인

## 서버 구현 시 주의점

- `role=ghost`가 내려와도 현재 펌웨어는 revival fallback처럼 처리한다.
- `revival_cooldown_count`는 누적값으로 내려줘야 한다. 펌웨어가 이전 count와 비교해서 증가분만 적용한다.
- `shift_machine`이 바뀌지 않으면 펌웨어가 새 DB 값을 다시 읽지 못할 수 있다.
- `revival_reduce_time`이 없거나 0이면 도움을 받아도 부활 시간이 줄지 않는다.
- 서버에서 `life_chip=0`만 만들고 `role`을 바꾸지 않으면 펌웨어가 백업으로 `role=revival` 요청을 보내지만, 최종 전환은 서버 DB 반영이 되어야 완료된다.
- OS fallback 타이머는 펌웨어 타이머의 백업이다. 정상 상황에서는 펌웨어가 먼저 `role=player` 요청을 보내고, 비정상 상황에서만 OS가 보정 전환한다.
