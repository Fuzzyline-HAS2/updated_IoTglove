/**
 * @file HAS2_Wifi.cpp
 * @author 김유빈
 * @brief
 * @version 1.0
 * @date 2022-09-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "HAS2_Wifi.h"

static String _activeHost = "http://172.30.1.43";
static Print *_has2DebugPrint = &Serial;
static const int BADLAND_WIFI_COUNT = 3;
static const int WIFI_MIN_RSSI = -70;
static const unsigned long WIFI_SCAN_INTERVAL_MS = 60000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 3000;
static const unsigned long WIFI_OFFLINE_REBOOT_MS = 90000; // roaming OFF 워치독: 연속 미연결 90s 초과 시 1회 리부팅
static const char *WIFI_PREF_NAMESPACE = "has2wifi";
static const char *WIFI_PREF_SSID = "last_ssid";
static const char *WIFI_PREF_PASSWORD = "last_pw";

static HAS2_WifiCandidate badland_wifi_candidates[BADLAND_WIFI_COUNT] = {
    {"badland_ruins", "Code3824@", -127, -127.0f, 0, 0},
    {"badland_auto", "Code3824@", -127, -127.0f, 0, 0},
    {"badland_shoot", "Code3824@", -127, -127.0f, 0, 0},
};

static void HAS2DebugPrintf(const char *format, ...)
{
  if (_has2DebugPrint == nullptr)
  {
    return;
  }

  char buffer[180];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  _has2DebugPrint->print(buffer);
}

/**
 * @brief 디버그 출력 대상을 설정 (기본값: Serial)
 *
 * @param debugPrint 디버그 메시지를 출력할 Print 객체 포인터. nullptr이면 출력 비활성화
 */
void HAS2_Wifi::SetDebugPrint(Print *debugPrint)
{
  _has2DebugPrint = debugPrint;
}

/**
 * @brief HAS2_Wifi 기본생성자
 *
 */
HAS2_Wifi::HAS2_Wifi()
    : HOST_NAME("http://172.30.1.43"),
      PHP_FILE_NAME("/has2.php"),
      server(HOST_NAME + PHP_FILE_NAME),
      lastWifiScanMs(0),
      wifiCandidatesInitialized(false)
{
}

// /**
//  * @brief HAS2_Wifi PHP 변경 생성자
//  *
//  * @param php 원하는 PHP 파일 입력["/test.php"]형식
//  */
// HAS2_Wifi::HAS2_Wifi(String php)
//     : HOST_NAME("http://172.30.1.43"),
//       PHP_FILE_NAME(php),
//       server(HOST_NAME + PHP_FILE_NAME)
// {
// }

/**
 * @brief HAS2_Wifi PHP 변경 생성자
 *
 * @param host 로컬호스트 주소 입력["http://172.30.1.59"]형식
 * @param php 원하는 PHP 파일 입력["/test.php"]형식
 */
HAS2_Wifi::HAS2_Wifi(String host, String php)
    : HOST_NAME(host),
      PHP_FILE_NAME(php),
      server(HOST_NAME + PHP_FILE_NAME),
      lastWifiScanMs(0),
      wifiCandidatesInitialized(false)
{
    _activeHost = host;
}

void HAS2_Wifi::EnsureWifiCandidatesInitialized()
{
  if (wifiCandidatesInitialized)
  {
    return;
  }

  for (int i = 0; i < BADLAND_WIFI_COUNT; i++)
  {
    badland_wifi_candidates[i].lastRssi = -127;
    badland_wifi_candidates[i].avgRssi = -127.0f;
    badland_wifi_candidates[i].seenCount = 0;
    badland_wifi_candidates[i].lastSeenMs = 0;
  }
  wifiCandidatesInitialized = true;
}

