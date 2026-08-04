#ifndef _SECRETS_H_
#define _SECRETS_H_

#define HMAC_SECRET "replace-with-ota-hmac-secret"
#define GLOVE_WIFI_PROFILE "store2-badland"
#define GLOVE_WIFI_SSID "replace-with-wifi-ssid"
#define GLOVE_WIFI_PASS "replace-with-wifi-password"
// Keep the "http://" scheme and no trailing slash. HAS2_Wifi recovers the bare
// IP with HOST_NAME.substring(7), so "https://" silently yields a wrong address.
#define GLOVE_SERVER_HOST "http://172.30.1.43"

#endif
