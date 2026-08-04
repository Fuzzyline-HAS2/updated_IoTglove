import pathlib
import subprocess
import tempfile
import unittest

from scripts.release_profiles import (
    PROFILES,
    PROFILE_SERVERS,
    PROFILE_SSIDS,
    asset_names,
    c_string,
    write_secrets_header,
)


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ReleaseProfilesTest(unittest.TestCase):
    def test_expected_profiles_have_unique_assets(self):
        self.assertEqual(
            PROFILES,
            ("store2-badland", "store2-city", "store3-error"),
        )
        all_assets = []
        for profile in PROFILES:
            names = asset_names("prd", profile)
            self.assertEqual(
                names["ttgo_manifest"], f"manifest-prd-{profile}.json"
            )
            self.assertEqual(
                names["beetle_manifest"], f"beetle-manifest-prd-{profile}.json"
            )
            all_assets.extend(names.values())
        self.assertEqual(len(all_assets), len(set(all_assets)))

    def test_secret_header_contains_profile_and_escapes_values(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "secrets.h"
            write_secrets_header(
                output,
                profile="store2-city",
                ssid="bar",
                wifi_password="password\\value",
                hmac_secret="hmac-secret",
            )
            text = output.read_text(encoding="utf-8")
        self.assertIn('#define GLOVE_WIFI_PROFILE "store2-city"', text)
        self.assertIn('#define GLOVE_WIFI_SSID "bar"', text)
        self.assertIn('#define GLOVE_WIFI_PASS "password\\\\value"', text)
        self.assertIn('#define GLOVE_SERVER_HOST "http://172.30.1.44"', text)
        self.assertEqual(c_string('bar"room\\ap'), '"bar\\\"room\\\\ap"')

    def test_profile_ssid_mapping_is_exact(self):
        self.assertEqual(
            PROFILE_SSIDS,
            {
                "store2-badland": "badland_ruins",
                "store2-city": "bar",
                "store3-error": "badland_shoot",
            },
        )
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "Wi-Fi SSID mismatch"):
                write_secrets_header(
                    pathlib.Path(directory) / "secrets.h",
                    profile="store2-city",
                    ssid="badland_ruins",
                    wifi_password="password",
                    hmac_secret="hmac-secret",
                )

    def test_profile_server_mapping_is_exact(self):
        self.assertEqual(
            PROFILE_SERVERS,
            {
                "store2-badland": "http://172.30.1.43",
                "store2-city": "http://172.30.1.44",
                "store3-error": "http://172.30.1.43",
            },
        )

    def test_profile_servers_keep_substring_parsable_form(self):
        # HAS2_Wifi.cpp calls HOST_NAME.substring(7) to recover the bare IP, so
        # the scheme must be exactly "http://" and no trailing slash may follow.
        for profile, host in PROFILE_SERVERS.items():
            self.assertTrue(host.startswith("http://"), profile)
            self.assertFalse(host.endswith("/"), profile)
            self.assertTrue(host[7:], profile)

    def test_every_profile_has_a_server(self):
        self.assertEqual(set(PROFILE_SERVERS), set(PROFILES))

    def test_unknown_profile_is_rejected(self):
        with self.assertRaises(ValueError):
            asset_names("prd", "unknown")

    def test_firmware_reads_server_host_from_secrets(self):
        header = (ROOT / "updated_IoTglove.h").read_text(encoding="utf-8")
        self.assertIn("#ifndef GLOVE_SERVER_HOST", header)
        self.assertIn("HAS2_Wifi has2wifi(GLOVE_SERVER_HOST)", header)

        sensor = (ROOT / "sensor.ino").read_text(encoding="utf-8")
        self.assertIn("GLOVE_SERVER_HOST", sensor)

        main = (ROOT / "updated_IoTglove.ino").read_text(encoding="utf-8")
        self.assertIn("GLOVE_SERVER_HOST", main)

    def test_server_host_is_not_hardcoded_in_firmware_sources(self):
        # The vendored HAS2_Wifi library keeps unreachable defaults on purpose and
        # secrets.example.h is where GLOVE_SERVER_HOST is defined; every other
        # tracked firmware source must go through the macro.
        suffixes = {".cpp", ".h", ".ino"}
        exempt = {"secrets.example.h"}
        tracked = subprocess.check_output(
            ["git", "ls-files"], cwd=ROOT, text=True
        ).splitlines()
        for relative_path in tracked:
            if relative_path.startswith("lib/vendor/") or relative_path in exempt:
                continue
            path = ROOT / relative_path
            if path.suffix in suffixes:
                self.assertNotIn(
                    "172.30.1.",
                    path.read_text(encoding="utf-8"),
                    f"Hardcoded server address found in {relative_path}",
                )

    def test_firmware_routes_ota_by_compiled_profile(self):
        ttgo = (ROOT / "game_state.ino").read_text(encoding="utf-8")
        beetle = (ROOT / "wifi_location" / "beetle_ota.ino").read_text(
            encoding="utf-8"
        )
        self.assertIn('channel + "-" + GLOVE_WIFI_PROFILE + ".json"', ttgo)
        self.assertIn('channel + "-" + GLOVE_WIFI_PROFILE + ".json"', beetle)
        self.assertIn("true, GLOVE_WIFI_PROFILE", ttgo)
        self.assertIn("true, GLOVE_WIFI_PROFILE", beetle)

        secure_ota = (ROOT / "SecureOTA.cpp").read_text(encoding="utf-8")
        self.assertIn('manifest["wifi_profile"]', secure_ota)
        self.assertIn("WiFi profile mismatch", secure_ota)

    def test_workflow_uses_repository_variables_and_shared_password_secret(self):
        workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
            encoding="utf-8"
        )
        for variable in (
            "GLOVE_WIFI_SSID_STORE2_BADLAND",
            "GLOVE_WIFI_SSID_STORE2_CITY",
            "GLOVE_WIFI_SSID_STORE3_ERROR",
        ):
            self.assertIn(f"vars.{variable}", workflow)
        self.assertIn("secrets.GLOVE_WIFI_PASS", workflow)
        self.assertNotIn("Code3824", workflow)

    def test_wifi_password_is_not_hardcoded_in_firmware_sources(self):
        forbidden_password = "Code" + "3824" + "@"
        suffixes = {".cpp", ".h", ".ino", ".yml", ".yaml"}
        tracked = subprocess.check_output(
            ["git", "ls-files"], cwd=ROOT, text=True
        ).splitlines()
        for relative_path in tracked:
            path = ROOT / relative_path
            if path.suffix in suffixes:
                self.assertNotIn(
                    forbidden_password,
                    path.read_text(encoding="utf-8"),
                    f"Wi-Fi password literal found in {path.relative_to(ROOT)}",
                )


if __name__ == "__main__":
    unittest.main()
