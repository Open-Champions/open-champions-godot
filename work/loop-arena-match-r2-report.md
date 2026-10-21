STATUS: BLOCKED — the required first milestone has no executed Player replacement constructor contract, so an exact native mode-B match cannot be built from the supplied research.

# Arena match implementation round 2 — 2026-10-21

## Base and evidence

- Merged the required wave-22 integration commit `af9e3b5` into `codex/arena-match` as local merge commit `4cc43f9`. The wave-23 report did not exist in the integration worktree, which also held uncommitted work; that worktree was untouched. `git merge-base --is-ancestor af9e3b5 HEAD` passed and the arena worktree is clean before this report.
- Inspected `t3/research-arena-match` at `9b7f6ed`, its passing Claude audit, `docs/spec/arena-match-probe.md`, per-class and core specs, and the reference models and probe. The research's recipe-0 waves 0–2 use 2, 3 and 4 creature fixtures. Its player allocation at `arena_startWave` is deliberately returned as null; the fighting probe rebinds an authored player. Goblin and Mummy constructor difftests use a base Creature fixture and stub holder, resources and engine services. Orc2 comparisons cover a neutral recruit and pinned combat call paths, not other variants. The Scorpion has broader class difftest coverage.

## Exact first blocker

The handoff ranks the first native step as replacing the player in `arena_startWave` with a 0x590-byte `Player_ctor`, record link, tick and input. `tools/probe_arena_match.py` explicitly makes this allocation return zero and uses a stand-in player. `docs/spec/arena-match-core.md` executes the `player_respawn` wrapper but stubs `Player_ctor` and spawn protection; it does not specify their state, calls or shared-seed draws. `docs/player.md` describes the object and record, but supplies no executed full constructor comparison. The native combat World's Full player is explicitly an authored setup, not `Player_ctor`; its current tick adapter is also a bounded combat composition. Substituting it would invent the replacement's state and seed progression. A real match's final seed and placement are explicitly unverified by the research.

This is the first required milestone, so later class and flow ports were not started out of order. The additional class-resource, constructor-chain, holder and live-world services needed to make Goblin, Mummy and Orc2 into real World actors remain unverified in the provided arena tests. Full-match replay needs executed Player replacement and class constructor/holder contracts, then a target-runtime comparison. Other class variants and spell branches remain refusals. Damage must use the documented 10..20 roll and must not assert exact HP. Clip lengths and root motion must come only from the owner's disc at run time.

## Verification and scope

- `git merge-base --is-ancestor af9e3b5 HEAD`: PASS.
- `git status --short --branch` after the merge: clean `codex/arena-match` before this report.
- No implementation changed. Native CTest, the Godot dev matrix, the requested arena smoke, the scripts-on/off 111-level sweeps and FULL_TRACE were not run; none could verify an unbuilt match. Wave-22's reported baseline results are not claimed as round-2 results.
- The private disc was not read in this round. No private bytes, names or decoded content were saved. No process was killed. No push or PR was made.

The required evidence to unblock the first step is an executed `Player_ctor`/arena replacement comparison with its record link, tick/input handoff and seed effects, using the same supplied account and disc revision. Subsequent class constructor and live-world comparisons are needed before a mode-B Godot smoke can truthfully reach three waves.
