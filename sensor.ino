#include "updated_IoTglove.h"
//****************************************** Initialize ******************************************
void SensorInit()
{
  // Neopixel init
  pixels.begin();
  pixels.setBrightness(ledBrightness);
  lightColor(white);

  // IR init
  IrInit();

  // Vibration moter init
  MotorInit();

  // Buzzer init
  ledcAttach(BUZZER_PIN, MotorFreq, MotorResolution);
  ledcWrite(BUZZER_PIN, 0);

  // QRD1114 init
  pinMode(QRD1114_PIN, INPUT);
}
//********************************************** IR **********************************************
/**
 * @brief IR Receiver, Transmitter 세팅
 */
void IrInit()
{
  irsend.begin();
  irrecv.enableIRIn(); // Start the receiver
  IrSendDataSetup((String)(const char *)my["device_name"]);
}

/**
 * @brief IR 송신 데이터 설정
 * @param device_name MYSQL에 저장되어 있는 디바이스 이름 => (const char*)my["device_name"]
 */
uint8_t IrRoleCode(const char *role)
{
  if (IsTaggerRole(role))
  {
    return IR_ROLE_TAGGER;
  }
  if (IsRevivalRole(role))
  {
    return IR_ROLE_GHOST;
  }
  return IR_ROLE_PLAYER;
}

const char *IrRoleName(uint8_t role_code)
{
  switch (role_code)
  {
  case IR_ROLE_PLAYER:
    return "player";
  case IR_ROLE_GHOST:
    return "revival";
  case IR_ROLE_TAGGER:
    return "tagger";
  default:
    return "";
  }
}

void IrSendDataSetup(String device_name)
{
  ir_send_data = 0;
  ir_send_data_valid = false;

  if (device_name.length() < 4)
  {
    DebugPrint("[IR] invalid device_name: ");
    DebugPrintln(device_name);
    return;
  }

  int group = device_name.charAt(1) - '0';
  int player = device_name.charAt(3) - '0';
  if (group < 0 || group > 9 || player < 0 || player > 9)
  {
    DebugPrint("[IR] invalid device_name: ");
    DebugPrintln(device_name);
    return;
  }

  uint8_t role_code = IrRoleCode(CurrentRole());
  byte address = (byte)group;
  byte command = (byte)(((role_code & IR_PLAYER_MASK) << IR_ROLE_SHIFT) | ((byte)player & IR_PLAYER_MASK));
  byte address_bar = ~address;
  byte command_bar = ~command;
  // IR data가 올바르게 전송되는지 확인하기 위해 _bar 변수 생성

  ir_send_data = ir_send_data ^ address;
  ir_send_data = ir_send_data << 8 ^ address_bar;
  ir_send_data = ir_send_data << 8 ^ command;
  ir_send_data = ir_send_data << 8 ^ command_bar;

  DebugPrint("ir_send_data : ");
  DebugPrintln((unsigned long)ir_send_data, HEX);
  DebugPrint("[IR] send role=");
  DebugPrint(IrRoleName(role_code));
  DebugPrint(" device=");
  DebugPrintln(device_name);

  ir_send_device_name = device_name;
  ir_send_role_code = role_code;
  ir_send_data_valid = true;
}

/**
 * @brief IR 송신
 */
void IrSend()
{
  const char *device_name_value = my["device_name"].as<const char *>();
  String device_name = device_name_value == NULL ? "" : String(device_name_value);
  uint8_t role_code = IrRoleCode(CurrentRole());
  if (!ir_send_data_valid || ir_send_device_name != device_name || ir_send_role_code != role_code)
  {
    IrSendDataSetup(device_name);
  }
  if (!ir_send_data_valid)
  {
    return;
  }

  unsigned long now = millis();
  if ((long)(now - last_ir_send_ms) < (long)next_ir_interval_ms)
  {
    return;
  }

  last_ir_send_ms = now;
  irsend.sendNEC(ir_send_data);
  // 다음 간격을 매번 랜덤하게 흔든다(150~250ms). 여러 글러브가 같은 리듬으로 송신해
  // 충돌이 고착되는 것을 막아 다중 환경 수신 성공률을 높인다.
  // esp_random()은 RF(WiFi) 활성 시 하드웨어 RNG라 글러브마다/매번 다른 값 → 별도 randomSeed 불필요.
  next_ir_interval_ms = IR_SEND_INTERVAL_MS + (esp_random() % (IR_SEND_JITTER_MS + 1));
}

