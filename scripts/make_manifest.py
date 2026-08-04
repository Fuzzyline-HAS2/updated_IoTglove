#!/usr/bin/env python3
import argparse
import json
import pathlib

from firmware_metadata import channel_for, read_firmware_version, sha256_file


def release_url(repo: str, tag: str, asset: str) -> str:
    return f"https://github.com/{repo}/releases/download/{tag}/{asset}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, help="owner/repo")
    parser.add_argument("--tag", required=True)
    parser.add_argument("--channel", default="")
    parser.add_argument("--profile", required=True)
    parser.add_argument("--firmware", required=True)
    parser.add_argument("--signature", required=True)
    parser.add_argument("--build-info", required=True)
    parser.add_argument("--firmware-asset", default="update.bin")
    parser.add_argument("--signature-asset", default="update.sig")
    parser.add_argument("--build-info-asset", default="build-info.json")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    firmware_path = pathlib.Path(args.firmware)
    signature_path = pathlib.Path(args.signature)
    build_info_path = pathlib.Path(args.build_info)
    firmware = read_firmware_version()
    channel = args.channel or channel_for(args.tag)

    manifest = {
        "channel": channel,
        "wifi_profile": args.profile,
        "version": firmware.version,
        "version_code": firmware.version_code,
        "firmware_url": release_url(args.repo, args.tag, args.firmware_asset),
        "signature_url": release_url(args.repo, args.tag, args.signature_asset),
        "build_info_url": release_url(args.repo, args.tag, args.build_info_asset),
        "size": firmware_path.stat().st_size,
        "firmware_sha256": sha256_file(firmware_path),
        "signature_sha256": sha256_file(signature_path),
        "build_info_sha256": sha256_file(build_info_path),
    }

    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
