#include "updated_IoTglove.h"
//****************************************** Initialize ******************************************
void SensorInit()
{
  // Neopixel init
  pixels.begin();

  // IR init
  IrInit();

  // Vibration moter init
  MotorInit();

  // Buzzer init
  ledcSetup(5, MotorFreq, MotorResolution);
  ledcAttachPin(BUZZER_PIN, 5);
  ledcWrite(5, 0);

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
void IrSendDataSetup(String device_name)
{
  // Address : 그룹(G) command : 플레이어(P) 에 해당하는 숫자
  // 1. String 변수를 char 배열형식으로 변경
  int str_len = device_name.length() + 1;
  char char_device_name[str_len];
  device_name.toCharArray(char_device_name, str_len);

  // 2. 데이터 나누기
  byte address = char_device_name[1] - '0'; // char형 변수에서 int 값을 얻기위해 -'0'
  byte command = char_device_name[3] - '0';
  byte address_bar = ~address;
  byte command_bar = ~command;
  // IR data가 올바르게 전송되는지 확인하기 위해 _bar 변수 생성

  ir_send_data = ir_send_data ^ address;
  ir_send_data = ir_send_data << 8 ^ address_bar;
  ir_send_data = ir_send_data << 8 ^ command;
  ir_send_data = ir_send_data << 8 ^ command_bar;

  Serial.print("ir_send_data : ");
  Serial.println(ir_send_data, HEX);
}

/**
 * @brief IR 송신
 */
void IrSend()
{
  irsend.sendNEC(ir_send_data);
  delay(500);
}

/**
 * @brief IR 수신
 */
void IrReceive()
{
  if (irrecv.decode(&results))
  {
    // 0. IR 수신데이터 해석
    ir_decode_data = IrDecoding((uint32_t)results.value);
    if ((!ir_receive_error) && (ir_decode_data != "error"))
    {
      Serial.print("IR DATA : ");
      Serial.println(ir_decode_data);
      // 1. IR 수신데이터[플레이어 정보]를 DB에서 읽어와서 술래인지, 플레이어인지 확인
      has2wifi.Receive(ir_decode_data);
      // 2. 플레이어이면 조건에 맞다면 DB에서 2명의 플레이어 생명칩 개수 수정 & 술래이면 해킹 페이지로 전환
      // 자신의 IR이 찍히면 인식 X
      if ((String)(const char *)tag["device_name"] != (String)(const char *)my["device_name"])
      {
        if ((String)(const char *)tag["role"] == "tagger" && !hacking)
        {
          if ((String)(const char *)my["role"] == "player" && (String)(const char *)tag["device_state"] == "activate")
          {
            ir_receive_timer.disable(ir_receive_timer_id);
            hacking = true;
            PageChange("hacking");
          }
        }
        else if ((String)(const char *)tag["role"] == "player")
        {
          if (((int)my["life_chip"] < (int)my["max_life_chip"]))
          {
            ir_receive_timer.disable(ir_receive_timer_id);
            lifechip_receive = true;
            PageChange("lifechip_rece");
          }
        }
      }
    }
  }
  irrecv.resume();
}

/**
 * @brief   수신된 IR데이터를 분석
 *
 * @param ir_data   수신된 IR 데이터
 * @return String   Decoding 성공시 IR 송신자의 device_name을 리턴
 */
String IrDecoding(uint32_t ir_data)
{
  // 0. 수신데이터 나누기
  byte address = (byte)(ir_data >> 24);
  byte address_bar = (byte)((0x00FF) & (ir_data >> 16));
  byte command = (byte)((0x0000FF) & (ir_data >> 8));
  byte command_bar = (byte)((0x000000FF) & (ir_data));

  String ir_decode_data;

  // 1. 수신데이터 오류 판별
  if ((address | address_bar) == 0xFF)
  {
    if ((command | command_bar) == 0xFF)
    {
      String group_data = (String)(address);
      String player_data = (String)(command);
      ir_decode_data = String("G" + group_data + "P" + player_data);
      ir_receive_error = false;
      return ir_decode_data;
    }
  }

  // 3. 오류가 발생했다면 error를 리턴
  ir_receive_error = true;
  return "error";
}

//******************************************* QRD1114 *******************************************
/**
 * @brief QRD1114를 통한 근접거리 측정
 *
 * @return float 거리 측정 값
 */
float DistanceCheck()
{
  int proximityADC = analogRead(QRD1114_PIN);
  float proximityV = (float)proximityADC * 5.0 / 1023.0;
  delay(100);
  // Serial.println(proximityV);
  return proximityV;
}

//******************************************* Battery *******************************************
/**
 * @brief ESP32에 내장되어 있는 배터리 측정 GPIO
 */
void BatteryCheck()
{
  analogRead(34);
  BL.pinRead();
  BL.getBatteryVolts();

  if (BL.getBatteryVolts() < 3.7)
  {
    static bool send_complete = false;
    if (!send_complete)
    {
      send_complete = true; // 큐에 저장되든 직접 전송되든 중복 방지
      SafeSend((String)(const char *)my["device_name"], "device_state", "battery");
    }
  }
}

void MotorInit()
{
  // Linear Motor Init
  pinMode(MOTOR_INA1_PIN, OUTPUT);
  pinMode(MOTOR_INA2_PIN, OUTPUT);
  ledcSetup(MotorLedChannel, MotorFreq, MotorResolution);
  ledcAttachPin(MOTOR_PWMA_PIN, MotorLedChannel);
  ledcWrite(MotorLedChannel, 0);
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
    break;
  }
}

void MotorOn(const int *vibration_pattern, int len)
{
  static int repeat = 0;
  if (repeat < len - 1)
  {
    repeat++;
  }
  else
  {
    repeat = 0;
  } // 진동 패턴 인덱스를 한칸씩 증가 시킴
  // Serial.print("repeat : "); Serial.println(repeat);
  // Serial.print("repeat[] : "); Serial.println(vibration_pattern[repeat]);
  ledcWrite(MotorLedChannel, Intensity(vibration_pattern[repeat]));
  delay(20);
  digitalWrite(MOTOR_INA1_PIN, HIGH);
  digitalWrite(MOTOR_INA2_PIN, LOW);
}

void MotorStop()
{
  digitalWrite(MOTOR_INA1_PIN, LOW);
  digitalWrite(MOTOR_INA2_PIN, LOW);
}

//******************************************* Neopixel *******************************************
void tagger_blink()
{
  static bool blink = false;

  if (!blink)
  {
    pixels.lightColor(purple);
    blink = true;
  }
  else
  {
    pixels.clear();
    blink = false;
  }
}

//******************************************* Beetle ********************************************
/**
 * @brief 게임 내부 가장 가까이에 있는 와이파이 이름을 Beetle로부터 수신받음
 */
void BeetleScanWifi()
{
  if (MySerial1.available())
  {
    wifi_name = MySerial1.readStringUntil(' ');
    Serial.println(wifi_name);
    if (wifi_name == "reset")
    {
      MySerial1.print((String)(const char *)my["device_state"] + " ");
    }
    if (game_state == activate)
    {
      // if (wifi_name.startsWith("HAS2"))
      if (wifi_name.startsWith("badland"))
      {
        SafeSend((String)(const char *)my["device_name"], "location", wifi_name);
      }
      else
      {
        MySerial1.read();
      }
    }
    while (MySerial1.available() > 0)
    {
      MySerial1.read();
    }
  }
}
