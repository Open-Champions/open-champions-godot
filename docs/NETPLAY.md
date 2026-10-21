# Lockstep session

`champions-netplay` runs 2–4 peers in one process. Each peer owns one
`netwire::Machine` and one Full combat world. Packets move through
`LoopbackTransport`. The wire codec, send cadence, hold/pacing and sync-hash
compare are the existing port of `ref_netwire.py`
(SHA-256 `4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42`,
pinned in [NETWORK_WIRE.md](NETWORK_WIRE.md)). This module does not regenerate
those vectors. `netwire-codec-exact` (103,468), `netwire-session-exact` (538)
and `netwire-tick-exact` (8,456) remain the byte comparison.

## What runs

`Machine::net_main` samples an 8-byte pad (button word, four stick channels,
two pressure bytes) plus the separate yaw and camera-flag arguments. The
local record is the raw sample. `tick` runs `local_roundtrip` on the local
slot before `pad_apply`. Remote records are already decoded and are applied
once. Send cycles stay `{6, 2, 4, 6}` indexed by remote-peer count. A machine
sends to one remote at a time, so on a single rendered frame one peer can
tick while another is still missing a sample. The session does not require
equal step counts in that call. Traces are compared by simulated tick, once
every peer has produced that tick. A loopback `send` is invisible to `take`
until `deliver`, which is the next rendered frame.

The creature uses the combat-play hit points `0x42f00000` and facing
`0x8000`. Its minimum, maximum and attack words are 0. With the spawn
default attack 100, both players are dead by tick 1160 while the creature
is still at full health, and the next tick refuses with a party wipe. Players
still hold the attack bit. The combat step is
`SessionAdapter::ready_tick(net_frame - 1)`. Wire frame 1 is combat frame 0.
After the tick the machine's seed, sim frame and player positions are
published, and the sync hash is the wire model's hash of that seed, the
accumulator and those positions. Each peer records the shared seed, four
player hit-point words and the creature hit-point word.

Full combat retains every list walk and every spell-list `iter_begin` until
65,536 iterators, then refuses. A tick walks four lists and begins one
iterator per player and per creature. Two peers with no creature stay under
that cap for 10,000 ticks. Two peers with the creature, and three peers with
no creature, refuse on the tick after 9,362. Four peers with no creature
refuse on the tick after 8,192. This session does not change that runtime.

The public test uses an authored flat heightfield (1,048,576 zero samples,
inside the 4,194,304 sample cap). `tools/netplay_level_measure.cpp` is not a
CTest. It walks a private disc with `level_flow::gob_entries` and a u16 LMP
count, decodes `.world` terrain, and prints counts only.

## What this does not simulate

Ghost prediction (`net_predictLocalPlayer` in the research netcode notes) is
not part of `ref_netwire.py`. It runs a local copy ahead and then restores
the shared seed, hit points and mana, so it does not change the lockstep
tick. Combat runtime has no contracted player clone. The codec's own stick
and yaw prediction still run inside `local_roundtrip`.

Sight stays the flat fixture's clear ray (`MINUS_ONE`). `terrain_sweep_circle`
has no radius argument on the combat raycast callback, and a miss returns
time `ONE`, which `can_see` treats as blocked. Water stays dry: a nonzero
water result refuses. `net_isLocalSlot` in the combat runtime returns 1 for
every slot. That stub is shared, so the peers still match. It is not a
per-machine local-player test.

`World::tick` refuses a nonzero byte at `0x402791` (`network, cutscene, PvP,
damage numbers or decals`). The same byte is what would enter the unported
`GutChunk_ctor` path. The session leaves the byte clear. With it clear and
two or more players alive, movement applies the per-machine screen clamp.
The session supplies an authored frustum wide enough that an actor still on
the level is inside it. Movement therefore uses the single-player speed base,
not the net-game base `0x3f400000`.

## Save

`save` is an authored journal (`OCNPLAY1`, version 1). It stores peer count,
render steps, tick count, a terrain identity (heightfield, element and sample
counts, the first element's extents, the first heightfield origin), the
placement words, the input journal and one peer's witness trace. It does not
store retail terrain bytes and it is not a PS2 save. `load` rebuilds fresh
worlds from the caller's snapshot, replays `step`, and requires the witness
and the other peers to match.

## Still open

- Internet datagrams. The research path is `sceInetSendTo` / `sceInetRecvFrom`,
  receive buffer `0x7c0`, and the documented timeouts. No socket is opened here.
- The lobby. `docs/lobby-protocol.md` in the research tree: TCP, big-endian
  length, little-endian payload, AES-256 ECB for `estr` only.
- Rendezvous. `docs/netlib.md` state 7 copies peer addresses and then deletes
  the UdpManager. UdpLibrary is the lobby handshake, not the gameplay lockstep.

Not claimed: original-client interoperability, Norrath, or a playable Godot
net scene.
