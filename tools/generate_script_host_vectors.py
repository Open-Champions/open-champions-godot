#!/usr/bin/env python3
"""Level-script host vectors from research t3/research-script 9649e6b.

Runs the pinned tools/ref_natives.py (Game, its Host, the ref_objects World)
and tools/ref_amx.py on synthetic scripts and synthetic objects and writes
what the reference produces to native/tests/vectors/script_host.txt. The C++
test replays every case through script_commands::Host. Expected values are
never taken from native output.

No disc, executable or script image is read. Each pinned file is read with
`git show 9649e6b:<path>` into a temporary directory and must hash to its
pin; nothing is imported from the research working tree.

The level entry is composed in the order the research documents:
docs/script.md 2.1 (level_loadScriptAndObjects runs preStart, loads the
objects, runs start), docs/issues/arena-entry.md 1.4 (script_runPostAction
0x1e60b8, then the deferred queue promoted once at 0x322e6c) and
docs/mainloop.md step 7 (the deferred calls run once outside any tick). A
ready tick is one script_runDeferred, the Script task (docs/script.md 2.4).

    python3 tools/generate_script_host_vectors.py \
        --research-dir <research-checkouts>/research-script \
        --out native/tests/vectors/script_host.txt
"""
import argparse
import hashlib
import importlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = "9649e6b"
PINS = {
    "tools/ref_natives.py": "5ba711f93299a6cdb684651ae5a3dda5c0ddc5370baeea3a54ff123f1cc44839",
    "tools/ref_amx.py": "091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f",
    "tools/ref_objects.py": "541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
}
DOC_PINS = {
    "docs/script.md": "572e4f5722797b8009f0e628fbc7f27fb117956a6d040c06e03f9fc21a70e6f6",
    "docs/issues/arena-entry.md": "4012ff9bf4dd08f55391b45fcf249f0f9e02f99ff0d20760e47bd8b164ba86ce",
    "docs/mainloop.md": "284140cc3a60cb3c100aa2dde8d08797123d9dce12ae570f499ec8d3ff34d288",
    "docs/script-composition.md": "f5c4847afc8193b871e8869528025b712a963e2d9eef6c3455f429c8495454ba",
}
DATA_CELLS = 640
STACK_CELLS = 256