void HAS2_Wifi::ScanBadlandNetworks(bool force)
{
  EnsureWifiCandidatesInitialized();
  WiFi.mode(WIFI_STA);

  unsigned long now = millis();
  if (!force && lastWifiScanMs != 0 && now - lastWifiScanMs < WIFI_SCAN_INTERVAL_MS)
  {
    return;
  }
  lastWifiScanMs = now;

  int network_count = WiFi.scanNetworks();
  if (network_count < 0)
  {
    Serial.println("WiFi scan failed");
    return;
  }

  for (int i = 0; i < network_count; i++)
  {
    String found_ssid = WiFi.SSID(i);
    int found_rssi = WiFi.RSSI(i);

    for (int candidate_index = 0; candidate_index < BADLAND_WIFI_COUNT; candidate_index++)
    {
      HAS2_WifiCandidate &candidate = badland_wifi_candidates[candidate_index];
      if (found_ssid == candidate.ssid)
      {
        candidate.lastRssi = found_rssi;
        candidate.avgRssi = candidate.seenCount == 0 ? found_rssi : (candidate.avgRssi * 0.7f + found_rssi * 0.3f);
        candidate.seenCount++;
        candidate.lastSeenMs = now;
      }
    }
  }

  for (int i = 0; i < BADLAND_WIFI_COUNT - 1; i++)
  {
    for (int j = i + 1; j < BADLAND_WIFI_COUNT; j++)
    {
      bool should_swap = false;
      if (badland_wifi_candidates[j].seenCount > 0 && badland_wifi_candidates[i].seenCount == 0)
      {
        should_swap = true;
      }
      else if (badland_wifi_candidates[j].seenCount > 0 && badland_wifi_candidates[i].seenCount > 0 &&
               badland_wifi_candidates[j].avgRssi > badland_wifi_candidates[i].avgRssi)
      {
        should_swap = true;
      }

      if (should_swap)
      {
        HAS2_WifiCandidate temp = badland_wifi_candidates[i];
        badland_wifi_candidates[i] = badland_wifi_candidates[j];
        badland_wifi_candidates[j] = temp;
      }
    }
  }

  WiFi.scanDelete();
}

bool HAS2_Wifi::TryConnect(const char *new_ssid, const char *new_password, unsigned long timeoutMs)
{
  Serial.print("Try WiFi: ");
  Serial.println(new_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(new_ssid, new_password);

  unsigned long started_ms = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started_ms < timeoutMs)
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    SaveLastWifi(new_ssid, new_password);
    PrintConnectedWifi();
    return true;
  }

  Serial.println("WiFi connect failed");
  return false;
}

bool HAS2_Wifi::TryConnectOrdered()
{
  ScanBadlandNetworks(true);

  for (int i = 0; i < BADLAND_WIFI_COUNT; i++)
  {
    HAS2_WifiCandidate &candidate = badland_wifi_candidates[i];
    if (candidate.seenCount == 0 || candidate.lastSeenMs < lastWifiScanMs)
    {
      Serial.print("Skip unseen WiFi: ");
      Serial.println(candidate.ssid);
      continue;
    }

    if (candidate.avgRssi < WIFI_MIN_RSSI)
    {
      Serial.print("Skip weak WiFi: ");
      Serial.print(candidate.ssid);
      Serial.print(" RSSI=");
      Serial.println(candidate.avgRssi);
      continue;
    }

    if (TryConnect(candidate.ssid, candidate.password, WIFI_CONNECT_TIMEOUT_MS))
    {
      return true;
    }
  }

  return false;
}

bool HAS2_Wifi::TryConnectSaved()
{
  wifi_preferences.begin(WIFI_PREF_NAMESPACE, true);
  String saved_ssid = wifi_preferences.getString(WIFI_PREF_SSID, "");
  String saved_password = wifi_preferences.getString(WIFI_PREF_PASSWORD, "");
  wifi_preferences.end();

  if (saved_ssid.length() == 0 || saved_password.length() == 0)
  {
    return false;
  }

  Serial.print("Try saved WiFi: ");
  Serial.println(saved_ssid);
  return TryConnect(saved_ssid.c_str(), saved_password.c_str(), WIFI_CONNECT_TIMEOUT_MS);
}

void HAS2_Wifi::SaveLastWifi(const char *new_ssid, const char *new_password)
{
  wifi_preferences.begin(WIFI_PREF_NAMESPACE, false);
  wifi_preferences.putString(WIFI_PREF_SSID, new_ssid);
  wifi_preferences.putString(WIFI_PREF_PASSWORD, new_password);
  wifi_preferences.end();
}

void HAS2_Wifi::SetRoamingEnabled(bool enabled)
{
  roamingEnabled = enabled;
}

void HAS2_Wifi::MaintainWifi()
{
  // roaming OFF (고정 AP 장치): 스캔/로밍 없이 setAutoReconnect(드라이버)가 같은 AP로 무한 재연결.
  // 끊긴 채 WIFI_OFFLINE_REBOOT_MS 초과 시에만 워치독으로 1회 리부팅.
  if (!roamingEnabled)
  {
    unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED)
    {
      lastConnectedMs = now; // 연결 중이면 워치독 타이머 리셋
      return;
    }
    if (lastConnectedMs != 0 && now - lastConnectedMs >= WIFI_OFFLINE_REBOOT_MS)
    {
      Serial.println("WiFi offline too long. Restart ESP (watchdog)");
      ESP.restart();
    }
    return;
  }

  // roaming ON (현행): 주기 스캔 + RSSI 로밍, 1패스 실패 시 즉시 리부팅.
  ScanBadlandNetworks(false);

  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.println("WiFi disconnected. Reconnecting...");
  if (!TryConnectOrdered())
  {
    Serial.println("Restart ESP");
    ESP.restart();
  }
}

