# Return to Arms inventory picture-cache pixels

The native module builds a picture filename from caller-supplied item fields,
definition and model tables, prefix colours and the **viewing player's** body
code. It finds that name in a bounded `PICCACHE.HDR` index, models the 64-slot
streaming cache, and decodes a selected sector payload from `PICCACHE.DAT`.
The grid placement and the separate gold-pile bottom-padding rule are also
available. It does not discover either file on disk. Norrath is not covered.

## Pinned evidence

The implementation was handwritten from these read-only research files on
`t3/research-ui` at commit
`5af769188dfbd47e15693f5041ae5d6d90292bc9`:

| Research file | SHA-256 |
|---|---|
| `docs/formats/inventory-icons.md` | `357af76eacd4c58ebab406f59d641fd4b85e52d0417b48f17fb028937654f8a6` |
| `tools/ref_inventory_icons.py` | `4c005d17db5a7927c7b8908e32ebdf904b14ab9be04b81e7dd351d1816ed5716` |
| `tools/test_ref_inventory_icons.py` | `80775a10156d0cc9f48ad84fe137556923f10d7209c459b76727b1bffa4cc8a3` |

Round 2 association, placement and flag behavior are pinned to the same
research branch at commit
`ad8c34ee7e4591a29fed361322bed1e085403752`:

| Research file | SHA-256 at round 2 commit |
|---|---|
| `docs/formats/icon-association.md` | `e42412b4735488d54d58c01fc258276dfdacb112b4c7e79374ef63e1b8b90746` |
| `tools/ref_icon_assoc.py` | `a7929a60523aae84ea024df48881a68824470140e510b0fbd594ef3f50ec823a` |
| `tools/test_ref_icon_assoc.py` | `fbdfd2c23306fc015a159cf9d7385e9ca341c4f5d471acb5dc204c0f267cb465` |
| `docs/formats/inventory-icons.md` | `f825e0c33f94284f05e011ae8c728ef78cf81e7cc26c0f03dee92e60804969b6` |
| `tools/ref_inventory_icons.py` | `7ca50e56d5daad2f315235ca6be065d6f671c46cadb145dd729c86f2056151e1` |
| `tools/test_ref_inventory_icons.py` | `902b1bbf2358e49134fb51f1558d5c5100b4e0f3cc9a40bc80ef6bb5c5cdbfc2` |

The research audit reports structural coverage of 11,511 entries in 449 groups
from a privately supplied SLUS-20973 v2.00 cache. That is research evidence,
not a native-client acceptance count or a rendered pixel comparison. This
worktree uses authored synthetic packets only; no private game files were
available under the shared project rules.

The round 2 research audit reports 22,986 filename executions, 7,285
parser-built item records and 1,652 index lookups, all matching its reference
(31,923 cases); all 7,285 parser-built names resolve to an index entry. Those
are research-side measurements, not native-client acceptance on game files.

## Item association, cache and placement

`picture_filename` follows `item_buildPictureFilename` (`0x107edc`): a name is
constructed from the definition picture stem or model texture, variant bits
11–15, prefix weapon/armour/alternate signed colours, flags, category, subtype,
slot, and viewer body code. Preset armour requires the caller's `item_getValue`
result. Concatenated variant and colour digits can alias; an item has no stored
picture ID. Inputs and all lookup tables are supplied by the caller.

`parse_index` reads bounded little-endian HDR groups and 10-byte entries;
`find_entry` splits off the last extension and trailing decimal number, then
searches groups in file order, including later groups with the same stem.
`payload_range` validates the selected 2048-byte sector span against a supplied
DAT size. The sector payload is then passed to `decode` below.

`PictureCache` models 64 named slots with Pending, Reading, Loaded and Missing
states. A request queues a miss and returns no picture until loaded. Call
`service(frame, read_done)` once per displayed frame: it completes an
outstanding read, then starts at most one pending read; an unfinished read
blocks the queue. At capacity it evicts the first oldest slot used strictly
before `frame - 8`, excluding the active read; otherwise the request is
dropped. The caller owns the actual asynchronous DAT I/O and must display
`equipmnt.lmp` / `pic_dagger1.tex` while no loaded picture is returned.
The model does not allocate or exhaust the original 256 data blocks.

