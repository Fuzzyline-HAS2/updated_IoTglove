#include "SecureOTA.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <stdarg.h>
#include "mbedtls/md.h"

#define OTA_HTTP_TIMEOUT_MS 30000
#define OTA_SIGNATURE_TIMEOUT_MS 15000
#define OTA_STREAM_WAIT_MS 30000
#define OTA_MAX_MANIFEST_BYTES 2048
#define OTA_MAX_FIRMWARE_BYTES 2000000

SecureOTA::SecureOTA(const char *firmware_url,
                     const char *version_url,
                     const char *signature_url,
                     const char *hmac_secret,
                     int current_version_code)
    : _firmware_url(firmware_url),
      _version_url(version_url),
      _signature_url(signature_url),
      _hmac_secret(hmac_secret),
      _current_version_code(current_version_code),
      _log_stream(nullptr),
      _on_success(nullptr),
      _on_skip(nullptr)
{
}

void SecureOTA::setLogStream(Print &stream)
{
  _log_stream = &stream;
}

void SecureOTA::setOnSuccess(std::function<void()> callback)
{
  _on_success = callback;
}

void SecureOTA::setOnSkip(std::function<void()> callback)
{
  _on_skip = callback;
}

void SecureOTA::_print(const char *msg)
{
  if (_log_stream)
  {
    _log_stream->print(msg);
  }
}

void SecureOTA::_println(const char *msg)
{
  if (_log_stream)
  {
    _log_stream->println(msg);
  }
}

void SecureOTA::_printf(const char *fmt, ...)
{
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  _print(buf);
}

bool SecureOTA::_validUrl(const char *url)
{
  if (url == nullptr || url[0] == '\0')
  {
    return false;
  }
  String text(url);
  return text.startsWith("https://") || text.startsWith("http://");
}

