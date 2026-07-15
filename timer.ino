#include "updated_IoTglove.h"
/**
 * @brief millis 기반 타이머 세팅
 */
void TimerInit()
{
  // IRrecv는 프레임 캡처 후 resume() 전까지 다음 수신을 멈추므로, 자주 폴링해야
  // STOP 구간에 프레임이 갇혀 누락되는 일이 없다. (지연 단축이 아니라 수신 신뢰성 목적)
  ir_receive_timer_id = ir_receive_timer.setInterval(20, IrReceive);
  ir_receive_timer.disable(ir_receive_timer_id);
  wifi_timer_id = wifi_timer.setInterval(1000, WifiTimerFunc);
  battery_timer_id = battery_timer.setInterval(60000, BatteryCheck);
}

/**
 * @brief Loop문에서 지속적으로 타이머의 시간을 체크
 */
void TimerRun()
{
  ir_receive_timer.run();
  wifi_timer.run();
  neopixel_timer.run();
  battery_timer.run();
}

void WifiTimerFunc()
{
  // (A) 매초 직접 WiFi.begin 재연결 제거: 같은 AP 재연결은 setAutoReconnect(드라이버)가 무한 담당하고,
  // 다른 AP 로밍/워치독은 has2wifi.Loop() 내부 MaintainWifi가 처리한다. (중복·충돌 방지)
  has2wifi.Loop(DataChange);

  // Situation("taken") 전송 성공 후 ReceiveMine이 shift_machine에 의존하지 않도록
  // hacking 중에는 매초 강제 폴링하여 life_chip 변화를 빠르게 감지한다.
  if (hacking)
  {
    has2wifi.ReceiveMine();
    DataChange();
  }
}