void ClearRevivalHelpRecords()
{
  for (int i = 0; i < REVIVAL_HELP_RECORDS; i++)
  {
    revival_help_records[i].device_name = "";
    revival_help_records[i].expires_at = 0;
  }
}

bool HelpRecordExpired(unsigned long now, unsigned long expires_at)
{
  return expires_at == 0 || (long)(now - expires_at) >= 0;
}

bool ShouldSendRevivalCooldown(String device_name, unsigned long ttl_ms)
{
  if (device_name.length() == 0)
  {
    return false;
  }

  unsigned long now = millis();
  for (int i = 0; i < REVIVAL_HELP_RECORDS; i++)
  {
    if (revival_help_records[i].device_name.length() > 0 && HelpRecordExpired(now, revival_help_records[i].expires_at))
    {
      revival_help_records[i].device_name = "";
      revival_help_records[i].expires_at = 0;
    }
  }

  for (int i = 0; i < REVIVAL_HELP_RECORDS; i++)
  {
    if (revival_help_records[i].device_name == device_name)
    {
      return false;
    }
  }

  int target_index = 0;
  for (int i = 0; i < REVIVAL_HELP_RECORDS; i++)
  {
    if (revival_help_records[i].device_name.length() == 0)
    {
      target_index = i;
      break;
    }

    if ((long)(revival_help_records[i].expires_at - revival_help_records[target_index].expires_at) < 0)
    {
      target_index = i;
    }
  }

  revival_help_records[target_index].device_name = device_name;
  revival_help_records[target_index].expires_at = now + ttl_ms;
  return true;
}

/**
 * @brief IR 수신
 */
