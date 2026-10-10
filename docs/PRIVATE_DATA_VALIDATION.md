# Optional private-file coverage report

After building the native targets, run:

```sh
build/native/champions-data-validate --game rta "/path/to/your/extracted/files"
```

Visual Studio builds place the executable under `build/native/Release/`.
Windows accepts Unicode paths through its wide-character entry point.
The input directory must be explicitly selected. The tool recursively checks
ANM, TEX/ETEX, VIF and GOB/LMP files, including recognized container payloads,
using the existing bounded native decoders. Symbolic links and unknown file
extensions are skipped. No files are extracted, written back or uploaded.

The TSV report contains decoded/refused/total counts per format and grouped
refusal reasons. It omits filenames and paths. A valid report exits zero even
when assets are refused; an invalid invocation, traversal failure or directory
without recognized files exits nonzero. Files above the 512MiB input limit are
reported refused. Norrath is rejected because its format contract is unvalidated.

Counts are **payload occurrences**, not unique disc assets. A file present both
loose and inside a container counts twice. Successfully reading a container is
separate from decoding its contents: refused children do not invalidate its
structurally valid table. Acceptance means the supported structure decoded,
rather than original rendering, model/animation pairing or gameplay fidelity.

The owner's full-disc comparison baseline is animations **4510/4510**, textures
**486/496**, and models **274/463**. Those full-disc totals have not been reproduced
by this change, and no coverage improvement is claimed. A local opt-in run on
the available private validation collection reported 919/919 animation,
146/148 texture and 98/123 model payload occurrences, plus 36/36 containers.
That collection includes repeated assets, authored containers and negative
fixtures. Its totals must not be compared with the full-disc baseline.
A later full-disc, byte-deduplicated validation now reports animations
4,087/4,087, TEX/ETEX 2,065/2,188 and models 1,455/1,456 (improved from 982).
The [model-form comparison](MODEL_FORMS.md) additionally matches all accepted
models' geometry and metadata exactly. Reflection/shell/facing and optional
metadata decoding are supported; facing remains authored-only. The remaining
world model requires an external row transform. Texture support is unchanged.
This full-disc corpus has a different counting scope from the earlier sample.

Public CLI checks exercise an authored empty archive, malformed inputs,
recursive traversal, mixed-case extensions, Unicode paths, ignored extensions,
symbolic links where permitted, and explicit title rejection. No original
content is committed or required by CI.

The [optional script-machine comparison](SCRIPT_MACHINE.md) additionally runs
owner-selected RTA scripts against the pinned Python interpreter and reports
complete-state parity with authored native callbacks. Its counters and explicit
HostFault/refusal outcomes do not establish game-native commands or a playable game.
