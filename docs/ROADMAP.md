# One native client, two games

## Current milestone

The default Godot scene is a bounded, Session-backed movement and health test with
authored controls. A standalone C++17 archive inspector and archive browser remain
available using the same parser through GDExtension. They list GOB/LMP
metadata, including nested LMP entries, with explicit little-endian fields and
bounded tables, names and payload ranges. The browser has explicit title selection
and a local file picker. Selected RTA lump entries can preview the documented
32-bit, direct/PSMCT32-uploaded 4-bit and 8-bit texture subsets. Bounded RTA static
[model forms](MODEL_FORMS.md) support base rest-pose inspection with neutral shading or opaque texture colors and single-bit variant visibility. The portable core also retains per-corner joint indices and exact byte weights; a bounded [skeleton metadata reader](ANIMATION_METADATA.md) now preserves RTA joint hierarchies and signed bind offsets. The [key decoder](ANIMATION_KEYS.md) also retains exact initial poses and velocity records. A [local-pose sampler](ANIMATION_POSES.md) now evaluates one clip at an explicit tick. The [transform builder](ANIMATION_TRANSFORMS.md) composes model-space and skin matrices. The [joint viewer](ANIMATION_VIEWER.md) now displays manually sampled RTA clips. A portable [weighted-position operation](SKINNING_POSITIONS.md) now applies explicit skin transforms; an [explicit Godot sampling bridge](SKINNING_BRIDGE.md) exposes positions for two selected entries. An [experimental sampled model viewer](SKINNING_VIEWER.md) draws explicitly selected pairs with unlit geometry. Semantic model pairing and animation playback remain work. Streamed texture cells,
original materials, engine script commands and playable gameplay remain unimplemented. WORLD draw applies the documented environment rows, console layer order, automap tile UVs and chunk alpha and constant-light modes through the existing native-fx lighting model; automap atlas pixels and the HUD window remain unresolved. Title labels
are not validated game profiles; texture and model decoding
explicitly rejects Norrath until its contract is established.
The [UI text gaps port](UI_TEXT_GAPS.md) adds the help box inner frame, button glyph and label, the bust meter and its font, the dialog fade step, the cursor glyph and the compass label from executed research; the compass mesh and chime slot life stay refused.

The standalone [Return to Arms UI layout service](HUD_LAYOUT.md) computes
documented static frame, base HUD and font metrics for later scene integration.
Round 3 adds the minimap, bust, flying-number, compass, help, notice, arena,
medal and net-camera arithmetic, plus the present-brightness formula, beside that service.
The play scene's inventory, shop, skills, stats and level-up screens call the existing item handlers; undocumented frame positions stay refusals.
The [front end](FRONTEND.md) (boot screens, main menu, character creation, options with the music slider, load/save and the game pause menu) is a native flow. Its documented draw calls (positions, frames, fonts, layers, tints, menu sound ids and the first-theme gate, research-menus-layout dc458d5) are painted with the user's own textures and fonts; class art, card rows, the intro picture and the credits layout stay a labelled host layout.

See the [browser guide](GODOT_BROWSER.md) for pinned dependencies, builds and
platform verification. Godot and godot-cpp are fetched locally, not vendored.

