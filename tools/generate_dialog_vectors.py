#!/usr/bin/env python3
"""Port the NPC dialog machine reference as exact, tick-by-tick vectors.

Reads tools/ref_dialog.py and tools/ref_core.py from t3/research-dialog at
334578d and drives them through the ten suite shapes of
tools/difftest_dialog.py (message, open, messages, tick, draw, free, interact,
builtins, mouth, sequence) with seeds 20973 and 7, plus .dbin and message-0
round trips. Every input is synthetic: no disc, no executable, no game text.

Each operation is followed by what the reference did: its result (or a
refusal), the log of calls outside the dialog state, the scripts it ran, the
inputs it asked for, and the fields of its state that changed. The C++ test
(native/tests/dialog_tests.cpp) replays the operations on the native port and
compares all of it.

  python3 tools/generate_dialog_vectors.py --research-dir DIR \
      --tsv native/tests/dialog_vectors.tsv --contract native/tests/dialog_contract.json
"""
import argparse
import hashlib
import json
import random
import subprocess
import sys
from pathlib import Path

COMMIT = "334578d"
POLICY = "rta-dialog-v1"
FILES = (
    "docs/dialog.md",
    "tools/ref_dialog.py",
    "tools/ref_core.py",
    "tools/test_ref_dialog.py",
    "tools/difftest_dialog.py",
    "tools/dialog_census.py",
)
SEEDS = (20973, 7)
CASES = 60
SEQUENCE_CASES = 24


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


# ---- text of values (native/src/dialog.cpp format_event) -------------------------------

def esc(text):
    out = []
    for c in text.encode("latin1"):
        if c in b"% \t,[]" or c < 0x21 or c > 0x7E:
            out.append("%%%02x" % c)
        else:
            out.append(chr(c))
    return "".join(out)


def arg(v):
    if isinstance(v, bool):
        return str(int(v))
    if isinstance(v, int):
        return str(v)
    if isinstance(v, str):
        return "s:" + esc(v)
    if isinstance(v, (bytes, bytearray)):
        return "b:" + bytes(v).hex()
    if isinstance(v, (list, tuple)):
        return "[" + ",".join(str(int(x)) for x in v) + "]"
    raise TypeError("log value %r" % (v,))


def event(t):
    return " ".join([t[0]] + [arg(v) for v in t[1:]])


def csv(values):
    return ",".join(str(int(v)) for v in values)


