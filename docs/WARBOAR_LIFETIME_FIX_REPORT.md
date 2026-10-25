STATUS: MORE

# WBAIP-1: campaign actor lifetime

Branch: `codex/play-boar-ai`, base `d0027035`. Local commits only; never pushed.

## Fix and evidence

The runtime retires dead or replaced actor entries before construction,
adoption, lookup and enumeration. Liveness requires both the live flag and
the identical object pointer in the owning World's address map. Construction
and adoption share checked insertion and return only the stored actor.
Invalid/foreign adoption and duplicate live adoption refuse without replacing
the live actor. Actor references expire when their GameObjects are destroyed;
retirement occurs on the next runtime access, not through a World callback.

Evidence: audit `play-boar-ai-r1.md`, WBAIP-1; research-campaign-combat at
`328b072`, `tools/ref_objects.py:571` (immediate pool return and address-map
removal) and `tools/ref_core.py:2134` (free-list head insertion). Native
`objects_lifetime.cpp` retains object instances while recycling pool addresses,
so pointer identity distinguishes successive occupants. No new game behavior,
constants, assets or retail bytes are introduced.

The same inherited defect exists in `codex/valor-fight` (already merged into
integration); this fix reaches integration through `codex/play-boar-ai`.

## Synthetic regressions

- Destroy/reconstruct at the same pool address; access the replacement's kind,
  health and runtime lookup, dispatch its tick, then verify dead-entry retirement.
- Shared-World destroy/re-adopt, with and without an intervening lookup; verify
  replacement identity, shared health and tick dispatch, reject duplicate live,
  retired and foreign objects, and preserve the existing live actor on refusal.

Changed files: `native/include/champions/campaign_actors.hpp`,
`native/src/campaign_actors.cpp`, `native/tests/campaign_actors_tests.cpp`,
`native/tests/campaign_fight_adopt_tests.cpp`, and this report.

## Gates

Release build and focused fixtures are run before the implementation commit.
Full CTest, rebuilt Godot dev matrix, 111-level sweep, unchanged FULL_TRACE
and boar approach checks follow that early commit. Final figures are recorded
here after execution; ignored evidence lives under `work/boar-ai/`.
