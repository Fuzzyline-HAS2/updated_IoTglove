#!/usr/bin/env python3
import argparse
import configparser
import json
import os
import pathlib
import subprocess

from firmware_metadata import read_firmware_version, sha256_file


ROOT = pathlib.Path(__file__).resolve().parents[1]


def run_text(command):
    try:
        return subprocess.check_output(command, cwd=ROOT, text=True).strip()
    except Exception:
        return ""


def read_platformio_config(env_name: str):
    config = configparser.RawConfigParser()
    config.optionxform = str
    config.read(ROOT / "platformio.ini", encoding="utf-8")
    section = f"env:{env_name}"
    if not config.has_section(section):
        return {}
    return {key: value.strip() for key, value in config.items(section)}


def parse_lib_deps(raw: str):
    libs = []
    for line in raw.splitlines():
        value = line.strip()
        if not value or value.startswith(";") or value.startswith("#"):
            continue
        libs.append({"source": "platformio", "spec": value})
    return libs


def vendor_libraries():
    vendor_root = ROOT / "lib" / "vendor"
    libs = []
    if not vendor_root.exists():
        return libs
    for path in sorted(vendor_root.iterdir()):
        if path.is_dir():
            metadata = path / "library.json"
            version = "unknown"
            if metadata.exists():
                try:
                    version = json.loads(metadata.read_text(encoding="utf-8")).get(
                        "version", "unknown"
                    )
                except Exception:
                    version = "unknown"
            libs.append({"source": "vendor", "name": path.name, "version": version})
    return libs


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", required=True)
    parser.add_argument("--signature", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--channel", required=True)
    parser.add_argument("--env", default="ttgo-t8-v171")
    parser.add_argument("--profile", required=True)
    parser.add_argument("--firmware-asset", default="update.bin")
    parser.add_argument("--signature-asset", default="update.sig")
    args = parser.parse_args()

    firmware_path = pathlib.Path(args.firmware)
    signature_path = pathlib.Path(args.signature)
    output_path = pathlib.Path(args.output)
    fw = read_firmware_version()
    pio = read_platformio_config(args.env)
    lib_deps = parse_lib_deps(pio.get("lib_deps", ""))
    env_vendor_libraries = vendor_libraries() if pio.get("lib_extra_dirs", "") else []

    build_info = {
        "firmware_version": fw.version,
        "firmware_version_code": fw.version_code,
        "channel": args.channel,
        "wifi_profile": args.profile,
        "git": {
            "repository": os.environ.get("GITHUB_REPOSITORY", ""),
            "tag": os.environ.get("GITHUB_REF_NAME", ""),
            "commit": os.environ.get("GITHUB_SHA") or run_text(["git", "rev-parse", "HEAD"]),
        },
        "build_environment": {
            "builder": "PlatformIO + Docker",
            "dockerfile": "Dockerfile",
            "python_image": "python:3.11.9-slim-bookworm",
            "platformio_core": os.environ.get("PLATFORMIO_CORE_VERSION", "6.1.19"),
            "platform": pio.get("platform", ""),
            "framework": pio.get("framework", ""),
            "board": pio.get("board", ""),
            "platformio_env": args.env,
            "esp32_arduino_core": "3.3.8",
        },
        "libraries": lib_deps + env_vendor_libraries + [
            {"source": "local", "name": "SecureOTA", "version": fw.version}
        ],
        "artifacts": {
            "firmware": {
                "file": args.firmware_asset,
                "size": firmware_path.stat().st_size,
                "sha256": sha256_file(firmware_path),
            },
            "signature": {
                "file": args.signature_asset,
                "size": signature_path.stat().st_size,
                "sha256": sha256_file(signature_path),
            },
        },
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(build_info, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
