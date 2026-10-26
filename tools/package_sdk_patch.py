"""Package only the additive Antigravity editor files for official SDK v1.08.1."""
import hashlib
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
editor = Path("Assets/Workshop_Project/Editor/TK2_AntigravityGroundEditor.cs")
folder = Path("Assets/Workshop_Project/Antigravity")
files = [editor, Path(str(editor) + ".meta"), Path(str(folder) + ".meta")]
files += sorted(path.relative_to(root) for path in (root / folder).rglob("*") if path.is_file())
files.append(Path("LICENSE-Antigravity.txt"))
instructions = """ANTIGRAVITY SDK PATCH 0.1.1 - for Unity.ModdingTools v1.08.1

1. Close Unity and back up your project.
2. Extract this ZIP into the existing Unity project root (the folder containing
   Assets, Packages and ProjectSettings). Merge the Assets folder.
3. Open the project with Unity 2020.3.47f1 and let it compile.
4. Select a road Collider with PTK_GroundType. Enable Antigravity surface.

This patch adds only the editor extension, its metadata, an example ground tile
and documentation. It does not replace the official ground script, packages,
project settings or your tracks. Keep all .meta files with their assets.

Actual driving requires a compatible game runtime or future native game update.
The unmodified game treats these surfaces as ordinary ground. Use marked tiles
only where Antigravity is intended; leave normal jumps and half-pipes unmarked.

Alternative: import the small .unitypackage from the same release through Unity.
Uninstall: remove TK2_AntigravityGroundEditor.cs and its .meta, plus the
Workshop_Project/Antigravity example folder and its .meta. In authored scenes,
remove __TK2_Antigravity_v1 children from surfaces you no longer want marked.

Original SDK: Pixel Edge Games. Extension: DarkSlayer.
"""
payload = {path.as_posix(): (root / path).read_bytes() for path in files}
payload["README-ANTIGRAVITY-PATCH.txt"] = instructions.encode("utf-8")
manifest = {
    "version": "0.1.1", "baseSdk": "1.08.1", "unity": "2020.3.47f1",
    "requiresGameRuntime": True,
    "files": {name: hashlib.sha256(data).hexdigest() for name, data in payload.items()},
}
payload["ANTIGRAVITY-PATCH-MANIFEST.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
output = root / "Releases/TK2-Antigravity-SDK-0.1.1-Patch-for-1.08.1.zip"
output.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, data in payload.items():
        archive.writestr(name, data)
with zipfile.ZipFile(output) as archive:
    assert archive.testzip() is None
    assert len(archive.namelist()) == len(payload)
    for name, digest in manifest["files"].items():
        assert hashlib.sha256(archive.read(name)).hexdigest() == digest, name
print(f"Verified {len(payload)} files; {output.name}: {output.stat().st_size} bytes")