def fnv(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return "%016x" % h


class Out:
    def __init__(self):
        self.lines = []

    def emit(self, *fields):
        self.lines.append("\t".join(str(f) for f in fields))


# ---- the run -------------------------------------------------------------------------

class Runner:
    """One case: a reference Dialog and Host, objects with synthetic addresses, and the
    last emitted snapshot so only changed fields are written."""

    def __init__(self, R, C, out, suite, ident):
        self.R, self.C, self.out = R, C, out
        self.host = R.Host()
        self.d = R.Dialog()
        self.objs = []
        self.last = {}
        self.last_objs = {}
        self.next_addr = 0x00100000
        self.nodes = None
        out.emit("case", suite, ident)

    def s32(self, v):
        return self.C.s32(v)

    # -- setup lines --
    def place(self, obj):
        obj.addr = self.next_addr
        self.next_addr += 0x1000
        self.objs.append(obj)
        props = ";".join("%s=%s" % (esc(k), esc(v)) for k, v in sorted(obj.props.items()))
        self.out.emit("obj", obj.addr, csv(obj.pos), obj.facing, obj.flags, int(obj.npc), obj.word24,
                      obj.anim_count, obj.anim_cur, obj.display_name, obj.flag20_track, csv(obj.tracks),
                      props or "-")
        return obj

    def host_lines(self):
        h, o = self.host, self.out
        o.emit("host", "network", int(h.network))
        o.emit("host", "local", csv(sorted(h.local)) or "-")
        o.emit("host", "num_players", h.num_players)
        o.emit("host", "alive", csv(h.alive) or "-")
        o.emit("host", "confirm_mask", h.confirm_mask)
        o.emit("host", "recall", h.recall)
        o.emit("host", "mission_menu", int(h.mission_menu))
        o.emit("host", "pause_menu", int(h.pause_menu))
        o.emit("host", "shop_open", h.shop_open)
        o.emit("host", "layout_height", h.layout_height)
        o.emit("host", "pads", ",".join("%d:%d" % kv for kv in sorted(h.raw_pad.items())) or "-")
        o.emit("host", "players", ",".join("%d:%d" % (k, v.addr if v is not None else 0)
                                            for k, v in sorted(h.player_obj.items())) or "-")
        for name, found in sorted(h.objects.items()):
            o.emit("objlist", esc(name), csv(x.addr for x in found) or "-")
        for k, v in sorted(h.scripts.items()):
            o.emit("script", esc(k), v)
        for k, v in sorted(h.texts.items()):
            o.emit("text", esc(k), csv(v) or "-")
        for (a, role), v in sorted(h.role_anims.items()):
            o.emit("role", a, esc(role), v)
        for k, v in sorted(h.anm_length.items()):
            o.emit("anm_length", esc(k), v)
        for k, v in sorted(h.vag_length.items()):
            o.emit("vag_length", esc(k), v)
        for k, (a, b) in sorted(h.anm_entry.items()):
            o.emit("anm_entry", esc(k), a, b)

    def set_host(self, key, value):
        """A host field that changes between operations."""
        h = self.host
        if key == "pads":
            h.raw_pad = dict(value)
            self.out.emit("host", "pads", ",".join("%d:%d" % kv for kv in sorted(h.raw_pad.items())) or "-")
            return
        setattr(h, key, value)
        self.out.emit("host", key, int(value))

    def file(self, nodes):
        """The conversation as fields; the test builds it with build_dbin, parses it
        back and compares the bytes' FNV-1a with the reference's build_dbin."""
        self.nodes = nodes
        data = self.R.build_dbin(nodes)
        self.out.emit("file", len(nodes), fnv(data))
        for x in nodes:
            self.out.emit("node", esc(x.condition) or "-", esc(x.text) or "-", esc(x.voice) or "-",
                          esc(x.speaker) or "-", x.raw_choices)
            for c in x.choices:
                self.out.emit("choice", esc(c.text) or "-", esc(c.condition) or "-", esc(c.action) or "-", c.next)

    def plant(self, **fields):
        """Assign Dialog fields directly (the difftest's perturbations)."""
        d, parts = self.d, []
        for k, v in fields.items():
            if k == "node":
                d.node = v
                parts.append("node=%s" % ("none" if v is None else v))
            elif k == "focus":
                d.focus = v
                parts.append("focus=%d" % (v.addr if v is not None else 0))
            elif k == "next_world":
                d.next_world = v
                parts.append("next_world=%s" % (esc(v) if v else "-"))
            elif k.startswith("buf"):
                i, what = int(k[3]), k[5:]
                d.buffers[i][what] = v
                parts.append("%s=%d" % (k, v))
            else:
                setattr(d, k, v)
                parts.append("%s=%d" % (k, v))
        self.out.emit("dialog", " ".join(parts))

    def objflags(self, obj, flags):
        obj.flags = flags
        self.out.emit("objflags", obj.addr, flags)

    # -- expectations --
    def snap(self):
        d, s32 = self.d, self.s32
        out = {}
        out["node"] = "none" if d.node is None else str(d.node)
        for k in ("node_index", "prompt_node", "prompt_shown", "revealing", "reveal_done", "reveal",
                  "at_end", "more", "cursor", "cursor_target", "scroll", "scroll_target", "total",
                  "cam_move", "closing", "end_sent", "drawn", "swoop", "side", "remote", "name_len",
                  "queue_mark", "idle_anim", "mutex", "swoop_done", "active_player", "focus_word",
                  "store_pending", "text_len", "hold", "anm_ready", "buf_sel", "cur_buf", "talking",
                  "hud_suppressed", "shown", "last_position"):
            out[k] = str(s32(getattr(d, k)))
        out["blend"] = str(d.blend)
        out["span"] = str(d.span)
        out["nudge"] = str(d.nudge)
        out["visible"] = csv(d.visible) or "-"
        out["choice_y"] = csv(s32(v) for v in d.choice_y)
        out["text"] = csv(d.text) or "-"
        out["name"] = csv(d.name) or "-"
        out["next_world"] = esc(d.next_world) if d.next_world else "-"
        out["buffers"] = "/".join("%d:%d:%d:%d" % (1 if b["ptr"] else 0, b["has"], s32(b["size"]), s32(b["vag"]))
                                  for b in d.buffers)
        out["focus"] = str(d.focus.addr if d.focus is not None else 0)
        out["store_index"] = "none" if d.store_index is None else str(d.store_index)
        out["pending_world"] = "none" if d.pending_world is None else (esc(d.pending_world) or "-")
        out["orient"] = "none" if d.orient is None else str(d.orient)
        out["nodes"] = "none" if d.nodes is None else str(len(d.nodes))
        return out

    def obj_snap(self):
        out = {}
        for o in self.objs:
            store = "-" if o.track_store is None else "%d:%d" % o.track_store
            out[o.addr] = "%d:%d:%d:%s:%s" % (o.flags, o.anim_count, o.anim_cur, store, csv(o.tracks))
        return out

    def expect(self, result=None, refused=None, io=None, ran=True):
        d, o = self.d, self.out
        if refused is not None:
            o.emit("refused")
        else:
            o.emit("ok", result)
        o.emit("log", *[event(t) for t in d.log]) if d.log else o.emit("log")
        if ran:
            o.emit("ran", *[esc(x) for x in self.host.ran]) if self.host.ran else o.emit("ran")
        if io is not None:
            o.emit("asked", *[x[0] if len(x) == 1 else "%s:%d" % x for x in io.asked]) if io.asked else o.emit("asked")
        now = self.snap()
        delta = [(k, v) for k, v in now.items() if self.last.get(k) != v]
        o.emit("state", " ".join("%s=%s" % kv for kv in delta))
        self.last = now
        objs = self.obj_snap()
        for addr, v in objs.items():
            if self.last_objs.get(addr) != v:
                o.emit("objstate", addr, v)
        self.last_objs = objs
        d.log = []
        self.host.ran = []

    def op(self, fields, fn, io=None):
        """Emit the operation, run it on the reference, emit what happened. Returns
        (ok, value)."""
        self.out.emit(*fields)
        try:
            value = fn()
        except ValueError:
            self.expect(refused=True, io=io)
            return False, None
        self.expect(result=self.result(value), io=io)
        return True, value

    def result(self, value):
        if value is None:
            return "-"
        if isinstance(value, bool):
            return str(int(value))
        if isinstance(value, int):
            return str(value)
        if isinstance(value, str):
            return esc(value)
        if isinstance(value, (bytes, bytearray)):
            return bytes(value).hex() or "-"
        if isinstance(value, list):           # messages sent by a tick
            return ",".join(bytes(m).hex() for m in value) or "-"
        if isinstance(value, tuple):          # talk_anim's store
            return "%d:%d" % value
        raise TypeError(value)

    # -- operations --
    def io_fields(self, io):
        C = self.C
        return ("tick", csv(io._camera), io.look, csv(io._held), csv(io._queue), csv(io._pos),
                csv(io._status), io.fade, io.text_option, io.shown)

    def tick(self, io):
        return self.op(self.io_fields(io), lambda: self.d.tick(self.host, io), io=io)

    def message(self, msg):
        msg = bytes(msg)
        return self.op(("msg", msg.hex()), lambda: self.d.on_message(self.host, msg, self.nodes))

    def draw(self, fade, opt, glyph, status):
        io = self.R.TickIO(stream_status=[status])
        ok, ops = self.op(("draw", fade, opt, glyph, status),
                          lambda: (self.d.draw(self.host, fade, opt, glyph, io.stream_status), None)[1], io=io)
        return ok


# ---- scenario builders (difftest_dialog.py shapes, without the emulator) --------------

def wide_text(R, rng, n):
    units = []
    for _ in range(n):
        r = rng.random()
        units.append(R.CARET if r < 0.05 else (R.SPACE if r < 0.2 else rng.randrange(0x41, 0x7B)))
    return units


def make_conversation(R, rng, host, nodes_n=None):
    n = nodes_n or rng.randint(1, 7)
    nodes = []
    for k in range(n):
        choices = []
        for i in range(rng.choice([0, 1, 1, 2, 2, 3, 4, 8, 10])):
            nxt = rng.choice([-1, -1] + list(range(n)))
            choices.append(R.Choice("ch%d_%d" % (k, i),
                                    rng.choice(["", "", "cc%d_%d" % (k, i), "nosuch%d" % i]),
                                    rng.choice(["", "act%d_%d" % (k, i), "missing%d" % i]), nxt))
        cond = rng.choice(["", "", "nc%d" % k, "gone%d" % k]) if k else rng.choice(["", "nc0"])
        nodes.append(R.Node(cond, "tx%d" % k, "v%d" % rng.randrange(3), "", choices))
        host.texts["tx%d" % k] = wide_text(R, rng, rng.randint(0, 120))
        for i in range(len(choices)):
            host.texts["ch%d_%d" % (k, i)] = wide_text(R, rng, rng.randint(1, 20))
            host.scripts["cc%d_%d" % (k, i)] = rng.choice([0, 0, 1, 2, -5])
            host.scripts["act%d_%d" % (k, i)] = rng.choice([0, 1])
        host.scripts["nc%d" % k] = rng.choice([0, 0, 0, 1, 3])
    for v in range(3):
        if rng.random() < 0.7:
            host.anm_length["v%d.anm" % v] = rng.randrange(0, 0x3000)
        host.vag_length["v%d.vag" % v] = rng.randrange(0, 0x20000)
        if rng.random() < 0.5:
            off = rng.randrange(0, 0x100000)
            host.anm_entry["v%d.anm" % v] = (off, off + rng.randrange(0, 0x3000))
    host.texts["Affirmation"] = [0x4F, 0x4B]
    host.texts["dialogMore"] = [0x4D, 0x6F]
    host.texts["nm"] = wide_text(R, rng, rng.randint(0, 14))
    host.layout_height = rng.choice([0, 19, 38, 57, 95, 133, 190, 400])
    return nodes


def make_world(R, C, rng, run, name="npc"):
    host = run.host
    count = rng.choice([1, 1, 2, 3])
    found = []
    for _ in range(count):
        o = R.Object(0, x=rng.uniform(-400, 400), y=rng.uniform(-400, 400), z=rng.uniform(-50, 50),
                     facing=rng.randrange(0x10000), flags=rng.choice([0x20, 0x20, 0x1100008, 0]),
                     npc=rng.random() < 0.7, word24=rng.randrange(1, 1 << 30),
                     props={"showname": "nm"}, anim_count=rng.randint(0, 3),
                     anim_cur=rng.choice([0, 5, 7]))
        run.place(o)
        found.append(o)
        host.role_anims[(o.addr, "dialogidle")] = rng.choice([0, 7])
        host.role_anims[(o.addr, "idle")] = rng.choice([5, 9])
    host.objects[name] = found
    p = R.Object(0, x=rng.uniform(-400, 400), y=rng.uniform(-400, 400), flags=rng.choice([0, 0, 8]))
    run.place(p)
    host.player_obj = {0: p if rng.random() < 0.9 else None}
    host.num_players = 1
    return found


def open_both(R, C, out, suite, ident, rng):
    run = Runner(R, C, out, suite, ident)
    nodes = make_conversation(R, rng, run.host)
    run.host.network = rng.random() < 0.3
    run.host.local = {0} if rng.random() < 0.85 else {1}
    make_world(R, C, rng, run)
    run.host_lines()
    run.file(nodes)
    cam = [C.f32(rng.uniform(-1000, 1000)) for _ in range(3)]
    msg = R.start_message("npc", 0, rng.choice([0, 1]), rng.choice([0, 1, 2, -1]), cam,
                          rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000))
    run.plant(active_player=0, anm_ready=1)
    run.expect(result="-")
    ok, _ = run.message(msg)
    return run, ok


