#include "iotglove.h"
/**
 * @brief millis 기반 타이머 세팅
 */
void TimerInit()
{
  ir_receive_timer_id = ir_receive_timer.setInterval(500, IrReceive);
  ir_receive_timer.disable(ir_receive_timer_id);
  wifi_timer_id = wifi_timer.setInterval(1000, WifiTimerFunc);
  battery_timer_id      = battery_timer.setInterval(60000, BatteryCheck);    // 1분마다 배터리 확인
  retry_timer_id        = retry_timer.setInterval(2000, RetryPending);       // 2초마다 재전송 시도
  wifi_manager_timer_id = wifi_manager_timer.setInterval(3000, WifiManagerRun); // 3초마다 RSSI 체크
}

/**
 * @brief Loop문에서 지속적으로 타이머의 시간을 체크
 */
void TimerRun()
{
  ir_receive_timer.run();
  wifi_timer.run();
  hacking_timer.run();
  neopixel_timer.run();
  battery_timer.run();
  retry_timer.run();
  wifi_manager_timer.run();
}

void WifiTimerFunc()
{
  // Connect()는 WifiManager가 담당. 여기서는 서버 폴링만
  if (WiFi.status() != WL_CONNECTED) return;
  has2wifi.Loop(DataChange);
}
