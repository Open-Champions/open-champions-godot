#!/usr/bin/env python3
"""Synthetic RTA message transport/encoder vectors driven by the pinned model.
Never loads executables, original tables, or private inputs; research is read-only.
"""
import argparse
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import random
import subprocess
import sys

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_messages.py': 'af6ea5fea7ccb6a06ba9992547a87062d9c3070b7ab0e9737241274def52e12c',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_items.py': '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e',
    'tools/difftest_messages.py': 'efa79458e2bc239b8e6b83983d365b779ca09fb941b62241be470b6837b2e5ec',
    'docs/spec/game-messages.md': 'daa037829a8c182e4c925c32bd7c3b3647cc08a88af4230cdde922155de46fd8',
}
KINDS = [('drop',7),('equip',3),('unequip',2),('join_panel',3),('weapon_class',2),
         ('stat_point',2),('skill_point',3),('combine',4),('level_ack',2),('use_potion',5),
         ('quick_slot',3),('assign_spell',4),('menu_state',3),('recall',1),('player_left',1),
         ('savepoint',1),('disconnect_request',1),('new_world',0),('exit_countdown',0),
         ('checksum_start',0),('checksum',3),('load_level',0),('medal_ack',0),
         ('quick_cast',2),('buy',2),('sell',7),('shop_closed',1)]
SCHEMA = 'rta-messages-transport-v1'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def bhex(data):
    return data.hex() or '-'

def action(op, *args):
    return ' '.join([op] + [format(x & ((1 << 64)-1), 'x') if isinstance(x,int) else x for x in args])

class CallbackError(Exception):
    pass

