#define __DEBUG__

#ifndef _IOTGLOVE_H_
#define _IOTGLOVE_H_

#include "library_and_pin.h"
#include "location_protocol.h"

//============================ Global Variable ============================
String wifi_name;

bool hacking;
int hack_count = 0;
#define HACK_THRESHOLD 1
#define REVIVAL_HELP_THRESHOLD 1
bool revival;
bool motor_on;
int vibe_level = 0; // 서버 vibe 값(0=끔, 1~5=진동 패턴 레벨)
bool pending_revival_vibration;
String ir_decode_data;
char current_hmi_page[20];

#define REVIVAL_HELP_RECORDS 10
#define REVIVAL_TICK_PER_SEC 1 // = 1000 / pgGhost.tScreenRefresh.tim (tim=1000ms → 초당 1 tick)
#define REVIVAL_FINISH_DISPLAY_MS 1500
#define TAG_CAPTURE_MIN_MS 3000
#define TAG_CAPTURE_TIMEOUT_MS 5000
#define SUR_CONNECT_MIN_MS 3000
#define SUR_CONNECT_TIMEOUT_MS 5000
#define PAGE_CHANGE_VIBRATION_ON_MS 120
#define PAGE_CHANGE_VIBRATION_GAP_MS 80
#define PAGE_CHANGE_VIBRATION_TOTAL_MS (PAGE_CHANGE_VIBRATION_ON_MS * 2 + PAGE_CHANGE_VIBRATION_GAP_MS)
#define PAGE_CHANGE_VIBRATION_INTENSITY 3
#define DEFAULT_REVIVAL_TIME_SEC 30
#define DEBUG_TELNET_PORT 23
#define ROLE_SEND_RETRY_MS 2000
#define BEETLE_OTA_TIMEOUT_MS 180000
#define IR_SEND_INTERVAL_MS 150 // 기준 송신 간격. 다중 글러브 환경에서 채널 점유율을 낮춰 충돌 완화 (100→150)
#define IR_SEND_JITTER_MS 100   // 랜덤 지터 폭. 실효 간격=150~250ms로 흔들어 글러브 간 lock-step 충돌 분산
#define IR_TAG_CACHE_TTL_MS 1000 // 같은 송신자 IR 연속 수신 시 서버 조회를 1초간 재사용해 태그 판정 지연을 줄임
#define IR_ROLE_PLAYER 0
#define IR_ROLE_GHOST 1
#define IR_ROLE_TAGGER 2
#define IR_ROLE_UNKNOWN 255
#define IR_ROLE_SHIFT 4
#define IR_PLAYER_MASK 0x0F
#define BEETLE_RX_BUFFER_SIZE 32
#define BOOT_SERIAL_BAUD 921600 // USB 디버그 Serial 보율 (Serial.begin(921600))
#define NEXTION_TFT_STARTUP_WINDOW_MS 1500
#define BOOT_WIFI_RETRY_MS 15000
#define BOOT_WIFI_STATUS_MS 1000
#define BOOT_WIFI_RETRY_PAUSE_MS 250
#define VERSION_REPORT_RETRY_MS 5000
#define VERSION_REPORT_MAX_ATTEMPTS 10
#define VERSION_REPORT_SUCCESS_TARGET 3

struct RevivalHelpRecord
{
    String device_name;
    unsigned long expires_at;
};

RevivalHelpRecord revival_help_records[REVIVAL_HELP_RECORDS];
bool revival_timer_active = false;
bool revival_finish_page = false;
bool revival_finish_sent = false;
unsigned long revival_started_ms = 0;
unsigned long revival_bonus_ms = 0;
unsigned long revival_finish_started_ms = 0;
unsigned long last_role_send_ms = 0;
int last_revival_cooldown_count = 0;
bool tag_capture_pending = false;
unsigned long tag_capture_started_ms = 0;
bool sur_connect_pending = false;
unsigned long sur_connect_started_ms = 0;
bool page_change_vibration_active = false;
unsigned long page_change_vibration_started_ms = 0;
bool beetle_ota_pending = false;
unsigned long beetle_ota_started_ms = 0;
String pending_ota_channel;
String pending_ota_tag;
bool version_report_pending = false;
unsigned long version_report_next_ms = 0;
uint8_t version_report_attempts = 0;
uint8_t version_report_successes = 0;
unsigned long last_ir_send_ms = 0;
unsigned long next_ir_interval_ms = IR_SEND_INTERVAL_MS; // 다음 송신까지 실효 간격(지터 포함). 매 송신 후 재계산
char beetle_rx_buffer[BEETLE_RX_BUFFER_SIZE];
uint8_t beetle_rx_len = 0;
bool beetle_rx_overflow = false;
String last_sent_location;
String pending_location;
unsigned long last_location_send_fail_at = 0;
bool location_vibe_muted = false;

