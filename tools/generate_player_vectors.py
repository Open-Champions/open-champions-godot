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
 'SpellBash_typeinfo','SpellShieldBash_typeinfo','Spell.v4','particleDef_ctor','particle_spawn','text_lookup','hud_showMessage','libc_sprintf',
 'Player_findTarget','Player.v26','str_prefixDiffers','pad_buttonValue','SpellAncestralCall_typeinfo','SpellAncestralCall_queryFor',
 'SpellBattleCry_typeinfo','SpellBattleCry_affects','SkillBerserkerRage_swingFactor',
 'world_collectObjectsInBox','Creature.v12','iter_begin','iter_next','iter_end',
 'Player_meleeHit','handle_removeNode','Player_pickAttackAnim',
 'player_recalcLoad','item_getResourceIndex','playerProjectile_ctor','item_getModifiedField1a',
 'script_runByName','Skill.v10','item_getDamageRange','MissileWeapon_ctor','Root_ctor','UnholyAura_ctor','Player_weaponTrails','dialog_22fbd0','Player_processDamage','Player_interact','light_link2']
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
    under_calls=[]
    class Under:
        def load(self,a,size):
            under_calls.append([a,size])
            salt,fail,effects=inputs[3]
            for effect in effects:memory.poke(*effect)
            if fail and len(under_calls)==fail:raise Injected()
            return sum(((a+i+salt)&255) << (8*i) for i in range(size))
    memory = ref.Memory(Under() if len(inputs)>=4 and inputs[3] else None); events=[]
    class Traced(ref.Tick):
        def _w(self, a, size, value):
            # Observe the reference's exact entry prefix before its first
            # ordinary-only store. The suffix is never substituted or modelled.
            if a == P+0x4c4 and getattr(self,'ordinary_boundary',False): raise Boundary()
            events.append([0,[a,size,value & ((1 << (8*size))-1)]])
            return super()._w(a,size,value)
        def call(self, name, *args):
            args=tuple(a & 0xffffffff if isinstance(a,int) else a for a in args)
            events.append([1,[service_key(name),[argument(a) for a in args]]])
            return super().call(name,*args)
    def service_key(name):
        return SERVICES.index(name) if name in SERVICES else list(name.encode("latin1"))
    def argument(a):
        return [3,0] if a is None else ([2,0] if a == "stack" else ([1,*a] if isinstance(a,tuple) else [0,a]))
    tick=Traced(memory,P, external=[bytes(v).decode('latin1') for v in inputs[4][0]] if len(inputs)==5 else ())
    for name in SERVICES:
        def missing(*args): raise Missing()
        tick.cb[name]=missing
    for service, result, effects, fail in inputs[1] + (inputs[4][1] if len(inputs)==5 else []):
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
                if value[0] in (6,7):return value[1]
                if value[0]==8:return None
                return tuple(tuple(v) for v in value[1:]) if value[0]==5 else (tuple(value[1:]) if value[0] in (1,4) else value[1])
            return value
        tick.cb[SERVICES[service] if isinstance(service,int) else bytes(service).decode('latin1')]=callback
    for poke in inputs[0]:memory.poke(*poke)
    def snapshot():
        return copy.deepcopy([sorted(memory.bytes.items()),memory.log,tick.writes,
            [[service_key(name),[argument(a) for a in args]] for name,args in tick.calls],
            events,tick.draws,getattr(tick,'R0',0)] + ([under_calls] if len(inputs)>=4 else []))
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
            elif op==22:
                helper,a,b,r0,instant=args;tick.R0=r0;tick.instant=instant
                if helper==0:tick.pad=a;tick.state_neutral()
                elif helper==1:result=int(tick.neutral_start_ability(a,b,r0))
                elif helper==2:tick.pad=a;result=int(tick.neutral_attack(b,tick.slot))
                elif helper==3:tick.old_arrow_notices()
                else:raise ValueError('neutral helper')
            elif op==23:
                helper,a,b,r0,instant=args;tick.R0=r0;tick.instant=instant
                if helper==0:tick.pad=a;tick.state_melee()
                elif helper==1:result=tick.melee_next_swing(a,ref.s32(b),r0)
                elif helper==2:result=tick.draw3()
                else:raise ValueError('melee helper')
            elif op==24:
                helper,a,b,extra,r0=args;tick.R0=r0
                if helper==0:tick.pad=a;tick.state_ranged()
                elif helper==1:tick.ranged_throw(a,ref.s32(b))
                elif helper==2:tick.ranged_shoot()
                elif helper==3:tick.missile(tick.vec(a),tick.vec(b),ref.s32(extra))
                elif helper==4:result=tick.aim_at_target(a if b else None,extra)
                elif helper==5:result=tick.obj_rand(a if b else None)
                elif helper==6:result=tick.strstr(a,b)
                else:raise ValueError('ranged helper')
            elif op==25:
                helper,a,b,extra,r0=args;tick.R0=r0
                if helper==0:tick.pad=a;tick.state_cast()
                elif helper==1:tick.pad=b;tick.cast_old_style(a)
                elif helper==2:tick.light_effect(a,0,[tick.u32(b+4*i) for i in range(extra)])
                elif helper==3:tick.old_fire_nova(a)
                elif helper==4:result=tick.strcmp(a,b)
                elif helper==5:tick.pad=a;tick.instant=extra;tick.switch()
                else:raise ValueError('legacy cast helper')
            elif op==26:tick.top();result=[tick.pad,tick.instant,tick.weapon0]
            elif op==27:tick.pad=args[0];tick.instant_abilities();result=tick.instant
            elif op==28:result=[tick.pad,tick.instant,getattr(tick,'weapon0',0)]
            elif op==29:tick.pad=args[0];tick.tail()
            elif op==30:
                result={'end':0,'scripted':1,'recall during block move':2,
                        'ability button index':3}[tick.run().returned]
            elif op==31:
                helper,a,value=args
                if helper==0:result=ref.s8(a)
                elif helper==1:result=tick.u8(a)
                elif helper==2:result=tick.u16(a)
                elif helper==3:result=tick.u64(a)
                elif helper==4:tick.w8(a,value)
                elif helper==5:tick.w16(a,value)
                elif helper==6:tick.w32(a,value)
                elif helper==7:result=list(tick.cstr(a,value))
                else:raise ValueError('utility helper')
            elif op==32:result=memory.load(*args)
            elif op==33:memory.write_bytes(*args)
            elif op in (34,35,36):
                def decode(v):
                    kind,*data=v
                    return data[0] if kind==0 else (tuple(data) if kind==1 else ('stack' if kind==2 else None))
                name=bytes(args[0]).decode('latin1')
                key=name if op!=35 else name+'.v'+str(args[3])
                if key not in tick.cb:
                    def missing(*unused):raise Missing()
                    tick.cb[key]=missing
                if op==34:result=tick.call(name,*(decode(a) for a in args[1]))
                elif op==35:result=tick.vcall(name,*args[1:4],*(decode(a) for a in args[4]))
                else:
                    spec,argv=args[1:];_,value,effects,fail=spec
                    def internal(*unused):
                        for log,a,size,v in effects:(memory.store if log else memory.poke)(a,size,v)
                        if fail:raise Injected()
                        if isinstance(value,list):
                            if value[0] in (6,7):return value[1]
                            if value[0]==8:return None
                            return tuple(tuple(v) for v in value[1:]) if value[0]==5 else (tuple(value[1:]) if value[0] in (1,4) else value[1])
                        return value
                    result=tick.helper(name,internal,*(decode(a) for a in argv))
            elif op==39:result=tick.use_potion(*args)
            elif op==37:tick.external=set(bytes(v).decode('latin1') for v in args[0])
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

