# Return to Arms inventory picture-cache pixels

This decoder accepts one **serialized sector payload** from the Return to Arms
`PICCACHE.DAT` picture cache. The caller must supply the bytes beginning at
`DAT + first_sector * 2048`, bounded to `sectors * 2048` by a separately
validated HDR entry. The decoder does not parse `PICCACHE.HDR`, discover the
file on disk, or associate an item/model with a picture. It rejects unsupported
profiles rather than treating the payload as a standalone TEX lump. Norrath is
not covered.

## Pinned evidence

The implementation was handwritten from these read-only research files on
`t3/research-ui` at commit
`5af769188dfbd47e15693f5041ae5d6d90292bc9`:

| Research file | SHA-256 |
|---|---|
| `docs/formats/inventory-icons.md` | `357af76eacd4c58ebab406f59d641fd4b85e52d0417b48f17fb028937654f8a6` |
| `tools/ref_inventory_icons.py` | `4c005d17db5a7927c7b8908e32ebdf904b14ab9be04b81e7dd351d1816ed5716` |
| `tools/test_ref_inventory_icons.py` | `80775a10156d0cc9f48ad84fe137556923f10d7209c459b76727b1bffa4cc8a3` |

The research audit reports structural coverage of 11,511 entries in 449 groups
from a privately supplied SLUS-20973 v2.00 cache. That is research evidence,
not a native-client acceptance count or a rendered pixel comparison. This
worktree uses authored synthetic packets only; no private game files were
available under the shared project rules.

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

The standalone decoder is Godot-free. The thin
`ChampionsArchiveInspector.decode_inventory_icon(sector_payload, "rta",
display_alpha=false)` binding accepts an explicitly supplied sector payload
and returns a dictionary containing `ok`, `width`, `height`, `flags`, and
`rgba` on success, or `ok=false` and `error` on refusal. A scene can create an
`Image.FORMAT_RGBA8` from that byte array. No scene uses this hook yet because
the item-to-cache-entry association and runtime inventory selection are outside
this contract. It does not try to infer those associations or select private
files itself. Decoded pixels can also be passed to the existing native
upscaler, with an explicit alpha presentation policy.

## Verification and limits

`native/tests/inventory_icons_tests.cpp` ports the reference's authored
fixture and exact logical crop, field-weave, CLUT, alpha, DMA chunk, page
bijection, page landmark, flag-variant and rejection vectors. Run it with:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^inventory-icons$' --output-on-failure
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
They do not prove original-game rendering parity, real-cache native acceptance,
semantic item association, equipment-cache compatibility, Norrath support, or
end-to-end inventory scene display. An item-to-payload map and private input
would be needed for those later measurements.
