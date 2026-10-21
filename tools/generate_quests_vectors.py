#!/usr/bin/env python3
"""Port the quest and mission model as exact vectors.

Reads ref_quests.py and the campaign operations in ref_natives.py from
t3/research-quests at 7efd3ce. No disc and no executable. The retail
experience table and the retail act and mission names are not copied.

The mission rows are difficulties 1..5 (+0x4bcc..+0x4c0c) and g_difficultyIdx
starts at 1, the executable image's value (t3/research-items 49bc578
docs/script-items.md section 3). Row 0 lies on the skill pairs, so no case
here runs at difficulty 0, and record dumps start at row 1.
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

COMMIT = "7efd3ce"
# Mission row 1, the first of the five rows (research-items 49bc578).
FIRST_DIFFICULTY = 1
IMAGE_DIFFICULTY = 1
FILES = (
    "docs/quests.md",
    "tools/ref_quests.py",
    "tools/test_ref_quests.py",
    "tools/difftest_quests.py",
    "tools/ref_natives.py",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(research):
    tools = research / "tools"
    # tools/dis.py shadows the standard library. Load dis first.
    import dis  # noqa: F401
    sys.path.append(str(tools))
    import ref_natives as natives  # noqa: E402
    import ref_quests as quests  # noqa: E402
    return natives, quests


class Script:
    def __init__(self):
        self.lines = []

    def emit(self, *fields):
        self.lines.append("\t".join("" if field is None else str(field) for field in fields))


def hex_field(data):
    data = bytes(data)
    if len(data) < 32:
        data += bytes(32 - len(data))
    return data[:32].hex()


def ascii(data):
    if isinstance(data, str):
        return data
    return bytes(data).split(b"\0", 1)[0].decode("ascii")


def pack_act(prefixes):
    return "|".join(prefixes)


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
    commit = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
    if not commit.startswith(COMMIT):
        raise SystemExit("research HEAD %s is not %s" % (commit, COMMIT))
    natives, quests = load(root)
    out = Script()
    out.emit("commit", COMMIT)

    def players(count, difficulty=IMAGE_DIFFICULTY):
        game = natives.Game()
        game.num_players = count
        game.difficulty = difficulty
        out.emit("reset")
        out.emit("set_players", count)
        out.emit("set_difficulty", difficulty)
        return game

    game = players(2)
    game.quest_add(b"Alpha", b"Body")
    out.emit("quest_add", "Alpha", "Body")
    game.quest_add(b"Alpha", b"Other")
    out.emit("quest_add", "Alpha", "Other")
    for slot in range(2):
        rec = game.rec(slot)
        out.emit("expect_count", slot, game.mem.s32(rec + natives.P_QUEST_COUNT))
        out.emit(
            "expect_line",
            slot,
            0,
            ascii(game.mem.cstr(rec + natives.P_QUESTS)),
            ascii(game.mem.cstr(rec + natives.P_QUESTS + 0x20)),
        )

    game = players(2)
    game.mem.w32(game.rec(0) + natives.P_QUEST_COUNT, 8)
    out.emit("plant_count", 0, 8)
    game.quest_add(b"Late", b"No")
    out.emit("quest_add", "Late", "No")
    out.emit("expect_count", 0, 8)
    out.emit("expect_count", 1, 0)

    game = players(2)
    game.quest_add(b"Kept", b"One")
    out.emit("quest_add", "Kept", "One")
    game.mem.w32(game.rec(1) + natives.P_QUEST_COUNT, 0)
    out.emit("plant_count", 1, 0)
    game.quest_add(b"Kept", b"Two")
    out.emit("quest_add", "Kept", "Two")
    out.emit("expect_count", 0, 1)
    out.emit("expect_count", 1, 0)
    out.emit("expect_line", 0, 0, "Kept", "One")

    game = players(1)
    long_title = b"ABCDEFGHIJKLMNOPQRSTUV"
    rec = game.rec(0)
    game.mem.strcpy(rec + natives.P_QUESTS, long_title)
    game.mem.strcpy(rec + natives.P_QUESTS + 0x20, b"body")
    out.emit(
        "plant_hex",
        0,
        0,
        game.mem.read(rec + natives.P_QUESTS, 32).hex(),
        game.mem.read(rec + natives.P_QUESTS + 0x20, 32).hex(),
    )
    game.mem.w32(rec + natives.P_QUEST_COUNT, 1)
    out.emit("plant_count", 0, 1)
    game.quest_add(b"Two", b"B")
    out.emit("quest_add", "Two", "B")
    game.quest_remove(long_title)
    out.emit("quest_remove", ascii(long_title))
    out.emit("expect_count", 0, game.mem.s32(rec + natives.P_QUEST_COUNT))
    for index in range(2):
        base = rec + natives.P_QUESTS + index * 0x40
        out.emit(
            "expect_hex",
            0,
            index,
            game.mem.read(base, 32).hex(),
            game.mem.read(base + 32, 32).hex(),
        )

    title = b"ABCDEFGHIJKLMNOPQRSTUV" + bytes(10)
    text = b"body-of-the-quest!!" + bytes(14)
    entries = [(title, text)] + [(b"later" + bytes(27), bytes(32))] * 7
    cleared, count = quests.clear_quest_list(entries, 1)
    out.emit("reset")
    out.emit("set_players", 1)
    out.emit("plant_hex", 0, 0, hex_field(title), hex_field(text))
    out.emit("plant_hex", 0, 1, hex_field(b"later" + bytes(27)), hex_field(bytes(32)))
    out.emit("plant_count", 0, 1)
    out.emit("clear_list", 0)
    out.emit("expect_count", 0, count)
    out.emit("expect_hex", 0, 0, hex_field(cleared[0][0]), hex_field(cleared[0][1]))
    out.emit("expect_hex", 0, 1, hex_field(cleared[1][0]), hex_field(cleared[1][1]))

    bit = 1 << 39
    items = [
        ("zzzz", bit),
        ("pickaxe", bit),
        ("zzzz", 0),
        ("sword", 1 << 38),
        ("bloodstonefay", bit),
        ("xxshardofyy", bit),
        ("Bloodstone", bit),
        ("pickaxe2", bit),
        ("spiritrunes", bit),
        ("emptybag", bit),
        ("fullbag", bit),
        ("twistedkey", bit),
        ("", bit),
    ]
    kept = quests.flag39_remaining(items)
    out.emit("reset")
    out.emit("set_players", 1)
    for name, flags in items:
        out.emit("item", 0, 0, name, flags)
    out.emit("flag39", 0, 0)
    for index, (name, flags) in enumerate(kept):
        out.emit("expect_item", 0, 0, index, name, flags)
    out.emit("expect_items", 0, 0, len(kept))

    table = [100 * i for i in range(80)]
    table.append(10000)
    cases = [
        (1, 0, 0, 0, 4.0, 9.0, 1.0, 3.0, 10),
        (5, 499, 0, 0, 4.0, 9.0, 1.0, 3.0, 1),
        (15, 1499, 0, 0, 4.0, 9.0, 1.0, 3.0, 1),
        (35, 3499, 0, 0, 0.0, 9.0, 1.0, 3.0, 1),
        (36, 3599, 0, 0, 4.0, 9.0, 1.0, 3.0, 1),
        (79, 7900, 0, 250, 4.0, 9.0, 1.0, 3.0, 10 ** 9),
        (80, 9999, 7, 4, 4.0, 9.0, 1.0, 3.0, 100),
        (5, 100, 0, 0, 4.0, 9.0, 1.0, 3.0, -5),
    ]
    for level, exp, skill, stat, hp, max_hp, mana, max_mana, amount in cases:
        got = quests.give_exp(
            level, exp, skill, stat, hp, max_hp, mana, max_mana, amount, table
        )
        out.emit(
            "exp",
            level,
            exp,
            skill,
            stat,
            hp,
            max_hp,
            mana,
            max_mana,
            amount,
            *table,
            got[0],
            got[1],
            got[2],
            got[3],
            got[4],
            got[5],
            got[6],
        )

    for evil, decision, mission, difficulty in (
        (0, 0, 0, 1),
        (2, 0, 2, 1),
        (2, 2, 1, 1),
        (2, 2, 2, 1),
        (2, 2, 3, 1),
        (1, 1, 2, 1),
        (4, 4, 2, 2),
    ):
        out.emit(
            "hub",
            evil,
            decision,
            mission,
            difficulty,
            quests.hub_name(evil, decision, mission, difficulty),
        )

    acts = [["hub", "blood1"], ["hub", "innovat1"], ["hub"], ["hub", "fire1"]]
    out.emit("acts", *(pack_act(act) for act in acts))
    for name, current in (
        ("blood1", 0),
        ("blood1_x", 0),
        ("blood", 0),
        ("fire1", 0),
        ("fire", 0),
        ("hub2", 0),
        ("hub2", 3),
        ("fire1", -1),
        ("HUB", 0),
        ("fire1", 0),
    ):
        out.emit("find_act", name, current, quests.find_act(name, current, acts))
    empty_acts = [[""], ["hub"]]
    out.emit("acts", *(pack_act(act) for act in empty_acts))
    out.emit("find_act", "fire1", 0, quests.find_act("fire1", 0, empty_acts))
    out.emit("acts", *(pack_act(act) for act in acts))
    for name, current in (("fire1", 0), ("fire1", 3), ("nope", 3), ("fire1", -1)):
        changed, new = quests.act_changed(name, current, acts)
        out.emit("act_changed", name, current, changed, new)

    game = players(2, 1)
    game.mission_set_available(1, 4, 1)
    out.emit("mission_on", 4, 1)
    game.mission_set_complete(1, 4, -1)
    out.emit("mission_done", 4, -1)
    game.mission_set_complete(1, 4, 0)
    out.emit("mission_done", 4, 0)
    game.mission_set_available(1, 13, 1)
    out.emit("mission_on", 13, 1)
    for slot in range(4):
        out.emit(
            "expect_mission",
            slot,
            1,
            4,
            game.mem.u8(game.mission_byte(slot, 1, 4)),
        )
    names = []
    game.run_script = lambda name: names.append(ascii(name)) or 0
    game.mission_grant_medal(4, 3)
    out.emit("medal", 4, 3, ",".join(names))
    out.emit("expect_medal", 0, 4, game.mem.u8(game.rec(0) + natives.P_MEDALS + 4))
    out.emit("expect_medal", 1, 4, game.mem.u8(game.rec(1) + natives.P_MEDALS + 4))

    # Row 5, the last row: mission 12 is +0x4c0c, the byte before the medals.
    game = players(2, 5)
    for mission, on in ((0, 1), (12, 1), (12, 3)):
        game.mission_set_available(5, mission, on)
        out.emit("mission_on", mission, on)
    game.mission_set_complete(5, 12, -1)
    out.emit("mission_done", 12, -1)
    game.mission_set_complete(5, 0, 2)
    out.emit("mission_done", 0, 2)
    for slot in range(4):
        for mission in (0, 12):
            out.emit("expect_mission", slot, 5, mission, game.mem.u8(game.mission_byte(slot, 5, mission)))
        out.emit("expect_medal", slot, 0, game.mem.u8(game.rec(slot) + natives.P_MEDALS))

    game = players(2, 1)
    game.n_setEvil(None, [0])
    out.emit("evil", 0)
    game.n_setEvil(None, [1])
    out.emit("evil", 1)
    game.n_setMadeDecision(None, [])
    out.emit("decision")
    out.emit(
        "expect_choice",
        game.mem.u8(game.rec(0) + natives.P_EVIL),
        game.mem.u8(game.rec(0) + natives.P_DECISION),
        game.mem.u8(game.rec(1) + natives.P_EVIL),
        game.mem.u8(game.rec(1) + natives.P_DECISION),
    )

    game = players(2)
    game.player_set_flag(-1, 0, 1)
    out.emit("flag", -1, 0, 1)
    game.player_set_flag(0, 8, 1)
    out.emit("flag", 0, 8, 1)
    game.player_set_flag(0, 3, 0)
    out.emit("flag", 0, 3, 0)
    out.emit("expect_flag", 0, game.mem.u8(game.rec(0) + natives.P_FLAGS))
    out.emit("expect_flag", 1, game.mem.u8(game.rec(1) + natives.P_FLAGS))

    game = players(2)
    game.n_givePlayerGold(None, [5])
    out.emit("gold", 5)
    game.n_givePlayerGold(None, [-5])
    out.emit("gold", -5)
    out.emit("expect_gold", 0, game.mem.s32(game.rec(0) + natives.P_GOLD))
    out.emit("expect_gold", 1, game.mem.s32(game.rec(1) + natives.P_GOLD))

    game = players(1)
    game.var_set(b"CurrentMission", 3)
    out.emit("var_set", "CurrentMission", 3)
    game.var_get(b"UnknownName")
    out.emit("var_get", "UnknownName")
    game.var_set(b"CURRENTMISSION", 200)
    out.emit("var_set", "CURRENTMISSION", 200)
    out.emit("expect_var", "currentmission", game.var_get(b"currentmission"))
    game.vars_clear_all()
    out.emit("vars_clear")
    out.emit("expect_var", "currentmission", 0)
    out.emit("expect_var", "unknownname", 0)

    game = players(1)
    # player_initNewCharacter marks mission 0 available and complete for
    # d = 1..5 (research-items 49bc578 docs/script-items.md section 3).
    out.emit("seed", 0)
    for difficulty in range(FIRST_DIFFICULTY, FIRST_DIFFICULTY + 5):
        out.emit("expect_mission", 0, difficulty, 0, 1 | 2)

    rows = []
    for index in range(13):
        rows.append(
            quests.MissionRow("s%d" % index, "", "m%d" % index, "b%d" % index, 1, 1, 1)
        )
    rows[2] = quests.MissionRow("s2", "", "", "", 0, 0, 0)
    rows[5] = quests.MissionRow("s5", "e5", "m5", "b5", 1, 1, 1)
    out.emit("menu_reset")
    for row in rows:
        out.emit(
            "menu_row",
            row.story,
            row.evil,
            row.medal,
            row.bonus,
            row.flag20,
            row.flag24,
            row.flag28,
        )
    out.emit("menu_avail", 1, 5)
    out.emit("menu_done", 5)
    out.emit("menu_medal", 5, 3)
    out.emit("menu_flags", 0, 0, 0, 1, 0)
    state = quests.new_menu()
    view = quests.CampaignView(
        rows, available={1, 5}, complete={5}, medals={5: 3}
    )
    for _ in range(18):
        quests.menu_tick(state, 0, view)
        out.emit("menu_button", 0)
        out.emit(
            "expect_menu",
            state["state"],
            state["row"],
            state["col"],
            state["counter"],
            state["mark"],
            state["paused"],
            state["pending"],
            state["orient"],
            state["c0"],
        )
    state["row"] = 4
    state["col"] = 0
    out.emit("menu_place", 4, 0)
    quests.menu_tick(state, quests.BTN_LEFT, view)
    out.emit("menu_button", quests.BTN_LEFT)
    out.emit(
        "expect_menu",
        state["state"],
        state["row"],
        state["col"],
        state["counter"],
        state["mark"],
        state["paused"],
        state["pending"],
        state["orient"],
        state["c0"],
    )
    state["row"] = 0
    state["col"] = 0
    out.emit("menu_place", 0, 0)
    quests.menu_tick(state, quests.BTN_CROSS, view)
    out.emit("menu_button", quests.BTN_CROSS)
    out.emit(
        "expect_menu",
        state["state"],
        state["row"],
        state["col"],
        state["counter"],
        state["mark"],
        state["paused"],
        state["pending"],
        state["orient"],
        state["c0"],
    )
    for _ in range(2):
        quests.menu_tick(state, 0, view)
        out.emit("menu_button", 0)
    view.fade = 128
    out.emit("menu_flags", 0, 0, 0, 1, 128)
    for _ in range(3):
        quests.menu_tick(state, 0, view)
        out.emit("menu_button", 0)
    out.emit(
        "expect_menu",
        state["state"],
        state["row"],
        state["col"],
        state["counter"],
        state["mark"],
        state["paused"],
        state["pending"],
        state["orient"],
        state["c0"],
    )

    row = quests.MissionRow("s", "e", "m", "b", 1, 1, 1)
    for index in range(12):
        out.emit(
            "helper",
            index,
            row.story,
            row.evil,
            row.medal,
            row.bonus,
            row.flag20,
            row.flag24,
            row.flag28,
            1,
            1,
            3,
            quests.helper_value(index, row, True, True, 3),
        )
    row.flag24 = 0
    out.emit(
        "helper",
        6,
        row.story,
        row.evil,
        row.medal,
        row.bonus,
        row.flag20,
        row.flag24,
        row.flag28,
        1,
        1,
        3,
        quests.helper_value(6, row, True, True, 3),
    )
    out.emit(
        "helper",
        7,
        row.story,
        row.evil,
        row.medal,
        row.bonus,
        row.flag20,
        row.flag24,
        row.flag28,
        1,
        1,
        3,
        quests.helper_value(7, row, True, True, 3),
    )
    row.medal = ""
    out.emit(
        "helper",
        7,
        row.story,
        row.evil,
        "",
        row.bonus,
        row.flag20,
        row.flag24,
        row.flag28,
        1,
        1,
        3,
        quests.helper_value(7, row, True, True, 3),
    )

    round2(out, natives, quests)

    text_out = "\n".join(out.lines) + "\n"
    args.tsv.parent.mkdir(parents=True, exist_ok=True)
    args.tsv.write_text(text_out, encoding="utf-8")
    contract = {
        "commit": commit,
        "policy": "rta-quests-v1",
        "lines": len(out.lines),
        "sha256": {name: digest(root / name) for name in FILES},
    }
    args.contract.write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8")
    print("lines %d commit %s" % (len(out.lines), commit))


def round2(out, natives, quests):
    """Round 2: the case structure of difftest_quests.py with synthetic rows,
    acts and thresholds, and seeded runs of the reference.

    Expected values come from ref_quests.py and ref_natives.py only. The
    retail mission rows, act names and experience words stay out: the
    difftest reads those from the executable, so its rows are replaced here
    by authored rows of the same shape (rows 2, 9 and 11 with null medal and
    bonus pointers and zero flag words, row 5 with an evil level).
    """
    import random

    rng = random.Random(20973)
    out.emit("round", 2)

    def record_line(game, slot):
        rec = game.rec(slot)
        out.emit(
            "expect_record",
            slot,
            game.mem.read(rec + natives.P_QUESTS, 0x200).hex(),
            game.mem.s32(rec + natives.P_QUEST_COUNT),
            game.mem.read(rec + natives.P_MISSIONS + FIRST_DIFFICULTY * 13, 65).hex(),
            game.mem.read(rec + natives.P_MEDALS, 13).hex(),
            game.mem.u8(rec + natives.P_EVIL),
            game.mem.u8(rec + natives.P_DECISION),
            game.mem.u8(rec + natives.P_FLAGS),
            game.mem.s32(rec + natives.P_GOLD),
        )

    def players(count, difficulty=IMAGE_DIFFICULTY):
        game = natives.Game()
        game.num_players = count
        game.difficulty = difficulty
        out.emit("reset")
        out.emit("set_players", count)
        out.emit("set_difficulty", difficulty)
        return game

    # ---- quest list: seeded quest_add / quest_remove (ref_natives) ----------
    titles = ["A", "Al", "Alpha", "Bravo", "Q", "LongTitleOfThirtyOneCharacters1",
              "Mid-length title", "ZZ"]
    texts = ["", "b", "Body text", "A body of exactly thirty-one ch", "x" * 12]
    for _scenario in range(48):
        count = rng.randint(1, 4)
        game = players(count, rng.randint(FIRST_DIFFICULTY, FIRST_DIFFICULTY + 4))
        for slot in range(4):
            if rng.random() < 0.15:
                planted = rng.choice((7, 8))
                game.mem.w32(game.rec(slot) + natives.P_QUEST_COUNT, planted)
                out.emit("plant_count", slot, planted)
        for _step in range(rng.randint(4, 14)):
            if rng.random() < 0.7:
                title, text = rng.choice(titles), rng.choice(texts)
                game.quest_add(title.encode(), text.encode())
                out.emit("quest_add", title, text)
            else:
                title = rng.choice(titles)
                game.quest_remove(title.encode())
                out.emit("quest_remove", title)
            for slot in range(4):
                out.emit("expect_count", slot, game.mem.s32(game.rec(slot) + natives.P_QUEST_COUNT))
        for slot in range(4):
            record_line(game, slot)

    # ---- missions, medals, evil, decision, flags, gold (ref_natives) --------
    for _scenario in range(48):
        count = rng.randint(0, 4)
        game = players(count, rng.randint(FIRST_DIFFICULTY, FIRST_DIFFICULTY + 4))
        names = []
        game.run_script = lambda name: names.append(ascii(name)) or 0
        for _step in range(rng.randint(6, 18)):
            kind = rng.randrange(8)
            if kind == 0:
                mission, on = rng.randint(0, 14), rng.randint(0, 3)
                game.n_setMissionAvailable(None, [mission, on])
                out.emit("mission_on", mission, on)
            elif kind == 1:
                mission, value = rng.randint(0, 14), rng.choice((-1, 0, 1, 2, 3))
                game.mission_set_complete(game.difficulty, mission, value)
                out.emit("mission_done", mission, value)
            elif kind == 2:
                mission, level = rng.randint(0, 13), rng.randint(0, 4)
                del names[:]
                game.mission_grant_medal(mission, level)
                out.emit("medal", mission, level, ",".join(names))
            elif kind == 3:
                on = rng.choice((0, 1, 2))
                game.n_setEvil(None, [on])
                out.emit("evil", on)
            elif kind == 4:
                game.n_setMadeDecision(None, [])
                out.emit("decision")
            elif kind == 5 and count:
                slot = rng.choice([-1] + list(range(count)))
                bit, on = rng.randint(0, 40), rng.randint(0, 1)
                game.player_set_flag(slot, bit, on)
                out.emit("flag", slot, bit, on)
            elif kind == 6:
                amount = rng.choice((5, -5, 7, -7, 400, 100, 10, 99999999, -99999999,
                                     rng.randint(-10 ** 9, 10 ** 9)))
                game.n_givePlayerGold(None, [amount])
                out.emit("gold", amount)
            else:
                slot = rng.randint(0, 3)
                gold = rng.choice((0, 99999990, 0x7FFFFFFC, -0x80000000, rng.randint(-10 ** 6, 10 ** 6)))
                game.mem.w32(game.rec(slot) + natives.P_GOLD, gold & 0xFFFFFFFF)
                out.emit("plant_gold", slot, gold)
        for slot in range(4):
            record_line(game, slot)

    # ---- quest variables (ref_natives vars_*) -------------------------------
    pool = ["Step", "step", "STEP", "currentMission", "whichAct", "finishedQuest", "a", "Z9_x"]
    for _scenario in range(16):
        game = players(1)
        for _step in range(rng.randint(3, 16)):
            kind = rng.randrange(5)
            name = rng.choice(pool)
            if kind < 3:
                value = rng.randint(-400, 400)
                game.var_set(name.encode(), value)
                out.emit("var_set", name, value)
            elif kind == 3:
                game.var_get(name.encode())
                out.emit("var_get", name)
            else:
                game.vars_clear_all()
                out.emit("vars_clear")
        table = game.quest_vars()
        if table:
            out.emit("expect_vars", "|".join("%s=%d" % (ascii(n), v) for n, v in table))

    # ---- menu helpers: difftest check_helpers on authored rows --------------
    def authored_rows():
        rows = []
        for index in range(13):
            rows.append(quests.MissionRow("s%d" % index, "", "m%d" % index, "b%d" % index, 1, 1, 1))
        for index in (2, 9, 11):
            rows[index] = quests.MissionRow("s%d" % index, "", "", "", 0, 0, 0)
        rows[5] = quests.MissionRow("s5", "e5", "m5", "b5", 1, 1, 1)
        return rows

    def helper_line(index, row, avail, done, medal):
        out.emit("helper", index, row.story, row.evil, row.medal, row.bonus, row.flag20,
                 row.flag24, row.flag28, 1 if avail else 0, 1 if done else 0, medal,
                 quests.helper_value(index, row, avail, done, medal))

    base_rows = authored_rows()
    cases = []
    for row in (0, 2, 5, 11):
        for medal in (0, 1, 2, 3):
            for complete in (False, True):
                cases.append((row, medal, complete, None))
    for edit in ("flag20", "flag24", "flag28", "flags", "medal"):
        cases.append((0, 3, True, edit))
    for row_index, medal, complete, edit in cases:
        info = base_rows[row_index]
        info = quests.MissionRow(info.story, info.evil, info.medal, info.bonus,
                                 info.flag20, info.flag24, info.flag28)
        if edit in ("flag20", "flags"):
            info.flag20 = 0
        if edit in ("flag24", "flags"):
            info.flag24 = 0
        if edit in ("flag28", "flags"):
            info.flag28 = 0
        if edit == "medal":
            info.medal = ""
        for index in range(12):
            helper_line(index, info, True, complete, medal)
    for _case in range(200):
        info = quests.MissionRow(rng.choice(("s", "")), rng.choice(("e", "")), rng.choice(("m", "")),
                                 rng.choice(("b", "")), rng.choice((0, 1, -1, 7)),
                                 rng.choice((0, 1, -1, 7)), rng.choice((0, 1, -1, 7)))
        helper_line(rng.randrange(12), info, rng.random() < 0.5, rng.random() < 0.5, rng.randint(-1, 4))

    # ---- mission menu: difftest check_menu paths on authored rows -----------
    def menu_expect(state):
        out.emit("expect_menu", state["state"], state["row"], state["col"], state["counter"],
                 state["mark"], state["paused"], state["pending"], state["orient"], state["c0"])

    def menu_flags(view):
        out.emit("menu_flags", 1 if view.evil else 0, 1 if view.local else 0, 1 if view.pause else 0,
                 1 if view.slot_active else 0, view.fade)

    def menu_boot(rows, available, complete=(), medals=None, evil=False):
        out.emit("menu_reset")
        for row in rows:
            out.emit("menu_row", row.story, row.evil, row.medal, row.bonus, row.flag20, row.flag24,
                     row.flag28)
        out.emit("menu_avail", *sorted(available))
        out.emit("menu_done", *sorted(complete))
        for mission, medal in sorted((medals or {}).items()):
            out.emit("menu_medal", mission, medal)
        view = quests.CampaignView(rows, available, complete, medals or {}, evil=evil)
        menu_flags(view)
        return quests.new_menu(), view

    def menu_tick(state, view, button):
        quests.menu_tick(state, button, view)
        out.emit("menu_button", button)
        menu_expect(state)

    def reach(state, view, n=17):
        for _ in range(n):
            menu_tick(state, view, 0)

    def place(state, row, col):
        state["row"], state["col"] = row, col
        out.emit("menu_place", row, col)

    for available, _row in (({1, 5}, 4), ({2, 3, 11}, 10), (set(), 0), ({0}, 0)):
        state, view = menu_boot(base_rows, available)
        reach(state, view)

    def after(available, complete, medals, row, col, button):
        state, view = menu_boot(base_rows, available, complete, medals)
        reach(state, view)
        place(state, row, col)
        menu_tick(state, view, button)

    B = quests
    after({1, 2}, set(), {}, 1, 0, B.BTN_CROSS)
    after({1}, set(), {}, 3, 0, B.BTN_CROSS)
    after({1, 2}, set(), {}, 1, 0, B.BTN_CIRCLE)
    after({1, 2}, set(), {}, 1, 0, B.BTN_SELECT)
    after({1, 2}, set(), {}, 1, 0, B.BTN_TRIANGLE)
    after({1, 2}, set(), {}, 1, 0, B.BTN_SQUARE)
    after({1, 2}, set(), {}, 1, 0, B.BTN_START)
    after({1, 3}, set(), {}, 2, 0, B.BTN_UP)
    after({1}, set(), {}, 0, 0, B.BTN_UP)
    after({1}, set(), {}, 0, 0, B.BTN_DOWN)
    after({1, 2, 3}, set(), {}, 1, 0, B.BTN_DOWN)
    after({12}, set(), {}, 11, 0, B.BTN_DOWN)
    after({2}, {2}, {2: 0}, 1, 0, B.BTN_RIGHT)
    after({1}, {1}, {1: 0}, 0, 0, B.BTN_LEFT)
    after({1}, {1}, {1: 3}, 0, 0, B.BTN_RIGHT)
    after({2}, set(), {2: 3}, 1, 0, B.BTN_RIGHT)
    after({2}, {2}, {2: 3}, 1, 1, B.BTN_RIGHT)
    after({3}, {3}, {3: 3}, 2, 1, B.BTN_RIGHT)
    after({2}, {2}, {2: 3}, 1, 2, B.BTN_LEFT)
    after({2}, set(), {2: 3}, 1, 2, B.BTN_LEFT)
    after({2}, set(), {}, 1, 1, B.BTN_LEFT)
    after({2}, {2}, {2: 0}, 1, 1, B.BTN_CROSS)
    after({2}, set(), {2: 3}, 1, 1, B.BTN_CROSS)
    after({2}, set(), {2: 3}, 1, 2, B.BTN_CROSS)
    after({2}, set(), {2: 0}, 1, 2, B.BTN_CROSS)

    # pause eats the button
    state, view = menu_boot(base_rows, {1, 2})
    reach(state, view)
    view.local = view.pause = True
    menu_flags(view)
    place(state, 1, 0)
    menu_tick(state, view, B.BTN_CROSS)
    # local without the pause menu, and the pause menu on a remote chooser
    for local, pause in ((True, False), (False, True)):
        state, view = menu_boot(base_rows, {1, 2})
        reach(state, view)
        view.local, view.pause = local, pause
        menu_flags(view)
        place(state, 1, 0)
        menu_tick(state, view, B.BTN_CROSS)
    # inactive slot closes; then the close countdown
    state, view = menu_boot(base_rows, {1})
    reach(state, view)
    view.slot_active = False
    menu_flags(view)
    menu_tick(state, view, 0)
    for _ in range(4):
        menu_tick(state, view, 0)
    # second idle tick
    state, view = menu_boot(base_rows, {1})
    reach(state, view)
    menu_tick(state, view, 0)
    menu_tick(state, view, 0)
    # launch chains: story, medal, bonus, evil, evil row without an evil level
    for evil, col, medals, complete, row in ((False, 0, {}, set(), 0), (False, 1, {1: 0}, {1}, 0),
                                             (False, 2, {1: 3}, set(), 0), (True, 0, {}, {6}, 5),
                                             (True, 0, {}, {2}, 1)):
        state, view = menu_boot(base_rows, {row + 1}, complete, medals, evil)
        reach(state, view)
        place(state, row, col)
        menu_tick(state, view, B.BTN_CROSS)
        for fade in (0, 0, 0, 128, 128, 128):
            view.fade = fade
            menu_flags(view)
            menu_tick(state, view, 0)
    # close countdown after circle
    state, view = menu_boot(base_rows, {1})
    reach(state, view)
    menu_tick(state, view, B.BTN_CIRCLE)
    for _ in range(4):
        menu_tick(state, view, 0)
    # state 3 returns to state 2 without reading the button
    state, view = menu_boot(base_rows, {1})
    reach(state, view)
    state["state"] = 3
    out.emit("menu_state", 3)
    menu_tick(state, view, B.BTN_CROSS)

    # test_ref_quests Menu.test_movement_and_launch, step for step
    rows = []
    for index in range(13):
        rows.append(quests.MissionRow("s%d" % index, "", "m%d" % index, "b%d" % index, 1, 1, 1))
    rows[2] = quests.MissionRow("s2", "", "", "", 0, 0, 0)
    rows[5] = quests.MissionRow("s5", "e5", "m5", "b5", 1, 1, 1)
    state, view = menu_boot(rows, {1, 5}, {5}, {5: 3})
    reach(state, view, 18)
    place(state, 4, 0)
    menu_tick(state, view, B.BTN_LEFT)
    menu_tick(state, view, B.BTN_LEFT)
    menu_tick(state, view, B.BTN_RIGHT)
    place(state, 4, 2)
    view.complete_set.clear()
    out.emit("menu_done")
    menu_tick(state, view, B.BTN_RIGHT)
    place(state, 5, 0)
    view.available_set.add(6)
    view.complete_set.add(6)
    out.emit("menu_avail", *sorted(view.available_set))
    out.emit("menu_done", *sorted(view.complete_set))
    menu_tick(state, view, B.BTN_RIGHT)
    place(state, 2, state["col"])
    menu_tick(state, view, B.BTN_UP)
    menu_tick(state, view, B.BTN_DOWN)
    place(state, 0, 0)
    menu_tick(state, view, B.BTN_CROSS)
    menu_tick(state, view, 0)
    menu_tick(state, view, 0)
    menu_tick(state, view, 0)
    view.fade = 128
    menu_flags(view)
    menu_tick(state, view, 0)
    menu_tick(state, view, 0)
    menu_tick(state, view, 0)

    # seeded menu runs: authored random rows, one button (or none) per tick
    buttons = [0, 0, B.BTN_TRIANGLE, B.BTN_CIRCLE, B.BTN_CROSS, B.BTN_SQUARE, B.BTN_SELECT,
               B.BTN_START, B.BTN_UP, B.BTN_RIGHT, B.BTN_DOWN, B.BTN_LEFT,
               B.BTN_UP, B.BTN_DOWN, B.BTN_LEFT, B.BTN_RIGHT]
    for _run in range(40):
        rows = []
        for index in range(13):
            rows.append(quests.MissionRow(
                "s%d" % index, rng.choice(("", "e%d" % index)), rng.choice(("", "m%d" % index)),
                rng.choice(("", "b%d" % index)), rng.choice((0, 1, -1)), rng.choice((0, 1, -1)),
                rng.choice((0, 1, -1))))
        available = set(m for m in range(13) if rng.random() < 0.5)
        complete = set(m for m in range(13) if rng.random() < 0.4)
        medals = dict((m, rng.randint(0, 3)) for m in range(13) if rng.random() < 0.5)
        state, view = menu_boot(rows, available, complete, medals, rng.random() < 0.3)
        reach(state, view)
        for _tick in range(rng.randint(20, 60)):
            if state["state"] >= 4:
                break
            if rng.random() < 0.05:
                view.local = rng.random() < 0.5
                view.pause = rng.random() < 0.5
                view.slot_active = rng.random() < 0.9
                menu_flags(view)
            menu_tick(state, view, rng.choice(buttons))
        while 5 <= state["state"] <= 8:
            view.fade = rng.choice((0, 64, 127, 128, 200))
            menu_flags(view)
            menu_tick(state, view, 0)
            if state["state"] == 8:
                menu_tick(state, view, 0)
                break

    # ---- hub name: difftest check_hub, then every bit pattern ---------------
    for evil, decision, mission, difficulty in ((0, 0, 0, 1), (2, 0, 2, 1), (0, 2, 2, 1), (2, 2, 0, 1),
                                                (2, 2, 1, 1), (2, 2, 2, 1), (2, 2, 3, 1), (1, 1, 2, 1),
                                                (4, 4, 2, 2), (1, 1, 2, 0)):
        out.emit("hub", evil, decision, mission, difficulty, quests.hub_name(evil, decision, mission, difficulty))
    for evil in (0, 1, 2, 4, 6, 0xFF):
        for decision in (0, 1, 2, 4, 6, 0xFF):
            for mission in range(4):
                for difficulty in range(5):
                    out.emit("hub", evil, decision, mission, difficulty,
                             quests.hub_name(evil, decision, mission, difficulty))

    # ---- experience: difftest check_exp on authored thresholds --------------
    def exp_line(table, level, exp, skill, stat, hp, max_hp, mana, max_mana, amount):
        got = quests.give_exp(level, exp, skill, stat & 0xFF, hp, max_hp, mana, max_mana, amount, table)
        out.emit("exp", level, exp, skill, stat & 0xFF, hp, max_hp, mana, max_mana, amount, *table,
                 *got)

    table = [100 * i for i in range(80)] + [10000]
    t1 = table[1]
    for level, exp, amount, hp, max_hp, mana, max_mana, stat in (
            (1, 0, 0, 10.0, 20.0, 3.0, 9.0, 0), (1, 0, 10, 10.0, 20.0, 3.0, 9.0, 0),
            (1, t1 - 5, 10, 10.0, 20.0, 3.0, 9.0, 0), (1, t1 - 5, 10, 0.0, 20.0, 3.0, 9.0, 0),
            (5, table[5] - 1, 5, 4.0, 8.0, 1.0, 2.0, 0), (15, table[15] - 1, 1, 4.0, 8.0, 1.0, 2.0, 0),
            (35, table[35] - 1, 1, 4.0, 8.0, 1.0, 2.0, 0), (36, table[36] - 1, 1, 4.0, 8.0, 1.0, 2.0, 0),
            (79, table[79], 10 ** 9, 4.0, 8.0, 1.0, 2.0, 250), (80, table[80] - 1, 100, 4.0, 8.0, 1.0, 2.0, 0),
            (5, 100, -5, 4.0, 8.0, 1.0, 2.0, 0), (2, 0, table[2], 4.0, 8.0, 1.0, 2.0, 0)):
        exp_line(table, level, exp, 0, stat, hp, max_hp, mana, max_mana, amount)
    steps = sorted(rng.sample(range(1, 2000000), 80))
    second = [0] + steps
    for _case in range(60):
        chosen = rng.choice((table, second))
        level = rng.randint(0, 80)
        exp = rng.randint(0, chosen[80] - 1)
        amount = rng.choice((0, 1, -1, rng.randint(-5000, 5000), rng.randint(0, 3000000)))
        exp_line(chosen, level, exp, rng.randint(0, 50), rng.randint(0, 255),
                 rng.choice((0.0, -1.0, 4.0)), 9.0, 1.0, 3.0, amount)

    # ---- acts: difftest check_acts on authored act rows ---------------------
    acts = [["hub", "aa1"], ["hub", "bb1"], ["hub"], ["hub", "cc1", "cc2"], ["dd"]]
    out.emit("acts", *(pack_act(act) for act in acts))
    for name in ("hub", "hub2", "aa1", "aa1_x", "aa", "cc", "cc2", "dd", "ddx", "HUB", "nope", "d"):
        for current in (-1, 0, 1, 3, 4, 7):
            out.emit("find_act", name, current, quests.find_act(name, current, acts))
            changed, new = quests.act_changed(name, current, acts)
            out.emit("act_changed", name, current, changed, new)

    # ---- flag 39: difftest check_flag39 -------------------------------------
    bit = 1 << 39

    def flag39(items, slot=0, bag=0):
        out.emit("reset")
        out.emit("set_players", 2)
        for name, flags in items:
            out.emit("item", slot, bag, name, flags)
        out.emit("flag39", slot, bag)
        kept = quests.flag39_remaining(items)
        for index, (name, flags) in enumerate(kept):
            out.emit("expect_item", slot, bag, index, name, flags)
        out.emit("expect_items", slot, bag, len(kept))

    for name in ("zzzz", "bloodstonefay", "xxshardofyy", "spiritrunes", "emptybag", "fullbag",
                 "pickaxe", "pickaxe2", "twistedkey", "Bloodstone", ""):
        flag39([(name, bit)])
    flag39([("zzzz", 0)])
    flag39([("zzzz", 1 << 38)])
    flag39([("pickaxe", bit), ("zzzz", bit)])
    flag39([("zzzz", bit), ("pickaxe", bit)])
    flag39([("zzzz", bit), ("yyyy", bit)])
    flag39([("zzzz", bit)], bag=1)
    flag39([("zzzz", bit)], bag=2)
    flag39([("zzzz", bit)], slot=1)

    # ---- act clear: difftest check_clear plus the flag-39 and variable parts
    out.emit("reset")
    out.emit("set_players", 2)
    blobs = []
    for slot, count, title in ((0, 1, b"ABCDEFGHIJKLMNOPQRSTUV"), (1, 2, b"QuestTwo"), (2, 3, b"Untouched")):
        entries = [(title + bytes(32 - len(title)), b"body-of-the-quest!!" + bytes(13)),
                   (b"SECOND" + bytes(26), bytes(32))] + [(bytes(32), bytes(32))] * 6
        blobs.append((count, entries))
        for index, (t, x) in enumerate(entries[:2]):
            out.emit("plant_hex", slot, index, hex_field(t), hex_field(x))
        out.emit("plant_count", slot, count)
        out.emit("item", slot, 0, "zzzz", bit)
        out.emit("item", slot, 0, "pickaxe", bit)
    out.emit("var_set", "Step", 9)
    out.emit("act_clear")
    for slot, (count, entries) in enumerate(blobs):
        if slot < 2:
            expect, n = quests.clear_quest_list(entries, count)
            kept = quests.flag39_remaining([("zzzz", bit), ("pickaxe", bit)])
        else:
            expect, n = entries, count
            kept = [("zzzz", bit), ("pickaxe", bit)]
        out.emit("expect_count", slot, n)
        for index, (t, x) in enumerate(expect):
            out.emit("expect_hex", slot, index, hex_field(t), hex_field(x))
        out.emit("expect_items", slot, 0, len(kept))
    out.emit("expect_var", "step", 0)


if __name__ == "__main__":
    main()
