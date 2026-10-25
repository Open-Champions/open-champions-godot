# Explicit native game profiles

`native/include/champions/game_profile.hpp` and `native/src/game_profile.cpp`
provide a standard-library-only C++17 profile registry. It is independent of
Godot, parsing, simulation and file access. Select an exact profile ID; successful
GOB/LMP listing never selects or validates a title or revision.

| ID | Title | Region/revision | Baseline | Format eligibility |
|---|---|---|---|---|
| `rta-us-v2.00` | Champions: Return to Arms | US v2.00 | Documented executable SHA-256 | GOB/LMP metadata, bounded lump textures and model forms below |
| `norrath-unvalidated` | Champions of Norrath | Unknown | None in this native registry | None; decoding refused |

RTA's executable baseline is SHA-256
`292f6f48e3609c0026def2307d76c222cd5b389303d53b89b0e107d7b1dcd675`.
This identifies the documented US v2.00 executable, **not** a disc image,
archive, asset, reconstructed build or complete playable client. The profile's
`DocumentedBaseline` status records published identity evidence, not a new
retail-runtime validation. No private inputs were read for this implementation.

## Evidence and bounded formats

Identity comes from the sibling research/reconstruction checkout's
[`docs/REBUILD.md`, Baseline and toolchain](https://github.com/Open-Champions/champions-return-to-arms/blob/adb1e7c3a9b1c41e86aed3f9f73cfef95c088c36/docs/REBUILD.md)
and [`README.md`](https://github.com/Open-Champions/champions-return-to-arms/blob/adb1e7c3a9b1c41e86aed3f9f73cfef95c088c36/README.md), inspected locally at commit
`adb1e7c3a9b1c41e86aed3f9f73cfef95c088c36`. They identify the US v2.00
executable and publish its SHA-256. This implementation copies only documented
identity facts; no reconstruction output, executable bytes or assets are used.

Format eligibility describes existing portable readers, not unrestricted support:

- `GobArchive` / `LmpArchive`: bounded archive metadata, as documented in the
  [browser guide](GODOT_BROWSER.md) and [native assessment](NATIVE_PORT.md).
  These parsers remain title-independent; listing does not validate content.
- `RtaTexture32`, `RtaTexture4`, `RtaTexture8`: the
  [documented lump-texture subsets](TEXTURE_PREVIEW.md), respectively bounded
  PSMCT32, plain-row PSMT4, and PSMT8 storage via bounded GS memory.
  Relocated headers, world cells and 4-bit uploads through 32-bit storage remain
  unsupported. The decoder still enforces all packet and allocation bounds.
- `RtaModel42` / `RtaModel44`: the existing [model forms](MODEL_FORMS.md).
  Reflection tags, shell normals and facing/attachment tables remain metadata;
  world `0x46`, original materials and renderer behavior are unsupported.

No animation, WORLD, behavior constants, script/ability/object contracts or save
schema are advertised by this first profile layer. Existing modules retain their
own evidence and adoption remains separate work. The Norrath reconstruction's
experimental executable does not establish native format compatibility; no
region, revision, executable hash or shared decoder is invented here.

## API and refusals

`gameProfiles()` returns const, process-lifetime registry entries;
`findGameProfile(id)` returns a const pointer or null. IDs are exact and
case-sensitive, with no ambiguous `rta` alias or fallback.

`checkContentFingerprint(profile, supplied)` returns a `ProfileCheck` with a
machine-readable code, useful diagnostic and `accepted()`. It refuses
unvalidated profiles, missing baselines, wrong fingerprint scopes/algorithms,
malformed digests and unknown fingerprints. SHA-256 text requires exactly 64
ASCII hex characters; uppercase and lowercase are equivalent, with no trimming,
prefixes or partial matching. Norrath refuses even if supplied the RTA digest.

`checkProfileDecoding(id, supplied, format)` resolves the registry entry, checks
the baseline, then rejects formats outside its explicit list. Unknown IDs and
unknown format enum values fail closed. An executable baseline match grants only
eligibility for that bounded reader; each supplied payload still needs normal
format validation. It cannot prove an archive belongs to that executable's disc.

For example, a caller that has explicitly selected and hashed the baseline
executable can ask:

```cpp
const champions::ContentFingerprint fingerprint{
    champions::FingerprintScope::Executable,
    champions::FingerprintAlgorithm::Sha256,
    selected_executable_sha256};
const auto result = champions::checkProfileDecoding(
    "rta-us-v2.00", fingerprint, champions::SupportedFormat::RtaModel42);
if (!result.accepted()) {
    // Display result.message and do not call a decoder.
}
```

Digest strings in `ContentFingerprint` are borrowed `string_view`s; keep caller
storage alive during the check. Registry storage is static. A directly supplied
`GameProfile` is caller-owned policy, not authenticated registry membership;
use `checkProfileDecoding` for the registered policy. The layer does not hash
files, infer profiles, inspect bytes, bind an executable to a content directory,
or bypass decoder errors.

## Existing callers and intended adoption

The texture/model gates in `gdextension/src/archive_inspector.cpp` still use the
explicit `rta` title label and their existing refusal behavior. The validator's
`--game rta` selection also remains unchanged. Neither receives the selected
executable's SHA-256, so changing them to this stricter identity gate would change
behavior and require UI/input plumbing and independent integration verification.
Their current title labels do not certify the baseline or enforce this registry.

Future adoption should select an exact revision profile, obtain the fingerprint
from an explicitly selected baseline executable, call `checkProfileDecoding`
before texture/model dispatch, and retain current archive/selection freshness
checks and all decoder bounds. The existing preview-selection fingerprint has a
different purpose: it detects stale bytes and cannot substitute for this executable
identity. No animation, creature, Session or texture-decoder internals change.

## Synthetic verification

`game-profiles` CTest exercises registry facts, exact ID lookup, both hex cases,
scope/algorithm/length/character errors, unknown executable fingerprints,
missing/unvalidated profiles, unsupported formats and unconditional Norrath
refusal. An authored four-byte empty LMP lists successfully while unknown and
Norrath inputs still fail the profile gate. No retail fixtures are required.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^game-profiles$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

These tests establish policy and diagnostics only. Other RTA revisions, Norrath
decoding, caller adoption, complete content provenance, cross-platform execution
and playable/exported-client behavior require separate evidence.
