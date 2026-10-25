#!/usr/bin/env python3
"""Port the executed script-effect cases from research tools/ref_script_effects.py.

The research difftest compares those cases with the retail executable. This
generator does not open that executable. It runs the same Python bodies on a
fresh memory image and writes the stores, the return and the calls. Addresses
are fixed synthetic fixtures so the C++ test can replay them. No retail
strings.

Each pinned research file is read with `git show 52984db:<path>` into a
temporary directory and must hash to its pin; nothing is imported from the
research working tree. A manifest with the pins is written beside the TSV.

    python3 tools/generate_script_effects_vectors.py \
        --research-dir <research-checkouts>/research-script \
        --out native/tests/vectors/script_effects.tsv \
        --manifest native/tests/vectors/script_effects.json
"""
import argparse
import hashlib
import importlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = "52984db"
PINS = {
    "tools/ref_script_effects.py": "d7e6cc778279a917629650672370f0a4efa9ce1e2678cc67c01e8433f19510b6",
    "tools/ref_natives.py": "5ba711f93299a6cdb684651ae5a3dda5c0ddc5370baeea3a54ff123f1cc44839",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
    "tools/ref_objects.py": "541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2",
    "tools/ref_amx.py": "091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f",
}
DOC_PINS = {
    "docs/script-effects.md": "f633f3df2e3e7c6336b091d86c199d68d1b1cdec49c10e85d9e2d545f694f3f5",
}
C = N = E = None


def git_show(research, path):
    return subprocess.check_output(["git", "-C", str(research), "show", "%s:%s" % (COMMIT, path)])


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stage(research, into):
    for path, want in list(PINS.items()) + list(DOC_PINS.items()):
        blob = git_show(research, path)
        got = sha(blob)
        if got != want:
            raise SystemExit("%s at %s hashed %s, pin is %s" % (path, COMMIT, got, want))
        if path in PINS:
            (into / Path(path).name).write_bytes(blob)


CREATURE = 0x200000
TEXT = 0x210000
TITLE = 0x210100
BODY = 0x210200
TEXTURE = 0x210300
NAME = 0x210400
CAMERA = 0x220000


def f32(value):
    return C.f32(value) & E.M32


class Case:
    def __init__(self, label):
        self.label = label
        self.mem = N.Mem()
        self.trace = E.Trace()
        self.lines = ["case " + label]
        self.mem.writes.clear()

    def pre(self, addr, size, value):
        self.mem.store(addr, size, value)
        self.lines.append("pre %x %d %x" % (addr, size, value & ((1 << (8 * size)) - 1)))

    def creature(self, flags=0x20):
        self.pre(CREATURE + 0xC, 4, flags)
        return CREATURE

    def finish(self, op, result=None):
        self.lines.append(op)
        if result is not None:
            self.lines.append("ret %x" % (result & E.M32))
        elif result is False:
            self.lines.append("ret none")
        for addr, size in self.mem.writes:
            self.lines.append("post %x %d %x" % (addr, size, self.mem.load(addr, size)))
        for name, args, fargs in self.trace.calls:
            text = "call " + name
            if args:
                text += " " + ",".join("%x" % (a & E.M32) for a in args)
            if fargs:
                text += " / " + ",".join("%x" % (a & E.M32) for a in fargs)
            self.lines.append(text)
        self.lines.append("end")
        return self


