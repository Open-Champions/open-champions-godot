#!/usr/bin/env python3
"""Replay research tools/ref_strings.py on synthetic cases for the native strings port.

Research t3/research-strings d4987fd (docs/strings.md, tools/ref_strings.py) ran the
original functions next to that model in tools/difftest_strings.py. This generator
does not open the executable or the disc. It builds random synthetic tables,
formats, HUD command sequences, sound slots and handle lists, runs the reference
model on them and writes what the model produced. native/tests/strings_tests.cpp
replays every operation on the C++ port and compares every value.

Where the port refuses on purpose (docs/STRINGS.md), the case is written as a
refusal and the model is not run for it:

  * a format that would read past its own terminator (it ends in a conversion,
    in a lone '%', or is empty): wstr_format would copy that terminator and keep
    reading the memory after it;
  * a HUD text that would overflow its buffer (bust text 31, compass label 31,
    message 99, help-F title 31, help-F body 127, formatted text 255 units,
    bust texture name 39 bytes).

Each pinned research file is read with `git show d4987fd:<path>` into a temporary
directory and must hash to its pin; nothing is imported from the research
working tree. A manifest with the pins and the vector hash is written beside the
vectors.

    python3 tools/generate_strings_vectors.py \\
        --research-dir <research-checkouts>/research-strings \\
        --out native/tests/vectors/strings.tsv.gz \\
        --manifest native/tests/vectors/strings.json
"""
import argparse
import gzip
import hashlib
import importlib
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = "d4987fd"
PINS = {
    "tools/ref_strings.py": "7e40f746a4ab66feb21afb4a6e38f27d08a9048135776452102364374c268b77",
    "tools/ref_core.py": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123",
}
DOC_PINS = {
    "docs/strings.md": "ab48cb18861d444d26658623dbddbdbc0eb9e423ec093af50795cc984faf130c",
}
SEEDS = (20973, 7)
R = RC = None


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


def hx(data):
    return bytes(data).hex() or "-"


def uh(units):
    """Units as 4-hex-digit groups, '-' when empty."""
    return "".join("%04x" % (u & 0xFFFF) for u in units) or "-"


# ---------------------------------------------------------------------------------------
# Random inputs (synthetic: keys and texts are random characters)
# ---------------------------------------------------------------------------------------
KEY_CHARS = b"abcdefghijklmnopqrstuvwxyz_0123456789ABCDEFXYZ"


def random_key(rng, lo=1, hi=24, high=0.1, quotes=0.0):
    out = bytearray()
    for _ in range(rng.randint(lo, hi)):
        roll = rng.random()
        if roll < high:
            out.append(rng.randint(0x80, 0xFF))
        elif roll < high + quotes:
            out.append(0x22)
        else:
            out.append(rng.choice(KEY_CHARS))
    return bytes(out)


def random_text(rng, lo=0, hi=40, alphabet=None):
    n = rng.randint(lo, hi)
    if alphabet:
        return "".join(rng.choice(alphabet) for _ in range(n))
    chars = []
    for _ in range(n):
        roll = rng.random()
        if roll < 0.75:
            chars.append(chr(rng.randint(0x20, 0x7E)))
        elif roll < 0.95:
            chars.append(chr(rng.randint(0xA0, 0x17F)))
        else:
            chars.append(chr(rng.randint(0x3000, 0x30FF)))
    return "".join(chars)


def colliding_keys(rng, count):
    target = rng.randrange(0x2000)
    found = []
    while len(found) < count:
        key = random_key(rng, 3, 8, high=0.0)
        if R.text_hash(key) == target and key not in found:
            found.append(key)
    return found


def make_table(rng, entries, collide=0, dup=0, quoted=0, pad=0, text=None):
    pairs, keys = [], set()
    for key in colliding_keys(rng, collide) if collide else []:
        pairs.append((key, (text or random_text)(rng)))
        keys.add(key)
    while len(pairs) < entries:
        key = random_key(rng, 1, 30, high=0.05)
        if key in keys:
            continue
        keys.add(key)
        pairs.append((key, (text or random_text)(rng)))
    for _ in range(dup):
        key = rng.choice(sorted(keys))
        pairs.insert(rng.randrange(len(pairs) + 1), (key, (text or random_text)(rng)))
    for _ in range(quoted):
        key = random_key(rng, 2, 12, high=0.0)
        pairs.append((key[:1] + b'"' + key[1:], (text or random_text)(rng)))
    rng.shuffle(pairs)
    return pairs, R.build_uni(pairs, pad=pad)


