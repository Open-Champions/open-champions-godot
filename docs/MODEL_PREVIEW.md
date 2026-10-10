# Static RTA model inspection

The bounded decoder is authored from the published [model/texture contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/c3d7264/docs/formats/model-texture.md). It targets unrelocated Return to Arms US v2.00 `.vif` lumps. Selecting RTA in the browser is an explicit profile choice, not automatic title or edition detection. Norrath decoding remains unavailable until independently validated.

## Scope

The first subset is the single-pass model program `0x42`. The decoder reconstructs triangle corners from VIF packet positions, normals, strip slots and copy rows. It preserves per-corner texture coordinates in texel units and the default mesh visibility mask. Positions are model-space rest positions divided by 16; normals use the documented scale of 127. Rendering uses two-sided neutral shading. Textured materials, animation, variants, attached parts, second/shell passes, world geometry and gameplay are outside this milestone.

The original model axes are Z-up. Presentation converts them to Godot with `(x, z, -y)`. Framing uses the visible decoded vertices rather than the stored signed-byte bounds, which can saturate for tall objects. Part masks describe meshes hidden on a parent and must not hide the part's own meshes. UVs remain texel coordinates until a separately validated material supplies a texture size.

The parser validates the full selected model, including hidden meshes. Unknown commands, unsupported programs and malformed table or packet ranges fail explicitly. Inputs and triangle output have fixed budgets. The adapter reopens the selected archive, validates its fingerprint and resolves the selected entry again rather than trusting UI offsets. No original game files are distributed.

## Verification

Local validation passed 11 native CTest cases and 358 model checks, including AddressSanitizer and UndefinedBehaviorSanitizer. Authored regressions cover malformed/truncated packets and tables, bounded allocations, visibility masks, per-corner attributes and the copy/write ordering of strip slots. The real Godot binding and presentation smoke pass 97 checks, including orbit, zoom, reset, selection invalidation and profile rejection.

Twenty distinct private models from twenty archives matched the research reader's triangle positions, normals, UVs and default visibility. The samples include models with up to 30 meshes and 18,682 triangles. Position/UV values compare exactly; float32 normal values compare within 1e-7 of the reference. Together the samples contain 110 meshes, 1,030 packets and 41,359 triangles. The private executable was freshly checked against the pinned RTA baseline. A private 4,937-triangle model also passed the real Godot binding and ArrayMesh creation headlessly. These samples do not establish disc-wide coverage.

Graphical/export and current platform CI checks are still pending under [issue 7](https://github.com/Open-Champions/open-champions-godot/issues/7). A successful parser or model viewer does not establish animation, original material behavior, collision, playable scenes or native game compatibility.