# Synthetic cases. Strings sit unpacked in the data block at the given data
# addresses (one cell per byte, then a zero cell, as scr_setString stores an
# unpacked string). A call is (native, args). An entry ending "keep" returns
# PRI as its last call left it; "zero" ends ZERO.pri, RETN, as a Pawn
# function without a return value does. Objects are constructed with known
# flags and added between preStart and start; flag 0x20 is the creature bit
# (ref_natives.OF_CREATURE). seed None means no session seed is bound.
CASES = [
    {
        "name": "seed_draw",
        "seed": 20973,
        "strings": {2000: "quest"},
        "publics": [
            ("start", "keep", [("setv", [2000, 7]), ("getv", [2000]), ("getRand", [])]),
        ],
        "objects": [],
        "ticks": 0,
        "quests": ["quest"],
    },
    {
        "name": "hide_show",
        "seed": None,
        "strings": {2000: "gate", 2100: "door"},
        "publics": [
            ("start", "zero", [("hideMonster", [2000]), ("hideMonster", [2100]),
                               ("showMonster", [2100])]),
        ],
        "objects": [("gate", 0x20, [0x41200000, 0x41A00000, 0], 0x100),
                    ("door", 0x0, [0, 0, 0x42C80000], 0x200)],
        "ticks": 0,
        "quests": [],
    },
    {
        "name": "particle",
        "seed": None,
        "strings": {2200: "gate"},
        "publics": [
            ("start", "zero", [("addParticleOnObject", [0, 2200, 1, 2, 3, 1000, -500, 250]),
                               ("addParticleOnObject", [-1, 2200, 0, 0, 0, 7, 0, 0]),
                               ("addParticleOnObject", [2, 2200, -4, 0, 9, 0, 3000, 1])]),
        ],
        "objects": [("gate", 0x0, [0x41200000, 0xC1A00000, 0x3F800000], 0)],
        "ticks": 0,
        "quests": [],
    },
    {
        "name": "entrance",
        "seed": None,
        "strings": {2000: "north"},
        "publics": [("start", "zero", [("setEntrance", [2000])])],
        "objects": [],
        "ticks": 0,
        "quests": [],
    },
    {
        # start queues "later", later queues "last". The load-time
        # script_runDeferred promotes "later", so it runs on tick 1.
        "name": "deferred_chain",
        "seed": None,
        "strings": {2000: "later", 2100: "last", 2200: "a", 2300: "b", 2400: "c"},
        "publics": [
            ("start", "zero", [("setv", [2200, 1]), ("addDeferedCall", [2000])]),
            ("later", "zero", [("setv", [2300, 1]), ("addDeferedCall", [2100])]),
            ("last", "zero", [("setv", [2400, 1])]),
        ],
        "objects": [],
        "ticks": 3,
        "quests": ["a", "b", "c"],
    },
    {
        # preStart runs before the objects are loaded: its hideMonster finds
        # nothing. start runs after them and finds its object.
        "name": "prestart_before_objects",
        "seed": None,
        "strings": {2000: "gate", 2100: "door", 2200: "p"},
        "publics": [
            ("preStart", "zero", [("hideMonster", [2000]), ("setv", [2200, 1])]),
            ("start", "zero", [("hideMonster", [2100])]),
        ],
        "objects": [("gate", 0x0, [0, 0, 0], 0),
                    ("door", 0x0, [0x40000000, 0, 0], 0)],
        "ticks": 0,
        "quests": ["p"],
    },
    {
        # The post action start sets runs at the end of the load, before the
        # deferred promotion; the name it queues runs on tick 1.
        "name": "post_action",
        "seed": None,
        "strings": {2000: "after", 2100: "tail", 2200: "x", 2300: "y"},
        "publics": [
            ("start", "zero", [("postAction", [2000])]),
            ("after", "zero", [("setv", [2200, 1]), ("addDeferedCall", [2100])]),
            ("tail", "zero", [("setv", [2300, 1])]),
        ],
        "objects": [],
        "ticks": 2,
        "quests": ["x", "y"],
    },
    {
        # One addv per Script task: n counts the ready ticks after the load.
        # godot/tests/script_commands_smoke.gd plays this image.
        "name": "tick_counter",
        "seed": None,
        "strings": {2000: "tick", 2100: "n"},
        "publics": [
            ("start", "zero", [("addDeferedCall", [2000])]),
            ("tick", "zero", [("addv", [2100, 1]), ("addDeferedCall", [2000])]),
        ],
        "objects": [],
        "ticks": 5,
        "quests": ["n"],
    },
]


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


def word(value):
    return "%08x" % (value & 0xFFFFFFFF)