typedef enum GameState
{
    setting,
    ready,
    activate
};
GameState game_state = setting;

//============================ Hardware Serial ============================
HardwareSerial MySerial1(1); // Beetle
HardwareSerial MySerial2(2); // Display

//=============================== Display ===============================
void DisplaySet();
void DisplayCheck();
void PageChange(const char *page);
void SendHmiValue(const char *target, int value);
void UpdateHmiLanguage();
void UpdateHmiBattery();
void UpdateHmiResults();
void AddRevivalGaugeBonus(int seconds);
bool ReadNextionVersion(uint32_t *version);

//========================== Nextion TFT Upload ==========================
void NextionTftUploadInit();
void NextionTftUploadUseBootSerial();
bool NextionTftUploadStartupWindow(unsigned long window_ms);
bool NextionTftUploadPoll();
void NextionTftUploadRestoreDisplaySerial();

//=============================== Debug ===============================
void DebugInit();
void DebugPoll();
Print *DebugOutput();
void DebugPrint(const char *value);
void DebugPrint(const String &value);
void DebugPrint(unsigned long value);
void DebugPrintln();
void DebugPrintln(const char *value);
void DebugPrintln(const String &value);
void DebugPrintln(unsigned long value);
void DebugPrintln(unsigned long value, int base);
void DebugPrintf(const char *format, ...);

extern const char FIRMWARE_BUILD_ID[];

//*=============================== Sensor ===============================*
/**
 * @brief IoT 글러브에 사용되는 센서, 모듈 세팅
 */
void SensorInit();
void BeetleScanWifi();

//================================ Wifi ==================================
#include <esp_wifi.h>

// WiFi direct connection target. Keep actual values in local secrets.h.
#ifndef GLOVE_WIFI_SSID
#define GLOVE_WIFI_SSID "badland_shoot"
#endif

#ifndef GLOVE_WIFI_PASS
#define GLOVE_WIFI_PASS "Code3824@"
#endif

static char glove_ssid[] = GLOVE_WIFI_SSID;
static char glove_pass[] = GLOVE_WIFI_PASS;

// 약신호 링크 안정화(최대 TX 파워 / 모뎀 슬립 off / 자동 재연결). WiFi.begin 전에 호출.
void WifiForceLowRateInit();
void StartVersionReport();
void UpdateVersionReport();
bool ReportDeviceVersions();

HAS2_Wifi has2wifi("http://172.30.1.43");

//================================ OTA ==================================
#ifndef OTA_CHANNEL
#define OTA_CHANNEL "dev"
#endif

#ifndef OTA_MANIFEST_URL
#define OTA_MANIFEST_URL ""
#endif

#define OTA_RELEASE_BASE_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download"
#define OTA_PRD_MANIFEST_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/latest/download/manifest-prd.json"
#define OTA_DEV_MANIFEST_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/dev-latest/manifest-dev.json"
#define OTA_RC_MANIFEST_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/rc-latest/manifest-rc.json"

SecureOTA ota(
  "https://raw.githubusercontent.com/Fuzzyline-HAS2/updated_IoTglove/main/update.bin",
  "https://raw.githubusercontent.com/Fuzzyline-HAS2/updated_IoTglove/main/version.txt",
  "https://raw.githubusercontent.com/Fuzzyline-HAS2/updated_IoTglove/main/update.sig",
  HMAC_SECRET,
  FIRMWARE_VERSION_CODE
);

void SettingFunc();
void ReadyFunc();
void ActivateFunc();
void ActivateRunOnce();
void DataChange();
const char *CurrentRole();
bool TextEquals(const char *value, const char *expected);
bool IsPlayerRole(const char *role);
bool IsTaggerRole(const char *role);
bool IsRevivalRole(const char *role);
bool IsRevivalRole(const String &role);
void ResetRevivalTimer();
void StartRevivalTimer();
void UpdateRevivalTimer();
void UpdateTagCaptureFlow();
void StartSurConnect();
void StopSurConnect();
void UpdateSurConnectFlow();
void ShowRolePage();
void ApplyRevivalCooldownChange(int previous_count);
bool IsFinalLifeTaken();
void StartPendingRevivalVibration();
void StopPendingRevivalVibration();
void StartPageChangeVibration();
void UpdateVibration();
bool IsGithubDeviceState(const char *device_state);
void ParseOtaDeviceState(const char *device_state, String &channel, String &tag);
void StartBeetleOtaThenTtgoOta(const char *device_state);
void UpdateBeetleOtaFlow();
void CancelBeetleOtaWait();
void FinishBeetleOtaWaitAndRunTtgoOta(const char *reason);

//=============================== Neopixel ===============================
#define NUMPIXELS 4
#define DEFAULT_BRIGHTNESS 50

