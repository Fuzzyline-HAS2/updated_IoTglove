#ifndef SECURE_OTA_H
#define SECURE_OTA_H

#include <Arduino.h>
#include <Print.h>
#include <functional>

class SecureOTA
{
public:
  SecureOTA(const char *firmware_url,
            const char *version_url,
            const char *signature_url,
            const char *hmac_secret,
            int current_version_code);

  void setLogStream(Print &stream);
  void setOnSuccess(std::function<void()> callback);
  void setOnSkip(std::function<void()> callback);

  bool check();
  bool checkManifest(const char *manifest_url,
                     const char *expected_channel,
                     bool allow_downgrade = false,
                     const char *expected_profile = "");

private:
  const char *_firmware_url;
  const char *_version_url;
  const char *_signature_url;
  const char *_hmac_secret;
  int _current_version_code;
  Print *_log_stream;
  std::function<void()> _on_success;
  std::function<void()> _on_skip;

  void _print(const char *msg);
  void _println(const char *msg);
  void _printf(const char *fmt, ...);

  bool _validUrl(const char *url);
  bool _downloadText(const char *url, String &text, int max_len);
  bool _downloadSignature(const char *signature_url, uint8_t sig[32]);
  bool _verifySignature(const uint8_t computed[32], const uint8_t downloaded[32]);
  int _checkServerVersionCode();
  bool _execOTA(const char *firmware_url, const char *signature_url, int expected_size);
};

#endif
