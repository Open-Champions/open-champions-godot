#!/usr/bin/env python3
"""Independent, asset-free Creature_takeDamage vectors from immutable RTA models.

Only supplied memory/callback recipes are authored here. HP arithmetic, pointer
selection, write order and callback dispatch execute the pinned Python method.
"""
import argparse
import collections
import gzip
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import types

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'tools/ref_creature.py', 'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'docs/spec/creature-tick.md', 'sha256': 'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
COMPOSITION_POLICY = 'rta-creature-hp-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')
SCENARIOS = ('basic', 'inert', 'ignore', 'hp_zero', 'hp_negative', 'amount_negative', 'amount_zero',
             'subnormal', 'large_finite', 'nan_like', 'signed_zero', 'angle_quadrants', 'attacker_null',
             'self', 'projectile', 'projectile_null_node', 'projectile_null_target', 'effect',
             'effect_null_node', 'effect_null_target', 'precedence', 'stale_target', 'clear_mutate_hp',
             'clear_mutate_route', 'clear_throw', 'link_mutate_hp', 'link_throw', 'missing_handlers',
             'alias_flags', 'callback_store', 'repeat_hits', 'random_bits')
C, A, OWNER_NODE, EFFECT_NODE, LINK_NODE = 0x10000, 0x20000, 0x30000, 0x30100, 0x30200
G_SIM_FRAME = 0x44fdcc
SEEDS = (0, 1, 0xdeadbeef, 0xffffffff)


def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, bytes): return 'b' + value.hex()
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned creature state')


def load_references(research_dir, scratch_dir):
    """Never import a live research working-tree file or follow its module path."""
    scratch_dir = pathlib.Path(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)
    modules = {}
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    try:
        with tempfile.TemporaryDirectory(prefix='pinned-', dir=scratch_dir) as directory:
            for name in ('core', 'creature', 'specification'):
                pin = SPECIFICATION_EVIDENCE if name == 'specification' else REFERENCES[name]
                source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']],
                                        capture_output=True, check=True, timeout=30).stdout
                if hashlib.sha256(source).hexdigest() != pin['sha256']:
                    raise ValueError('immutable reference hash mismatch: ' + name)
                path = pathlib.Path(directory) / pathlib.Path(pin['path']).name
                path.write_bytes(source)
                if name == 'specification': continue
                module = types.ModuleType('pinned_creature_hp_' + name)
                module.__file__ = str(path)
                sys.modules[module.__name__] = module
                exec(compile(source, str(path), 'exec'), module.__dict__)
                modules[name] = module
                if name == 'core': sys.modules['ref_core'] = module
    finally:
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core', None)
        else: sys.modules['ref_core'] = previous
    return modules


class CallbackFailure(Exception):
    pass


