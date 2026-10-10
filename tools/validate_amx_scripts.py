"""Optional exact AMX comparison against scripts from the owner's own disc.

Only aggregate counts and trace digests leave this process. Native engine calls
use identical authored stubs; this does not validate the game-native handlers.
"""
import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
import pathlib
import subprocess
import sys

REFERENCE = "091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f"
COMMIT = "5ad489a445d41612d0795c063e1ce67557d5c441"
STATE = ("cip", "frm", "hea", "stk", "stp", "flags", "curline", "curfile",
         "dbgcode", "dbgaddr", "dbgparam", "dbgname", "error", "hdr_stp", "params")
HEADER = ("size", "cod", "dat", "hea0", "hdr_stp", "start", "num_publics",
          "publics_off", "num_natives", "natives_off", "num_libraries",
          "num_pubvars", "data_offset")


def byte_hex(data):
    return bytes(data).hex() or "-"


def state(machine):
    return ",".join("N" if (v := getattr(machine, name, None)) is None else str(v)
                    for name in STATE)


def metadata(machine, reference):
    if not machine._parsed:
        return "-"
    try:
        return (",".join(str(getattr(machine, name)) for name in HEADER) + ";" +
                "".join(f"{byte_hex(n)}:{a}," for n, a in machine.publics()) + ";" +
                "".join(byte_hex(n) + "," for n in machine.native_names()))
    except reference.HostFault:
        return "H"


class Stubs:
    def __init__(self, reference):
        self.reference = reference
        self.calls = []
        self.nested = 0
        self.rng = self.mode = 0

    def native(self, name):
        def invoke(machine, args):
            strings = []
            for a in args:
                if a >= 0 and not a & 3 and machine.get_addr(a) == 0:
                    try:
                        strings.append(byte_hex(machine.get_string(a)))
                    except self.reference.HostFault:
                        strings.append("N")
                else:
                    strings.append("N")
            self.calls.append(byte_hex(name) + ":" +
                              "".join(str(a) + "," for a in args) + ":" +
                              "".join(s + "," for s in strings))
            if name == b"callScript":
                self.nested += 1
                address = machine.cell(machine.params + 4)
                if machine.get_addr(address) != 0:
                    raise self.reference.HostFault("callScript address refused")
                return machine.call(machine.get_string(address),
                                    pri=0x5eed0000 + len(self.calls))
            if self.mode < 2:
                return self.mode
            self.rng = (self.rng * 1664525 + 1013904223) & 0xffffffff
            slot = self.rng & 7
            if slot < 3: return 0
            if slot < 5: return 1
            if slot == 5: return 2 + ((self.rng >> 8) % 11)
            if slot == 6: return -1 - ((self.rng >> 8) % 8)
            return self.reference.s32(self.rng)
        return invoke