FLOAT_EDGE = (0x7F800000, 0xFF800000, 0x7FC00000, 0x7F800001, 0xFFFFFFFF, 0x7FFFFFFF,
              0x00000001, 0x80000001, 0x007FFFFF, 0x00000000, 0x80000000, 0x7F7FFFFF,
              0x4B000000, 0xCF000000, 0x3C23D70A, 0x3BA3D70A)


def float_arg(rng, kind):
    if kind == "int":
        v = rng.choice((rng.randint(-1000, 1000), rng.randint(-(1 << 24), 1 << 24),
                        rng.randint(-(1 << 31), (1 << 31) - 1)))
        return R.cvt_s_w(v)
    if kind == "float":
        roll = rng.random()
        if roll < 0.4:
            return RC.f32(rng.uniform(-1000, 1000))
        if roll < 0.6:
            return RC.f32(rng.choice((0.125, 0.375, -0.125, 2.675, 1.005, 0.005, -0.0, 0.0, 1e-3, -1e-3)))
        if roll < 0.75:
            return RC.f32(rng.uniform(-1e9, 1e9))
        if roll < 0.85:
            return rng.choice(FLOAT_EDGE)
        return rng.getrandbits(32) & 0x7F7FFFFF | (rng.getrandbits(1) << 31)
    return rng.getrandbits(32)


def reads_past_terminator(units):
    """True when wstr_format would copy the format's own terminator: the loop copies a
    0 unit, or a '%' is followed by the terminator (docs/strings.md 2.3)."""
    i = 0
    while True:
        c = units[i] if i < len(units) else 0
        if c == 0:
            return True
        if c == 0x25:
            if (units[i + 1] if i + 1 < len(units) else 0) == 0:
                return True
            i += 2
            continue
        i += 1
        if (units[i] if i < len(units) else 0) == 0:
            return False


# ---------------------------------------------------------------------------------------
# 1. Hash and table
# ---------------------------------------------------------------------------------------
def gen_hash(out, rng):
    for _ in range(4000):
        key = random_key(rng, 0, 60, high=0.3)
        out.append("H %s %x" % (hx(key), R.text_hash(key)))


def slots_line(table):
    used = ["%x:%x" % (i, a) for i, a in enumerate(table.slots) if a]
    return "TS %d %s" % (len(used), " ".join(used) if used else "-")


def gen_tables(out, rng):
    for round_ in range(6):
        table = R.TextTable()
        out.append("T new")
        blobs, keys = [], []
        base = 0x100000 + round_ * 0x40000
        for b in range(rng.randint(1, 3)):
            entries = rng.choice((5, 20, 60, 160))
            pairs, data = make_table(rng, entries, collide=rng.choice((0, 4, 12)),
                                     dup=rng.choice((0, 2, 6)), quoted=rng.choice((0, 2)),
                                     pad=rng.choice((0, 0x5A)))
            table.register(data, base)
            out.append("T reg %x %s" % (base, hx(data)))
            out.append(slots_line(table))
            blobs.append((base, data))
            keys += [k for k, _t in pairs]
            base += (len(data) + 0x10) & ~0xF
        for step in range(700):
            roll = rng.random()
            if roll < 0.02 and len(blobs) > 1:
                victim = blobs.pop(rng.randrange(len(blobs)))
                table.unregister(victim[1], victim[0])
                out.append("T unreg %x %s" % (victim[0], hx(victim[1])))
                out.append(slots_line(table))
                continue
            if roll < 0.55 and keys:
                key = rng.choice(keys)
                if rng.random() < 0.15:
                    key = key[:1] + b'"' + key[1:]
            else:
                key = random_key(rng, 0, 40, high=0.2, quotes=0.05)
            stale = bytes(rng.getrandbits(8) for _ in range(rng.randint(0, 20)))
            if rng.random() < 0.5:
                out.append("T find %s %x" % (hx(key), table.find(key)))
            else:
                addr, units, hit = table.lookup(key, stale)
                out.append("T look %s %s %x %d %s %d %s" % (
                    hx(key), hx(stale), addr, int(hit), uh(units), table.nf_index,
                    uh(table.nf_ring[table.nf_index])))
        # a key copy that overruns the 0x200-byte stack buffer is refused
        out.append("T refuse %s" % hx(b"k" * 0x200))
        # the walk the table registers, entry by entry (offsets and units)
        base, data = blobs[0]
        out.append("T walk %s" % hx(data))
        walked = list(R.uni_entries(data))
        for koff, key, toff, units, nxt in walked:
            out.append("T entry %x %s %x %s %x" % (koff, hx(key), toff, uh(units), nxt))
        out.append("T entries-end %d" % len(walked))


