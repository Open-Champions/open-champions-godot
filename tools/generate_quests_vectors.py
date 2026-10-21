#!/usr/bin/env python3
"""Port the quest and mission model as exact vectors.

Reads ref_quests.py and the campaign operations in ref_natives.py from
t3/research-quests at 7efd3ce. No disc and no executable. The retail
experience table and the retail act and mission names are not copied.
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

COMMIT = "7efd3ce"
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

    def players(count, difficulty=0):
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

    game = players(2, 0)
    game.player_set_flag(-1, 0, 1)
    out.emit("flag", -1, 0, 1)
    game.player_set_flag(0, 8, 1)
    out.emit("flag", 0, 8, 1)
    game.player_set_flag(0, 3, 0)
    out.emit("flag", 0, 3, 0)
    out.emit("expect_flag", 0, game.mem.u8(game.rec(0) + natives.P_FLAGS))
    out.emit("expect_flag", 1, game.mem.u8(game.rec(1) + natives.P_FLAGS))

    game = players(2, 0)
    game.n_givePlayerGold(None, [5])
    out.emit("gold", 5)
    game.n_givePlayerGold(None, [-5])
    out.emit("gold", -5)
    out.emit("expect_gold", 0, game.mem.s32(game.rec(0) + natives.P_GOLD))
    out.emit("expect_gold", 1, game.mem.s32(game.rec(1) + natives.P_GOLD))

    game = players(1, 0)
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

    game = players(1, 0)
    # docs/quests.md: a new character has mission 0 available and complete.
    out.emit("seed", 0)
    for difficulty in range(5):
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


if __name__ == "__main__":
    main()
