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
 'task_isRunning','net_isSlotActive','res_find','anim_start','anim_startBlendInOut',
 'SpellBlessed_typeinfo','creature_findSpellByType','SpellBlessed_getStat0',
 'SkillRegeneration_getStat0','net_isLocalSlot','sound_playOnObject','invmsg_sendDrop',
 'inventory_removeItem','player_countPotions','player_itemWeight','hud_countActiveNotices',
 'model_getAttachPoint','Player.v17','anim_findTrackByFlag','Spell.v1',
 'inventory_equipItem','Player_recalcDamage','Skill.v1','world_collectObjectsInArc',
 'Player.v20','Player.v39','Skill.v2','creature_addSpell','handle_link','Skill.v5','handle_clear',
 'Player_move','sound_stop','decal_add','water_disturb','pool_new','LightEffect_ctor',
 'Vehicle.v17','obj_updateGridCell','world_damageInRadius','GroundPoundEffect_ctor',
 'SpellBash_typeinfo','SpellShieldBash_typeinfo','Spell.v4','particleDef_ctor','particle_spawn']
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
            if a == P+0x4c4 and getattr(self,'ordinary_boundary',False): raise Boundary()
            events.append([0,[a,size,value & ((1 << (8*size))-1)]])
            return super()._w(a,size,value)
        def call(self, name, *args):
            args=tuple(a & 0xffffffff if isinstance(a,int) else a for a in args)
            events.append([1,[SERVICES.index(name),[argument(a) for a in args]]])
            return super().call(name,*args)
    def argument(a):
        return [2,0] if a == "stack" else ([1,*a] if isinstance(a,tuple) else [0,a])
    tick=Traced(memory,P)
    for name in SERVICES:
        def missing(*args): raise Missing()
        tick.cb[name]=missing
    for service, result, effects, fail in inputs[1]:
        index=[0]
        def callback(*args, result=result, effects=effects, fail=fail, index=index):
            for log,a,size,value in effects:
                (memory.store if log else memory.poke)(a,size,value)
            if fail: raise Injected()
            value=result
            if isinstance(value,list) and value[0]==3:
                value=value[1][min(index[0],len(value[1])-1)]
                index[0]+=1
            if isinstance(value,list):
                return tuple(value[1:]) if value[0]==1 else value[1]
            return value
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
                tick.ordinary_boundary=True
                try:outcome=tick.run();assert outcome.returned=='scripted';result=1
                finally:tick.ordinary_boundary=False
            elif op==1:result=tick.choose_pad()
            elif op==2:tick.idle_fidget(*args)
            elif op==3:result=tick.can_block(*args)
            elif op==4:tick.try_block_move(*args)
            elif op==5:result=tick.has_anim(*args)
            elif op==6:result=tick.skill_rank(*args)
            elif op==7:result=tick.passive_level(*args)
            elif op==8:memory.poke(*args)
            elif op==9:memory.store(*args)
            elif op==10:tick.regen_health(*args)
            elif op==11:tick.regen_mana(*args)
            elif op==12:tick.tick_enchant_timers(*args)
            elif op==13:tick.use_potion(*args)
            elif op==14:tick.use_potion_real(*args)
            elif op==15:tick.update_cast(*args)
            elif op==16:result=tick.weapon_swap_input(*args)
            elif op==17:tick.cast_skill(*args)
            elif op==18:result=[list(v) for v in tick.hands_midpoint(*args)]
            elif op==19:result=tick.skill_of(*args)
            elif op==20:
                state,pad,r0=args;tick.pad=pad;tick.R0=r0
                getattr(tick,('state_hit','state_dead','state_jump','state_rush','state_block_move',
                    'state_warcry','state_vehicle','state_pound','state_skill','state_block')[state])()
            elif op==21:
                helper,a,b=args
                if helper==0:result=tick.anim_step(a)
                elif helper==1:result=tick.move(a,b)
                elif helper==2:result=tick.frames()
                elif helper==3:result=tick.top_track(ref.s32(a))
                elif helper==4:tick.stop_loop_sound()
                elif helper==5:tick.clear_extra_tracks()
                elif helper==6:result=tick.slow_scale(a)
                elif helper==7:result=tick.haste_of(a)
                elif helper==8:result=tick.anim_name(ref.s32(a))
                elif helper==9:result=tick.selected_ability() or 0
                elif helper==10:tick.sound(a,b)
                elif helper==11:result=tick.unit_sphere_shared()
                else:raise ValueError('helper')
            else:raise ValueError('opcode')
        except ref.Leave:status=5
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
    return [[i, (1 if i in (9,15) else 0), [],0] for i in range(19)]

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