# ---------------------------------------------------------------------------------------
# 2. Formatter
# ---------------------------------------------------------------------------------------
CONVERSIONS = ("%", "d", "c", "s", "u", "f", "x", ".", "l", "i")


ASCII = "".join(chr(c) for c in range(0x20, 0x7F) if c != 0x25)


def random_format(rng, ascii_only=False):
    parts, kinds = [], []
    for _ in range(rng.randint(0, 10)):
        if rng.random() < 0.5:
            text = random_text(rng, 1, 6, alphabet=ASCII if ascii_only else None)
            parts += [ord(c) for c in text.replace("%", "#")]
        else:
            conv = rng.choice(CONVERSIONS)
            parts += [0x25, ord(conv)]
            if conv in "dcsuf":
                kinds.append(conv)
    return parts, kinds


def gen_helpers(out, rng):
    for _ in range(600):
        units = [rng.randint(1, 0xFFFF) for _ in range(rng.randint(0, 60))]
        out.append("W copy %s %s" % (uh(units + [0]), uh(R.wstr_copy(units))))
        raw = bytes(rng.randint(1, 255) for _ in range(rng.randint(0, 60)))
        out.append("W from %s %s" % (hx(raw), uh(R.wstr_from_ascii(raw))))
        out.append("W to %s %s" % (uh(units + [0]), hx(R.wstr_to_ascii(units))))
    for raw in FLOAT_EDGE + tuple(float_arg(rng, "float") for _ in range(400)):
        out.append("D %08x %s" % (raw & 0xFFFFFFFF, R._float_text(raw).decode()))


def gen_formats(out, rng, wide=True, count=2500):
    strings_c, strings_w = {}, {}
    addr = 0x1000
    for _ in range(6):
        text = random_text(rng, 0, 20, alphabet="abcdefgh XYZ\xe9\xff").encode("latin1")
        strings_c[addr] = text
        out.append("FM c %x %s" % (addr, hx(text)))
        addr += 0x100
    for _ in range(6):
        units = [rng.randint(1, 0xFFFF) for _ in range(rng.randint(0, 12))]
        strings_w[addr] = units
        out.append("FM w %x %s" % (addr, uh(units)))
        addr += 0x100
    caddrs, waddrs = sorted(strings_c), sorted(strings_w)
    for _ in range(count):
        units, kinds = random_format(rng, not wide)
        ending = rng.random()
        if ending < 0.15:
            conv = rng.choice("dcf%x")
            units = units + [0x25, ord(conv)]
            if conv in "dcf":
                kinds.append(conv)
        elif ending < 0.22:
            units = units + [0x25]
        elif ending < 0.24:
            units, kinds = [], []
        tail = [ord(c) for c in random_text(rng, 0, 6, alphabet="abcXYZ09 ")] + [0x41, 0, 0]
        args = []
        for kind in kinds[:9]:
            if kind == "s":
                args.append(R.cvt_s_w(rng.choice(caddrs)))
            elif kind == "u":
                args.append(R.cvt_s_w(rng.choice(waddrs)))
            elif kind in "dc":
                args.append(float_arg(rng, "int" if rng.random() < 0.8 else "raw"))
            else:
                args.append(float_arg(rng, "float"))
        args = (args + [float_arg(rng, "raw") for _ in range(8)])[:8]
        if wide:
            head = "F %s %s" % (uh(units + [0] + tail), " ".join("%08x" % a for a in args))
            fmt = units + [0] + tail
        else:
            fmt8 = bytes(units)
            head = "FA %s %s" % (hx(fmt8), " ".join("%08x" % a for a in args))
            fmt = R.wstr_from_ascii(fmt8) + tail
        if reads_past_terminator(fmt) or len(kinds) > 8:
            out.append(head + " R")
            continue
        written, used = R.wstr_format(fmt, args, cstr=lambda a: strings_c[a] + b"\0",
                                      wstr=lambda a: strings_w[a] + [0])
        out.append(head + " %s %d" % (uh(written), used))
    # a widened format over the 0x200-byte stack buffer is refused
    out.append("FA %s %s R" % (hx(b"a" * 0x100), " ".join("00000000" for _ in range(8))))


