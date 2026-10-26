"""Build the self-contained x64 native companion with installed Visual C++ tools."""
from pathlib import Path
import hashlib
import json
import subprocess
import shutil

source = Path(__file__).resolve().parent
root = source.parents[1]
out = source / "out"
out.mkdir(exist_ok=True)
locator = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
installs = json.loads(subprocess.check_output([str(locator), "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-format", "json"]))
if not installs:
    raise SystemExit("Visual C++ x64 Build Tools are required.")
installs.sort(key=lambda item: tuple(map(int, item["installationVersion"].split("."))), reverse=True)
compiler = Path(installs[0]["installationPath"]) / "VC/Auxiliary/Build/vcvars64.bat"
vendor = source / "vendor/minhook/src"
flags = "/nologo /O2 /MT /W4 /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX"
commands = ["@echo off", f'call "{compiler}" >nul', "if errorlevel 1 exit /b %errorlevel%",
    f'cl {flags} /c "{vendor / "buffer.c"}" "{vendor / "hook.c"}" "{vendor / "trampoline.c"}" "{vendor / "hde/hde64.c"}"',
    "if errorlevel 1 exit /b %errorlevel%",
    f'cl {flags} /EHsc /std:c++17 /LD "{source / "Native.cpp"}" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:TK2-Antigravity.dll /IMPLIB:TK2-Antigravity.lib /DYNAMICBASE /NXCOMPAT',
    "if errorlevel 1 exit /b %errorlevel%",
    f'cl {flags} /EHsc /std:c++17 "{source / "Launcher.cpp"}" /Fe:TK2-Antigravity.exe /link /DYNAMICBASE /NXCOMPAT',
    "exit /b %errorlevel%"]
batch = out / "build.cmd"
batch.write_text("\n".join(commands) + "\n", encoding="utf-8")
subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=out, check=True)
receipt = {"version": "0.1.0-alpha.1", "architecture": "x64", "files": {},
    "gameplayValidation": "pending offline marked-track test"}
for name in ("TK2-Antigravity.exe", "TK2-Antigravity.dll"):
    path = out / name
    receipt["files"][name] = {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
(out / "build-verification.json").write_text(json.dumps(receipt, indent=2) + "\n")
shutil.copy2(source / "vendor/minhook/LICENSE.txt", out / "MinHook-LICENSE.txt")
shutil.copy2(root / "LICENSE-Antigravity.txt", out / "LICENSE-Antigravity.txt")
print(json.dumps(receipt, indent=2))