void HAS2_Wifi::PrintConnectedWifi()
{
  Serial.println("WiFi connected");
  Serial.print("Connected SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("Connected RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Wifi 연결 및 초기설정
 *
 */
void HAS2_Wifi::Setup()
{
  if (!TryConnectSaved() && !TryConnectOrdered())
  {
    Serial.println("Restart ESP");
    ESP.restart();
  }
  delay(1000);

  my_mac = WiFi.macAddress();
  Serial.print("MY MAC=");
  Serial.println(my_mac);

  ReceiveMine();

  Serial.print("DeviceName : ");
  Serial.println((const char *)my["device_name"]);
}

void HAS2_Wifi::Connect(String theme)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  if (!TryConnectOrdered())
  {
    Serial.println("Restart ESP");
    ESP.restart();
  }
}

/**
 * @brief Wifi 연결 및 초기설정
 *
 */
void HAS2_Wifi::Setup(char *new_ssid, char *new_password)
{
  Serial.print("SSID : ");
  Serial.println((const char *)new_ssid);
  my_mac = WiFi.macAddress();
  Serial.print("MY MAC=");
  Serial.println(my_mac);

  if (!TryConnect((const char *)new_ssid, (const char *)new_password, WIFI_CONNECT_TIMEOUT_MS))
  {
    Serial.println("Restart ESP");
    ESP.restart();
  }
  delay(1000);

  my_mac = WiFi.macAddress();
  Serial.print("MY MAC=");
  Serial.println(my_mac);

  ReceiveMine();

  Serial.print("DeviceName : ");
  Serial.println((const char *)my["device_name"]);
}

void HAS2_Wifi::Setup(String theme)
{
  Serial.print("WiFi theme: ");
  Serial.println(theme);

  if (!TryConnectSaved() && !TryConnectOrdered())
  {
    Serial.println("Restart ESP");
    ESP.restart();
  }
  delay(1000);

  my_mac = WiFi.macAddress();
  Serial.print("MY MAC=");
  Serial.println(my_mac);

  ReceiveMine();

  Serial.print("DeviceName : ");
  Serial.println((const char *)my["device_name"]);
}
/**
 * @brief 다른 장치의 데이터를 읽음
 *
 * @param device_name 다른 장치의 이름
 */
void HAS2_Wifi::Receive(String device_name)
{
  String string_request = server + "?request=" + "Receive" + "&table=" + "device" + "&key=" + device_name;
  HttpRequest("Receive", string_request);
}

/**
 * @brief [메인 프로그램 전용] mp3재생 장치의 데이터를 읽음
 *
 * @param device_name 장치의 이름
 * @param value mp3 재생 순서
 */
void HAS2_Wifi::ReceiveMP3(String device_name, int value)
{
  String string_request = server + "?request=" + "ReceiveMP3" + "&table=" + "device" + "&key=" + device_name + "&value=" + value;
  HttpRequest("Receive", string_request);
}

/**
 * @brief 원하는 장치의 데이터를 수정
 *
 * @param device_name 데이터 변경을 당하는 장치의 이름
 * @param column 변경할 데이터의 컬럼
 * @param value 변경할 데이터의 값
 */
void HAS2_Wifi::Send(String device_name, String column, String value)
{
  String string_request = server + "?request=" + "Send" + "&table=" + "device" + "&key=" + device_name + "&column=" + column + "&value=" + value;
  HttpRequest("Send", string_request);
}

/**
 * @brief
 *
 * @param affected_device_name 영향을 받는 장치
 * @param situation  상황
 */
void HAS2_Wifi::Situation(String affected_device_name, String situation, String key_device)
{
  String key = key_device.length() ? key_device : (String)(const char *)my["device_name"];
  String string_request = server + "?request=" + "Situation" + "&table=" + situation + "&key=" + key + "&value=" + affected_device_name;
  HttpRequest("Send", string_request);
}

/**
 * @brief 자신의 데이터를 읽음
 *
 */
void HAS2_Wifi::ReceiveMine()
{
  String string_request = server + "?request=" + "ReceiveMine" + "&table=" + "device" + "&mac=" + my_mac;
  HttpRequest("ReceiveMine", string_request);
}

/**
 * @brief 반복적으로 ShfitMachin의 데이터를 읽음
 *
 */
void HAS2_Wifi::Loop()
{
  MaintainWifi();

  String string_request = server + "?request=" + "Loop" + "&table=" + "device" + "&mac=" + my_mac;
  HttpRequest("Loop", string_request);
  if ((int)shift_machine["shift_machine"] >= 1)
  {
    ReceiveMine();
  }
  if ((int)shift_machine["watchdog"] >= 1)
  {
    Send((String)(const char *)my["device_name"], "watchdog", "0");
    ESP.restart();
  }
}

/**
 * @brief 반복적으로 ShfitMachin의 데이터를 읽음
 *
 * @param Func shift_machine가 1 이상이 되면 실행되는 함수포인터
 */
void HAS2_Wifi::Loop(void (*Func)(void))
{
  MaintainWifi();

  String string_request = server + "?request=" + "Loop" + "&table=" + "device" + "&mac=" + my_mac;
  HttpRequest("Loop", string_request);
  if ((int)shift_machine["shift_machine"] >= 1)
  {
    ReceiveMine();
    Func();
  }
  if ((int)shift_machine["watchdog"] >= 1)
  {
    Send((String)(const char *)my["device_name"], "watchdog", "0");
    ESP.restart();
  }
  if ((String)(const char *)my["device_state"] == "update")
  {
    FirmwareUpdate((String)(const char *)my["device_type"], HOST_NAME.substring(7));
  }
}

/**
 * @brief [private] Http 통신
 *
 * @param request 원하는 명령
 * @param string_request Http에게 보내는 형식 문자열
 */
void HAS2_Wifi::HttpRequest(String request, String string_request)
{
  // WiFi 끊김 중에는 서버 요청을 스킵한다. 어차피 못 닿으므로, 무응답 대기(기본 ~5s TCP 타임아웃)로
  // 메인 루프가 블로킹돼 IR/진동/디스플레이가 버벅이는 것을 막는다. 연결되면 다음 호출에서 정상 재개.
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  //   int httpRequestCnt = 0;
  // ReRequsetHttp:

  http.begin(string_request); // 요청을 PHP로 전송

  int httpcode = http.GET();

  if (httpcode > 0)
  {
    if (httpcode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      if (request != "Loop")
      {
        Serial.println(payload);
      }
      if (request != "Send")
      {
        JsonParsing(request, payload);
      }
    }
    else
    {
      Serial.printf("HTTP GET... code: %d\n", httpcode);
    }
  }
  else
  {
    Serial.printf("HTTP GET... failed, error: %s\n", http.errorToString(httpcode).c_str());
    // if(httpRequestCnt < 2){
    //   Serial.printf("HTTP GET... failed, error: %s\n", http.errorToString(httpcode).c_str());
    //   Serial.printf("Rerequest count: %d\n",httpRequestCnt);
    //   httpRequestCnt++;
    //   goto ReRequsetHttp;
    // }
    // else{
    //   Serial.printf("HTTP GET... failed, error: %s\n", http.errorToString(httpcode).c_str());
    //   Serial.printf("Maximum request exceed!");
    // }
  }
  http.end();
}

/**
 * @brief [private] Json 디코딩하여 원하는 데이터를 읽어옴
 *
 * @param request 원하는 명령
 * @param json String 형식의 json 파일
 */
void HAS2_Wifi::JsonParsing(String request, String json)
{
  if (request == "Loop")
  {
    auto error = deserializeJson(shift_machine, json);
    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
    }
  }
  else if (request == "ReceiveMine")
  {
    auto error = deserializeJson(my, json);
    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
    }
  }
  else if (request == "Receive")
  {
    auto error = deserializeJson(tag, json);
    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
    }
  }
}

void HAS2_Wifi::FirmwareUpdate(String device_type, String ip_address)
{
  WiFiClient client;

  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);

  String bin_file_name = "/" + device_type + ".bin";
  Serial.println(bin_file_name);
  t_httpUpdate_return ret = httpUpdate.update(client, ip_address, 80, bin_file_name);

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    break;
  }
}

void update_started()
{
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished()
{
  HAS2_Wifi has2_wifi(_activeHost);
  has2_wifi.Send((String)(const char *)my["device_name"], "device_state", "setting");
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total)
{
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err)
{
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

// 전역변수 선언
HTTPClient http;
StaticJsonDocument<100> shift_machine;
StaticJsonDocument<1000> my;
StaticJsonDocument<1000> tag;
StaticJsonDocument<500> skill;