void IrReceive()
{
  static String cached_tag_device_name;
  static String cached_tag_role;
  static String cached_tag_device_state;
  static int cached_tag_revival_time = 0;
  static unsigned long cached_tag_expires_at = 0;
  static int revival_help_count = 0;

  if (!irrecv.decode(&results))
  {
    return;
  }

  auto decode_type = results.decode_type;
  uint16_t bits = results.bits;
  uint64_t value = results.value;
  irrecv.resume();

  if (sur_connect_pending)
  {
    return;
  }

  // NEC 32bit 프레임만 수용한다. 다른 프로토콜/노이즈/깨진 캡처가 우연히
  // 보수(complement) 체크를 통과해 G208P208 같은 쓰레기값이 나오는 것을 막는다.
  if (decode_type != NEC || bits != kNECBits)
  {
    return;
  }

  ir_decode_data = IrDecoding((uint32_t)value);
  if ((!ir_receive_error) && (ir_decode_data != "error"))
  {
    DebugPrint("IR DATA : ");
    DebugPrintln(ir_decode_data);

    String tag_device_name_text = ir_decode_data;
    String tag_role_text;
    String tag_device_state_text = "activate";
    int tag_revival_time = 0;

    unsigned long now = millis();
    bool role_in_ir = ir_decode_role == IR_ROLE_TAGGER || ir_decode_role == IR_ROLE_GHOST;
    if (role_in_ir)
    {
      tag_role_text = IrRoleName(ir_decode_role);
    }
    else
    {
      bool cache_valid = cached_tag_device_name == ir_decode_data &&
                         cached_tag_expires_at != 0 &&
                         (long)(now - cached_tag_expires_at) < 0;
      if (!cache_valid)
      {
        unsigned long receive_start_ms = millis();
        has2wifi.Receive(ir_decode_data);
        unsigned long receive_elapsed_ms = millis() - receive_start_ms;
        if (receive_elapsed_ms > 200)
        {
          DebugPrintf("[IR] Receive %s took %lu ms\n", ir_decode_data.c_str(), receive_elapsed_ms);
        }

        const char *server_tag_device_name = tag["device_name"].as<const char *>();
        if (!TextEquals(server_tag_device_name, ir_decode_data.c_str()))
        {
          DebugPrint("[IR] Drop stale tag data decoded=");
          DebugPrint(ir_decode_data);
          DebugPrint(" server=");
          DebugPrintln(server_tag_device_name == NULL ? "(null)" : server_tag_device_name);
          hack_count = 0;
          cached_tag_device_name = "";
          cached_tag_role = "";
          cached_tag_device_state = "";
          cached_tag_revival_time = 0;
          cached_tag_expires_at = 0;
          return;
        }

        cached_tag_device_name = server_tag_device_name == NULL ? "" : server_tag_device_name;
        const char *server_tag_role = tag["role"].as<const char *>();
        const char *server_tag_device_state = tag["device_state"].as<const char *>();
        cached_tag_role = server_tag_role == NULL ? "" : server_tag_role;
        cached_tag_device_state = server_tag_device_state == NULL ? "" : server_tag_device_state;
        cached_tag_revival_time = tag["revival_time"].as<int>();
        cached_tag_expires_at = millis() + IR_TAG_CACHE_TTL_MS;
      }

      tag_device_name_text = cached_tag_device_name;
      tag_role_text = cached_tag_role;
      tag_device_state_text = cached_tag_device_state;
      tag_revival_time = cached_tag_revival_time;
    }

    const char *tag_device_name = tag_device_name_text.c_str();
    const char *my_device_name = my["device_name"].as<const char *>();
    if (!TextEquals(tag_device_name, my_device_name))
    {
      const char *my_role = my["role"].as<const char *>();
      const char *tag_role = tag_role_text.c_str();
      const char *tag_device_state = tag_device_state_text.c_str();
      const char *my_device_state = my["device_state"].as<const char *>();

      if (IsTaggerRole(tag_role) && !hacking)
      {
        revival_help_count = 0;
        if (IsPlayerRole(my_role) && TextEquals(tag_device_state, "activate"))
        {
          hack_count++;
          if (hack_count >= HACK_THRESHOLD)
          {
            hack_count = 0;
            hacking = true;
            if (IsFinalLifeTaken())
            {
              StartPendingRevivalVibration();
            }
            unsigned long situation_start_ms = millis();
            has2wifi.Situation(ir_decode_data, "taken");
            unsigned long situation_elapsed_ms = millis() - situation_start_ms;
            if (situation_elapsed_ms > 200)
            {
              DebugPrintf("[IR] Situation taken %s took %lu ms\n", ir_decode_data.c_str(), situation_elapsed_ms);
            }
          }
        }
        else
        {
          hack_count = 0;
        }
      }
      else if (IsRevivalRole(tag_role))
      {
        hack_count = 0;
        if (IsPlayerRole(my_role) && TextEquals(my_device_state, "activate") && !hacking)
        {
          // 송신 기기를 구분하지 않고 누적한다(tagger의 hack_count와 동일).
          // 임계값 도달 시점의 ir_decode_data를 부활 상황 전송 대상으로 사용한다.
          revival_help_count++;
          if (revival_help_count < REVIVAL_HELP_THRESHOLD)
          {
            return;
          }

          revival_help_count = 0;

          int revival_time = tag_revival_time;
          if (revival_time <= 0)
          {
            revival_time = my["revival_time"].as<int>();
          }
          if (revival_time <= 0)
          {
            revival_time = DEFAULT_REVIVAL_TIME_SEC;
          }

          unsigned long ttl_ms = (unsigned long)revival_time * 1000UL;
          if (!sur_connect_pending && ShouldSendRevivalCooldown(ir_decode_data, ttl_ms))
          {
            StartSurConnect();
            unsigned long situation_start_ms = millis();
            has2wifi.Situation(ir_decode_data, "revival_cooldown");
            unsigned long situation_elapsed_ms = millis() - situation_start_ms;
            if (situation_elapsed_ms > 200)
            {
              DebugPrintf("[IR] Situation revival_cooldown %s took %lu ms\n", ir_decode_data.c_str(), situation_elapsed_ms);
            }
          }
        }
        else
        {
          revival_help_count = 0;
        }
      }
      else
      {
        hack_count = 0;
        revival_help_count = 0;
      }
    }
  }
}

