# Native Antigravity companion — 0.1.0-alpha.1

Windows x64, marked surfaces only. No BepInEx, managed loader, .NET install or
replacement game files are required. Uses IL2CPP metadata and MinHook.

**Experimental: native physics have not yet been validated on an exported loop.**
Resolving all 33 hook/guard contracts is a compatibility check, not a driving test.

## Run

1. Extract the companion into its own writable folder. Keep the EXE and DLL
   together. Do not put them in the game's plugins or overwrite Steam DLLs.
2. Turn off the previous global Antigravity module if you use it. The launcher
   refuses activation if a configuration file still enables that implementation.
3. Start the game normally and stay in the offline main menu.
4. Run `Check compatibility.cmd` first. It resolves metadata without enabling
   hooks and unloads the probe when finished.
5. Run `Start Antigravity.cmd` to enable the companion for this game session.
6. Play an offline track exported with marked surfaces from SDK 0.1.1.

Unmarked colliders keep native physics. Marked road is identified by a direct
child Transform named `__TK2_Antigravity_v1`, created by the SDK inspector.
No per-map names, hard-coded binary addresses or game-version allowlist is used.

Log: `TK2-Antigravity.log` beside the launcher. A `READY` entry confirms hook
activation. `Marked surface attached` confirms a marked collider was detected.
The compatibility command alone does not produce a `READY` entry.

Close the game to unload an activated companion. To uninstall, delete its folder.
An active runtime cannot be safely replaced mid-session. Online-room operations
and leaderboard uploads are blocked for the entire activated session, including
after leaving a marked section. Restart normally to play online.

## Behavior and limits

- The motor's up/forward frame follows marked collision normals. Untagged ground
  releases it. Bounded contact grace retains the underlying marked road in hops.
- Steering forces are calculated in a flat frame and transformed back. Human
  steering does not invert at the top. Camera exit retains the tested eased pose.
- Human hops remain native impulses rotated into the road frame. Grounded visual
  flags are scoped to animation calls and restored immediately.
- Attached AI get conservative surface-relative route guidance, no automatic
  hops, earned native drift boosts and capped sideways contact velocity. This
  assistance changes AI handling. Teleport-only paths still need authoring work.
- Racer contact/bounce is suppressed while attached and around immediate neighbors.
- No settings are saved into the game/profile. Collider flags and global physics
  settings are not changed; temporary motor/physics fields are restored.

This prototype ports the earlier offline experiment into a separate native
companion. It is not a native game-source implementation or a guarantee that
every existing Workshop map becomes drivable. Build success and metadata checks
do not establish wheel contact, jumps, camera clearance, AI or frame pacing.

## Build and source

`python Runtime/NativeAntigravity/build.py` from the SDK repository. Requires the
installed Visual C++ x64 Build Tools and Python 3. Standard Windows DLLs and the
game's own IL2CPP exports are the only runtime dependencies.

- `Native.cpp`: scoped game callbacks and offline guards.
- `Game.h`, `Metadata.h`: complete method signatures and reflected field contracts.
- `Math.h`: road-frame vector/quaternion operations.
- `Launcher.cpp`: exact-process loading, compatibility mode and diagnostics.

Native evidence was reviewed from the installed GameAssembly's current metadata
and focused native slices. REA's Windows adapter could not decompile the raw x64
slice, so the existing direct Ghidra results were retained as the implementation
reference. No game binary, recovered game source or original track asset is shipped.

Extension: DarkSlayer. Original game/SDK: Pixel Edge Games.
MinHook: Tsuda Kageyu and contributors, BSD 2-Clause; included license applies.
Extension code: MIT, see `LICENSE-Antigravity.txt`.