def data_block(strings):
    cells = [0] * DATA_CELLS
    for address, text in strings.items():
        if address % 4 or address // 4 + len(text) + 1 > DATA_CELLS:
            raise SystemExit("string outside the data block")
        for index, char in enumerate(text.encode()):
            cells[address // 4 + index] = char - 0x100 if char & 0x80 else char
        cells[address // 4 + len(text)] = 0
    return b"".join(struct.pack("<i", value) for value in cells)


def image_of(A, case):
    natives = []
    lines = []
    for name, ending, calls in case["publics"]:
        lines.append(name + ":")
        lines.append(("PROC",))
        for native, args in calls:
            if native not in natives:
                natives.append(native)
            for value in reversed(args):
                lines.append(("PUSH.C", value))
            lines.append(("PUSH.C", 4 * len(args)))
            lines.append(("SYSREQ.C", natives.index(native)))
            lines.append(("STACK", 4 * (len(args) + 1)))
        if ending == "zero":
            lines.append(("ZERO.pri",))
        lines.append(("RETN",))
    cells, labels = A.assemble(lines)
    return A.build_image(cells, data=data_block(case["strings"]),
                         publics=[(name, labels[name]) for name, _e, _c in case["publics"]],
                         natives=natives, stack_cells=STACK_CELLS)


def queue(N, game):
    return "%d %d %d" % (game.mem.u32(N.G_DEFER_COUNT), game.mem.u32(N.G_DEFER_QUEUED),
                         game.mem.u32(N.G_DEFER_FLIP))


def quests(game, names):
    return " ".join("%s=%d" % (n, N_s32(game.var_get(n.encode()))) for n in names)


def N_s32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def run_case(A, N, case):
    game = N.Game()
    if case["seed"] is not None:
        game.seed = case["seed"]
    image = image_of(A, case)
    machine = A.Machine(image)
    if machine.load() != 0:
        raise SystemExit("synthetic image did not load")
    game.set_script(machine)
    out = ["CASE " + case["name"],
           "SEED " + ("-" if case["seed"] is None else word(case["seed"])),
           "IMAGE " + image.hex()]
    for name, flags, pos, angle in case["objects"]:
        out.append("OBJECT %s %s %s %s %s %s" % (name, word(flags), word(pos[0]), word(pos[1]),
                                                 word(pos[2]), word(angle)))
    out.append("TICKS %d" % case["ticks"])
    out.append("QUESTNAMES " + " ".join(case["quests"]))
    defined = {name for name, _e, _c in case["publics"]}
    results = []
    if "preStart" in defined:
        results.append(game.run_script(b"preStart"))
    objects = []
    for name, flags, pos, angle in case["objects"]:
        objects.append((name, game.new_object(names=[name.encode()], flags=flags, pos=pos,
                                              angle=angle)))
    if "start" in defined:
        results.append(game.run_script(b"start"))
    game.run_post_action()
    game.run_deferred()
    out.append("ENTRY " + " ".join(word(r) for r in results))
    out.append("AT 0 " + queue(N, game) + " " + quests(game, case["quests"]))
    for tick in range(1, case["ticks"] + 1):
        game.run_deferred()
        out.append(("AT %d %s %s" % (tick, queue(N, game), quests(game, case["quests"]))).rstrip())
    out.append("SEED_OUT " + word(game.seed))
    out.append("DRAWS %d" % game.draws)
    for name, obj in objects:
        out.append("OBJ %s %s %s %d" % (name, word(obj.flags), word(obj.u(N.C_HIDDEN_BYTE, 1)),
                                        1 if game.world.is_off_grid(obj) else 0))
    entrance = game.mem.cstr(N.G_ENTRANCE_NAME).decode()
    out.append("ENTRANCE " + (entrance if entrance else "-"))
    for name, args in game.host.log:
        if name == "particle_spawn":
            definition, pos, vel, backdate = args
            out.append("SPAWN %s %s %s %s" % (word(definition), " ".join(word(v) for v in pos),
                                              " ".join(word(v) for v in vel), word(backdate)))
        elif name not in ("obj_moveOffGrid", "obj_updateGridCell"):
            raise SystemExit("unexpected call-out %s" % name)
    out.append("END")
    return [line.rstrip() for line in out]


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--research-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        stage(args.research_dir, Path(temp))
        sys.path.insert(0, temp)
        A = importlib.import_module("ref_amx")
        N = importlib.import_module("ref_natives")
        lines = ["# Generated by tools/generate_script_host_vectors.py. Do not edit by hand.",
                 "# Research %s; synthetic scripts and objects only." % COMMIT]
        for path, digest in sorted(list(PINS.items()) + list(DOC_PINS.items())):
            lines.append("# %s sha256 %s" % (path, digest))
        for case in CASES:
            lines.extend(run_case(A, N, case))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("cases %d" % len(CASES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
