#!/usr/bin/env python3
"""Write native/tests/vectors/hud_r3.tsv from the pinned UI research references.

Reads only tools/ref_hud_dynamic.py and tools/ref_ui_composite.py at
t3/research-ui 1ed740a (no game files). The native test replays every row.

  python3 -I tools/generate_hud_r3_vectors.py
"""
import os
import pathlib
import sys

RESEARCH = os.environ.get(
    "CHAMPIONS_UI_RESEARCH",
    "../champions-re-wt/research-ui/tools")
sys.path.insert(0, RESEARCH)
import ref_core as F          # noqa: E402
import ref_hud_dynamic as H   # noqa: E402
import ref_ui_composite as C  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "native" / "tests" / "vectors" / "hud_r3.tsv"


def raw(bits):
    return "0x%08x" % (int(bits) & 0xffffffff)


def num(v):
    return str(int(v))


def flag(v):
    return "1" if v else "0"


def row(*fields):
    return "\t".join(str(f) for f in fields)


def main():
    lines = [
        "# hud-r3 synthetic vectors from ref_hud_dynamic.py and ref_ui_composite.py",
        "# research t3/research-ui 1ed740a2aed879eedad9fdc8a9ee0cceed8ad402",
    ]
    for mode in (0, 1, 2):
        for players in (1, 4, 0):
            words = H.minimap_params(mode, players)
            lines.append(row("minimap_params", mode, players, *[raw(w) for w in words]))
        extra = H.minimap_extra(mode)
        lines.append(row("minimap_extra", mode, raw(extra[0]), raw(extra[1])))
    cases = [
        (0x3f800000, 0x3f800000),
        (0, 0x3ba3d70a),
        (0, 0x3c000000),
        (0x41200000, 0),
        (0x3f000000, 0x40400000),
    ]
    for cur, tgt in cases:
        lines.append(row("minimap_approach", raw(cur), raw(tgt),
                         raw(H.minimap_approach(cur, tgt))))
    lines.append(row("minimap_zoom", raw(0x3f666666), raw(0x40000000),
                     raw(H.minimap_zoom(0x3f666666, 0x40000000))))
    for kind in range(-1, 5):
        got = H.minimap_player_blip(kind)
        if got is None:
            lines.append(row("minimap_blip", kind, "-", "-", "-"))
        else:
            lines.append(row("minimap_blip", kind, *[num(c) for c in got]))
    for health, frame in ((0, 0), (0, 0x20), (1, 0), (-1, 0), (10, 0x20)):
        lines.append(row("minimap_drawn", health, frame,
                         flag(H.minimap_player_drawn(health, frame))))
    for kind in (0, 1, 2, -1, 7):
        lines.append(row("flying_life", kind, num(H.flying_text_life(kind))))
    for word in (0, 29, 30, 59, 60, 90, 0xffffffff):
        lines.append(row("flying_drift", raw(word), num(H.flying_text_drift(word))))
    for start in (30, 180):
        life, alpha = start, 0x80
        for n in range(start):
            life, alpha, delete = H.flying_text_step(life, alpha)
            lines.append(row("flying_step", num(start), num(n), num(life), num(alpha),
                             flag(delete)))
    life, alpha, delete = H.flying_text_step(5, 2)
    lines.append(row("flying_step", 5, "alpha2", num(life), num(alpha), flag(delete)))
    for kind, crit in ((1, 1), (1, 0), (2, 0), (2, 1), (0, 0), (0, 1), (3, 1)):
        colour, force, lift = H.flying_text_style(kind, crit, 0x40)
        lines.append(row("flying_style", kind, flag(crit), 0x40, raw(colour),
                         flag(force), raw(lift)))
    z = H.flying_text_anchor_z(F.from_int(3), 0x42700000)
    lines.append(row("flying_z", raw(F.from_int(3)), raw(0x42700000), raw(z)))
    x, y = H.flying_text_screen(5, (-4) & 0xffffffff, 0x43000000, 0x42800000, 0x40000000)
    lines.append(row("flying_screen", raw(5), raw((-4) & 0xffffffff), raw(0x43000000),
                     raw(0x42800000), raw(0x40000000), raw(x), raw(y)))
    x, y = H.flying_text_screen(9, 0, 0, 0, F.ONE)
    lines.append(row("flying_screen", raw(9), raw(0), raw(0), raw(0), raw(F.ONE), raw(x), raw(y)))
    words = [0x3f800000, 0x40000000, 0x40400000, 0x40800000] + [0] * 12
    got = H.mat4_xyz(words, H.COMPASS_OFFSET)
    lines.append(row("mat4_xyz", *[raw(w) for w in words], *[raw(w) for w in H.COMPASS_OFFSET],
                     *[raw(w) for w in got]))
    camera = (0x3f800000, 0, 0x40000000)
    anchor = H.compass_anchor(words, camera)
    lines.append(row("compass_anchor", *[raw(w) for w in words], *[raw(w) for w in camera],
                     *[raw(w) for w in anchor]))
    lines.append(row("compass_label", *[num(v) for v in H.COMPASS_LABEL], raw(H.COMPASS_LABEL_COLOUR)))
    for delay in (-1, 0, 12, 13, 20, 100):
        lines.append(row("help_alpha", delay, num(H.help_button_alpha(delay)),
                         raw(H.help_button_colour(delay))))
    for height, width in ((0, 0), (40, 10), (21, -4), (100, 50)):
        box = H.help_box(height, width)
        lines.append(row("help_box", height, width, box["button"][0], box["button"][1],
                         box["label"][0], box["label"][1], box["frame_height"],
                         box["inner_height"]))
    for word, height in ((0, 10), (1, 0), (-1, 21)):
        column = H.notice_column(word)
        box = H.notice_box(column, height)
        lines.append(row("notice_box", word, height, column,
                         box["title"][0], box["title"][1],
                         *box["body"],
                         box["button"][0], box["button"][1],
                         box["label"][0], box["label"][1],
                         *box["frame"],
                         *box["rect"]))
    for t0, t2, t3, stack0 in ((0, 0, -1, -0x1c), (10, 5, 7, 0), (0, -4, -1, 8)):
        x, y, width, layer = H.message_box_placement(t0, t2, t3, stack0)
        lines.append(row("message_box", num(t0), num(t2), num(t3), num(stack0),
                         raw(x), raw(y), num(width), num(layer)))
    for pause, armed, menu, network, first in (
            (True, True, True, True, True),
            (False, False, True, False, True),
            (False, True, False, True, False),
            (False, True, True, True, False),
            (False, True, True, True, True),
            (False, True, True, False, False)):
        got = H.medal_prompt(pause, armed, menu, network, first)
        if got is None:
            lines.append(row("medal", flag(pause), flag(armed), flag(menu), flag(network),
                             flag(first), "0"))
        else:
            lines.append(row("medal", flag(pause), flag(armed), flag(menu), flag(network),
                             flag(first), "1",
                             got["title_key"] or "-", got["body_key"] or "-",
                             *[raw(v) if i < 2 else num(v) for i, v in enumerate(got["box"])],
                             num(got["list"][0]), num(got["list"][1])))
    for args in ((False, 1, True, False, 0, 1),
                 (True, 0, False, True, 5, 1),
                 (True, 0, True, False, 0, 2),
                 (True, 0, True, True, 0, 0),
                 (True, 1, False, True, 1, 0)):
        lines.append(row("net_cam", *[flag(a) if isinstance(a, bool) else num(a) for a in args],
                         flag(H.net_cam_show(*args))))
    for slot in (0, 1, 3):
        lines.append(row("net_pad", slot, num(H.net_cam_pad(slot))))
    lines.append(row("net_cam_text", num(H.NET_CAM_NAME[0]), num(H.NET_CAM_NAME[1]),
                     num(H.NET_CAM_PROMPT[0]), num(H.NET_CAM_PROMPT[1]),
                     raw(H.NET_CAM_PROMPT_COLOUR)))
    for bits in (0, 0x3f800000, 0x40000000, 0x40400000):
        lines.append(row("arena_active", raw(bits), flag(H.arena_player_active(bits))))
    for bits in (0, 0x3f800000, 0x40000000, 0x3f000000):
        for step in range(5):
            lines.append(row("arena_step", raw(bits), step,
                             flag(H.arena_step_shown(bits, step))))
    for step in range(5):
        a, b = H.arena_pulse_args(step)
        lines.append(row("arena_pulse", step, num(a), num(b)))
    for fade, hide in ((0, False), (0, True), (F.ONE, True), (F.ONE, False),
                       (0x3f000000, False), (0x3f000000, True)):
        lines.append(row("bust_fade", raw(fade), flag(hide), raw(H.bust_fade_step(fade, hide))))
    for fade in (0, F.ONE, 0x3f000000):
        text_a, sprite_a = H.bust_alphas(fade)
        lines.append(row("bust_alpha", raw(fade), num(text_a), num(sprite_a)))
    for value, maximum, width, tick in ((0, 4, 32, 0), (2, 4, 32, 2), (4, 4, 10, 7),
                                        (1, 3, 9, 8)):
        name, clip = H.bust_meter(value, maximum, width, tick)
        lines.append(row("bust_meter", value, maximum, width, tick, name, num(clip)))
    matrix = C.dimx_matrix()
    for r in range(4):
        for c in range(4):
            lines.append(row("dimx", r, c, num(matrix[r][c])))
    for channel, x, y in ((0, 0, 0), (128, 1, 0), (255, 2, 3), (3, 3, 1), (7, 0, 1)):
        lines.append(row("dither5", channel, x, y, num(C.dither5(channel, x, y))))
    for colour, factor in ((0x80aabbcc, F.ONE), (0x80404040, 0x3f000000), (0x00ffffff, 0)):
        lines.append(row("scale_rgb", raw(colour), raw(factor), raw(C.scale_rgb(colour, factor))))
    for r, g, b, bright in ((128, 0, 255, C.BRIGHTNESS_DEFAULT), (0, 0, 0, 0),
                            (255, 255, 255, F.ONE)):
        lines.append(row("present", r, g, b, raw(bright),
                         raw(C.present_colour(r, g, b, bright)),
                         raw(C.present_gain(bright))))
    for value in (0, 123, 124, 132, 133, 200):
        lines.append(row("present_step", value, num(C.present_channel_step(value))))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n")
    print("wrote %s (%d rows)" % (OUT, sum(1 for line in lines if not line.startswith("#"))))


if __name__ == "__main__":
    main()
