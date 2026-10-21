#include "updated_IoTglove.h"

int ClampDisplayValue(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

int JsonIntOrDefault(const char *key, int fallback)
{
    if (my[key].isNull())
    {
        return fallback;
    }
    return my[key].as<int>();
}

void SendHmiValue(const char *target, int value)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s.val=%d", target, value);
    sendCommand(cmd);
}

void SendHmiPic(const char *page, const char *component, const char *base_var, int value, int min_value, int max_value)
{
    int clamped = ClampDisplayValue(value, min_value, max_value);
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "%s.%s.pic=%s.%s.val+%d", page, component, page, base_var, clamped);
    sendCommand(cmd);
}

void UpdatePageBattery(const char *page)
{
    int battery_pack = ClampDisplayValue(JsonIntOrDefault("battery_pack", 0), 0, 4);
    char target[40];
    snprintf(target, sizeof(target), "%s.vBatteryPack", page);
    SendHmiValue(target, battery_pack);
    SendHmiPic(page, "pBatteryPack", "vBatPicNum", battery_pack, 0, 4);
}

// Nextion HMI(glove.HMI)의 언어 변수는 pgStart 페이지의 전역 변수 vLanguage 하나뿐이다.
// 각 페이지의 preinitialize 이벤트가 `pgX.pic=vLangPicNum.val+pgStart.vLanguage.val`
// 로 배경 그림을 고르므로 값은 그림 오프셋이다. HMI 그림 라이브러리는 페이지마다
// [영어, 한국어] 순서로 쌍을 이루고(예: pgInfo 168=EXPLORING, 169=맵 탐색) vLanguage 의
// HMI 기본값은 1 이다. 따라서 0 = 영어, 1 = 한국어(기본).
// (dev.29 이전 코드는 존재하지 않는 pgInfo.vLanguage 에 써서 명령이 거부되어 항상 기본값
//  1(한국어)만 표시됐고, dev.30 은 변수 이름은 고쳤지만 값을 거꾸로 보냈다.)
int HmiLanguageValue()
{
    const char *selected_language = shift_machine["selected_language"].as<const char *>();
    return (selected_language != NULL && strcmp(selected_language, "EN") == 0) ? 0 : 1;
}

void UpdateHmiLanguage()
{
    int hmi_language = HmiLanguageValue();
    SendHmiValue("pgStart.vLanguage", hmi_language);

    // 배경 그림은 페이지가 열릴 때(preinitialize)만 언어를 읽는다. 준비 화면(pgInfo)에
    // 머무는 동안 언어가 바뀌면 같은 페이지를 다시 열어 바로 반영한다. 게임 중 페이지는
    // 타이머·게이지 상태가 있어 강제로 다시 열지 않고 다음 페이지 전환부터 반영된다.
    if (hmi_language != last_hmi_language)
    {
        last_hmi_language = hmi_language;
        if (game_state == ready && TextEquals(current_hmi_page, "pgInfo"))
        {
            sendCommand("page pgInfo");
        }
    }
}

void UpdateHmiBattery()
{
    UpdatePageBattery("pgSurvivor");
    UpdatePageBattery("pgGhost");
    UpdatePageBattery("pgRevival");
}

void UpdateHmiResults()
{
    int level = JsonIntOrDefault("lv", 0);
    int lives_lost = JsonIntOrDefault("lives_lost", 0);
    int batteries_gained = JsonIntOrDefault("batteries_gained", 0);
    int round_taken_chip = JsonIntOrDefault("round_taken_chip", 0);

    SendHmiValue("pgSurResult.vSurLevel", ClampDisplayValue(level, 0, 99));
    SendHmiValue("pgSurResult.vSurTakenLives", ClampDisplayValue(lives_lost, 0, 30));
    SendHmiValue("pgSurResult.vBatTotal", ClampDisplayValue(batteries_gained, 0, 20));
    SendHmiValue("pgTagResult.vTagLevel", ClampDisplayValue(level, 0, 99));
    SendHmiValue("pgTagResult.vTagTakenLives", ClampDisplayValue(round_taken_chip, 0, 30));
}

void AddRevivalGaugeBonus(int seconds)
{
    if (seconds <= 0)
    {
        return;
    }

    // HMI(pgGhost)의 게이지 타이머 tScreenRefresh는 tim=1000ms마다 vTick을 +1 한다(초당 1 tick).
    //   완료 시점: vTick >= vReviveTimeMax * (1000 / tim)
    // 도움받은 seconds만큼 막대를 앞으로 당기려면 (seconds * 1000/tim) tick을 더한다.
    // 1000/tim 은 REVIVAL_TICK_PER_SEC 상수로 박아 둔다(tim 변경 시 함께 갱신).
    int bonus_ticks = seconds * REVIVAL_TICK_PER_SEC;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "pgGhost.vTick.val=pgGhost.vTick.val+%d", bonus_ticks);
    sendCommand(cmd);
}

bool ReadHmiNumber(const char *target, uint32_t *value, uint32_t timeout_ms)
{
    if (target == NULL || value == NULL)
    {
        return false;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "get %s", target);
    sendCommand(cmd);
    return recvRetNumber(value, timeout_ms);
}

bool ReadNextionVersion(uint32_t *version)
{
    // vVersion은 글로벌 변수라 어느 페이지에서든 읽힘.
    // 페이지를 이동(page pgSetting -> page 0)하면 게임 화면이 검은 page 0으로
    // 튀므로 이동 없이 직접 읽기만 수행한다. 실패 시 UpdateVersionReport가 재시도.
    return ReadHmiNumber("pgSetting.vVersion.val", version, 500);
}

void DisplaySet()
{
    UpdateHmiLanguage();
    UpdateHmiBattery();
    UpdateHmiResults();
    SendHmiValue("pgGhost.vReviveTimeMax", JsonIntOrDefault("revival_time", DEFAULT_REVIVAL_TIME_SEC));
}

void DisplayCheck()
{
    while (MySerial2.available() > 0)
    {
        MySerial2.read();
    }
}

void PageChange(const char *page)
{
    bool page_changed = !TextEquals(current_hmi_page, page);
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "page %s", page);
    sendCommand(cmd);
    if (page_changed)
    {
        StartPageChangeVibration();
    }
    snprintf(current_hmi_page, sizeof(current_hmi_page), "%s", page);
}
