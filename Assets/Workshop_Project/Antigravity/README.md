# Antigravity surfaces — SDK 0.1.1

Select a road object with a Collider, add `PTK_GroundType`, and enable
**Antigravity surface** in its inspector. Existing friction, audio and effects
remain independent. Mark the physical loop/ceiling surfaces only, not the whole
track. Leave ordinary jumps and half-pipes unmarked.

The authoring control adds an empty direct child named `__TK2_Antigravity_v1`.
Keep it under that ground object. It has no collider or custom gameplay script.
Turning the control off removes it. Normal prefab/AssetBundle export preserves
the marker. The example prefab is a simple marked road tile, not a complete track.

**The unmodified game does not implement antigravity.** A compatible runtime or
future native game update is required. Without one, the tile behaves as ordinary
ground. Do not advertise a Workshop track as stock-game compatible antigravity.

Use a continuous, collidable road and an AI path following the physical loop.
Teleport splines that skip the loop do not supply a usable physical AI route.

Import the release `.unitypackage` into the official v1.08.1 Unity project, or
clone this repository recursively. Unity version: **2020.3.47f1**. The addon does
not modify existing ground enum numbers, bit packing, scripts or friction values.

Credit: original SDK by Pixel Edge Games; extension by DarkSlayer.