def neutral_recipes():
    out=[];A=0x70000;S=0x71000;T=0x72000;N=0x73000;O=0x74000;AMMO=0x75000
    def cb():
        c=bindings()+[[i,0,[],0] for i in range(19,67)]
        c[30][1]=[1,0,0,0];c[37][1]=[2,[]];c[36][1]=1;c[39][1]=2
        c[43][1]=0x40000000;c[16][1]=0x90000;c[49][1]=0x76000;c[62][1]=0x77000
        c[66][1]=0x3f800000;c[45][1]=5
        return c
    def base(sl=0):
        r=R+sl*0x4c30
        return setup(sl)+[[P+0x58c,4,2],[P+0x118,1,4],[P+0x119,1,4],[P+0x24,4,0x30000],
          [0x30000,4,0x31000],[P+0x48,4,T],[T+26*8,2,0xfffc],[T+39*8,2,2],
          [P+0x482,1,1],[P+0x4ae,2,0],[P+0x488,4,A],[A,2,ord('o')],[0x535e30,2,ord('n')],
          [A+0x20,2,1],[A+0x22,2,0x1234],[A+0x24,1,1],[A+0x25,1,2],[A+0x2c,4,0x40000000],
          [0x41b174,4,S],[S+0x60,4,T],[S+8,4,0],[T+8,2,0xfffe],[T+40,2,4],
          [r+0xc0,4,0x42c80000],[r+0xc4,4,0x42c80000],[r+0x4a80,4,4],[r+0x4a86,1,0],
          [r+0x4b08,2,0xffff],[0x41b980+4*sl,4,8],[0x41b9d0+4*sl,4,16],
          [P+0x234,4,N],[N+0xc,4,O],[O+0x10,4,0x41200000],[P+0x448,4,0],
          [0x44fdcc,4,100],[r+0x4410,4,AMMO],[AMMO+4,2,10]]
    def add(label,p,ops,c=None):out.append(('neutral_'+label,[p,cb() if c is None else c,ops]))
    def state(sl=0,instant=0):return [22,0,PAD+sl*0x58,0,R+sl*0x4c30,instant]
    def start(cost=0x40000000,sl=0):return [22,1,A,cost,R+sl*0x4c30,0]
    def attack(instant=0,sl=0):return [22,2,PAD+sl*0x58,8,R+sl*0x4c30,instant]
    # Complete direct starts, every legacy ID branch, every kind, classes and copy paths.
    for sid in (0xe,0x25,0xc,4,5,6,0x13,0x12,1,0xffff):
      for cls in (0,1,2,3,4,5,6,7,128,255):
        for copy in (0,0x20000):
          add('legacy_start',base()+[[A+0x20,2,sid],[R+0x4a86,1,cls],[P+0xc,4,copy]], [start(),start()])
    for kind in (0,1,2,7,8,128,255):
      for allowed in (0,1):
        for copy in (0,0x20000):
          c=cb();c[36][1]=allowed
          p=base()+[[A,2,ord('n')],[A+0x25,1,kind],[P+0xc,4,copy]]
          add('skill_start',p,[start(),state()],c)
    for sl in range(5):
      for ground in (0,1,128):
        for bits in (0,4,8,16,28):
          for instant in (0,8,0xffffffff):
            add('state_input',base(sl)+[[P+0x482,1,ground],[P+0x488,4,0],
              [R+sl*0x4c30+0x4a60,2,2],[PAD+sl*0x58+8,4,bits],[PAD+sl*0x58+0xc,4,bits]], [state(sl,instant)])
    # Gates on the ability path, with selected versus forced and new versus old.
    gates=[[],[[P+0x4ae,2,1]],[[P+0x118,1,3]],[[0x44fe08,4,0x10000000]],
      [[0x41b818,4,1]],[[A+0x20,2,0xffff]],[[R+0x4bc9,1,1]],[[0x4f7128,4,1]],
      [[A+0x20,2,0x25],[R+0x4a86,1,4]],[[A+0x20,2,0x25],[R+0x4a86,1,6]],
      [[A+0x2c,4,0x4479c000],[R+0xc0,4,0x42a00000]],[[A+0x24,1,0]],
      [[R+0xc0,4,0x3f800000]],[[P+0x450,4,1]],[[R+0x4a60,2,2]]]
    for gate in gates:
      for forced in (0,A):
        for kind in (0,2):
          for network in (0,1,128):
            for new in (0,1):
              p=base()+[[P+0x488,4,forced],[R+0x4a60,2,0],[R+0x4a84,1,0],
                [R+0x4440,2,ord('n') if new else ord('o')],[R+0x4440+0x20,2,1],
                [R+0x4440+0x24,2,1|(kind<<8)],[R+0x4440+0x2c,4,0x40000000],
                [A,2,ord('n') if new else ord('o')],[A+0x25,1,kind],
                [PAD+8,4,4],[PAD+0xc,4,4],[0x402791,1,network]]+gate
              add('ability_gates',p,[state()])
    for value in (0,0x80000000,0x3f7fffff,0x3f800000,0x3f800001,0x40000000,
                  0x4479c000,0x7f800000,0xff800000,0x7fc01234):
      for skillkind in (0,1):
        c=cb();c[43][1]=value
        add('cost_bits',base()+[[A,2,ord('n')],[S+8,4,skillkind],[PAD+0xc,4,4]], [state()],c)
        add('mana_bits',base()+[[R+0xc0,4,value],[PAD+0xc,4,4]], [state()])
        add('heavy_bits',base()+[[A+0x20,2,0x25],[R+0xc0,4,value]], [start(value)])
        add('timer_bits',base()+[[P+0x482,1,0],[R+0x4a5c,4,value]], [state()])
    for level in (0,1,2,5,127,128,255):
      add('heavy_passive',base()+[[A+0x20,2,0x25],[0x4ef538,8,0x20000],
          [R+0xef,1,level],[PAD+0xc,4,4]], [state()])
    for skillkind in (0,1):
      for sid in (0xe,0x25,0xc,4,5,6,0x13,0x12,1):
        add('ability_state',base()+[[A+0x20,2,sid],[PAD+0xc,4,4],[S+8,4,skillkind]], [state()])
    # Heavy cooldown, track bounds, warcry cooldown and failed allocation.
    for count in (0,1,2,5,6,127,128,255):
      for busy in (0,1,2):
        for pool in (0,0x76000):
          c=cb();c[49][1]=pool
          add('heavy_gates',base()+[[A+0x20,2,0x25],[P+0x118,1,count],[P+0x119,1,count],[P+0x44c,4,busy]], [start()],c)
          add('cry_gates',base()+[[A+0x20,2,0xe],[P+0x374,4,busy]], [start()],c)
    # Punch own seed, signed ammo counts, target presence/distance and animation reuse.
    for cls in (4,6):
      for ammo in (0,AMMO):
        for count in (0,1,20,21,0x7fff,0x8000,0xffff):
          for node in (0,N):
            for distance in (0,0x426fffff,0x42700000,0x42700001,0x7fc01234):
              add('punch',base()+[[R+0x4a86,1,cls],[R+0x4410,4,ammo],[AMMO+4,2,count],
                [P+0x234,4,node],[P+0x10,4,0],[P+0x14,4,0],[P+0x18,4,0],
                [O+0x10,4,distance],[P+0x44,4,count]], [attack()])
    for ident in (0x535fb8,0x535fc8):
      add('punch_playing',base()+[[R+0x4a86,1,4],[AMMO+4,2,0],[P+0x11c,4,0x90000]], [attack()])
    # Bow pressure short circuit, exact threshold, signed track counts, forced bypass.
    for one in (0,0x3e051eb7,0x3e051eb8,0x3e051eb9,0x7fc01234):
      for two in (0,0x3e051eb7,0x3e051eb8,0x7fc01234):
        for count in (0,1,4,127,128,255):
          c=cb();c[66][1]=[3,[one,two]]
          add('bow_pressure',base()+[[R+0x50,4,1],[R+0x4a86,1,4],[PAD+8,4,8],
            [P+0x488,4,0],[P+0x119,1,count]], [attack()],c)
    for copy in (0,0x20000):
      for cls in (0,4,6,128,255):
        for held in (0,4,8):
          for instant in (0,4,0xffffffff):
            for restricted in (0,1):
              add('attack_gates',base()+[[R+0x4a86,1,cls],[R+0x50,4,1],[R+0x4410,4,0],
                [P+0xc,4,copy],[PAD+8,4,held],[PAD+0xc,4,held],[P+0x488,4,0],
                [0x4f7128,4,restricted]], [attack(instant)])
    for selector in (0,1,127,128,255):
      for differs in (0,1):
        c=cb();c[65][1]=differs
        offset=selector if selector<128 else selector-256
        add('restricted_throw',base()+[[R+0x4a86,1,6],[R+0x50,4,1],[R+0x4a7c,1,selector],
          [R+0x4434+4*offset,4,0x78000],[0x78000,4,0x79000],[0x79000,4,0x7a000],
          [0x4f7128,4,1],[PAD+8,4,8]], [attack()],c)
    for positions in ((),(0,),(63,),(0,31,63)):
      p=base()+[[R+0x4a86,1,4],[R+0x50,4,1],[R+0x4410,4,0],[PAD+8,4,8]]
      for i in positions:p.extend([[R+0x130+i*0x48,4,0x78000],[0x7801c,1,8]])
      add('last_ammo',p,[attack(),attack()])
    for count in (0,1,19,20,21,0x7fff,0x8000,0xffff):
      add('ammo_notice',base()+[[AMMO+4,2,count]], [[22,3,0,0,R,0]])
    # Block success/refusal and copy-safe timer updates/footstep event gates.
    for equipment in (0,0x78000):
      for cls in (0,4,6):
        for request in (0,1,2):
          for flags in (0,0x10000000):
            c=cb();c[45][1]=request
            add('block_idle',base()+[[P+0x482,1,0],[P+0x454,4,request],[R+0x43f0,4,equipment],
              [R+0x4a86,1,cls],[PAD+8,4,16],[0x44fe08,4,flags]], [state()],c)
    # Reached-service failure/missing and dynamic callback effects, including target rereads.
    reaches={60:(base()+[[0x4f7128,4,1],[PAD+0xc,4,4]],state()),
      61:(base()+[[0x4f7128,4,1],[PAD+0xc,4,4]],state()),
      62:(base()+[[A+0x20,2,0xe]],start()),
      63:(base()+[[R+0x4a86,1,4],[AMMO+4,2,0]],attack()),
      64:(base()+[[R+0x4a86,1,4],[AMMO+4,2,0]],attack()),
      65:(base()+[[R+0x4a86,1,6],[R+0x50,4,1],[R+0x4434,4,0x78000],
            [0x4f7128,4,1],[PAD+8,4,8]],attack()),
      66:(base()+[[R+0x4a86,1,4],[R+0x50,4,1],[PAD+8,4,8],[P+0x488,4,0]],attack())}
    for service,(p,op) in reaches.items():
      for mode in ('fail','missing','mutate'):
        c=cb()
        if mode=='fail':c[service][3]=1
        elif mode=='missing':c=[b for b in c if b[0]!=service]
        else:c[service][2]=[[0,P+0x3c8,4,1],[0,P+0xc,4,0x20000],[0,P+0x118,1,6],
          [1,P+0x234,4,N],[0,O+0x10,4,0x42700000],[0,R+0x4c30+0x4a86,1,4]]
        add('callback_prefix',p,[op],c)
    for service in (6,16,18,24,29,36,39,43,45,49,50):
      for sid in (0xe,0x25,4,1):
        c=cb();c[service][2]=[[0,P+0x3c8,4,1],[0,P+0xc,4,0x20000],
          [0,P+0x119,1,2],[0,R+0x4c30+0x4a86,1,4],[0,R+0x4c30+0x4b08,2,0xffff]]
        add('callback_rereads',base()+[[A+0x20,2,sid],[PAD+0xc,4,4]], [state()],c)
    # Existing callbacks fail at reached sites with all preceding writes retained.
    paths=[(base()+[[A+0x20,2,0xe]],start(),(62,16,18)),
      (base()+[[A+0x20,2,0x25]],start(),(16,24,18,49,50)),
      (base()+[[A+0x20,2,4],[R+0x4a86,1,4]],start(),(29,)),
      (base()+[[A,2,ord('n')],[PAD+0xc,4,4]],state(),(39,43,6)),
      (base()+[[P+0x482,1,0]],state(),(45,10))]
    for p,op,services in paths:
      for service in services:
        for mode in ('fail','missing'):
          c=cb()
          if mode=='fail':c[service][3]=1
          else:c=[b for b in c if b[0]!=service]
          add('existing_callback_prefix',p,[op],c)
    return out

