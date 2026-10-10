#!/usr/bin/env python3
"""Authored, asset-free composition of five immutable RTA Python model objects.

The composition (host task order, failure latch, budgets and replay inputs) is a
new test policy, not a recovered engine loop or game-native implementation.
"""
import argparse
import collections
import gzip
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import types

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'items': {'commit': '14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512', 'path': 'tools/ref_items.py', 'sha256': '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e'},
    'combat': {'commit': '7212ecfecbeae8dd2015f6c82897226f49a94695', 'path': 'tools/ref_combat.py', 'sha256': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
    'amx': {'commit': '5ad489a445d41612d0795c063e1ce67557d5c441', 'path': 'tools/ref_amx.py', 'sha256': '091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f'},
    'netwire': {'commit': '54258f9d7e281ef07f0675040e82e6264f9386cf', 'path': 'tools/ref_netwire.py', 'sha256': '4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'},
}
# Five-entry canonical JSON map is the single registry provenance identifier.
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
COMPOSITION_POLICY = 'rta-authored-session-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')


def load_references(research_dir):
    modules = {}
    for name, pin in REFERENCES.items():
        source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']], capture_output=True, check=True, timeout=30).stdout
        if hashlib.sha256(source).hexdigest() != pin['sha256']:
            raise ValueError('immutable reference hash mismatch: ' + name)
        module = types.ModuleType('pinned_session_' + name)
        sys.modules[module.__name__] = module
        exec(compile(source, 'pinned_' + name + '.py', 'exec'), module.__dict__)
        modules[name] = module
    return modules


def sc(value):
    if value is None:
        return '_'
    if isinstance(value, bool):
        return str(int(value))
    if isinstance(value, (bytes, bytearray)):
        return 'b' + bytes(value).hex()
    if isinstance(value, str):
        return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)):
        return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int):
        return str(value)
    raise TypeError('unowned or noninteger state in session trace')

GLOBAL_FIELDS = 'network_mode num_peers lif last_sent net_frame cycle phase modem last_hash_frame stall ended eq min_ready win long_stall kicked kick_state msg_attached sim_frame rand_seed sync_acc level_pending num_players'.split()
PEER_FIELDS = 'port send_frame msg_write msg_read newest acked lead sim silence heard local dropped kick oos drop_frame kicked_notice'.split()
DEFINITION_FIELDS = 'addr index preset name level weight count damage_min damage_max price armor attack slot category subtype damage_bits flags picture model_row'.split()
HEADER_FIELDS = 'size cod dat hea0 start num_publics publics_off num_natives natives_off num_libraries num_pubvars data_offset'.split()
NATIVE_NAMES = ('authored_set', 'authored_evaluate', 'authored_roll')
SCENARIOS = ('ready', 'delayed', 'no_ready', 'hold', 'no_sim', 'level_pending', 'native_failure', 'budget_failure', 'vm_error', 'hash_collision', 'message', 'message_failure')


def authored_definition(r):
    d = r.Definition()
    values = [r.A_DEFS, 0, False, b'authored-blade', 1, 0x3f800000, 1, 3, 9, 17, 5, 7, 0, 0, 0, 1, 0, None, 0]
    for key, value in zip(DEFINITION_FIELDS, values):
        setattr(d, key, value)
    return d


def authored_script(r, scenario):
    if scenario == 7:
        program = [('PROC',), 'loop:', ('JUMP', 'loop')]
    elif scenario == 8:
        program = [('PROC',), ('HALT', 11)]
    else:
        program = [('PROC',), ('PUSH.C', 3), ('PUSH.C', 0x15), ('PUSH.C', 8), ('SYSREQ.C', 0), ('STACK', 12),
                   ('PUSH.C', 0), ('SYSREQ.C', 1), ('STACK', 4), ('STOR.pri', 0),
                   ('PUSH.C', 0x41100000), ('PUSH.C', 0x40400000), ('PUSH.C', 8), ('SYSREQ.C', 2), ('STACK', 12), ('STOR.pri', 4), ('RETN',)]
    code, _ = r.assemble(program)
    return r.build_image(code, data=[0]*8, publics=[('probe', 0)], natives=list(NATIVE_NAMES), stack_cells=64, start=0)


def codec(c):
    return [[[x.index, x.pred, x.value, x.velocity] for x in c.ch], c.yaw_prev, c.yaw_pred, c.yaw_delta, c.pad46, c.count]


def sent(v):
    return [v.peer, v.ip, v.port, v.data, list(v.undefined)]


