#include "wifi_location.h"

void InitBeetleOta()
{
  beetle_ota.setLogStream(Serial);
  beetle_ota.setOnSuccess([]() {
    MySerial1.print("beetle_ota_done\n");
  });
  beetle_ota.setOnSkip([]() {
    MySerial1.print("beetle_ota_skip\n");
  });
}

void StopBeetleWifi()
{
  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
}

bool IsValidOtaChannel(const String &channel)
{
  return channel == "dev" || channel == "rc" || channel == "prd";
}

bool IsSafeOtaTag(const String &tag)
{
  if (tag.length() == 0 || tag.length() > 48)
  {
    return false;
  }

  for (unsigned int i = 0; i < tag.length(); i++)
  {
    char c = tag.charAt(i);
    bool ok = (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_';
    if (!ok)
    {
      return false;
    }
  }
  return true;
}

bool IsGithubCommand(const String &command)
{
  return command == "github" ||
         command.startsWith("github_") ||
         command.startsWith("github@");
}

void ParseOtaCommand(const String &command, String &channel, String &tag)
{
  channel = BEETLE_OTA_CHANNEL;
  tag = "";

  if (command.startsWith("github_"))
  {
    String rest = command.substring(7);
    int tag_pos = rest.indexOf('@');
    if (tag_pos >= 0)
    {
      channel = rest.substring(0, tag_pos);
      tag = rest.substring(tag_pos + 1);
    }
    else
    {
      channel = rest;
    }
  }
  else if (command.startsWith("github@"))
  {
    tag = command.substring(7);
  }

  channel.trim();
  tag.trim();

  if (!IsValidOtaChannel(channel))
  {
    channel = BEETLE_OTA_CHANNEL;
  }
}

String BuildTaggedBeetleManifestUrl(const String &channel, const String &tag)
{
  return String(BEETLE_OTA_RELEASE_BASE_URL) + "/" + tag + "/beetle-manifest-" + channel + "-" + GLOVE_WIFI_PROFILE + ".json";
}

String ResolveBeetleManifestUrl(const String &channel, const String &tag)
{
  if (tag.length() > 0)
  {
    if (!IsSafeOtaTag(tag))
    {
      return String("");
    }
    return BuildTaggedBeetleManifestUrl(channel, tag);
  }

  if (strlen(BEETLE_OTA_MANIFEST_URL) > 0)
  {
    return String(BEETLE_OTA_MANIFEST_URL);
  }

  if (channel == "dev")
  {
    return String(BEETLE_OTA_DEV_MANIFEST_URL);
  }
  if (channel == "rc")
  {
    return String(BEETLE_OTA_RC_MANIFEST_URL);
  }
  return String(BEETLE_OTA_PRD_MANIFEST_URL);
}

bool EnsureWifiConnected()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.println("[OTA] connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(GLOVE_WIFI_SSID, GLOVE_WIFI_PASS);

  unsigned long started_ms = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started_ms < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  bool connected = WiFi.status() == WL_CONNECTED;
  Serial.println(connected ? "[OTA] WiFi connected" : "[OTA] WiFi connect failed");
  return connected;
}

void RunBeetleOta(const String &command)
{
  String channel;
  String tag;
  ParseOtaCommand(command, channel, tag);

  Serial.println("[OTA] command accepted");
  Serial.print("[OTA] channel=");
  Serial.print(channel);
  Serial.print(" tag=");
  Serial.println(tag.length() > 0 ? tag : "(latest)");
  MySerial1.print("beetle_ota_start\n");

  if (!EnsureWifiConnected())
  {
    MySerial1.print("beetle_ota_error\n");
    StopBeetleWifi();
    return;
  }

  String manifest_url = ResolveBeetleManifestUrl(channel, tag);
  if (manifest_url.length() == 0)
  {
    MySerial1.print("beetle_ota_error\n");
    StopBeetleWifi();
    return;
  }

  bool ok = beetle_ota.checkManifest(manifest_url.c_str(), channel.c_str(), true, GLOVE_WIFI_PROFILE);
  if (!ok)
  {
    MySerial1.print("beetle_ota_error\n");
  }

  StopBeetleWifi();
}