/**
 * @brief   수신된 IR데이터를 분석
 *
 * @param ir_data   수신된 IR 데이터
 * @return String   Decoding 성공시 IR 송신자의 device_name을 리턴
 */
String IrDecoding(uint32_t ir_data)
{
  ir_decode_role = IR_ROLE_UNKNOWN;

  // 0. 수신데이터 나누기
  byte address = (byte)(ir_data >> 24);
  byte address_bar = (byte)((0x00FF) & (ir_data >> 16));
  byte command = (byte)((0x0000FF) & (ir_data >> 8));
  byte command_bar = (byte)((0x000000FF) & (ir_data));

  String ir_decode_data;

  // 1. 수신데이터 오류 판별
  if (address_bar == (byte)(~address))
  {
    if (command_bar == (byte)(~command) && address <= 9)
    {
      byte role_code = command >> IR_ROLE_SHIFT;
      byte player = command & IR_PLAYER_MASK;

      // 구형 프레임(command=player)은 role 비트가 없어 서버 조회 fallback으로 처리한다.
      // 신형 action 프레임은 G=1, T=2가 상위 4비트에 들어온다.
      if (command <= 9)
      {
        player = command;
        ir_decode_role = IR_ROLE_UNKNOWN;
      }
      else if ((role_code == IR_ROLE_GHOST || role_code == IR_ROLE_TAGGER) && player <= 9)
      {
        ir_decode_role = role_code;
      }
      else
      {
        ir_receive_error = true;
        return "error";
      }

      String group_data = (String)(address);
      String player_data = (String)(player);
      ir_decode_data = String("G" + group_data + "P" + player_data);
      ir_receive_error = false;
      return ir_decode_data;
    }
  }

  // 3. 오류가 발생했다면 error를 리턴
  ir_receive_error = true;
  return "error";
}

//******************************************* Battery *******************************************
/**
 * @brief ESP32에 내장되어 있는 배터리 측정 GPIO
 */
void BatteryCheck()
{
  analogRead(34);
  BL.pinRead();
  float battery_volts = BL.getBatteryVolts();

  has2wifi.Send((String)(const char *)my["device_name"], "battery_remaining", String(battery_volts, 2));
}

void MotorInit()
{
  // Linear Motor Init
  pinMode(MOTOR_INA1_PIN, OUTPUT);
  pinMode(MOTOR_INA2_PIN, OUTPUT);
  ledcAttach(MOTOR_PWMA_PIN, MotorFreq, MotorResolution);
  ledcWrite(MOTOR_PWMA_PIN, 0);
}

int Intensity(int intensity)
{
  switch (intensity)
  {
  case 0:
    return 0;
    break;
  case 1:
    return vibration_mode1;
    break;
  case 2:
    return vibration_mode2;
    break;
  case 3:
    return vibration_mode3;
    break;
  case 4:
    return vibration_mode4;
    break;
  case 5:
    return vibration_mode5;
    break;
  default:
    return 0;
    break;
  }
  return 0;
}