`grid_placement` uses HDR width and height: columns are `width/32 + 1`, rows
are `ceil(height * float_from_bits(0x3cf0f0f0))`, with 32×34 cells. For panel
origin `panelX`, column and `rowY`, `x = panelX + 15 + column*32 + footW/2 -
width/2` and `y = int(rowY - 30) + (footH - height)/2 + 6`, using integer
halves truncated toward zero. The grid ignores header flag `0x0800`.
`bottom_anchor_y` adds one to the top y when `0x0800` is set because a
bottom-aligned gold-pile picture has a transparent last row. `0x0040` is
always set in the cache but no runtime reader was found; `0x1000` remains
without a general documented meaning. Neither changes decoded pixels.

## Native contract

`champions::inventory_icons::decode(data, size)` returns logical width, height,
flags, serialized qword count and offset, and exactly `width * height * 4`
row-major RGBA8 bytes. Original GS alpha is preserved; in that convention
`0x80` represents opaque. `display_rgba` is an explicit presentation choice
that maps alpha to `min(255, 2*a)` without changing RGB, including hidden RGB.

The parser reads the little-endian picture header, checks the packet offset
and `Q * 16` span, then walks bounded GIF tags. It supports packed A+D GS
register writes and IMAGE data in the documented profile. It requires one
16×16 RGBA palette upload and one image upload, both host-to-local PSMCT32,
with the documented destination and transfer bounds. Optional LABEL writes
after the image do not become pixels. The image upload is a rectangle of
32-bit words; its width and height cannot be inferred from the logical icon.

Uploaded words use PSMCT32 GS addresses with DBW=1. Visible indices read the
same memory with PSMT8 GS addresses and TBW=2. Output row `y` reads stored row
`y/2 + (H/2 if y is odd else 0)`, using logical height. Palette indices swap
bits 3 and 4 before RGBA lookup. Missing supplied GS words, malformed packets,
odd height, unsupported header flags, transfer formats and dimensions fail
explicitly. `dma_chunks` models the later runtime split into at most 79-qword
chunks with NEXT and RET tags; those runtime headers are absent from DAT.

The standalone module is Godot-free. The thin
`ChampionsArchiveInspector.decode_inventory_icon(sector_payload, "rta",
display_alpha=false)` binding accepts an explicitly supplied sector payload
and returns a dictionary containing `ok`, `width`, `height`, `flags`, and
`rgba` on success, or `ok=false` and `error` on refusal. A scene can create an
`Image.FORMAT_RGBA8` from that byte array. `lookup_inventory_icon(index_bytes,
picture_name, "rta")` returns the HDR entry's sector span and dimensions;
`inventory_icon_grid_placement(width, height, panel_x, column, row_y, "rta")`
returns the documented footprint and
origin. A scene can use the resolved sector span with the decoder. These
methods neither load files nor enforce the streaming schedule. The play shell now offers an explicit HDR/DAT picture inspection grid through
the original-art panel. It uses bounded range reads, native decoding and grid
placement with authored first-fit cells. It does not connect live item ownership,
the filename builder or streaming-cache scheduling: item tables, viewer body and
original callsite screens are not part of the scene's present contract. See the
latest [playable integration report](PLAYABLE_WAVE_2026_10_17.md). Decoded pixels can also be
passed to the existing native upscaler, with an explicit alpha policy.

## Verification and limits

`native/tests/inventory_icons_tests.cpp` ports the reference's authored
fixture and exact logical crop, field-weave, CLUT, alpha, DMA chunk, page
bijection, page landmark, flag-variant, bottom-padding and rejection vectors.
`native/tests/inventory_icon_assoc_tests.cpp` ports the 15 authored filename,
body, index and streaming tests, then checks the documented grid formula and
bounded sector ranges. Run them with:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^inventory-icons$|^inventory-icon-assoc$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
# With the pinned godot-cpp checkout and Godot 4.5.2 from GODOT_BROWSER.md:
cmake -S gdextension -B build/godot-extension \
  -DGODOT_CPP_DIR=/absolute/path/to/pinned/godot-cpp \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension -j 4
Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
Godot --headless --path godot --quit-after 120 \
  --script res://tests/inventory_icons_smoke.gd
```

The synthetic tests establish parity with the documented reference vectors.
They do not prove original-game rendering parity or real-cache native
acceptance. The 1,561 HDR entries unreached by broad generated-name
enumeration, which screens call the picture function, spectator viewer-body
selection, the original inventory's exact screenshot placement, equipment
cache compatibility, Norrath support, and the original item-driven inventory screen
remain gated on research or scene ownership. The explicit picture inspection
grid has synthetic exported-scene acceptance; this does not remove those gates. Private files were unavailable
under the shared rules, so no native private-input measurement was made.