# ---------------------------------------------------------------------------------------
# 3. HUD commands
# ---------------------------------------------------------------------------------------
def hud_text(rng):
    roll = rng.random()
    if roll < 0.3:
        return random_text(rng, 0, 30)
    if roll < 0.5:
        return random_text(rng, 25, 40)
    if roll < 0.6:
        return random_text(rng, 90, 140)
    if roll < 0.8:
        pieces = []
        for _ in range(rng.randint(0, 4)):
            pieces.append(random_text(rng, 0, 8, alphabet="abc XYZ:"))
            pieces.append(rng.choice(("%d", "%c", "%%", "%f", "%x", "%s", "%u")))
        if rng.random() < 0.6:
            pieces.append(random_text(rng, 1, 8, alphabet="abc XYZ."))
        return "".join(pieces)
    return random_text(rng, 0, 12)


class Peek:
    """What a lookup would return, without touching the miss ring: the port refuses
    before the binder looks anything up."""

    def __init__(self, table):
        self.table = table

    def units(self, key):
        addr = self.table.find(key)
        if addr == 0:
            return R.nf_text(key)
        for a, (taddr, units) in self.table.texts.items():
            if taddr == addr:
                return list(units)
        raise AssertionError("text address without an entry")


def resolve(table, hud, addr):
    if addr == 0:
        return []
    if R.HELP_F_TITLE <= addr < 0x5D3188:
        return hud.read_units(addr)
    out, k = [], 0
    while True:
        unit = table.unit_at(addr + 2 * k)
        if unit == 0:
            return out
        out.append(unit)
        k += 1


def hud_state(table, hud):
    return "E %s %x %x %d %x %x %s %s %d %d %d %s %s %08x %08x %s %08x %08x %08x %d %d" % (
        uh(hud.read_units(R.MESSAGE_TEXT)), hud.msg_frames, hud.msg_force, hud.help_open,
        hud.help_title, hud.help_body, uh(resolve(table, hud, hud.help_title)),
        uh(resolve(table, hud, hud.help_body)), hud.help_close_delay, hud.bust_shown,
        hud.compass_shown, hx(hud.bust_texture), uh(hud.read_units(R.BUST_TEXT)),
        hud.word(R.BUST_VALUE), hud.word(R.BUST_MAX), uh(hud.read_units(R.COMPASS_LABEL)),
        hud.compass_target[0], hud.compass_target[1], hud.compass_target[2], hud.latch,
        table.nf_index)