def suite_message(R, C, out, rng, cases, tag):
    for n in range(cases):
        run = Runner(R, C, out, "message", "%s/%d" % (tag, n))
        run.host.local = {0} if rng.random() < 0.8 else {1}
        run.host_lines()
        name = "".join(rng.choice("abcdefghij_0123") for _ in range(rng.randint(0, 40)))
        a, b = rng.randrange(-200, 300), rng.randrange(-200, 300)
        cam = [C.f32(rng.uniform(-3000, 3000)) for _ in range(3)]
        yaw, pitch = rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000)
        is_open = rng.random() < 0.15
        ap = rng.choice([0, 0, 1])
        run.plant(active_player=ap, node=0 if is_open else None, swoop_done=1)
        run.expect(result="-")
        run.op(("start", esc(name) or "-", a, b, csv(cam), yaw, pitch),
               lambda: run.d.start(run.host, name, a, b, cam, yaw, pitch))
        # what message 0 reads back
        msg = R.start_message(name, ap, a, b, cam, yaw, pitch)
        m = R.parse_start_message(msg)
        out.emit("startmsg", esc(name) or "-", ap, a, b, csv(cam), yaw, pitch, msg.hex(),
                 esc(m["name"]) or "-", m["player"], m["a"], m["b"], csv(m["cam"]), m["yaw"], m["pitch"])


