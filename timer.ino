#include "iotglove.h"
/**
 * @brief millis 기반 타이머 세팅
 */
void TimerInit()
{
  ir_receive_timer_id = ir_receive_timer.setInterval(500, IrReceive);
  ir_receive_timer.disable(ir_receive_timer_id);
  wifi_timer_id = wifi_timer.setInterval(1000, WifiTimerFunc);
  battery_timer_id = battery_timer.setInterval(60000, BatteryCheck); // 1분마다 배터리 확인
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
}

void WifiTimerFunc()
{
  // has2wifi.Connect("city");
  has2wifi.Connect("badland");
  has2wifi.Loop(DataChange);

}
