#!/usr/bin/env python3
"""Port the item, gold, skill and monster-builder script commands as exact vectors.

Reads tools/ref_script_items.py, tools/ref_monster_build.py, tools/ref_items.py,
tools/ref_messages.py, tools/ref_natives.py and tools/ref_core.py from
t3/research-items at 49bc578 (docs/script-items.md). No disc and no executable:
every definition, name, record and string below is authored here, and the
"image" the parser reads is a synthetic one written by this script at the
addresses ref_items reads.

Expected values come only from the reference models. Cases are seeded.
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

COMMIT = "49bc578"
FILES = (
    "docs/script-items.md",
    "tools/ref_script_items.py",
    "tools/ref_monster_build.py",
    "tools/difftest_script_items.py",
    "tools/difftest_monster_build.py",
    "tools/test_ref_script_items.py",
    "tools/test_ref_monster_build.py",
    "tools/ref_items.py",
    "tools/ref_messages.py",
    "tools/ref_natives.py",
    "tools/ref_core.py",
)
SEED = 20973
# The handler's stack record. On the console it is in the handler's frame; the
# native service keeps it outside the binder memory at this host identity.
SCRATCH = 0xFFFF0000
NAME_AREA = 0x00700000          # synthetic image: definition name strings
OBJ_BASE = 0x01200000           # synthetic player objects
HOLD_ONCE = (0x537E08, 0x537E10)  # inventory_giveItem's two held-once names
RECALL = 0x537DB8               # the recall name inventory_giveItem tests
NET_MODE = 0x402791


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(research):
    tools = research / "tools"
    import dis  # noqa: F401  (tools/dis.py shadows the standard library)
    sys.path.append(str(tools))
    import ref_core  # noqa: E402
    import ref_items  # noqa: E402
    import ref_monster_build  # noqa: E402
    import ref_natives  # noqa: E402
    import ref_script_items  # noqa: E402
    return ref_core, ref_items, ref_monster_build, ref_natives, ref_script_items


class Out:
    def __init__(self):
        self.lines = []

    def emit(self, *fields):
        self.lines.append(" ".join(str(f) for f in fields))


def hx(data):
    return bytes(data).hex() if data else "-"


def runs(mem, addrs, gap=1):
    """Merge a set of byte addresses into (start, bytes) runs read from mem. Runs
    closer than gap bytes are joined (the bytes between are read from mem too)."""
    out, start, prev = [], None, None
    for a in sorted(addrs):
        if start is None:
            start = prev = a
        elif a - prev <= gap:
            prev = a
        else:
            out.append((start, mem.read(start, prev - start + 1)))
            start = prev = a
    if start is not None:
        out.append((start, mem.read(start, prev - start + 1)))
    return out


def written(mem, skip=()):
    addrs = set()
    for addr, size in mem.writes:
        for k in range(size):
            a = (addr + k) & 0xFFFFFFFF
            if not any(lo <= a < hi for lo, hi in skip):
                addrs.add(a)
    return addrs


def arg_token(value):
    if isinstance(value, (bytes, bytearray)):
        return "s" + (bytes(value).hex() or "-")
    return "i%d" % value


# ---------------------------------------------------------------------------------------
# Synthetic image: the definition rows, names and strings ref_items.Tables and
# inventory_giveItem read. Authored names only.
# ---------------------------------------------------------------------------------------
DEFS = [
    # name, weight, count, slot (+0x1c), category (+0x1d), subtype, flags
    (b"Sword", 6.0, 1, 0, 0, 1, 0x0000),
    (b"Key of Ash", 0.25, 1, 16, 2, 0, 0x0100),
    (b"Ash", 0.5, 1, 16, 2, 0, 0x0100),
    (b"Arrows", 0.1, 20, 8, 0, 3, 0x0100 | (3 << 11)),
    (b"Throwing Axe", 2.0, 5, 0, 0, 2, (1 << 42) | 0x0100),
    (b"Helm", 9.0, 1, 1, 1, 0, 2 << 11),
    (b"Ring", 0.1, 1, 5, 2, 0, 4 << 11),
    (b"UniqA", 1.0, 1, 16, 2, 0, 0),
    (b"UniqB", 1.0, 1, 16, 2, 0, 0),
    (b"Recall", 0.5, 1, 16, 2, 0, 0x0100),
    (b"Greatsword", 22.0, 1, 2, 0, 4, 1 << 11),
    (b"Plate", 40.0, 1, 1, 1, 0, 0),
    (b"", 0.0, 1, 16, 2, 0, 0),
]
PRESETS = [
    (b"Fine Sword", 5.0, 1, 0, 0, 1, 0),
    (b"Bag of Ash", 3.0, 1, 16, 2, 0, 0x0100),
]
FORMATS_AUTHORED = {0x520450: b"wt%d", 0x5204B0: b"var%d"}


def build_image(N, I):
    img = N.Mem()
    next_name = [NAME_AREA]

    def name(text):
        at = next_name[0]
        img.write(at, text + b"\0")
        next_name[0] += len(text) + 1
        return at

    def rows(base, table):
        addrs = []
        for k, (text, weight, count, slot, cat, sub, flags) in enumerate(table):
            a = base + 0x40 * k
            img.w32(a, name(text))
            img.w16(a + 4, 1 + k % 3)
            img.w32(a + 8, I.f_bits(weight))
            img.w16(a + 0xC, count)
            img.w8(a + 0x1C, slot & 0xFF)
            img.w8(a + 0x1D, cat & 0xFF)
            img.w8(a + 0x1E, sub & 0xFF)
            img.w64(a + 0x28, flags)
            addrs.append(a)
        return addrs

    defs = rows(I.A_DEFS, DEFS)
    presets = rows(I.A_PRESETS, PRESETS)
    for addr, _code in I.TOKEN_FORMATS:
        img.write(addr, FORMATS_AUTHORED.get(addr, b"") + b"\0")
    for addr, text in ((I.S_ARROWS, b"zarr"), (I.S_NAME_SUFFIX, b"%s_n"), (I.S_SPACE, b" "),
                       (I.S_OF_KEY, b"ofk"), (I.S_THROWN_STEM, b"zthr"), (I.S_POTION_STEM, b"zpot"),
                       (I.S_CIRCLET_STEM, b"zcir"), (I.S_RECALL, b"zrec"),
                       (I.S_CIRCLET_STEM_EQUIP, b"zcire"),
                       (HOLD_ONCE[0], b"UniqA"), (HOLD_ONCE[1], b"UniqB"), (RECALL, b"Recall")):
        img.write(addr, text + b"\0")
    img.write(I.A_GOLD_SCALE_MP, struct.pack("<d", 1.0))
    return img, defs, presets


def fake_exe(I, img):
    """ref_items.Exe over the synthetic image."""
    class FakeExe(I.Exe):
        def __init__(self):
            self.img = img

        def read(self, addr, size):
            return self.img.read(addr, size)

        def cstr(self, addr):
            return self.img.cstr(addr)
    return FakeExe()


# ---------------------------------------------------------------------------------------
# Random worlds
# ---------------------------------------------------------------------------------------
class World:
    def __init__(self, rng, R, I, N, img, defs, presets):
        self.rng, self.R, self.I, self.N = rng, R, I, N
        self.img, self.defs, self.presets = img, defs, presets
        self.mem = N.Mem(under=img)
        self.objects = []      # (addr, flags)

    def plant(self):
        rng, R, I, mem = self.rng, self.R, self.I, self.mem
        players = rng.choice([0, 1, 1, 2, 2, 3, 4, 4])
        mem.w32(R.G_NUM_PLAYERS, players)
        mem.w32(R.G_RAND_SEED, rng.getrandbits(32))
        mem.w32(R.G_MEDAL_MISSION, rng.choice([0, 0, 0, 1]))
        mem.w8(R.G_MEDAL_PLAYER, rng.randrange(4))
        mem.w8(NET_MODE, rng.choice([0, 0, 0, 1]))
        mem.w32(0x4F719C, rng.choice([1, 1, 2, 3]))      # g_difficultyIdx
        mem.w32(R.G_LOAD_BONUS, rng.choice([0, 0, 0, 1]))
        mem.w32(R.G_SKILL_OBJECTS + 4 * R.SKILL_AXE_BANDOLIER, 0x01300000)
        mem.w32(R.G_SKILL_OBJECTS + 4 * R.SKILL_LIGHT_QUIVER, 0x01300100)
        for cls in range(4):
            table = R.G_CLASS_SKILLS + cls * R.CLASS_SKILLS_SIZE
            for row in rng.sample(range(48), 3):
                mem.w64(table + row * R.CLASS_SKILL_ROW,
                        rng.choice([R.PASSIVE_LOAD, R.PASSIVE_AMMO, 0x80]))
        if rng.random() < 0.15:
            mem.w8(0x41B69D, 1)
            mem.w32(0x4EBBB0, 0x01400000)
        alive = []
        for slot in range(4):
            r = R.rec(slot)
            mem.w32(r + R.P_CLASS, rng.randrange(4))
            mem.w32(r + R.P_EFF_STRENGTH, rng.randrange(0, 120))
            mem.w32(r + R.P_GOLD, rng.choice([0, 5, 100, 99999990, rng.getrandbits(31)]))
            for k in range(48):
                if rng.random() < 0.1:
                    mem.w8(r + R.P_PASSIVES + k, rng.choice([1, 2, 3, 0xF0]))
            pairs = rng.sample([0x10, 0x20, R.SKILL_AXE_BANDOLIER, R.SKILL_LIGHT_QUIVER, 0x30],
                               rng.randrange(0, 4))
            for k, ident in enumerate(pairs):
                mem.w16(r + R.P_SKILL_PAIRS + 4 * k, ident)
                mem.w16(r + R.P_SKILL_PAIRS + 4 * k + 2, rng.randrange(0, 6))
            mem.w16(r + R.P_SKILL_PAIRS + 4 * len(pairs), 0xFFFF)
            if rng.random() < 0.8:
                obj = OBJ_BASE + 0x100 * slot
                flags = R.OBJ_DEAD if rng.random() < 0.15 else 0
                mem.w32(obj + R.OBJ_FLAGS, flags)
                for k in range(3):
                    mem.w32(obj + R.OBJ_POS + 4 * k, I.f_bits(rng.uniform(-500, 500)))
                mem.w32(r + R.P_OBJECT, obj)
                mem.w32(obj + 0x3C8, slot)
                self.objects.append((obj, flags))
                alive.append(slot)
            for bag in range(3):
                n = rng.choice([0, 0, 1, 2, 3, 4, 6])
                if rng.random() < 0.01:
                    n = 64
                for i in range(n):
                    entry = R.bag_entry(slot, bag, i)
                    if rng.random() < 0.06:
                        continue                      # an empty entry inside the count
                    d = rng.choice(self.defs[:-1] if rng.random() < 0.95 else self.defs)
                    self.item(entry, d)
                mem.w32(R.bag_count_at(slot, bag), n)
                if n < 64 and rng.random() < 0.1:
                    # a stale record at the count: inventory_giveItem sees a full bag
                    self.item(R.bag_entry(slot, bag, n), rng.choice(self.defs[:-1]))
            carried = rng.choice([0.0, 10.0, 150.0, 300.0, 9998.0])
            limit = rng.choice([171.0, 200.0, 9999.0, 50.0])
            mem.w32(r + R.P_LOAD, I.f_bits(carried))
            mem.w32(r + R.P_LOAD_LIMIT, I.f_bits(limit))
            for k, which in enumerate((0x4434, 0x4438, 0x443C)):
                if rng.random() < 0.4 and mem.s32(R.bag_count_at(slot, 0)) > 0:
                    mem.w32(r + which, R.bag_entry(slot, 0, rng.randrange(mem.s32(R.bag_count_at(slot, 0)))))
            for e in range(17):
                if rng.random() < 0.15 and mem.s32(R.bag_count_at(slot, 0)) > 0:
                    mem.w32(r + 0x43F0 + 4 * e, R.bag_entry(slot, 0, rng.randrange(mem.s32(R.bag_count_at(slot, 0)))))
            mem.w8(r + 0x4A7C, rng.randrange(2))
        mem.w32(R.G_ALIVE_LIST, alive[0] if alive else rng.randrange(4))
        return players

    def item(self, entry, d):
        rng, mem = self.rng, self.mem
        mem.w32(entry, d)
        mem.w16(entry + 4, rng.choice([1, 1, 2, 5, 20]))
        flags = mem.load(d + 0x28, 8)
        mem.w64(entry + 8, flags | (rng.randrange(4) << 11 if flags & 0xF800 else 0))
        mem.w8(entry + 0x20, 0xFF)
        mem.w8(entry + 0x21, 0xFF)
        for k in range(4):
            if rng.random() < 0.2:
                mem.w16(entry + 0x24 + 8 * k, rng.choice([0x24, 0x0D, 7]))
                mem.w16(entry + 0x26 + 8 * k, rng.randrange(-20, 60) & 0xFFFF)
            else:
                mem.w16(entry + 0x24 + 8 * k, 0xFFFF)

    def names(self):
        return [self.mem.cstr(self.mem.u32(d)) for d in self.defs]


def emit_world(out, w, before):
    out.emit("objects", len(w.objects), *("%x:%x" % o for o in w.objects))
    for start, data in runs(before, set(a for a, s in before.writes for a in range(a, a + s)), 8):
        out.emit("init", "%x" % start, hx(data))


def snapshot(N, mem):
    """A copy of mem's written bytes (the image stays under)."""
    copy = N.Mem(under=mem.under)
    for page, data in mem.pages.items():
        copy.pages[page] = bytearray(data)
    copy.writes = list(mem.writes)
    return copy