def suite_open(R, C, out, rng, cases, tag):
    for n in range(cases):
        open_both(R, C, out, "open", "%s/%d" % (tag, n), rng)
    for recall, menu in ((5, False), (0, True)):
        run = Runner(R, C, out, "open", "%s/refusal-%d-%d" % (tag, recall, int(menu)))
        nodes = make_conversation(R, rng, run.host, 2)
        make_world(R, C, rng, run)
        run.host.recall, run.host.mission_menu = recall, menu
        run.host_lines()
        run.file(nodes)
        run.plant(mutex=1)
        run.expect(result="-")
        run.message(R.start_message("npc", 0, 1, 0, (0, 0, 0), 0, 0))
    for count in (10, 11, 12):
        run = Runner(R, C, out, "open", "%s/overlap-%d" % (tag, count))
        run.host.texts = {"t": [0x41] * 30, "nm": [], "c": [0x42]}
        o = run.place(R.Object(0, flags=0x20, props={"showname": "nm"}))
        run.host.objects["npc"] = [o]
        run.host_lines()
        run.file([R.Node("", "t", "v", "", [R.Choice("c", "", "", -1) for _ in range(count)])])
        run.plant(anm_ready=1)
        run.expect(result="-")
        run.message(R.start_message("npc", 0, 0, 0, (0, 0, 0), 0, 0))
    # no object of that name: the original reads address 0xc
    run = Runner(R, C, out, "open", "%s/no-speaker" % tag)
    run.host.texts = {"t": [0x41], "nm": []}
    run.host_lines()
    run.file([R.Node("", "t", "v", "", [])])
    run.message(R.start_message("npc", 0, 0, 0, (0, 0, 0), 0, 0))


