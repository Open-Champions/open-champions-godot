# Static WORLD ground in the controllable scene

The scene can load original static ground geometry from an explicitly selected
RTA WORLD file. Collision and presentation decode the same owned bytes into
separate data. The original dry creature movement policy still consumes ready
input frames against native terrain; display coordinates never feed simulation.
The practice ground remains an authored collision-plane illustration.

The presentation-only `world_geometry` target follows immutable published
`world_info.World` methods for packets, ordered strip copies, chunk transforms
and triangles. It preserves shared packet identity, all 33 raw header words,
raw positions/normals/strip tables/extra copies/GIF tags/UVs, chunk metadata,
transformed binary64 bits, triangles and omission counts. Translation precedes
rotation; quarter-turn codes and mirrored/free rotations remain distinct.
Both documented VIF programs 0x46 and 0x48 are supported.

Neutral two-sided meshes provide ground geometry. Textures, original materials,
lighting, scenery models and emitters are omitted explicitly. Water and moving
pieces refuse the whole input until their services and lifetime are established.
The reader's self-copy behavior is the pinned Python contract; this does not
claim full VU-program or original-renderer equivalence.

## Ownership and replacement

The bridge prepares both decoders, a new Controller, returned observations and
actual ArrayMesh resources before exposing a candidate token. The scene prepares
its detached subtree and material before committing. Invalid, cancelled or reset
candidate tokens refuse; failed replacements retain the old controller, mesh,
pause state and observations. Reset retains selected ground and starts fresh
owned native state. Ready queues, focus release and the 512-tick practice bound
are unchanged. No original player input or spawn behavior is claimed; files must
support native floor queries at the authored starting point.

The pinned godot-cpp Dictionary move assignment overwrites opaque storage without
releasing its old owner. Publication therefore uses lvalue copy assignment instead
of `std::swap` on dictionaries. WeakRef checks verify actual release of replaced,
active-controller and cancelled-candidate meshes. Source/export smoke teardown
also releases owners and drains queued scene nodes while the renderer is alive.

The native decoder has lowerable hard limits for input bytes, chunks, packets,
batches, source vertices, strip slots, transformed vertices, triangles, VIF
commands, unpack rows and expanded render triangles. Sharing never bypasses
expansion limits. The scene separately limits visible output to 4,096 surfaces
and one million triangles. These are explicit safety boundaries, not original
engine limits.

## Reference and local evidence

- `tools/world_info.py`: commit `2f937fca823488bc97b9680fde9d957c3fa9907a`,
  SHA-256 `2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5`.
- `docs/spec/render-pipeline.md`: commit `abf07ea411ac78e4cacb1da3e85b388a1839928a`,
  SHA-256 `3eb890d80325c7b04a584de04af8360fc8f4c0e412d3a21656b4e99db570a04b`.
- `docs/formats/world.md`: commit `f2b996dded898f1b0b558b901cab875f775cfdc7`,
  SHA-256 `81a7e067e859eaaae171d0422b5388510691d289ae37d6e42a25e5549d24cd20`.

The 73 authored cases compare complete native output with original Python:
raw inventory, packet aliases, every orientation, free/mirrored transforms,
ordered copies, restarts, degenerate triangles, omissions and malformed inputs.
Trace SHA-256:
`1d019b2a36d19c1f742440723e6dcce7f0328d885db33ca202ad2ed870c99639`.
Canonical plus three fresh generation processes produced identical gzip and
manifest bytes; seven atomic-publication failure checks passed. The native API
has 63 ownership, input-preservation and allocation-bound checks. All three new
CTest gates passed under ASAN/UBSAN with runtime and both entrypoints instrumented;
macOS leak detection was disabled. Engine mesh-destruction checks are separate
from that sanitizer limitation. The strict 32-suite artifact registry refuses
missing hosts, changed provenance and identical wrong results across hosts.

The source scene passed 68 checks, including real mesh destruction, with no mesh
RID or ObjectDB leak warnings; its archive browser passed 283 checks. All 84 native CTest cases passed (791.83 seconds); 424 Python tests passed with
230 explicit native-path/platform skips. GCC 15 checked four changed native units,
and LLVM 20 formatting covered all tracked native sources plus the new decoder,
tests and bridge. Universal Mac debug and release bindings compiled. The actual
Mac export also passed 68 scene and 283 browser checks without engine resource
leak warnings. Two independent final integration reviews found no blocker.
Manual exported-app visual inspection is currently blocked by the locked desktop;
exact-head five-platform CI and actual artifact comparison remain gates before merge.

## Owner-disc evidence and limits

The selected RTA US v2 corpus contains 119 WORLD occurrences, 116 byte-unique
inputs. Native collision projection retains its earlier 116/116 acceptance,
1,856 complete states across 16 arithmetic configurations and 118,784 height
queries. That evidence is separate from geometry and scene loading.

Static geometry decoded 48/116 unique inputs; 68 refused (49 primary water
refusals and 19 moving-piece refusals; overlapping flags count once). All 48
complete native geometry observations matched independently executed original
Python, including 1,105,260,991 serialized observation bytes. All 48 ASAN/UBSAN
observations matched those independently verified states, with both private
entrypoint and runtime instrumented. Original inputs were unchanged.

The actual source Godot binding accepted 17/116 inputs, prepared and committed
nonempty meshes, ran one ready movement tick and reset. Its 99 refusals comprise
49 water, 19 moving-piece, 19 starting-floor and 12 scene-resource-limit cases.
A complete controllable source scene separately loaded one real input with
266 meshes / 11,448 triangles and passed movement, pause and reset checks.
These are headless execution observations; manual export/render inspection is a
separate gate. Original bytes and detailed private records remain outside source
and packages. The optional private-directory CLI reports `terrain` and
`world-geometry` acceptance independently, preserving occurrence denominators.

Neither neutral geometry, these model comparisons nor exports establish a
playable retail game, original materials, PS2 networking or Norrath behavior.