def gen_hud(out, rng, seed_index):
    table = R.TextTable()
    pairs, data = make_table(rng, 80, dup=4, text=hud_text)
    base = 0x300000 + seed_index * 0x100000
    table.register(data, base)
    out.append("U new")
    out.append("U reg %x %s" % (base, hx(data)))
    keys = [k for k, _t in pairs]
    hud = R.Hud()
    objects = {}
    for _ in range(6):
        name = random_key(rng, 3, 10, high=0.0)
        objects[name] = tuple(rng.getrandbits(32) for _ in range(3))
        out.append("U obj %s %08x %08x %08x" % (hx(name), *objects[name]))
    cmds = R.Commands(table, hud, find_object=lambda n: objects.get(n))
    peek = Peek(table)
    local_mask = 0xF
    hud.local = lambda slot: bool(local_mask >> (slot & 3) & 1) if 0 <= slot < 4 else False

    def key():
        if rng.random() < 0.8:
            return rng.choice(keys)
        return random_key(rng, 1, 26, high=0.0, quotes=0.05)

    def gate_open():
        if hud.suppressed or hud.local_menu:
            return False, False
        if hud.camera_mode is None:
            return False, True
        return not (hud.camera_mode != 1 or hud.loadscreen or hud.menu_open), False

    for step in range(3000):
        hud.events = []
        roll = rng.random()
        refuse = False
        if roll < 0.08:
            hud.suppressed = int(rng.random() < 0.15)
            hud.local_menu = int(rng.random() < 0.15)
            hud.menu_open = int(rng.random() < 0.1)
            hud.camera_mode = rng.choice((1, 1, 1, 1, 0, 2, None))
            hud.loadscreen = int(rng.random() < 0.1)
            hud.warmup = int(rng.random() < 0.1)
            hud.num_players = rng.choice((1, 1, 2, 4))
            hud.network = int(rng.random() < 0.3)
            hud.alive = rng.sample(range(4), rng.randint(0, 4))
            local_mask = rng.getrandbits(4)
            out.append("U in %d %d %d %d %d %d %d %d %s %x" % (
                hud.suppressed, hud.local_menu, hud.menu_open,
                -1 if hud.camera_mode is None else hud.camera_mode, hud.loadscreen,
                hud.warmup, hud.num_players, hud.network,
                ",".join(str(a) for a in hud.alive) or "-", local_mask))
            out.append(hud_state(table, hud))
            continue
        if roll < 0.25:
            k = key()
            line = "U setinfo %s" % hx(k)
            units = peek.units(k)
            if not hud.local_menu and len(units) > R.LIMITS["message"]:
                refuse = True
            else:
                cmds.set_info(k)
        elif roll < 0.38:
            t, b = key(), key()
            line = "U help %s %s" % (hx(t), hx(b))
            cmds.add_help_message(t, b)
        elif roll < 0.48:
            k1, k2 = key(), key()
            v1, v2 = (rng.choice((rng.randint(-50, 5000), rng.getrandbits(32))) for _ in range(2))
            line = "U helpf %s %08x %s %08x" % (hx(k1), v1 & 0xFFFFFFFF, hx(k2), v2 & 0xFFFFFFFF)
            outs = []
            for k, v in ((k1, v1), (k2, v2)):
                units = peek.units(k)
                if reads_past_terminator(units + [0]):
                    refuse = True
                    break
                try:
                    formatted, _used = R.format_one_int(units + [0], v)
                except R.FormatError:
                    refuse = True
                    break
                outs.append(R.visible(formatted))
            if not refuse:
                if len(outs[0]) > R.LIMITS["help F title"] or len(outs[1]) > R.LIMITS["help F body"]:
                    refuse = True
            if not refuse:
                cmds.add_help_message_f(k1, v1, k2, v2)
        elif roll < 0.62:
            texture = random_key(rng, 1, 44, high=0.0)
            k = key()
            value = rng.choice((rng.randint(-5, 300), rng.getrandbits(32)))
            maximum = rng.choice((0xFFFFFFFF, rng.randint(0, 300)))
            line = "U bust %s %s %08x %08x" % (hx(texture), hx(k), value & 0xFFFFFFFF, maximum)
            units = peek.units(k)
            ok, fault = gate_open()
            if fault:
                refuse = True
            elif ok and (len(units) > R.LIMITS["bust text"] or len(texture) > R.LIMITS["bust texture name"]):
                refuse = True
            else:
                cmds.show_bust(texture, k, value, maximum)
        elif roll < 0.70:
            name = rng.choice(sorted(objects)) if rng.random() < 0.85 else random_key(rng, 3, 10, high=0.0)
            k = key()
            line = "U compass %s %s" % (hx(name), hx(k))
            units = peek.units(k)
            ok, fault = gate_open()
            if name not in objects or fault:
                refuse = True
            elif ok and len(units) > R.LIMITS["compass label"]:
                refuse = True
            else:
                cmds.show_compass(name, k)
        elif roll < 0.76:
            text = [ord(c) for c in random_text(rng, 0, 120)]
            frames = rng.choice((180, 0, 1, 0xFFFF, 40))
            slot = rng.choice((-1, -1, 0, 1, 2, 3))
            force = rng.choice((0, 0, 1, 0x80))
            line = "U msg %s %x %d %x" % (uh(text), frames, slot, force)
            stores = (slot == -1 or hud.local(slot)) and not (R.sx8(force) == 0 and hud.local_menu)
            if stores and len(text) > R.LIMITS["message"]:
                refuse = True
            else:
                hud.show_message(text, frames, slot, force)
        elif roll < 0.92:
            line = "U tickmsg"
            hud.tick_message()
        else:
            line = "U tickw"
            hud.tick_widgets()
        events = ";".join(",".join(str(x) for x in e) for e in hud.events) or "-"
        out.append(line + (" R" if refuse else " ok %s" % events.replace(" ", "")))
        out.append(hud_state(table, hud))