def suite_messages(R, C, out, rng, cases, tag):
    for n in range(cases):
        run, ok = open_both(R, C, out, "messages", "%s/%d" % (tag, n), rng)
        if not ok:
            continue
        d = run.d
        for step in range(rng.randint(1, 6)):
            f = {}
            f["revealing"] = rng.choice([0, 1])
            f["reveal_done"] = rng.choice([0, 1])
            f["at_end"] = rng.choice([0, 1, 1])
            f["more"] = rng.choice([0, 0, 1])
            f["blend"] = rng.choice([C.ONE, C.ONE, C.ZERO, C.f32(0.5)])
            f["scroll"] = rng.choice([0, 5, 76, d.total])
            f["scroll_target"] = rng.choice([f["scroll"], f["scroll"] + 5])
            if d.visible:
                f["cursor"] = rng.randrange(len(d.visible))
            f["closing"] = rng.choice([0, 0, 1])
            f["next_world"] = rng.choice(["", "", "lvl%d" % step])
            f["store_pending"] = rng.choice([0, 0, 1])
            f["anm_ready"] = 1
            run.plant(**f)
            run.expect(result="-")
            kind = rng.choice(["confirm", "confirm", "move", "end", "leave"])
            if kind == "confirm":
                msg = [R.MSG_CONFIRM]
            elif kind == "move":
                msg = [R.MSG_MOVE, rng.choice([-1, 1, 2, -3]) & 0xFF]
            elif kind == "end":
                msg = [R.MSG_END]
            else:
                msg = [R.MSG_LEAVE]
            ok, _ = run.message(msg)
            if not ok or d.node is None:
                break


def random_io(R, C, rng):
    return R.TickIO(camera=[rng.choice([0, 0, 1, 2]) for _ in range(4)] + [1],
                    look=rng.choice([C.f32(-0.3), C.f32(-0.55), C.f32(-0.9), C.ZERO]),
                    held_ready=[rng.choice([0, 1])], queue_count=[rng.choice([0, 1, 2])],
                    stream_position=[rng.choice([0, 0, 5, -1])], stream_status=[rng.choice([0, 0, 1])],
                    fade=rng.choice([C.ONE, C.ZERO, C.f32(0.36)]), text_option=rng.choice([1, 1, 0]),
                    shown=rng.randrange(0, 60))