def melee_recipes():
    out=[];W=0x70000;D=0x71000;O=0x72000;N=0x73000;T=0x74000;A=0x75000
    def cb():
        c=bindings()+[[i,0,[],0] for i in range(19,80)]
        c[30][1]=[1,0,0,0];c[37][1]=[2,[]];c[72][1]=[2,[]]
        c[68][1]=[4,0,0];c[70][1]=[4,0,0]
        c[62][1]=0x76000;c[16][1]=0x77000;c[45][1]=1
        return c
    def base(cls=0,t=10):
        return setup()+[[P+0x58c,4,3],[P+0x118,1,4],[P+0x119,1,4],
          [P+0x24,4,0x30000],[0x30000,4,0x31000],[P+0x48,4,T],
          [P+0x234,4,N],[N+0xc,4,O],[O+0x10,4,0x41200000],[O+0x48,4,T],
          [T+26*8,2,0xfffc],[T+20*8,2,2],[T+12*8,2,4],
          [P+0x448,4,(100-t)&0xffffffff],[P+0x45c,4,99],[P+0x458,4,0],
          [P+0x470,4,0x3fcccccc],[R+0x4a86,1,cls],[R+0x4a60,2,2],
          [R+0x4b08,2,0xffff],[0x44fdcc,4,100],[0x41b980,4,8],
          [0x41b9d0,4,16],[0x41b990,4,4],[0x41b9a0,4,32],[P+0x47f,1,0],
          [0x535e30,2,ord('n')],[A,2,ord('o')],[P+0x488,4,A]]
    def state(instant=0):return [23,0,PAD,0,R,instant]
    def next(steps=7,started=90):return [23,1,R,steps&0xffffffff,started&0xffffffff,0]
    def add(label,p,ops=None,c=None):out.append(('melee_'+label,[p,cb() if c is None else c,[state()] if ops is None else ops]))
    # Full state: every class, heavy/nonheavy, offline/network hit/sound/voice/sweep/window/end times.
    for cls in (0,1,2,3,4,5,6,7,8,127,128,255):
      for heavy in (0,1,2):
        for t in (0,1,6,9,16,17,18,22,23,29,39,40,54,60,94,95,127,128):
          add('timing',base(cls,t)+[[P+0x44c,4,heavy]])
    for network in (1,128,255):
      for local in (0,1):
        for copy in (0,0x20000):
          for countdown in (0,1,2,0x7fffffff,0x80000000,0xffffffff):
            for t in (1,6,9):
              c=cb();c[23][1]=local
              add('network',base(0,t)+[[0x402791,1,network],[P+0xc,4,copy],[P+0x55c,4,countdown]],c=c)
    # Combo wrapping/clamping and all formatting/late ranged-class paths.
    for cls in (0,1,2,3,4,5,6,7,8,127,128,255):
      for step in (0,3,6,8,126,127,128,255):
        for t in (39,40,53,54,60):
          add('next',base(cls,t)+[[P+0x460,1,step],[P+0x461,1,255]], [next(started=100-t)])
    for steps in (0,1,2,4,7,9,0x7fffffff,0x80000000,0xffffffff):
      for heavy in (0,2):
        add('next_steps',base()+[[P+0x460,1,127],[P+0x44c,4,heavy]], [next(steps)])
    # Haste modifiers use authored timed records; no proprietary inputs.
    for cls in (0,2,7):
      for haste in (0,1,10,100,0xffff):
        for buffs in (0,1,2,3):
          c=cb();c[71][1]=0x3fc00000
          p=base(cls,16)+[[R+0x4434,4,W],[W,4,D],[W+0x24,2,0x14],[W+0x26,2,haste],[W+0x30,2,0xffff],[P+0x374,4,buffs&1],[P+0x37c,4,buffs&2],
            [R+0x43f4,4,A],[A+0x24,2,0x14],[A+0x26,2,5],[A+0x30,2,0xffff]]
          add('haste',p,c=c)
    for best in (0,0x3f000000,0x3f800000,0x40000000,0xbf800000):
      for accepted in (0,1):
        for ancestral in (0,1):
          c=cb();c[20][1]=[3,[0x78000 if ancestral else 0,0x79000]]
          c[68][1]=[4,accepted,best];c[70][1]=[4,accepted,best]
          add('spell_speed',base()+[[0x4f8420,4,2],[0x4f8440,4,0],[0x4f8444,4,1],
            [R+0x43d8,4,O],[R+0x4c30+0x43d8,4,A]],c=c)
    for rank in (0,1,2,255):
      c=cb();c[71][1]=0x3fc00000
      add('berserker',base()+[[R+0x50,4,5],[R+0x4b08,2,0x47],[R+0x4b0a,2,rank],[R+0x4b0c,2,0xffff]],c=c)
    # Target filtering, dot/reach boundaries, ties, sweep linking, virtual response.
    for cls in (0,1,2,3):
      for flags in (0,1,8,0x40,0x20,0x1000000):
        for pvp in (0,1):
          for distance in (0,0x41200000,0x42400000,0x42680000,0xc1200000):
            c=cb();c[72][1]=[2,[P,O,A]];c[38][1]=1
            c[42][2]=[[0,P+0x234,4,N]]
            add('target',base(cls,1)+[[P+0x460,1,3],[O+0xc,4,flags],[0x4f711c,4,pvp],
              [O+0x10,4,distance],[A+0x10,4,distance],[A+0x1c,4,0x3f800000]],c=c)
    for facing in (0,0x4000,0x8000,0xc000,0xffff):
      for radius in (0,0x3f800000,0x42400000):
        c=cb();c[72][1]=[2,[O,A]];c[42][2]=[[0,P+0x234,4,0]]
        add('target_no_link',base(2,1)+[[P+0x234,4,0],[P+0x38,2,facing],[O+0x1c,4,radius]],c=c)
    for special in (0,1):
      for immunity in (0,0x4000000):
        c=cb();c[72][1]=[2,[O]]
        add('target_player_filter',base(0,1)+[[O+0xc,4,0x20],[O+0x312,2,special],[O+0xa8,8,immunity]],c=c)
    # Sweep iterator consumes ordered sequence and preserves callback stores between rereads.
    for t in (94,95,127,128,140):
      for facing in (0,0x4000,0x8000,0xffff):
        c=cb();c[74][1]=N;c[75][1]=[3,[N,0]]
        add('sweep',base(2,t)+[[P+0x38,2,facing],[P+0x460,1,3]],c=c)
    # Formatting variants and nonlockstep RNG advancement.
    for flags in (0,8):
      for bits in (0,0x20000000):
        for seed in (0,1,0x7fffffff,0xffffffff):
          add('sound',base(0,6)+[[R+0x4434,4,W],[W,4,D],[D+0x20,4,flags],
            [W+8,8,bits],[W+0x2a,2,0xffff],[0x404468,4,seed]])
    for seed in (0,1,0x7fffffff,0xffffffff):add('rng3',base()+[[0x404468,4,seed]],[[23,2,0,0,0,0]]*3)
    # Instant skill requests, signed ability indices, block/end/track-reset gates.
    for instant in (0,4,32,36,0xffffffff):
      for pressed in (0,4,8,16,32,36):
        add('input',base(0,23)+[[PAD+0xc,4,pressed],[R+0x4440,2,ord('o')]], [state(instant)])
    for track_count in (0,4,5,6,127,128,255):
      for stick in (0,0x3f4ccccc,0x3f800000):
        for t in (39,40,96):
          add('end',base(0,t)+[[P+0x118,1,track_count],[PAD+0x14,4,stick]])
    for cls in (0,4,5,6):
      for block in (0,1):
        for level in (0,0x10000000):
          add('block',base(cls,22)+[[PAD+0xc,4,16],[P+0x454,4,block],[0x44fe08,4,level]])
    for frame in (0,1,0x7fffffff,0x80000000,0xffffffff):
      for last in (0,60,61,0xffffffff):
        add('frame_wrap',base()+[[0x44fdcc,4,frame],[P+0x45c,4,last],[P+0x448,4,frame-6&0xffffffff]])
    # Every new callback's failure prefix, plus service mutation of captured/current fields.
    for service in (67,68,69,70,71,72,73,74,75,76,77,78,79,9,42,44,62):
      c=cb();c[service][3]=1
      p=base(2,128)+[[0x4f8420,4,1],[R+0x43d8,4,O]]
      c[20][1]=0x78000;c[68][1]=[4,1,0x3f800000];c[72][1]=[2,[O]]
      c[74][1]=N;c[75][1]=0
      if service in (69,70):c[20][1]=[3,[0,0x78000]]
      if service==71:p += [[R+0x50,4,5],[R+0x4b08,2,0x47],[R+0x4b0a,2,1],[R+0x4b0c,2,0xffff]]
      if service in (72,73,42,44):p += [[P+0x448,4,99],[O+0xc,4,0x20]]
      if service in (77,78):p += [[P+0x38,2,0xffff]]
      if service in (79,62):ops=[next()]
      else:ops=[state()]
      add('failure',p,ops,c)
      c2=[x for x in c if x[0]!=service];add('missing',p,ops,c2)
    for service in (9,45,64,79,62,16,17):
      c=cb();c[service][2]=[[1,P+0x448,4,12],[0,R+0x4a86,1,4]]
      add('callback_mutation',base(0,23)+[[PAD+0xc,4,8]],[state()],c)
    for source in (4,32,36,0xffffffff):
      c=cb();c[36][1]=1;c[39][1]=1;c[43][1]=0
      p=base(0,23)+[[PAD+0xc,4,source if source!=0xffffffff else 0],
        [A,2,ord('n')],[A+0x20,2,1],[A+0x24,1,1],[0x41b174,4,W],
        [W+0x60,4,T],[W+8,4,0],[R+0x4440,2,ord('n')],
        [R+0x4440+0x20,2,1],[R+0x4440+0x24,1,1]]
      add('instant_skill',p,[state(source)],c)
    for t in (21,22,23,40):
      add('selected_heavy',base(0,t)+[[R+0x4a60,2,0],[R+0x4a84,1,0],
        [R+0x4440+0x20,2,0x25],[R+0x4a80,4,4],[PAD+0xc,4,4]])
    for linked in (0,N):
      c=cb();c[63][2]=[[0,P+0x234,4,linked]]
      add('next_ranged_no_target',base(4,60),[next(started=40)],c)
    for service in (67,69):
      c=cb();c[service][2]=[[0,0x4f8440,4,1],[0,R+0x43d8,4,A]]
      add('alive_reread',base()+[[0x4f8420,4,1],[0x4f8440,4,0],
        [R+0x43d8,4,O],[R+0x4c30+0x43d8,4,A]],c=c)
    for cls in (0,2,7):
      for slow in (0,1,59,60,61,0xffffffff):
        for t in (1,16,127,0x7fffffff):
          add('slow_thresholds',base(cls,t)+[[P+0x2fc,4,slow]])
    for service in (9,45,79):
      c=cb();c[service][2]=[[0,P+0x3c8,4,1],[0,P+0x58c,4,2]]
      add('captured_record',base(0,23)+[[PAD+0xc,4,8],[R+0x4c30+0x4b08,2,0xffff]],c=c)
    add('repeated',base(0,6),[state(),[8,0x44fdcc,4,109],state(),[8,PAD+0xc,4,8],state()])
    return out

