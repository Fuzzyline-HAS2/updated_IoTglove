#define __DEBUG__

#ifndef _IOTGLOVE_H_
#define _IOTGLOVE_H_

#include "library_and_pin.h"

//============================ Global Variable ============================
String wifi_name;

bool hacking_state;
bool hacking;
bool revival;
bool breath_hold;
bool lifechip_send;
bool lifechip_receive;
bool motor_on;
String ir_decode_data;

typedef enum GameState { setting, ready, activate };
GameState game_state = setting;

//============================ Hardware Serial ============================
HardwareSerial MySerial1(1); // Beetle
HardwareSerial MySerial2(2); // Display

//=============================== Display ===============================
void DisplaySet();
void DisplayCheck();
void NextionReceived(String nextion_string);
void PageChange(String page);

//*=============================== Sensor ===============================*
/**
 * @brief IoT 글러브에 사용되는 센서, 모듈 세팅
 */
void SensorInit();

//=============================== Beetle =================================
void BeetleScanWifi();

//================================ Wifi ==================================
HAS2_Wifi has2wifi("http://172.30.1.43");

bool activate_bool;

void SettingFunc();
void ReadyFunc();
void ActivateFunc();
void ActivateRunOnce();
void DataChange();

//=============================== Neopixel ===============================
#define NUMPIXELS 4

int standard_neo = 20;
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Neopixel 색상정보
int white[3] = {standard_neo, standard_neo, standard_neo};
int red[3] = {standard_neo, 0, 0};
int yellow[3] = {standard_neo, standard_neo, 0};
int green[3] = {0, standard_neo, 0};
int purple[3] = {standard_neo, 0, standard_neo};
int blue[3] = {0, 0, standard_neo};

//================================== IR ==================================
#define DECODE_NEC_
IRsend irsend(IR_SEND_PIN);
IRrecv irrecv(IR_RECEIVE_PIN);

decode_results results;

uint64_t ir_send_data = 0;
bool ir_receive_error = false;

void IrInit();
void IrSendDataSetup(char device_name[]);
void IrSend();
void IrReceive();
String IrDecoding(uint32_t ir_data);

//=========================== Vibration Motor ===========================
const int MotorFreq = 5000;
const int MotorResolution = 8;
const int MotorLedChannel = 3;
const int VibrationLedChannel = 4;
const int MotorMAX_DUTY_CYCLE = (int)(pow(2, MotorResolution) - 1);

#define ARRAYINDEX(X) sizeof(X) / sizeof(int)

#define vibration_mode0 0
#define vibration_mode1 130
#define vibration_mode2 180
#define vibration_mode3 210
#define vibration_mode4 230
#define vibration_mode5 240

const int vibration_pattern_1[] = {1, 0, 0, 1, 5, 2, 0, 0, 0,
                                   1, 2, 1, 0, 0, 0, 0, 0, 0}; // BPM 83
const int vibration_pattern_2[] = {1, 0, 1, 5, 3, 0,
                                   0, 1, 2, 1, 0, 0}; // BPM 125
const int vibration_pattern_3[] = {5, 5, 5, 5, 5, 5, 5};
const int vibration_pattern_4[] = {
    1, 2, 1, 0, 0, 1, 5, 2, 1, 0, 1, 2, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // BPM 56

void MotorInit();
int Intensity(int intensity);
void MotorOn(const int *vibration_pattern, int len);
void MotorStop();

//=============================== QRD1114 ===============================
/**
 * @brief QRD1114 근접센서를 통한 근접거리 측정
 *
 * @return float 측정값 (0.1 ~ 20.1 [mm])
 */
float DistanceCheck();

//=============================== Battery ===============================
Pangodream_18650_CL BL(ADC_PIN, CONV_FACTOR, READS);

void BatteryCheck();

//================================ Timer ================================
// 타이머 사용 선언
SimpleTimer ir_receive_timer; // ir 수신 타이머
SimpleTimer player_ir_send_timer;
SimpleTimer hacking_timer; // ir 수신 타이머
SimpleTimer wifi_timer;
SimpleTimer neopixel_timer;
SimpleTimer battery_timer;      // 배터리 주기적 확인 타이머
SimpleTimer retry_timer;        // WiFi 끊김 시 재전송 타이머
SimpleTimer wifi_manager_timer; // RSSI 체크 및 로밍 타이머

void TimerInit();
void TimerRun();
void WifiTimerFunc();

// 타이머 설정 관련 함수
int ir_receive_timer_id;
int player_ir_send_timer_id;
int hacking_timer_id;
int wifi_timer_id;
int neopixel_timer_id;
int battery_timer_id;
int retry_timer_id;
int wifi_manager_timer_id;

//============================== Wifi Recovery ==============================
volatile bool just_reconnected = false; // 재연결 직후 플래그 (wifi_manager → wifi_recovery)

#define PENDING_QUEUE_SIZE 8
enum ActionType { ACTION_SEND, ACTION_SITUATION };
struct PendingAction {
    ActionType type;
    String device_name;
    String key;
    String value;
    bool active;
};

void SafeSend(String device, String column, String value);
void SafeSituation(String device, String situation);
void RetryPending();

//============================== Wifi Manager ==============================
void WifiManagerInit(String theme);
void WifiManagerRun();

//================================ Neo =================================
void tagger_blink();
#endif