def suite_tick(R, C, out, rng, cases, tag):
    for n in range(cases):
        run, ok = open_both(R, C, out, "tick", "%s/%d" % (tag, n), rng)
        if not ok:
            continue
        d, host = run.d, run.host
        run.set_host("pause_menu", rng.random() < 0.1)
        run.set_host("pads", {0: 0, 1: 0})
        for step in range(rng.randint(5, 40)):
            if rng.random() < 0.3:
                f = dict(cam_move=rng.choice([0, 1, -1]), swoop_done=rng.choice([0, 1]),
                         hold=rng.choice([0, 1]), anm_ready=rng.choice([0, 1, 1]),
                         talking=rng.choice([0, 1]), closing=rng.choice([0, 0, 1]),
                         end_sent=rng.choice([0, 1]), blend=rng.choice([C.ONE, C.ZERO, C.f32(0.9)]),
                         cursor_target=rng.randrange(max(len(d.visible), 1)))
                run.plant(**f)
                run.set_host("shop_open", rng.choice([0, 0, 1]))
                ob = host.player_obj.get(0)
                if ob is not None and rng.random() < 0.2:
                    run.objflags(ob, ob.flags ^ 8)
                run.expect(result="-")
            pads = {0: rng.choice([0, 0, host.confirm_mask, 0x1000, 0x4000, 0x100000, 0x200000,
                                   0x1000 | host.confirm_mask]),
                    1: rng.choice([0, host.confirm_mask])}
            run.set_host("pads", pads)
            ok, _ = run.tick(random_io(R, C, rng))
            if not ok:
                break


def suite_draw(R, C, out, rng, cases, tag):
    for n in range(cases):
        run, ok = open_both(R, C, out, "draw", "%s/%d" % (tag, n), rng)
        if not ok:
            continue
        d = run.d
        for step in range(rng.randint(1, 5)):
            f = dict(cam_move=rng.choice([0, 0, 0, 1]), drawn=rng.choice([0, 1]),
                     reveal_done=rng.choice([0, 1, 1]), revealing=rng.choice([0, 1]),
                     at_end=rng.choice([0, 1, 1]), more=rng.choice([0, 0, 1]), hold=rng.choice([0, 0, 1]),
                     closing=rng.choice([0, 1]), blend=rng.choice([C.ONE, C.ZERO, C.f32(rng.random())]))
            if d.visible:
                f["cursor"] = rng.randrange(len(d.visible))
                f["cursor_target"] = rng.randrange(len(d.visible))
            f["name_len"] = rng.randrange(0, len(d.name) + 1)
            f["scroll"] = rng.randrange(0, 300)
            f["active_player"] = rng.choice([0, 0, 1])
            run.plant(**f)
            run.expect(result="-")
            fade = C.f32(rng.choice([0.0, 0.12, 0.5, 0.96, 1.0]))
            opt = rng.choice([1, 1, 0])
            glyph = rng.randrange(1, 256)
            status = rng.choice([0, 0, 1])
            if not run.draw(fade, opt, glyph, status):
                break


def suite_free(R, C, out, rng, cases, tag):
    for n in range(cases):
        run, ok = open_both(R, C, out, "free", "%s/%d" % (tag, n), rng)
        if not ok:
            continue
        run.op(("free",), lambda: run.d.free())


def suite_interact(R, C, out, rng, cases, tag):
    for n in range(cases):
        run = Runner(R, C, out, "interact", "%s/%d" % (tag, n))
        host = run.host
        host.local = {0} if rng.random() < 0.8 else {1}
        props = {}
        if rng.random() < 0.8:
            props["dialogscript"] = rng.choice(["talk", "nosuch"])
        if rng.random() < 0.3:
            props["shop"] = rng.choice(["1", "0", "2", " 12x", "-3", "+0", "x"])
        obj = run.place(R.Object(0, x=rng.uniform(-200, 200), y=rng.uniform(-200, 200),
                                 facing=rng.randrange(0x10000), props=props,
                                 display_name=rng.choice([1, 1, 1, 0])))
        p = run.place(R.Object(0, x=rng.uniform(-200, 200), y=rng.uniform(-200, 200)))
        host.player_obj = {0: p if rng.random() < 0.9 else None}
        host.scripts = {"talk": 0}
        run.host_lines()
        mutex = rng.choice([0, 0, 1])
        flag = rng.choice([0, 1])
        run.plant(mutex=mutex, active_player=3)
        run.expect(result="-")
        if rng.random() < 0.3:
            run.op(("trigger", obj.addr), lambda: run.d.trigger(host, obj))
        else:
            run.op(("interact", obj.addr, 0, flag), lambda: run.d.interact(host, obj, 0, bool(flag)))


