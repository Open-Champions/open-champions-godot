# RTA 32-bit texture preview

The archive browser can preview the documented 32-bit lump-texture subset in a
selected Return to Arms GOB or LMP. Select a `.tex` or `.etex` entry, then use the
preview control. Norrath decoding is explicitly unavailable pending an independently
identified baseline and format contract. Merely selecting RTA does not detect or
certify the archive's edition.

## Supported contract

The portable decoder is independently authored from the published
[model/texture research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/c3d7264/docs/formats/model-texture.md)
for the US v2.00 RTA baseline. It accepts unrelocated 0x80-byte lump headers,
32-bit PSMCT32 storage and one bounded host-to-local GIF upload rectangle covering
every used pixel. Padding outside the used dimensions may be cropped. It returns
owned RGBA8 pixels in stored row order and cropped to the used dimensions. Header,
packet, register, transfer and allocation bounds are validated before publishing
pixels. Unsupported variants fail explicitly.

Palette formats, streamed world texture cells, blank render surfaces, unknown
registers and relocated headers are not decoded. The decoder is separate from
Godot and can be tested without any original game files. The browser revalidates
the archive and selected entry before previewing; a content fingerprint rejects a
selection after its file changes. It does not trust a UI-supplied byte offset.

## Display semantics

RGB preview ignores stored alpha so that the underlying colors remain inspectable.
The optional alpha view shows an opaque grayscale channel scaled from 0..128 to 0..255,
saturating above 128. It does not infer opacity or a material's blend equation.
Model and sprite alpha have different uses in the original renderer. Field weaving,
orientation in the game, filtering, wrapping, lighting and material blending are
not reproduced by this initial inspection view.

No game data is included in the source, tests, packages or public screenshots.
Original inputs remain on the owner's device. The preview is an asset-inspection
feature; it is not gameplay, a native game scene or a compatibility claim for Norrath.

## Verification

Local native validation passed nine CTest cases, including 349 texture checks.
The texture test also passed AddressSanitizer and UndefinedBehaviorSanitizer.
Godot editor and actual Mac export smoke checks passed 47 checks.

The final decoder output matched all RGBA bytes of one unique private 128×128
RTA texture against the published research decoder. Four available level copies
contained the same pixels, so they are counted as one sample. This establishes
that sample comparison; it does not verify rendering against a running PS2 game
or every texture on the disc. Private samples and pixel exports remain private.

The exported Mac application was visually checked with an authored asymmetric
fixture: RGB colors, alpha-channel display, selection/filter behavior and explicit
unsupported-format errors. The real private archive opened and listed its entries.
A desktop-inspection service failure prevented the final private-image click;
that graphical check remains unverified. Windows graphical inspection is also
unverified. Updated platform CI results will be recorded separately.
