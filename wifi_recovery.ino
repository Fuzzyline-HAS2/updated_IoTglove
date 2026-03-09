#include "iotglove.h"

#define PENDING_QUEUE_SIZE 8

enum ActionType { ACTION_SEND, ACTION_SITUATION };

struct PendingAction {
    ActionType type;
    String device_name;
    String key;    // Send: column명 / Situation: situation명
    String value;  // Send: value / Situation: 미사용
    bool active;
};

PendingAction pending_queue[PENDING_QUEUE_SIZE];

/**
 * @brief 대기 큐에 액션 추가. 큐가 꽉 찼으면 가장 오래된 항목을 밀어내고 추가
 */
void AddToQueue(ActionType type, String device, String key, String value)
{
    for (int i = 0; i < PENDING_QUEUE_SIZE; i++)
    {
        if (!pending_queue[i].active)
        {
            pending_queue[i].type        = type;
            pending_queue[i].device_name = device;
            pending_queue[i].key         = key;
            pending_queue[i].value       = value;
            pending_queue[i].active      = true;
            Serial.println("[Recovery] Queued: " + key + "=" + value);
            return;
        }
    }
    // 큐가 꽉 찼으면 shift 후 마지막 슬롯에 저장
    for (int i = 0; i < PENDING_QUEUE_SIZE - 1; i++)
    {
        pending_queue[i] = pending_queue[i + 1];
    }
    pending_queue[PENDING_QUEUE_SIZE - 1].type        = type;
    pending_queue[PENDING_QUEUE_SIZE - 1].device_name = device;
    pending_queue[PENDING_QUEUE_SIZE - 1].key         = key;
    pending_queue[PENDING_QUEUE_SIZE - 1].value       = value;
    pending_queue[PENDING_QUEUE_SIZE - 1].active      = true;
    Serial.println("[Recovery] Queue full, replaced oldest: " + key + "=" + value);
}

/**
 * @brief 대기 큐에 있는 액션을 순서대로 재전송
 */
void DrainQueue()
{
    for (int i = 0; i < PENDING_QUEUE_SIZE; i++)
    {
        if (pending_queue[i].active)
        {
            if (pending_queue[i].type == ACTION_SEND)
            {
                has2wifi.Send(pending_queue[i].device_name, pending_queue[i].key, pending_queue[i].value);
                Serial.println("[Recovery] Retried Send: " + pending_queue[i].key + "=" + pending_queue[i].value);
            }
            else if (pending_queue[i].type == ACTION_SITUATION)
            {
                has2wifi.Situation(pending_queue[i].device_name, pending_queue[i].key);
                Serial.println("[Recovery] Retried Situation: " + pending_queue[i].key);
            }
            pending_queue[i].active = false;
        }
    }
}

/**
 * @brief WiFi 상태 확인 후 전송. 끊겼으면 큐에 저장
 */
void SafeSend(String device, String column, String value)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        has2wifi.Send(device, column, value);
    }
    else
    {
        AddToQueue(ACTION_SEND, device, column, value);
    }
}

/**
 * @brief WiFi 상태 확인 후 Situation 전송. 끊겼으면 큐에 저장
 */
void SafeSituation(String device, String situation)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        has2wifi.Situation(device, situation);
    }
    else
    {
        AddToQueue(ACTION_SITUATION, device, situation, "");
    }
}

/**
 * @brief 5초마다 타이머에서 호출. WiFi 재연결 시 큐 드레인
 */
void RetryFunc()
{
    if (WiFi.status() != WL_CONNECTED) return;
    DrainQueue();
}