def run_case(modules, config, variant, scenario):
    core, reference = modules['core'], modules['creature']
    core.set_fpu(MODELS[config // 4], bool(config & 2), bool(config & 1))
    memory = reference.Memory()
    ordered_events, callback_pokes = [], []

    class TracedModel(reference.Model):
        # Instrumentation only: original code owns mutations and callback lookup.
        def _w(self, address, size, value):
            ordered_events.append(['write', address, size, value & ((1 << (8 * size)) - 1)])
            return super()._w(address, size, value)

        def call(self, name, *args):
            canonical = [v & 0xffffffff if isinstance(v, int) else v for v in args]
            ordered_events.append(['call', name, canonical])
            return super().call(name, *args)

    model = TracedModel(memory)
    p = memory.poke
    # Sparse zero bytes remain present: the comparison is the entire memory map,
    # not just selected output fields or nonzero differences.
    p(C + 0xc, 4, 0x20)
    p(C + 0x10, 4, core.from_int(1 + variant))
    p(C + 0x14, 4, core.from_int(2 - variant))
    p(C + 0xa8, 8, 0x80000000 | variant)
    p(C + 0xc8, 4, 0x3f000000)
    p(C + 0x227, 1, 0xa5)
    p(C + 0x228, 4, 0x41200000)
    p(C + 0x22c, 4, 0x41a00000)
    p(C + 0x230, 1, 0x5a)
    p(C + 0x231, 1, 0)
    p(C + 0x232, 2, 0x1234)
    p(C + 0x2dc, 4, 0x40000 | variant)
    p(C + 0x2e0, 2, 0x5566)
    p(C + 0x2e2, 2, 0xcafe)
    p(C + 0x2e4, 4, LINK_NODE)
    p(C + 0x304, 4, 0x76543210)
    for offset, value in zip((0x340, 0x344, 0x348, 0x34c, 0x350), (0x41200000, 0x3f800000, 0xbf800000, 0, 0x7fc00000)):
        p(C + offset, 4, value)
    p(C + 0x360, 4, EFFECT_NODE)
    p(C + 0x3af, 1, 0x69)
    p(C + 0x3b0, 2, 0x1234 + variant)
    p(C + 0x3b2, 1, 0x96)
    p(A + 0xc, 4, 0x40)
    p(A + 0x10, 4, core.from_int(-3 + variant))
    p(A + 0x14, 4, core.from_int(7 - 2 * variant))
    p(A + 0x4c, 4, OWNER_NODE)
    p(OWNER_NODE + 0xc, 4, 0x40000 + variant * 0x100)
    p(EFFECT_NODE + 0xc, 4, 0x50000 + variant * 0x100)
    p(LINK_NODE + 0xc, 4, 0x60000 + variant * 0x100)
    p(G_SIM_FRAME, 4, (0, 1, 0x7fffffff, 0xffffffff)[variant])
    attacker, flags, amount = A, 0x80000 | (1 << variant), 0x40400000

    if scenario == 'inert': p(C + 0x231, 1, (1, 0x7f, 0x80, 0xff)[variant])
    elif scenario == 'ignore': p(C + 0xa8, 8, 0x1000000000000 | (variant << 60))
    elif scenario == 'hp_zero': p(C + 0x228, 4, (0, 0x80000000, 0, 0x80000000)[variant])
    elif scenario == 'hp_negative': p(C + 0x228, 4, (0xbf800000, 0xc1200000, 0xff7fffff, 0x80000001)[variant])
    elif scenario == 'amount_negative': amount = (0xbf800000, 0xc1200000, 0xff7fffff, 0x80000001)[variant]
    elif scenario == 'amount_zero': amount = (0, 0x80000000, 0, 0x80000000)[variant]
    elif scenario == 'subnormal':
        p(C + 0x228, 4, (1, 0x80000001, 0x007fffff, 0x807fffff)[variant])
        p(C + 0xc8, 4, (0x007fffff, 1, 0x807fffff, 0x80000001)[variant])
        amount = (0x80000001, 1, 0x007fffff, 0x807fffff)[variant]
    elif scenario == 'large_finite':
        p(C + 0x228, 4, (0x7f7fffff, 0x7f000000, 0xff7fffff, 0x00800000)[variant])
        p(C + 0xc8, 4, (0x7f7fffff, 0xff7fffff, 0x00800000, 0x80800000)[variant])
        amount = (0xff7fffff, 0x7f7fffff, 0x7f7fffff, 1)[variant]
    elif scenario == 'nan_like':
        p(C + 0x228, 4, (0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000)[variant])
        p(C + 0xc8, 4, 0x7fffffff)
        amount = (0x7f800000, 0x7fffffff, 0xffffffff, 0xffc00000)[variant]
    elif scenario == 'signed_zero':
        p(C + 0x228, 4, (0, 0x80000000, 0, 0x80000000)[variant])
        p(C + 0xc8, 4, (0x80000000, 0, 0, 0x80000000)[variant])
        amount = (0, 0, 0x80000000, 0x80000000)[variant]
    elif scenario == 'angle_quadrants':
        p(C + 0x10, 4, 0); p(C + 0x14, 4, 0)
        x, y = ((1, 1), (-1, 1), (-1, -1), (1, -1))[variant]
        p(A + 0x10, 4, core.from_int(x)); p(A + 0x14, 4, core.from_int(y))
    elif scenario == 'attacker_null': attacker = 0; flags |= 0x10000000
    elif scenario == 'self': attacker = C
    elif scenario.startswith('projectile'):
        p(A + 0xc, 4, 0xc0)
        if scenario == 'projectile_null_node': p(A + 0x4c, 4, 0)
        elif scenario == 'projectile_null_target': p(OWNER_NODE + 0xc, 4, 0)
    elif scenario.startswith('effect'):
        flags |= 0x10000000
        if scenario == 'effect_null_node': p(C + 0x360, 4, 0)
        elif scenario == 'effect_null_target': p(EFFECT_NODE + 0xc, 4, 0)
    elif scenario == 'precedence':
        flags |= 0x10000000; p(A + 0xc, 4, 0xc0)
        if variant == 1: p(A + 0x4c, 4, 0)
        elif variant == 2: p(OWNER_NODE + 0xc, 4, 0)
        elif variant == 3: p(OWNER_NODE + 0xc, 4, 0xffffffff)
    elif scenario == 'stale_target':
        flags |= 0x10000000
        p(EFFECT_NODE + 0xc, 4, (0xdeadbeef, 0xffffffff, C + 0x229, 1)[variant])
    elif scenario == 'clear_mutate_route': flags |= 0x10000000
    elif scenario == 'alias_flags':
        attacker = C + 0x2d8
        p(C + 0x2e4, 4, LINK_NODE | 0x80)
        p(attacker + 0x10, 4, core.from_int(variant + 1))
        p(attacker + 0x14, 4, core.from_int(-variant - 1))
        p(attacker + 0x4c, 4, OWNER_NODE)
    elif scenario == 'random_bits':
        seed = SEEDS[variant]
        values = []
        for _ in range(8):
            seed = (seed * 1664525 + 1013904223) & 0xffffffff
            values.append(seed)
        for address, value in zip((C + 0x228, C + 0xc8, C + 0x10, C + 0x14, A + 0x10, A + 0x14), values): p(address, 4, value)
        amount, flags = values[6], values[7]

    def callback_poke(address, size, value):
        value &= (1 << (8 * size)) - 1
        callback_pokes.append([len(model.calls), address, size, value])
        memory.poke(address, size, value)

    def change_hp():
        callback_poke(C + 0x228, 4, (0xc1200000, 0x3f800000, 0x7fc00000, 0x80000000)[variant])
        callback_poke(C + 0xc8, 4, (0x40000000, 0xbf800000, 0xff7fffff, 1)[variant])

    def clear(head):
        callback_poke(head, 4, 0)
        if scenario in ('clear_mutate_hp', 'clear_throw'): change_hp()
        if scenario == 'clear_mutate_route':
            callback_poke(A + 0xc, 4, (0x80, 0, 0, 0x80)[variant])
            callback_poke(A + 0x4c, 4, 0 if variant == 3 else OWNER_NODE)
            callback_poke(OWNER_NODE + 0xc, 4, 0xfeed0000 + variant)
            if variant == 2:
                callback_poke(C + 0x231, 1, 0xff)
                callback_poke(C + 0xa8, 8, 0x1000000000000)
        if scenario == 'callback_store': memory.store(C + 0x228, 4, 0x3f800000 + variant)
        if scenario == 'clear_throw': raise CallbackFailure('authored clear failure')

    def link(head, back, obj):
        callback_poke(head, 4, LINK_NODE)
        callback_poke(LINK_NODE + 0xc, 4, obj)
        if scenario in ('link_mutate_hp', 'link_throw'): change_hp()
        if scenario == 'link_mutate_hp' and variant == 3: callback_poke(head, 4, 0x12345678)
        if scenario == 'callback_store': memory.store(C + 0xc8, 4, 0xbf800000 + variant)
        if scenario == 'link_throw': raise CallbackFailure('authored link failure')

    model.cb.update({'handle_clear': clear, 'handle_link': link})
    if scenario == 'missing_handlers':
        if variant in (0, 3): model.cb.clear()
        elif variant == 1: del model.cb['handle_clear']
        else: del model.cb['handle_link']
    results = []
    amounts = (0x3f800000, 0x41200000, 0xc0a00000, 0x80000000) if scenario == 'repeat_hits' else (amount,)
    for index, hit in enumerate(amounts):
        try:
            result = model.take_damage(C, attacker, flags | (index << 8), hit)
            results.append(result)
        except CallbackFailure:
            results.append('CallbackFailure')
    return [results, sorted(memory.bytes.items()), model.writes, memory.log, model.calls, callback_pokes, model.draws, ordered_events]


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None:
        scratch_dir = pathlib.Path(__file__).resolve().parents[1] / 'work' / 'creature-hp-generator'
    modules = load_references(research_dir, scratch_dir)
    output = pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):
        raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True, exist_ok=True)
    vector_hash, trace_hash = hashlib.sha256(), hashlib.sha256()
    trace_fnv = 14695981039346656037
    counts, number = collections.Counter(), 0
    with output.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as stream:
        header = b'# creature_hp_vectors_v1\n'
        stream.write(header); vector_hash.update(header)
        for config in range(16):
            for variant in range(4):
                for scenario in SCENARIOS:
                    first = sc(run_case(modules, config, variant, scenario))
                    second = sc(run_case(modules, config, variant, scenario))
                    if first != second:
                        raise AssertionError('fresh reference executions diverged')
                    row = ('\t'.join([scenario, str(config), f'{number};{variant}', 'ok', first]) + '\n').encode('ascii')
                    stream.write(row); vector_hash.update(row); trace_hash.update(row)
                    for byte in row:
                        trace_fnv = ((trace_fnv ^ byte) * 1099511628211) & 0xffffffffffffffff
                    counts[scenario] += 1; number += 1
    manifest = {
        'schema_version': 1, 'suite': 'creature-hp',
        'reference_sha256': REFERENCE_SHA256, 'references': REFERENCES,
        'specification_evidence': SPECIFICATION_EVIDENCE, 'composition_policy': COMPOSITION_POLICY,
        'generator_sha256': hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'vector_count': number, 'vector_sha256': vector_hash.hexdigest(),
        'compressed_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
        'expected_trace_sha256': trace_hash.hexdigest(), 'expected_trace_fnv1a64': f'{trace_fnv:016x}',
        'operations': dict(sorted(counts.items())),
        'settings': {'models': list(MODELS), 'mul_bias': [False, True], 'div_nearest': [False, True]},
        'replicas_per_case': 2,
        'trace_columns': ['operation', 'configuration', 'id;variant', 'status', 'results_complete_memory_writes_calls_pokes_draws_ordered_events'],
        'scope': 'Complete pinned Creature_takeDamage slot2 at0x116638 on authored sparse memory with explicit supplied handle callbacks. RTA only; not pending-damage reactions, death processing, gameplay or a Session/combat/object adapter.',
        'composition_contract': {
            'reference_execution': 'Both modules and the prose specification are read using immutable git show, SHA256-verified and temporarily materialized only under ignored work. No live research module import or C++ result feeds expectations.',
            'memory': 'Complete sorted sparse byte map, including explicitly stored zero bytes. Unwritten bytes read zero without materialization. Raw addresses and stale targets remain numeric, never host pointers.',
            'callbacks': 'Authored clear/link callbacks modify supplied memory. Pokes carry callback ordinal separately; Memory.store also appends its own log. Neither operation is silently promoted to a Model write. Missing handlers explicitly use the reference zero-result policy.',
            'ordering': 'A recording subclass wraps original Model._w and Model.call without replacing their behavior. Ordered write/call events are recorded before the original operation, matching original model logs and exception timing.',
            'exceptions': 'Only the authored CallbackFailure is normalized in result entries; unexpected reference errors fail generation. There is no failure latch or rollback in this primitive.',
            'arithmetic': 'Every configuration selects the pinned integer-modeled core FPU. NaN-like labels identify raw patterns; no IEEE host-float behavior is substituted.',
            'boundary': 'Native address-width, recursion and allocation/history limits are independently tested wrapper refusals, not attributed to the unbounded Python memory implementation.',
        },
    }
    output.with_name(output.name[:-7] + '.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n', encoding='ascii', newline='\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path('native/tests/vectors/creature_hp.tsv.gz'))
    parser.add_argument('--scratch-dir', type=pathlib.Path)
    args = parser.parse_args()
    manifest = generate(args.research_dir, args.output, args.scratch_dir)
    print(json.dumps({key: manifest[key] for key in ('vector_count', 'vector_sha256', 'expected_trace_sha256', 'expected_trace_fnv1a64')}))


if __name__ == '__main__':
    main()