def recovery_recipes():
    out=[]
    def cb(): return bindings()+[[i,0,[],0] for i in range(19,30)]
    def base(sl=0):
        r=R+sl*0x4c30
        return setup(sl)+[[r+0xc0,4,0x41200000],[r+0xc4,4,0x42c80000],
            [r+0x50,4,1],[r+0x4b08,2,0xffff],[0x537c08,4,0xffffffff]]
    def add(label,pokes,ops,callbacks=None):
        out.append(('recovery_'+label,[pokes,cb() if callbacks is None else callbacks,ops]))
    for sl in range(5):
        r=R+sl*0x4c30
        for cls in (0,1,0xffffffff):
            for bonus in (0,127,128,255):
                p=base(sl)+[[r+0x50,4,cls],[P+0x475,1,bonus],[P+0x477,1,bonus],
                   [0x4ef538+(cls if cls<128 else -1)*0xa88,8,0x10],
                   [0x4ef538+(cls if cls<128 else -1)*0xa88+0x38,8,0x200],
                   [r+0xef,1,7],[r+0xf0,1,3]]
                add('regen_passive_bonus',p,[[10,P],[11,P],[10,P],[11,P]])
    bits=(0,1,0x80000000,0x007fffff,0x00800000,0x3f800001,0x42c80000,
          0xbf800000,0x7f800000,0xff800000,0x7fc01234,0x7f7fffff)
    for value in bits:
        for target in (0xac,0xb0,0xb4,0xb8,0xc0,0xc4,0xc8,0xbc):
            add('regen_float_bits',base()+[[R+0xb4,4,0x3f800000],[R+0xc8,4,0x3f800000],
                [R+0xb8,4,0x3f000000],[R+0xbc,4,0x3f000000],[R+target,4,value]],[[10,P],[11,P]])
    for flag in (0,8,0x20000,0x20008):
        for hp in (0,0xbf800000,0x42c80000,0x7fc01234):
            add('real_alive',base()+[[P+0xc,4,flag],[R+0xac,4,hp]],[[10,P],[11,P],[12,P],[13,0],[13,1]])
    for lane in (0xb,0xc):
        for value in (0,1,100,0x7fff,0x8000,0xffff):
            p=base()
            for i in range(17):
                item=0x30000+i*0x48
                p += [[R+0x43f0+4*i,4,item],[item,4,0x90000],
                      [item+0x24,2,lane],[item+0x26,2,value],
                      [item+0x2c,2,lane],[item+0x2e,2,0x7fff]]
            add('regen_equipment',p,[[10,P],[11,P]])
    for rank in (0,1,0x7fff,0x8000,0xffff):
        for stat in (0,100,0xffffffff):
            c=cb();c[19][1]=0x22222;c[20][1]=0x33333;c[21][1]=stat;c[22][1]=stat
            add('regen_spells',base()+[[R+0x4b08,2,0x13],[R+0x4b0a,2,rank],
                  [R+0x4b0c,2,0xffff],[0x41b170+4*0x13,4,0x44444]],[[11,P]],c)
    for service in (19,20,21,22):
        for failure in (False,True):
            c=cb();c[20][1]=0x33333;c[service][3]=int(failure)
            c[service][2]=[[0,P+0x3c8,4,1],[1,R+0xc0,4,0x42c60000],
                          [0,R+0xc4,4,0x42c70000],[0,R+0x4c30+0x4b08,2,0xffff]]
            add('regen_callback_prefix',base()+[[R+0x4b08,2,0x13],[R+0x4b0a,2,1],
                  [R+0x4b0c,2,0xffff]],[[11,P],[11,P]],c)
    add('regen_missing',base(),[[11,P]],bindings())
    # Timers: full equipment set, repeated ticks, duplicate/empty slots,
    # signed frames, signed IDs outside s16 and repeated ID list entries.
    for frames in (0,1,2,0x7fff,0x8000,0xffff):
        for layout in range(4):
            p=base()+[[0x537c08,4,11],[0x537c0c,4,12],[0x537c10,4,11],
                      [0x537c14,4,0x1000b],[0x537c18,4,0xffffffff]]
            for i in range(17):
                item=0x30000+i*0x48
                p += [[R+0x43f0+4*i,4,item],[item,4,0 if i==16 else 0x90000]]
                for k in range(4):
                    ident=((11,11,12,-1),(11,-1,11,12),(-1,11,12,11),(12,13,11,14))[layout][k]
                    p += [[item+0x24+8*k,2,ident&0xffff],[item+0x26+8*k,2,0x8001+k],
                          [item+0x28+8*k,2,0xffff-k],[item+0x2a+8*k,2,frames]]
            add('enchant_expiry',p,[[12,P],[12,P]])
    def potion(kind=0, sizes=(0,), stacks=1):
        p=base()+[[R+0xac,4,0x3f800000],[R+0xb0,4,0x44160000],
                  [R+0xc0,4,0x3f800000],[R+0xc4,4,0x44160000],
                  [R+0x3740,4,len(sizes)],[0x4f85ca,2,0xffff],[0x4f85c8,2,0xffff]]
        # Authored strings; contents are never copied from original assets.
        for k in range(2):
            for size in range(9):
                name=0x90000+k*0x1000+size*0x100
                p += [[(0x4ef478 if k==0 else 0x4ef4a0)+4*size,4,name],
                      [name,2,ord('a')+size+((ord('x')+k)<<8)],[name+2,1,0]]
        for i,size in enumerate(sizes):
            obj=0xa0000+i*0x40
            p += [[R+0x2540+i*0x48,4,obj],[obj,4,0x90000+kind*0x1000+size*0x100],
                  [R+0x2544+i*0x48,2,stacks]]
        return p
    for kind in (0,1):
        for wanted in range(9):
            for sizes in ((wanted,),(0,8),(0,3,5),(),(wanted,wanted)):
                for stacks in (0,1,2):
                    c=cb();c[27][1]=3;c[29][1]=7;c[28][1]=0x3f800001
                    now_at=0xac if kind==0 else 0xc0
                    # exact deficit chosen without host floating arithmetic
                    import struct
                    now=int.from_bytes(struct.pack('<f',600-(wanted*60+10)),'little')
                    add('potion_selection',potion(kind,sizes,stacks)+[[R+now_at,4,now]],
                        [[13,kind],[10,P],[11,P],[13,kind]],c)
        for count in (0,1,5,6,0xffffffff):
            for local in (0,1):
                for menu in (0,1,255):
                    c=cb();c[23][1]=local;c[27][1]=count;c[16][1]=0x88888
                    c[28][1]=0x3f800001;c[29][1]=0xffffffff
                    p=potion(kind,(8,))+[[0x41b69d,1,menu],[0x4ebbb0,4,0xb0000],
                         [0xb0000+0x17bb0,4,0xc0000],[0xb0000+0x17bb0+0x22,2,0x8001],
                         [0xc001d,1,255],[R+0x1330,4,2],[R+0x2538,4,1]]
                    add('potion_machine_load',p,[[13,kind]],c)
        for service in (16,23,24,25,26,27,28,29):
            for mode in ('fail','mutate','missing'):
                c=cb();c[23][1]=1;c[27][1]=2;c[28][1]=0x3f800000
                if mode=='fail':c[service][3]=1
                elif mode=='missing':c=[b for b in c if b[0]!=service]
                else:c[service][2]=[[0,P+0x3c8,4,1],[1,R+0x4c30+0x1330,4,1]]
                p=potion(kind,(8,))+[[R+0x1330,4,1]]
                if service==25:p += [[0x41b69d,1,1],[0x4ebbb0,4,0xb0000]]
                if service==29:p=potion(kind,())
                add('potion_callback_prefix',p,[[13,kind]],c)
        add('potion_list_args',potion(kind,(8,))+[[0x41b69d,1,1],[0x4ebbb0,4,0xb0000],
              [0xb0000+0x17bb0+2*0x1518+3*0x48,4,0xc0000],
              [0xb0000+0x17bb0+2*0x1518+3*0x48+0x22,2,17],[0xc001d,1,4]],
              [[14,P,kind,2,3,0xffffffff]], [[i,1 if i==23 else 0,[],0] for i in range(30)])
        for maximum in (0,0x3f800000,0x42c80000,0x7f7fffff):
            add('potion_full_rates',potion(kind)+[[R+0xb0,4,maximum],[R+0xc4,4,maximum],
                  [R+0xac,4,maximum],[R+0xc0,4,maximum]],[[13,kind]])
    # Helpers use their argument P while notices use Tick.P, exactly as reference.
    alt=P+0x1000
    add('alternate_player',base()+[[alt+0x3c8,4,0],[alt+0x477,1,255]],
        [[10,alt],[11,alt],[12,alt],[14,alt,0,0,0,0xffffffff]])
    return out

