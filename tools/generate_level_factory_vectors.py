#!/usr/bin/env python3
"""Object-factory table and vectors from research t3/research-script-session 6841b8a.

Reads the pinned tools/ref_level_factory.py with `git show 6841b8a:` (it must hash to its
pin) and writes:

  native/src/level_factory_table.inc   the 206 name tests, in the original's order:
                                       kind, string address, pool size, calls, clear colour
  native/tests/vectors/level_factory.tsv
                                       authored strings at the real addresses, an authored
                                       ASCII class table, and classification cases with what
                                       the reference returns for them

No disc or executable is read. The authored strings are not the game's: the C++ test
checks the native rules against the reference's rules, not the game's text. That the
reference matches the game is research's tools/difftest_level_factory.py (every record of
the disc's level files, 0 mismatches).

    python3 tools/generate_level_factory_vectors.py \\
        --research-dir <research-checkouts>/research-script-session
"""
import argparse
import hashlib
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = "6841b8a"
PINS = {
    "tools/ref_level_factory.py": "f6eae7a3d4440a30b61cf4e6ac4a9f6a5bedec0468619e8c70ad212274968895",
}
DOC_PINS = {
    "docs/script-session.md": "16699f2f6e1fe013b85fc6e1310c47c8115530e416c506428a5770159bd4b47e",
}
ROOT = Path(__file__).resolve().parents[1]


def show(research, path):
    return subprocess.run(["git", "-C", str(research), "show", "%s:%s" % (COMMIT, path)],
                          check=True, capture_output=True).stdout


def load(research):
    for path, pin in list(PINS.items()) + list(DOC_PINS.items()):
        data = show(research, path)
        got = hashlib.sha256(data).hexdigest()
        if got != pin:
            raise SystemExit("%s at %s hashes to %s, not the pin %s" % (path, COMMIT, got, pin))
    tmp = Path(tempfile.mkdtemp())
    (tmp / "ref_level_factory.py").write_bytes(show(research, "tools/ref_level_factory.py"))
    spec = importlib.util.spec_from_file_location("ref_level_factory", tmp / "ref_level_factory.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Authored:
    """Authored text at the reference's addresses; ASCII class bytes (upper 1, space 8)."""

    def __init__(self, table):
        self.table = table

    def string(self, addr):
        return self.table[addr]

    def ctype(self, index):
        return (1 if 65 <= index <= 90 else 0) | (8 if index in (9, 10, 11, 12, 13, 32) else 0)


def authored(F):
    t = {}
    for k, row in enumerate(F.TESTS + F.NESTED):
        t[row[1]] = b"Cls%dName" % k
    # A few names that exercise prefixes, case and the special rows.
    t[F.TESTS[0][1]] = b"First"
    t[F.LINE] = b"Line"
    t[F.LINE_NAME_KEY] = b"name"
    t[F.LINE_WAVE] = b"wave"
    keys = [b"CreatureContainer", b"AutoContainer", b"AutoChest", b"AutoProp", b"AutoEmitter",
            b"AutoDoor", b"AutoSwitch", b"Critter", b"Generator", b"SmokeGen"]
    for (_g, addr, _o), key in zip(F.FALLBACKS, keys):
        t[addr] = key
    for addr, key in zip(F.LIGHT_KEYS, (b"ParticleEmitter", b"DynamicLight", b"Halo")):
        t[addr] = key
    for addr, key in zip(F.LIGHT_FORMATS, (b"ParticleEmitter%d", b"DynamicLight%d", b"Halo%d")):
        t[addr] = key
    return Authored(t)


def cases(F, s):
    out = []
    for kind, addr, *_ in F.TESTS:
        name = s.string(addr)
        out += [(name, []), (name.upper(), None), (name.lower() + b"x", []), (name[:-1], [])]
        if kind == "prefix":
            out.append((name + b"Tail", [b"autoprop=1"]))
    out += [(b"Line", [b"name=wave"]), (b"LINE", [b"name=WaVe"]), (b"Line", [b"name=other"]),
            (b"Line", []), (b"Line", None), (b"", [])]
    keys = [s.string(a) for _g, a, _o in F.FALLBACKS]
    for key in keys:
        low = key.lower()
        for value in (b"0", b"1", b"-1", b"x", b"", b" 2", b"4294967296", b"99999999999999999999"):
            out.append((b"NoSuchClass", [low + b"=" + value]))
            out.append((b"NoSuchClass", [b"other=1", low + b"=" + value, b"halo3=1"]))
            out.append((b"NoSuchClass", [low + b"=" + value, b"dynamiclight=1"]))
        out.append((b"NoSuchClass", [key + b"=1"]))
        out.append((b"NoSuchClass", [low + b"x=1", low]))
    out += [(b"NoSuchClass", []), (b"NoSuchClass", None)]
    return out


def esc(b):
    return b.decode("latin-1").replace("\\", "\\\\").replace("\t", "\\t").replace("|", "\\p")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--research-dir", required=True)
    parser.add_argument("--table", default=str(ROOT / "native/src/level_factory_table.inc"))
    parser.add_argument("--out", default=str(ROOT / "native/tests/vectors/level_factory.tsv"))
    args = parser.parse_args()
    F = load(Path(args.research_dir).expanduser())
    lines = ["// Generated by tools/generate_level_factory_vectors.py from research",
             "// t3/research-script-session %s tools/ref_level_factory.py. Do not edit." % COMMIT,
             "// {prefix, string address, pool size (0: no object), {calls}, call count, clear colour}"]
    for row in F.TESTS:
        kind, addr, size, calls = row[:4]
        clear = len(row) > 4 and row[4] == F.CLEAR_COLOR
        c = list(calls) + [0] * (2 - len(calls))
        lines.append("{%s, 0x%06x, 0x%x, {0x%06x, 0x%06x}, %d, %s}," % (
            "true" if kind == "prefix" else "false", addr, size or 0, c[0], c[1], len(calls),
            "true" if clear else "false"))
    Path(args.table).write_text("\n".join(lines) + "\n")
    s = authored(F)
    out = ["# Generated by tools/generate_level_factory_vectors.py from research",
           "# t3/research-script-session %s. Authored strings, not the game's." % COMMIT]
    for row in F.TESTS:
        kind, addr, size, calls = row[:4]
        out.append("T\t%s\t0x%06x\t0x%x\t%s\t%d" % (kind, addr, size or 0,
                                                    ",".join("0x%x" % c for c in calls),
                                                    1 if len(row) > 4 and row[4] == F.CLEAR_COLOR else 0))
    for addr in sorted(s.table):
        out.append("S\t0x%06x\t%s" % (addr, esc(s.table[addr])))
    count = 0
    for name, props in cases(F, s):
        got = F.classify(name, props, s)
        kind = "object" if got.creates else ("color" if got.clear_color else ("call" if got.calls else "nothing"))
        prop = "-" if props is None else "|".join(esc(p) for p in props)
        out.append("C\t%s\t%s\t%s\t%s\t%s" % (esc(name), prop if props is None or props else "",
                                             kind, ",".join("0x%x" % x for x in got.sizes),
                                             ",".join("0x%x" % x for x in got.calls)))
        count += 1
    Path(args.out).write_text("\n".join(out) + "\n")
    print("wrote %d tests and %d cases" % (len(F.TESTS), count))


if __name__ == "__main__":
    main()
