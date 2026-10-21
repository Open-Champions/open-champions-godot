# Native network-wire rules

`champions-netwire` ports Return to Arms' published Python codec, peer/session
state and network-tick model to portable C++17. The contract is
[ref_netwire.py at 54258f9](https://github.com/Open-Champions/champions-return-to-arms-re/blob/54258f9d7e281ef07f0675040e82e6264f9386cf/tools/ref_netwire.py),
SHA-256 `4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42`,
and its [wire specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/54258f9d7e281ef07f0675040e82e6264f9386cf/docs/spec/net-wire.md).
The model's MIT license and contributor attribution apply to the authored port.
No retail packet captures, game assets or original executable bytes are shipped.

## Exact comparisons

| Suite | Cases | Compared behavior |
|---|---:|---|
| Codec | 103,468 | Raw arithmetic, bitstreams, channels, frame compression and local round trips |
| Session | 538 | Headers, peer images and rings, messages, packet build/parse, receive, relay and kick handling |
| Tick | 8,456 | Controller repeat/mute, launch packets, synchronization hashes, pacing and stateful main/tick callbacks |

All sixteen arithmetic configurations are explicit. Each machine owns its FPU
context; the implementation neither consults nor changes the core's current
context. Simulation uses raw integer representations rather than host floats or
viewer binary64 arithmetic. Callbacks supply game tasks, message handlers and
external observations.

Tests compare packet bytes, all peer memory and mutable counters, ring contents,
callback arguments/order, outgoing packets, errors and retained partial changes.
The fixtures cover input-copy ownership, callback replacement and live inbox
mutation. A peer-count error occurs when the invalid slot is accessed, preserving
earlier valid-slot changes. Undefined packet padding remains explicitly marked;
the model's zero placeholders do not establish original stack bytes.

The three suites pass optimized and ASan/UBSan runs on Apple Silicon. The
integrated build has 34 CTest cases. Missing, empty, malformed, changed, duplicate
and incomplete fixtures refuse rather than report a successful comparison.
CI requires sixteen exact simulation suites on five native hosts, including
independent comparison of actual compressed trace bytes, fixture provenance and
the tested source commit. The combined gameplay corpus has 1,408,647 cases per
host. Target-platform results are recorded in the pull request checks.

## Reproduce

Ordinary CMake/CTest uses compressed authored fixtures and needs no research
checkout or disc. To regenerate fixtures, select the external research checkout;
generators load the exact pinned revision and validate its hash:

```sh
python3 tools/generate_netwire_codec_vectors.py --research-dir /path/to/champions-re --output work/netwire_codec.tsv
python3 tools/generate_netwire_session_vectors.py --research-dir /path/to/champions-re --output work/netwire_session.tsv
python3 tools/generate_netwire_tick_vectors.py --research-dir /path/to/champions-re --output work/netwire_tick.tsv
```

Manifests pin the reference, generator, decompressed fixture and expected result
hashes. The public fixtures contain only authored inputs and model results.

## Bounds and remaining integration

Owned byte buffers are limited to 16MiB, frame collections to 65,536 and bounded
loops to 1,048,576 operations. Mutable Python integers use signed64 storage;
overflow refuses explicitly unless the model specifies wrapping. Four owned peer
objects support field mutation, but replacing an active whole peer object is
outside the callback contract. Resource exhaustion raises ResourceLimit.

The pinned Python model does not terminate for `read_frames(b'', 1)` or
`read_axis_symbols(BitReader(b''), 1)`: the exhausted reader returns zero and the
zero-length run never advances. Both were independently confirmed with a
one-second subprocess timeout. Native budget anchors refuse these cases; they
are excluded from finite Python vector comparisons. The research source is
unchanged.

This establishes parity with the RTA wire model within the stated bounds.
The [lockstep session](NETPLAY.md) drives this model for 2–4 peers through an
in-process loopback. Internet datagrams, the lobby and rendezvous are still
open. Socket transport, actual engine task/message handlers, live peer
interoperability, console compatibility and Norrath behavior require separate
evidence. The library is not yet connected to a playable scene. Remaining
model-format coverage follows the five gameplay ports.