def ranged_recipes():
    out=[];W=0x70000;D=0x71000;N=0x72000;O=0x73000;T=0x74000;A=0x75000;S=0x76000
    def cb(throw=False):
        c=bindings()+[[i,0,[],0] for i in range(19,88)]
        c[30][1]=[5,[0x3f800000,0xc0000000,0x40400000],[0x3f800000,0,0]] if throw else [1,0x3f800000,0xc0000000,0x40400000]
        c[37][1]=[2,[]];c[72][1]=[2,[]];c[68][1]=[4,0,0];c[70][1]=[4,0,0]
        c[23][1]=1;c[24][1]=7;c[61][1]=0x77000;c[16][1]=0x78000;c[49][1]=0x79000
        return c
    def base(kind=0,t=22):
        return setup()+[[P+0x231,1,0],[P+0x58c,4,0],[P+0x480,1,kind],
          [P+0x488,4,A],[P+0x448,4,(100-t)&0xffffffff],[0x44fdcc,4,100],
          [P+0x38,2,0xffff],[P+0x48,4,T],[T+26*8,2,0xfffc],[T+20*8,2,2],[T+12*8,2,4],
          [R+0x4434,4,W],[R+0x43f8,4,D],[R+0x4410,4,D],[D+4,2,2],
          [W,4,0x7a000],[0x7a000,4,0x7b000],[0x7b000,2,ord('z')],
          [W+0x24,2,0x14],[A+0x20,2,0],[A+0x2c,4,0x40000000],
          [R+0xc0,4,0x42c80000],[R+0x4b08,2,0xffff],[0x41b980,4,8],
          [0x3e9ce8,4,0xabcdef00],[0x3e9cec,4,0x12345678],
          [O+0x10,4,0x41200000],[O+0x14,4,0x41a00000],[O+0x18,4,0x41f00000],
          [O+0x20,4,0x42000000],[O+0xc,4,0x20],[O+0x48,4,T],[N+0xc,4,O],
          [O+0x308,4,0x40000000],[O+0x30c,4,0xc0400000]]
    def op(h=0,a=PAD,b=0,extra=0):return [24,h,a,b&0xffffffff,extra&0xffffffff,R]
    def add(label,p,ops=None,c=None):out.append(('ranged_'+label,[p,cb() if c is None else c,[op()] if ops is None else ops]))
    # Release frame, network halving, attack latching and end/flag gates.
    for kind in (0,1,2,3,5,6,7,8,127,128,255):
      for net in (0,1,128):
        for t in (0,4,5,10,11,14,15,21,22,23,60):
          for copyflag in (0,0x20000):
            add('state_timing',base(kind,t)+[[0x402791,1,net],[P+0xc,4,copyflag],[PAD+0xc,4,8],[P+0x48c,4,0x3f800000]],c=cb(kind==7))
    for slow in (0,1,30,59,60,61,128,255):
      for haste in (0,1,10,0x7fff,0x8000,0xffff):
        for kind in (0,7):
          add('state_speed',base(kind)+[[P+0x2fc,1,slow],[W+0x26,2,haste]],c=cb(kind==7))
    for flag in (0,0x10,0xffff):
      for duration in (0,0x3f800000,0xbf800000,0x7fc00000,0x7f800000):
        add('end_gate',base(t=60)+[[P+0x11a,2,flag],[P+0x48c,4,duration]])
    add('no_weapon',base()+[[R+0x4434,4,0]])
    add('no_ammo',base()+[[R+0x4410,4,0]])
    # Bow target selection, clamps and first accepted object, including copy services.
    for target in (0,1,2):
      for kind in (0,1,2,3,4,5,6,7,127,128,255):
        for copyflag in (0,0x20000):
          for count in (0,1,2,0x7fff,0x8000,0xffff):
            c=cb();c[37][1]=[2,[] if not target else [O,O+0x400]];c[38][1]=[3,[0,1]] if target==2 else 1
            add('shoot_target_count',base(kind)+[[P+0xc,4,copyflag],[D+4,2,count]], [op(2)],c)
    for z in (0xc7c35000,0xc3960000,0,0x43960000,0x47c35000,0x7fc00000):
        c=cb();c[37][1]=[2,[O]];c[38][1]=1
        add('pitch',base()+[[O+0x18,4,z]], [op(2)],c)
    for spawn in ([0,0,0],[0x41200000,0x41a00000,0x42380000]):
        c=cb();c[30][1]=[1,*spawn];c[37][1]=[2,[O]];c[38][1]=1
        add('target_geometry',base()+[[O+0x20,4,0x41800000]],[op(2)],c)
    for ability in (0,A):
      for aid in (0xffff,0,0x2e):
        for forced in (0,A):
          for used in (0,1,3,0x7fffffff,0x80000000,0xffffffff):
            c=cb();c[85][1]=used
            add('skill_ammo',base()+[[P+0x488,4,ability],[P+0x484,4,forced],[A+0x20,2,aid],
                [R+0x4b08,2,0x2e],[R+0x4b0a,2,3],[0x41b170+4*0x2e,4,S],[S+0x60,4,T]], [op(2)],c)
    for cell in (0,1,63):
      for copyflag in (0,0x20000):
        d=R+0x130+cell*0x48
        add('shoot_remove',base()+[[R+0x4410,4,d],[d+4,2,1],[P+0xc,4,copyflag]], [op(2)])
    for count in (0,1,2,20,21,0x7fff,0x8000,0xffff):
      for target in (0,N):
        for flags in (0,0x1000000,0x100000000):
          for copyflag in (0,0x20000):
            add('throw',base(7,11)+[[D+4,2,count],[P+0x234,4,target],[W+8,8,flags],[P+0xc,4,copyflag]], [op(1,W,11)],cb(True))
    for cell in (0,1,63):
        d=R+0x130+cell*0x48
        add('throw_remove',base(7,11)+[[R+0x43f8,4,d],[d+4,2,1]], [op(1,W,11)],cb(True))
    # Both substring exemptions and the prefix veto, with binary strings.
    for hay in (0x536010,0x536020,0):
      for prefix in (0,1):
        c=cb(True);c[64][1]=prefix
        p=base(7,11)+[[W+8,8,0x1000000]]
        if hay:p += [[hay,4,0x7a7a7a]]
        add('throw_make',p,[op(1,W,11)],c)
    for frame in (10,11,12,0x7fffffff,0x80000000,0xffffffff):
        add('throw_frame',base(7,frame),[op(1,W,11)],cb(True))
    for forced in (0,A):
        add('throw_skill',base(7,11)+[[P+0x484,4,forced]],[op(1,W,11)],cb(True))
    # Aiming uses current target after turn; optional scale is deliberately unused.
    for divisor in (0,0x43af0000,0x443b8000,0x7fc00000,0xbf800000):
      for present in (0,1):
        for rank in (0,1,2,127,128,255):
          add('aim_lead',base()+[[P+0x234,4,N],[0x4ef538,8,0x200000],[R+0xef,1,rank]], [op(4,divisor,present,0xdeadbeef)])
    add('aim_none',base(),[op(4,0,0)])
    for object in (P,O,0):
      for seed in (0,1,0xdeadbeef,0xffffffff):
        for present in (0,1):
          add('object_rng',base()+[[object+0x44,4,seed]], [op(5,object,present),op(5,object,present)])
    for hay,needle in ((b'',b''),(b'abc',b'b'),(b'abc',b'd'),(b'ab',b'abc'),(b'\xffab',b'\xff'),(b'x'*256,b'x'*256),(b'x'*256,b'y')):
        p=base()+[[0x80000+i,1,x] for i,x in enumerate(hay)]+[[0x81000+i,1,x] for i,x in enumerate(needle)]
        # Keep transport's existing 512 poke cap for long strings.
        if len(p)>512:p=[[0x80000+i,1,x] for i,x in enumerate(hay)]+[[0x81000,1,ord('x')]]
        add('substring',p,[op(6,0x80000,0x81000)])
    for block in (0,0x79000):
      for facing in (0,0x7fffffff,0x80000000,0xffffffff):
        c=cb();c[49][1]=block
        add('missile',base(),[op(3,P+0x10,O+0x10,facing)],c)
    # Callback rereads: target, slot, kind, stack, and copy gate can change.
    for service,effects in ((62,[[0,P+0x234,4,N]]),(63,[[0,N+0xc,4,O+0x400]]),
        (38,[[0,P+0xc,4,0x20000]]),(84,[[0,P+0x480,1,3]]),
        (87,[[0,P+0x480,1,5]]),(26,[[0,P+0x3c8,4,1]]),
        (81,[[0,D+4,2,1]]),(61,[[0,P+0xc,4,0x20000]])):
        for throw in (False,True):
          c=cb(throw);c[service][2]=effects;c[37][1]=[2,[O]];c[38][1]=1
          add('callback_mutation',base(7 if throw else 0,11)+[[P+0x234,4,N],[D+4,2,1]], [op(1,W,11) if throw else op(2)],c)
    # Every service on both release paths: missing and injected failure prefixes.
    paths=[(base(),op(2),cb(),(30,62,37,83,84,61,23,16,24,49,87,44)),
        (base(3)+[[D+4,2,1]],op(2),cb(),(86,26)),
        (base()+[[P+0x234,4,N]],op(2),cb(),(63,)),
        (base(7,11),op(1,W,11),cb(True),(62,30,81,61,23,16,24,49,82,29)),
        (base(7,11)+[[W+8,8,0x1000000]],op(1,W,11),cb(True),(64,)),
        (base(),op(),cb(),(9,80,45))]
    for p,command,c,services in paths:
      for service in services:
        for fail in (0,1):
          changed=copy.deepcopy(c)
          if fail:changed[service][3]=1;changed[service][2]=[[1,0x88000,4,0xabcdef]]
          else:changed=[x for x in changed if x[0]!=service]
          add('service_failure',p,[command],changed)
    # Additional callbacks that require a target or the skill-0x2e branch.
    c=cb();c[37][1]=[2,[O,O+0x400]];c[38][1]=1
    p=base()+[[P+0x234,4,N]]
    for service in (38,73):
      for fail in (0,1):
        changed=copy.deepcopy(c)
        if fail:changed[service][3]=1;changed[service][2]=[[1,0x88000,4,0xabcdef]]
        else:changed=[x for x in changed if x[0]!=service]
        add('target_service_failure',p,[op(2)],changed)
    p=base()+[[P+0x484,4,A],[A+0x20,2,0x2e],[R+0x4b08,2,0xffff],[0x41b170+4*0x2e,4,S],[S+0x60,4,T]]
    for fail in (0,1):
        c=cb()
        if fail:c[85][3]=1;c[85][2]=[[1,0x88000,4,0xabcdef]]
        else:c=[x for x in c if x[0]!=85]
        add('stat_service_failure',p,[op(2)],c)
    # Independent index calls must retain different table rows and callbacks.
    c=cb(True);c[81][1]=[3,[1,2]]
    add('model_indices',base(7,11)+[[0x3e9ce8+16,4,11],[0x3e9cec+32,4,22]],[op(1,W,11)],c)
    # Removal and HUD callbacks reread slot and count, respectively.
    c=cb(True);c[29][2]=[[0,D+4,2,21]]
    add('notice_count_reread',base(7,11)+[[D+4,2,1]],[op(1,W,11)],c)
    c=cb();c[85][2]=[[0,P+0x3c8,4,1]];c[85][1]=2
    add('stat_slot_reread',p+[[R+0x4c30+0x4434,4,W],[R+0x4c30+0x4410,4,D]],[op(2)],c)
    for throw in (False,True):
        c=cb(throw);c[26][2]=[[0,P+0x3c8,4,1]]
        d=R+0x130
        add('remove_slot_reread',base(7 if throw else 0,11)+[[R+(0x43f8 if throw else 0x4410),4,d],[d+4,2,1]], [op(1,W,11) if throw else op(2)],c)
    c=cb();c[9][2]=[[0,P+0x3c8,4,1]]
    add('state_captured_record',base()+[[R+0x4c30+0x4410,4,D],[R+0x4c30+0x4434,4,W]],[op()],c)
    c=cb();c[80][2]=[[0,P+0x3c8,4,1],[0,0x41b980+4,4,16]]
    add('load_slot_reread',base(t=23)+[[PAD+0xc,4,16]],[op()],c)
    add('repeat',base(),[op(2),op(2),[8,0x44fdcc,4,123],op()])
    return out


