#!/usr/bin/env python3
"""PlatformIO pre-build step: compile in the WiFi OTA password.

Creates <project>/.ota_password (gitignored) with a random password on first
build, and passes it to the firmware as MUSE_OTA_PASSWORD. tools/wifi_upload.py
reads the same file, so there is nothing to configure.
"""

Import("env")  # noqa: F821  (provided by PlatformIO)

# Listed without "pre:" in platformio.ini, so this runs as a post script: the
# project sources build from the cloned projenv, which needs the define too.
try:
    Import("projenv")  # noqa: F821
    build_envs = [env, projenv]  # noqa: F821
except Exception:  # pre-script context: env is the one the sources use
    build_envs = [env]  # noqa: F821

import os
import secrets

project_dir = env.get("PROJECT_DIR", os.getcwd())  # noqa: F821
path = os.path.join(project_dir, ".ota_password")

if not os.path.exists(path):
    with open(path, "w") as f:
        f.write(secrets.token_urlsafe(18) + "\n")
    os.chmod(path, 0o600)
    print("pre_ota_secret: generated .ota_password")

with open(path) as f:
    password = f.read().strip()

for build_env in build_envs:
    build_env.Append(CPPDEFINES=[("MUSE_OTA_PASSWORD", build_env.StringifyMacro(password))])