def casting_recipes():
    out=[]
    A,S,T,O,H=0x70000,0x71000,0x72000,0x73000,0x74000
    def cb():
        c=bindings()+[[i,0,[],0] for i in range(19,45)]
        c[30][1]=[3,[[1,0x3f800000,0x40000000,0x42800000],
                       [1,0x40400000,0x40800000,0x42480000]]]
        c[32][1]=0xffffffff;c[36][1]=1;c[37][1]=[2,[]];c[39][1]=3
        c[40][1]=O;c[43][1]=0x41200000
        return c
    def base(sl=0):
        r=R+sl*0x4c30
        return setup(sl)+[[P+0x4ae,2,0],[P+0x488,4,0],[P+0x58c,4,5],
            [r+0xc0,4,0x42c80000],[r+0x4a80,4,8],[P+0x38,2,0x8001],
            [P+0x48,4,T],[S+0x60,4,T+0x400],[O+0x14,4,T+0x800],
            [T+17*8,2,0xffff],[T+20*8,2,0x8000],[T+39*8,2,7],
            [T+0x400+8,2,0xfffe],[T+0x400+16,2,9],[T+0x400+40,2,0x7fff],
            [T+0x808,2,0xfff0],[A,4,0x00007871],[0x535e30,4,0x00007871],
            [A+0x20,2,2],[0x41b170+8,4,S],[S+4,4,1],[S+8,4,1],[S+0xc,2,0x8001],
            [P+0x47b,1,2],[H+0xc,4,O],[P+0x588,4,H]]
    def add(label,pokes,ops,c=None):
        out.append(('casting_'+label,[pokes,cb() if c is None else c,ops]))
    for sl in range(5):
        add('lifecycle',base(sl),[[19,A],[18,P,0],[18,P,1],[17,P,A],[15,P,PAD+sl*0x58],[17,P,A]])
    for flags in (0,0x20000):
        for bit in (0,1):
            for kind in (0,1,0xffffffff):
                for spell in (0,O):
                    c=cb();c[40][1]=spell
                    add('cast_kind_copy',base()+[[P+0xc,4,flags],[S+4,4,bit],[S+8,4,kind]],[[17,P,A]],c)
    for gate in range(6):
        c=cb();p=base();ability=A
        if gate==0:p += [[P+0x4ae,2,1]]
        if gate==1:ability=0
        if gate==2:p += [[A,1,ord('z')]]
        if gate==3:p += [[P+0xc,4,0x20000],[S+4,4,0]]
        if gate==4:c[36][1]=0
        if gate==5:p += [[P+0x4ae,2,0xffff]]
        add('cast_gates',p,[[17,P,ability]],c)
    # First accepted target in callback order, with zeros/duplicates and both
    # rejection and acceptance; subsequent results cannot overwrite the winner.
    for objects,answers in (([],[1]),([0,0x75000,0x76000],[0,0,1]),
                            ([0x75000,0x76000],[0,0]),([0x75000,0x75000,0x76000],[0,1,1]),
                            ([0,0x76000],[1,1])):
        for xy in ((0,0),(0x3f800000,0),(0,0xbf800000),(0x7fc01234,0x7f800000)):
            c=cb();c[37][1]=[2,objects];c[38][1]=[3,answers]
            c[39][1]=[3,[0xffffffff,0x80000000]]
            p=base()+[[0x75010,4,xy[0]],[0x75014,4,xy[1]],
                      [0x76010,4,xy[1]],[0x76014,4,xy[0]]]
            add('targets',p,[[17,P,A]],c)
    bits=(0,0x80000000,1,0x007fffff,0x00800000,0x3f800001,0xbf800000,
          0x4267ffff,0x42680000,0x42680001,0xc2680001,0x7f800000,0xff800000,0x7fc01234,0x7f7fffff)
    for value in bits:
        for lane in range(3):
            c=cb();a=[0x3f800000]*3;b=[0x40000000]*3;a[lane]=value;b[lane]=value
            c[30][1]=[3,[[1,*a],[1,*b]]];c[43][1]=value
            add('aim_cost_bits',base()+[[P+0x18,4,0]],[[17,P,A],[15,P,PAD]],c)
    for state in (0,2,5,11,0xffffffff):
        for held in (0,8):
            for mana in (0,0xbf800000,0x3f800000,0x7fc01234):
                add('update_gates',base()+[[P+0x58c,4,state],[PAD+8,4,held],[R+0xc0,4,mana]],[[15,P,PAD]])
    for track in (0xffffffff,0x80000000,0,1,126):
        for progress in (0,0x3f7fffff,0x3f800000,0x7fc01234):
            for weight in (0xbf800000,0x3f000000,0x40000000,0x7fc01234):
                c=cb();c[32][1]=track;p=base()
                if track<127:p += [[P+0x124+track*0x20,4,progress],[P+0x12c+track*0x20,4,weight]]
                add('update_tracks',p,[[15,P,PAD]],c)
    for forced in (0,1):
        for handle,spell in ((0,0),(H,0),(H,O)):
            add('update_forced_handle',base()+[[P+0x488,4,forced],[P+0x588,4,handle],[H+0xc,4,spell]],[[15,P,PAD]])
    for ident in (0,0x7fff,0x8000,0xffff):
        signed=ident if ident<0x8000 else ident-0x10000
        add('signed_skill_id',base()+[[A+0x20,2,ident],[0x41b170+4*signed,4,S]],[[19,A],[17,P,A]])
    # Swap latch, three-attempt fallback, malformed signed selector bytes,
    # secondary equipment, force bypass, no-combat and held-block suppression.
    def swap(sl=0):
        r=R+sl*0x4c30
        return base(sl)+[[P+0x58c,4,2],[0x41b9d0+4*sl,4,8],[0x41ba30+4*sl,4,4],
            [r+0x4434,4,0x77000],[r+0x4438,4,0x78000],[r+0x443c,4,0x79000],
            [0x77000,4,0x7a000],[0x78000,4,0x7b000],[0x7b01e,1,6]]
    for state in (0,2,3,8,11,14,0xffffffff):
        for force in (0,1,0xffffffff):
            for input_bits in range(4):
                for latch in (0,1,128):
                    p=swap()+[[P+0x58c,4,state],[PAD+8,4,8 if input_bits&1 else 0],
                       [PAD+0xc,4,4 if input_bits&2 else 0],[R+0x4a7d,1,latch],[R+0x43fc,4,1]]
                    add('swap_gates',p,[[16,P,PAD,force],[16,P,PAD,force]])
    for selector in (0,1,2,127,128,255):
        for latch in (0,1):
            for primary in (0,1,2,3):
                p=swap()+[[R+0x4a7c,1,selector],[R+0x4a7d,1,latch],[PAD+0xc,4,4],
                    [R+0x4434,4,0x77000 if primary&1 else 0],
                    [R+0x4438,4,0x78000 if primary&2 else 0]]
                add('swap_selectors',p,[[16,P,PAD,0]])
    for sl in range(5):
        r=R+sl*0x4c30
        for combat in (0,0x10000000):
            for equipped in (0,0x77000,0x78000):
                add('swap_equipped',swap(sl)+[[0x44fe08,4,combat],[r+0x43f0,4,equipped],
                    [r+0x43f4,4,0x79000],[PAD+sl*0x58+0xc,4,4]],[[16,P,PAD+sl*0x58,0]])
    # Every new callback's reached failure/missing prefix and mutation rereads.
    for service in range(30,45):
        op=[15,P,PAD] if service in (31,32,33) else ([16,P,PAD,1] if service in (34,35) else [17,P,A])
        for mode in ('fail','missing','mutate'):
            c=cb();c[37][1]=[2,[0x75000]];c[38][1]=1;p=swap() if service in (34,35) else base()
            if mode=='fail':c[service][3]=1
            elif mode=='missing':c=[b for b in c if b[0]!=service]
            else:c[service][2]=[[0,P+0x3c8,4,1],[1,P+0x18,4,0x42c80000],
                 [0,P+0x38,2,0xffff],[0,P+0x544,4,0xdeadbeef],[0,S+8,4,0],
                 [0,P+0xc,4,0x20000],[0,R+0x4a7c,1,1]]
            add('callback_prefix',p,[op,op],c)
    # Equip notice callback failure/mutation; original R remains captured but
    # notice addresses use Tick.P's current slot.
    for mode in ('normal','fail','mutate'):
        c=cb()
        if mode=='fail':c[29][3]=1
        if mode=='mutate':c[34][2]=[[0,P+0x3c8,4,1],[0,R+0x4a7c,1,1]]
        add('swap_notice',swap()+[[PAD+0xc,4,4]],[[16,P,PAD,0]],c)
    alt=P+0x2000
    add('alternate_player',base()+[[alt+0x3c8,4,0],[alt+0x48,4,T],[alt+0x58c,4,5],
        [alt+0x588,4,H]],[[18,alt,1],[17,alt,A],[15,alt,PAD],[16,alt,PAD,1]])
    return out

