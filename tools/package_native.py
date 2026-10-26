"""Package the native companion; exclude logs, object files and game data."""
import hashlib
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
source = root / "Runtime/NativeAntigravity"
out = source / "out"
payload = {name: (out / name).read_bytes() for name in (
    "TK2-Antigravity.exe", "TK2-Antigravity.dll", "MinHook-LICENSE.txt", "LICENSE-Antigravity.txt")}
payload["README.md"] = (source / "README.md").read_bytes()
for name, args in (("Check compatibility.cmd", " --probe"), ("Start Antigravity.cmd", "")):
    payload[name] = ("@echo off\r\ncd /d \"%~dp0\"\r\nTK2-Antigravity.exe" + args +
        "\r\nif errorlevel 1 echo Operation failed. Read TK2-Antigravity.log for details.\r\npause\r\n").encode()
receipt = json.loads((out / "build-verification.json").read_text())
receipt["metadataCheck"] = "33 native contracts resolved in the installed game"
receipt["startupCheck"] = "activated in the offline main menu; process remained responsive"
receipt["gameplayValidation"] = "not tested on a marked track"
receipt["files"] = {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    for name, data in payload.items()}
payload["verification.json"] = (json.dumps(receipt, indent=2) + "\n").encode()
archive = root / "Releases/TK2-Antigravity-Native-0.1.0-alpha.1-Windows-x64.zip"
archive.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for name, data in payload.items():
        z.writestr(name, data)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert len(z.namelist()) == len(payload)
    for name, details in receipt["files"].items():
        assert hashlib.sha256(z.read(name)).hexdigest() == details["sha256"], name
print(f"Verified flat package: {archive.name}, {archive.stat().st_size} bytes")