def legacy_cast_recipes():
    out=[]; A=0x75000; S=0x76000; V=0x74000; O=0x73000; N=0x72000
    def cb():
        c=bindings()+[[i,0,[],0] for i in range(19,90)]
        c[30][1]=[3,[[1,0x3f800000,0x40000000,0x40400000],[1,0x40a00000,0xc0000000,0x40e00000]]]
        c[37][1]=[2,[]];c[72][1]=[2,[]];c[68][1]=[4,0,0];c[70][1]=[4,0,0]
        c[36][1]=1;c[32][1]=0xffffffff;c[49][1]=0x79000
        c[39][1]=2;c[43][1]=0x40000000;c[16][1]=0x78000
        return c
    def base(kind=0,phase=0,sid=0x19,new=False):
        p=setup()+[[P+0x231,1,0],[P+0x58c,4,5],[P+0x488,4,A],[P+0x47b,1,phase],
          [P+0x24,4,0x7a000],[0x7a000+0x6c,4,0x7b000],[P+0x4ae,2,0],
          [P+0x38,2,0xfffc],[P+0x48,4,V],[V+39*8,2,0xfffc],
          [R+0x43d8,4,O],[R+0x4a80,4,8],[PAD+8,4,8],[R+0xc0,4,0x42c80000],
          [A+0x20,2,sid&0xffff],[A+0x22,2,0xfff1],[A+0x25,1,kind],[A+0x28,2,0xfffb],
          [A+0x40,4,0x3d4ccccc],[P+0x118,1,0],[P+0x11a,2,0x8020],
          [P+0x124,4,0x3f800000],[P+0x12c,4,0x40000000],[P+0x104,4,0x3f800000],
          [R+0x4b08,2,0xffff],[R+0x50,4,0],[0x4ef538,8,0x8000],[R+0xef,1,2],
          [S+0x60,4,V],[V+1*8,2,0xfffc],[V+2*8,2,2],[V+5*8,2,4]]
        for k,v in enumerate((0,0x7fff,0x8000)):p.append([A+0x30+2*k,2,v])
        for k,v in enumerate((0xffff,0x8000,0x7fff,0x1234)):p.append([A+0x38+2*k,2,v])
        if new:p.extend([[0x41b170+4*(sid&0xffff),4,S],[S+4,4,1]])
        else:p.append([A,1,ord('x')])
        return p
    def op(h=0,a=PAD,b=0,extra=0):return [25,h,a,b,extra,R]
    def add(label,p,ops=None,c=None):out.append(('legacycast_'+label,[p,cb() if c is None else c,[op()] if ops is None else ops]))
    # All three legacy kinds plus unsupported signed kind/phase values.
    for kind in (0,1,2,3,127,128,255):
      for phase in (0,1,2,3,127,128,255):
        for copyflag in (0,0x20000):
          add('legacy_kinds',base(kind,phase)+[[P+0xc,4,copyflag]])
    for kind in (0,1,2):
      for phase in (0,1,2,255):
        for flags in (0,0x20,0x8000,0x8020):
          for held in (0,8):
            add('end_gates',base(kind,phase,9)+[[P+0x11a,2,flags],[PAD+8,4,held],[P+0x104,4,0x41200000]])
    # Single-use specials, allocation failure and status-counter toggles.
    for sid in (0x15,0x19,0x1a,0x1b,0x7fff,0xffff):
      for phase in (0,1,2):
        for allocate in (0,0x79000):
          c=cb();c[49][1]=allocate
          add('special',base(0,phase,sid),c=c)
    for counter in (0,1,0x7fffffff,0x80000000,0xffffffff):
      add('unholy_toggle',base(0,1,0x1a)+[[O+0x39c,4,counter]])
    for progress in (0,0x80000000,0x3f7fffff,0x3f800000,0x411fffff,0x41200000,0x7fc01234):
      for phase in (0,1,2):
        for kind in (0,2):
          add('legacy_progress',base(kind,phase)+[[P+0x104,4,progress]])
    # New-style phases use unsigned dispatch but signed byte checks inside.
    for phase in (0,1,2,3,128,255):
      for skind in (0,1,6,0xffffffff):
        for network in (0,1,128):
          for track in (0xffffffff,0,1):
            c=cb();c[32][1]=track
            add('new_phases',base(0,phase,0,new=True)+[[S+8,4,skind],[0x402791,1,network]],c=c)
    for point in (0,0x3effffff,0x3f000000,0x3f7fffff,0x3f800000,0x7fc01234):
      for network in (0,1):
        for weight in (0,0x80000000,0x3f000000,0x40000000,0x7fc01234):
          c=cb();c[32][1]=0
          add('cast_point',base(0,1,0,new=True)+[[P+0x124,4,point],[P+0x12c,4,weight],[0x402791,1,network]],c=c)
    for kind in (0,1,2,255):
      for flags in (0,0x8000):
        for cost in (0,0x80000000,0x3f800001,0x7fc01234):
          c=cb();c[43][1]=cost
          add('held_mana',base(kind,2,0,new=True)+[[P+0x11a,2,flags]],c=c)
    # Ability selection, invalid ids, and captured ability after callbacks.
    for selected in (0,1,2,0xffff):
      for sid in (0,0xffff):
        a=R+0x4440
        add('selected',base()+[[P+0x488,4,0],[R+0x4a60,2,selected],[R+0x4a84,1,0],[R+0x4a85,1,0],[a,1,ord('x')],[a+0x20,2,sid]])
    add('absent',base()+[[A+0x20,2,0xffff]])
    for result in (0,1):
      c=cb();c[36][1]=result;c[36][2]=[[0,P+0x47b,1,2]]
      add('can_use_reread',base(1,0,0,new=True),c=c)
    for service,effects in ((9,[[0,P+0x488,4,0],[0,P+0x47b,1,1]]),
                            (32,[[0,P+0x47b,1,1],[0,0x402791,1,1]]),
                            (36,[[0,S+8,4,6]]),
                            (43,[[0,P+0x3c8,4,1]])):
      c=cb();c[service][2]=effects
      add('new_callback_reread',base(1,2 if service==43 else 0,0,new=True),c=c)
    for service,effects in ((30,[[0,P+0x10,4,0x41200000]]),
                            (17,[[0,P+0x3c8,4,1],[0,R+0x4c30+0x4a80,4,8]]),
                            (46,[[0,P+0x3c8,4,1],[0,P+0x118,1,1]]),
                            (49,[[0,P+0x18,4,0x41a00000],[0,P+0x3c8,4,1]])):
      c=cb();c[service][2]=effects
      add('legacy_callback_reread',base(1 if service in (17,46) else 0,0 if service==17 else 1,0x1b if service==49 else 9)+[[PAD+8,4,0]],c=c)
    # Every reached service's success and retained missing/injected prefixes.
    scenarios=[base(0,1,0x19),base(0,1,0x15),base(0,1,0x1a),base(0,0,0x1b),
               base(2,0)+[[P+0x104,4,0x41200000]],base(1,0)+[[PAD+8,4,0]],
               base(1,2,0,new=True),base(0,0,0,new=True),base(0,1,0,new=True)]
    for p in scenarios:
      for service in (9,45,30,36,32,17,18,16,24,46,49,50,58,59,53,88,89,39,43,31,40,44):
        for fail in (0,1):
          c=cb()
          if fail:c[service][3]=1;c[service][2]=[[1,0x88000,4,0xabcdef]]
          else:c=[x for x in c if x[0]!=service]
          add('service_failure',p,c=c)
    # Independent ring body: all 219 angles, lazy definition and passive damage.
    for level in (0,1,2,10,127,128,255):
      for built in (0,1):
        add('nova',base()+[[0x4ef3f8,4,built],[R+0xef,1,level]],[op(3,A)])
    c=cb();c[59][2]=[[0,P+0x10,4,0x41200000],[0,P+0x3c8,4,1]]
    add('nova_rereads',base(),[op(3,A)],c)
    add('nova_repeat',base(),[op(3,A),op(3,A)])
    for kind in (0,1,2,255):
      for phase in (0,1,2,255):
        add('direct_old',base(kind,phase),[op(1,A,PAD)])
    add('direct_absent',base(),[op(1,0,PAD)])
    for count in (0,1,4,8,9):
      for allocate in (0,0x79000):
        c=cb();c[49][1]=allocate
        add('light_args',base()+[[N+4*k,4,(k*0x12345678)&0xffffffff] for k in range(count)], [op(2,0x42100000,N,count)],c)
    # Binary and bounded (256-byte) strcmp semantics.
    for x,y in ((b'',b''),(b'a',b'a'),(b'a',b'b'),(b'\xff',b'\xfe'),(b'abc\0x',b'abc\0y'),(b'x'*256+b'a',b'x'*256+b'b')):
      p=[[N+k,1,v] for k,v in enumerate(x)]+[[O+k,1,v] for k,v in enumerate(y)]
      # Keep public transport's bounded 512 poke limit.
      if len(p)>512:p=p[:256]+[[O+k,1,v] for k,v in enumerate(y[:256])]
      add('strcmp',p,[op(4,N,O)])
    # Dispatcher exercises every documented state and unknown no-op states;
    # delegated services stay supplied synthetic callbacks.
    for state in (*range(15),15,0xffffffff):
      p=base()+[[P+0x58c,4,state]]
      add('dispatch',p,[op(5)])
    add('sequence',base(1),[op(),[8,PAD+8,4,0],op(),[8,P+0x58c,4,15],op(5)])
    return out