// 진동 패턴을 비블로킹으로 한 스텝씩 재생한다(루프를 막지 않음).
// step_ms 간격으로 다음 세기로 넘어가며, 레벨이 바뀌면 패턴을 처음부터 다시 시작한다.
void MotorStep(int level)
{
  if (level < 1 || level > VIBE_PATTERN_COUNT)
  {
    MotorStop();
    return;
  }

  const VibePattern *vp = &VIBE_PATTERNS[level - 1];

  static int active_level = 0;
  static int step_index = 0;
  static unsigned long last_step_ms = 0;
  unsigned long now = millis();

  if (level != active_level) // 레벨 전환 시 패턴 리셋
  {
    active_level = level;
    step_index = 0;
    last_step_ms = 0;
  }

  if (last_step_ms != 0 && now - last_step_ms < (unsigned long)vp->step_ms)
  {
    return; // 아직 다음 스텝 시간이 아님
  }
  last_step_ms = now;

  ledcWrite(MOTOR_PWMA_PIN, Intensity(vp->pattern[step_index]));
  digitalWrite(MOTOR_INA1_PIN, HIGH);
  digitalWrite(MOTOR_INA2_PIN, LOW);

  step_index++;
  if (step_index >= vp->len)
  {
    step_index = 0;
  }
}

void MotorStop()
{
  ledcWrite(MOTOR_PWMA_PIN, 0);
  digitalWrite(MOTOR_INA1_PIN, LOW);
  digitalWrite(MOTOR_INA2_PIN, LOW);
}

//******************************************* Neopixel *******************************************
void UpdateBrightness() {
  int serverBrightness = my["brightness"].as<int>();
  if (serverBrightness <= 0 || serverBrightness > 100) {
    ledBrightness = DEFAULT_BRIGHTNESS;
  } else {
    ledBrightness = map(serverBrightness, 1, 100, 1, 255);
  }
  pixels.setBrightness(ledBrightness);
  pixels.show();
}

void lightColor(int color[])
{
  pixels.fill(pixels.Color(color[0], color[1], color[2]));
  pixels.show();
}

void ota_success_blink()
{
  for (int i = 0; i < 6; i++)
  {
    lightColor(i % 2 == 0 ? red : blue);
    delay(300);
  }
  pixels.clear();
  pixels.show();
}

void tagger_blink()
{
  static bool blink = false;

  if (!blink)
  {
    lightColor(purple);
    blink = true;
  }
  else
  {
    pixels.clear();
    pixels.show();
    blink = false;
  }
}

//******************************************* Beetle ********************************************
/**
 * @brief 게임 내부 가장 가까이에 있는 와이파이 이름을 Beetle로부터 수신받음
 */
bool SendLocationToServer(const String &location)
{
  String device_name = (String)(const char *)my["device_name"];
  if (device_name.length() == 0)
  {
    DebugPrintln("[TTGO] send failed: missing device_name");
    return false;
  }

  HTTPClient location_http;
  String request = String("http://172.30.1.43/has2.php?request=Send&table=device&key=") +
                   device_name + "&column=location&value=" + location;
  location_http.begin(request);
  int http_code = location_http.GET();
  if (http_code == HTTP_CODE_OK)
  {
    location_http.end();
    return true;
  }

  DebugPrintf("[TTGO] send http failed code=%d location=%s\n", http_code, location.c_str());
  location_http.end();
  return false;
}

void QueueLocationSend(const String &location, bool force_now)
{
  if (location == HAS3_UNKNOWN_ROOM)
  {
    if (!location_vibe_muted)
    {
      DebugPrintln("[TTGO] location unknown: mute proximity vibration");
    }
    location_vibe_muted = true;
    pending_location = "";
    last_location_send_fail_at = 0;
    return;
  }

  if (location_vibe_muted)
  {
    DebugPrint("[TTGO] location recovered: unmute proximity vibration=");
    DebugPrintln(location);
    location_vibe_muted = false;
  }

  if (location == last_sent_location && pending_location.length() == 0)
  {
    DebugPrint("[TTGO] skip duplicate location=");
    DebugPrintln(location);
    return;
  }

  unsigned long now = millis();
  if (!force_now && pending_location == location &&
      last_location_send_fail_at != 0 &&
      now - last_location_send_fail_at < 3000)
  {
    DebugPrint("[TTGO] retry pending location=");
    DebugPrint(location);
    DebugPrint(" wait=");
    DebugPrintln((unsigned long)(3000 - (now - last_location_send_fail_at)));
    return;
  }

  DebugPrint("[TTGO] send location=");
  DebugPrintln(location);
  if (SendLocationToServer(location))
  {
    last_sent_location = location;
    pending_location = "";
    last_location_send_fail_at = 0;
    DebugPrint("[TTGO] send success location=");
    DebugPrintln(location);
  }
  else
  {
    pending_location = location;
    last_location_send_fail_at = now;
    DebugPrint("[TTGO] send failed location=");
    DebugPrintln(location);
  }
}