def suite_builtins(R, C, out, rng, cases, tag):
    for n in range(cases):
        run = Runner(R, C, out, "builtins", "%s/%d" % (tag, n))
        host, d = run.host, run.d
        host.network = rng.random() < 0.4
        host.local = rng.choice([{0}, {1}, {0, 1}])
        host.alive = rng.choice([[0], [0, 1], [1, 0]])
        host.num_players = 2
        host.recall = rng.choice([0, 0, 3])
        run.host_lines()
        first_active = rng.choice([0, 1])
        is_open = rng.random() < 0.5
        ap = rng.choice([0, 1])
        run.plant(node=0 if is_open else None, active_player=ap, mutex=1)
        run.expect(result="-")
        kind = rng.choice(["store", "menu", "swoop", "swoopdone", "next", "running", "release", "startDialog"])
        val = rng.choice([0, 3, 20, 21, -1, 7])
        a, b = rng.randrange(0, 3), rng.randrange(0, 3)
        if kind == "store":
            run.op(("store", val), lambda: d.activate_store(host, val))
        elif kind == "menu":
            run.op(("menu", first_active), lambda: d.activate_mission_menu(host, first_active))
        elif kind == "swoop":
            run.op(("swoop", val & 0xFFFFFFFF), lambda: d.change_swoop(val & 0xFFFFFFFF))
        elif kind == "swoopdone":
            run.op(("swoopdone",), lambda: d.start_with_swoop_done())
        elif kind == "next":
            run.op(("nextworld", "lvl%d" % n), lambda: d.new_world_post("lvl%d" % n))
        elif kind == "running":
            run.op(("running",), lambda: int(d.is_open))
        elif kind == "release":
            run.op(("release",), lambda: d.mutex_release())
        else:
            cam = [C.f32(i * 10.5) for i in range(3)]
            name = "npc%d" % (n % 7)
            run.op(("start", name, a, b, csv(cam), 0, 0), lambda: d.start(host, name, a, b, cam, 0, 0))


def suite_mouth(R, C, out, rng, cases, tag):
    for n in range(cases):
        run = Runner(R, C, out, "mouth", "%s/%d" % (tag, n))
        host, d = run.host, run.d
        f = R.Object(0, anim_count=rng.randint(0, 4))
        f.tracks = [C.f32(rng.uniform(0, 600)) for _ in range(8)]
        f.flag20_track = rng.choice([-1000000000, 0, 1, 2]) if f.anim_count else -1000000000
        if f.flag20_track >= 0 and f.flag20_track >= f.anim_count:
            f.flag20_track = 0
        run.place(f)
        host.role_anims[(f.addr, "talk")] = rng.choice([0, 11])
        run.host_lines()
        cur = rng.choice([0, 1])
        fields = dict(focus=f if rng.random() < 0.9 else None, cur_buf=cur)
        fields["buf%d_has" % cur] = rng.choice([0, 1])
        fields["buf%d_vag" % cur] = rng.randrange(0, 0x40000)
        fields["idle_anim"] = rng.choice([0, 9])
        fields["last_position"] = rng.choice([0, 0x5000])
        fields["nudge"] = rng.choice([C.ZERO, R.NUDGE])
        run.plant(**fields)
        run.expect(result="-")
        pos = rng.choice([d.last_position, 0x400, rng.randrange(0, 0x80000)])
        if rng.choice(["talk", "sync"]) == "talk":
            run.op(("talk",), lambda: d.talk_anim(host))
        else:
            run.op(("sync", pos), lambda: d.sync_mouth(host, lambda: pos))


def suite_sequence(R, C, out, rng, cases, tag):
    for n in range(cases):
        run, ok = open_both(R, C, out, "sequence", "%s/%d" % (tag, n), rng)
        if not ok:
            continue
        d, host = run.d, run.host
        run.set_host("pads", {0: 0})
        for t in range(400):
            run.set_host("pads", {0: rng.choice([0, 0, 0, host.confirm_mask, 0x1000, 0x4000])})
            io = R.TickIO(camera=[rng.choice([0, 1])], fade=C.f32(min(1.0, 0.12 * (t % 10))),
                          stream_status=[rng.choice([0, 0, 1])], held_ready=[1],
                          queue_count=[t % 3], stream_position=[t % 2], text_option=1,
                          shown=rng.randrange(0, 200))
            run.plant(anm_ready=1)
            run.expect(result="-")
            ok, sent = run.tick(io)
            if not ok:
                break
            stop = False
            for msg in sent:
                ok, _ = run.message(msg)
                if not ok:
                    stop = True
                    break
            if stop or d.node is None:
                break


