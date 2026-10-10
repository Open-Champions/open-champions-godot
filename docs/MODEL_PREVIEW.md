# Static RTA model inspection

The bounded decoder is authored from the published [model/texture contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/c3d7264/docs/formats/model-texture.md). It targets unrelocated Return to Arms US v2.00 `.vif` lumps. Selecting RTA in the browser is an explicit profile choice, not automatic title or edition detection. Norrath decoding remains unavailable until independently validated.

## Using the viewer

Open an owner-supplied archive under the Return to Arms profile, select a supported `.vif` entry and choose **Preview selected model**. Drag within the panel or use the direction buttons to orbit; scroll or use +/− to zoom. **Frame** restores the initial camera. Models use their default visibility selection; variants and equipped parts are not assembled. When a supported same-name texture is present in the same LMP, **Texture colors** applies its RGB pixels. Turn it off to return to neutral shading. Missing, ambiguous or unsupported textures leave the model visible and explain why.

## Scope

The first subset is the single-pass model program `0x42`. The decoder reconstructs triangle corners from VIF packet positions, normals, strip slots and copy rows. It preserves per-corner texture coordinates in texel units and the default mesh visibility mask. Positions are model-space rest positions divided by 16; normals use the documented scale of 127. Rendering is two-sided, with optional RGB texture inspection or neutral shading. Original material blending/lighting, animation, variants, attached parts, second/shell passes, world geometry and gameplay are outside this milestone.

The original model axes are Z-up. Presentation converts them to Godot with `(x, z, -y)`. Framing uses the visible decoded vertices rather than the stored signed-byte bounds, which can saturate for tall objects. Part masks describe meshes hidden on a parent and must not hide the part's own meshes. UVs remain texel coordinates until a separately validated material supplies a texture size.

The parser validates the full selected model, including hidden meshes. Unknown commands, unsupported programs and malformed table or packet ranges fail explicitly. Inputs and triangle output have fixed budgets. The adapter reopens the selected archive, validates its fingerprint and resolves the selected entry again rather than trusting UI offsets. No original game files are distributed.

## Verification

Local validation passed 11 native CTest cases and 358 model checks, including AddressSanitizer and UndefinedBehaviorSanitizer. Authored regressions cover malformed/truncated packets and tables, bounded allocations, visibility masks, per-corner attributes and the copy/write ordering of strip slots. The real Godot binding and presentation smoke pass 103 checks, including orbit, zoom, reset, selection invalidation and profile rejection.

Twenty distinct private models from twenty archives matched the research reader's triangle positions, normals, UVs and default visibility. The samples include models with up to 30 meshes and 18,682 triangles. Position/UV values compare exactly; float32 normal values compare within 1e-7 of the reference. Together the samples contain 110 meshes, 1,030 packets and 41,359 triangles. The private executable was freshly checked against the pinned RTA baseline. A private 4,937-triangle model also passed the real Godot binding and ArrayMesh creation headlessly. These samples do not establish disc-wide coverage.

The exported Universal 2 Mac application at `1f6cadc` passed all 103 checks and was inspected graphically on Apple Silicon. A private 4,937-triangle cart model rendered with neutral shading; orbit buttons, actual pointer dragging, wheel zoom and framing reset were verified. An authored eight-triangle crystal supplies the public screenshot. No original model geometry or pixels are included in delivered packages or screenshots.

The desktop test tool delivered changing absolute pointer positions with zero relative motion and no button mask. The viewer now tracks the press and uses position differences, with release/focus-loss cancellation. The corrected exported app visibly responds to dragging. Temporary event diagnostics were removed before the final build.

At source revision `1f6cadc`, the [Godot platform run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38042828784) passed on Apple Silicon/Intel macOS, Windows ARM64/x64 and Linux x64. Each job passed 103 source checks, 103 actual exported-application checks and 21 packaging tests. The [native foundation run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38042828774) passed all 11 CTest cases on those five hosts. Delivered Mac Universal 2 and Windows ARM64/x64 packages were verified against that revision, ZIP checksums, per-file inventories and smoke-log hashes; both Windows extension binaries import only `KERNEL32.dll`. Windows, Intel Mac and Linux graphical interaction and clean-machine installation remain unverified. A successful parser or model viewer does not establish animation, original material behavior, collision, playable scenes or native game compatibility.


## Texture-color inspection

The adapter resolves the model's full entry stem plus `.tex` inside its actual LMP byte range. Names compare with ASCII case folding; multiple matches are ambiguous even when one uses the exact spelling. Another GOB container with the same name never supplies a fallback texture. The existing owned archive and fingerprint checks cover geometry and pixels together. A missing, empty, malformed or unsupported companion retains valid geometry with a clear texture status.

UVs are divided once by the decoded used width and height. The texture keeps its stored top-left row order. This view uses opaque RGB, nearest filtering and clamp-to-edge sampling as explicit inspection choices. Raw alpha is retained in the adapter result but ignored by the model display. RTA's inverted model alpha, alpha test, per-packet blending, texture combine mode and lighting are not replaced with ordinary Godot transparency. Original wrap/filter state also remains unverified.

Local source validation passes 128 Godot checks, 11 native CTest cases and 21 packaging tests. Twenty private same-LMP model/texture pairs retain reference geometry; 19 usable textures match the reference dimensions and complete RGBA hashes. One empty texture correctly retains the geometry with an unavailable texture status. Synthetic regressions cover physical-container isolation, duplicate names, rectangular UV normalization, opaque display without mutating source alpha, and invalidation cleanup.

Current integration and platform validation is tracked in [issue 8](https://github.com/Open-Champions/open-champions-godot/issues/8).
