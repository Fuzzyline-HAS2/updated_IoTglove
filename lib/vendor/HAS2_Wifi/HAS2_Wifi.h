/**
 * @file HAS2_Wifi.h
 * @author 김유빈
 * @brief
 * @version 1.0
 * @date 2022-09-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _HAS2_WIFI_
#define _HAS2_WIFI_

#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>  // wifi 관련 라이브러리
#include <ArduinoJson.h> // wifi 관련 라이브러리
#include <HTTPUpdate.h>  // OTA 관련 라이브러리

#include <Preferences.h>

void update_started();
void update_finished();
void update_progress(int cur, int total);
void update_error(int err);

// 전역변수를 main 파일에서 사용할 수 있게 extern 선언
extern HTTPClient http;
extern StaticJsonDocument<100> shift_machine;
extern StaticJsonDocument<1000> my;
extern StaticJsonDocument<1000> tag;
extern StaticJsonDocument<500> skill;

typedef struct HAS2_WifiCandidate
{
    const char *ssid;
    const char *password;
    int lastRssi;
    float avgRssi;
    int seenCount;
    unsigned long lastSeenMs;
} HAS2_WifiCandidate;


/**
 * @brief HAS2 전용 Wifi 라이브러리
 *
 */
class HAS2_Wifi
{
private:
    String HOST_NAME;
    String PHP_FILE_NAME;
    String server;
    String device_name;
    String my_mac;
    Preferences wifi_preferences;
    unsigned long lastWifiScanMs;
    bool wifiCandidatesInitialized;
    bool roamingEnabled = true;        // 기본 ON. SetRoamingEnabled(false)로 끄면 스캔/로밍 없이 setAutoReconnect에 맡김
    unsigned long lastConnectedMs = 0; // roaming OFF 워치독용. 마지막으로 연결돼 있던 시각

    friend class HTTPUpdate;

    bool HttpRequest(String request, String string_request);
    void JsonParsing(String request, String json);
    void EnsureWifiCandidatesInitialized();
    void ScanBadlandNetworks(bool force = false);
    bool TryConnect(const char *new_ssid, const char *new_password, unsigned long timeoutMs = 3000);
    bool TryConnectOrdered();
    bool TryConnectSaved();
    void SaveLastWifi(const char *new_ssid, const char *new_password);
    void MaintainWifi();
    void PrintConnectedWifi();

public:
    HAS2_Wifi();
    // HAS2_Wifi(String php);
    HAS2_Wifi(String host, String php = "/has2.php");

    void SetDebugPrint(Print *debugPrint);
    void SetRoamingEnabled(bool enabled); // false: 고정 AP 장치(스캔/로밍 off, 90s 워치독). 기본 true(현행 로밍)

    void Setup();
    void Setup(char *new_ssid, char *new_password);
    void Setup(String theme);
    void Connect(String theme);
    void Receive(String device_name);
    void ReceiveMP3(String device_name, int value);
    void ReceiveMine();
    void Send(String device_name, String column, String value);
    bool Situation(String affected_device_name, String situation, String key_device = "");
    void Loop();
    void Loop(void (*Func)(void));
    void FirmwareUpdate(String device_type, String ip_address = "172.30.1.43");
};

#endif