# ---------------------------------------------------------------------------------------
# 4. sound_playOnObject, 5. Creature_setCharm (shared handle pool dump)
# ---------------------------------------------------------------------------------------
class Obj:
    """An object with the three heads; position words for the sound slot."""

    def __init__(self, ident, addr):
        self.ident, self.addr = ident, addr
        self.heads = [RC.Cell(addr), RC.Cell(addr + 4), RC.Cell(addr + 8)]
        self.position = (0, 0, 0)


def node_ref(node, cell):
    if node is None:
        return "n"
    if node is cell:
        return "c"
    if isinstance(node, RC.Cell):
        return "C%x" % node.name
    return "%x" % node.index


def dump_pool(tag, pool, cells, ident):
    parts = []
    for cell in cells:
        nodes, node = [], cell.next
        while node is not None:
            nodes.append("%x/%s/%s/%s" % (node.index, node_ref(node.prev, cell),
                                          node_ref(node.partner, None),
                                          "n" if node.obj is None else "%x" % ident(node.obj)))
            node = node.next
        parts.append(".".join(nodes) or "-")
    free, node = [], pool.free
    while node is not None and len(free) < 16:
        free.append("%x" % node.index)
        node = node.next
    return "%s %s | %s" % (tag, " ".join(parts), ".".join(free) or "-")


SLOT_NAMES = ("tracks", "capacity", "flags", "seq", "track_link", "track_time", "track_flags",
              "serial", "no_pos", "frozen", "released", "volume", "near", "far")


def slot_text(slot):
    words = [getattr(slot, n) & 0xFFFFFFFF for n in SLOT_NAMES]
    words += list(slot.track_words) + list(slot.pos)
    return ",".join("%x" % w for w in words)


def gen_sound(out, rng):
    for round_ in range(3):
        pool = RC.HandlePool()
        model = R.SoundSeqs(pool)
        out.append("S new %x" % round_)
        seqs = []
        for k in range(12):
            seq = 0x400000 + 0x100 * k
            group = rng.choice((0, 0, 1, 2, 3, 5))
            model.groups[seq] = group
            seqs.append(seq)
            out.append("S group %x %x" % (seq, group))
        for g in range(1, 8):
            cap = rng.choice((0, 1, 2, 3, 6))
            model.limits[g] = cap
            out.append("S limit %x %x" % (g, cap))
        model.serial = rng.randrange(0x80)
        out.append("S serial %x" % model.serial)
        for i, slot in enumerate(model.slots):
            slot.capacity = rng.choice((1, 1, 1, 1, 0))
            if rng.random() < 0.6:
                slot.tracks, slot.seq = 1, rng.choice(seqs)
            out.append("S slot %x %x %x %x" % (i, slot.capacity, slot.tracks, slot.seq))
        objects = []
        for k in range(10):
            obj = Obj(k, 0x500000 + 0x400 * k)
            obj.position = tuple(rng.getrandbits(32) for _ in range(3))
            objects.append(obj)
            out.append("S obj %x %08x %08x %08x" % (k, *obj.position))
        cells = [s.handle_head for s in model.slots] + [c for o in objects for c in o.heads]
        ident = lambda thing: thing.ident
        finish = rng.choice((0.02, 0.1, 0.25))
        for _ in range(400):
            roll = rng.random()
            if roll < finish:
                i = rng.randrange(R.SEQ_SLOTS)
                model.slots[i].tracks = 0
                out.append("S finish %x" % i)
                continue
            if roll < finish + 0.05:
                k = rng.randrange(len(objects))
                for cell in objects[k].heads:
                    pool.clear(cell)
                out.append("S kill %x" % k)
                out.append(dump_pool("SP", pool, cells, ident))
                continue
            if roll < finish + 0.09:
                i = rng.randrange(R.SEQ_SLOTS)
                link = 0x600000 + rng.randrange(0x1000) * 0x20
                model.slots[i].track_link = link
                out.append("S cut %x %x" % (i, link))
                continue
            seq = rng.choice(seqs) if rng.random() < 0.97 else 0
            k = rng.randrange(len(objects)) if rng.random() < 0.7 else -1
            frozen = rng.choice((0, 0, 1))
            volume = RC.f32(rng.uniform(0, 2))
            model.frozen = frozen
            links_before = [s.track_link for s in model.slots]
            handle, chosen = model.play_on_object(seq, objects[k] if k >= 0 else None, volume)
            cut = 0
            if chosen is not None and links_before[chosen] and not model.slots[chosen].track_link:
                cut = links_before[chosen]
            out.append("S play %x %d %x %08x %08x %d %x" % (
                seq, k, frozen, volume, handle & 0xFFFFFFFF, -1 if chosen is None else chosen, cut))
            out.append("SS %x %s" % (model.serial, " ".join(slot_text(s) for s in model.slots)))
            out.append(dump_pool("SP", pool, cells, ident))


