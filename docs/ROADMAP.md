# One native client, two games

## Current milestone

The delivered components are a standalone C++17 archive inspector and a Godot
archive browser using the same parser through GDExtension. They list GOB/LMP
metadata, including nested LMP entries, with explicit little-endian fields and
bounded tables, names and payload ranges. The browser has explicit title selection
and a local file picker. Selected RTA lump entries can preview the documented
32-bit, plain 4-bit and 8-bit texture subsets. A bounded one-pass RTA static model
subset supports rest-pose inspection with neutral shading or opaque texture colors and single-bit variant visibility. The portable core also retains per-corner joint indices and exact byte weights; a bounded [skeleton metadata reader](ANIMATION_METADATA.md) now preserves RTA joint hierarchies and signed bind offsets. Skeleton binding and animation playback remain work. Other palette layouts,
materials, animation, scripts and gameplay remain unimplemented. Title labels
are not validated game profiles; texture and model decoding
explicitly rejects Norrath until its contract is established.

See the [browser guide](GODOT_BROWSER.md) for pinned dependencies, builds and
platform verification. Godot and godot-cpp are fetched locally, not vendored.

The initial private archive comparisons cover **Return to Arms only**. Norrath now has an
[experimental hash-pinned executable](https://github.com/Open-Champions/champions-of-norrath/blob/main/docs/BASELINE.md);
its retail revision and native format/behavior compatibility remain unverified here. Public tests
use authored synthetic bytes and do not certify either game's entire data set.

## Architecture and remaining work

| Layer | Responsibility | Evidence boundary |
|---|---|---|
| Portable C++ core (`native/`) | Validated archives, future format decoders, simulation contracts and reproducible traces | Archive metadata and a bounded RTA 32-bit/plain 4-bit/8-bit texture decoder are implemented; bounded static rest-pose model geometry is also available; broader decoders and simulation remain work. |
| Game profiles (planned) | Explicit title, region/revision, baseline fingerprint, supported formats, constants, script/ability/object contracts and save schema | RTA research is a starting point. Norrath needs its own evidence before sharing a rule. |
| Thin GDExtension (`gdextension/`) | Expose validated archive metadata and limited RTA texture pixels and model geometry to Godot | Copies results into owned Godot values; archive fingerprints reject stale preview selections. Simulation remains work. |
| Godot 4 presentation (`godot/`) | Archive selection, nested metadata, 32-bit/plain 4-bit/8-bit texture preview, static model inspection and status/error UI | Gameplay scenes, original rendering/materials, gameplay input, audio and simulation remain work. |

Select the title/profile explicitly until detection is verified. Use a profile's
supported revisions and content fingerprints to reject unknown inputs with a
useful error. Share a decoder or behavior only after both games' contracts agree;
keep revisions and differences explicit. A successful GOB listing is not a title
or revision detector. No Norrath executable hash or supported revision is invented.

The source siblings own original PS2 reconstruction:
[Return to Arms](https://github.com/Open-Champions/champions-return-to-arms) and
[Norrath](https://github.com/Open-Champions/champions-of-norrath).
Use their documented behavior to author portable implementations. Binary-matching
progress is measured there and must not be counted as native client completion.
Existing [format research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/main/docs/FORMATS.md)
and the [preserved native assessment](NATIVE_PORT.md)
remain research references; do not duplicate active area-agent investigations.

## Why Godot

[Open-BFME's client](https://github.com/Open-BFME/openbfme-godot) is a useful
precedent for separating a portable C++ core from the presentation engine and
from original-executable matching projects. It does not supply Snowblind assets,
formats, simulation or an automatic PS2 conversion.

Godot's [GDExtension interface](https://docs.godotengine.org/en/stable/engine_details/engine_api/gdextension/what_is_gdextension.html)
and [official C++ bindings](https://github.com/godotengine/godot-cpp) provide the
integration route. The initial browser pins Godot 4.5.2, its matching export templates,
and godot-cpp at its Godot 4.5 API tag. The archive library continues to build and
test without any Godot dependency.

Native client targets are Apple Silicon/Intel macOS and ARM64/x64 Windows.
Godot documents [macOS exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html)
and [Windows exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html);
matching extension binaries, renderer behavior and actual exported-app execution
still need per-platform verification. Inspector CI is not Godot export evidence.
Linux remains useful for portable-core tests and tooling; client support needs
its own launch and rendering evidence.

## Useful next milestones

1. **Establish both profiles.** Record each privately supplied baseline's title,
   region/revision and non-content fingerprints. Compare archive contracts and
   add authored fixtures for established differences. Reject unsupported variants.
2. **Display one real texture.** The supported RTA 32-bit, plain 4-bit and 8-bit subsets are described in
   [texture preview evidence](TEXTURE_PREVIEW.md); the broader milestone is incomplete.
   Recover remaining documented layouts, implement
   a bounded decoder with synthetic fixtures, then expose it through GDExtension.
   Compare dimensions, orientation, palette and alpha against a private reference
   on native Mac and Windows exports. The user supplies the original file.
3. **Inspect one real mesh.** The [bounded model viewer](MODEL_PREVIEW.md) covers rest-pose geometry and texture-color inspection; original material behavior remains work. Decode a static model/material, check scale, winding
   and UVs, and provide orbit/zoom. Repeat separately for each supported profile;
   document unsupported asset variants and Norrath/RTA differences.
4. **Build one controllable scene.** Add movement, collision, camera, one trigger
   and native save/reload. Label provisional movement/navigation and include only
   authored scenes in public packages. Animation, combat, inventory and script
   built-ins require further recovered contracts and focused checks.
5. **Compare native peers.** Record inputs and deterministic state traces on
   arm64 and x64, reproduce a divergence and explain the first differing field
   before expanding multiplayer. Original PS2-client and lobby interoperability
   are separate compatibility milestones.

Each milestone needs an executable result and recorded evidence. A blank window,
placeholder character or accepted archive table is not a completed game feature.

## Deterministic behavior and game differences

Specify integer widths/overflow, conversion rules, floating-point expectations,
random streams, stable iteration order and fixed simulation tick sequencing.
Keep input collection, wall-clock timers, audio, camera, loading speed and render
rate outside authoritative state. Game-specific scripts, object layouts, abilities,
content IDs, navigation, save formats and network behavior are not assumed shared.

Godot physics/navigation and scene multiplayer are useful facilities but are not
proven equivalents of Snowblind collision/pathfinding or the original network
protocol. An initial native save schema is also distinct from PS2 save import or
round-trip compatibility. Cross-platform trace comparison and behavior tests are
required even when both peers use a fixed update rate.

## Content and distribution

Runtime access will use a user-selected game-data folder; original assets stay
outside the source tree and exported packages. Public CI uses synthetic fixtures.
The inspector packages contain only the inspector, license, usage and build metadata.
Browser exports contain authored UI and the native extension, without original game data.
A future playable export must state the exact supported title/revision, operating
systems, remaining provisional behavior and independent verification results.