def text_choices(rng, names):
    real = [n for n in names if n]
    a, b = rng.choice(real), rng.choice(real)
    return [a, a.lower(), a[:-1], b"", b"nothing here", a + b" and " + b, b"take the " + a,
            b + b", " + a, b"zz" + a + b"zz", a.upper()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--research-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--cases", type=int, default=40)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()
    root = args.research_dir.resolve()
    missing = [name for name in FILES if not (root / name).is_file()]
    if missing:
        raise SystemExit("missing " + ", ".join(missing))
    commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if not commit.startswith(COMMIT):
        raise SystemExit("research HEAD %s is not %s" % (commit, COMMIT))
    C, I, MB, N, R = load(root)
    img, defs, presets = build_image(N, I)
    tables = I.Tables(fake_exe(I, img))
    rng = random.Random(args.seed)
    out = Out()
    out.emit("# generated by tools/generate_script_items_vectors.py from research-items", COMMIT)
    for start, data in runs(img, set(a for a, s in img.writes for a in range(a, a + s))):
        out.emit("image", "%x" % start, hx(data))

    # ---- handler cases ------------------------------------------------------------------
    handlers = []
    for _ in range(args.cases):
        handlers += [("playerHasItem", "s"), ("onePlayerHasItem", "is"), ("countQuestItems", "s"),
                     ("removePlayerItem", "is"), ("givePlayerItem", "sii"),
                     ("giveOnePlayerItem", "isii"), ("givePlayerItem", "sii"),
                     ("giveOnePlayerItem", "isii")]
    for _ in range(args.cases // 3):
        handlers += [("removeAllItems", ""), ("removeAllWeapons", ""), ("givePlayerGold", "i"),
                     ("takePlayerGold", "i"), ("checkPlayerGold", ""), ("checkPlayerMinGold", ""),
                     ("clearAllSkills", "")]
    count = 0
    for name, sig in handlers:
        w = World(rng, R, I, N, img, defs, presets)
        w.plant()
        before = snapshot(N, w.mem)
        w.mem.writes = []
        names = w.names()
        call_args = []
        for kind in sig:
            if kind == "i":
                if name in ("givePlayerGold", "takePlayerGold"):
                    call_args.append(rng.choice([0, 7, 31, -7, 100, 0x20, 1000000, -1000]))
                elif not call_args and name == "giveOnePlayerItem":
                    call_args.append(rng.choice([0, 0, 0, 1, 1, 2, 3, -1, 4]))
                elif not call_args and name in ("onePlayerHasItem", "removePlayerItem"):
                    call_args.append(rng.choice([-1, -1, 0, 1, 2, 3, 4, -2]))
                else:
                    call_args.append(rng.choice([0, 0, 1, 1, 2]))
            else:
                if name in ("givePlayerItem", "giveOnePlayerItem"):
                    pool = names[:-1] + [b"Fine Sword", b"Bag of Ash", b"Unknown thing",
                                         b"Sword+wt20", b"Ring+var2", b"Arrows+wt-50"]
                    call_args.append(rng.choice(pool))
                elif name == "removePlayerItem":
                    call_args.append(rng.choice(text_choices(rng, names)))
                else:
                    call_args.append(rng.choice(names[:-1] + text_choices(rng, names)[:4]))
        answers = {
            "net_isLocalSlot": rng.choice([0, 1]),
            "pool_new": rng.choice([0, 0x01F00000]),
            "skill_lightQuiverComplement": I.f_bits(rng.choice([0.9, 0.75, 1.0])),
            "skill_axeBandolierComplement": I.f_bits(rng.choice([0.5, 0.8])),
            "vars_get": rng.choice([0, 0, 1]),
            "text_lookup": 0x01500000,
        }
        host = R.Host({k: (lambda *a, _v=v: _v) for k, v in answers.items()})
        comp = R.Composer(w.mem, tables, host, SCRATCH)
        fault = None
        result = None
        try:
            result = getattr(comp, name)(*call_args)
        except (R.Fault, I_Fault(I)) as error:   # noqa: F821
            fault = str(error)
        refused = None
        for call, cargs in host.log:
            if call == "inventory_equipItem":
                slot, entry = cargs[0], cargs[1]
                # inventory_giveItem returned -1: the entry is 0x48 bytes before the bag
                if not any(R.bag_entry(slot, b, 0) <= entry < R.bag_entry(slot, b, 64)
                           and (entry - R.bag_entry(slot, b, 0)) % R.ITEM_SIZE == 0 for b in range(3)):
                    refused = "equip entry outside the bag"
                    break
        count += 1
        out.emit("case", count, name)
        emit_world(out, w, before)
        for k, v in answers.items():
            out.emit("answer", k, v)
        out.emit("call", name, *(arg_token(a) for a in call_args))
        if refused:
            out.emit("expect_refused", refused.replace(" ", "_"))
        elif fault is not None:
            out.emit("expect_fault")
        else:
            out.emit("expect_result", result if result is not None else 0)
            for call, cargs in host.log:
                flat = []
                for a in cargs:
                    if isinstance(a, tuple):
                        flat += ["%x" % (x & 0xFFFFFFFF) for x in a]
                    else:
                        flat.append("%x" % (a & 0xFFFFFFFF))
                out.emit("expect_log", call, *flat)
            for start, data in runs(w.mem, written(w.mem, skip=((SCRATCH, SCRATCH + 0x48),))):
                out.emit("expect_bytes", "%x" % start, hx(data))
        out.emit("end")

    # ---- item weight, carried load, load limit --------------------------------------------
    for _ in range(args.cases * 2):
        w = World(rng, R, I, N, img, defs, presets)
        w.plant()
        slot = rng.randrange(4)
        answers = {"skill_lightQuiverComplement": I.f_bits(rng.choice([0.9, 0.75])),
                   "skill_axeBandolierComplement": I.f_bits(rng.choice([0.5, 0.8]))}
        host = R.Host({k: (lambda *a, _v=v: _v) for k, v in answers.items()})
        before = snapshot(N, w.mem)
        w.mem.writes = []
        R.recalc_load(w.mem, tables, host, slot)
        count += 1
        out.emit("case", count, "recalcLoad")
        emit_world(out, w, before)
        for k, v in answers.items():
            out.emit("answer", k, v)
        out.emit("call", "recalcLoad", "i%d" % slot)
        out.emit("expect_result", w.mem.u32(R.rec(slot) + R.P_LOAD))
        for call, cargs in host.log:
            out.emit("expect_log", call, *("%x" % (a & 0xFFFFFFFF) for a in cargs))
        out.emit("end")
    count += 1
    out.emit("case", count, "itemWeightNull")
    out.emit("call", "itemWeight", "i0", "i0")
    out.emit("expect_result", R.item_weight(N.Mem(), None, R.Host(), 0, 0))
    out.emit("end")
    for _ in range(args.cases * 2):
        strength = rng.choice([0, 46, 120, -200, 9874, 9000, rng.randrange(-1000, 20000)])
        level = rng.choice([0, 1, 2, 5, 127])
        bonus = rng.choice([0, 1, -1, 0x80000000])
        out.emit("limit", strength, level, bonus, R.load_limit(strength, level, bonus))
    for _ in range(args.cases):
        carried = I.f_bits(rng.choice([0.0, 160.0, 171.0, 200.0, -0.0, 9998.5]))
        if rng.random() < 0.2:
            carried = 0x80000000
        limit = I.f_bits(rng.choice([171.0, 200.0, 9999.0]))
        weight = rng.choice([0, 0x80000000, I.f_bits(10.0), I.f_bits(11.0), I.f_bits(0.5)])
        strict = rng.choice([0, 1])
        mem = N.Mem()
        mem.w32(R.rec(0) + R.P_LOAD, carried)
        mem.w32(R.rec(0) + R.P_LOAD_LIMIT, limit)
        out.emit("fits", carried, limit, weight, strict, int(R.fits(mem, 0, weight, bool(strict))))

    # ---- game_scanCm1Vars ------------------------------------------------------------------
    for _ in range(args.cases):
        word = rng.choice([0xFFFFFFFF, 0, 4, 9, 12])
        out.emit("cm1_cached", word, R.scan_cm1_vars(0, None, word))
        values = {n: rng.choice([0, 0, 0, 1, 5]) for n in R.CM1_NAMES}
        asked = []

        def get(n, _v=values, _a=asked):
            _a.append(n)
            return _v[n]
        index = R.scan_cm1_vars(1, get, 0)
        out.emit("cm1_scan", ",".join("%s=%d" % (n.decode(), values[n]) for n in R.CM1_NAMES), index,
                 ",".join(n.decode() for n in asked))

    # ---- arena_spawnMonster ------------------------------------------------------------------
    class_names = [b"Mon%02d" % k for k in range(MB.ARENA_CLASS_COUNT)]
    for _ in range(args.cases * 2):
        mem = N.Mem()
        mem.w16(MB.G_WAVE, rng.choice([0, 1, 3, 15, 16, 31, 100, 0xFFFC]))
        mem.w16(MB.G_RECIPE_COUNT, rng.choice([5, 5, 3, 1]))
        mem.w16(MB.G_RING_ANGLE, rng.getrandbits(16))
        mem.w16(MB.G_RING_STEP, rng.getrandbits(16))
        centre = 0x01600000
        mem.w32(MB.G_RING_CENTRE, centre)
        for k in range(3):
            mem.w32(centre + 4 * k, I.f_bits(rng.uniform(-3000, 3000)))
        mem.w32(MB.G_RAND_SEED, rng.getrandbits(32))
        misses = rng.choice([0, 0, 1, 3, 16, 17, 20])
        spot = tuple(I.f_bits(rng.uniform(-3000, 3000)) for _ in range(3))
        with_name = rng.random() < 0.75
        name = rng.choice(class_names) if with_name else None
        creature = 0x01700000
        if not with_name:
            mem.w32(creature + 0x10, 1)
        obj_answer = rng.choice([0x01800000, 0x01800000, 0])
        block = rng.choice([0x01900000, 0])
        tries = [0]

        def free_spot(point, rng_, radius, flag, _t=tries, _m=misses, _s=spot):
            _t[0] += 1
            return (_s, 0) if _t[0] <= _m else (_s, 1)
        host = MB.Host({"world_findFreeSpot": free_spot,
                        "world_createObject": lambda *a, _o=obj_answer: _o,
                        "pool_new": lambda *a, _b=block: _b})
        before = snapshot(N, mem)
        mem.writes = []
        fault = None
        result = None
        try:
            result = MB.arena_spawn_monster(mem, host, name, None if with_name else creature, class_names)
        except MB.Fault as error:
            fault = str(error)
        count += 1
        out.emit("case", count, "arenaSpawn")
        for start, data in runs(before, set(a for a, s in before.writes for a in range(a, a + s))):
            out.emit("init", "%x" % start, hx(data))
        out.emit("answer", "misses", misses)
        out.emit("answer", "spot", *("%x" % v for v in spot))
        out.emit("answer", "object", obj_answer)
        out.emit("answer", "block", block)
        out.emit("call", "arenaSpawn", arg_token(name) if name else "i0", "i%d" % (0 if with_name else creature))
        if fault is not None:
            out.emit("expect_fault")
        else:
            out.emit("expect_result", result if result is not None else 0)
            for call, cargs in host.log:
                flat = []
                for a in cargs:
                    if isinstance(a, tuple):
                        flat += ["%x" % (x & 0xFFFFFFFF) for x in a]
                    elif isinstance(a, (bytes, bytearray)):
                        flat.append("s" + bytes(a).hex())
                    elif a is None:
                        flat.append("none")
                    else:
                        flat.append("%x" % (a & 0xFFFFFFFF))
                out.emit("expect_log", call, *flat)
            for start, data in runs(mem, written(mem)):
                out.emit("expect_bytes", "%x" % start, hx(data))
        out.emit("end")
    for k, n in enumerate(class_names):
        out.emit("arena_class", k, n.decode())

    # ---- ScriptGenerator_spawn ---------------------------------------------------------------------
    for _ in range(args.cases):
        mem = N.Mem()
        gen = 0x01A00000
        for k in range(3):
            mem.w32(gen + 0x10 + 4 * k, I.f_bits(rng.uniform(-900, 900)))
        mem.w16(gen + 0x38, rng.getrandbits(16))
        mem.w32(gen + 0x2C, rng.choice([0, 0x01B00000]))
        obj = rng.choice([0x01C00000, 0x01C00000, 0])
        if obj:
            mem.w64(obj + 0xA8, rng.getrandbits(64))
        dyn = rng.choice([b"3Orc", b"8Creature", b"8Skeleton"])
        type_name = rng.choice([b"Orc", b"orc2", b"Skeleton"])
        host = MB.Host({"world_createObject": lambda *a, _o=obj: _o})
        before = snapshot(N, mem)
        mem.writes = []
        result = MB.script_generator_spawn(mem, host, gen, 0x5D32C8, type_name, lambda o, _d=dyn: _d)
        count += 1
        out.emit("case", count, "generatorSpawn")
        for start, data in runs(before, set(a for a, s in before.writes for a in range(a, a + s))):
            out.emit("init", "%x" % start, hx(data))
        out.emit("answer", "object", obj)
        out.emit("answer", "dynamic", dyn.decode())
        out.emit("call", "generatorSpawn", "s" + type_name.hex())
        if any(c == "virtual" for c, _ in host.log):
            out.emit("expect_refused", "Skeleton_rise_state")
        else:
            out.emit("expect_result", result)
            for call, cargs in host.log:
                flat = []
                for a in cargs:
                    if isinstance(a, tuple):
                        flat += ["%x" % (x & 0xFFFFFFFF) for x in a]
                    elif isinstance(a, (bytes, bytearray)):
                        flat.append("s" + bytes(a).hex())
                    else:
                        flat.append("%x" % (a & 0xFFFFFFFF))
                out.emit("expect_log", call, *flat)
            for start, data in runs(mem, written(mem)):
                out.emit("expect_bytes", "%x" % start, hx(data))
        out.emit("end")

    # ---- generateMonster's name table ------------------------------------------------------------
    for _ in range(args.cases):
        mem = N.Mem()
        existing = rng.sample([b"Orc", b"Goblin", b"Kobold", b"Ogre", b"Mummy"], rng.randrange(0, 4))
        for k, n in enumerate(existing):
            mem.write(MB.G_GEN_NAMES + MB.GEN_NAME_SIZE * k, n + b"\0")
        start_count = len(existing)
        if rng.random() < 0.1:
            start_count = 0x80 | rng.randrange(4)
        mem.w8(MB.G_GEN_COUNT, start_count)
        name = rng.choice(existing + [b"Ghoul", b"Hag", b"X" * 25]) if existing else rng.choice([b"Ghoul", b"X" * 25])
        before = snapshot(N, mem)
        mem.writes = []
        at = MB.intern_class_name(mem, name)
        count += 1
        out.emit("case", count, "intern")
        for start, data in runs(before, set(a for a, s in before.writes for a in range(a, a + s))):
            out.emit("init", "%x" % start, hx(data))
        out.emit("call", "intern", "s" + name.hex())
        out.emit("expect_result", at)
        for start, data in runs(mem, written(mem)):
            out.emit("expect_bytes", "%x" % start, hx(data))
        out.emit("end")

    args.out.write_text("\n".join(out.lines) + "\n")
    contract = {
        "commit": commit,
        "policy": "rta-script-items-v1",
        "lines": len(out.lines),
        "seed": args.seed,
        "cases": args.cases,
        "sha256": {name: digest(root / name) for name in FILES},
    }
    args.contract.write_text(json.dumps(contract, indent=2) + "\n")
    print("%d lines, %d cases" % (len(out.lines), count))


def I_Fault(I):
    return getattr(I, "Fault", RuntimeError)


if __name__ == "__main__":
    main()