def suite_files(R, C, out, rng, cases, tag):
    """.dbin round trips, the start candidates, the graph reach and the refusals."""
    import struct
    for n in range(cases):
        run = Runner(R, C, out, "files", "%s/%d" % (tag, n))
        nodes = make_conversation(R, rng, run.host)
        data = R.build_dbin(nodes)
        parsed = R.parse_dbin(data)
        out.emit("dbin", data.hex(), "ok", len(parsed), csv(R.start_candidates(parsed)) or "-",
                 csv(sorted(R.reachable_nodes(parsed))) or "-",
                 "|".join("%s/%s/%s/%s/%d/%s" % (esc(x.condition) or "-", esc(x.text) or "-",
                                                 esc(x.voice) or "-", esc(x.speaker) or "-", x.raw_choices,
                                                 ";".join("%s,%s,%s,%d" % (esc(c.text) or "-", esc(c.condition) or "-",
                                                                           esc(c.action) or "-", c.next)
                                                          for c in x.choices) or "-")
                          for x in parsed))
        # one corruption per file, each refused
        bad = bytearray(data)
        what = rng.choice(["size", "count", "choices", "jump", "negative"])
        if what == "size":
            bad = bad[:-1]
        elif what == "count":
            bad[0] = (bad[0] + 1) & 0xFF
        elif what == "choices":
            struct.pack_into("<i", bad, R.NODE_NCHOICES, 13)
        elif what == "negative":
            struct.pack_into("<i", bad, R.NODE_NCHOICES, -1)
        else:
            struct.pack_into("<i", bad, R.NODE_NCHOICES, 1)
            struct.pack_into("<i", bad, R.NODE_CHOICES + R.CH_NEXT, len(nodes))
        try:
            R.parse_dbin(bytes(bad))
            out.emit("dbin", bytes(bad).hex(), "ok?")
        except ValueError:
            out.emit("dbin", bytes(bad).hex(), "refused")
    out.emit("dbin", "-", "refused")
    for name, cond_results in (("first", [0, 1, 1]), ("none", [0, 0, 0]), ("missing", [None, 0, 1])):
        nodes = [R.Node("c0", "t", "v", "", []), R.Node("c1", "t", "v", "", []), R.Node("c2", "t", "v", "", [])]
        scripts = {"c%d" % i: v for i, v in enumerate(cond_results) if v is not None}
        ran = []

        def runner(entry):
            ran.append(entry)
            return scripts.get(entry, R.MISSING_ENTRY)
        start = R.pick_start(nodes, runner)
        out.emit("pick", ",".join("%s" % ("-" if v is None else v) for v in cond_results), start, ",".join(ran))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--research-dir", type=Path, required=True)
    parser.add_argument("--tsv", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    args = parser.parse_args()
    root = args.research_dir.resolve()
    missing = [name for name in FILES if not (root / name).is_file()]
    if missing:
        raise SystemExit("missing " + ", ".join(missing))
    commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if not commit.startswith(COMMIT):
        raise SystemExit("research HEAD %s is not %s" % (commit, COMMIT))
    tools = root / "tools"
    import dis  # noqa: F401  tools/dis.py shadows the standard library
    sys.path.append(str(tools))
    import ref_core as C  # noqa: E402
    import ref_dialog as R  # noqa: E402
    C.set_fpu("ps2")
    out = Out()
    out.emit("policy", POLICY, COMMIT)
    for seed in SEEDS:
        for suite in (suite_files, suite_message, suite_open, suite_messages, suite_tick, suite_draw,
                      suite_free, suite_interact, suite_builtins, suite_mouth, suite_sequence):
            rng = random.Random("%d/%s" % (seed, suite.__name__))
            cases = SEQUENCE_CASES if suite is suite_sequence else CASES
            suite(R, C, out, rng, cases, str(seed))
    out.emit("end")
    text = "\n".join(out.lines) + "\n"
    args.tsv.write_text(text)
    contract = {
        "commit": commit,
        "policy": POLICY,
        "seeds": list(SEEDS),
        "lines": len(out.lines),
        "sha256": {name: digest(root / name) for name in FILES},
        "vectors_sha256": hashlib.sha256(text.encode()).hexdigest(),
    }
    args.contract.write_text(json.dumps(contract, indent=2) + "\n")
    print("wrote %d lines" % len(out.lines))


if __name__ == "__main__":
    main()