def preswitch_recipes(modules):
    out=[]
    A,S,T,V,W,D=0x75000,0x76000,0x77000,0x78000,0x79000,0x7a000
    def cb():
        c=[[i,0,[],0] for i in range(94)]
        c[15][1]=1;c[36][1]=1;c[39][1]=2;c[43][1]=0x3f800000
        return c
    def base(sl=0):
        r=R+sl*0x4c30;pd=PAD+sl*0x58
        return setup(sl)+[[P+0x231,1,0],[P+0x58c,4,2],[P+0x488,4,0],[P+0x4ae,2,0],
          [r+0x50,4,1],[r+0xc0,4,0x41200000],[r+0xc4,4,0x42c80000],
          [r+0x4b08,2,0xffff],[0x537c08,4,0xffffffff],[0x4f7188,4,0xc7afc800],
          [0x41b990+sl*4,4,1],[0x41b9a0+sl*4,4,2],[0x41b9d0+sl*4,4,4],
          [0x41ba10+sl*4,4,8],[0x41ba20+sl*4,4,16],[pd+8,4,0],[pd+0xc,4,0],
          [r+0x4a84,1,0],[r+0x4a85,1,1],[A+0x25,1,7],[A+0x20,2,3],
          [0x41b170+12,4,S],[S+0x60,4,T],[P+0x48,4,V],
          [T+8,2,0xfffc],[T+40,2,8],[V+39*8,2,0xfff8]]
    def ability(a):
        # Authored name; no original string or asset.
        return [[a,4,0x007a7978],[0x535e30,4,0x007a7978],[a+0x20,2,3],[a+0x25,1,7]]
    def add(label,p,ops=None,c=None):
        out.append(('preswitch_'+label,[p,cb() if c is None else c,[[26],[28]] if ops is None else ops]))
    for sl in range(5):
      for flags in (0,8,0x20000,0x20008,0x400000):
        for state in (0,2,5,11,12,14,0xffffffff):
          add('slot_flags_state',base(sl)+[[P+0xc,4,flags],[P+0x58c,4,state]])
    for scripted in (1,128,255):
      for state in (0,1,2,15):
        add('scripted',base()+[[P+0x231,1,scripted],[P+0x58c,4,state]])
    # Fade clamp, signed aura level and wrapping arithmetic.
    for fade in (0,0x80000000,0x3f000000,0x3f800000,0xbf800000,0x7fc01234):
      for rate in (0,0x3f800000,0xbf800000,0x7f7fffff):
        for level,direction in ((0,1),(79,1),(0,0xffffffff),(0x7fffffff,0x7fffffff),(0x80000000,0x80000000)):
          add('aura',base()+[[0x4d8aa0,4,fade],[0x4d8a90,4,rate],[0x4d8a40,4,level],[0x4d8ab0,4,direction]])
    for draw_flag in (0,1,128):
      for weapon_kind in (0,1,4,255):
        for index in (0,1,255):
          add('weapon',base()+[[P+0x464,1,draw_flag],[R+0x4a7c,1,index],
              [R+0x4434+(index if index<128 else index-256)*4,4,W if weapon_kind else 0],
              [W,4,D],[D+0x1e,1,weapon_kind]])
    for gate in range(12):
      p=base();c=cb()
      if gate==0:p.append([0x41b4fc,4,1])
      elif gate==1:c[13][1]=1
      elif gate==2:p.append([P+0x4b4,2,2])
      elif gate==3:p.append([0x41b51d,1,1])
      elif gate==4:p.append([0x4f7114,4,1])
      elif gate==5:c[4][1]=1
      elif gate==6:c[14][1]=1
      elif gate==7:c[15][1]=0
      elif gate==8:p.append([P+0xc,4,0x400000])
      elif gate==9:p.append([0x503890,4,1])
      elif gate==10:p.append([0x4f66c8,1,1])
      else:p.append([R+0xac,4,0])
      add('pad_gate',p,c=c)
    # Dialog is queried independently before turn and interaction.
    for target in (0,1,0x7fff,0x8000,0xffff,0xffffffff):
      for current in (0,0x7fff,0x8000,0xffff):
        c=cb();c[13][1]=[3,[0,1,1]];c[91][1]=target
        add('dialog_turn',base()+[[P+0x38,2,current]],c=c)
    for cls in (0,1):
      for state in (2,12):
        for mana in (0,0x80000000,0x3a000000,0x41200000,0xbf800000,0x7fc01234):
          add('status_mana',base()+[[R+0x50,4,cls],[P+0x58c,4,state],[R+0xc0,4,mana],[P+0x374,4,1]])
    for plane in (0xc7afc800,0xc7afc7ff,0,0x40800000,0x7fc01234):
      for invuln in (0,1):
        for flags in (0,8):
          add('kill_plane',base()+[[0x4f7188,4,plane],[0x44f20c,4,invuln],[P+0xc,4,flags],[P+0x2dc,4,123]])
    for gate in range(7):
      p=base();c=cb()
      if gate==0:p.append([0x41b4f4,4,1])
      elif gate==1:p.append([0x41b4fc,4,1])
      elif gate==2:c[13][1]=[3,[0,0,1]]
      elif gate==3:c[4][1]=[3,[0,0,1]]
      elif gate==4:p.append([P+0xc,4,0x20000])
      elif gate==5:p.append([0x41b818,4,1])
      add('interaction',p,c=c)
    for pressed in range(8):
      for selected in (0,1,2,4):
        for flags in (0,0x20000):
          p=base()+[[PAD+0xc,4,pressed],[R+0x4a80,4,selected],[P+0xc,4,flags]]
          add('ability_buttons',p)
    for pressed in (8,16,24):
      for in_pit in (False,True):
        for full in (False,True):
          # Synthetic bag cell matches all supplied zero name pointers.
          p=base()+[[PAD+0xc,4,pressed],[R+0x3740,4,1],[R+0x2544,2,2],
            [R+0xac,4,0x42c80000 if full else 0x41a00000],
            [R+0xc0,4,0x42c80000 if full else 0x41200000],
            [0x535ed8,2,0x71],[0x41b721,2,0x71 if in_pit else 0x72]]
          add('potions',p)
    for held in (0,4,0xf000):
      for no_combat in (0,0x10000000):
        for equipped in (False,True):
          add('block',base()+[[PAD+8,4,held],[0x44fe08,4,no_combat],[R+0x43fc,4,W if equipped else 0]])
    for state in (2,3,8,11):
      for blocked in (False,True):
        p=base()+[[P+0x58c,4,state],[PAD+0xc,4,32],[0x41ba30,4,32],
          [PAD+8,4,4 if blocked else 0],[R+0x43fc,4,W if blocked else 0],
          [R+0x4438,4,W],[W,4,D],[D+0x1e,1,1]]
        add('swap',p)
    for count in (0,4,5,127,255):
      for moving in (False,True):
        add('fidget',base()+[[P+0x118,1,count],[P+0x463,1,1],
          [PAD+0x14,4,0x3f800000 if moving else 0]])
    # Integrated natural/pending recovery and modifier expiry before damage.
    add('recovery_expiry',base()+[[R+0xb4,4,0x3f800000],[R+0xb8,4,0x3f000000],
      [R+0xc8,4,0x3f800000],[R+0xbc,4,0x3f000000],[R+0x43f0,4,W],
      [W,4,D],[W+0x2a,2,1],[0x537c08,4,11],[0x537c0c,4,0xffffffff]])
    # Instant family independently and through complete pre-switch.
    for forced in (False,True):
      for i in (0,1):
        for kind in (0,7,255):
          for held in (0,0xf000):
            for recast in (0,1,0xffff):
              a=A if forced else R+0x4440+i*0x68
              p=base()+ability(a)+[[P+0x488,4,a if forced else 0],[a+0x25,1,kind],
                [PAD+0xc,4,1<<i],[PAD+8,4,held],[P+0x4ae,2,recast]]
              add('instant_gates',p,[[27,PAD],[28]])
    for cost in (0,0x80000000,0x3f800000,0x41200000,0x41200001,0xbf800000,0x7fc01234):
      for accepted in (0,1):
        for forced in (False,True):
          a=A if forced else R+0x4440;c=cb();c[36][1]=accepted;c[43][1]=cost
          p=base()+ability(a)+[[P+0x488,4,a if forced else 0],[PAD+0xc,4,1]]
          add('instant_cost',p,[[27,PAD],[28]],c)
          add('top_instant',p,c=c)
    # A non-instant first button falls through; old-style instant returns instead.
    for first_kind in (0,7):
      p=base()+ability(R+0x4440+0x68)+[[R+0x4440,4,0x0071],[R+0x4440+0x25,1,first_kind],[PAD+0xc,4,3]]
      add('instant_order',p,[[27,PAD],[28]])
    for service,effects in ((36,[[0,P+0x488,4,0]]),(39,[[0,A+0x20,2,4]]),
                            (43,[[0,P+0x3c8,4,1],[0,R+0x4c30+0xc0,4,0x42c80000],[0,0x41b990+4,4,32]])):
      c=cb();c[service][2]=effects
      add('instant_reread',base()+ability(A)+[[P+0x488,4,A]],[[27,PAD],[28]],c)
    # Callbacks mutate slot/current records; R0 remains captured at entry.
    for service in (0,1,2,3,90,13,92,93,19,20,22,34,35):
      c=cb();c[service][2]=[[0,P+0x3c8,4,1],[0,R+0x4c30+0xac,4,0x41a00000],
                          [0,R+0x4c30+0xc0,4,0x41200000],[0,R+0x4c30+0x4b08,2,0xffff]]
      add('top_reread',base()+[[R+0x4430,1,0]],c=c)
    # A rich normal input reaches potion, instant and block query paths.
    scenarios=[base(),base()+ability(A)+[[P+0x488,4,A]],
      base()+[[PAD+0xc,4,24],[R+0x3740,4,1],[R+0x2544,2,2]],
      base()+[[PAD+8,4,4],[R+0x43fc,4,W]],base()+[[P+0x231,1,1]],
      base()+[[0x4f7188,4,0x40800000]],
      base()+[[PAD+0xc,4,32],[0x41ba30,4,32],[R+0x4438,4,W],[W,4,D]]]
    # Discover reached services solely from pinned Python observations, then
    # generate a missing-handler and a synchronous-effect failure for each.
    for p in scenarios:
      c=cb();ob=run(modules,0,[p,c,[[26],[28]]])
      reached={call[0] for call in ob[1][0][2][3]}
      for service in sorted(reached):
        for fail in (False,True):
          cc=cb()
          if fail:cc[service][3]=1;cc[service][2]=[[1,0x88000,4,0xabcdef]]
          else:cc=[row for row in cc if row[0]!=service]
          add('service_failure',p,c=cc)
    add('repeat',base(),[[26],[28],[26],[28]])
    return out

