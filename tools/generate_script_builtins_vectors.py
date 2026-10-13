#!/usr/bin/env python3
"""Synthetic exact-state vectors for the pinned RTA variable built-ins subset."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import random
import subprocess
import sys
from pathlib import Path

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_natives.py': '5ba711f93299a6cdb684651ae5a3dda5c0ddc5370baeea3a54ff123f1cc44839',
    'docs/spec/script-builtins.md': '2beea1e91b3f71770a3d2ecd963fa856b9ee78a08647a2e218c03c53678cc2b4',
    'tools/difftest_natives.py': 'c5e3d4367db8919ce8dd73e106fd6b8e0510cc2889e801bdc17203e5eb83251b',
    'tools/ref_amx.py': '091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_objects.py': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2',
}
NATIVES = ['getv', 'setv', 'addv', 'getpv', 'setpv']
FIELDS = ['cip', 'frm', 'hea', 'stk', 'stp', 'flags', 'curline', 'curfile',
          'dbgcode', 'dbgaddr', 'dbgparam', 'error', 'params']


def number(value):
    return f'{value & 0xffffffff:08x}'


def text(value):
    return value.hex() or '-'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('native/tests/vectors/script_builtins.tsv.gz'))
    args = parser.parse_args()
    for path, pin in PINS.items():
        current = (args.research_dir / path).read_bytes()
        pinned = subprocess.check_output(['git', '-C', str(args.research_dir), 'show', COMMIT + ':' + path])
        if hashlib.sha256(current).hexdigest() != pin or hashlib.sha256(pinned).hexdigest() != pin:
            parser.error('reference pin mismatch: ' + path)
    # Imports are read-only, including their dependencies: never emit __pycache__.
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('natives_reference', args.research_dir / 'tools/ref_natives.py')
    R = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(R)
    A = sys.modules['ref_amx']
    rows = []
    counts = {}
    errors = {}
    mode = 0
    g = m = None

    def reset():
        nonlocal g, m
        g = R.Game()
        image = A.build_image([0], data=bytes(4096), natives=NATIVES, stack_cells=128)
        m = A.Machine(image, natives=g.natives())
        assert m.load() == 0
        g.host.handlers['memcard_requestConfigSave'] = save

    def save():
        if mode == 1:
            raise RuntimeError('authored callback exception')
        if mode in (2, 3):
            g.var_set(b'Callback', g.var_get(b'Callback') + 1)
            g.profile_set(b'Unsaved', 99, False)
        if mode == 3:
            g._slots.clear()
        return 0xdeadbeef  # ignored by profile_set

    def snap():
        memory = ','.join(f'{(page << 12) + i:08x}:{value:02x}'
                          for page, data in sorted(g.mem.pages.items())
                          for i, value in enumerate(data) if value) or '-'
        writes = ','.join(f'{address:08x}:{size:08x}' for address, size in g.mem.writes) or '-'
        slots = ','.join(f'{slot:08x}:{address & 0xffffffff:08x}' for slot, address in sorted(g._slots.items())) or '-'
        assert all(name == 'memcard_requestConfigSave' and not params for name, params in g.host.log)
        events = ','.join(name for name, _ in g.host.log) or '-'
        registers = ','.join(number(getattr(m, name)) for name in FIELDS)
        return '|'.join([memory, writes, slots, events, registers, bytes(m.image).hex()])

    def emit(op, name=b'', value=0, pack=False, address=0, count=8, cb=0):
        nonlocal mode
        mode = cb
        if op == 'reset':
            reset()
        g.mem.writes.clear()
        g.host.log.clear()
        result = '-'
        try:
            if op in NATIVES or op == 'nulcell':
                m.set_string(0, name, pack)
                if op == 'nulcell':
                    m.set_cell(0, 0x80000000)
                    m.set_cell(4, 0)
                m.set_cell(3000, count)
                m.set_cell(3004, address)
                m.set_cell(3008, value)
                m.set_cell(3012, 0xfacefeed)
                m.error = 7
                error, result = m._call_native(NATIVES.index('setv' if op == 'nulcell' else op), 3000)
                assert error == 0
                result = number(result)
            elif op in ('shortadd', 'shortset'):
                m.set_string(0, name, False)
                params = len(m.image) - m.data_offset - 8
                m.set_cell(params, 0)
                m.set_cell(params + 4, 0)
                m.error = 7
                m._call_native(NATIVES.index('addv' if op == 'shortadd' else 'setv'), params)
            elif op == 'get':
                result = number(g.var_get(name))
            elif op == 'set':
                g.var_set(name, value)
            elif op == 'pget':
                result = number(g.profile_get(name))
            elif op == 'pset':
                g.profile_set(name, value, pack)
            elif op == 'clear':
                g.vars_clear_all()
            elif op == 'index':
                entry = g.var_by_index(R.s32(value))
                result = '-' if entry is None else text(entry[0]) + ':' + number(entry[1])
            elif op == 'forget':
                g._slots.clear()
            elif op == 'unknown':
                R.GameState._missing('unported')(m, [])
            elif op != 'reset':
                raise AssertionError(op)
        except R.StackGarbage:
            result = 'StackGarbage'
        except A.HostFault:
            result = 'HostFault'
        except RuntimeError:
            result = 'CallbackFault'
        inputs = '|'.join([text(name), number(value), str(int(pack)), number(address), number(count), str(cb)])
        rows.append('\t'.join([op, inputs, result, snap()]))
        counts[op] = counts.get(op, 0) + 1
        if result in ('StackGarbage', 'HostFault', 'CallbackFault'):
            errors[result] = errors.get(result, 0) + 1

    # Stateful edge sequences: insertion on read; equal writes; case folding;
    # empty names, lower-bound/upper-bound clamp, signed32 overflow and callbacks.
    emit('reset')
    for name in (b'One', b'TWO', b'', b'A-Z_09 !', b'xyz'):
        emit('getv', name, count=0)
        for value in (-2147483648, -129, -128, -1, 0, 1, 127, 128, 2147483647):
            emit('setv', name, value, pack=True, count=4)
            emit('getv', name.lower(), count=12)
            emit('addv', name, value, count=-4)
        emit('setpv', name, 100, cb=2)
        emit('setpv', name.lower(), 100, cb=1)  # equal => callback not called
        emit('setpv', name, -200, cb=1)  # write prefix survives callback exception
        emit('getpv', name)
    emit('clear')
    for index in (-2147483648, -1, 0, 1, 2, 3, 4, 5, 2147483647):
        emit('index', value=index)
    # Known 0x20 slot survives these handlers, including rejected heap/top/negative
    # pointers; a rejected pointer before conversion or after forgetting is unknown.
    emit('reset')
    emit('getv', b'ignored', address=4096)
    emit('setv', b'Last', 70)
    for op in NATIVES:
        for address in (4096, 4608, -4):
            emit(op, b'Current', 5, address=address)
    emit('forget')
    emit('getpv', b'ignored', address=4096)
    emit('setpv', b'Nested', 12, cb=3)
    emit('getv', b'ignored', address=4096)
    emit('unknown')
    for address in (4, 8, 2, -4, 4604):
        emit('getv', b'Address', address=address)
    emit('getv', b'Count', count=10000)
    emit('shortadd', b'CreatedBeforeFault')
    emit('shortset', b'NotCreated')
    # Strings and faults retain conversion slot even when subsequent processing
    # refuses. 159-byte names fit; 160 do not. High bytes are explicitly refused.
    for pack in (False, True):
        for length in (0, 1, 2, 3, 4, 79, 159, 160, 161):
            emit('getv', b'Q' * length, pack=pack)
        for name in (b'\x80', b'x\xff', b'\xffabc', b'\x7fAZ'):
            emit('setv', name, 55, pack=pack)
    # Direct helpers, with profile save disabled and index traversal retained.
    emit('reset')
    for value in (-129, -128, 0, 127, 128):
        emit('set', b'Direct', value)
        emit('get', b'DIRECT')
        emit('pset', b'Profile', value, pack=False)
        emit('pget', b'PROFILE')
        emit('pset', b'Profile', value, pack=True, cb=1)
    emit('pset', b'NewZero', 0, pack=True)
    emit('pset', b'NewZero', 0, pack=True, cb=1)
    emit('clear')
    emit('index', value=0)
    emit('index', value=1)
    # Unpacked strings terminate on a zero cell, not a zero low byte. A negative
    # first cell can therefore yield an embedded NUL. Preserve the pinned model's
    # byte-string table semantics rather than guessing C-library behavior.
    emit('reset')
    emit('nulcell', value=55)
    for name in (b'a\0b', b'\0', b'A\0XYZ'):
        emit('set', name, 17)
        emit('get', name)
        emit('pset', name, -4, pack=True)
        emit('pget', name)
    # Table walks span pages and the quest table's nominal 0x2000-byte area.
    # The reference has no bounds check; no adjacent service is inferred here.
    emit('reset')
    for i in range(70):
        name = f'Entry{i:03d}_'.encode() + b'x' * 141
        emit('set', name, i - 35)
        emit('get', name.upper())
    for i in (-1, 0, 32, 69, 70):
        emit('index', value=i)
    emit('clear')
    # Seeded sequences use no game data. Include every native and host helper,
    # with packed/unpacked strings, stale slots, too few/too many pushed cells.
    rng = random.Random(0x4e415449)
    names = [b'Alpha', b'ALPHA', b'beta', b'Gamma', b'', b'__', b'a b', b'9']
    for block in range(12):
        emit('reset')
        for i in range(80):
            op = rng.choice(NATIVES + ['get', 'set', 'pget', 'pset', 'clear', 'index', 'forget'])
            value = rng.choice([-2147483648, -129, -128, -1, 0, 1, 127, 128, 2147483647, R.s32(rng.getrandbits(32))])
            emit(op, rng.choice(names), value, bool(rng.getrandbits(1)),
                 rng.choice([0, 0, 0, 4096]), rng.choice([-4, 0, 4, 8, 12]), rng.randrange(4))
    output = ('# rta-script-builtins-variables-v1\n# op\tinputs\tresult\tcomplete_state\n' + '\n'.join(rows) + '\n').encode('ascii')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('wb') as stream:
        with gzip.GzipFile(filename='', fileobj=stream, mode='wb', mtime=0) as compressed:
            compressed.write(output)
    manifest = {
        'schema': 'rta-script-builtins-variables-v1', 'rows': len(rows),
        'operations': counts, 'errors': errors, 'reference_commit': COMMIT,
        'reference_files': PINS, 'vectors_sha256': hashlib.sha256(output).hexdigest(),
        'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'scope': 'Synthetic variable tables, ordered memory writes, string slot 0x20, save callback events and full owned AMX image/register state. Five built-ins only.',
        'unsupported_natives': [name for name in R.NATIVE_NAMES if name not in NATIVES],
    }
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(f'{len(rows)} exact state transitions, {len(counts)} operations; {errors}')


if __name__ == '__main__':
    main()
