#!/usr/bin/env python3
"""Write native/tests/vectors/ui_text_gaps.tsv and native/tests/ui_text_gaps_contract.json.

Reads only tools/ref_ui_text_gaps.py and its imports at research
t3/research-ui-text-gaps (no game files). The native test replays every row.

  python3 -I tools/generate_ui_text_gaps_vectors.py

CHAMPIONS_UI_TEXT_RESEARCH points at that branch's tools/ directory.
"""
import hashlib
import json
import os
import pathlib
import random
import sys

RESEARCH = os.environ.get(
    "CHAMPIONS_UI_TEXT_RESEARCH",
    "../champions-re-wt/research-ui-text-gaps/tools")
sys.path.insert(0, RESEARCH)
import ref_anim as A          # noqa: E402
import ref_core as F          # noqa: E402
import ref_strings as RS      # noqa: E402
import ref_ui_text_gaps as R  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "native" / "tests" / "vectors" / "ui_text_gaps.tsv"
CONTRACT = HERE.parent / "native" / "tests" / "ui_text_gaps_contract.json"
COMMIT = "523b3db9f22525bd03c70b9ecb8ba252e7bd77fe"
SEEDS = (20973, 7)
M32 = 0xFFFFFFFF


def h(v):
    return "%08x" % (int(v) & M32)


def esc(data):
    """Letters, digits and . _ < > stay; every other byte is %xx (the C++ test does the same)."""
    return "".join(chr(b) if chr(b).isascii() and (chr(b).isalnum() or chr(b) in "._<>") else "%%%02x" % b
                   for b in data)


def canon(call):
    out = [call[0]]
    for a in call[1:]:
        if isinstance(a, tuple):
            for b in a:
                out.append(h(b))
        elif isinstance(a, (bytes, bytearray)):
            out.append("s:" + esc(bytes(a)))
        elif isinstance(a, str):
            out.append("s:" + esc(a.encode("latin-1")))
        else:
            out.append(h(a))
    return "\t".join(out)


def calls_field(calls):
    # calls are joined by '|' (no call text contains it); empty list -> '-'
    return "|".join(canon(c).replace("\t", "~") for c in calls) or "-"


def raws(values):
    return ",".join(h(v) for v in values) or "-"


def rnd_raw(rng):
    pool = [F.ZERO, F.ONE, F.TWO, 0x3F000000, 0x3F4CCCCC, 0x40400000, 0x3F800001, 0x3DF5C28F,
            0x3F75C28B, 0xBF800000, 0x40A00000]
    return rng.choice(pool) if rng.random() < 0.6 else F.from_int(rng.randrange(0, 300))


