STATUS: IN_PROGRESS

PD-1: Attached-dialog trigger callee preflight now throws RefusedResult regardless
of result use, increments refusal/stopped counters, and preserves whole-set
preflight. Other command policies are unchanged. Statement-form regressions
require Stop::Refused and no campaign mutation for mixed targets, unknown/special
level domains, and cutscene NPCs. Synthetic script values are authored fixtures.

Evidence: research-dialog@334578d docs/dialog.md:92-93 establishes the base route;
docs/monsters-2.md:4304-4310 establishes NPC overrides; docs/spec/player-tick.md:75
pins the existing cutscene word. No target conditions or gameplay values changed.
Unconditional entry termination implements the audit's host refusal requirement.

PD-2: This is the intended work report; docs/PLAY_DIALOG.md now links it and
separates historical gates from this revision's validation.

Gates: pending Release build, focused native tests, full CTest, dialog Godot tests,
and FULL_TRACE comparison. Existing integration/journey limits remain unchanged.