def gen_charm(out, rng):
    for round_ in range(3):
        pool = RC.HandlePool()
        out.append("C new %x" % round_)
        creatures = []
        for k in range(16):
            flags = rng.choice((0x20, 0x20, 0x20, 0x60, 0x0, 0x28))
            c = R.Creature(0x700000 + 0x400 * k, flags)
            c.ident = k
            creatures.append(c)
            out.append("C creature %x %x" % (k, flags))
        lists = [RC.Cell(0x7F0000 + 4 * j) for j in range(4)]
        for c in creatures:
            c.target.next = None
        cells = [cell for c in creatures for cell in c.heads + [c.target]] + lists
        ident = lambda thing: thing.ident

        for _ in range(400):
            roll = rng.random()
            c = rng.choice(creatures)
            if roll < 0.45:
                target = rng.choice(creatures)
                first = rng.random() < 0.85
                if first:
                    pool.clear(c.target)
                pool.link(c.target, target.heads[0], target)
                out.append("C take %x %x %d" % (c.ident, target.ident, int(first)))
            elif roll < 0.55:
                j, target = rng.randrange(4), rng.choice(creatures)
                pool.link(lists[j], target.heads[0], target)
                out.append("C list %x %x" % (j, target.ident))
            elif roll < 0.62:
                victim = rng.choice(creatures)
                for cell in victim.heads:
                    pool.clear(cell)
                out.append("C kill %x" % victim.ident)
            else:
                charm = rng.choice((0, 0, 0xFFFF, 120, 5))
                c.charm = charm
                frames, team = rng.choice((0xFFFF, 600, 1)), rng.choice((0, 1, 2, 0x81))
                stored = R.set_charm(pool, c, frames, team)
                out.append("C charm %x %x %x %x %d %x %x" % (c.ident, charm, frames, team,
                                                              int(stored), c.team, c.charm))
            out.append(dump_pool("CP", pool, cells, ident))


def build(research):
    global R, RC
    with tempfile.TemporaryDirectory() as tmp:
        stage(research, Path(tmp))
        sys.path.insert(0, tmp)
        try:
            RC = importlib.import_module("ref_core")
            R = importlib.import_module("ref_strings")
        finally:
            sys.path.remove(tmp)
    lines = ["# strings vectors from research %s tools/ref_strings.py" % COMMIT]
    counts = {}
    for index, seed in enumerate(SEEDS):
        for name, fn in (("hash", gen_hash), ("tables", gen_tables), ("helpers", gen_helpers),
                         ("formats", lambda o, r: gen_formats(o, r, True)),
                         ("formats_ascii", lambda o, r: gen_formats(o, r, False, 800)),
                         ("hud", lambda o, r: gen_hud(o, r, index)),
                         ("sound", gen_sound), ("charm", gen_charm)):
            rng = random.Random("%s/%d" % (name, seed))
            before = len(lines)
            lines.append("# section %s seed %d" % (name, seed))
            fn(lines, rng)
            counts["%s/%d" % (name, seed)] = len(lines) - before
    lines.append("END")
    return "\n".join(lines) + "\n", counts


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--research-dir", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--manifest", required=True)
    args = parser.parse_args()
    text, counts = build(Path(args.research_dir).expanduser())
    raw = text.encode("utf-8")
    Path(args.out).write_bytes(gzip.compress(raw, compresslevel=9, mtime=0))
    manifest = {
        "schema": "rta-strings-v1",
        "reference_commit": COMMIT,
        "pins": dict(sorted(list(PINS.items()) + list(DOC_PINS.items()))),
        "seeds": list(SEEDS),
        "generator_sha256": sha(Path(__file__).read_bytes()),
        "vectors_sha256": sha(raw),
        "lines": counts,
    }
    Path(args.manifest).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print("%d lines, sha256 %s" % (text.count("\n"), manifest["vectors_sha256"]))


if __name__ == "__main__":
    main()
