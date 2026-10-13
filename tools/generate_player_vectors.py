#!/usr/bin/env python3
"""Synthetic inputs and exact full snapshots from immutable ref_player.py.
No retail inputs, decompiler output, native output, or live research imports.
"""
import argparse
import copy
import gzip
import hashlib
import json
import pathlib
import random
import subprocess
import sys
import tempfile
import types

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
 'tools/ref_player.py': 'ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d',
 'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
 'docs/spec/player-tick.md': 'ba83f8672626cc5625ae108dd2838b187de06d8d37c54cdb57afae3cc71e3f37',
 'tools/difftest_player.py': 'dabb453fa62eb3e3ca66c28d5d80decc9e89fdcef86bf8266027bc5af0bcee47',
}
SERVICES = ['Creature_run','Creature_applyKnockback','light_relink','light_unlink',
 'net_getCamFlag','game_isFrozen','player_voice','memcard_isBusy','pad_rumble',
 'Player_animStep','Player_footstep','Creature_moveToDest','fx_spawn','dialog_isOpen',
 'task_isRunning','net_isSlotActive','res_find','anim_start','anim_startBlendInOut']
P, R, PAD = 0x10000, 0x4d8af0, 0x41bfd8

def packed(value):
    return json.dumps(value, separators=(',', ':'))

def load(research):
    sources = {}
    for path, expected in PINS.items():
        data = subprocess.run(['git','-C',str(research),'show',COMMIT+':'+path],
                              check=True,capture_output=True,timeout=30).stdout
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('pinned hash mismatch: '+path)
        sources[path] = data
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    try:
        modules = []
        # Paths/imports are confined to temporary immutable reference copies.
        with tempfile.TemporaryDirectory() as directory:
            for name in ('ref_core','ref_player'):
                path = pathlib.Path(directory)/(name+'.py')
                path.write_bytes(sources['tools/'+name+'.py'])
                module = types.ModuleType(name); module.__file__ = str(path)
                exec(compile(path.read_bytes(),str(path),'exec'),module.__dict__)
                modules.append(module)
                if name == 'ref_core': sys.modules[name] = module
        return modules
    finally:
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core',None)
        else: sys.modules['ref_core'] = previous

class Boundary(Exception): pass
class Missing(Exception): pass
class Injected(Exception): pass

