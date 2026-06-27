#include "updated_IoTglove.h"

// Internal ESP-IDF API — not in public headers. Signature stable across ESP-IDF 4.x/5.x but may change.
extern "C" esp_err_t esp_wifi_internal_set_fix_rate(wifi_interface_t ifx, bool en, wifi_phy_rate_t rate);

/**
 * @brief 약신호(-80dBm 이하) 링크 안정화 설정. WiFi.begin 전에 호출.
 *        - 모뎀 슬립 off (약신호에서 지연/누락 감소)
 *        - 최대 TX 파워 20dBm
 *        - 끊기면 자동 재연결
 *  ※ -80dBm 이하에서도 연결을 유지하는 핵심은 AP의
 *    Minimum RSSI 차단 해제이며, 그 설정은 Omada 컨트롤러에서 적용됨.
 *    거리가 멀어지면 rate adaptation 이 자동으로 1~2Mbps 로 낮춰 링크를 유지한다.
 */
/**
 * @brief 고정 저속 rate(11B/1Mbps) + 최대 TX 파워 적용.
 *        재연결(드라이버 auto-reconnect 포함) 때마다 fix_rate가 풀리므로
 *        연결 이벤트에서 다시 호출해 약신호 링크 튜닝을 유지한다.
 */
void WifiApplyLowRate()
{
  esp_err_t protocolResult = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  esp_err_t rateResult = esp_wifi_internal_set_fix_rate(WIFI_IF_STA, true, WIFI_PHY_RATE_1M_L);
  Serial.printf("[WIFI] protocol=%s fixed_rate=%s\n", esp_err_to_name(protocolResult), esp_err_to_name(rateResult));
  esp_wifi_set_max_tx_power(80); // 20 dBm
}

void WifiForceLowRateInit()
{
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  delay(100);

  // 끊김 후 재연결되면 고정 rate가 초기화되므로, STA 연결 이벤트마다 재적용한다.
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
               { WifiApplyLowRate(); },
               ARDUINO_EVENT_WIFI_STA_CONNECTED);

  WifiApplyLowRate();
}
