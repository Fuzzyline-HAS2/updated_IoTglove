#include "updated_IoTglove.h"

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
#define RSSI_STABLE    -67  // 이상: 유지
#define RSSI_WATCHING  -75  // ~ -67: 관찰
#define RSSI_ROAM_GAIN  8   // 현재보다 8dB 이상 강한 AP 발견 시 로밍

// ======================== 상태머신 ========================
enum WifiManagerState { WM_STABLE, WM_WATCHING, WM_SCANNING, WM_ROAMING, WM_DISCONNECTED };
WifiManagerState wm_state = WM_STABLE;

// ======================== 플래그 ========================
bool is_intentional_roam  = false; // 의도적 로밍 중 (disconnect 이벤트 무시용)
bool need_reconnect_scan  = false; // 끊긴 후 재연결 스캔 필요 여부

// ======================== 유틸 ========================
bool isKnownAP(String ssid)
{
    for (int i = 0; i < CURRENT_SSID_COUNT; i++)
        if (ssid == CURRENT_SSIDS[i]) return true;
    return false;
}

// ======================== 비동기 스캔 콜백: 재연결용 ========================
void onDisconnectScanDone(int numNetworks)
{
    if (numNetworks <= 0)
    {
        Serial.println("[WifiManager] Reconnect scan: no AP found, will retry.");
        need_reconnect_scan = true; // 다음 WifiManagerRun에서 재시도
        WiFi.scanDelete();
        return;
    }

    String best_ssid = "";
    int    best_rssi = -200;

    for (int i = 0; i < numNetworks; i++)
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
        Serial.println("[WifiManager] Reconnecting to " + best_ssid + " (" + String(best_rssi) + " dBm)");
        WiFi.begin(best_ssid.c_str(), WIFI_PASSWORD);
    }
    else
    {
        Serial.println("[WifiManager] No known AP found, will retry.");
        need_reconnect_scan = true; // 다음 WifiManagerRun에서 재시도
    }
}

// ======================== 비동기 스캔 콜백: 로밍용 ========================
void onRoamScanDone(int numNetworks)
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
// 주의: 플래그 세팅만. 무거운 작업 금지
void WiFiEventHandler(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("[WifiManager] Connected - IP: " + WiFi.localIP().toString() +
                       "  SSID: " + WiFi.SSID());
        just_reconnected    = true;
        is_intentional_roam = false;
        need_reconnect_scan = false;
        wm_state            = WM_STABLE;
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (!is_intentional_roam)
        {
            Serial.println("[WifiManager] Disconnected. Scan scheduled.");
            wm_state            = WM_DISCONNECTED;
            need_reconnect_scan = true; // WifiManagerRun에서 async scan 시작
        }
        break;

    default:
        break;
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
    else
    {
        CURRENT_SSIDS      = BADLAND_SSIDS;
        CURRENT_SSID_COUNT = 5;
    }
    WiFi.setAutoReconnect(false); // 재연결은 WifiManager가 직접 관리
    WiFi.onEvent(WiFiEventHandler);
}

// ======================== 1초마다 타이머에서 호출 ========================
void WifiManagerRun()
{
    // --- 끊긴 상태 처리 ---
    if (WiFi.status() != WL_CONNECTED)
    {
        if (wm_state == WM_ROAMING) return; // 의도적 로밍 중, 대기

        if (need_reconnect_scan)
        {
            need_reconnect_scan = false;
            Serial.println("[WifiManager] Starting reconnect scan (async)...");
            WiFi.scanNetworksAsync(onDisconnectScanDone); // non-blocking
        }
        return;
    }

    // --- 연결 상태: RSSI 체크 ---
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
        // < -75 dBm: 로밍 후보 비동기 스캔
        if (wm_state != WM_SCANNING && wm_state != WM_ROAMING)
        {
            wm_state = WM_SCANNING;
            Serial.println("[WifiManager] Weak signal (" + String(rssi) + " dBm), scanning for roam...");
            WiFi.scanNetworksAsync(onRoamScanDone); // non-blocking
        }
    }
}
