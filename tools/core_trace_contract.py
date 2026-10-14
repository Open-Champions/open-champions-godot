"""Construct expected native trace records from Python-generated result vectors."""
import hashlib


def expected_records(suite, lines):
    """Yield the exact authored trace bytes, including the math header."""
    count = 0
    if suite == "math":
        yield b"# rta-core-math-trace-v1\n"
    for line in lines:
        if isinstance(line, bytes):
            line = line.decode("ascii")
        line = line.rstrip("\r\n")
        if not line or line.startswith("#"):
            continue
        count += 1
        columns = line.split("\t")
        if suite == "arithmetic":
            if len(columns) != 7:
                raise ValueError("malformed arithmetic expectation")
            record = f"{count}\t{columns[3]}\t{int(columns[6], 16):016x}\n"
        elif suite == "math":
            if len(columns) != 6:
                raise ValueError("malformed math expectation")
            record = f"{columns[0]}\t{':'.join(columns[1:4])}\t{columns[4]}\t{columns[5]}\n"
        elif suite in ("session-anim-cache", "animation-runtime", "session-turn", "creature-turn", "animation-decoder", "session-animation", "animation-clock", "world-geometry", "state", "items", "item-generation", "item-names",
                       "combat-common", "combat-damage", "combat-stats", "combat-items",
                       "amx-image", "amx-execute", "amx-host",
                       "netwire-codec", "netwire-session", "netwire-tick", "session-replay", "objects-runtime", "session-objects", "creature-hp", "creature-movement", "combat-creature", "raw-handles", "session-hp", "session-combat", "world-queries", "session-terrain", "session-creature", "terrain-file", "session-control", "session-control-creature"):
            if len(columns) != 5:
                raise ValueError(f"malformed {suite} expectation")
            record = line + "\n"
        else:
            raise ValueError("unknown trace contract")
        yield record.encode("ascii")


def expected_trace(suite, lines):
    digest = hashlib.sha256()
    count = 0
    for record in expected_records(suite, lines):
        digest.update(record)
        if record.strip() and not record.startswith(b"#"):
            count += 1
    return count, digest.hexdigest()