int ledBrightness = DEFAULT_BRIGHTNESS;
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Neopixel 색상정보
int white[3]  = {255, 255, 255};
int red[3]    = {255, 0,   0  };
int yellow[3] = {255, 255, 0  };
int green[3]  = {0,   255, 0  };
int skyblue[3] = {0,   180, 255};
int purple[3] = {255, 0,   255};
int blue[3]   = {0,   0,   255};

void lightColor(int color[]);
void UpdateBrightness();

//================================== IR ==================================
IRsend irsend(IR_SEND_PIN);
IRrecv irrecv(IR_RECEIVE_PIN);

decode_results results;

uint64_t ir_send_data = 0;
bool ir_receive_error = false;
bool ir_send_data_valid = false;
String ir_send_device_name;
uint8_t ir_send_role_code = IR_ROLE_UNKNOWN;
uint8_t ir_decode_role = IR_ROLE_UNKNOWN;

void IrInit();
uint8_t IrRoleCode(const char *role);
const char *IrRoleName(uint8_t role_code);
void IrSendDataSetup(String device_name);
void IrSend();
void IrReceive();
String IrDecoding(uint32_t ir_data);
void ClearRevivalHelpRecords();
bool ShouldSendRevivalCooldown(String device_name, unsigned long ttl_ms);

//=========================== Vibration Motor ===========================
const int MotorFreq = 5000;
const int MotorResolution = 8;
#define ARRAYINDEX(X) sizeof(X) / sizeof(int)

#define vibration_mode0 0
#define vibration_mode1 130
#define vibration_mode2 180
#define vibration_mode3 210
#define vibration_mode4 230
#define vibration_mode5 240

// 진동 패턴 5단계 — 모두 L3와 같은 심장박동(lub-dub) 모티프.
// 구조: {주박동(상승→강타) | 휴식 | 약한 2차 박동 | 휴식}. 값은 세기 레벨(0~5, Intensity()로 PWM 변환).
// 레벨↑ = BPM↑(step_ms↓) + 세기↑ → "위험할수록 심장이 빠르고 세게 뛰는" 체감. L3가 BPM 125 기준점.
const int vibration_pattern_1[] = {1, 0, 1, 3, 2, 0, 0, 0, 1, 0, 0, 0};       // BPM  75 약하고 느린 두근
const int vibration_pattern_2[] = {1, 0, 1, 4, 2, 0, 0, 1, 1, 1, 0, 0};       // BPM 100 차분한 두근
const int vibration_pattern_3[] = {1, 0, 1, 5, 3, 0, 0, 1, 2, 1, 0, 0};       // BPM 125 또렷한 두근(기존)
const int vibration_pattern_4[] = {2, 0, 2, 5, 4, 0, 0, 1, 3, 1, 0, 0};       // BPM 150 강해진 두근
const int vibration_pattern_5[] = {3, 0, 3, 5, 5, 0, 0, 2, 4, 2, 0, 0};       // BPM 175 쿵쿵 몰아치는 심장

struct VibePattern
{
    const int *pattern;
    int len;
    int step_ms;
};

const VibePattern VIBE_PATTERNS[] = {
    {vibration_pattern_1, (int)ARRAYINDEX(vibration_pattern_1), 200}, // L1 BPM  75
    {vibration_pattern_2, (int)ARRAYINDEX(vibration_pattern_2), 150}, // L2 BPM 100
    {vibration_pattern_3, (int)ARRAYINDEX(vibration_pattern_3), 120}, // L3 BPM 125
    {vibration_pattern_4, (int)ARRAYINDEX(vibration_pattern_4), 100}, // L4 BPM 150
    {vibration_pattern_5, (int)ARRAYINDEX(vibration_pattern_5), 86},  // L5 BPM 175
};
#define VIBE_PATTERN_COUNT ((int)(sizeof(VIBE_PATTERNS) / sizeof(VibePattern)))
#define REVIVAL_VIBE_LEVEL 3 // 부활 대기 진동은 기존 심장박동(L3)으로 재생

void MotorInit();
int Intensity(int intensity);
void MotorStep(int level); // level 1~VIBE_PATTERN_COUNT 패턴을 비블로킹으로 한 스텝씩 재생
void MotorStop();

//=============================== Battery ===============================
Pangodream_18650_CL BL(ADC_PIN, CONV_FACTOR, READS);

void BatteryCheck();

//================================ Timer ================================
// 타이머 사용 선언
SimpleTimer ir_receive_timer; // ir 수신 타이머
SimpleTimer wifi_timer;
SimpleTimer neopixel_timer;
SimpleTimer battery_timer;

void TimerInit();
void TimerRun();
void WifiTimerFunc();

// 타이머 설정 관련 함수
int ir_receive_timer_id;
int wifi_timer_id;
int neopixel_timer_id;
int battery_timer_id;

//================================ Neo =================================
void ota_success_blink();
void tagger_blink();
#endif
