#ifndef _SECRETS_H_
#define _SECRETS_H_

#define HMAC_SECRET "replace-with-ota-hmac-secret"
#define GLOVE_WIFI_PROFILE "store2-badland"
#define GLOVE_WIFI_SSID "replace-with-wifi-ssid"
#define GLOVE_WIFI_PASS "replace-with-wifi-password"
// Keep the "http://" scheme and no trailing slash. HAS2_Wifi recovers the bare
// IP with HOST_NAME.substring(7), so "https://" silently yields a wrong address.
#define GLOVE_SERVER_HOST "http://172.30.1.43"
// Numeric form of GLOVE_WIFI_PROFILE for location_protocol.h room tables.
// See HAS3_PROFILE_* there: badland=1, city=2, error=3.
// Nothing cross-checks this against GLOVE_WIFI_PROFILE. CI generates both from
// one table so they cannot drift, but a hand-edited secrets.h can pair the wrong
// two: city SSID/server with badland rooms compiles and reports rooms silently
// wrong. Change GLOVE_WIFI_PROFILE and GLOVE_PROFILE_ID together.
#define GLOVE_PROFILE_ID 1

#endif