def build():
    rows = []

    def add(case):
        rows.extend(case.lines)

    specs = (
        ((100.0, 200.0, 10, 4, 6, 8, 3.5), (0.5, 2.0, 0.25, 1.5, 0.0), "scale-half"),
        ((100.5, 1.0, 1, 1, 3, 1, 2.0), (0.5, 0.1, 0.1, -1.0, 0.25), "scale-trunc"),
    )
    for (hp, mx, arm, d0, d2, d4, xp), scales, label in specs:
        case = Case(label)
        case.creature()
        case.pre(CREATURE + 0x228, 4, f32(hp))
        case.pre(CREATURE + 0x22C, 4, f32(mx))
        case.pre(CREATURE + 0xE4, 2, arm)
        case.pre(CREATURE + 0xD0, 2, d0)
        case.pre(CREATURE + 0xD2, 2, d2)
        case.pre(CREATURE + 0xD4, 2, d4 & 0xFFFF)
        case.pre(CREATURE + 0xD8, 4, f32(xp))
        case.mem.writes.clear()
        patterns = tuple(f32(s) for s in scales)
        result = E.scale_stats(case.mem, CREATURE, patterns)
        case.finish("op scale %x %s" % (CREATURE, " ".join("%x" % p for p in patterns)), result)
        add(case)

    def charm(label, flags, timer, frames, team, target=None):
        case = Case(label)
        case.creature(flags)
        case.pre(CREATURE + 0x312, 2, timer & 0xFFFF)
        if target is not None:
            case.pre(CREATURE + 0x234, 4, target)
        case.trace.returns["handle_clear"] = 7
        case.lines.append("stub handle_clear 7")
        case.mem.writes.clear()
        result = E.set_charm(case.mem, case.trace, CREATURE, frames & E.M32, team)
        case.finish("op charm %x %x %x" % (CREATURE, frames & E.M32, team), result)
        add(case)

    charm("charm-permanent", 0x20, 0xFFFF, 5, 1)
    charm("charm-player", 0x60, 3, 5, 1)
    charm("charm-plain", 0, 3, 5, 1)
    charm("charm-ok", 0x20, 10, 0xFFFFFFFF, 3, 0x44)

    case = Case("slot34")
    case.creature()
    case.mem.writes.clear()
    E.slot34_set_home_range(case.mem, CREATURE, 0x1ABCD)
    case.finish("op slot34 %x 1abcd" % CREATURE, None)
    add(case)

    case = Case("slot27")
    case.creature()
    case.mem.writes.clear()
    E.slot27_set_state(case.mem, CREATURE, 0x12345)
    case.finish("op slot27 %x 12345" % CREATURE, None)
    add(case)

    def message(label, menu, slot, force, frames, local):
        case = Case(label)
        case.pre(E.G_LOCAL_MENU, 1, menu)
        case.trace.returns["net_isLocalSlot"] = local
        case.lines.append("stub net_isLocalSlot %x" % local)
        case.mem.writes.clear()
        result = E.show_message(case.mem, case.trace, TEXT, frames, slot & E.M32, force)
        case.finish("op message %x %x %x %x" % (TEXT, frames, slot & E.M32, force), result)
        add(case)

    message("message-open", 0, -1, 0, 180, 1)
    message("message-menu", 1, -1, 0, 180, 1)
    message("message-remote", 0, 2, 1, 90, 0)
    message("message-force", 1, 2, 1, 90, 1)

    def help_case(label, slot, network, alive, local, kind):
        case = Case(label)
        case.pre(N.G_NETWORK_MODE, 1, network)
        case.pre(N.G_ALIVE_COUNT, 4, len(alive))
        for index, live in enumerate(alive):
            case.pre(N.G_ALIVE_LIST + 4 * index, 4, live)
        case.trace.returns["res_find"] = 0x55
        case.trace.returns["sound_playOnObject"] = 0x8001
        case.lines.append("stub res_find 55")
        case.lines.append("stub sound_playOnObject 8001")
        if kind == "const":
            case.trace.returns["net_isLocalSlot"] = local
            case.lines.append("stub net_isLocalSlot %x" % local)
        else:
            case.trace.returns["net_isLocalSlot"] = lambda slot, *_rest: 1 if slot == 0 else 0
            case.lines.append("local eq0")
        case.mem.writes.clear()
        result = E.show_help(case.mem, case.trace, slot & E.M32, TITLE, BODY)
        case.finish("op help %x %x %x" % (slot & E.M32, TITLE, BODY), result)
        add(case)

    help_case("help-offline", -1, 0, [], 1, "const")
    help_case("help-remote", 1, 0, [], 0, "const")
    help_case("help-net", -1, 1, [1, 0], None, "eq0")
    help_case("help-net-none", -1, 1, [1], None, "eq0")

    case = Case("fade-off")
    case.mem.writes.clear()
    E.set_fade(case.mem, 0)
    case.finish("op fade 0", None)
    add(case)
    case = Case("fade-on")
    case.mem.writes.clear()
    result = E.set_fade(case.mem, 1)
    case.finish("op fade 1", result)
    add(case)

    def step(label, current, delta):
        case = Case(label)
        case.pre(0x4F66AC, 4, current & E.M32)
        case.mem.writes.clear()
        result = E.step_fade(case.mem, case.trace, delta & E.M32)
        case.finish("op step %x" % (delta & E.M32), result)
        add(case)

    step("fade-clamp", 40, 100)
    step("fade-down", 128, -16)
    step("fade-zero", 5, -10)

    def open_hud(case):
        case.pre(E.G_CAMERA, 4, CAMERA)
        case.pre(CAMERA + 0x34, 4, 1)
        case.pre(E.G_HUD_SUPPRESSED, 1, 0)
        case.pre(E.G_LOCAL_MENU, 1, 0)
        case.pre(E.G_LOADSCREEN, 1, 0)
        case.pre(N.G_MENU_OPEN_FLAG, 4, 0)

    case = Case("bust-suppressed")
    open_hud(case)
    case.pre(E.G_HUD_SUPPRESSED, 1, 1)
    case.mem.writes.clear()
    result = E.set_bust(case.mem, case.trace, TEXT, TEXT, 3, 4)
    case.finish("op bust %x %x 3 4" % (TEXT, TEXT), result)
    add(case)

    case = Case("compass-mode")
    open_hud(case)
    case.pre(CAMERA + 0x34, 4, 0)
    case.pre(E.G_COMPASS_POS + 12, 4, 0x99)
    case.lines.append("keep %x 4 99" % (E.G_COMPASS_POS + 12))
    case.mem.writes.clear()
    result = E.set_compass(case.mem, case.trace, (f32(9.0), f32(8.0), f32(7.0), 0x99), TEXT)
    case.finish("op compass %x %x %x %x" % (f32(9.0), f32(8.0), f32(7.0), TEXT), result)
    add(case)

    case = Case("bust-load")
    open_hud(case)
    case.pre(E.G_LOADSCREEN, 1, 1)
    case.mem.writes.clear()
    result = E.set_bust(case.mem, case.trace, TEXT, TEXT, 3, 4)
    case.finish("op bust %x %x 3 4" % (TEXT, TEXT), result)
    add(case)

    case = Case("compass-menu")
    open_hud(case)
    case.pre(N.G_MENU_OPEN_FLAG, 4, 5)
    case.mem.writes.clear()
    result = E.set_compass(case.mem, case.trace, (0, 0, 0, 0), TEXT)
    case.finish("op compass 0 0 0 %x" % TEXT, result)
    add(case)

    case = Case("compass-open")
    open_hud(case)
    case.pre(E.G_COMPASS_POS + 12, 4, 0x99)
    case.lines.append("keep %x 4 99" % (E.G_COMPASS_POS + 12))
    case.mem.writes.clear()
    words = (f32(9.0), f32(8.0), f32(7.0))
    result = E.set_compass(case.mem, case.trace, words + (0x99,), TEXT)
    case.finish("op compass %x %x %x %x" % (words[0], words[1], words[2], TEXT), result)
    add(case)

    case = Case("bust-open")
    open_hud(case)
    case.mem.writes.clear()
    result = E.set_bust(case.mem, case.trace, TEXTURE, TEXT, 3, 4)
    case.finish("op bust %x %x 3 4" % (TEXTURE, TEXT), result)
    add(case)

    for handle, label in ((0x8003, "frozen-slot"), (0xFFFFFFFF, "frozen-none"), (3, "frozen-clear")):
        case = Case(label)
        case.mem.writes.clear()
        result = E.set_plays_while_frozen(case.mem, handle)
        case.finish("op frozen %x" % handle, result)
        add(case)

    for count, label in ((0, "queue-empty"), (1, "queue-one"), (8, "queue-full")):
        case = Case(label)
        count_at, _stride = E.stream_count_addr(2)
        case.pre(count_at, 4, count)
        case.mem.writes.clear()
        result = E.queue_stop(case.mem, 2)
        case.finish("op queue 2", result)
        add(case)

    for volume, scalar, label, open_gate in (
            (0, 1.0, "music-shut", False),
            (E.MUSIC_GATE, 1.0, "music-edge", True),
            (f32(1.0), 0.5, "music-half", True)):
        case = Case(label)
        case.pre(E.G_MUSIC_VOLUME, 4, volume)
        case.pre(E.G_MUSIC_SCALAR, 4, f32(scalar))
        case.mem.writes.clear()
        result = E.music_play_level(case.mem, case.trace, NAME, 3)
        expect = 0 if open_gate else 0x500000
        if result != expect:
            raise SystemExit("%s model returned %x, expected %x" % (label, result, expect))
        case.finish("op music %x 3" % NAME, result)
        add(case)

    return rows


def main():
    global C, N, E
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--research-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        stage(args.research_dir, Path(temp))
        sys.path.insert(0, temp)
        C = importlib.import_module("ref_core")
        N = importlib.import_module("ref_natives")
        E = importlib.import_module("ref_script_effects")
        rows = build()
    cases = sum(1 for line in rows if line.startswith("case "))
    lines = ["# Generated by tools/generate_script_effects_vectors.py. Do not edit by hand.",
             "# Research %s ref_script_effects.py; synthetic fixtures only." % COMMIT]
    for path, digest in sorted(list(PINS.items()) + list(DOC_PINS.items())):
        lines.append("# %s sha256 %s" % (path, digest))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines + rows) + "\n", encoding="ascii")
    manifest = {
        "schema": "rta-script-effects-v1",
        "reference_commit": COMMIT,
        "pins": dict(sorted(list(PINS.items()) + list(DOC_PINS.items()))),
        "generator_sha256": sha(Path(__file__).resolve().read_bytes()),
        "vectors_sha256": sha(args.out.read_bytes()),
        "cases": cases,
    }
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("cases %d" % cases)
    return 0


if __name__ == "__main__":
    sys.exit(main())
