STATUS: COMPLETE — BLEND-7 fixed; all requested verification passed, with one CTest timeout settled by an isolated pass.

# BLEND-7: per-surface creature FX lighting (2026-10-21)

Branch `codex/blend-fx` in `~/Projects/open-champions/worktrees/blend-fx`, rebased onto completed wave 19 `63c40ab`. Implementation commit `d34f172` is local only. Nothing was pushed and no PR or issue was opened.

## Change and evidence

`real_creature.gd` assigns per-surface `ShaderMaterial`s for mixed-pass models. The FX renderer now accepts a per-surface shader carrying the existing `native_fx_vertex_colors` marker, duplicates it once per source surface, and reuses that copy on relight. If a shader supplies `native_fx_unshaded_code`, the copy uses it, as the existing instance-override path does. Unmarked surface shaders still refuse.

The creature environment shader already multiplies both `base_texture` and `env_texture` samples by `COLOR.rgb`. That matches `docs/spec/render-pipeline.md` 9.6 at research pin `t3/research-textures` `1528708`: the environment pass uses the same vertex colour. No equation, pass order or constant changed.

A synthetic mixed-pass model now exercises `real_creature.apply()` and FX lighting together. It checks per-surface assignment, an environment pass on only the tagged surface, `env_texture * COLOR.rgb`, successful lighting, multiplication of the input vertex colour and copy reuse. Its rendered environment-gradient checks remain in the same smoke.

Changed tracked files: `godot/fx/native_fx_renderer.gd`, `godot/real_data/real_creature.gd` (comment), `godot/tests/creature_blend_smoke.gd`.

## Verification

- Arm64 Release configure/build: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4`: PASS. `ctest --test-dir build/native --output-on-failure -j 3`: 212/213 passed, one `player-exact` timeout at 240.04 s under concurrent load. Required isolated retry `ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j 1`: 1/1 passed in 222.93 s. All 213 cases are thereby verified; the first full run was not a clean pass. The test binaries are Mach-O arm64.
- Godot 4.5.2 dev matrix: `python3 work/playable-wave19/run-godot.py`, using wave 19's 50-command list: 47/50 first pass. Three opt-in creature-animation entries initially lacked this worktree's binary. After staging the matching wave-19 `template_debug` binary, all three passed; final **50/50**. The same-head archive and level-texture debug binaries were staged locally, with no tracked binary changes.
- Creature blend rendered smoke: `Godot --path godot --rendering-method gl_compatibility --script res://tests/creature_blend_smoke.gd`: 21 checks passed, including after rebase. The Forward+ entry also passed 21 checks. `fx_shade_batch_smoke.gd`: 12 checks, 0 failures.
- 111-level sweep: `python3 work/private/run_sweeps.py level` with wave-19 recipes/lights, `--all --levels=0 --ticks=60`, four headless slices: 111/111 tried, loaded and played; 0 failed checks; 0 refusals. Every deterministic count matches wave-19 `r19bln` slice by slice. Only `time.*`, audio frame total and memory peaks were excluded from equality.
- Creature-preview sweep: `python3 work/private/run_sweeps.py preview`, same inputs and four slices with `--levels=28`: 111/111 tried, loaded and played; 111 previews, 107 textured, 107 environment, 76 two-pass. The FX material refusal is **99 → 0** versus wave-19 `r19c`. The four texture-size refusals remain four. All other deterministic counts match each wave-19 slice, with the same run-dependent exclusions.
- `playable_wave_smoke_full_combat.gd -- --ready-ticks=640`: 2,620 checks, 0 failures. `FULL_TRACE` SHA-256 `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes, unchanged from wave 19.

The private RTA US v2.00 disc was read only for counts. Its bytes, names and decoded content were not committed. Counts and logs remain in ignored `work/private/`.

## Remaining limits

Only arm64 native CTest was requested. No x86_64, Windows or exported-app verification was run for this fix. The synthetic rendered check and headless disc sweeps do not amount to windowed visual acceptance of every creature.