def action_recipes():
    out=[]
    def cb():
        c=bindings()+[[i,0,[],0] for i in range(19,60)]
        c[30][1]=[3,[[1,0x3f800000,0x40000000,0x42800000],[1,0x40400000,0x40800000,0x42480000]]]
        c[37][1]=[2,[]];c[49][1]=0x70000;c[16][1]=0x90000;c[20][1]=0x73000;c[57][1]=1
        return c
    def base(sl=0):
        r=R+sl*0x4c30
        return setup(sl)+[[P+0x4a8,4,0xffffffff],[P+0x118,1,4],[P+0x119,1,4],
            [P+0x24,4,0x30000],[0x30000,4,0x31000],[r+0x4b08,2,0xffff],
            [0x5362a0,8,0x4066800000000000],[P+0x540,4,0x72000],
            [0x72048,4,0x74000],[0x74088,2,0xffff],[0x73014,4,0x75000],[0x75020,2,0x8000],
            [0x41b721,2,ord('a')],[0x535e40,2,ord('b')],
            [0x41b9d0+sl*4,4,8],[r+0x4a80,4,4],[PAD+sl*0x58+0xc,4,4]]
    def add(label,pokes,ops,c=None):out.append(('action_'+label,[pokes,cb() if c is None else c,ops]))
    # All states, combinations of signed flags/counts, copy, held input, animation events.
    for state in range(10):
        for sl in range(5):
            for flags in (0,8,0x20,0x28):
                for copy in (0,0x20000):
                    c=cb();c[9][1]=flags|1
                    p=base(sl)+[[P+0x11a,2,flags],[P+0xc,4,copy],[P+0x47b,1,1],
                        [P+0x47f,1,1],[P+0x4a8,4,3],[PAD+sl*0x58+8,4,8],
                        [P+0x188,4,0x4080a3d6],[P+0xf0,4,0xffffffe2]]
                    add('states',p,[[20,state,PAD+sl*0x58,R+sl*0x4c30]]*2,c)
    for state in (0,2,4,5,7,9):
        for count in (0,1,4,6,7,127,128,255):
            for bit in (0,1,128,255):
                p=base()+[[P+0x118,1,count],[P+0x47b,1,bit],[P+0x47f,1,bit],
                    [P+0x47d,1,bit],[P+0x47e,1,bit],[P+0x11a,2,0],
                    [P+(count if count<128 else count-256)*0x20+0x104,4,0x3f800000]]
                add('signed_tracks_flags',p,[[20,state,PAD,R]])
    for value in (0,1,0x80000000,0x007fffff,0x00800000,0x3f800001,0xbf800000,
                  0x4080a3d5,0x4080a3d6,0x4080a3d7,0x7f800000,0xff800000,0x7fc01234):
        for wet in (0,0x2000000000000000):
            p=base()+[[P+0xa8,8,wet],[P+0x194,4,value],[P+0x30,4,value],[P+0x18,4,value]]
            add('jump_water_bits',p,[[20,2,PAD,R],[20,4,PAD,R]])
    for frame in (0,179,180,181,0x80000000,0xffffffff):
        for start in (0,1,0xffffffff):
            for countdown in (0,1,2,0x7fff,0x8000,0xffff):
                add('dead_time',base()+[[0x44fdcc,4,frame],[P+0x448,4,start],[0x4f84a8,2,countdown]],[[20,1,PAD,R]])
    for events in range(16):
        for copy in (0,0x20000):
            for angle in (0,0x7fff,0x8000,0xffff):
                c=cb();c[9][1]=events
                add('pound_events',base()+[[P+0xc,4,copy],[P+0x38,2,angle],
                    [R+0x4b08,2,0x26],[R+0x4b0a,2,0x8001],[R+0x4b0c,2,0xffff]],[[20,7,PAD,R]],c)
    for state in (6,14):
        for selector in (0,1,2,0x8000,0xffff):
            for index in (0,1,127,128,255):
                for ident in (0x31,0x22,0xffff):
                    addr=(R+0x4440+(index if index<128 else index-256)*0x68)&0xffffffff
                    p=base()+[[P+0x58c,4,state],[R+0x4a60,2,selector],[R+0x4a84,1,index],
                        [R+0x4a85,1,index],[addr+0x20,2,ident]]
                    add('skill_selection',p,[[21,9,0,0],[20,8,PAD,R]])
    for held in (0,8):
        for rank in (0,1,0x7fff,0x8000,0xffff):
            for seed in range(12):
                add('block_particles',base()+[[PAD+8,4,held],[P+0x184,4,0x3f800000],
                    [R+0x4b08,2,0x3b],[R+0x4b0a,2,rank],[R+0x4b0c,2,0xffff],
                    [0x404464,4,seed]],[[20,9,PAD,R],[20,9,PAD,R]])
    for bit in (1,128,255):add('recall_leave',base()+[[0x41b51d,1,bit]],[[20,4,PAD,R]])
    for slow in range(256):
        add('slow',base()+[[P+0x2fc,1,slow]],[[21,6,0x3f800001,0],[21,6,0x7fc01234,0]])
    for count in (0,1,2,4,127,128,255):
        add('clear_tracks',base()+[[P+0x119,1,count]],[[21,5,0,0]])
    for value in (0,1,0x7fff,0x8000,0xffff):
        add('haste',base()+[[0x80024,2,0x14],[0x80026,2,value]],[[21,7,0x80000,0]])
    for cls in (0,1,7,127,128,255):
        add('animation_lookup',base()+[[R+0x4a86,1,cls],
            [0x30000+(cls if cls<128 else cls-256)*0x68+0x84,4,0x98765]],[[21,8,0x84,0]])
    for value in (0,1,0x7fffffff,0x80000000,0xffffffff):
        add('frames',base()+[[0x44fdcc,4,value],[P+0x448,4,0xffffffff]],[[21,2,0,0]])
    for count in (0,4,128,255):add('top_track',base()+[[P+0x118,1,count]],[[21,3,0xf0,0],[21,3,0xffffffff,0]])
    add('helpers',base(),[[21,0,0x40000000,0],[21,1,2,3],[21,4,0,0],[21,10,0x81234,0x82345],[21,11,0,0]])
    # Absence/zero returns, animation reuse, copy dismount, and both spell answers.
    for state in (5,7):
        c=cb();c[49][1]=0
        add('allocation_zero',base()+[[0x44fdcc,4,30],[P+0xf0,4,0]],[[20,state,PAD,R]],c)
    for selector in (0,1):
        for ident in (0x31,0x22):
            for spell in (0,0x73000):
                for answer in (0,1):
                    c=cb();c[20][1]=spell;c[57][1]=answer
                    p=base()+[[R+0x4a60,2,selector],[R+0x4440+0x20,2,ident],
                        [P+0x58c,4,6]]
                    add('skill_spell_answers',p,[[20,8,PAD,R]],c)
    for copy in (0,0x20000):
        for press in (0,0x10):
            for playing in (0,0x90000):
                for equal in (0,1):
                    for fallback in (0,1):
                        c=cb();c[16][1]=[3,[0x90000,0 if fallback else 0x90000,0x91000]]
                        p=base()+[[P+0xc,4,copy],[PAD+0xc,4,press],
                            [P+0x11c,4,playing],[0x535e40,2,ord('a') if equal else ord('b')]]
                        add('vehicle_gates',p,[[20,6,PAD,R]],c)
    for playing in (0,0x90000):
        for held in (0,8):
            for progress in (0,0x3f7fffff,0x3f800000,0x7fc01234):
                for flag in (0,1):
                    add('block_reuse',base()+[[P+0x11c,4,playing],[PAD+8,4,held],
                        [P+0x184,4,progress],[P+0x47f,1,flag]],[[20,9,PAD,R]])
    for pvp in (0,1):
        for bits in (0,0x8000000000000000,0x4066800000000000,
                     0x7ff0000000000000,0xfff0000000000000,0x7ff8000000001234):
            add('dead_double_bits',base()+[[0x4f711c,4,pvp],[0x5362a0,8,bits],
                [0x44fdcc,4,180]],[[20,1,PAD,R]])
    # Each new service reaches failures/missing/mutations, with complete prefixes.
    reaches={45:3,46:0,47:1,48:2,49:5,50:5,51:6,52:6,53:7,54:7,55:8,56:8,57:8,58:9,59:9}
    for service,state in reaches.items():
        for mode in ('fail','missing','mutate'):
            c=cb();c[9][1]=9
            p=base()+[[P+0x4a8,4,1],[P+0x47b,1,1],[0x44fdcc,4,180],[P+0xf0,4,150],
                [P+0xa8,8,0x2000000000000000],[P+0x194,4,0x4080a3d6],
                [R+0x4a60,2,0],[R+0x4440+0x20,2,0x22 if service==56 else 0x31],
                [R+0x4b08,2,0x3b],[R+0x4b0a,2,1],[R+0x4b0c,2,0xffff]]
            if mode=='fail':c[service][3]=1
            elif mode=='missing':c=[x for x in c if x[0]!=service]
            else:c[service][2]=[[0,P+0x3c8,4,1],[1,P+0x118,1,5],[0,P+0x11a,2,8],[0,R+0x4c30+0x4b08,2,0xffff]]
            add('callback_prefix',p,[[20,state,PAD,R]],c)
    # Existing services can change slots/counts/flags between rereads.
    for service in (9,10,16,17,24,30):
        for state in range(10):
            c=cb();c[service][2]=[[0,P+0x3c8,4,1],[0,P+0x118,1,6],
                [0,P+0xc,4,0x20000],[0,R+0x4c30+0x4b08,2,0xffff]]
            add('callback_rereads',base(),[[20,state,PAD,R]],c)
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/player.tsv.gz'))
    args=parser.parse_args();modules=load(args.research_dir)
    rows=[];counts={}
    for config in range(16):
        for label,inputs in recipes()+recovery_recipes()+casting_recipes()+action_recipes():
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
        'statuses':{'0':'success','1':'ordinary boundary refusal','2':'missing handler','4':'injected callback exception','5':'reference Leave'},
        'scope':'bounded action states hit/dead/jump/rush/block-move/warcry/vehicle/pound/skill/block and shared helpers; entry through scripted return; choose_pad; idle_fidget; can_block; try_block_move; has_anim; skill_rank; passive_level; regen_health; regen_mana; tick_enchant_timers; use_potion_real; use_potion; update_cast; weapon_swap_input; hands_midpoint; skill_of; cast_skill'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(compressed)
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(packed({'count':len(rows),'expanded_bytes':len(data),'sha256':manifest['vector_sha256']}))
if __name__=='__main__':main()
