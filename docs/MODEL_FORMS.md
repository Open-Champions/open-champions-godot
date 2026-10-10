# RTA model forms and private validation

The portable decoder now accepts documented ordinary `0x42` and two-pass `0x44`
model packets, shell-normal blocks and facing-point tables. It retains both GIF
tags, exact raw corner attributes, joint weights and optional metadata without
depending on Godot. The implementation follows the
[model format](https://github.com/Open-Champions/champions-return-to-arms-re/blob/c3d72647df7476f8cb6fa0105cbeccfcf4dc1a8f/docs/formats/model-texture.md),
[reference reader](https://github.com/Open-Champions/champions-return-to-arms-re/blob/c3d72647df7476f8cb6fa0105cbeccfcf4dc1a8f/tools/model_info.py)
and [render-pipeline contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/abf07ea/docs/spec/render-pipeline.md).
The reader is pinned at `c3d72647df7476f8cb6fa0105cbeccfcf4dc1a8f`, SHA-256
`9cb38fadec899fde8ed457d7daf050bb7f6ebab80795494e27d697470c088870`.
No original model, disc data or decompiler output is distributed.

## Preserved structure

Each packet chooses its stride from its actual GIF block and validates the
microprogram against it. Header flag `0x10` permits two-pass packets; a model
with that flag can also contain ordinary packets. The private full-disc metadata
walk found 10,135 ordinary packets within models carrying that flag. Assuming
that every packet was two-pass incorrectly refused 94 models. This packet-level
nuance is reported here; the research files remain unchanged.

Strip replay preserves source snapshots, the different store order for stride
four, overlapping copies and destination no-draw bits. Geometry is emitted once;
both passes' tags and packet corner ranges remain separate metadata. Shell normals
are preserved as signed raw values; renderer displacement uses `/32768`, unlike
the viewer's ordinary-normal `/127` representation.

Facing records retain all sixteen bytes, signed positions, signed normals and
joint, plus the complete terminator. Positions use `/16`; facing normals use
`/128`. Tables require a bounded complete sentinel. Attach matrices and effect
segments retain raw float32 words. Hull points, mesh bounds, triangle hints,
sample-point records and their original bounded VIF block remain owned metadata.
Nonfinite metadata does not become an evaluated transform.

Budgets remain 16MiB input, 64 meshes, 32,768 packets and 200,000 triangles.
Packet input remains bounded to 128 VU qwords, including actual tags and shell
normals. Strip limits remain 83 vertices for stride three and 62 for stride four.
Table extents cannot overlap; malformed, inconsistent and truncated input refuses.

## Optional exact private comparison

Build `champions-model-forms-validate`, then select an extracted VIF directory
outside every source repository:

```sh
python3 tools/validate_model_forms.py --native build/native/champions-model-forms-validate \
  --research-dir /path/to/champions-re --input-dir /private/extracted/models \
  --report work/private-model-report.json
```

The Python tool loads the hash-pinned reference from a git object. It compares
all exposed header/table fields, raw attributes, float32 viewer representations,
corner weights/order, packet tags/ranges and optional metadata. Counts distinguish
decoded, refused, matched, mismatched and unverified output. An unchanged input
and transport binary are required. Duplicate bytes are counted separately from
unique inputs. Case-insensitive VIF discovery skips links; bounded output and
timeout failures cannot count as successful comparisons. A run with no matched
model fails.

Reports contain aggregate counts and a digest, with no paths, model names or
geometry. The native transport itself emits private decoded data to its caller;
do not publish that raw output. Ordinary CTest uses authored fixtures and needs
no disc or external research checkout.

## Measured results

The owner-selected US v2.00 RTA disc has executable SHA-256
`292f6f48e3609c0026def2307d76c222cd5b389303d53b89b0e107d7b1dcd675`.
The private corpus deduplicates identical bytes per extension. Model decoding
improved from **982/1,456 to 1,455/1,456**, adding 473 accepted models.
All 1,455 decoded models match the pinned reader exactly: 6,174 meshes,
43,671 packets and 4,902,228 corners, including 419 reflection, 105 shell,
102 variant and 185 attachment forms, 419 sample blocks and 111 effect segments.
There are no mismatches, unverified accepted models, transport errors or skipped
inputs. The remaining reference-accepted/native-refused model uses world `0x46`.
Comparison digest:
`4b6d3d4eee5824a57ba5664d048a5546058bad596f1d215ce70fc86b8db00e44`.

The unchanged generic scanner reports animations **4,087/4,087** and combined
TEX/ETEX **2,065/2,188** for this byte-deduplicated corpus. The disc contains
4,510 animation occurrences, 2,323 model occurrences and 3,347 texture/ETEX
occurrences. These totals differ from the owner's earlier 463-model/496-texture
sample; no direct improvement against that sample is inferred.

All 1,199 authored model checks and 24 transport/comparison tests pass optimized
and ASan/UBSan on Apple Silicon. The integrated build passes 35 CTest cases;
source formatting and simulation-dependency checks pass. Six authored models
exercise ordinary, two-pass, shell and facing forms without original content.
The implementation commit carries this evidence; target-platform checks remain
available on its pull request.

## Evidence boundary

Reflection, shell and facing support here means structural decoding and retained
data. Reflection coordinates, lighting/blending, shell displacement and facing
evaluation remain renderer or engine work. Facing-point evidence is authored:
the selected RTA disc contains no model with flag `0x40`.

The world `0x46` form remains explicitly refused because its STMOD positions
depend on an external row transform. Silently treating that transform as zero
would invent a standalone rest pose. Norrath behavior, original rendering,
model/animation pairing and gameplay require independent evidence.