bool SecureOTA::_downloadText(const char *url, String &text, int max_len)
{
  if (!_validUrl(url))
  {
    _println("[OTA] invalid text URL");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(OTA_HTTP_TIMEOUT_MS);

  HTTPClient http;
  http.begin(client, String(url));
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(OTA_HTTP_TIMEOUT_MS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    _printf("[OTA] text download failed HTTP %d\n", httpCode);
    http.end();
    client.stop();
    return false;
  }

  int len = http.getSize();
  if (len > max_len)
  {
    _printf("[OTA] text too large: %d bytes\n", len);
    http.end();
    client.stop();
    return false;
  }

  text = http.getString();
  text.trim();
  http.end();
  client.stop();

  if (text.length() == 0 || text.length() > max_len)
  {
    _printf("[OTA] invalid text length: %d bytes\n", text.length());
    return false;
  }
  return true;
}

bool SecureOTA::_downloadSignature(const char *signature_url, uint8_t sig[32])
{
  if (!_validUrl(signature_url))
  {
    _println("[OTA] invalid signature URL");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(OTA_HTTP_TIMEOUT_MS);

  HTTPClient http;
  http.begin(client, String(signature_url));
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(OTA_SIGNATURE_TIMEOUT_MS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    _printf("[OTA] signature download failed HTTP %d\n", httpCode);
    http.end();
    client.stop();
    return false;
  }

  int len = http.getSize();
  if (len != 32)
  {
    _printf("[OTA] signature must be raw 32 bytes, got %d\n", len);
    http.end();
    client.stop();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t bytesRead = stream->readBytes(sig, 32);
  http.end();
  client.stop();

  if (bytesRead != 32)
  {
    _printf("[OTA] incomplete signature read: %d bytes\n", (int)bytesRead);
    return false;
  }
  return true;
}

bool SecureOTA::_verifySignature(const uint8_t computed[32], const uint8_t downloaded[32])
{
  return memcmp(computed, downloaded, 32) == 0;
}

int SecureOTA::_checkServerVersionCode()
{
  String versionText;
  if (!_downloadText(_version_url, versionText, 32))
  {
    return -1;
  }

  int serverVersionCode = versionText.toInt();
  _printf("[OTA] server code %d / current code %d\n", serverVersionCode, _current_version_code);
  return serverVersionCode;
}

bool SecureOTA::_execOTA(const char *firmware_url, const char *signature_url, int expected_size)
{
  if (!_validUrl(firmware_url) || !_validUrl(signature_url))
  {
    _println("[OTA] invalid firmware or signature URL");
    return false;
  }

  uint8_t downloaded_sig[32];
  _println("[OTA] downloading signature");
  if (!_downloadSignature(signature_url, downloaded_sig))
  {
    _println("[OTA] signature download failed");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(OTA_HTTP_TIMEOUT_MS);

  HTTPClient http;
  http.begin(client, String(firmware_url));
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(OTA_HTTP_TIMEOUT_MS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    _printf("[OTA] firmware download failed HTTP %d\n", httpCode);
    http.end();
    client.stop();
    return false;
  }

  int contentLength = http.getSize();
  _printf("[OTA] firmware size: %d bytes\n", contentLength);
  if (contentLength <= 0 || contentLength > OTA_MAX_FIRMWARE_BYTES)
  {
    _println("[OTA] invalid firmware size");
    http.end();
    client.stop();
    return false;
  }

  if (expected_size > 0 && contentLength != expected_size)
  {
    _printf("[OTA] manifest size mismatch: %d != %d\n", contentLength, expected_size);
    http.end();
    client.stop();
    return false;
  }

  if (!Update.begin(contentLength))
  {
    _println("[OTA] not enough OTA space");
    http.end();
    client.stop();
    return false;
  }

  mbedtls_md_context_t hmac_ctx;
  mbedtls_md_init(&hmac_ctx);
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mbedtls_md_setup(&hmac_ctx, md_info, 1) != 0 ||
      mbedtls_md_hmac_starts(&hmac_ctx, (const unsigned char *)_hmac_secret, strlen(_hmac_secret)) != 0)
  {
    _println("[OTA] HMAC init failed");
    mbedtls_md_free(&hmac_ctx);
    Update.abort();
    http.end();
    client.stop();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t written = 0;
  int remaining = contentLength;
  bool streamError = false;

  while (remaining > 0)
  {
    unsigned long wait_start = millis();
    while (stream->available() == 0)
    {
      if (millis() - wait_start > OTA_STREAM_WAIT_MS)
      {
        _println("[OTA] stream timeout");
        streamError = true;
        break;
      }
      delay(10);
    }
    if (streamError)
    {
      break;
    }

    int toRead = min((int)stream->available(), min(remaining, (int)sizeof(buf)));
    int bytesRead = stream->readBytes(buf, toRead);
    if (bytesRead <= 0)
    {
      continue;
    }

    size_t writeSize = Update.write(buf, bytesRead);
    if (writeSize != (size_t)bytesRead)
    {
      _println("[OTA] firmware write failed");
      streamError = true;
      break;
    }

    mbedtls_md_hmac_update(&hmac_ctx, buf, bytesRead);
    written += bytesRead;
    remaining -= bytesRead;
  }

  uint8_t computed_sig[32];
  mbedtls_md_hmac_finish(&hmac_ctx, computed_sig);
  mbedtls_md_free(&hmac_ctx);

  if (streamError || written != (size_t)contentLength)
  {
    _printf("[OTA] incomplete firmware download: %d / %d\n", (int)written, contentLength);
    Update.abort();
    http.end();
    client.stop();
    return false;
  }

  if (!_verifySignature(computed_sig, downloaded_sig))
  {
    _println("[OTA] signature verification failed");
    Update.abort();
    http.end();
    client.stop();
    return false;
  }

  if (!Update.end(true))
  {
    _printf("[OTA] update failed: %d\n", Update.getError());
    Update.abort();
    http.end();
    client.stop();
    return false;
  }

  if (!Update.isFinished())
  {
    _println("[OTA] update not finished");
    http.end();
    client.stop();
    return false;
  }

  _println("[OTA] update complete");
  http.end();
  client.stop();

  if (_on_success)
  {
    _on_success();
    delay(2000);
  }

  delay(3000);
  ESP.restart();
  return true;
}

bool SecureOTA::check()
{
  _println("[OTA] raw URL check");
  if (WiFi.status() != WL_CONNECTED)
  {
    _println("[OTA] WiFi disconnected");
    return false;
  }

  int serverVersionCode = _checkServerVersionCode();
  if (serverVersionCode < 0)
  {
    return false;
  }

  if (serverVersionCode <= _current_version_code)
  {
    _printf("[OTA] skip raw OTA: %d <= %d\n", serverVersionCode, _current_version_code);
    if (_on_skip)
    {
      _on_skip();
    }
    return true;
  }

  return _execOTA(_firmware_url, _signature_url, 0);
}

bool SecureOTA::checkManifest(const char *manifest_url,
                              const char *expected_channel,
                              bool allow_downgrade,
                              const char *expected_profile)
{
  _println("[OTA] manifest check");
  if (WiFi.status() != WL_CONNECTED)
  {
    _println("[OTA] WiFi disconnected");
    return false;
  }

  String manifestText;
  if (!_downloadText(manifest_url, manifestText, OTA_MAX_MANIFEST_BYTES))
  {
    return false;
  }

  StaticJsonDocument<1024> manifest;
  DeserializationError error = deserializeJson(manifest, manifestText);
  if (error)
  {
    _printf("[OTA] manifest JSON error: %s\n", error.c_str());
    return false;
  }

  const char *channel = manifest["channel"] | "";
  const char *wifiProfile = manifest["wifi_profile"] | "";
  const char *version = manifest["version"] | "";
  int versionCode = manifest["version_code"] | -1;
  const char *firmwareUrl = manifest["firmware_url"] | "";
  const char *signatureUrl = manifest["signature_url"] | "";
  int expectedSize = manifest["size"] | 0;

  if (expected_channel != nullptr && expected_channel[0] != '\0' && strcmp(channel, expected_channel) != 0)
  {
    _printf("[OTA] channel mismatch: %s != %s\n", channel, expected_channel);
    return false;
  }

  if (expected_profile != nullptr && expected_profile[0] != '\0' && strcmp(wifiProfile, expected_profile) != 0)
  {
    _printf("[OTA] WiFi profile mismatch: %s != %s\n", wifiProfile, expected_profile);
    return false;
  }

  if (versionCode <= 0)
  {
    _println("[OTA] invalid manifest version_code");
    return false;
  }

  _printf("[OTA] manifest %s code %d / current %d\n", version, versionCode, _current_version_code);
  if (versionCode == _current_version_code)
  {
    _println("[OTA] manifest version is already installed");
    if (_on_skip)
    {
      _on_skip();
    }
    return true;
  }

  if (versionCode < _current_version_code && !allow_downgrade)
  {
    _println("[OTA] manifest version is older");
    if (_on_skip)
    {
      _on_skip();
    }
    return true;
  }

  if (versionCode < _current_version_code)
  {
    _println("[OTA] downgrade allowed");
  }

  if (!_validUrl(firmwareUrl) || !_validUrl(signatureUrl))
  {
    _println("[OTA] manifest URL is invalid");
    return false;
  }

  return _execOTA(firmwareUrl, signatureUrl, expectedSize);
}