def tick_recipes(modules):
    """Complete tail and full orchestration, solely authored synthetic state."""
    out=[]
    def cb():
        c=[[i,0,[],0] for i in range(95)]
        c[15][1]=1;c[32][1]=0xffffffff
        c[30][1]=[1,0x3f800000,0x40000000,0x42800000]
        for i in (37,72):c[i][1]=[2,[]]
        for i in (68,70):c[i][1]=[4,0,0]
        return c
    def base(sl=0):
        r=R+sl*0x4c30
        return setup(sl)+[[P+0x231,1,0],[P+0x58c,4,2],[P+0x488,4,0],
          [P+0x4a8,4,0xffffffff],[r+0x4b08,2,0xffff],
          [r+0x4a60,2,0],[r+0x4a84,1,0],[r+0x4a85,1,1],
          [r+0x4440,2,ord('x')],[0x536250,2,ord('y')],
          [0x537c08,4,0xffffffff],[0x4f7188,4,0xc7afc800],
          [0x4d8aa0+4*sl,4,0x3f000000],[0x4d8a90+4*sl,4,0],
          [r+0x4bc8,1,1],[r+0x4bc9,1,255],[r+0x4bca,2,65535],
          [P+0xc,4,0x400000],[r+0xc4,4,0x42c80000],
          [0x8659a0+sl*16,4,0xffffffff],[0x8659a4+sl*16,4,0x7fffffff],
          [0x8659a8+sl*16,4,0x80000000],[0x8659ac+sl*16,4,255]]
    def add(label,p,ops=None,c=None):
        out.append(('tick_'+label,[p,cb() if c is None else c,[[29,PAD],[28]] if ops is None else ops]))
    for sl in range(5):
      for fade in (0,0x80000000,1,0x3f000000,0x3f800000,0x40000000,0xbf800000,0x7f800000,0xff800000,0x7fc01234):
        for frame in (0,1,5,11,65535,0x80000000,0xffffffff):
          add('aura_wave',base(sl)+[[0x4d8aa0+sl*4,4,fade],[0x44fdcc,4,frame]])
    for match in (False,True):
      for recall in (0,1,128,255):
        for angle in (0,1,0x3fff,0x4000,0x7fff,0x8000,0xbfff,0xffff):
          add('position',base()+[[0x536250,2,ord('x') if match else ord('y')],
              [0x41b51d,1,recall],[P+0x38,2,angle]])
    for selected in (0,1,2,0xffff,0x8000):
      for index in (0,1,127,128,255):
        add('ability_selector',base()+[[R+0x4a60,2,selected],[R+0x4a84,1,index],[R+0x4a85,1,index]])
    for flags in (0,0x20000,0x400000,0x420000):
      for state in (0,2,5,14,0xffffffff):
        for count in (0,1,2,127,128,255):
          add('countdown',base()+[[P+0xc,4,flags],[P+0x58c,4,state],[P+0x488,4,0x75000],
              [R+0x4bc8,1,count],[R+0x4bc9,1,count],[R+0x4bca,2,count*257]],[[29,PAD],[29,PAD]])
    for word in (0,1,0xffff,0x10000,0x7fffffff,0x80000000,0xffffffff):
      for fade in (1,0x3eaaaaab,0x3f000000,0x3f800000,0x40000000,0x7f800000):
        add('colours',base()+[[0x8659a0+4*k,4,word] for k in range(4)]+[[0x4d8aa0,4,fade]])
    for health in (0,0x80000000,0x3f800000,0xbf800000,0x7fc01234):
      for held in (0,4):
        for state in (2,5):
          p=base()+[[R+0xac,4,health],[PAD+8,4,held],[R+0x4a80,4,4],[P+0x58c,4,state]]
          c=cb();c[32][1]=0;c[30][1]=[3,[[1,1,2,0],[1,3,4,0x43000000]]]
          add('update_cast',p,c=c)
    # Every synchronous callback read boundary, including slot/selector/FPU rereads.
    for service in (94,2,30,31,32,33):
      for target,value in ((P+0x3c8,1),(R+0x4a60,2),(0x4d8aa0,0),
                           (P+0x58c,5),(P+0x488,0x75000),(P+0xc,0x20000)):
        c=cb();c[service][2]=[[0,target,4 if target!=R+0x4a60 else 2,value],
            [0,R+0x4c30+0xac,4,0x3f800000],[0,R+0x4c30+0x4b08,2,0xffff],
            [0,0x4d8aa4,4,0x3f800000]]
        p=base()+[[P+0x588,4,0x77000],[0x7700c,4,0x78000]]
        add('tail_reread',p,c=c)
    # Full ticks across every dispatch mapping, scripted exit and both ordinary Leaves.
    for sl in range(5):
      for state in (*range(15),15,0xffffffff):
        for flags in (0,8,0x20000):
          p=base(sl)+[[P+0x58c,4,state],[P+0xc,4,flags],[P+0x11a,2,8],
            [P+0x118,1,4],[P+0x119,1,4],[R+sl*0x4c30+0x50,4,1],
            [0x5362a0,8,0x4066800000000000]]
          add('full_states',p,[[30],[28],[30],[28]])
    for scripted in (1,128,255):
      for state in (*range(15),0xffffffff):
        c=cb();c[9][1]=1
        add('full_scripted',base()+[[P+0x231,1,scripted],[P+0x58c,4,state]],[[30],[28]],c)
    add('full_recall_leave',base()+[[P+0x58c,4,11],[0x41b51d,1,1]],[[30],[28]])
    add('full_ability_leave',base()+[[P+0x58c,4,15],[R+0x4a60,2,2]],[[30],[28]])
    # Reuse ALL pre-switch input scenarios in a full tick, preserving their original
    # recipe bindings separately. Fill only missing state/tail services here.
    for label,(p,c,ops) in preswitch_recipes(modules):
      c=copy.deepcopy(c);bound={row[0] for row in c}
      c.extend(row for row in cb() if row[0] not in bound)
      for row in c:
        if row[0] in (30,37,68,70,72):row[1]=cb()[row[0]][1]
      add('full_'+label,p,[[30],[28]],c)
    # Failure prefixes for every reached service in representative complete paths.
    scenarios=[base()]+[base()+[[P+0x58c,4,i],[P+0x118,1,4],[P+0x11a,2,8]] for i in range(15)]
    for p in scenarios:
      observed=run(modules,0,[p,cb(),[[30]]])
      reached={call[0] for call in observed[1][0][2][3]}
      for service in sorted(reached):
        for fail in (False,True):
          c=cb()
          if fail:c[service][3]=1;c[service][2]=[[1,0x88000,4,0xabcdef]]
          else:c=[row for row in c if row[0]!=service]
          add('full_failure',p,[[30],[28]],c)
    return out