def network_snapshot(m, config):
    # Snapshot strips executable callbacks; the live session hook is separately recorded.
    return [[getattr(m, x) for x in GLOBAL_FIELDS],
            [[p.pack(), [getattr(p, x) for x in PEER_FIELDS], p.pad_prev, codec(p.recv_state), codec(p.send_state)] for p in m.peers],
            m.kick_acks, m.cam_yaw, m.input_lock, m.send_cycles, codec(m.sim_enc), codec(m.sim_dec), m.positions, m.pads,
            [m.pad_globals.start_block, m.pad_globals.mute, m.pad_globals.allow], [sent(x) for x in m.outbox], m.log,
            [], [0]*4, 0, 0, 0, 0, 0, [config//4, bool(config&2), bool(config&1)]]


class CompositionFailure(Exception):
    pass


class Oracle:
    """Authored ownership/failure policy around calls to the five pinned models."""
    def __init__(self, modules, config, seed, scenario):
        self.r = modules
        self.config, self.seed_initial, self.scenario = config, seed, scenario
        options = (MODELS[config//4], bool(config&2), bool(config&1))
        modules['core'].set_fpu(*options)
        modules['combat'].set_fpu(*options)
        self.network = modules['netwire'].Machine(2, 0, modules['netwire'].Fpu(*options))
        self.network.network_mode = 1
        self.network.rand_seed = 0
        self.network.run_tasks = self.task
        self.started = self.failed = False
        self.failure_operation = ''
        self.events = []
        self.definition = authored_definition(modules['items'])
        self.item = modules['items'].Item(self.definition)
        self.item.count = 1
        self.seed = modules['combat'].Seed(seed)
        self.refresh_combat_item()
        self.bindings = {name: self.native(name) for name in NATIVE_NAMES}
        self.vm = modules['amx'].Machine(authored_script(modules['amx'], scenario), self.bindings)
        error = self.vm.load()
        if error:
            raise AssertionError('authored image refused')
        self.vm._natives()
        # Declaration metadata belongs to the loaded script; later mutable VM
        # headers remain raw snapshot data and need not stay parseable.
        self.declared_native_names = [x.decode('latin1') if isinstance(x, bytes) else x
                                      for x in self.vm.native_names()]
        self.vm.profile = {}
        self.vm.budget = 100000
        if scenario in (10, 11):
            self.network.handlers[7] = self.message
            self.network.queue_message(7, bytes([25]))

    def guard(self, operation, function):
        if self.failed:
            raise CompositionFailure('latched')
        try:
            return function()
        except Exception:
            if not self.failed:
                self.failed = True
                self.failure_operation = operation
            raise

    def event(self, kind, name='', values=()):
        self.events.append([kind, name, self.network.net_frame, list(values)])

    def refresh_combat_item(self):
        d = self.definition
        c = self.r['combat']
        self.combat_definition = c.ItemDef(name=d.name.decode('latin1'), dmg_min=d.damage_min, dmg_max=d.damage_max,
            armor=d.armor, attack=d.attack, category=d.category, subtype=d.subtype, dmg_type=d.damage_bits, flags=d.flags)
        self.combat_item = c.Item(defn=self.combat_definition, count=self.item.count, flags=self.item.flags,
            mods=[[-1, 0, 0, 0] if m is None else list(m) for m in self.item.mods])

    def modifier(self, code, v0, v1=0, v2=0):
        def action():
            result = self.item.set_modifier(code, v0, v1, v2)
            self.refresh_combat_item()
            self.event('modifier', 'item', [code, v0, v1, v2, result])
            return result
        return self.guard('set_modifier', action)

    def evaluate(self):
        def action():
            i, c = self.r['items'], self.r['combat']
            self.refresh_combat_item()
            result = [i.item_weight(self.item), i.item_armor(self.item), i.item_attack_rating(self.item),
                      *i.item_damage_range(self.item, True), c.item_armor_value(self.combat_item),
                      c.item_attack_rating(self.combat_item), *c.item_damage_range(self.combat_item, True)]
            self.event('item', 'item', result)
            return result
        return self.guard('evaluate_item', action)

    def draw(self):
        def action():
            value = self.seed.draw()
            self.event('draw', '', [value, self.seed.state])
            return value
        return self.guard('draw', action)

    def roll(self, minimum, maximum):
        def action():
            result = self.r['combat'].base_roll(minimum, maximum, self.draw())
            self.event('roll', '', [minimum, maximum, result])
            return result
        return self.guard('roll_damage', action)

    def native(self, name):
        def function(_vm, params):
            label = 'probe:' + name
            def action():
                self.event('native', label, params)
                if name == 'authored_set':
                    result = self.modifier(params[0], params[1], 7, 0)
                elif name == 'authored_evaluate':
                    values = self.evaluate()
                    result = self.r['amx'].s32(self.r['core'].add(self.r['core'].from_int(values[2]), values[0]))
                elif self.scenario == 6:
                    self.draw()
                    raise CompositionFailure('authored native failure')
                else:
                    result = self.r['amx'].s32(self.roll(params[0]&0xffffffff, params[1]&0xffffffff))
                self.event('native_result', label, [result is not None, result or 0])
                return result
            return self.guard('native:' + label, action)
        return function

    def message(self, payload):
        def action():
            self.event('message', 'authored_message', payload)
            self.modifier(0x0d, payload[0])
            if self.scenario == 11:
                raise CompositionFailure('authored message failure')
        return self.guard('message:authored_message', action)

    def execute(self):
        def action():
            self.event('execute', 'probe', [0, 0, 0])
            self.vm.budget = 100000
            result = self.vm.execute(0)
            self.event('result', 'probe', [result[0], result[1] is not None, result[1] or 0])
            if result[0] != 0:
                raise CompositionFailure('vm returned error')
            return result
        return self.guard('execute', action)

    def task(self, _network):
        self.event('task', 'authored_script')
        self.execute()
        self.network.sim_frame += 1
        self.network.sync_acc += 17

    def submit(self, k, inbox):
        def action():
            self.started = True
            sample = struct.pack('<H6B', 257*(k+1), 128+k, 64+3*k, 255-k, 17*k, 31*k, 47*k)
            return self.network.net_main(sample, yaw=k*1234, camera_flag=k&1, frame_step=1,
                no_sim=self.scenario==4 and k<2, hold_arg=self.scenario==3 and k>=2, render_frame=k, inbox=inbox)
        return self.guard('submit', action)

    def snapshot(self):
        vm = self.vm
        script = [list(vm.snapshot()), vm.params, vm._parsed, hasattr(vm, 'size'), bool(vm.callback),
                  *[getattr(vm, name, 0) for name in HEADER_FIELDS], vm.budget,
                  None if vm._decoded is None else [[key, *value] for key, value in sorted(vm._decoded.items())],
                  None if vm.profile is None else sorted(vm.profile.items()),
                  None if vm._bound is None else [0 if x is None else 1 if x is self.r['amx']._FOREIGN else 2 for x in vm._bound],
                  None if vm._bound is None else [callable(x) for x in vm._bound],
                  [callable(vm._pending_natives[x]) for x in sorted(vm._pending_natives)],
                  sorted(vm._pending_natives), self.declared_native_names, sorted(self.bindings)]
        definition = [getattr(self.definition, name) for name in DEFINITION_FIELDS]
        definition[3] = definition[3].decode('latin1')
        item = self.item
        record = [item.count, item.flags, item.a, item.b, item.prefix, item.suffix,
                  [None if x is None else [x[0], x[1:]] for x in item.mods]]
        d = self.combat_definition
        combat_definition = [[ord(x) for x in d.name], d.dmg_min, d.dmg_max, d.armor, d.attack, d.category, d.subtype, d.dmg_type, d.flags]
        item_snapshot = ['definition', record, combat_definition, self.combat_item.count, self.combat_item.flags, self.combat_item.mods]
        return [['rta', self.config//4, bool(self.config&2), bool(self.config&1), 2, 0, 1, self.seed_initial, 0],
                self.started, self.failed, False, 0, self.failure_operation, ['authored_script'],
                [[7, 'authored_message']] if self.scenario in (10, 11) else [], network_snapshot(self.network, self.config), True,
                [['definition', definition]], [['item', item_snapshot]], [['probe', script]], self.seed.state, self.seed.draws, self.events]


def ready_packet(r, config):
    m = r.Machine(2, 1, r.Fpu(MODELS[config//4], bool(config&2), bool(config&1)))
    for frame in range(1, 9):
        m.peers[1].set_record(frame, r.rec12(frame*17, (128+frame, 64+frame, 255-frame, frame*9),
            (frame*11, frame*13), frame*123, frame&1), start=0, count=0)
    return m.build_packet(0, 1, 8, r.PKT_RAW_RELAY, 1, None, raw=True).data


def run_scenario(modules, config, seed, scenario):
    oracle = Oracle(modules, config, seed, scenario)
    packet = ready_packet(modules['netwire'], config)
    results = []
    if scenario == 5:
        oracle.network.level_pending = True
    for k in range(4):
        if scenario == 5 and k == 3:
            oracle.network.level_pending = False
        inbox = [packet] if (scenario == 1 and k == 2) or (scenario not in (1, 2) and k == 0) else []
        try:
            datagrams, held, steps = oracle.submit(k, inbox)
            results.append([[sent(x) for x in datagrams], held, steps])
        except (CompositionFailure, modules['amx'].Limit):
            results.append('failed')
    if scenario == 9:
        before = oracle.network.peers[0].sync(oracle.network.net_frame)
        armor = modules['combat'].item_armor_value(oracle.combat_item)
        oracle.modifier(0x0d, 99)
        after = oracle.network.peers[0].sync(oracle.network.net_frame)
        results.append([before == after, armor != modules['combat'].item_armor_value(oracle.combat_item)])
    return [results, oracle.snapshot()]


def generate(research_dir, output):
    modules = load_references(research_dir)
    output = pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):
        raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True, exist_ok=True)
    vector_hash = hashlib.sha256()
    trace_hash = hashlib.sha256()
    trace_fnv = 14695981039346656037
    counts = collections.Counter()
    seeds = (0, 1, 0xdeadbeef, 0xffffffff)
    number = 0
    with output.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as stream:
        header = b'# session_vectors_v1\n'
        stream.write(header)
        vector_hash.update(header)
        for config in range(16):
            for seed in seeds:
                for scenario, name in enumerate(SCENARIOS):
                    first = sc(run_scenario(modules, config, seed, scenario))
                    second = sc(run_scenario(modules, config, seed, scenario))
                    if first != second:
                        raise AssertionError('independent replay replicas diverged')
                    row = ('\t'.join([name, str(config), f'{number};{seed};{scenario}', 'ok', first]) + '\n').encode('ascii')
                    stream.write(row)
                    vector_hash.update(row)
                    trace_hash.update(row)
                    for byte in row:
                        trace_fnv = ((trace_fnv ^ byte) * 1099511628211) & 0xffffffffffffffff
                    counts[name] += 1
                    number += 1
    manifest = {
        'schema_version': 1,
        'suite': 'session-replay',
        'reference_sha256': REFERENCE_SHA256,
        'references': REFERENCES,
        'composition_policy': COMPOSITION_POLICY,
        'generator_sha256': hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'vector_count': number,
        'vector_sha256': vector_hash.hexdigest(),
        'compressed_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
        'expected_trace_sha256': trace_hash.hexdigest(),
        'expected_trace_fnv1a64': f'{trace_fnv:016x}',
        'operations': dict(sorted(counts.items())),
        'settings': {'models': list(MODELS), 'mul_bias': [False, True], 'div_nearest': [False, True]},
        'seeds': list(seeds),
        'replicas_per_case': 2,
        'trace_columns': ['operation', 'configuration', 'id;seed;scenario', 'status', 'step_results_and_complete_owned_snapshot'],
        'scope': 'Independent authored composition of pinned RTA module models; no original game loop, native handlers, movement, HP updates, UDP transport, console or Norrath parity claim.',
        'composition_contract': {
            'task_order': 'Ready-frame task executes authored AMX public, then explicitly increments supplied sim_frame and sync_acc by 1 and 17. This is a fixture policy, not a recovered engine scheduler.',
            'numeric_calls': 'Script callbacks call pinned items modifiers/statistics, explicit field-copy combat record adapter, pinned combat statistics and base_roll/Seed.draw, and pinned core from_int/add. Items retain their fixed Ps2 arithmetic; core/combat/netwire use the selected configuration.',
            'rng_domains': 'Combat seed/draw history is separate from network_seed (zero here). Drawing never implicitly publishes combat state to the wire hash.',
            'failure_policy': 'First callback/VM/budget exception latches its enclosing host operation, retains partial model state, and rejects later mutating submissions. VM nonzero errors latch execute. This wrapper policy is authored.',
            'budget': 'Authored per-execute budget100000, checked by pinned AMX model; native wrapper resource caps remain separately tested refusals.',
            'messages': 'Explicit authored message7 sets an item modifier; message failure preserves modifier and latches the host. No original game-message semantic claim.',
            'equality': 'Two separately constructed, identically configured replay replicas compare complete owned snapshots. These are equal local roles, not opposite transport endpoints.',
            'negative_anchor': 'hash_collision preserves the network sync record while changing converted combat armor. Full item/VM/network/RNG/events state is serialized, not the network hash alone.',
        },
    }
    manifest_path = output.with_name(output.name[:-7] + '.json')
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n', encoding='ascii', newline='\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path('native/tests/vectors/session_replay.tsv.gz'))
    args = parser.parse_args()
    manifest = generate(args.research_dir, args.output)
    print(json.dumps({key: manifest[key] for key in ('vector_count', 'vector_sha256', 'expected_trace_sha256', 'expected_trace_fnv1a64')}))


if __name__ == '__main__':
    main()
