# IoT Glove

> ## Ver 0.0.0
<br/>

## Ver 1.0.0
- 영어 버전 업로드
    - ChangeLanguage() 함수 추가
<br/>

## Ver 2.0.0
- 배터리 잔량 주기적 확인 (60초마다 BatteryCheck)
- WiFi 오류 복구 시스템 추가 (wifi_manager.ino, wifi_recovery.ino)

---

## WiFi 동작 구조

### 관련 파일

| 파일 | 역할 |
|---|---|
| `wifi_manager.ino` | 네트워크 레이어: RSSI 모니터링, 로밍, 재연결 |
| `wifi_recovery.ino` | 서버 레이어: 전송 실패 복구, 재연결 후 동기화 |
| `timer.ino` | 타이머 관리 |

---

## 1. WiFi 연결 & 로밍

### 타이머 주기
`WifiManagerRun()`이 **1초마다** 실행되며 RSSI(신호 강도)를 체크합니다.

### RSSI 상태머신

```
STABLE    (RSSI > -67 dBm)   : 현재 AP 유지
WATCHING  (-67 ~ -75 dBm)    : 신호 약해짐, 관찰
SCANNING  (RSSI < -75 dBm)   : 비동기 스캔 시작 → 더 강한 AP 탐색
ROAMING                       : 더 강한 AP 발견 시 전환
DISCONNECTED                  : 완전 끊김, 재연결 스캔
```

### 로밍 조건
- 현재 AP보다 **8 dBm 이상** 강한 AP가 발견될 때 자동으로 갈아탑니다.
- 같은 테마 AP 목록 안에서만 로밍합니다.

```
badland 테마: badland_ruins, badland_shoot, badland_prison, badland_check, badland_auto
city 테마   : HAS2_food, HAS2_office, HAS2_gun, HAS2_bar, HAS2_house, tp-link
```

### 테마 설정
`iotglove.ino`의 `IotGloveInit()`에서 테마를 지정합니다.

```cpp
has2wifi.Setup("badland_ruins", "Code3824@");
WifiManagerInit("badland"); // "badland" 또는 "city"
```

---

## 2. Non-Blocking 재연결

### 기존 방식의 문제점
WiFi가 끊기면 `WiFi.scanNetworks()`(blocking)를 호출해 **2~5초간 게임 루프 전체가 멈췄습니다.**
디스플레이, 진동모터, IR 수신이 모두 중단되는 문제가 있었습니다.

### 개선된 방식: 이벤트 + 비동기 스캔

```
[이벤트 핸들러] WiFi 끊김 감지
    → need_reconnect_scan = true  (플래그만 세팅, 즉시 반환)
    → 게임 루프 계속 실행

[1초 후] WifiManagerRun() 실행
    → WiFi.scanNetworksAsync(onDisconnectScanDone)  (non-blocking, 바로 반환)
    → 게임 루프 계속 실행

[스캔 완료] onDisconnectScanDone() 콜백
    → 가장 강한 AP 선택 → WiFi.begin()
    → AP 없으면 need_reconnect_scan = true → 1초 후 재시도
```

- `WiFi.onEvent()`로 연결/끊김을 **이벤트 기반**으로 감지합니다.
- 이벤트 핸들러 안에서는 **플래그 세팅만** 하고, 실제 처리는 메인 루프에서 합니다.
- `WiFi.setAutoReconnect(false)`로 ESP32 자동 재연결을 끄고 WifiManager가 직접 관리합니다.

---

## 3. 전송 실패 복구 (Pending Queue)

### 문제
WiFi가 끊겨있거나 HTTP 요청이 실패하면 `has2wifi.Send()`, `has2wifi.Situation()` 호출이 그냥 유실됩니다.
유령 전환, 생명칩 송수신, 배터리 알림 등이 서버에 반영되지 않는 원인이었습니다.

### SafeSend / SafeSituation

기존 `has2wifi.Send()` 대신 `SafeSend()`를 사용합니다.

```
SafeSend() 호출
    → WiFi 연결 & HTTP 성공  : 바로 전송
    → WiFi 끊김 or HTTP 실패 : Pending Queue에 저장
```

### Pending Queue

- 최대 **8개** 항목 저장
- 큐가 꽉 차면 **가장 오래된 항목을 밀어내고** 최신 항목 저장
- 항목 구조: `타입(Send/Situation)`, `device_name`, `key`, `value`

### RetryPending (2초마다 실행)

```
WiFi 연결 상태 확인
    → 연결 안됨: 종료
    → 연결됨: DrainQueue() 실행
        → 큐 항목 순서대로 재전송 시도
        → 성공한 항목만 큐에서 제거
        → 실패한 항목은 유지 → 2초 후 재시도
```

---

## 4. 재연결 후 서버 동기화

### 동기화 순서

WiFi가 재연결되면 아래 순서로 동기화합니다.

```
1. DrainQueue()     : 큐에 쌓인 미전송 데이터 먼저 서버에 반영
2. IsQueueEmpty()?
    → NO  : 아직 실패 항목 있음 → 동기화 보류, 2초 후 재시도
    → YES : 큐 완전히 비워짐
3. ReceiveMine()    : 서버에서 최신 상태 수신
4. DataChange()     : 수신한 상태 기기에 적용
```

### 왜 이 순서인가

큐를 먼저 비우지 않고 `ReceiveMine()`을 먼저 호출하면 아래 문제가 생깁니다.

```
예시: 생명칩 0 → 유령 전환 Send가 큐에 저장된 상태에서 재연결
    ReceiveMine() 먼저 호출 → 서버: role = player (아직 ghost 안 보냄)
    DataChange()            → 기기가 player 상태로 덮어씌워짐  ← 잘못된 동기화
```

큐 먼저 비우면:
```
    DrainQueue()  → ghost Send 전송 → 서버: role = ghost
    ReceiveMine() → ghost 수신
    DataChange()  → 기기 ghost 상태로 정상 반영  ← 올바른 동기화
```

---

## 타이머 요약

| 타이머 | 주기 | 함수 | 역할 |
|---|---|---|---|
| `wifi_timer` | 1초 | `WifiTimerFunc` | 서버 폴링 (Loop/DataChange) |
| `wifi_manager_timer` | 1초 | `WifiManagerRun` | RSSI 체크 & 로밍 & 재연결 |
| `retry_timer` | 2초 | `RetryPending` | 큐 재전송 & 재연결 후 동기화 |
| `battery_timer` | 60초 | `BatteryCheck` | 배터리 잔량 확인 |
| `ir_receive_timer` | 500ms | `IrReceive` | IR 수신 |
