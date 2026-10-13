#!/usr/bin/env python3
"""Synthetic arena base services and Skelight states from immutable reference sources."""
import argparse
import hashlib
import importlib
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
    'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'docs/spec/arena-monsters.md': 'd9a1a7b81b79c293b64ce5911112456ddd177df15fb8d0ffa0c09230887cd87b',
    'tools/difftest_arena_monsters.py': 'd342f5ca8b2fa7edde61c05350481900fa758c80d8a4df300da63ae8804dc911',
}
C, D, T = 0x10000, 0x20000, 0x30000
MASK = (1 << 64)-1

def packed(rows):
    return ';'.join(','.join(str(x & MASK) for x in row) for row in rows) or '-'

def value(v):
    if isinstance(v, int): return 'i'+str(v)
    return 'v'+','.join(map(str, v))

def calltext(name, args):
    return 'c'+name+'('+ ';'.join(map(value,args))+')'

def writetext(w):
    return 'w('+','.join(map(str,w))+')'

def snapshot(m, events, results):
    return '|'.join([','.join(f'{a}:{b}' for a,b in sorted(m.mem.bytes.items())),
                     ';'.join(map(writetext,m.mem.log)), ';'.join(map(writetext,m.writes)),
                     ';'.join(calltext(n,a) for n,a in m.calls), ';'.join(events),
                     str(m.draws), ';'.join(results)])

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--research-dir',type=Path,required=True)
    p.add_argument('--output',type=Path,default=Path('native/tests/vectors/arena_monsters.tsv'))
    a=p.parse_args()
    sys.dont_write_bytecode=True
    with tempfile.TemporaryDirectory(prefix='arena-pinned-') as tmp:
        for path,sha in PINS.items():
            data=subprocess.check_output(['git','-C',str(a.research_dir),'show',COMMIT+':'+path])
            if hashlib.sha256(data).hexdigest()!=sha: raise ValueError('pin mismatch: '+path)
            if path.startswith('tools/ref_'): (Path(tmp)/Path(path).name).write_bytes(data)
        sys.path.insert(0,tmp)
        R=importlib.import_module('ref_arena_monsters')
        rng=random.Random(0x4152454e)
        cases=[]
        states=list(range(40))+[0xffffffff,0x80000000,0x7fffffff,0x10000,0x10006]
        # Exhaust priorities, signed timer reads, PvP gate and inert rejection.
        for target in (0,0x40000):
            for root,stun,cooldown in ((0,0,0),(1,0,0),(0,1,0),(0,0,1),(0xffff,0x8000,0xffff),(1,1,1)):
                for state in states:
                    extra=[[C+0x234,4,target],[C+0x314,2,root],[C+0x31c,2,stun],[C+0x318,2,cooldown]]
                    # State 1 can still be filtered/refused: omit only a resolved wander.
                    for op in (0,1): cases.append((extra,[[op,state]],[7,0,0,0]))
        for state in states:
            cases.append(([[0x4f7120,4,1],[0x503890,4,1]],[[0,state]],[0,0,0,0]))
            cases.append(([[C+0x231,1,255]],[[0,state]],[0,0,0,0]))
        for count in (-1,0,1,2,3,4,7,12,0x80000000,0xffffffff):
            for external in (0,1):
                for failure in range(0,12):
                    cases.append(([],[[0,20]],[count,failure,external,external]))
        # Slot16 signed this adjustment, unknown virtual body, and PvP bypasses.
        for adjustment in (4,0xfffc,0x8000,0x7fff):
            signed = adjustment if adjustment < 0x8000 else adjustment-0x10000
            cases.append(([[T+128,2,adjustment],[C+signed+0x24,4,D]],[[0,20]],[4,0,0,0]))
        cases.append(([[T+132,4,0x12345678]],[[0,20]],[4,0,0,0]))
        for a_mode,b_mode,gate,finished in ((1,1,1,0),(0,0,1,0),(0,1,0,0),(0,1,1,1)):
            cases.append(([[0x4f711c,4,a_mode],[0x4f7120,4,b_mode],[0x503890,4,gate],
                           [0x503894,4,finished]],[[0,6]],[0,0,0,0]))
        for _ in range(128):
            cases.append(([[C+0x44,4,rng.getrandbits(32)],[C+0x300,4,rng.getrandbits(32)],
                           [C+0x232,2,rng.getrandbits(16)]],[[0,6],[0,18],[0,23],[0,0],[2,0]],[0,0,0,0]))
        # Append the base family: the original 1,544 rows remain byte-identical.
        target=[[C+0x234,4,0x40000],[0x4000c,4,0x41000],
                [0x41010,4,0x41200000],[0x41014,4,0],[0x41018,4,0x80000000]]
        for scenario in range(32):
            for failure in range(36):
                cases.append(([[0x4f85c4,1,255]],
                    [[3,0 if scenario&1 and not scenario&16 else 0x60000,0x3f800000,0xc0200000,0x7fc12345,
                      0xffff8123,D,0x70000,0x41200000]], [0, failure,0,scenario&2,scenario]))
        for scenario in (0,1,4,8,15,31):
            ctor=[3,0x60000,0,0x80000000,0x3f800000,0x1234,D,0,0x41200000]
            cases.append(([],[ctor,ctor,[8,0],[9,0x12345678],[15,0]], [0,0,0,scenario&2,scenario]))
        for _ in range(128):
            entry=rng.randrange(32)
            cases.append(([[D+8*entry+4,4,0x60000]],
                [[4,entry]+[rng.getrandbits(32) for _ in range(8)]],[0,0,0,0,0]))
        for node in ([],target):
            for timer in (0,1,0x8000,0xffff):
                cases.append((node+[[C+0x314,2,timer],[C+0x318,2,timer],[C+0x31c,2,timer]],
                              [[5,0],[6,0],[7,0],[8,0],[9,0xffffffff],[10,0],
                               [11,0xffff8123,0x18000,0xffffffff,0x80000000,0x7fc12345,0x42c80000],
                               [13,0x42100000],[14,0]], [0xffffffff,0,0,0,0]))
        for scenario in (0,1,2,4,8,15):
            for quiet in (0,1,0xffffffff):
                for failure in range(10):
                    cases.append(([[C+0xc,4,2],[C+0xa8,8,MASK],[0x4f7120,4,1],
                                  [C+0x2e4,4,0x40000],[0x4000c,4,0x41000]],
                                  [[16,quiet]], [1,failure,0,scenario&2,scenario]))
        for arc in (0,0x7fff,0x8000,0xffff):
            for result in (0,1,0xffffffff):
                for scenario in (0,2,16,32):
                    for failure in (0,1,2):
                        cases.append((target+[[C+0x3ac,2,65535],[C+0x3ae,2,65534],
                                            [C+0x3b0,2,arc]],[[15,scenario&2]],
                                      [result,failure,0,0,scenario]))
        for adjustment in (0,4,0xfffc,0x8000,0x7fff):
            cases.append((target+[[T+26*8,2,adjustment]],[[10,0]],[0,0,0,0,0]))
        cases.append((target+[[T+26*8+4,4,0x10bb08],[T+26*8,2,0xfffc]],[[10,0]],[0,0,0,0,64]))
        for facing in (0,0x3ffe,0x3fff,0x4000,0x8000,0xffff):
            for speed in (0,0x42c7ffff,0x42c80000,0x42c80001,0x43000000,0x7f800000):
                cases.append((target+[[0x41038,2,facing],[0x41308,4,speed]],[[12,0]],[0,0,0,0,0]))
        for _ in range(256):
            cases.append((target+[[a,4,rng.getrandbits(32)] for a in
                                  (C+0x10,C+0x14,0x41010,0x41014,0x41308,0x4130c)]+
                                  [[0x41038,2,rng.getrandbits(16)]],[[12,0]],[0,0,0,0,0]))
        rows=[];counts={}
        for extra,commands,options in cases:
            pokes=[[C+0x24,4,D],[C+0x48,4,T],[T+132,4,0x3c2738],
                   [D+12,4,0x77770000],[D+0x1c,4,0x11110000],[D+0x24,4,0x22220000],
                   [D+0x34,4,0x33330000],[D+0x44,4,0x44440000],
                   [C+0x10,8,0x400000003f800000],[C+0x18,4,0x7fc12345],
                   [C+0x38,2,0xc123],[C+0x44,4,0x12345678],[C+0x300,4,0xfffffff0],
                   [C+0xf4,4,0xcafebabe],[C+0xa8,8,0x8000000000000001],
                   [0x8e4560,8,0xfedcba9876543210],[0x404464,4,0x89abcdef]]+extra
            mem=R.RC.Memory()
            for row in pokes: mem.poke(*row)
            external=['Creature.v16'] if options[2] else []
            if len(options)>4 and options[4]&64: external.append('Creature.v26')
            m=R.Model(mem,external=external)
            events=[];results=[];calls=0
            original=m._w
            def write(addr,size,v):
                events.append(writetext((addr,size,v & ((1<<(8*size))-1))))
                original(addr,size,v)
            m._w=write
            original_call=m.call
            def call(name,*args):
                args=tuple(v&0xffffffff if isinstance(v,int) else v for v in args)
                events.append(calltext(name,args))
                return original_call(name,*args)
            m.call=call
            def callback(name):
                def fn(*args):
                    nonlocal calls
                    calls+=1
                    if options[1] and calls==options[1]: raise RuntimeError('synthetic callback failure')
                    if options[3]:
                        mem.poke(C+0x24,4,D)
                        mem.poke(D+12,4,0x77770000+calls)
                        mem.poke(C+0x38,2,0xc123+calls)
                        mem.store(0x50000,4,calls)
                    scenario=options[4] if len(options)>4 else 0
                    if name=='GameObject_ctor':
                        mem.store(C+0x24,4,args[2])
                        for k,v in enumerate(args[1]): mem.store(C+0x10+4*k,4,v)
                    if name=='obj_getPropInt': return (0xffff8001+calls) if scenario&1 else 0
                    if name=='obj_getPropStr':
                        if args[1] in (0x52fbf8,0x52fc08,0x52fa00): return 0x60000+calls if scenario&4 else 0
                        return 0x61000 if args[1]==(0x52fa80,0x52fa88,0x52fa90)[scenario%3] and scenario&8 else 0
                    if name=='balance_lookup': return (scenario&8,0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000)
                    if name=='Creature_setStats': mem.store(C+0x38,2,0x4321)
                    if name=='creature_canSeeObj':
                        if scenario&16: mem.store(C+0x234,4,0)
                        if scenario&32:
                            mem.store(C+0x3ae,2,7);mem.store(C+0x38,2,0x8765)
                            return 0 if calls==1 else options[0]
                        return options[0]
                    if name in ('creature_countAlliesNear','creature_isBlockedBy'): return options[0]
                    if name=='Creature.v16': return 0x99990000+calls
                    if name=='model_getSamplePointCount': return options[0]
                    if name=='model_getSamplePoint': return ((0x3f800000,0x80000000,0x7fc12345),(0,1,2))
                    return 0
                return fn
            for name in ('handle_clear','anim_start','anim_startBlendInOut','anim_syncToSimFrame',
                         'Creature.v16','model_getSamplePointCount','model_getSamplePoint','fx_spawn','obj_delete',
                         'GameObject_ctor','obj_getPropInt','obj_getPropStr','obj_moveOffGrid','balance_lookup',
                         'Creature_setStats','Creature.v26','creature_canSeeObj','creature_countAlliesNear',
                         'creature_isBlockedBy','script_runByName','loot_spawnNamedItem','anim_jumpTopToEnd',
                         'arena_addKill','creature_stopBossMusic'):
                m.cb[name]=callback(name)
            unsupported=False
            for command in commands:
                op,state=command[:2]
                # Inspect filtered state on independent memory to exclude only unported wander.
                if op==0:
                    probe=R.Model(R.RC.Memory());probe.mem.bytes=mem.bytes.copy()
                    refused,filtered=probe.filter_state(C,state)
                    if not refused and filtered==1: unsupported=True;break
                try:
                    if op==0: m.skelight_set_state(C,state);results.append('ok')
                    elif op==1:
                        refused,state=m.filter_state(C,state);results.append(f'f{refused},{state}')
                    elif op==2: m.skelight_shatter(C,state);results.append('ok')
                    elif op==3:
                        m.creature_ctor(C,state,tuple(command[2:5]),*command[5:]);results.append('ok')
                    elif op==4:
                        m.set_def_entry_params(C,state,command[2:6],command[6:10]);results.append('ok')
                    elif op in (9,16):
                        getattr(m,{9:'slot34_set_home_range',16:'slot28_on_death'}[op])(C,state);results.append('ok')
                    else:
                        methods={5:'is_not_rooted',6:'is_attack_ready',7:'is_not_stunned',8:'slot16_model',
                                 10:'turn_to_target',11:'can_see_target_in',12:'is_target_charging',
                                 13:'count_allies_near_target',14:'is_blocked_by_target',15:'slot25_can_see_target'}
                        args=command[1:] if op in (11,13,15) else []
                        results.append('r'+str(getattr(m,methods[op])(C,*args)))
                except RuntimeError: results.append('callback_error');break
            if unsupported: continue
            rows.append('\t'.join([str(len(rows)),packed(pokes),packed(commands),packed([options]),snapshot(m,events,results)]))
            counts[str(commands[0][0])]=counts.get(str(commands[0][0]),0)+1
        data=('# arena-monsters-v1\n'+'\n'.join(rows)+'\n').encode()
        a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data)
        a.output.with_suffix('.json').write_text(json.dumps({'schema':'arena-monsters-v1','research_commit':COMMIT,
            'references':PINS,'cases':len(rows),'operations':counts,'vector_sha256':hashlib.sha256(data).hexdigest(),
            'scope':'Skelight set-state excluding resolved wander; filter_state; shatter; complete arena creature base family',
            'observations':'all sparse bytes, memory store log, model writes/calls/events, shared draw count, results'},indent=2)+'\n')
        print(f'{len(rows)} exact reference cases')
if __name__=='__main__': main()
