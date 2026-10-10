# Native level-script machine

`champions-amx` translates the published Return to Arms AMX reference into portable
C++17, with no engine, viewer or host floating-point dependency. Its contract is
[ref_amx.py at 5ad489a](https://github.com/Open-Champions/champions-return-to-arms-re/blob/5ad489a445d41612d0795c063e1ce67557d5c441/tools/ref_amx.py),
SHA-256 `091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f`,
and the accompanying
[specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/5ad489a445d41612d0795c063e1ce67557d5c441/docs/spec/script-machine.md).
The model's MIT license and contributor copyright apply to this authored
translation. No original script, executable or decompilation is distributed.

## Exact authored fixtures

| Suite | Cases | Compared behavior |
|---|---:|---|
| Image | 3,779 | Headers, loading, relocation, debug/case operands, compact encoding and fixture assembly |
| Execute | 6,412 | Instructions, division, jumps, memory, callbacks, nested runs, budgets, errors and exits |
| Host | 1,107 | Strings, heap, binding, dispatch, snapshots and retained state |

Tests compare complete images and machine fields, decoded operands where
relevant, callback arguments/order, profiles and remaining budgets. The execution
corpus reaches all 131 dispatch cases. Opcode NONE requires authored decoded-map
injection because the loader refuses it. CASETBL's executable handler remains a
HostFault. Numeric runtime errors, HostFault, Limit and direct zero-division
exceptions are tested separately. Missing, changed, empty, malformed, duplicate
and truncated fixtures fail.

The port preserves ignored relocation failure, signed packed-string detection,
unsigned heap-allocation underflow, forward overlapping MOVS, early CMPS exit and
RETN's saved-stack drift. It does not apply the proposed research patch. Callback
replacement retains captured callable/decoded-map/profile identities while
nested executions share the live budget. Snapshots restore image bytes and
fourteen saved fields; bindings, callback, params, profile, budget, decoded map
and convenience fields remain outside restore.

Host addresses have signed64 representation limits; flags are uint32. Images,
strings and snapshots have a 64MiB resource ceiling, decoded maps one million
instructions, and nested native dispatch 128 invocations. These limits refuse
with ResourceLimit, a Limit subclass. Forged negative physical offsets and
undefined pre-load attributes refuse as HostFault. Outside-buffer Python
IndexError/struct.error cases normalize to HostFault. These safety boundaries do
not represent successful execution beyond the owned image.

## Optional owner-disc comparison

Build `champions-amx-validate`, then select an extracted script directory and a
research checkout containing the exact pinned model:

```sh
python3 tools/validate_amx_scripts.py --game rta \
  --directory /private/extracted/scripts \
  --research-dir /path/to/champions-re \
  --native build/native/champions-amx-validate \
  --padding 256 --report work/private-script-report.json
```

Each public entry runs from fresh state under five authored return sequences,
then every entry runs without reloading. Identical stubs include nested
`callScript`. Every image byte, saved field, native argument/string, opcode
profile and budget is compared exactly. Reports contain aggregate decoded/refused
counts and a digest, with no script paths, names or contents. Inputs and raw
transport remain private. Links are skipped; missing input, a stuck/refused
transport or any mismatch fails. Loader refusals still require a successful
transport exit.

The available private RTA corpus has 120 scripts and 1,828 public entries. With
explicitly supplied 256 zero bytes after each file, all 10,968 runs match:
419,943 instructions, 37,541 native calls and 1,883 nested calls, no host faults or
budget refusals, and fourteen matching VM error 4 results. Comparison digest:
`6abed90b51444cc02b9a493a17a970a5fbc961ea24a89d9219310c22afeaf598`.
The authored return generator differs from the research differential test, so
instruction/call counts need not equal its report.

Without padding, the same corpus has eight matching HostFaults and 419,927
instructions; digest:
`69a2510cdbc5b42812d257f630afeda61a2422afbcc3d91aefa4c4e342636451`.
Out-of-file stack behavior is preserved. Neither mode establishes what followed
the file on a console.

## Reproduction and limits

Regenerate authored vectors with the external pinned research checkout:

```sh
python3 tools/generate_amx_image_vectors.py --research-dir /path/to/champions-re --output work/amx_image.tsv
python3 tools/generate_amx_execute_vectors.py --research-dir /path/to/champions-re --output work/amx_execute.tsv
python3 tools/generate_amx_host_vectors.py --research-dir /path/to/champions-re --output work/amx_host.tsv
```

Manifests pin reference, generator and decompressed fixture hashes. Ordinary
CMake/CTest needs no research checkout or game files. CI requires all thirteen
simulation suites on five hosts, comparing actual traces against independent
expectations and the tested commit. The integrated local build passes 31 CTest
cases, including six transport anchors. Each AMX suite passes optimized and
ASan/UBSan checks on Apple Silicon.

The game's 191 native engine commands remain explicit callback dependencies.
This establishes interpreter/model parity, including private execution with
stubs. Engine commands, a playable scene, console behavior and Champions of
Norrath script compatibility require separate evidence. The
[network-wire port](NETWORK_WIRE.md) follows this interpreter; remaining
model-format coverage comes next.
