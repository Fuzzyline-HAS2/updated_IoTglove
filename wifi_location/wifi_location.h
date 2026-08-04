#ifndef WIFI_LOCATION_H
#define WIFI_LOCATION_H

#include <Arduino.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "../firmware_version.h"
#include "../location_protocol.h"
#include "../secrets.h"
#include "../SecureOTA.h"

#define SERIAL1_RX_PIN 6
#define SERIAL1_TX_PIN 5
#define BEETLE_LED_PIN 10

#define BLE_SCAN_SECONDS 1
#define BLE_SCAN_INTERVAL 100
#define BLE_SCAN_WINDOW 100
#define BLE_MAX_SAMPLES 96
#define BLE_MAX_DEVICES 24
#define BLE_SAMPLE_WINDOW_MS 1500
#define BLE_DEVICE_EMA_KEEP_MS 5000
#define BLE_EVAL_INTERVAL_MS 200
#define BLE_SWITCH_HOLD_MS 1200
#define BLE_UNKNOWN_TIMEOUT_MS 5000
#define BLE_ROOM_REPEAT_MS 1000
#define BLE_MIN_DEVICE_SAMPLES 2

#define WIFI_CONNECT_TIMEOUT_MS 15000

#ifndef BEETLE_OTA_CHANNEL
#define BEETLE_OTA_CHANNEL "dev"
#endif

#ifndef BEETLE_OTA_MANIFEST_URL
#define BEETLE_OTA_MANIFEST_URL ""
#endif

#ifndef GLOVE_WIFI_PROFILE
#error "GLOVE_WIFI_PROFILE must be defined in secrets.h"
#endif

#define BEETLE_OTA_RELEASE_BASE_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download"
#define BEETLE_OTA_PRD_MANIFEST_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/latest/download/beetle-manifest-prd-" GLOVE_WIFI_PROFILE ".json"
#define BEETLE_OTA_DEV_MANIFEST_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/dev-latest/beetle-manifest-dev-" GLOVE_WIFI_PROFILE ".json"
#define BEETLE_OTA_RC_MANIFEST_URL "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/download/rc-latest/beetle-manifest-rc-" GLOVE_WIFI_PROFILE ".json"

#define STRINGIFY_VALUE(value) #value
#define STRINGIFY(value) STRINGIFY_VALUE(value)

enum GameState
{
  setting,
  ready,
  activate
};

struct BleSample
{
  char device_name[HAS3_DEVICE_NAME_BUFFER_SIZE];
  uint8_t room_index;
  int8_t rssi;
  unsigned long timestamp_ms;
};

struct BleDeviceState
{
  char device_name[HAS3_DEVICE_NAME_BUFFER_SIZE];
  uint8_t room_index;
  bool has_ema;
  float ema;
  unsigned long last_seen_ms;
};

struct BleRoomScore
{
  bool active;
  float score;
  uint8_t devices;
};

extern const char BEETLE_BUILD_ID[];
extern HardwareSerial MySerial1;
extern GameState game_state;
extern BLEScan *ble_scan;
extern BleSample ble_samples[BLE_MAX_SAMPLES];
extern uint8_t ble_sample_count;
extern BleDeviceState ble_devices[BLE_MAX_DEVICES];
extern int8_t stable_room_index;
extern int8_t candidate_room_index;
extern unsigned long candidate_started_ms;
extern unsigned long last_room_sent_ms;
extern unsigned long last_valid_has3_ms;
extern unsigned long ble_location_started_ms;
extern unsigned long last_ble_eval_ms;
extern bool ble_scan_segment_done;
extern SecureOTA beetle_ota;

bool TextEquals(const char *value, const char *expected);

void InitBleScanner();
void ResetBleLocationState();
void AddBleSample(const char *device_name, const char *room, int rssi, unsigned long timestamp_ms);
void ScanBleLocation();

void InitBeetleOta();
void StopBeetleWifi();
bool IsGithubCommand(const String &command);
bool IsValidOtaChannel(const String &channel);
bool IsSafeOtaTag(const String &tag);
void ParseOtaCommand(const String &command, String &channel, String &tag);
String ResolveBeetleManifestUrl(const String &channel, const String &tag);
bool EnsureWifiConnected();
void RunBeetleOta(const String &command);

void HandleTtgoCommand(const String &command);
void ReadTtgoCommands();

#endif
