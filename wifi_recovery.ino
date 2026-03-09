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
 * @brief 큐가 완전히 비었는지 확인
 */
bool IsQueueEmpty()
{
    for (int i = 0; i < PENDING_QUEUE_SIZE; i++)
        if (pending_queue[i].active) return false;
    return true;
}

/**
 * @brief 대기 큐를 순서대로 재전송. 성공한 것만 큐에서 제거
 */
void DrainQueue()
{
    for (int i = 0; i < PENDING_QUEUE_SIZE; i++)
    {
        if (!pending_queue[i].active) continue;

        bool success = false;
        if (pending_queue[i].type == ACTION_SEND)
        {
            success = has2wifi.Send(pending_queue[i].device_name, pending_queue[i].key, pending_queue[i].value);
            Serial.println("[Recovery] Retry Send " + String(success ? "OK" : "FAIL") + ": " + pending_queue[i].key + "=" + pending_queue[i].value);
        }
        else if (pending_queue[i].type == ACTION_SITUATION)
        {
            success = has2wifi.Situation(pending_queue[i].device_name, pending_queue[i].key);
            Serial.println("[Recovery] Retry Situation " + String(success ? "OK" : "FAIL") + ": " + pending_queue[i].key);
        }

        if (success)
            pending_queue[i].active = false; // 성공한 것만 제거, 실패는 다음 retry에서 재시도
    }
}

/**
 * @brief WiFi 상태 확인 후 전송. 실패하면 큐에 저장
 */
void SafeSend(String device, String column, String value)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        AddToQueue(ACTION_SEND, device, column, value);
        return;
    }
    bool success = has2wifi.Send(device, column, value);
    if (!success)
    {
        AddToQueue(ACTION_SEND, device, column, value);
    }
}

/**
 * @brief WiFi 상태 확인 후 Situation 전송. 실패하면 큐에 저장
 */
void SafeSituation(String device, String situation)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        AddToQueue(ACTION_SITUATION, device, situation, "");
        return;
    }
    bool success = has2wifi.Situation(device, situation);
    if (!success)
    {
        AddToQueue(ACTION_SITUATION, device, situation, "");
    }
}

/**
 * @brief 2초마다 실행.
 *        - 큐에 항목이 있으면 재전송 시도 (성공한 것만 제거)
 *        - 재연결 직후(just_reconnected)이고 큐가 완전히 비워졌을 때만 서버 동기화
 *          (큐 남아있으면 동기화 보류 → 다음 retry에서 재시도)
 */
void RetryPending()
{
    if (WiFi.status() != WL_CONNECTED) return;
    if (!just_reconnected && IsQueueEmpty()) return; // 할 일 없음

    DrainQueue();

    // 재연결 직후이고 큐까지 비워졌을 때만 서버 상태 동기화
    if (just_reconnected && IsQueueEmpty())
    {
        just_reconnected = false;
        Serial.println("[Recovery] Queue empty after reconnect, syncing server state...");
        has2wifi.ReceiveMine();
        DataChange();
    }
}