void RetryPendingLocationSend()
{
  if (pending_location.length() == 0 || last_location_send_fail_at == 0)
  {
    return;
  }

  if (millis() - last_location_send_fail_at >= 3000)
  {
    QueueLocationSend(pending_location, true);
  }
}

void ProcessBeetleLine(const char *line, String &loop_location_candidate)
{
  if (line == nullptr || line[0] == '\0')
  {
    return;
  }

  String frame = line;
  frame.trim();
  DebugPrintln(frame);

  if (frame == "reset")
  {
    MySerial1.print((String)(const char *)my["device_state"] + " ");
    return;
  }
  if (frame == "beetle_ota_start")
  {
    // Wait for a terminal result (done/skip/error) before running TTGO OTA,
    // so the Beetle outcome is visible on telnet before TTGO reboots.
    DebugPrintln("[TTGO] beetle OTA started, waiting for result");
    return;
  }
  if (frame == "beetle_ota_done")
  {
    DebugPrintln("[TTGO] beetle OTA success");
    FinishBeetleOtaWaitAndRunTtgoOta("beetle_ota_done");
    return;
  }
  if (frame == "beetle_ota_skip")
  {
    DebugPrintln("[TTGO] beetle OTA skipped (already current)");
    FinishBeetleOtaWaitAndRunTtgoOta("beetle_ota_skip");
    return;
  }
  if (frame == "beetle_ota_error")
  {
    DebugPrintln("[TTGO] beetle OTA error");
    FinishBeetleOtaWaitAndRunTtgoOta("beetle_ota_error");
    return;
  }

  if (!frame.startsWith("ROOM:"))
  {
    DebugPrint("[TTGO] invalid frame: ");
    DebugPrintln(frame);
    return;
  }

  String room = frame.substring(5);
  room.trim();
  if (!Has3IsValidRoom(room.c_str()))
  {
    DebugPrint("[TTGO] invalid frame: ");
    DebugPrintln(frame);
    return;
  }

  DebugPrint("[TTGO] received room=");
  DebugPrintln(room);
  loop_location_candidate = room;
}

void BeetleScanWifi()
{
  String loop_location_candidate;

  while (MySerial1.available() > 0)
  {
    char c = (char)MySerial1.read();
    if (c == '\n')
    {
      if (beetle_rx_overflow)
      {
        beetle_rx_overflow = false;
        beetle_rx_len = 0;
        continue;
      }

      if (beetle_rx_len > 0)
      {
        beetle_rx_buffer[beetle_rx_len] = '\0';
        ProcessBeetleLine(beetle_rx_buffer, loop_location_candidate);
        beetle_rx_len = 0;
      }
      continue;
    }

    if (c == '\r')
    {
      continue;
    }

    if (beetle_rx_overflow)
    {
      continue;
    }

    if (beetle_rx_len < BEETLE_RX_BUFFER_SIZE - 1)
    {
      beetle_rx_buffer[beetle_rx_len++] = c;
    }
    else
    {
      DebugPrintln("[TTGO] uart overflow, drop line");
      beetle_rx_overflow = true;
      beetle_rx_len = 0;
    }
  }

  if (loop_location_candidate.length() > 0)
  {
    if (game_state == activate)
    {
      QueueLocationSend(loop_location_candidate, true);
    }
    else
    {
      DebugPrint("[TTGO] skip location while inactive=");
      DebugPrintln(loop_location_candidate);
    }
  }

  RetryPendingLocationSend();
}
