#!/usr/bin/env python3
"""Built level creatures from research t3/research-script-session 6841b8a.

Runs the pinned reference constructors (tools/ref_monsters_1.py on ref_arena_monsters,
ref_creature and ref_core; tools/ref_creature_init.py) on authored level records the way
docs/script-session.md section 5 composes them, and writes what they produce to
native/tests/vectors/level_objects.tsv: the authored image and factory strings, the
records, and per record either the object's bytes (without the fields kNotCopied leaves
out) and the shared seed afterwards, or "stub" when the composition
does not build it. The C++ test adopts the same records into script_commands::Host and
compares. Expected values are never taken from native output.

The call-outs are answered as docs/script-session.md section 5 says the host answers them:
GameObject_ctor (research docs/spec/object-system.md 2.2, one shared draw), the property
getters (lowered key, '=', libc atoi), balance_lookup (none found), Creature_setStats (one
own draw), Creature_init (ref_creature_init, the model getter answering 0), handle_clear on
an empty list, and the animation, run-list, grid and hide-mask calls (no effect on the
object bytes compared).

No disc or executable is read: the image is authored. Each pinned file is read with
`git show 6841b8a:<path>` and must hash to its pin.

    python3 tools/generate_level_objects_vectors.py \\
        --research-dir <research-checkouts>/research-script-session
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
COMMIT = "6841b8a"
PINS = {
    "tools/ref_monsters_1.py": "33a76a127c66ffd0b4c8c91c014d250c91c567cd168c2bc8870d5abae647c4d0",
    "tools/ref_arena_monsters.py": "84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16",
    "tools/ref_creature.py": "debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
    "tools/ref_creature_init.py": "6ca358ce821aa653288bf11e6b0f7ad5617a4a6cae21f12eff7dfe73178d607e",
    "tools/ref_level_factory.py": "f6eae7a3d4440a30b61cf4e6ac4a9f6a5bedec0468619e8c70ad212274968895",
}
DOC_PINS = {
    "docs/script-session.md": "16699f2f6e1fe013b85fc6e1310c47c8115530e416c506428a5770159bd4b47e",
    "docs/spec/object-system.md": None,
}
ROOT = Path(__file__).resolve().parents[1]
M32 = 0xFFFFFFFF
SEED0 = 20973
# The rows of the five ported classes (native/src/level_factory_table.inc).
ROWS = {"smallspider": 0x532D88, "gnaw": 0x532B88, "mosquito": 0x532C60,
        "scorpion": 0x532D10, "snobadge": 0x532A30, "badger": 0x532A40}
SIZES = {"smallspider": 0x3C8, "gnaw": 0x3C8, "mosquito": 0x3C8, "scorpion": 0x3D0,
         "snobadge": 0x3D0, "badger": 0x3D0}
# kNotCopied (native/src/script_commands_objects.cpp).
NOT_COPIED = [(0x2C, 4), (0x34, 4), (0x368, 4), (0x228, 4), (0x22C, 4), (0xD0, 2), (0xD2, 2),
              (0xD4, 2), (0xD8, 4), (0xE4, 2), (0x20, 4), (0x29, 1), (0x220, 8), (0x230, 1)]
SERIAL = 0x4F85C4


def show(research, path):
    return subprocess.run(["git", "-C", str(research), "show", "%s:%s" % (COMMIT, path)],
                          check=True, capture_output=True).stdout


def load(research):
    tmp = Path(tempfile.mkdtemp())
    for path, pin in PINS.items():
        data = show(research, path)
        got = hashlib.sha256(data).hexdigest()
        if got != pin:
            raise SystemExit("%s at %s hashes to %s, not the pin %s" % (path, COMMIT, got, pin))
        (tmp / Path(path).name).write_bytes(data)
    doc = show(research, "docs/script-session.md")
    if hashlib.sha256(doc).hexdigest() != DOC_PINS["docs/script-session.md"]:
        raise SystemExit("docs/script-session.md does not match its pin")
    sys.path.insert(0, str(tmp))
    return {n: importlib.import_module(n) for n in
            ("ref_core", "ref_creature", "ref_arena_monsters", "ref_monsters_1", "ref_creature_init",
             "ref_level_factory")}


class Image:
    """The authored executable: a few strings and constants, an ASCII class table."""

    def __init__(self):
        self.bytes = {}

    def put(self, addr, data):
        for i, b in enumerate(data):
            self.bytes[addr + i] = b

    def load(self, addr, size):
        v = 0
        for i in range(size):
            b = self.bytes.get(addr + i)
            if b is None:
                b = self.ctype_byte(addr + i)
            v |= b << (8 * i)
        return v

    @staticmethod
    def ctype_byte(addr):
        c = addr - 0x5628E1
        if -128 <= c <= 127:
            return (1 if 65 <= c <= 90 else 0) | (2 if 97 <= c <= 122 else 0) | \
                   (8 if c in (9, 10, 11, 12, 13, 32) else 0)
        return 0

    def string(self, addr):
        out = bytearray()
        while True:
            b = self.load(addr + len(out), 1)
            if b == 0:
                return bytes(out)
            out.append(b)

    def ctype(self, index):
        return self.ctype_byte(0x5628E1 + index)


def authored_image(F, CI):
    img = Image()
    for k, row in enumerate(F.TESTS):
        img.put(row[1], b"Cls%dName\0" % k)
    names = {"smallspider": b"Spider", "gnaw": b"Gnaw", "mosquito": b"Mosq", "scorpion": b"Scorp",
             "snobadge": b"SnoBadge", "badger": b"Badger"}
    for key, addr in ROWS.items():
        img.put(addr, names[key] + b"\0")
    img.put(0x549AF0, b"MosqBig\0")                       # the large-mosquito prefix
    img.put(0x545EA0, struct.pack("<d", 0.25))             # Gnaw's two scale doubles
    img.put(0x545EA8, struct.pack("<d", 0.5))
    return img, names


KEYS = {
    0x52F9F0: b"wait", 0x52F9F8: b"hide", 0x52FA00: b"showname", 0x52FA10: b"dead",
    0x52FA18: b"homerange", 0x52FA28: b"pointblankrange", 0x52FA38: b"visualrange",
    0x52FA48: b"visualarc", 0x52FA58: b"scale", 0x52FA60: b"miniboss", 0x52FA70: b"ignoredamage",
    0x52FA80: b"light1", 0x52FA88: b"light2", 0x52FA90: b"ambient",
    0x52FAC8: b"dead", 0x52FAD0: b"change%d", 0x52FB58: b"initScript",
    0x52FAE0: b"FireWeapons", 0x52FAF0: b"FireBurstWeapons", 0x52FB08: b"ColdWeapons",
    0x52FB18: b"PoisonWeapons", 0x52FB28: b"ShockWeapons", 0x52FB38: b"DiseaseWeapons",
    0x52FB48: b"NoMagicWeapons",
}
# The authored key texts are not the game's: they only have to agree between the
# reference run and the C++ test, which reads them from the same table.

PROP_SETS = [
    [], [b"name=alpha"], [b"name=\"quoted\""], [b"wait=1"], [b"hide=1", b"name=h"],
    [b"showname=Bob"], [b"homerange=250"], [b"pointblankrange=90", b"visualrange=700"],
    [b"visualarc=1000"], [b"scale=150"], [b"ignoredamage=1"], [b"light1=1"], [b"ambient=x"],
    [b"change3=1", b"change0=1"], [b"Wait=1"], [b"wait=0"], [b"wait= 7"],
    [b"miniboss=1"], [b"dead=1"], [b"initScript=go"], [b"initscript=go"], [b"FireWeapons=1"],
    [b"fireweapons=1"], [b"change31=x", b"name=two", b"name=three"],
]


class Strict(dict):
    def get(self, key, default=None):
        if key not in self:
            raise KeyError("unanswered call-out " + key)
        return self[key]


class Refused(Exception):
    pass


def run_case(R, img, cls, name, pos, facing, props, seed):
    """One record through the reference composition.  Returns (bytes, seed) or the
    refusal text.  The creature serial +0x230 is not compared: the records before it that
    the host does not build would have taken serials in the original."""
    M1, CI, F, C = R["ref_monsters_1"], R["ref_creature_init"], R["ref_level_factory"], R["ref_core"]
    RC = R["ref_creature"]
    keyimg = Image()
    keyimg.bytes = dict(img.bytes)
    for addr, text in KEYS.items():
        keyimg.put(addr, text + b"\0")
    props = [b"name=" + RC_strip(p[5:]) if p.startswith(b"name=") else p for p in props]

    def prop_find(key):
        return F.prop_find(props, key, keyimg)
    if prop_find(keyimg.string(0x52FAC8)) is not None:
        return "Creature_init dead"
    if prop_find(keyimg.string(0x52FB58)) is not None:
        return "Creature_init initScript"
    mem = RC.Memory(under=keyimg)
    c = 0x5F88C0 if SIZES[cls] <= 0x180 else 0x620000
    state = {"seed": seed, "scratch": 0x01F00000}

    def place(text):
        at = state["scratch"]
        for i, b in enumerate(text + b"\0"):
            mem.poke(at + i, 1, b)
        state["scratch"] = (at + len(text) + 16) & ~15
        return at
    type_ptr = place(name)

    def gameobject(self_, pos_, def_, props_):
        for off, v in ((0, 0), (4, 0), (8, 0), (0x48, 0x5122A8), (0x1C, 0x41C00000), (0x20, 0x42900000),
                       (0x0C, 0), (0x10, pos_[0]), (0x14, pos_[1]), (0x18, pos_[2]), (0x34, 0),
                       (0x24, def_)):
            mem.store(self_ + off, 4, v)
        mem.store(self_ + 0x28, 1, 25)
        mem.store(self_ + 0x29, 1, 73)
        state["seed"] = (state["seed"] * 0x19660D + 0x3C6EF35F) & M32
        mem.store(self_ + 0x44, 4, state["seed"] >> 16)

    def getint(self_, key, nth):
        t = prop_find(keyimg.string(key))
        return 0 if t is None else F.atoi(t, keyimg)

    def getstr(self_, key, nth):
        t = prop_find(keyimg.string(key))
        return 0 if t is None else place(t)

    def setstats(self_, roll, *f):
        if roll:
            mem.store(self_ + 0x44, 4, (mem.load(self_ + 0x44, 4) * 0x19660D + 0x3C6EF35F) & M32)

    def handle_clear(head):
        if mem.load(head, 4):
            raise Refused("handle_clear on a list with nodes")

    class Obj:
        def flags64(self):
            return mem.load(c + 0xA8, 8)

        def set_flags64(self, v):
            mem.store(c + 0xA8, 8, v)

        def weapon(self):
            return mem.load(c + 0x50, 4)

        def prop_str(self, addr, text=None):
            return prop_find(text if text is not None else keyimg.string(addr))

        def prop_int(self, addr):
            t = prop_find(keyimg.string(addr))
            return 0 if t is None else F.atoi(t, keyimg)

        def store(self, off, size, value):
            mem.store(c + off, size, value)

        def store_global(self, addr, size, value):
            raise Refused("Creature_init initScript")

    class Host:
        def vcall(self, slot, args):
            if slot == CI.SLOT_MINIBOSS:
                raise Refused("Creature_init MiniBoss")
            if slot == CI.SLOT_SET_STATE:
                raise Refused("Creature_init dead")
            return 0                                   # the model getter: no model here

        def weapon_vcall(self, slot, args):
            raise Refused("Creature_init weapon element")

        def bounds(self, model):
            return [0] * 6

        def hide_mask(self, model, mask):
            return 0

        def call(self, name, args):
            raise Refused(name)

        def cutscene(self):
            return 0

        def change_key(self, n):
            return keyimg.string(0x52FAD0).replace(b"%d", b"%d" % n)

    def creature_init(self_):
        CI.run(Obj(), Host())

    noop = lambda *a: 0  # noqa: E731
    cb = Strict({
        "GameObject_ctor": gameobject, "obj_getPropInt": getint, "obj_getPropStr": getstr,
        "balance_lookup": lambda name: (0, 0, 0, 0, 0, 0), "Creature_setStats": setstats,
        "Creature_init": creature_init, "handle_clear": handle_clear,
        "obj_moveOffGrid": noop, "obj_runEvery60": noop, "obj_runEveryFrame": noop,
        "anim_clear": noop, "anim_push": noop, "anim_randomizeTopTime": noop, "anim_start": noop,
        "anim_startBlendInOut": noop, "anim_syncToSimFrame": noop, "model_buildHideMask": noop,
    })
    m = M1.Model(mem, cb)
    # The factory passes the record's angle sign-extended (0x159cb0-0x159cc8).
    facing = (facing - 0x10000 if facing & 0x8000 else facing) & M32
    try:
        if cls == "smallspider":
            m.smallspider_ctor(c, type_ptr, pos, facing, 0)
        elif cls == "gnaw":
            m.gnaw_ctor(c, type_ptr, pos, facing, 0)
        elif cls == "mosquito":
            m.mosquito_ctor(c, pos, facing, 0, type_ptr)
        elif cls == "scorpion":
            m.scorpion_ctor(c, type_ptr, pos, facing, 0)
        else:
            m.badger_ctor(c, type_ptr, pos, facing, 0, 1 if cls == "snobadge" else 0)
    except Refused as r:
        return str(r)
    data = bytearray(mem.load(c + i, 1) for i in range(SIZES[cls]))
    for off, size in NOT_COPIED:
        for i in range(size):
            data[off + i] = 0
    return bytes(data), state["seed"]


def RC_strip(value):
    """objects::strip_quotes / GameObject_ctor step 7: a leading double quote and up to
    the next one."""
    if not value.startswith(b'"'):
        return value
    end = value.find(b'"', 1)
    return value[1:] if end < 0 else value[1:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--research-dir", required=True)
    parser.add_argument("--out", default=str(ROOT / "native/tests/vectors/level_objects.tsv"))
    args = parser.parse_args()
    R = load(Path(args.research_dir).expanduser())
    F = R["ref_level_factory"]
    img, names = authored_image(F, R["ref_creature_init"])
    out = ["# Generated by tools/generate_level_objects_vectors.py from research",
           "# t3/research-script-session %s. Authored image and records, not the game's." % COMMIT]
    full = dict(img.bytes)
    for addr, text in KEYS.items():
        for i, b in enumerate(text + b"\0"):
            full[addr + i] = b
    for addr in sorted(full):
        out.append("B\t0x%06x\t%02x" % (addr, full[addr]))
    seed, count = SEED0, 0
    positions = [(0x41200000, 0xC1A00000, 0), (0, 0, 0x42C80000), (0x44000000, 0x43000000, 0x3F800000)]
    facings = [0, 0x4000, 0xC000, 0x8001]
    out.append("SEED\t%d" % SEED0)
    k = 0
    for cls in ("smallspider", "gnaw", "mosquito", "scorpion", "snobadge", "badger"):
        variants = [names[cls], names[cls].upper()]
        if cls == "mosquito":
            variants.append(b"MosqBigOne")
        if cls == "badger":
            variants.append(b"badgerling")
        for v in variants:
            for props in PROP_SETS:
                pos = positions[k % len(positions)]
                facing = facings[k % len(facings)]
                k += 1
                got = run_case(R, img, cls, v, pos, facing, props, seed)
                rec = "R\t%s\t%08x,%08x,%08x\t%04x\t%s" % (v.decode(), pos[0], pos[1], pos[2], facing,
                                                         "|".join(p.decode() for p in props))
                if isinstance(got, str):
                    # A refusal before GameObject_ctor draws nothing; after it the draw
                    # is made (the host keeps a stub with the draw made).
                    out.append(rec + "\tstub\t" + got)
                    if got not in ("Creature_init dead", "Creature_init initScript"):
                        seed = (seed * 0x19660D + 0x3C6EF35F) & M32
                else:
                    data, seed = got
                    # Size, then the non-zero bytes as offset:value.
                    sparse = ",".join("%x:%02x" % (i, b) for i, b in enumerate(data) if b)
                    out.append(rec + "\tbuilt\t%x;%s\t%08x" % (len(data), sparse, seed))
                count += 1
    Path(args.out).write_text("\n".join(out) + "\n")
    print("wrote %d records" % count)


if __name__ == "__main__":
    main()
