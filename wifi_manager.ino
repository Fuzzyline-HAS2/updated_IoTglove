#include "iotglove.h"

// ======================== AP 목록 ========================
const char *BADLAND_SSIDS[] = {
    "badland_ruins", "badland_shoot", "badland_prison",
    "badland_check", "badland_auto"
};
const char *CITY_SSIDS[] = {
    "HAS2_food", "HAS2_office", "HAS2_gun",
    "HAS2_bar", "HAS2_house", "tp-link"
};
const char *WIFI_PASSWORD = "Code3824@";

// 현재 테마 AP 목록 (WifiManagerInit에서 설정)
const char **CURRENT_SSIDS      = BADLAND_SSIDS;
int          CURRENT_SSID_COUNT = 5;

// ======================== RSSI 임계값 ========================
#define RSSI_STABLE    -67   // 이상: 유지
#define RSSI_WATCHING  -75   // ~ -67: 관찰
#define RSSI_ROAM_GAIN  8    // 현재보다 8dB 이상 강한 AP 발견 시 로밍

// ======================== 상태머신 ========================
enum WifiManagerState { WM_STABLE, WM_WATCHING, WM_SCANNING, WM_ROAMING, WM_DISCONNECTED };
WifiManagerState wm_state = WM_STABLE;

// 의도적 로밍 중 플래그 (로밍 시 disconnect 이벤트 무시용)
bool is_intentional_roam = false;

// ======================== 유틸 ========================
bool isKnownAP(String ssid)
{
    for (int i = 0; i < CURRENT_SSID_COUNT; i++)
        if (ssid == CURRENT_SSIDS[i]) return true;
    return false;
}

// ======================== 비동기 스캔 콜백 (로밍용) ========================
void onScanDone(int numNetworks)
{
    if (numNetworks <= 0)
    {
        wm_state = WM_WATCHING;
        WiFi.scanDelete();
        return;
    }

    int    current_rssi = WiFi.RSSI();
    String current_ssid = WiFi.SSID();
    String best_ssid    = "";
    int    best_rssi    = -200;

    for (int i = 0; i < numNetworks; i++)
    {
        String ssid = WiFi.SSID(i);
        int    rssi = WiFi.RSSI(i);
        // 현재 AP 제외하고 badland AP 중 가장 강한 것 탐색
        if (isKnownAP(ssid) && ssid != current_ssid && rssi > best_rssi)
        {
            best_rssi = rssi;
            best_ssid = ssid;
        }
    }
    WiFi.scanDelete();

    if (best_ssid != "" && best_rssi > current_rssi + RSSI_ROAM_GAIN)
    {
        Serial.println("[WifiManager] Roaming: " + current_ssid + "(" + String(current_rssi) +
                       ") -> " + best_ssid + "(" + String(best_rssi) + ")");
        is_intentional_roam = true;
        wm_state = WM_ROAMING;
        WiFi.begin(best_ssid.c_str(), WIFI_PASSWORD);
    }
    else
    {
        Serial.println("[WifiManager] No better AP found. Staying on " + current_ssid);
        wm_state = WM_WATCHING;
    }
}

// ======================== WiFi 이벤트 핸들러 ========================
// 주의: 이벤트 핸들러에서는 플래그 세팅만. 무거운 작업 금지
void WiFiEventHandler(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("[WifiManager] Connected - IP: " + WiFi.localIP().toString() +
                       "  SSID: " + WiFi.SSID());
        just_reconnected    = true;
        is_intentional_roam = false;
        wm_state            = WM_STABLE;
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (!is_intentional_roam)
        {
            // 의도치 않은 끊김
            Serial.println("[WifiManager] Disconnected unexpectedly.");
            wm_state = WM_DISCONNECTED;
        }
        // 의도적 로밍 중 발생한 disconnect는 무시 (GOT_IP에서 처리)
        break;

    default:
        break;
    }
}

// ======================== 끊긴 상태 재연결 ========================
void handleDisconnected()
{
    Serial.println("[WifiManager] Reconnecting...");

    // 어차피 끊긴 상태라 blocking scan 해도 HTTP 영향 없음
    int    n         = WiFi.scanNetworks();
    String best_ssid = "";
    int    best_rssi = -200;

    for (int i = 0; i < n; i++)
    {
        if (isKnownAP(WiFi.SSID(i)) && WiFi.RSSI(i) > best_rssi)
        {
            best_rssi = WiFi.RSSI(i);
            best_ssid = WiFi.SSID(i);
        }
    }
    WiFi.scanDelete();

    if (best_ssid != "")
    {
        Serial.println("[WifiManager] Best AP: " + best_ssid + " (" + String(best_rssi) + " dBm)");
        WiFi.begin(best_ssid.c_str(), WIFI_PASSWORD);
    }
    else
    {
        Serial.println("[WifiManager] No badland AP found.");
    }
}

// ======================== 초기화 ========================
void WifiManagerInit(String theme)
{
    if (theme == "city")
    {
        CURRENT_SSIDS      = CITY_SSIDS;
        CURRENT_SSID_COUNT = 6;
    }
    else // badland (기본값)
    {
        CURRENT_SSIDS      = BADLAND_SSIDS;
        CURRENT_SSID_COUNT = 5;
    }
    WiFi.onEvent(WiFiEventHandler);
}

// ======================== 3초마다 타이머에서 호출 ========================
void WifiManagerRun()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        if (wm_state != WM_ROAMING) // 의도적 로밍 중이면 건드리지 않음
            handleDisconnected();
        return;
    }

    // 연결 상태 - RSSI 체크 및 상태 전환
    int rssi = WiFi.RSSI();
    Serial.println("[WifiManager] RSSI: " + String(rssi) + " dBm  SSID: " + WiFi.SSID());

    if (rssi > RSSI_STABLE)
    {
        wm_state = WM_STABLE;
    }
    else if (rssi > RSSI_WATCHING)
    {
        wm_state = WM_WATCHING;
    }
    else
    {
        // < -75 dBm: 로밍 후보 비동기 스캔 시작
        if (wm_state != WM_SCANNING && wm_state != WM_ROAMING)
        {
            wm_state = WM_SCANNING;
            Serial.println("[WifiManager] Weak signal (" + String(rssi) + " dBm), scanning...");
            WiFi.scanNetworksAsync(onScanDone);
        }
    }
}
