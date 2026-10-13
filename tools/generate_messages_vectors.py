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
    services = {}
    service_throw = None
    def local(_):
        if local_throw:
            raise CallbackError()
        return slot
    def external(*_):
        if mutation:
            mem.poke(*mutation)
        if external_throw:
            raise CallbackError()
    def service(name):
        def run(*args):
            if mutation:
                mem.poke(*mutation)
            if name == service_throw:
                raise CallbackError()
            return services.get(name, 0)
        return run
    machine = R.Machine(mem, callbacks={'net_getLocalPlayerSlot':local,
        **{name:external for name in ('cutscene_onMsg','voice_onMsg','dialog_onMsg','text_onMsg')},
        **{name:service(name) for name in ('net_isLocalSlot','res_find','sound_playOnObject','text_lookup','hud_showMessage')}})
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
            elif op == 'service':
                services[t[1]] = n(2)
            elif op == 'throwservice':
                service_throw = t[1]
            elif op == 'invoke':
                values = [int(v,16) for v in t[2:]]
                # Scalar API uses signed64 patterns, except masks/value/passive slot.
                unsigned = {'passive_level': (0,1), 'raise_passive_level': (1,),
                    'rebuild_abilities': (0,), 'on_assign_spell': (3,), 'mask_local_buttons': (2,)}
                values = [v if i in unsigned.get(t[1],()) else R.s64(v) for i,v in enumerate(values)]
                result = getattr(machine,t[1])(*values)
                results.append('ok' if result is None else format(result & R.M64,'x'))
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
        if name in ('net_getLocalPlayerSlot','cutscene_onMsg','voice_onMsg','dialog_onMsg','text_onMsg'):
            data, arg = (b'',args[0]) if name=='net_getLocalPlayerSlot' else args
            out += ';'+name+':'+bhex(data)+':'+format(arg,'x')
        else:
            out += ';'+name+':args'+''.join(':'+format(v & R.M64,'x') for v in args)
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
    # Player abilities / HUD controls. Every table/string is authored here.
    def poke(a, size, value):
        return action('poke', a & R.M32, size, value)
    def invoke(name, *args):
        return action('invoke', name, *args)
    def string(a, data):
        return [poke(a+i,1,b) for i,b in enumerate(data+b'\0')]
    def setup(slot=0):
        r = (R.G_PLAYERS+R.s32(slot)*R.REC_SIZE)&R.M32
        c = string(R.S_NEWSTYLE,b'newskill')+string(R.S_NEWSTYLE_HUD,b'newskill')+string(R.S_TABLE_END,b'END')
        c += string(R.G_OLD_ABILITIES+R.ABILITY_SIZE,b'END')
        c += string(R.G_OLD_ABILITIES,b'authored')
        c += [poke(r+R.R_ABILITIES+i*R.ABILITY_SIZE+0x20,2,-1) for i in range(15)]
        c += [poke(r+R.R_SKILL_RANKS+i*4,2,-1) for i in range(8)]
        return r,c
    for slot in [0,1,4,-1,0x80000000,0xffffffff]:
        r,c = setup(slot)
        for ident in [0,1,0x7fff,-2,0x10001,0x80000000]:
            for rank in [0,1,-1,0x7fff,0x8000,0x10001]:
                add('skill-ranks',c+[invoke('set_skill_rank',slot,ident,rank),invoke('skill_rank',slot,ident),
                    invoke('set_skill_rank',slot,ident,rank+1),invoke('skill_rank',slot,ident),invoke('skill_rank',slot,123)])
        for button in [0,1,-1,0x100000000]:
            for index in [0,1,-1,127,128,255,256]:
                for local in [0,1]:
                    add('quick-slot',c+[poke(r+R.R_QUICK_A,1,17),poke(r+R.R_QUICK_B,1,index),
                        action('service','net_isLocalSlot',local),action('service','res_find',0xabc),
                        invoke('on_quick_slot',slot,button,index)])
        for state in [0,1,2,-1,127,128]:
            for player in [0,0x10000,0xfffffff8]:
                for old in [0,59,60,61,0x80000000,0xffffffff]:
                    add('menu-state',c+[poke(r+R.R_OBJECT,4,player),poke(player+12,4,0xaaffffff),
                        poke(R.G_INPUT_LOCK+R.s32(slot)*12,4,old),invoke('on_menu_state',slot,state)])
        for frames in [-1,0,60,0x80000000,0x100000000]:
            add('button-lock',c+[poke(R.G_INPUT_LOCK+R.s32(slot)*12,4,0x80000000),
                invoke('mask_local_buttons',slot,frames,0xfedcba98)])
    for slot in [0,4,5,-1,0x100000000]:
        r,c=setup(slot)
        for cls in [0,1,-1]:
            table=(R.G_CLASS_SKILLS+cls*0xa88)&R.M32
            for entry in [0,23,47]:
                for level in [0,1,127,128,255]:
                    base=c+[poke(r+0x50,4,cls),poke(table+entry*0x38,8,0xabc000),poke(r+R.R_PASSIVES+entry,1,level)]
                    add('passives',base+[invoke('passive_level',slot,0xabc000),
                        invoke('raise_passive_level',slot,0xabc000,level+1),invoke('passive_level',slot,0xabc000),
                        invoke('raise_passive_level',slot,0xabc000,-2),invoke('passive_level',slot,0xddd)])
    float_patterns=[0,0x80000000,1,0x3f800000,0x40000000,0x41200000,0x7f7fffff,0x7f800000,0xffffffff,0xc1200000]
    for mask in [0,0x40,0x80,0x100,0x20000,0xffffffffffffffff]:
        for ident in [5,0x1d,-1]:
            for index in [-1,0,7,14,-2]:
                r,c=setup()
                base=c+[poke(R.G_OLD_ABILITIES+0x48,8,mask),poke(R.G_OLD_ABILITIES+0x20,2,ident)]
                # Fill every copied field with distinct authored patterns, leaving gaps intact.
                for offset,width in [(0x22,2),(0x24,1),(0x25,1),(0x26,2),(0x28,2),(0x2c,4),
                    *[(0x30+2*k,2) for k in range(8)],(0x40,4),*[(0x50+4*k,4) for k in range(5)]]:
                    base.append(poke(R.G_OLD_ABILITIES+offset,width,0x12340000+offset))
                for hp in float_patterns:
                    add('rebuild-abilities',base+[poke(r+R.R_HP,4,hp),poke(r+R.R_HP_MAX,4,0x40000000),
                        poke(r+R.R_MANA,4,hp),poke(r+R.R_MANA_MAX,4,0x40000000),poke(r+R.R_LOAD_LIMIT,4,hp),
                        invoke('rebuild_abilities',mask,0,index)])
                add('rebuild-owned',base+[poke(r+R.R_ABILITY_MASK,8,mask),invoke('rebuild_abilities',mask,0,-1)])
                add('rebuild-missing',base+[invoke('rebuild_abilities',0x123456,0,index)])
    r,c=setup()
    for free in [-1,0,7,14]:
        for ident in [1,-2,0x10001]:
            for kind in [0,1,3,0x10003]:
                base=c+[poke(r+R.R_ABILITIES+i*R.ABILITY_SIZE+0x20,2,9) for i in range(15)]
                if free!=-1: base += [poke(r+R.R_ABILITIES+free*R.ABILITY_SIZE+0x20,2,-1)]
                base += [poke(R.G_SKILL_TABLE+ident*4,4,0x10000),poke(0x10008,4,kind)]
                add('add-skill',base+[invoke('add_skill_ability',ident,0),invoke('add_skill_ability',ident,0)])
    for target in [0,3,7,14,-1]:
        for kind in [0,1,-1]:
            for value in [1,0x20000,0xffffffffffffffff]:
                for held in [-1,2]:
                    for name in [b'newskill',b'prefix-newskill-suffix',b'other']:
                        r,c=setup()
                        dest=(r+R.R_ABILITIES+target*R.ABILITY_SIZE)&R.M32
                        base=c+string(dest,name)+[poke(dest+0x20,2,held),
                            poke(R.G_OLD_ABILITIES+0x48,8,0x20000),poke(R.G_OLD_ABILITIES+0x20,2,8)]
                        for i in [0,2,7]:
                            a=r+R.R_ABILITIES+i*R.ABILITY_SIZE
                            base += [poke(a+0x20,2,value),poke(a+0x48,8,value)]
                        base += [poke(R.G_SKILL_TABLE+R.s32(value)*4,4,0x10000),poke(R.G_SKILL_TABLE+2*4,4,0x10100),
                            poke(0x10008,1,2),poke(0x10108,1,4),invoke('set_skill_rank',0,2,5),
                            action('service','net_isLocalSlot',1),action('service','res_find',0x1234)]
                        add('assign-spell',base+[invoke('on_assign_spell',0,target,kind,value)])
    for slot in [0,-1,0x80000000]:
        r,c=setup(slot)
        for index in [0,7,-1,0x7fffffff]:
            for player in [0,0x10000,0xfffffff8]:
                for queued in [0,1]:
                    for kind in [0,1,2,255]:
                        for state in [0,2,0xffffffff]:
                            a=(r+R.R_ABILITIES+R.s32(index)*R.ABILITY_SIZE)&R.M32
                            add('quick-cast',c+[poke(r+R.R_OBJECT,4,player),poke(player+0x488,4,queued),
                                poke(a+0x25,1,kind),poke(player+0x58c,4,state),action('service','text_lookup',0x1234),
                                invoke('on_quick_cast',slot,index)])
    for name in ['net_isLocalSlot','res_find','sound_playOnObject','text_lookup','hud_showMessage']:
        r,c=setup()
        base=c+[action('service','net_isLocalSlot',1),action('service','res_find',0xabc),
            action('service','text_lookup',0xdef),poke(r+R.R_ABILITIES+0x25,1,1)]
        op=invoke('on_quick_cast',0,0) if name in ['text_lookup','hud_showMessage'] else invoke('on_quick_slot',0,0,1)
        add('ability-callback-failure',base+[action('throwservice',name),op])
        add('ability-callback-mutation',base+[action('mutate',r+R.R_QUICK_B,1,1),op])
    # Allocation/search boundaries and aliasing of raw32 addresses.
    for free in [-1,0,1,14]:
        for owned in [0,1]:
            r,c=setup()
            base=c+[poke(r+R.R_ABILITIES+i*R.ABILITY_SIZE+0x20,2,2) for i in range(15)]
            if free!=-1: base += [poke(r+R.R_ABILITIES+free*R.ABILITY_SIZE+0x20,2,-1)]
            base += [poke(R.G_OLD_ABILITIES+0x48,8,0x20000),poke(r+R.R_ABILITY_MASK,8,0x20000 if owned else 0)]
            add('ability-allocation',base+[invoke('rebuild_abilities',0x20000,0,-1)])
    r,c=setup()
    # Solve 104*index = address-(R+abilities) modulo 2**32.
    def index_for(address):
        return (((address-r-R.R_ABILITIES)&R.M32)//8*pow(13,-1,1<<29))%(1<<29)
    add('ability-null-record',c+[invoke('on_quick_cast',0,index_for(0))])
    add('ability-row-alias',c+[poke(R.G_OLD_ABILITIES+0x48,8,0x20000),
        invoke('rebuild_abilities',0x20000,0,index_for(R.G_OLD_ABILITIES))])
    for trial in range(128):
        r,c=setup()
        base=c
        for i in range(3):
            row=R.G_OLD_ABILITIES+i*R.ABILITY_SIZE
            base += string(row,('authored-'+str(i)).encode())
            base += [poke(row+0x48,8,0x111 if i<2 else 0x222),poke(row+0x20,2,rng.randrange(32))]
            for offset,width in [(0x22,2),(0x24,1),(0x25,1),(0x26,2),(0x28,2),(0x2c,4),
                *[(0x30+2*k,2) for k in range(8)],(0x40,4),*[(0x50+4*k,4) for k in range(5)]]:
                base.append(poke(row+offset,width,rng.getrandbits(32)))
        base += string(R.G_OLD_ABILITIES+3*R.ABILITY_SIZE,b'END')
        add('ability-table-search',base+[invoke('rebuild_abilities',[0x111,0x222,0x333][trial%3],0,trial%15)])
    # No duplicate found, target outside the first eight, and exact marker duplicate refusal.
    for index in [0,7,14]:
        r,c=setup()
        add('assign-no-match',c+[poke(R.G_SKILL_TABLE+12,4,0x10000),poke(0x10008,4,2),
            invoke('set_skill_rank',0,3,0x7fff),invoke('on_assign_spell',0,index,1,3)])
    for entry in [0,14]:
        r,c=setup()
        a=r+R.R_ABILITIES+entry*R.ABILITY_SIZE
        add('add-skill-duplicate',c+string(a,b'newskill')+[poke(a+0x20,2,7),
            poke(R.G_SKILL_TABLE+28,4,0x10000),invoke('add_skill_ability',7,0)])
    for level in [-1,-2,0x80000000,0xffffffff,0x100000001]:
        r,c=setup()
        add('passive-signed-raise',c+[poke(R.G_CLASS_SKILLS,8,0x123),poke(r+R.R_PASSIVES,1,0xff),
            invoke('raise_passive_level',0,0x123,level),invoke('passive_level',0,0x123)])
    data = ('\n'.join(rows)+'\n').encode()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    compressed = io.BytesIO()
    with gzip.GzipFile(filename="",mode="wb",fileobj=compressed,mtime=0) as stream:
        stream.write(data)
    args.output.write_bytes(compressed.getvalue())
    manifest = {'schema':SCHEMA,'research_commit':COMMIT,'research_files':PINS,
        'generator_sha256':digest(Path(__file__).read_bytes()),'vectors_sha256':digest(data),
        'rows':len(rows),'groups':counts,'scope':'transport, encoders, player abilities and HUD controls'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(f'{len(rows)} exact message cases written')

if __name__ == '__main__':
    main()
