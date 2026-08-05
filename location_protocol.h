#ifndef HAS3_LOCATION_PROTOCOL_H
#define HAS3_LOCATION_PROTOCOL_H

#include <Arduino.h>

#define HAS3_ROOM_COUNT 6
#define HAS3_UNKNOWN_ROOM "unknown"
#define HAS3_LOCAL_NAME_PREFIX "HAS3:"
#define HAS3_MAX_DEVICE_NAME_LEN 18
#define HAS3_DEVICE_NAME_BUFFER_SIZE (HAS3_MAX_DEVICE_NAME_LEN + 1)

// Room names differ per store, so the table is selected at compile time. Must
// stay in sync with PROFILE_IDS / PROFILE_ROOMS in scripts/release_profiles.py.
#define HAS3_PROFILE_STORE2_BADLAND 1
#define HAS3_PROFILE_STORE2_CITY 2
#define HAS3_PROFILE_STORE3_ERROR 3

#ifndef GLOVE_PROFILE_ID
#error "GLOVE_PROFILE_ID must be defined in secrets.h"
#endif

#if GLOVE_PROFILE_ID == HAS3_PROFILE_STORE2_BADLAND
static const char *const HAS3_ROOMS[HAS3_ROOM_COUNT] = {
    "prison",
    "ruins",
    "checkpoint",
    "shoot",
    "warehouse",
    "academy"};
#elif GLOVE_PROFILE_ID == HAS3_PROFILE_STORE2_CITY
static const char *const HAS3_ROOMS[HAS3_ROOM_COUNT] = {
    "house",
    "office",
    "bar",
    "gunshop",
    "foodcourt",
    "academy"};
#elif GLOVE_PROFILE_ID == HAS3_PROFILE_STORE3_ERROR
static const char *const HAS3_ROOMS[HAS3_ROOM_COUNT] = {
    "bamboo",
    "toilet",
    "sleep",
    "underground",
    "hallway",
    "crack"};
#else
#error "Unknown GLOVE_PROFILE_ID; see HAS3_PROFILE_* in location_protocol.h"
#endif

inline int Has3RoomIndex(const char *room)
{
  if (room == nullptr)
  {
    return -1;
  }

  for (int i = 0; i < HAS3_ROOM_COUNT; i++)
  {
    if (strcmp(room, HAS3_ROOMS[i]) == 0)
    {
      return i;
    }
  }
  return -1;
}

inline bool Has3IsValidRoom(const char *room)
{
  return Has3RoomIndex(room) >= 0 || (room != nullptr && strcmp(room, HAS3_UNKNOWN_ROOM) == 0);
}

// A device name starts with its room's initial, uppercased: "bar itembox 1"
// advertises as BI1. Deriving the prefix from HAS3_ROOMS keeps the room table
// the only place a room or its initial is declared.
inline int Has3RoomIndexFromPrefix(char prefix)
{
  for (int i = 0; i < HAS3_ROOM_COUNT; i++)
  {
    char initial = HAS3_ROOMS[i][0];
    if (initial >= 'a' && initial <= 'z')
    {
      initial = (char)(initial - 'a' + 'A');
    }
    if (initial == prefix)
    {
      return i;
    }
  }
  return -1;
}

inline bool Has3IsDevicePrefix(char prefix)
{
  return Has3RoomIndexFromPrefix(prefix) >= 0;
}

inline bool Has3IsValidDeviceName(const char *device_name)
{
  if (device_name == nullptr)
  {
    return false;
  }

  size_t len = strlen(device_name);
  if (len < 2 || len > HAS3_MAX_DEVICE_NAME_LEN || !Has3IsDevicePrefix(device_name[0]))
  {
    return false;
  }

  for (size_t i = 0; i < len; i++)
  {
    char ch = device_name[i];
    bool ok = (ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '-';
    if (!ok)
    {
      return false;
    }
  }

  return true;
}

inline const char *Has3RoomFromDeviceName(const char *device_name)
{
  if (!Has3IsValidDeviceName(device_name))
  {
    return nullptr;
  }

  int room_index = Has3RoomIndexFromPrefix(device_name[0]);
  if (room_index < 0)
  {
    return nullptr;
  }
  return HAS3_ROOMS[room_index];
}

inline bool Has3ParseLocalName(const char *local_name, char *device_name, size_t device_name_size)
{
  if (local_name == nullptr || strncmp(local_name, HAS3_LOCAL_NAME_PREFIX, 5) != 0)
  {
    return false;
  }

  const char *name = local_name + 5;
  if (!Has3IsValidDeviceName(name))
  {
    return false;
  }

  size_t name_len = strlen(name);
  if (device_name != nullptr && device_name_size <= name_len)
  {
    return false;
  }

  if (device_name != nullptr)
  {
    strncpy(device_name, name, device_name_size);
    device_name[device_name_size - 1] = '\0';
  }
  return true;
}

inline bool Has3PayloadHasCompleteLocalName(const uint8_t *payload, size_t payload_len, const char *expected_name)
{
  if (payload == nullptr || expected_name == nullptr)
  {
    return false;
  }

  size_t expected_len = strlen(expected_name);
  size_t pos = 0;
  while (pos < payload_len)
  {
    uint8_t field_len = payload[pos];
    if (field_len == 0)
    {
      break;
    }

    if (pos + field_len >= payload_len + 1)
    {
      break;
    }

    uint8_t ad_type = payload[pos + 1];
    size_t value_len = field_len - 1;
    const uint8_t *value = payload + pos + 2;
    if (ad_type == 0x09 && value_len == expected_len && memcmp(value, expected_name, expected_len) == 0)
    {
      return true;
    }

    pos += field_len + 1;
  }

  return false;
}

#endif
