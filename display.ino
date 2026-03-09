#include "iotglove.h"

/**
 * @brief 디스플레이 화면 초기화
 */
void DisplaySet()
{
    String cmd = "";
    if ((String)(const char *)my["role"] != "tagger")
    {
        // life_chip 초기화
        cmd = "player.life_chip.val=" + (String)(const char *)my["life_chip"];
        sendCommand(cmd.c_str());
        // BatteryPack 초기화
        cmd = "player.BatteryPack.pic=player.battery_pic.val+" + (String)(const char *)my["battery_pack"];
        sendCommand(cmd.c_str());

        // LifeChip 초기화
        if ((int)my["life_chip"] > 1)
        {
            cmd = "player.LifeChip.pic=player.life_chip_pic.val+1";
            sendCommand(cmd.c_str());
        }
        else
        {
            cmd = "player.LifeChip.pic=player.life_chip_pic.val";
            sendCommand(cmd.c_str());
        }

        // level 초기화
        cmd = "player.level.val=" + (String)(const char *)my["lv"];
        sendCommand(cmd.c_str());

        // exp 초기화
        cmd = "player.exp.val=" + (String)(const char *)my["exp"];
        sendCommand(cmd.c_str());
    }
    else
    {
        // taken_chip 초기화
        cmd = "tagger.taken_chip.val=" + (String)(const char *)my["taken_chip"];
        sendCommand(cmd.c_str());

        // TakenChip 초기화
        if ((int)my["taken_chip"] > 0)
        {
            cmd = "tagger.TakenChip.pic=tagger.taken_chip_pic.val+1";
            sendCommand(cmd.c_str());
        }
        else
        {
            cmd = "tagger.TakenChip.pic=tagger.taken_chip_pic.val";
            sendCommand(cmd.c_str());
        }

        // level 초기화
        cmd = "tagger.level.val=" + (String)(const char *)my["lv"];
        sendCommand(cmd.c_str());

        // exp 초기화
        cmd = "tagger.exp.val=" + (String)(const char *)my["exp"];
        sendCommand(cmd.c_str());
    }
}

/**
 * @brief 디스플레이에 변화를 주거나 변화가 있을시 실행
 */
// void DisplayCheck()
// {
//     if (MySerial2.available() > 0)
//     {
//         String nextion_string = MySerial2.readStringUntil(' ');
//         Serial.print("Display Out Run : ");
//         Serial.println(nextion_string);
//         if (nextion_string.startsWith("H_"))
//         {
//             nextion_string = nextion_string.substring(2);
//             Serial.print("Nextion String : ");
//             Serial.println(nextion_string);
//             NextionReceived(&nextion_string);
//         }
//     }
// }

void DisplayCheck()
{
    if (MySerial2.available() > 0)
    {
        if (MySerial2.read() == 'H')
        {
            if (MySerial2.read() == '_')
            {
                String nextion_string = MySerial2.readStringUntil(' ');
                Serial.print("Nextion String : ");
                Serial.println(nextion_string);
                NextionReceived(&nextion_string);
            }
        }
    }
}

/**
 * @brief 디스플레이에서 오는 Serial을 확인
 *
 * @param nextion_string Serial 문자열 데이터
 */
void NextionReceived(String *nextion_string)
{
    if (*nextion_string == "lifesend")
    {
        IrSend();
        PageChange("lifechip_send");
        delay(4000);
        PageChange("player");
    }
    else if (*nextion_string == "lifechip_receive")
    {
        SafeSituation(ir_decode_data, "send_life");
        if ((String)(const char *)my["role"] == "ghost")
        {
            SafeSend((String)(const char *)my["device_name"], "role", "revival");
        }
        else if ((String)(const char *)my["role"] == "player")
        {
            PageChange("player");
            ir_receive_timer.enable(ir_receive_timer_id);
        }
        lifechip_receive = false;
    }
    else if (*nextion_string == "revival")
    {
        SafeSend((String)(const char *)my["device_name"], "role", "player");
        revival = false;
        ir_receive_timer.enable(ir_receive_timer_id);
    }
    else if (*nextion_string == "hacking")
    {
        ir_receive_timer.disable(ir_receive_timer_id);
    }
    else if (*nextion_string == "die")
    {
        hacking = false;
        if ((int)my["life_chip"] > 1)
        {
            SafeSend((String)(const char *)my["device_name"], "role", "revival");
        }
        if ((int)my["life_chip"] > 0)
        {
            SafeSituation(ir_decode_data, "taken");
        }
    }
    else if (*nextion_string == "messege_exit")
    {
        if ((String)(const char *)my["role"] == "player")
        {
            PageChange("player");
        }
        else if ((String)(const char *)my["role"] == "ghost")
        {
            Serial.println("messege ghost");
            PageChange("ghost");
        }
        SafeSend((String)(const char *)my["device_name"], "message_sender", "no");
    }
    else
    {
        Serial.print("Error String : ");
        Serial.println(*nextion_string);
    }
}

void PageChange(String page)
{
    static String cur_page = "";

    String cmd = "page " + page;
    sendCommand(cmd.c_str());
    cur_page = page;
}