def utility_recipes():
    """Width and binary string bodies; no callback or gameplay input is needed."""
    out=[]
    def add(label,pokes,ops):out.append(('utility_'+label,[pokes,[],ops]))
    for byte in range(256):
        # Exercise every signed-byte result and irrelevant high bits.
        add('s8',[],[[31,0,byte,0],[31,0,byte+0x1234567800,0],
                     [31,0,0xffffffffffffff00+byte,0]])
    values=(0,1,0x7f,0x80,0xff,0x100,0x7fff,0x8000,0xffff,0x10000,
            0x7fffffff,0x80000000,0xffffffff,0x100000000,
            0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff)
    for value in values:
        for offset in range(8):
            a=0x90000+offset
            add('read',[[a,8,value]],[[31,helper,a,0] for helper in (1,2,3)])
            for helper in (4,5,6):
                add('write',[[a,8,0x8877665544332211]],
                    [[31,helper,a,value]]+[[31,h,a,0] for h in (1,2,3)])
    for value in (-1,-128,-256,-32768,-65536,-2147483648,-4294967296,-9223372036854775808):
        add('signed_write',[],[[31,h,0x90003,value] for h in (4,5,6)]+
                               [[31,h,0x90003,0] for h in (1,2,3)])
    # Missing bytes remain zero, and overlapping stores produce complete histories.
    add('sparse',[[0x90001,1,0xff],[0x90005,1,0x80]],
        [[31,h,0x90000,0] for h in (1,2,3)]+
        [[31,6,0x90002,0x12345678],[31,4,0x90003,-1],[31,5,0x90005,0x8001],
         [31,3,0x90000,0]])
    for helper,width in ((1,1),(2,2),(3,8)):
        add('edge_read',[[0x100000000-width,width,0xffffffffffffffff]],
            [[31,helper,0x100000000-width,0]])
    for helper,width in ((4,1),(5,2),(6,4)):
        add('edge_write',[],[[31,helper,0x100000000-width,-1],
                            [31,1 if width==1 else 2,0x100000000-width,0]])
    data=bytes(range(1,256))*2
    pokes=[[0x90000+i,1,b] for i,b in enumerate(data)]
    for limit in (-9223372036854775808,-1,0,1,2,127,255,256,257,509,510,511,1024,9223372036854775807):
        add('string_limit',pokes,[[31,7,0x90000,limit]])
    for position in (0,1,127,255,256,257,509):
        add('string_nul',pokes+[[0x90000+position,1,0]],
            [[31,7,0x90000,position],[31,7,0x90000,position+1],[31,7,0x90000,1024]])
    add('string_empty',[],[[31,7,0,256],[31,7,0xffffffff,256],
                          [31,7,0xffffffffffffffff,0],[31,7,0xffffffffffffffff,-1]])
    add('string_edge',[[0xffffffff,1,0xff]],[[31,7,0xffffffff,1]])
    return out

def memory_recipes():
    rows=[]
    def add(label,pokes,commands,under):
        rows.append(('memory_'+label,[pokes,[],commands,under]))
    # Every sparse-presence pattern, retaining zero as a present byte.
    for width in range(9):
        for mask in range(1 << width):
            a=0x90001
            pokes=[[a+i,1,(i*37)&255] for i in range(width) if mask & (1 << i)]
            for salt in (0,193):
                add('mask',pokes,[[32,a,width],[32,a,width]], [salt,0,[]])
    for width in range(9):
        a=0x100000000-width if width else 0xffffffff
        add('edge',[],[[32,a,width]], [255,0,[]])
        add('zero',[],[[32,a,width]], [])
    for data in ([],[0],[0,255,128,1],list(range(256))):
        for under in ([],[7,0,[]]):
            add('bytes',[[0x90000,8,0xffffffffffffffff]],
                [[33,0x90002,data],[32,0x90000,8],[33,0x90002,list(reversed(data))],[32,0x90000,8]],under)
    # The delegate changes both a present byte and a later missing byte. The
    # current read must still use the pre-call presence/value snapshot.
    for fail in (0,1,2,3):
        add('mutation',[[0x90001,1,0x11],[0x90003,1,0x33]],
            [[32,0x90000,8],[32,0x90000,8]],
            [73,fail,[[0x90001,1,0xee],[0x90006,1,0xab]]])
        add('whole_failure',[],[[32,0x90000,8],[32,0x90000,8]],
            [73,fail,[[0x90001,1,0xee],[0x90006,1,0xab]]])
    add('tick',[],[[31,3,0x90000,0],[31,5,0x90002,0x1234],[31,3,0x90000,0]], [21,0,[]])
    add('empty',[],[[33,0xffffffffffffffff,[]],[32,0,0]], [0,0,[]])
    return rows

def callback_recipes(modules):
    rows=[]
    key=lambda name:list(name.encode('latin1'))
    names=list(modules[1].Tick.HELPERS)
    def add(label,pokes,commands,external=(),callbacks=()):
        rows.append(('generic_'+label,[pokes,[],commands,[],[[key(v) for v in external],list(callbacks)]]))
    arguments=[[0,-1],[0,0x100000001],[0,0xffffffffffffffff],[1,0x80000000,0x7fffffff,0x3f800000],[2,0],[3,0]]
    for name in ('synthetic','Player.v987','', 'binary'+chr(255), 'Creature_run'):
        for value in (0,0xffffffff,[6,-1],[6,-2147483648],[7,0xffffffffffffffff],[8],[1,1,2,3],[2,[0,7,0xffffffff]],[4,1,0x3f800000],[5,[1,2,3],[4,5,6]]):
            for fail in (0,1):
                add('call',[],[[34,key(name),arguments],[34,key(name),[]]],
                    callbacks=[[key(name),value,[[1,0x90000,4,123],[0,0x90004,1,77]],fail]])
        add('missing',[],[[34,key(name),arguments]])
    for value in (0,7,[6,-1],[8],[1,1,2,3]):
        add('potion_return',[],[[39,0],[39,1]], external=['Player_usePotion'],
            callbacks=[[key('Player_usePotion'),value,[],0]])
    for slot in (0,1,17,39,987):
        for adjustment in (0,1,32767,32768,65535):
            for obj in (0x10000,1,0xffffff00):
                add('virtual',[[obj+0x14,4,0x90000],[0x90000+8*slot,2,adjustment]],
                    [[35,key('Authored'),obj,0x14,slot,arguments]],
                    callbacks=[[key('Authored.v'+str(slot)),[1,1,2,3],[],0]])
    for name in names+['arbitrary_helper']:
        for fail in (0,1):
            for selected in (False,True):
                for present in (False,True):
                    add('helper',[],[[36,key(name),[[],[4,1,9],[[1,0x90000,1,3]],fail],arguments],
                                      [37,[]], [36,key(name),[[],7,[[0,0x90001,1,4]],0],arguments]],
                        external=[name] if selected else [],
                        callbacks=[[key(name),[1,5,6,7],[[1,0x90002,1,8]],fail]] if present else [])
    # Exercise helper selection in every existing gameplay recipe, preserving
    # original services. Delegated helpers return zero and record their exact args.
    corpus=recipes()+recovery_recipes()+casting_recipes()+action_recipes()+neutral_recipes()+melee_recipes()+ranged_recipes()+legacy_cast_recipes()+preswitch_recipes(modules)+tick_recipes(modules)
    seen=set();reached=set()
    for label,inputs in corpus:
        candidate=copy.deepcopy(inputs)+[[],[[key(v) for v in names],[[key(v),0,[],0] for v in names]]]
        result=run(modules,0,candidate)
        # Deduplicate by helper call sequence and command family; retain all
        # distinct routes (including early refusals) rather than sampled labels.
        calls=[c for c in result[1][-1][2][3] if isinstance(c[0],list)] if result[1] else []
        reached.update(bytes(c[0]).decode("latin1") for c in calls)
        signature=packed([inputs[2],calls])
        if calls and signature not in seen:
            seen.add(signature);rows.append(('generic_route_'+label,candidate))
            for value in (1,0xffffffff,[6,-1]):
                alternative=copy.deepcopy(candidate)
                for binding in alternative[4][1]:binding[1]=value
                rows.append(('generic_route_value_'+label,alternative))
            failure=copy.deepcopy(candidate)
            for binding in failure[4][1]:binding[2:]=[[[1,0x99000,1,77]],1]
            rows.append(('generic_route_failure_'+label,failure))
    assert reached==set(names), ('unexercised helper routes',set(names)-reached)
    return rows

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/player.tsv.gz'))
    args=parser.parse_args();modules=load(args.research_dir)
    rows=[];counts={}
    new_callbacks=callback_recipes(modules)
    for config in range(16):
        for label,inputs in recipes()+recovery_recipes()+casting_recipes()+action_recipes()+neutral_recipes()+melee_recipes()+ranged_recipes()+legacy_cast_recipes()+preswitch_recipes(modules)+tick_recipes(modules)+utility_recipes()+memory_recipes()+new_callbacks:
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
        'protocol':'row=[label,config,[pokes,callbacks,commands],[initial,steps]]; step=[status,result,snapshot]; snapshot=[bytes,stores,writes,calls,events,shared_draws,R0]; memory/callback rows append underlying=[] or [salt,fail_call,pokes] to input, and delegate_calls=[[address,width],...] to snapshot; callback rows append [external_names,named_bindings] as field five, names are byte arrays, generic call keys use byte arrays, commands 34/35/36/37/39 are named call/virtual/helper/selection/potion result',
        'statuses':{'0':'success','1':'ordinary boundary refusal','2':'missing handler','4':'injected callback exception','5':'reference Leave'},
        'scope':'complete generic callback constructor, named call, virtual dispatch and selectable helper externalization; complete Memory constructor, underlying load delegate and unlogged byte writes; complete scalar-width access and configurable binary string reads; complete ordinary tail and full tick orchestration; complete pre-switch top and instant abilities; complete casting state and legacy spell effects, action-state switch and strcmp; complete ranged/projectile state, target lead, arbitrary-object RNG and substring lookup; complete melee/combo state and third RNG; complete neutral state and ability/attack/old-arrow subpaths; bounded action states hit/dead/jump/rush/block-move/warcry/vehicle/pound/skill/block and shared helpers; entry through scripted return; choose_pad; idle_fidget; can_block; try_block_move; has_anim; skill_rank; passive_level; regen_health; regen_mana; tick_enchant_timers; use_potion_real; use_potion; update_cast; weapon_swap_input; hands_midpoint; skill_of; cast_skill'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(compressed)
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(packed({'count':len(rows),'expanded_bytes':len(data),'sha256':manifest['vector_sha256']}))
if __name__=='__main__':main()
