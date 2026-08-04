#include "wifi_location.h"

#ifndef BEETLE_PIO_BUILD
#include "../SecureOTA.cpp"
#endif

const char BEETLE_BUILD_ID[] =
    "beetle=1"
    ";firmware=" FIRMWARE_VERSION
    ";wifi_profile=" GLOVE_WIFI_PROFILE
    ";code=" STRINGIFY(FIRMWARE_VERSION_CODE);

HardwareSerial MySerial1(1);
GameState game_state = setting;

BLEScan *ble_scan = nullptr;
BleSample ble_samples[BLE_MAX_SAMPLES];
uint8_t ble_sample_count = 0;
BleDeviceState ble_devices[BLE_MAX_DEVICES];
int8_t stable_room_index = -1;
int8_t candidate_room_index = -1;
unsigned long candidate_started_ms = 0;
unsigned long last_room_sent_ms = 0;
unsigned long last_valid_has3_ms = 0;
unsigned long ble_location_started_ms = 0;
unsigned long last_ble_eval_ms = 0;
bool ble_scan_segment_done = false;

SecureOTA beetle_ota(
    "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/latest/download/beetle-update-" GLOVE_WIFI_PROFILE ".bin",
    "https://raw.githubusercontent.com/Fuzzyline-HAS2/updated_IoTglove/main/version.txt",
    "https://github.com/Fuzzyline-HAS2/updated_IoTglove/releases/latest/download/beetle-update-" GLOVE_WIFI_PROFILE ".sig",
    HMAC_SECRET,
    FIRMWARE_VERSION_CODE);

void setup()
{
  Serial.begin(115200);
  MySerial1.begin(115200, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);
  MySerial1.setTimeout(20);

  pinMode(BEETLE_LED_PIN, OUTPUT);
  digitalWrite(BEETLE_LED_PIN, LOW);

  WiFi.mode(WIFI_OFF);
  InitBleScanner();
  ResetBleLocationState();
  InitBeetleOta();

  Serial.println(BEETLE_BUILD_ID);
  MySerial1.print("reset\n");
}

void loop()
{
  ReadTtgoCommands();
  ScanBleLocation();
}
