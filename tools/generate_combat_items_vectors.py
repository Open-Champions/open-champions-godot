"""Generate exact authored item-to-combat integration results from ref_combat.py."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys

COMMIT = "7212ecfecbeae8dd2015f6c82897226f49a94695"
PIN = "b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--research-dir", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("native/tests/vectors/combat_items.tsv"))
    args = parser.parse_args()
    source = args.research_dir / "tools/ref_combat.py"
    if hashlib.sha256(source.read_bytes()).hexdigest() != PIN:
        parser.error("reference source differs from pinned contract")
    committed = subprocess.check_output(["git", "-C", str(args.research_dir), "show", COMMIT + ":tools/ref_combat.py"])
    if hashlib.sha256(committed).hexdigest() != PIN:
        parser.error("reference commit differs from pinned contract")
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("combat_reference", source)
    model = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(model)
    rng = random.Random(0x4349544d)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    rows = 0
    with args.output.open("w", encoding="ascii", newline="\n") as out:
        out.write("# rta-combat-items-v1\n")
        for case in range(512):
            # Definition/category values and four complete slots are supplied data.
            fields = [rng.randrange(-32768, 32768) for _ in range(4)]
            fields += [rng.randrange(3), rng.randrange(7), rng.getrandbits(32), rng.getrandbits(64), rng.randrange(-32768, 32768), rng.getrandbits(64)]
            name = bytes(rng.randrange(256) for _ in range(case % 9))
            mods = [None if (case + i) % 7 == 0 else [rng.choice((0x0d, 0x23, 0x15, 0x16, 0x18, 0x17, 0x26)), *[rng.randrange(-32768, 32768) for _ in range(3)]] for i in range(4)]
            mod_text = ";".join("-" if slot is None else ",".join(map(str, slot)) for slot in mods)
            definition = model.ItemDef(name=name.decode("latin1"), dmg_min=fields[0], dmg_max=fields[1], armor=fields[2], attack=fields[3], category=fields[4], subtype=fields[5], dmg_type=fields[6], flags=fields[7])
            item = model.Item(defn=definition if case % 31 else None, count=fields[8], flags=fields[9], mods=[(-1, 0, 0, 0) if slot is None else slot for slot in mods])
            name_text = ",".join(format(ord(c), "x") for c in definition.name) or "-"
            record_state = (",".join(map(str, fields[:8])) + "|" + name_text) if item.defn else "none"
            record_state += "|" + ",".join(map(str, fields[8:])) + "|" + ";".join(",".join(map(str, slot)) for slot in item.mods)
            for config in range(16):
                model.set_fpu(model.Fpu.MODELS[config // 4], bool(config & 2), bool(config & 1))
                try:
                    results = [model.item_armor_value(item), model.item_attack_rating(item), *model.item_damage_range(item, False), *model.item_damage_range(item, True)]
                    result = ",".join(map(str, results))
                except AttributeError:
                    result = "error"
                arguments = "|".join((str(config), str(int(item.defn is not None)), ",".join(map(str, fields)), name.hex() or "-", mod_text))
                out.write("\t".join(("combat-items", "convert", arguments, result, record_state)) + "\n")
                rows += 1
    data = args.output.read_bytes()
    expected = b"".join(line + b"\n" for line in data.splitlines() if not line.startswith(b"#"))
    manifest = {"schema_version": 1, "reference_commit": COMMIT, "reference_path": "tools/ref_combat.py", "reference_sha256": PIN,
                "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "vector_sha256": hashlib.sha256(data).hexdigest(),
                "vector_count": rows, "expected_trace_sha256": hashlib.sha256(expected).hexdigest(), "op_counts": {"convert": rows},
                "scope": "Authored item-to-combat bridge inputs and pinned combat statistics; all16 FPU configurations; no original data or executable."}
    args.output.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(rows, "exact item-to-combat vectors")


if __name__ == "__main__":
    main()
