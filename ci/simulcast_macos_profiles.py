#!/usr/bin/env python3
"""Install and validate the two Simulcast-specific Developer ID profiles."""
import base64
import os
import plistlib
import subprocess
from pathlib import Path

BUNDLE = "io.github.notadeveloper4665.SimulcastStudio"
team = os.environ["SIGNING_TEAM"]
host = os.environ.get("HOST_PROFILE", "")
camera = os.environ.get("CAMERA_PROFILE", "")
output = Path(os.environ["GITHUB_OUTPUT"])

if not (host and camera):
    with output.open("a") as out:
        out.write("enabled=false\n")
    print("Camera profiles are incomplete; building Browser Source without virtual camera.")
    raise SystemExit(0)

def install(encoded, bundle, label):
    path = Path(os.environ["RUNNER_TEMP"]) / (label + ".provisionprofile")
    path.write_bytes(base64.b64decode(encoded, validate=True))
    path.chmod(0o600)
    profile = plistlib.loads(subprocess.check_output(["security", "cms", "-D", "-i", str(path)]))
    ent = profile["Entitlements"]
    if team not in profile["TeamIdentifier"]:
        raise RuntimeError(f"{label} profile belongs to a different Apple team")
    identity = ent.get("com.apple.application-identifier", ent.get("application-identifier"))
    if identity != team + "." + bundle:
        raise RuntimeError(f"{label} profile must match {bundle} exactly")
    if team + "." + BUNDLE not in ent.get("com.apple.security.application-groups", []):
        raise RuntimeError(f"{label} profile does not authorize the shared Simulcast app group")
    if label == "host" and not ent.get("com.apple.developer.system-extension.install"):
        raise RuntimeError("Host profile is missing system extension installation entitlement")
    uid = profile["UUID"]
    # Both locations are used by different Xcode versions.
    for folder in ("Library/MobileDevice/Provisioning Profiles", "Library/Developer/Xcode/UserData/Provisioning Profiles"):
        dest = Path.home() / folder
        dest.mkdir(parents=True, exist_ok=True)
        (dest / (uid + ".provisionprofile")).write_bytes(path.read_bytes())
    return uid

host_id = install(host, BUNDLE, "host")
camera_id = install(camera, BUNDLE + ".mac-camera-extension", "camera")
with output.open("a") as out:
    out.write(f"enabled=true\nhost={host_id}\ncamera={camera_id}\n")
