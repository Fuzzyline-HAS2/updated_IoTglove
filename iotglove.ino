/**
 * @file iotglove.ino
 * @author YuBin Kim
 * @brief HAS2_iotglove
 * @version 0.1.0
 * @date 2023-04-04
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "iotglove.h"

//************************************************ Core1
//********************************************************************
/**
 * @brief IoT Glove Intialize
 */
void IotGloveInit() {
  Serial.begin(115200);
  MySerial1.begin(115200, SERIAL_8N1, SERIAL1_RX_PIN,
                  SERIAL1_TX_PIN); // Beetle과 UART 통신 연결 세팅
  nexInit();                       // 디스플레이 세팅
  MySerial2.begin(9600, SERIAL_8N1, SERIAL2_RX_PIN, SERIAL2_TX_PIN);
  // has2wifi.Setup();     // 사무실 와이파이
  // has2wifi.Setup("city"); // 쌈지 시티 와이파이
  has2wifi.Setup("badland_ruins", "Code3824@"); // 쌈지 배드랜드 와이파이
  WifiManagerInit("badland");                   // WiFi 이벤트 핸들러 등록 (테마: badland / city)
  SensorInit(); // IoT Glove 사용 센서, 모듈 세팅
  TimerInit();  // 타이머 세팅
  BatteryCheck();
  has2wifi.Loop();
  DataChange();
}

/**
 * @brief 아두이노 기본 문법 (전원이 켜지면 한번만 실행)
 */
void setup() {
  delay(500);
  IotGloveInit();
}

/**
 * @brief 아두이노 기본 문법 (전원이 켜져있는동안 Core1에서 계속 실행)
 */
void loop() {
  TimerRun();
  BeetleScanWifi();

  if (game_state == activate) {
    ActivateFunc();
  }
}