The initial private archive comparisons cover **Return to Arms only**. Norrath now has an
[experimental hash-pinned executable](https://github.com/Open-Champions/champions-of-norrath/blob/main/docs/BASELINE.md);
its retail revision and native format/behavior compatibility remain unverified here. Public tests
use authored synthetic bytes and do not certify either game's entire data set.

## Architecture and remaining work

| Layer | Responsibility | Evidence boundary |
|---|---|---|
| Portable C++ core (`native/`) | Validated archives, format decoders, simulation contracts and reproducible traces | RTA core, items, combat, script-machine and network-wire model ports have exact tests; archives, texture subsets and model forms decode independently of Godot. Engine integration remains work. |
| [Game profiles](GAME_PROFILES.md) (initial native registry) | Explicit title, region/revision, executable baseline fingerprint and bounded format eligibility; constants, script/ability/object contracts and save schema remain planned | Documented RTA US v2.00 identity; Norrath is unvalidated and refuses decoding. Existing callers still use title labels; registry adoption remains work. |
| Thin GDExtension (`gdextension/`) | Expose validated archive metadata and limited RTA texture pixels and model geometry to Godot | Copies results into owned Godot values; archive fingerprints reject stale preview selections. Native-owned Session commands and observations support the bounded movement test; the scene explicitly selects dry creature movement with authored controls and a 512-ready-tick cap; original player controls remain work. |
| Godot 4 presentation (`godot/`) | Archive selection, nested metadata, 32-bit/plain 4-bit/8-bit texture preview, static model inspection and status/error UI | Authored movement/health controls, flat-terrain presentation and private model selection run through native state; original controls, complete levels, materials and audio remain work. |

Select the title/profile explicitly until detection is verified. Use a profile's
supported revisions and content fingerprints to reject unknown inputs with a
useful error. Share a decoder or behavior only after both games' contracts agree;
keep revisions and differences explicit. A successful GOB listing is not a title
or revision detector. No Norrath executable hash or supported revision is invented.

The source siblings own original PS2 reconstruction:
[Return to Arms](https://github.com/Open-Champions/champions-return-to-arms) and
[Norrath](https://github.com/Open-Champions/champions-of-norrath).
Use their documented behavior to author portable implementations. Binary-matching
progress is measured there and must not be counted as native client completion.
Existing [format research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/main/docs/FORMATS.md)
and the [preserved native assessment](NATIVE_PORT.md)
remain research references; do not duplicate active area-agent investigations.

## Why Godot

[Open-BFME's client](https://github.com/Open-BFME/openbfme-godot) is a useful
precedent for separating a portable C++ core from the presentation engine and
from original-executable matching projects. It does not supply Snowblind assets,
formats, simulation or an automatic PS2 conversion.

Godot's [GDExtension interface](https://docs.godotengine.org/en/stable/engine_details/engine_api/gdextension/what_is_gdextension.html)
and [official C++ bindings](https://github.com/godotengine/godot-cpp) provide the
integration route. The initial browser pins Godot 4.5.2, its matching export templates,
and godot-cpp at its Godot 4.5 API tag. The archive library continues to build and
test without any Godot dependency.

Native client targets are Apple Silicon/Intel macOS and ARM64/x64 Windows.
Godot documents [macOS exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html)
and [Windows exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html);
matching extension binaries, renderer behavior and actual exported-app execution
still need per-platform verification. Inspector CI is not Godot export evidence.
Linux remains useful for portable-core tests and tooling; client support needs
its own launch and rendering evidence.

## Current development order

1. **Core game systems.** The [Python-contract C++ core](CORE_SIMULATION.md)
   now implements RTA arithmetic, math, RNGs, pool metadata and handle lists.
   Exact generated vectors and cross-platform state traces gate changes.
2. **Items, then combat.** The [item systems](ITEM_SYSTEMS.md) port records,
   statistics, generation, loot, shops and naming from `ref_items.py`.
   The [combat systems](COMBAT_SYSTEMS.md) now add damage, statistics and
   experience under their own pinned Python-generated exact vectors.
   The [shared Skill foundation](SKILLS_A.md) ports RTA skill statistics, range RNG,
   mana dispatch and Dodge/Regeneration numeric helpers; individual skill lifecycles remain work.
   The standalone [skills 69–101 subset](SKILLS_C.md) adds shared RTA
   statistics, mana/range rules and all six passive helpers; active skill
   casts, spell lifecycles and engine integration remain unsupported.
3. **Script machine, then network wire.** The [script machine](SCRIPT_MACHINE.md)
   ports `ref_amx.py` with exact state tests and private script validation.
   The standalone [script built-ins subset](SCRIPT_BUILTINS.md) now ports 44
   RTA quest/profile, campaign-state and player-record commands with complete synthetic state tests;
   the remaining 147 built-ins and scene integration remain unsupported.
   The [level-script command host](SCRIPT_COMMANDS.md) runs the documented
   binder and executed callees for monster values, stat scales, charm with an
   empty or live target in the host pool, fades, default- or options-volume
   music, stream-2 stop and the inferred boss-music bit of `setCFlag`. The
   five text setters run on the game string table ([STRINGS.md](STRINGS.md));
   unknown callee states refuse. The play session binds an offline seed stream, starts one
   player at difficulty 1 and uses the documented object factory with the
   owner-selected executable. The [item commands](SCRIPT_ITEMS.md) run their
   researched has, count, remove, give and generate handlers; gives need item
   tables and a bound seed; ground items and `givePlayerRandom` refuse at their
   documented evidence boundaries. The [item-round gaps](ITEMS_GAPS.md) cover the
   host bit, generators and bounded hide/show behavior on stubs. The
   arena monster builder is a separate library call.
   With the level script on, the real-data play scene attaches the
   [NPC dialog machine](DIALOG.md): the game folder's talkers can be talked
   to, and choice actions reach the quest and item models. The conversation
   camera swoop, the box word, the cursor glyph and the `dialog_isOpen` gates of
   `docs/spec/dialog-gaps.md` are ported (see [DIALOG.md](DIALOG.md)); openings
   whose speaker has no complete `dialogOffset` stay refused.
   The [string table and text commands](STRINGS.md) port `ref_strings.py`:
   `gtext.uni`, `wstr_format`, `setInfo`, the help box, `showBust` and
   `showCompass`, drawn over the real-data play view.
   The [network-wire rules](NETWORK_WIRE.md) now port `ref_netwire.py` with exact
   packet, peer state and callback tests. The [lockstep session](NETPLAY.md)
   runs that model for 2–4 peers on an in-process loopback and compares Full
   combat seed, health and position traces. The worlds run the offline co-op
   simulation carried by the lockstep transport, not the original net-game
   path. Internet datagrams, the lobby and rendezvous
   remain open. Engine commands and socket integration remain separate work.
   The standalone [game-message transport subset](GAME_MESSAGES.md) adds pinned
   registration, all sender encoders, ring writes and external/null delivery;
   inventory, HUD and shop handler execution remains explicitly unsupported.
4. **Complete documented model coverage.** [Model-form decoding](MODEL_FORMS.md)
   retains reflection, shell and facing-point data after the gameplay ports. Use optional
   [private-file coverage reports](PRIVATE_DATA_VALIDATION.md) to measure real
   asset acceptance. Do not compare sample occurrences with full-disc totals.
5. **Integrate a controllable scene.** Begin with the owned
   [session/replay host](SESSION_HOST.md), connecting ready network frames to
   explicitly authored tasks through the validated modules. The
   [object runtime](OBJECT_RUNTIME.md) ports the published object lifetime,
   scheduler and query contract. Its [Session adapter](SESSION_OBJECTS.md)
   connects ready frames to the owned World. The standalone
   [creature hit-point function](CREATURE_HP.md) ports damage-to-HP updates;
   [owned combat storage adapter](COMBAT_CREATURE.md) connects combat callbacks
   to the base HP body. A [shared handle adapter](RAW_HANDLES.md) connects
   the existing World pool to explicit creature-memory reads. The
   [Session health adapter](SESSION_HP.md) retains that state across ready
   scheduler frames. The [base combat integration](SESSION_COMBAT.md) connects
   restricted nonplayer damage to the existing shared random authority.
   The [supplied terrain kernel](WORLD_QUERIES.md) ports collision queries and
   the isolated-object movement subset. The [Session terrain adapter](SESSION_TERRAIN.md) connects supplied velocity to owned ready ticks under a strict single-actor domain. The [terrain-file loader](TERRAIN_FILE.md) projects explicitly selected RTA WORLD bytes into the same owned collision data. The [bounded controllable scene](SESSION_CONTROL.md) connects that single-actor domain to keyboard presentation with exact state replay. The standalone [creature collision step](CREATURE_MOVEMENT.md) ports original root motion, water bookkeeping, ordered contacts, gravity and settling through explicit callbacks. Its [dry singleton Session adapter](SESSION_CREATURE.md) now shares the existing World, Level and HP Memory under ready-tick ordering, with explicit authored dispatch storage and complete observations. The [scene's explicit DryCreature policy](SESSION_CONTROL_CREATURE.md) now uses this path with a 512-ready-tick cap while preserving the legacy isolated default and fixtures. Original player controls, water services and dynamic contacts remain unvalidated. The [static WORLD ground decoder](WORLD_GEOMETRY.md) now prepares actual geometry and collision from the same selected bytes, with atomic mesh/controller replacement and explicit water/moving-piece refusals. Drawn water follows the documented colour, slope UVs and layer-3 blend in [WATER_DRAW.md](WATER_DRAW.md); the claim-frame seed and the drawer splash point stay refused. Continue with supported game-system integration and richer terrain presentation,
   input, collision and saves. The [authored native save schema](NATIVE_SAVE.md)
   reconstructs the bounded Controller and its complete Session histories through
   checked replay; Godot save/load bindings and PS2 save import remain separate.
   The [PS2 memory-card import](PS2_SAVE_IMPORT.md) reads RTA US v2.00 slot and options images and loads one character into native script and item state; populated level bodies and the camera load stay refused.
   The standalone [animation clock](ANIMATION_CLOCK.md)
   now implements cache-free track lifecycle, timing and events from pinned Python
   contracts. The [Session animation adapter](SESSION_ANIMATION.md) connects owned
   timing and events to explicit ready tasks and health state; this authored order
   does not establish original Creature animation dispatch. Scene playback, root
   motion and sound remain separate work. The standalone [raw32 decoder](ANIMATION_DECODER.md)
   now covers clip parsing, owned decoder-slot lifetime and original joint
   decoding, including stale state and failure prefixes. The standalone
   [cache-bearing animation runtime](ANIMATION_RUNTIME.md) now connects lifecycle,
   timing and decoding through one canonical track-cell registry. Session/scene
   consumers, pose, sound and animation-generated root motion require separate
   integration evidence.
   The standalone [creature turning step](CREATURE_TURN.md) now preserves original
   spring/clamp/slow behavior and complete shared-memory writes. The explicit
   [Session turning adapter](SESSION_TURN.md) now supplies ready-task dispatch
   through existing HP/World identities and shared memory; controller and original
   player/AI dispatch still require separate integration evidence.
   The standalone [player tick subset](PLAYER_TICK.md) now ports original RTA
   entry and complete scripted-control ticks, plus ordered pad selection, fidget/
   block helpers and animation/skill lookups. Ordinary action ticks explicitly
   refuse after entry; the switch, tail, remaining helpers and scene integration
   remain work.
   The standalone [monster contract subset](MONSTER_SYSTEMS.md) now ports
   SmallSpider state/touch behavior, Mosquito/Scorpion part shedding and prefix
   comparison with pinned complete-state vectors. Constructors, run loops and
   the remaining monster states are explicitly unported.
   Compare native peers using exact state traces.
   Validate Norrath separately before sharing RTA behavior.

Viewer extension is paused. RTA byte-exact reconstruction is parked until a
period PS2 `ee-gcc` compiler is used, with function names from the research
repository's `names.tsv` and `symbols/`. Modern Clang probes are not continued.

Each milestone needs an executable result and recorded evidence. A blank window,
placeholder character or accepted archive table is not a completed game feature.

## Deterministic behavior and game differences

Specify integer widths/overflow, conversion rules, floating-point expectations,
random streams, stable iteration order and fixed simulation tick sequencing.
Keep input collection, wall-clock timers, audio, camera, loading speed and render
rate outside authoritative state. Game-specific scripts, object layouts, abilities,
content IDs, navigation, save formats and network behavior are not assumed shared.

Godot physics/navigation and scene multiplayer are useful facilities but are not
proven equivalents of Snowblind collision/pathfinding or the original network
protocol. An initial native save schema is also distinct from PS2 save import or
round-trip compatibility. Cross-platform trace comparison and behavior tests are
required even when both peers use a fixed update rate.

## Content and distribution

Runtime access will use a user-selected game-data folder; original assets stay
outside the source tree and exported packages. Public CI uses synthetic fixtures.
The inspector packages contain only the inspector, license, usage and build metadata.
Browser exports contain authored UI and the native extension, without original game data.
A future playable export must state the exact supported title/revision, operating
systems, remaining provisional behavior and independent verification results.

The standalone [arena monster subset](ARENA_MONSTERS.md) now ports RTA Skelight's
non-wander state transitions, shatter and base state filter with pinned complete
synthetic observations. Monster constructors/ticks, wander, Horsemen, Zek,
spells, level scripting and engine integration remain separate work.

The standalone [arena mode subset](ARENA_MODES.md) ports the documented RTA
versus and wave-arena task: scoring, timers, respawn placement, win/lose, and
the mode A damage cuts. Unpublished session and net fields, and bodies the
research did not execute, stay explicit refusals. It does not start a match
from the scene.
The standalone [RTA skills 35–68 statistics subset](SKILLS_B.md) now ports common
Skill statistics, all per-ID mana costs, statistic helpers for five passives and base
spell/random state with exact synthetic vectors. Active casts, spell lifecycle,
projectiles and effects remain unsupported; it has no Session or scene integration.

The opt-in [multi-actor combat runtime](COMBAT_RUNTIME.md) adds one shared-memory
RTA actor World, executed combat helpers and a ready-frame integration adapter.
It preserves explicit partial-path refusals. Session/Controller wiring, original
player movement/action flow and Godot combat presentation remain separate work.

The isolated [creature animation module](CREATURE_ANIMATION.md) adds original
raw32 pose/blend/root execution and the documented SmallSpider dispatch through
a new Godot-facing API. Original-asset rendered acceptance and additional class
dispatch remain separate work; the existing cosmetic scene preview is unchanged.

A follow-up [dispatch surface](CREATURE_ANIMATION.md#round-3-creature-tables-and-player-animation-dispatch)
now covers the recorded clip-selection contract for 47 root-motion, 10 steered
and 10 own-machine classes, documented player skill states 6, 9 and 0xe, and
event observations. State 7, the 6 no-dispatch classes and unrecorded conditions
still refuse. The standalone [RTA native audio model](NATIVE_AUDIO.md) now ports VAG/VAT parsing,
the PS2 ADPCM decoder with loop points and a deterministic tick-driven voice, stream
and music policy with synthetic vectors, plus a Godot generator bridge. Boss
override order and queue flags 0x40/0x100 follow the executed playback contract;
intro drift, the cutscene ±0.1 threshold and the 48 kHz labelling question stay
gated. The opt-in playable
integration now uses [synthetic audio banks](PLAYABLE_AUDIO.md) on ready ticks.

Its round-2 [Full composition](COMBAT_RUNTIME.md#round-2-the-full-composition)
ports the composed research runtime: movement, the real player tick, armed
melee, blocking and knock-downs, projectiles, Holy Strike, Shock of Fire, Wave
of Flame and the monster helpers, with exact synthetic fixtures. Session wiring
and Godot presentation remain separate work.
Its round-3 bodies add the disease cloud and the DiseaseEffect object it
attaches, the GutChunk constructor, the Strong Stance phantom, and arrow kinds
1, 2, 5 and 6. Potions, deflection, the bow release, the Holy Shield light,
silent block sparks and the remaining skill paths stay refused.
Round 4 adds thrown potions 1–3, the Holy Shield absorb stub and silent block sparks.
Round 5 follows the sample-point absorb, the processDamage and silent-0 block-spark angles, AntQueen states 0 and 1, Skeleton states 0 and 1, Soul's demon gate and MultiFireProjectile_run. PlanarSteel, HornOmen, SpellArena, Pustule, SmokeGen and DiseaseCorpse stay unexecuted.
Round 6 executes poison gas, disease corpses, the multi-fire hit and end, SmokeGen, Pustule, SpellArena, and the HornOmen and PlanarSteel constructors and runs. Their destructors are not in the research and refuse.
combat-ctors round 2 (research-combat-ctors-2 5b92837) runs AntQueen_run in every state with pickFlyDest, Skeleton_isEnemy, the take-damage slots, the AntQueen/Skeleton/Soul/Fire/LooseFire/poison-gas destructors and the fire entry of a touch; the AntQueen is back in the Full fight (slot 4). Skeleton_run past states 0 and 1, allyInLineOfFire, onNearbyEvent, GutChunk_ctor and the death path stay refused.

The mounted Horsemen ([HORSEMEN_MOUNT.md](HORSEMEN_MOUNT.md), research `t3/research-horsemen-mount` 338d459) are ported: the Nightmare mount's constructor, set-state rider table, run, contact slots and charge picker replay 1,101 research-model cases exactly. Level load builds all four Rider variants with their observed named MiniBoss mounts and reciprocal handles, as well as a placed Nightmare. The play scene does not yet tick the pair; its run-list order remains a host decision.