def run_script(path, native, reference, padding, budget, digest, counts):
    data = path.read_bytes()
    if len(data) < 0xa0 or len(data) + padding > 64 * 1024 * 1024:
        counts["size_refused"] += 1
        return
    machine = reference.Machine(data + bytes(padding))
    stubs = Stubs(reference)
    process = subprocess.Popen([str(native), str(path), str(padding)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, text=True, encoding="ascii")
    reader = concurrent.futures.ThreadPoolExecutor(max_workers=1)
    try:
        try:
            e = machine.load()
            loaded = str(e)
            if not e:
                loaded += "," + str(machine.register({n: stubs.native(n)
                                                     for n in machine.native_names()}))
        except reference.HostFault: loaded = "H"
        except reference.Limit: loaded = "L"
        expected = ["L", loaded, metadata(machine, reference), state(machine), byte_hex(machine.image)]
        compare(read_line(reader, process), expected, digest)
        counts["load_compared"] += 1
        if loaded != "0,0":
            counts["load_refused"] += 1
            return
        if machine._decoded is None:
            counts["relocation_refused"] += 1
            return
        counts["decoded"] += 1
        saved = machine.snapshot()
        entries = machine.publics()
        counts["entrypoints"] += len(entries)
        # Every public entry from fresh state, then every public without reloading.
        commands = [(1, index, seed, mode) for index in range(len(entries))
                    for seed, mode in ((0, 0), (0, 1), (1000, 2), (1001, 2), (1002, 2))]
        commands += [(int(index == 0), index, 5000 + index, 2)
                     for index in range(len(entries))]
        for fresh, index, seed, mode in commands:
            pri = reference.s32((seed * 69069 + index * 214013 + 0x5eed1234) & 0xffffffff)
            if fresh: machine.restore(saved)
            stubs.calls, stubs.nested, stubs.rng, stubs.mode = [], 0, seed, mode
            machine.budget, machine.profile = budget, {}
            process.stdin.write(f"R {fresh} {index} {seed} {pri} {mode} {budget}\n")
            process.stdin.flush()
            outcome, result = "", "N"
            try:
                error, value = machine.execute(index, pri=pri)
                outcome = str(error)
                if value is not None: result = str(value)
            except reference.HostFault: outcome = "H"
            except reference.Limit: outcome = "L"
            profile = "".join(f"{op}:{n}," for op, n in sorted(machine.profile.items()))
            expected = ["R", outcome, result, state(machine), byte_hex(machine.image),
                        "".join(c + ";" for c in stubs.calls) or "-", profile,
                        str(machine.budget), str(stubs.nested)]
            compare(read_line(reader, process), expected, digest)
            counts["runs"] += 1
            counts["native_calls"] += len(stubs.calls)
            counts["nested_runs"] += stubs.nested
            counts["instructions"] += budget - machine.budget
            counts["host_faults"] += outcome == "H"
            counts["budget_refused"] += outcome == "L"
            if outcome not in ("H", "L", "0"):
                errors = counts["vm_errors"]
                errors[outcome] = errors.get(outcome, 0) + 1
    finally:
        process.stdin.close()
        if process.poll() is None:
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        process.stdout.close()
        reader.shutdown(wait=True)
        # Check even an early loader refusal, without hiding an active mismatch.
        if process.returncode and sys.exc_info()[0] is None:
            raise ValueError("native transport refused")


def read_line(reader, process):
    # A stuck child cannot indefinitely block an unattended optional validation.
    future = reader.submit(process.stdout.readline, 512 * 1024 * 1024 + 1)
    try:
        line = future.result(timeout=30)
    except concurrent.futures.TimeoutError:
        process.kill()
        process.wait()
        raise ValueError("native transport timed out") from None
    if len(line) > 512 * 1024 * 1024:
        raise ValueError("native transport trace exceeds explicit resource limit")
    return line


def compare(actual, expected, digest):
    row = actual.rstrip("\r\n").split("\t")
    if row != expected:
        # Intentionally omit raw paths, entry names and image content.
        mismatch = next((i for i, (a, b) in enumerate(zip(row, expected)) if a != b),
                        min(len(row), len(expected)))
        raise ValueError(f"private AMX state mismatch at field {mismatch}")
    digest.update(("\t".join(row) + "\n").encode("ascii"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True, choices=["rta"])
    parser.add_argument("--directory", required=True, type=pathlib.Path)
    parser.add_argument("--research-dir", required=True, type=pathlib.Path)
    parser.add_argument("--native", required=True, type=pathlib.Path)
    parser.add_argument("--padding", type=int, default=256, choices=[0, 256])
    parser.add_argument("--budget", type=int, default=200000)
    parser.add_argument("--report", type=pathlib.Path)
    args = parser.parse_args()
    if not 1 <= args.budget <= 1000000:
        parser.error("budget must be in 1..1000000")
    if not args.directory.is_dir() or not args.native.is_file():
        parser.error("select a directory and built native validation transport")
    source = args.research_dir / "tools/ref_amx.py"
    if hashlib.sha256(source.read_bytes()).hexdigest() != REFERENCE:
        parser.error("AMX reference revision differs from the pin")
    pinned = subprocess.check_output(["git", "-C", str(args.research_dir), "show",
                                      COMMIT + ":tools/ref_amx.py"])
    if hashlib.sha256(pinned).hexdigest() != REFERENCE:
        parser.error("published reference pin mismatch")
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("private_amx_reference", source)
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    digest = hashlib.sha256()
    counts = dict(files=0, size_refused=0, load_compared=0, load_refused=0,
                  relocation_refused=0, decoded=0, entrypoints=0, runs=0,
                  native_calls=0, nested_runs=0, instructions=0, host_faults=0,
                  budget_refused=0, vm_errors={})
    # Never follow links out of the selected private input tree.
    files = sorted(p for p in args.directory.rglob("*") if p.suffix.lower() == ".scr"
                   and p.is_file() and not p.is_symlink()
                   and not any(parent.is_symlink() for parent in p.parents))
    if not files:
        parser.error("selected directory has no script files")
    for path in files:
        counts["files"] += 1
        run_script(path, args.native.resolve(), reference, args.padding, args.budget, digest, counts)
    report = dict(schema_version=1, game=args.game, reference_sha256=REFERENCE,
                  reference_commit=COMMIT, padding_bytes=args.padding,
                  instruction_budget=args.budget, counts=counts,
                  exact_trace_sha256=digest.hexdigest(),
                  scope="Private-script Python parity with identical authored native stubs; no engine-native or console claim")
    text = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError):
        print("Private AMX comparison failed; no script contents were published.", file=sys.stderr)
        raise SystemExit(1)
