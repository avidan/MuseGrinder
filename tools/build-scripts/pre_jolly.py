#!/usr/bin/env python3
"""PlatformIO pre-build step: generate Jolly frames before compiling.

Runs tools/gen_jolly.py, which converts Meta's jollybot.gif
(from the local muse-gadget-sdk clone) into src/muse/jolly_anim.c/h.
Fails loudly if the GIF can't be found — see tools/gen_jolly.py.
"""

try:
    Import("env")
    platformio_mode = True
except Exception:
    platformio_mode = False

import os
import subprocess
import sys


def main():
    if platformio_mode:
        project_dir = env.get("PROJECT_DIR", os.getcwd())
    else:
        project_dir = os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        )
    script = os.path.join(project_dir, "tools", "gen_jolly.py")
    out_dir = os.path.join(project_dir, "src", "muse")
    r = subprocess.run(
        [sys.executable, script, "--out-dir", out_dir],
        capture_output=True,
        text=True,
    )
    print(r.stdout)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        sys.exit(r.returncode)


main()