def gen(rng, lines):
    # 1. ramps
    for _ in range(80):
        pulse = [rnd_raw(rng) for _ in range(4)]
        fade = [rnd_raw(rng) for _ in range(4)]
        p, f = R.ui_ramps_step(pulse, fade)
        lines.append("\t".join(["ramps", raws(pulse), raws(fade), raws(p), raws(f)]))
    # 2. layout
    for b in (1, 0, 2, 0xFF, 0x80, 7):
        words = R.button_layout(b)
        s = ",".join("%x=%x" % (a, v) for a, v in sorted(words.items())) or "-"
        lines.append("\t".join(["layout", str(b), s]))
    for _ in range(60):
        mask = 1 << rng.randrange(0, 16)
        lines.append("\t".join(["padglyph", h(mask), h(R.pad_mask_to_glyph(mask))]))
    for _ in range(30):
        mask = rng.getrandbits(32)
        lines.append("\t".join(["padglyph", h(mask), h(R.pad_mask_to_glyph(mask))]))
    for word in (0x7E, 0x7B, 0, 0x17B, 0x100, 0xFF, 0x41):
        lines.append("\t".join(["glyphtext", h(word), R.glyph_text(word).hex() or "-"]))
    # 3. help task
    for _ in range(300):
        st = {'delay': rng.choice([0, 0, 1, 2, 5, 12, 13, 60, 90, -3]),
              'open': rng.choice([1, 1, 1, 0, 0x100]), 'net': rng.choice([0, 0, 1]),
              'paused': rng.choice([0, 1]), 'pause_byte': rng.choice([0, 1]),
              'local_menu': rng.choice([0, 1]), 'lockout': 0,
              'dl_free': rng.choice([5000, 3000, 2999, 0, -5]),
              'pulse': rng.choice([F.ONE, F.TWO, 0x3FCCCCCC, F.ZERO]),
              'title': rng.choice([0, 0x86AA70]), 'body': rng.choice([0, 0x86AA90]),
              'font_med': 0x4F7100, 'glyph_word': rng.choice([0x7E, 0x7B, 0, 0x1FF]),
              'num_players': rng.choice([0, 1, 2, 4]),
              'pads': [rng.choice([0, 0x40, 0x20, 0x60, 0xFFFF]) for _ in range(4)],
              'confirm_mask': rng.choice([0x40, 0x20]),
              'alive': rng.sample(range(0, 4), rng.randrange(0, 5)),
              'wrap_height': rng.randrange(0, 200), 'label_width': rng.randrange(-5, 180)}
        st['local'] = {s for s in st['alive'] if rng.random() < 0.5}
        calls, out = R.help_task(st)
        inp = ",".join(str(x) for x in [
            st['delay'], st['open'], st['net'], st['paused'], st['pause_byte'], st['local_menu'],
            st['dl_free'], h(st['pulse']), h(st['title']), h(st['body']), h(st['font_med']),
            h(st['glyph_word']), st['num_players'], h(st['confirm_mask']),
            st['wrap_height'], st['label_width']])
        pads = ",".join(h(p) for p in st['pads'])
        alive = ",".join(str(s) + ("L" if s in st['local'] else "r") for s in st['alive']) or "-"
        res = ",".join(str(x) for x in [out['delay'], out['open'] & 0xFF, out['paused'], out['pause_byte'],
                                         out.get('lockout', 0)])
        lines.append("\t".join(["help", inp, pads, alive, res, calls_field(calls)]))
    for delay in range(-2, 20):
        lines.append("\t".join(["helpalpha", str(delay), str(R.help_button_alpha(delay))]))
    for wh in (0, 21, 42, 200):
        draws = ";".join("%s,%d,%d,%s" % (d.resource, d.x, d.y, "-" if d.clip is None else "/".join(map(str, d.clip)))
                         for d in R.help_inner_frame(wh))
        lines.append("\t".join(["helpinner", str(wh), draws]))
    # 4. bust
    for _ in range(300):
        maximum = rng.choice([-1, -1, 0, 1, 10, 100, 12345, -5])
        value = rng.choice([-3, 0, 1, 5, 9, 10, 50, 100, 99999])
        sim = rng.choice([0, 1, 2, 3, 7, 8, 100, 12345, -1, -2, -7, -8])
        tex = {}
        for name in (b'frame.tex', b'<portrait>', b'fieldnumber.tex'):
            tex[name] = (rng.randrange(1, 160) | (0x10000 if rng.random() < 0.05 else 0), rng.randrange(1, 100))
        for n in (0, 1, 2, 3, -1, -2, -3):
            tex[b'bar_yellow0%d.tex' % n] = (rng.randrange(1, 130), rng.randrange(1, 30))
        st = {'shown': rng.choice([1, 1, 1, 0]), 'latch': rng.choice([0, 1]), 'fade': rng.choice(
            [F.ZERO, 0x3DACF3DF, 0x3F000000, F.ONE, 0x3F7C0000]),
            'value': value, 'maximum': maximum, 'sim_frame': sim, 'font_small': 0x4F7104, 'tex': tex}
        calls, out = R.hud_draw_bust(st)
        sizes = ";".join("%s=%d,%d" % (k.decode(), *v) for k, v in sorted(tex.items()))
        inp = ",".join(str(x) for x in [st['shown'], st['latch'], h(st['fade']), value, maximum, sim,
                                         h(st['font_small'])])
        res = ",".join([h(out['fade']), str(out['latch']), str(out['value'] if out['value'] < 0x80000000 else out['value'] - 2 ** 32)])
        lines.append("\t".join(["bust", inp, sizes, res, calls_field(calls)]))
    # 5. sprite quad
    for _ in range(400):
        frame = rng.choice([0, 1])
        fieldm = rng.choice([0, 1, 1])
        w, hh = rng.randrange(1, 300), rng.randrange(1, 200)
        x, y = rng.randrange(-20, 650), rng.randrange(-20, 460)
        u0, v0 = rng.randrange(0, 20), rng.randrange(0, 20)
        u1, v1 = u0 + rng.randrange(1, 300), v0 + rng.randrange(1, 200)
        fields = (rng.randrange(0, 2), rng.randrange(0, 2))
        qs = R.sprite_quad(frame, fieldm, w, hh, x, y, u0, v0, u1, v1, fields)
        lines.append("\t".join(["quad", ",".join(str(i) for i in (frame, fieldm, w, hh, x, y, u0, v0, u1, v1, *fields)),
                                str(R.field_calls(frame, fieldm, y)),
                                ",".join("%x:%x" % (v & 0xFFFFFFFFFFFFFFFF, r) for v, r in qs)]))
    for _ in range(100):
        hh = rng.randrange(1, 40)
        y = rng.randrange(-10, 40)
        line = rng.randrange(-15, 80)
        got = R.woven_line(line, y, hh)
        lines.append("\t".join(["woven", "%d,%d,%d" % (line, y, hh), "-" if got is None else "%d,%d" % got]))
    # 6. compass
    for _ in range(200):
        view = [rnd_raw(rng) if rng.random() < 0.3 else F.from_int(rng.randrange(0, 5)) for _ in range(16)]
        cam = [F.from_int(rng.randrange(0, 2000)) for _ in range(3)]
        pxy = [F.from_int(rng.randrange(0, 4000)) for _ in range(2)]
        txy = [F.from_int(rng.randrange(0, 4000)) for _ in range(2)]
        st = {'gate': rng.choice([1, 1, 0]), 'player': rng.choice([0, 1, -1]),
              'object': rng.choice([1, 1, 0]), 'dialog_open': rng.choice([0, 0, 1]),
              'shown': rng.choice([1, 1, 0]), 'view': view, 'camera': cam, 'player_xy': pxy,
              'target_xy': txy, 'flags': rng.choice([0, 8, 0x28]), 'light_flag': rng.choice([0, 1]),
              'light_depth': rng.choice([0, 10, 25]), 'font_small': 0x4F7104}
        calls, out = R.hud_draw_compass(st)
        inp = ",".join(str(x) for x in [st['gate'], st['player'], st['object'], st['dialog_open'], st['shown'],
                                         st['flags'], st['light_flag'], st['light_depth'], h(st['font_small'])])
        res = ",".join([h(out['flags']), str(out['light_flag']), str(out['light_depth']),
                        str(out.get('angle', 0))])
        lines.append("\t".join(["compass", inp, raws(view), raws(cam), raws(pxy), raws(txy), res, calls_field(calls)]))
    # 7. sound sequence update
    for _ in range(300):
        slots, views = [], []
        n = rng.randrange(1, 6)
        for _i in range(n):
            s = R.Slot()
            count = rng.choice([0, 1, 1, 1, 0x80])
            s.state.count = count
            nev = rng.randrange(0, 4)
            has = rng.random() < 0.8
            events = sorted(rng.randrange(0, 50) for _ in range(nev))
            clip = A.Clip(A.encode_clip([((0, 0, 0), (4096, 0, 0, 0))], 40,
                                        sound_times=events if has else None), 'x')
            if count:
                track = s.state.tracks[0]
                track.clip = clip
                track.sound_idx = rng.randrange(0, nev + 1)
                track.time = F.from_int(rng.randrange(0, 80))
            s.state.flags = rng.choice([0, 0, A.STATUS_ENDED, 4])
            s.released = rng.choice([0, 0, 1])
            s.duration = {id(clip): rng.randrange(0, 60)}
            s.while_frozen = rng.choice([0, 1])
            s.no_pos = rng.choice([0, 1])
            s.serial = rng.randrange(0, 70000)
            s.volume = rng.choice([F.ONE, 0x3F000000])
            obj = tuple(F.from_int(rng.randrange(0, 100)) for _ in range(3))
            if rng.random() < 0.5:
                s.handle, s.handle_obj = 1, obj
            s.pos = tuple(F.from_int(rng.randrange(0, 100)) for _ in range(3))
            slots.append(s)
            views.append((count, s.released, has, track.sound_idx if count else 0, events,
                          s.state.flags, s.duration[id(clip)], track.time if count else 0,
                          s.while_frozen, s.handle_obj, s.no_pos, s.pos, s.serial, s.volume))
        voices = [[rng.choice([0, 4, 0x10, 0x14, 0x1C]), rng.randrange(-1, n + 1), rng.randrange(0, 70000)]
                  for _ in range(rng.randrange(0, 8))]
        # voice generations that can match a slot serial
        for v in voices:
            if rng.random() < 0.6:
                j = rng.randrange(0, n)
                v[1], v[2] = j, slots[j].serial
        frozen = rng.choice([0, 1])
        cam = tuple(F.from_int(rng.randrange(0, 100)) for _ in range(3))
        orig = A.anim_advance
        A.anim_advance = lambda *a, **k: None
        try:
            try:
                events = R.sound_seq_update_all(None, slots, voices, frozen, cam)
            except A.HostFault:
                events = None
        finally:
            A.anim_advance = orig
        sdesc = ";".join(",".join(str(x) for x in [
            c & 0xFF, int(rel), int(has), idx & 0xFFFF, "/".join(str(e) for e in ev) or "-", fl, dur, h(t),
            int(wf), "-" if ho is None else "/".join(h(q) for q in ho), int(npos), "/".join(h(q) for q in pos),
            ser, h(vol)]) for (c, rel, has, idx, ev, fl, dur, t, wf, ho, npos, pos, ser, vol) in views)
        vdesc = ";".join("%x,%d,%d" % (f, o, g) for f, o, g in voices) or "-"
        if events is None:
            lines.append("\t".join(["seq", sdesc, vdesc, str(frozen), raws(cam), "fault"]))
            continue
        evs = ",".join("%s:%d:%s" % (e[0], e[1], h(e[2]) if e[0] == 'advance' else "0") for e in events) or "-"
        after = ";".join("%d,%s,%s" % (s.state.count & 0xFF, h(s.state.tracks[0].time if s.state.count else 0),
                                       "/".join(h(q) for q in s.pos)) for s in slots)
        lines.append("\t".join(["seq", sdesc, vdesc, str(frozen), raws(cam), evs, after]))
    # The chime's slot life (R.chime_frames) is not replayed: it needs anim_advance's sound
    # cursor, which Native Runtime refuses. Only the update loop above is ported.
    # 8. showName: quotes in the request are dropped before the lookup
    for _ in range(40):
        entries = [(b"key%d" % i + bytes(rng.choice(b"abcxyz") for _ in range(rng.randrange(0, 12))),
                    [rng.randrange(0x20, 0x7F) for _ in range(rng.randrange(1, 8))]) for i in range(rng.randrange(1, 5))]
        blob = bytearray()
        for key, units in entries:
            koff = len(blob)
            blob += key + b"\0"
            while len(blob) < ((koff + len(key) + 2) & ~1):
                blob += b"\0"
            for u in units:
                blob += u.to_bytes(2, "little")
            blob += b"\0\0"
        blob += b"\0\0"
        table = RS.TextTable()
        table.register(bytes(blob), 0x600000)
        steps = []
        for _s in range(rng.randrange(1, 14)):
            base = rng.choice([e[0] for e in entries] + [b"missing_key_that_is_long_enough", b"nope", b"", b"q"])
            req = bytearray(base)
            for _q in range(rng.randrange(0, 4)):
                req.insert(rng.randrange(0, len(req) + 1), ord('"'))
            req = bytes(req)
            units, length, hit = R.show_name(table, req)
            steps.append("%s,%d,%s,%d" % (req.hex() or "-", int(hit), "".join("%04x" % u for u in units) or "-", table.nf_index))
        lines.append("\t".join(["showname", bytes(blob).hex(), ";".join(steps)]))