def run(modules, config, inputs):
    core, ref = modules
    core.set_fpu(('ps2','ps2g1','chop','nearest')[config//4],bool(config&2),bool(config&1))
    memory = ref.Memory(); events=[]
    class Traced(ref.Tick):
        def _w(self, a, size, value):
            # Observe the reference's exact entry prefix before its first
            # ordinary-only store. The suffix is never substituted or modelled.
            if a == P+0x4c4: raise Boundary()
            events.append([0,[a,size,value & ((1 << (8*size))-1)]])
            return super()._w(a,size,value)
        def call(self, name, *args):
            args=tuple(a & 0xffffffff if isinstance(a,int) else a for a in args)
            events.append([1,[SERVICES.index(name),[argument(a) for a in args]]])
            return super().call(name,*args)
    def argument(a):
        return [1,*a] if isinstance(a,tuple) else [0,a]
    tick=Traced(memory,P)
    for name in SERVICES:
        def missing(*args): raise Missing()
        tick.cb[name]=missing
    for service, result, effects, fail in inputs[1]:
        def callback(*args, result=result, effects=effects, fail=fail):
            for log,a,size,value in effects:
                (memory.store if log else memory.poke)(a,size,value)
            if fail: raise Injected()
            return result
        tick.cb[SERVICES[service]]=callback
    for poke in inputs[0]:memory.poke(*poke)
    def snapshot():
        return copy.deepcopy([sorted(memory.bytes.items()),memory.log,tick.writes,
            [[SERVICES.index(name),[argument(a) for a in args]] for name,args in tick.calls],
            events,tick.draws,getattr(tick,'R0',0)])
    initial=snapshot();steps=[]
    for command in inputs[2]:
        status=result=0
        try:
            op,*args=command
            if op==0:
                outcome=tick.run();assert outcome.returned=='scripted';result=1
            elif op==1:result=tick.choose_pad()
            elif op==2:tick.idle_fidget(*args)
            elif op==3:result=tick.can_block(*args)
            elif op==4:tick.try_block_move(*args)
            elif op==5:result=tick.has_anim(*args)
            elif op==6:result=tick.skill_rank(*args)
            elif op==7:result=tick.passive_level(*args)
            elif op==8:memory.poke(*args)
            elif op==9:memory.store(*args)
            else:raise ValueError('opcode')
        except Boundary:status=1
        except Missing:status=2
        except Injected:status=4
        steps.append([status,result,snapshot()])
    return [initial,steps]

def setup(slot=0):
    r=R+slot*0x4c30
    return [[P+0x3c8,4,slot],[P+0x231,1,1],[P+0x58c,4,1],
        [P+0x10,4,0x3f800000],[P+0x14,4,0xc0000000],[P+0x18,4,0x40400000],
        [P+0x20,4,0x42000000],[P+0x44,4,1],[P+0x544,4,0x80000],
        [P+0x488,4,0x12345678],[r+0xac,4,0x41a00000],[r+0xb0,4,0x42c80000],
        [P+0x4b2,2,200],[P+0x4ae,2,9],[0x4f83f4,4,0x3f800000],
        [0x404464,4,0xdeadbeef],[0x44fdcc,4,0],[0x404468,4,0x87654321]]

def bindings():
    return [[i, (1 if i in (9,15) else 0), [],0] for i in range(len(SERVICES))]

def recipes():
    out=[]
    def add(label,pokes,ops,cb=None):out.append((label,[pokes,bindings() if cb is None else cb,ops]))
    for sl in range(5):
        for state in (0,1,2,11,14,15,0xffffffff):
            add('scripted_states',setup(sl)+[[P+0x58c,4,state]],[[0],[0]])
    for immunity in range(256):
        add('immunity',setup()+[[R+0x4c27,1,immunity]],[[0]])
    for protection in (0,1,19,20,21,0x7fff,0x8000,0xffff):
        for flags in (0,2,8,0x20002):
            add('spawn_copy',setup()+[[P+0x4b4,2,protection],[P+0xc,4,flags]],[[0],[0]])
    for counter,limit in ((0,0),(199,200),(200,200),(0x7fff,0),(0xffff,0xffff),(0xfffe,0)):
        for gate in (0,1,2,3):
            cb=bindings();p=setup()+[[P+0x4b0,2,counter],[P+0x4b2,2,limit]]
            if gate==1:cb[4][1]=1
            if gate==2:p.append([0x44fcd0,1,255])
            if gate==3:p.append([PAD+8,4,1])
            add('idle_voice',p,[[0],[0]],cb)
    for value in (0,1,0x80000000,0x007fffff,0x00800000,0x3f800001,0xbf800000,0x7f800000,0xff800000,0x7fc01234,0x7f7fffff):
        for alive in (1,2,3):
            add('light_bits',setup()+[[0x4f83f4,4,value],[0x4f8420,4,alive]],[[0]])
    for network in (0,1,255):
        for phase in (0,20,21,63):
            for hp in (0,0x80000000,0xbf800000,0x41a00000,0x42200000,0x7fc01234):
                add('rumble',setup()+[[0x402791,1,network],[0x44fdcc,4,phase],[R+0xac,4,hp]],[[0]])
    for gate in range(5):
        cb=bindings();p=setup()+[[0x402791,1,1]]
        if gate&1:p.extend([[0x4f713c,4,1],[0x4f7114,4,1]])
        if gate&2:cb[7][1]=1
        add('rumble_gates',p,[[0]],cb)
    # Breath seeds include rejection iterations and parity skips. Complete
    # raw positions, own/shared seeds and fx vectors are compared, all contexts.
    for seed in range(40):
        add('breath',setup()+[[0x44fe08,4,0x100],[P+0x44,4,seed]],[[0],[0]])
    add('ordinary_boundary',setup()+[[P+0x231,1,0]],[[0]])
    for service in range(19):
        cb=bindings();cb[service][3]=1
        add('callback_failure',setup()+[[0x44fe08,4,0x100],[P+0x44,4,1]],[[0],[1]],cb)
    for service in (0,1,2,3,4,5,6,9,10,11):
        cb=bindings();cb[service][2]=[[0,P+0x3c8,4,1],[1,R+0x4c30+0xac,4,0x41a00000]]
        add('callback_mutation',setup()+[[P+0x4b2,2,0]],[[0],[0]],cb)
    add('missing_callback',setup(),[[0]],[])
    # Every ordered zero-pad gate, plus conflicting gates to test short circuit.
    pad_gates=[(0x41b4fc,4,1),(P+0x4b4,2,1),(0x41b51d,1,255),
      (0x4f7114,4,1),(P+0xc,4,0x400000),(0x503890,4,1),(0x4f66c8,1,128),(R+0xac,4,0)]
    for gate in pad_gates:
        for network in (0,1):add('pad_memory_gates',setup()+[list(gate),[0x402791,1,network]],[[1]])
    for service in (13,4,14,15):
        cb=bindings();cb[service][1]=0 if service==15 else 1
        add('pad_callback_gates',setup(),[[1]],cb)
    add('pad_open',setup(),[[1]])
    for tracks in (0,3,4,5,6,127,128,255):
        for moving in (0,1):
            for fidget in (0,1):
                add('fidget_cancel',setup()+[[P+0x118,1,tracks],[P+0x463,1,fidget],[PAD+0x14,4,0x3f800000 if moving else 0]],[[2,P,PAD]])
    # Search synthetic seeds that enter the rare fidget branch using its original RNG.
    seeds=[s for s in range(100000) if (((s*0x19660d+0x3c6ef35f)&0xffffffff)>>16)<0xe2][:8]
    for cls in (0,1,2,3,4,5,6,7,255):
        for seed in seeds:
            cb=bindings();cb[16][1]=0x90000
            add('fidget_start',setup()+[[P+0x118,1,4],[0x404464,4,seed],[R+0x4a86,1,cls]],[[2,P,PAD],[2,P,PAD]],cb)
    for two,one,shield,state,cls in [(a,b,c,d,e) for a in (0,0x20000) for b in (0,0x21000) for c in (0,0x22000) for d in (0,11,0xffffffff) for e in (0,4,255)]:
        add('can_block',setup()+[[R+0x43f8,4,two],[R+0x43f0,4,one],[R+0x43fc,4,shield],[0x20000,4,0x30000],[0x3001e,1,cls]],[[3,0,state]])
    for cls in (0,1,7,255):
        for gate in range(5):
            p=setup()+[[P+0x47f,1,1],[0x41ba30,4,4],[0x41b9d0,4,8],[PAD+0xc,4,4],[PAD+8,4,8],[R+0x4a86,1,cls],[P+0x24,4,0x30000],[0x30000+(cls if cls<128 else cls-256)*0x68+0xa4,4,0x90000]]
            if gate==1:p.append([0x44fe08,4,0x10000000])
            if gate==2:p.append([P+0x47f,1,0])
            if gate==3:p.append([PAD+0xc,4,0])
            if gate==4:p.append([PAD+8,4,0])
            add('block_move',p,[[4,P,PAD]])
    for count in (0,1,4,127,128,255):
        for index in (0,3,126):
            cb=bindings();cb[16][1]=0x90000
            add('has_anim',setup()+[[P+0x118,1,count],[P+0x11c+0x20*index,4,0x90000]],[[5,P,0x88888]],cb)
    for id in (0,1,0xffff,0x8000):
        for rank in (0,1,0x7fff,0x8000,0xffff):
            add('skill_rank',setup()+[[R+0x4b08,2,id],[R+0x4b0a,2,rank],[R+0x4b0c,2,0xffff]],[[6,0,id],[6,0,0x1234]])
    for sl in (0,1,4,5,0xffffffff):
        for level in (0,127,128,255):
            p=setup();r=R+(sl if sl<=4 else 0)*0x4c30
            p.extend([[r+0x50,4,1],[0x4ef538+0xa88+47*0x38,8,0xfedcba9876543210],[r+0xef+47,1,level]])
            add('passive',p,[[7,sl,0xfedcba9876543210],[7,sl,0x123456789]])
    rng=random.Random(0x1a5030)
    for _ in range(100):
        p=setup(rng.randrange(5));p += [[P+0x4b0,2,rng.randrange(65536)],[P+0x4b2,2,rng.randrange(65536)],[P+0xc,4,rng.choice((0,2,8,0x20000))],[0x44fdcc,4,rng.getrandbits(32)],[P+0x58c,4,rng.randrange(16)],[0x44fe08,4,0x100],[P+0x44,4,rng.getrandbits(32)]]
        add('seeded_scripted',p,[[0],[0],[1]])
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/player.tsv.gz'))
    args=parser.parse_args();modules=load(args.research_dir)
    rows=[];counts={}
    for config in range(16):
        for label,inputs in recipes():
            result=run(modules,config,inputs)
            assert result==run(modules,config,inputs),'reference nondeterminism'
            rows.append(packed([label,config,inputs,result])+'\n')
            counts[label]=counts.get(label,0)+1
    data=''.join(rows).encode('ascii')
    compressed=gzip.compress(data,mtime=0)
    manifest={'schema':1,'commit':COMMIT,'pins':PINS,'services':SERVICES,
        'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'vector_sha256':hashlib.sha256(data).hexdigest(),
        'compressed_sha256':hashlib.sha256(compressed).hexdigest(),
        'count':len(rows),'operations':counts,'expanded_bytes':len(data),
        'protocol':'row=[label,config,[pokes,callbacks,commands],[initial,steps]]; step=[status,result,snapshot]; snapshot=[bytes,stores,writes,calls,events,shared_draws,R0]',
        'statuses':{'0':'success','1':'ordinary boundary refusal','2':'missing handler','4':'injected callback exception'},
        'scope':'entry through scripted return; choose_pad; idle_fidget; can_block; try_block_move; has_anim; skill_rank; passive_level'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(compressed)
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(packed({'count':len(rows),'expanded_bytes':len(data),'sha256':manifest['vector_sha256']}))
if __name__=='__main__':main()
