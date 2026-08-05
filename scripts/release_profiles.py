#!/usr/bin/env python3
"""Release profile helpers shared by GitHub Actions and local verification."""

from __future__ import annotations

import argparse
import json
import os
import pathlib


PROFILE_SSIDS = {
    "store2-badland": "badland_ruins",
    "store2-city": "bar",
    "store3-error": "badland_shoot",
}
# HAS2 server per profile. Not a secret, so the table is the single source of
# truth instead of a GitHub Actions variable. Keep the "http://" scheme and no
# trailing slash: HAS2_Wifi.cpp recovers the bare IP with HOST_NAME.substring(7).
PROFILE_SERVERS = {
    "store2-badland": "http://172.30.1.43",
    "store2-city": "http://172.30.1.44",
    "store3-error": "http://172.30.1.43",
}
# Numeric ids let location_protocol.h pick a room table with #if. The C
# preprocessor cannot compare GLOVE_WIFI_PROFILE, which is a string literal.
# Must stay in sync with the HAS3_PROFILE_* constants in location_protocol.h.
PROFILE_IDS = {
    "store2-badland": 1,
    "store2-city": 2,
    "store3-error": 3,
}
# BLE location rooms per profile, in HAS3_ROOMS order. A device name carries its
# room's initial as the first character (bar itembox 1 -> BI1), so the initials
# must stay unique within a profile. Room counts must stay equal across profiles
# because HAS3_ROOM_COUNT is a shared compile-time array bound.
PROFILE_ROOMS = {
    "store2-badland": (
        "prison",
        "ruins",
        "checkpoint",
        "shoot",
        "warehouse",
        "academy",
    ),
    "store2-city": (
        "house",
        "office",
        "bar",
        "gunshop",
        "foodcourt",
        "academy",
    ),
    "store3-error": (
        "bamboo",
        "toilet",
        "sleep",
        "underground",
        "hallway",
        "crack",
    ),
}
PROFILES = tuple(PROFILE_SSIDS)


def validate_profile(profile: str) -> str:
    if profile not in PROFILES:
        raise ValueError(
            f"Unknown release profile {profile!r}; expected one of {', '.join(PROFILES)}"
        )
    return profile


def c_string(value: str) -> str:
    """Escape a value for a C string literal without exposing it in logs."""
    return json.dumps(value, ensure_ascii=False)


def write_secrets_header(
    output: pathlib.Path,
    *,
    profile: str,
    ssid: str,
    wifi_password: str,
    hmac_secret: str,
) -> None:
    validate_profile(profile)
    required = {
        "Wi-Fi SSID": ssid,
        "Wi-Fi password": wifi_password,
        "HMAC secret": hmac_secret,
    }
    missing = [name for name, value in required.items() if not value]
    if missing:
        raise ValueError(f"Missing required value(s): {', '.join(missing)}")

    expected_ssid = PROFILE_SSIDS[profile]
    if ssid != expected_ssid:
        raise ValueError(
            f"Wi-Fi SSID mismatch for {profile}: expected {expected_ssid!r}"
        )

    output.write_text(
        "#ifndef _SECRETS_H_\n"
        "#define _SECRETS_H_\n\n"
        f"#define HMAC_SECRET {c_string(hmac_secret)}\n"
        f"#define GLOVE_WIFI_PROFILE {c_string(profile)}\n"
        f"#define GLOVE_WIFI_SSID {c_string(ssid)}\n"
        f"#define GLOVE_WIFI_PASS {c_string(wifi_password)}\n"
        f"#define GLOVE_SERVER_HOST {c_string(PROFILE_SERVERS[profile])}\n"
        f"#define GLOVE_PROFILE_ID {PROFILE_IDS[profile]}\n\n"
        "#endif\n",
        encoding="utf-8",
    )


def asset_names(channel: str, profile: str) -> dict[str, str]:
    validate_profile(profile)
    if channel not in {"dev", "rc", "prd"}:
        raise ValueError(f"Invalid release channel: {channel!r}")
    return {
        "ttgo_firmware": f"update-{profile}.bin",
        "ttgo_signature": f"update-{profile}.sig",
        "ttgo_build_info": f"build-info-{profile}.json",
        "ttgo_manifest": f"manifest-{channel}-{profile}.json",
        "beetle_firmware": f"beetle-update-{profile}.bin",
        "beetle_signature": f"beetle-update-{profile}.sig",
        "beetle_build_info": f"beetle-build-info-{profile}.json",
        "beetle_manifest": f"beetle-manifest-{channel}-{profile}.json",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    secrets_parser = subparsers.add_parser("write-secrets")
    secrets_parser.add_argument("--profile", required=True)
    secrets_parser.add_argument("--output", default="secrets.h")

    assets_parser = subparsers.add_parser("assets")
    assets_parser.add_argument("--profile", required=True)
    assets_parser.add_argument("--channel", required=True)

    args = parser.parse_args()
    if args.command == "write-secrets":
        write_secrets_header(
            pathlib.Path(args.output),
            profile=args.profile,
            ssid=os.environ.get("GLOVE_WIFI_SSID_VALUE", ""),
            wifi_password=os.environ.get("GLOVE_WIFI_PASS_VALUE", ""),
            hmac_secret=os.environ.get("HMAC_SECRET_VALUE", ""),
        )
        print(f"Wrote {args.output} for profile {args.profile}")
        return

    for name, value in asset_names(args.channel, args.profile).items():
        print(f"{name}={value}")


if __name__ == "__main__":
    main()