def main():
    lines = ["# ui-text-gaps vectors from ref_ui_text_gaps.py, research t3/research-ui-text-gaps " + COMMIT]
    for seed in SEEDS:
        lines.append("# seed %d" % seed)
        gen(random.Random(seed), lines)
    OUT.write_text("\n".join(lines) + "\n")
    files = ["docs/ui-text-gaps.md", "docs/spec/ui-text-gaps.md", "tools/ref_ui_text_gaps.py",
             "tools/ref_core.py", "tools/ref_anim.py", "tools/ref_strings.py", "tools/ref_ui_composite.py",
             "tools/ref_ui_layout.py", "tools/ref_hud_dynamic.py", "tools/test_ref_ui_text_gaps.py",
             "tools/difftest_ui_text_gaps.py", "tools/ui_text_gaps_census.py"]
    root = pathlib.Path(RESEARCH).parent
    contract = {"commit": COMMIT, "policy": "rta-ui-text-gaps-v1", "seeds": list(SEEDS),
                "lines": len(lines),
                "sha256": {f: hashlib.sha256((root / f).read_bytes()).hexdigest() for f in files},
                "vectors_sha256": hashlib.sha256(OUT.read_bytes()).hexdigest()}
    CONTRACT.write_text(json.dumps(contract, indent=2) + "\n")
    print("wrote %d lines" % len(lines))


if __name__ == "__main__":
    main()