def expected(R, script):
    mem = R.Memory()
    slot = 0
    local_throw = external_throw = False
    mutation = None
    def local(_):
        if local_throw:
            raise CallbackError()
        return slot
    def external(*_):
        if mutation:
            mem.poke(*mutation)
        if external_throw:
            raise CallbackError()
    machine = R.Machine(mem, callbacks={'net_getLocalPlayerSlot':local,
        **{name:external for name in ('cutscene_onMsg','voice_onMsg','dialog_onMsg','text_onMsg')}})
    results = []
    for command in script.split(';'):
        t = command.split()
        op = t[0]
        n = lambda i: int(t[i],16)
        payload = lambda i: bytes.fromhex(t[i]) if t[i] != '-' else b''
        try:
            if op == 'poke':
                mem.poke(n(1)&R.M32,n(2),n(3))
            elif op == 'local':
                slot = n(1)&R.M32
            elif op == 'throwlocal':
                local_throw = True
            elif op == 'throwexternal':
                external_throw = True
            elif op == 'mutate':
                mutation = (n(1)&R.M32,n(2),n(3))
            elif op == 'register':
                results.append(format(machine.register_handler(n(1)), 'x'))
            elif op == 'all':
                results.append(','.join(format(v,'x') for v in machine.register_all().values()))
            elif op in ('send','deliver'):
                getattr(machine,op)(n(1),payload(2))
                results.append('ok')
            elif op == 'encode':
                name, _ = KINDS[n(1)]
                args = [int(v,16) for v in t[3:]]
                if name == 'new_world':
                    args = [payload(2)]
                family, data = getattr(R,'msg_'+name)(*args)
                results.append(family+':'+bhex(data))
            else:
                raise AssertionError(op)
        except CallbackError:
            results.append('callback-error')
        except ValueError:
            results.append('error')
    out = 'M'+''.join(';'+format(a,'x')+':'+format(b,'x') for a,b in sorted(mem.bytes.items()))
    out += '/W'+''.join(';'+format(a,'x')+':'+format(s,'x')+':'+format(v,'x') for a,s,v in mem.log)
    out += '/C'
    for name,args in machine.calls:
        data, arg = (b'',args[0]) if name=='net_getLocalPlayerSlot' else args
        out += ';'+name+':'+bhex(data)+':'+format(arg,'x')
    return out+'/R;'+'|'.join(results)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=Path('native/tests/vectors/messages.tsv.gz'))
    args = parser.parse_args()
    for path, sha in PINS.items():
        if digest((args.research_dir/path).read_bytes()) != sha:
            parser.error('working source SHA mismatch: '+path)
        pinned = subprocess.check_output(['git','-C',str(args.research_dir),'show',COMMIT+':'+path])
        if digest(pinned) != sha:
            parser.error('pinned commit SHA mismatch: '+path)
    sys.dont_write_bytecode = True
    sys.path.insert(0,str((args.research_dir/'tools').resolve()))
    spec = importlib.util.spec_from_file_location('pinned_messages',args.research_dir/'tools/ref_messages.py')
    R = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(R)
    rows = []
    counts = {}
    def add(group,commands):
        script = ';'.join(commands)
        rows.append(group+'\t'+script+'\t'+expected(R,script))
        counts[group] = counts.get(group,0)+1
    rng = random.Random(0x20973)
    edges = [0,1,0x7f,0x80,0xff,0x100,0x7fff,0x8000,0xffff,0x10000,
             0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffff,0xffffffffffffffff]
    for kind,(name,count) in enumerate(KINDS):
        for seed in edges+[rng.getrandbits(64) for _ in range(64)]:
            values = [seed]*count
            add('encode-'+name,[action('encode',kind,'-',*values)])
            if count > 1:
                values = [rng.getrandbits(64) for _ in range(count)]
                add('encode-'+name,[action('encode',kind,'-',*values)])
    for name in [b'',b'authored-level',b'a\0b',bytes(range(256)),b'x'*300]:
        add('encode-new-world',[action('encode',17,bhex(name))])
    # Duplicate, null, holes, full-table overflow, and repeated startup registration.
    add('registration',['all','all',action('register',0)])
    for occupied in range(17):
        base = [action('poke',R.G_MSG_HANDLERS+4*i,4,0x100000+i) for i in range(occupied)]
        add('registration',base+['all','all'])
        for handler in [0,0x100000,0x198f64,0xffffffff]:
            add('registration',base+[action('register',handler),action('register',handler)])
    for _ in range(64):
        add('registration',[action('poke',R.G_MSG_HANDLERS+4*i,4,rng.choice([0,0x198f64,0x31f354,0x123456])) for i in range(16)]+['all'])
    # No family execution except null and the four documented external callbacks.
    for channel in [0,1,2,4,6,15,16,0xffffffff,0x40000000]:
        for data in [b'',b'\x00',b'\xff\x00\x80',bytes(range(24))]:
            for op in ['deliver','send']:
                add('delivery',['all',action(op,channel,bhex(data))])
    for channel in [0,1,2,6]:
        add('callback-failure',['all','throwexternal',action('deliver',channel,'010203')])
        add('callback-mutation',['all',action('mutate',0x1234,4,0xff123456),action('send',channel,'aaff')])
    # Network mode uses any nonzero signed byte; raw slot patterns and wrap positions.
    for mode in [1,0x7f,0x80,0xff]:
        for slot in [0,1,3,0xffffffff,0x80000000]:
            peer = (R.G_PEERS+R.s32(slot)*R.PEER_SIZE)&R.M32
            for pos in [0,0x1ffe,0x1fff,0x2000,0xffffffff]:
                for size in [0,1,13,16,255,256,300]:
                    data = bytes(rng.getrandbits(8) for _ in range(size))
                    add('ring',[action('poke',R.G_NETWORK_MODE,1,mode),action('local',slot),
                        action('poke',peer+0x44ac,4,pos),action('send',0x123456ff,bhex(data))])
    # Ordered sequences write over earlier ring bytes, including a complete ring lap.
    for size in [8191,8192,8193]:
        add('ring-lap',[action('poke',R.G_NETWORK_MODE,1,1),action('send',5,bhex(bytes((i&255 for i in range(size))))),action('send',7,'aabbcc')])
    for _ in range(32):
        commands = ['all',action('poke',R.G_NETWORK_MODE,1,1),action('local',rng.randrange(4))]
        for _ in range(20):
            commands.append(action('send',rng.getrandbits(32),bhex(bytes(rng.getrandbits(8) for _ in range(rng.randrange(33))))))
        add('ring-sequence',commands)
    add('local-callback-failure',[action('poke',R.G_NETWORK_MODE,1,1),'throwlocal',action('send',5,'aabb')])
    data = ('\n'.join(rows)+'\n').encode()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    compressed = io.BytesIO()
    with gzip.GzipFile(filename="",mode="wb",fileobj=compressed,mtime=0) as stream:
        stream.write(data)
    args.output.write_bytes(compressed.getvalue())
    manifest = {'schema':SCHEMA,'research_commit':COMMIT,'research_files':PINS,
        'generator_sha256':digest(Path(__file__).read_bytes()),'vectors_sha256':digest(data),
        'rows':len(rows),'groups':counts,'scope':'registration, all encoders, ring, null and external delivery'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(f'{len(rows)} exact message cases written')

if __name__ == '__main__':
    main()
