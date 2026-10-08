#!/usr/bin/env python3
"""Upload firmware to the grinder over WiFi (the Muse add-on's POST /ota).

Usage:
    python3 tools/wifi_upload.py                      # V1 build, musegrinder.local
    python3 tools/wifi_upload.py --env waveshare-esp32s3-touch-amoled-164-v2
    python3 tools/wifi_upload.py --host 192.168.1.21 --firmware path/to/firmware.bin

The password comes from .ota_password at the repo root, which the firmware
build generates and compiles in (tools/build-scripts/pre_ota_secret.py).
The grinder rejects images for a different board revision and refuses
updates while grinding.
"""

import argparse
import http.client
import json
import os
import socket
import sys
import time
import uuid

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_ENV = "waveshare-esp32s3-touch-amoled-164"
CHUNK = 16 * 1024


def resolve(host):
    try:
        return socket.gethostbyname(host)
    except socket.gaierror:
        sys.exit(f"Could not resolve {host}. Is the grinder on WiFi? Try --host <ip>.")


def get_status(ip, timeout=5):
    conn = http.client.HTTPConnection(ip, 80, timeout=timeout)
    try:
        conn.request("GET", "/status")
        return json.loads(conn.getresponse().read())
    finally:
        conn.close()


def upload(ip, firmware, password):
    boundary = uuid.uuid4().hex
    head = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode()
    tail = f"\r\n--{boundary}--\r\n".encode()
    size = os.path.getsize(firmware)

    conn = http.client.HTTPConnection(ip, 80, timeout=120)
    conn.putrequest("POST", "/ota")
    conn.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
    conn.putheader("Content-Length", str(len(head) + size + len(tail)))
    conn.putheader("X-OTA-Password", password)
    conn.endheaders()

    conn.send(head)
    sent, start = 0, time.time()
    with open(firmware, "rb") as f:
        while chunk := f.read(CHUNK):
            conn.send(chunk)
            sent += len(chunk)
            pct = 100 * sent // size
            rate = sent / 1024 / max(time.time() - start, 0.001)
            print(f"\r[UPLOAD] {pct:3d}%  {sent // 1024} / {size // 1024} KB  ({rate:.0f} KB/s)",
                  end="", flush=True)
    conn.send(tail)
    print()
    response = conn.getresponse()
    body = response.read().decode(errors="replace")
    conn.close()
    return response.status, body


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="musegrinder.local")
    ap.add_argument("--env", default=DEFAULT_ENV, help="PlatformIO env whose firmware.bin to send")
    ap.add_argument("--firmware", help="explicit firmware.bin path (overrides --env)")
    args = ap.parse_args()

    firmware = args.firmware or os.path.join(REPO, ".pio", "build", args.env, "firmware.bin")
    if not os.path.exists(firmware):
        sys.exit(f"No firmware at {firmware}. Build first: pio run -e {args.env}")

    pw_path = os.path.join(REPO, ".ota_password")
    if not os.path.exists(pw_path):
        sys.exit("No .ota_password. Build the firmware once (it generates the password).")
    password = open(pw_path).read().strip()

    ip = resolve(args.host)
    before = get_status(ip)
    print(f"[INFO] {args.host} ({ip}): build #{before.get('build', '?')} "
          f"board {before.get('board', '?')}, grinding={before.get('grinding')}")
    if before.get("grinding"):
        sys.exit("[ERROR] Grinder is grinding; try again when it is idle.")
    if "build" not in before:
        print("[WARN] Firmware on the grinder predates WiFi OTA; install this build over Bluetooth first.")

    print(f"[INFO] Sending {os.path.relpath(firmware, REPO)} ({os.path.getsize(firmware) // 1024} KB)")
    status, body = upload(ip, firmware, password)
    if status != 200:
        sys.exit(f"[ERROR] Grinder refused the update ({status}): {body}")
    print("[OK] Upload accepted; grinder is restarting...")

    for _ in range(40):
        time.sleep(2)
        try:
            after = get_status(ip, timeout=3)
        except (OSError, http.client.HTTPException, ValueError):
            continue
        print(f"[OK] Grinder is back on build #{after.get('build')} "
              f"(reset: {after.get('reset_reason')})")
        return
    sys.exit("[ERROR] Grinder did not come back within 80s; check the screen.")


if __name__ == "__main__":
    main()
