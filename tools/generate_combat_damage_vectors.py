#!/usr/bin/env python3
"""Synthetic RTA damage vectors; imports only strictly pinned external research.
No executable, retail resource, or host floating-point gameplay calculation.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import random
import subprocess
import sys
import types
PIN='7212ecfecbeae8dd2015f6c82897226f49a94695'
SHA='b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'


def encode(value):
    if value is None:return 'n'
    if isinstance(value,bool):return 'b'+str(int(value))
    if isinstance(value,str):return 's'+value.encode('utf-8').hex()
    if isinstance(value,(list,tuple)):return '['+','.join(map(encode,value))+']'
    return str(value)


class CallbackError(Exception):pass


def run(r, op, parameters, setup):
    w=r.World();objects=[r.Obj(tag='v',flags=32,hp=0x42c80000,max_hp=0x42c80000),r.Obj(tag='a',flags=96,hp=0x42c80000,max_hp=0x42c80000,slot=1,pos=[0x3f800000,0,0]),r.Obj(tag='o',flags=96,slot=2)]
    v,a,o=objects
    w.players=[r.PlayerRec(obj=x,hp=0x42480000,max_hp=0x42c80000) for x in objects]
    w.alive=[0,1,2]
    stats=[0,0,0]; mode=0; fail_at=0; pool_ok=1; enemy=1;calls=0
    def obj(index):return None if index<0 else objects[index]
    for command in setup.split(';'):
        if not command:continue
        key,value=command.split('=',1)
        if key=='spell':
            p=value.split(':');target=int(p[0]);spell=r.Spell(kind=p[1],owner=obj(int(p[2])),skill=int(p[3]),level=int(p[4]),f18=int(p[5]),f1c=int(p[6]),f20=int(p[7]),f2c=int(p[8]));objects[target].spells.append(spell)
        elif key=='skill':
            slot,skill,rank=map(int,value.split(','));w.players[slot].skills.append((skill,rank))
        elif key=='seed':w.seed.state=int(value)
        elif key=='cosmetic':w.cosmetic.state=int(value)
        elif key=='stats':stats=list(map(int,value.split(',')))
        elif key=='mode':mode=int(value)
        elif key=='fail_at':fail_at=int(value)
        elif key=='pool_ok':pool_ok=int(value)
        elif key=='enemy':enemy=int(value)
        else:
            target,field=key.split('.')
            x=w if target=='w' else w.players[int(target[1:])] if target.startswith('p') else objects['vao'.index(target)]
            old=getattr(x,field)
            if isinstance(old,list):new=[] if value=='-' else list(map(int,value.split(',')))
            elif field in ('owner','status_source'):new=obj(int(value))
            elif isinstance(old,str):new=value
            elif isinstance(old,bool):new=bool(int(value))
            else:new=int(value)
            setattr(x,field,new)
    def callback(name,*args):
        nonlocal calls
        calls+=1;w.log('cb_'+name,*args)
        if mode==1:
            v.armor=-123;v.res[0]=0x3f000000;v.hp=0x41200000;a.kind=0x10
        if mode==2:w.seed.draw();w.cosmetic.draw()
        if fail_at==calls:raise CallbackError()
    def stat(skill,stat,level):callback('stat',skill,stat,level);return stats[stat%3]
    def stat_for(skill,stat,level,caster,cap):callback('stat_for',skill,stat,level,caster.tag if caster else None,cap);return stats[stat%3]
    def adjust(skill,level,n):callback('adjust',skill,level,n);return level
    def pool(size):callback('pool',size);return pool_ok
    def is_enemy(x,y):callback('enemy',x.tag,y.tag);return enemy
    def take(x,amount,source,flags):
        callback('take',x.tag,amount,source.tag if source else None,flags)
        if mode==3:
            x.hp=r.fsub(x.hp,amount);x.flags|=8
            w.players[1].hp=r.fsub(w.players[1].hp,0x3f800000)
    w.stat_fn=stat;w.stat_for_fn=stat_for;w.adjust_fn=adjust;w.pool_fn=pool;w.enemy_fn=is_enemy;w.take_damage_fn=take
    p=parameters
    try:
        if op=='base_roll':result=r.base_roll(*p)
        elif op=='proc_amount':result=r._proc_amount(w,*p)
        elif op=='status':result=r.set_status(w,v,p[0],p[1],obj(p[2]))
        elif op=='fx':result=r._fx(w,p[0],v)
        elif op=='bloodlust':result=r.bloodlust_armor_factor(w,v.spells[0])
        elif op=='spirit':result=r.spirit_shield_absorbs(w,v.spells[0])
        elif op=='frost':result=r.frost_armor_on_hit(w,v.spells[0],p[0],obj(p[1]))
        elif op=='thorns':result=r.thorns(w,p[0],v,a,p[1])
        elif op=='crit':result=r.crit_bonus(w,p[0],a,p[1])
        elif op=='stun_chance':result=r.stun_chance(w,v,p[0])
        elif op=='stun_effect':result=r.stun_effect(w,v)
        elif op=='retribution':result=r.retribution(w,v.spells[0],p[0])
        elif op=='colour':result=r.colour_class(v,p[0])
        elif op=='facing':result=r._facing_front(v,a)
        elif op=='damage':
            desc=None if len(p)==6 else [tuple(p[i:i+4]) for i in range(6,len(p),4)]
            result=r.damage_apply(w,v,obj(p[5]),p[0],p[1],p[2],p[3],desc,p[4])
        else:raise ValueError(op)
        result=encode(result)
    except (IndexError,AttributeError,CallbackError) as error:
        result='!'+('Error' if isinstance(error,(IndexError,AttributeError)) else type(error).__name__)
    def identity(x):return -1 if x is None else objects.index(x)
    def spell(s):return [s.kind,s.skill,s.level,identity(s.owner),s.f18,s.f1c,s.f20,s.f2c]
    def snapshot(x):return [x.tag,x.flags,x.pos,x.facing,x.owner_link,identity(x.owner),x.cls,x.innoruuk_immune,x.res,x.flash,x.cflags,x.kind,x.dot,x.armor,x.stamp_f0,x.hp,x.max_hp,x.status,x.poison_a,x.poison_b,identity(x.status_source),[spell(s) for s in x.spells],x.counter11,x.counter12,x.slot,x.state,x.model_points,x.boss_rank]
    state=[w.seed.state,w.seed.draws,w.cosmetic.state,w.cosmetic.draws,w.music,[snapshot(x) for x in objects],[[p.hp,p.max_hp,p.skills] for p in w.players],calls,w.events]
    return result,encode(state)


def generate(research,output):
    source=research/'tools/ref_combat.py';raw=source.read_bytes()
    pinned=subprocess.run(['git','-C',str(research),'show',PIN+':tools/ref_combat.py'],check=True,capture_output=True).stdout
    if hashlib.sha256(raw).hexdigest()!=SHA or raw!=pinned:raise ValueError('Reference does not match pinned source')
    sys.dont_write_bytecode=True
    r=types.ModuleType('external_combat');r.__file__=str(source);exec(compile(raw,str(source),'exec'),r.__dict__)
    cases=[]
    def add(op,args=(),setup=''):cases.append((op,list(args),setup))
    def spell(kind,owner=0,skill=7,level=1,active=0,target=0):return f'spell={target}:{kind}:{owner}:{skill}:{level}:0:0:0:{active}'
    R=[0,1,0x80000000,0x00800000,0x3f000000,0x3f800000,0x3f800001,0x41200000,0x42c80000,0xbf800000,0x7f7fffff,0x7fffffff,0xffffffff]
    for low,high in itertools.product(R,R):
        for draw in [0,65535,0xffffffff]:add('base_roll',[low,high,draw])
    for low,high in [(0,0),(1,1),(2,9),(10,5),(10,9),(-12,-3),(2147483647,-2147483648),(-2147483648,2147483647)]:
        for seed in [0,1,0xffffffff]:add('proc_amount',[low,high],f'seed={seed}')
    for value in R:
        for which in [-6,-5,-1,0,4,5]:
            for src in [-1,0,1]:add('status',[which,value,src],'v.status_source=2')
    add('fx',[0xffffffff])
    for level in [-129,-128,-1,0,127,128,255]:
        for stat in [-2147483648,-100,0,25,100,101,2147483647]:
            for active in [0,1]:add('bloodlust',[],spell('BloodLust',level=level,active=active)+f';stats=0,0,{stat}')
            for seed in [0,1]:
                add('spirit',[],spell('SpiritShield',level=level)+f';stats=100,{stat},0;seed={seed}')
                add('frost',[0x41200000,-1],spell('FrostArmor',level=level)+f';stats=0,{stat},0')
            add('retribution',[0x41200000],spell('Retribution',owner=1,level=level)+f';stats=0,{stat},0')
    for counter in [0,0x42c80000,0x42ca0000]:
        add('spirit',[],f'spell=0:SpiritShield:0:7:1:1:{counter}:7:0;stats=100,100,0')
    for skill,rank in itertools.product([9,0x21,0x35,0x51],[-1,0,1,20]):
        add('crit',[skill,rank],spell('CriticalHit',target=1,owner=1)+';stats=25,3,0')
        for enemy in [0,1]:add('crit',[skill,rank],spell('CriticalHit',target=2,owner=2)+f';skill=2,53,7;skill=2,81,11;w.pvp_a=1;enemy={enemy};stats=25,3,0')
    for flags,boss,pvp,chance in itertools.product([0,32,96],[-1,0,1,2],[0,1],[-1,0,5,6,100]):add('stun_chance',[1],f'v.flags={flags};v.boss_rank={boss};w.pvp_a={pvp};stats={chance},0,0')
    for flags,pool in itertools.product([0,32],[0,1]):add('stun_effect',[],f'v.flags={flags};pool_ok={pool}')
    for index,value,flags in itertools.product(range(5),R,[0,1,4,8,0x100,0xffffffff]):
        res=[0]*5;res[index]=value;add('colour',[flags],'v.res='+','.join(map(str,res)))
    for cf in [0,2,4,8,16,32,64,128,256,511]:add('colour',[13],f'v.cflags={cf}')
    for angle,x,y in itertools.product([0,16384,32768,49152,65535,-32768],[-1,0,1],[-1,0,1]):add('facing',[],f'v.facing={angle};a.pos={r.f32(x)},{r.f32(y)},0')
    for level,points,chance in itertools.product([0,1],[0,1],[-1,0,100]):add('thorns',[9,level],f'v.model_points={points};stats=3,{chance},0;a.flags=32')
    for chance in [0,100]:add('frost',[0x41200000,1],spell('FrostArmor')+f';stats=0,25,{chance};a.flags=32')
    # Cross-product exercises early exits, range rolls, two-hit differences,
    # crit suppression, null/self sources, mitigation and presentation gates.
    rng=random.Random(0xc0b47)
    flags=[0,1,4,8,0x40,0x100,0x200,0x4000,0x8000,0x20000,0x800000,0x8000000,0x10000000,0x40000000]
    effects=[0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1f,0x20,0x21,0x22,0x26,0x29]
    for i in range(600):
        effect=effects[i%len(effects)];kind=[0,0x10,0x12][i%3]
        args=[rng.choice(R[:10]),rng.choice(R[:10]),rng.choice([-100,0,1,15,100000]),rng.choice(flags),i%2,[-1,0,1][i%3]]
        args += [effect,rng.choice([-1,0,1,100,32767]),rng.choice([-1,1,2,100]),rng.choice([0,3,100])]
        setup=f'seed={rng.getrandbits(32)};cosmetic={rng.getrandbits(32)};v.kind={kind};v.armor={rng.choice([-1,0,1,10,32767])};v.flags={rng.choice([0,32,96,40])};a.flags={rng.choice([0,32,96,0x20020])};w.pvp_a={i%2};w.frame={i%64};pool_ok={i%2};stats=0,0,0;w.numbers_option={i%2};w.network_mode={(i//2)%2}'
        add('damage',args,setup)
    basic=[0x41200000,0x41a00000,100000,1,0,1]
    for effect in effects:
        for pool in [0,1]:
            for hp in [0,0x42c80000]:
                for resistance in [0,0x3f000000,0x3f800000,0x40000000]:
                    add('damage',basic+[effect,100,100,100],f'v.kind={18 if effect in (0x19,0x1a) else 16};pool_ok={pool};v.hp={hp};v.res='+','.join([str(resistance)]*5))
    for setup in ['a.flags=131104','v.cls=Innoruuk;v.innoruuk_immune=1','v.cflags=2048','v.cflags=1048656','a.flags=128;a.owner_link=1;a.owner=2','v.cflags=512','v.cflags=512;w.cutscene=1','v.counter12=1;a.kind=16;w.frame=4321','v.cflags=35184372088832;stats=3,100,0','v.flags=96;skill=0,9,1;skill=0,82,2;stats=3,100,0',spell('UndeadShield')+';v.flags=96;a.kind=16',spell('SecondaryUndeadShield')+';v.flags=96;a.kind=16',spell('BloodLust',active=1)+';stats=0,0,200;v.armor=10',spell('SpiritShield')+';stats=10,100,0',spell('FrostArmor')+';stats=0,25,100',spell('Retribution',target=1,owner=1)+';stats=0,50,0',spell('CriticalHit',target=1,owner=1)+';skill=1,33,3;stats=100,0,0']:
        for flags in [1,0x40,0x20001,0x800000]:add('damage',basic[:3]+[flags,0,1],setup)
    for mode in [1,2,3]:
        for fail in range(0,13):
            add('damage',basic+[0x1f,100,2,4,0x21,100,3,0,0x26,1,0,0,0x1d,100,0,0],spell('BloodLust',active=1)+spell('Retribution',target=1,owner=1).join([';',''])+f';mode={mode};fail_at={fail};stats=25,50,20')
    for setup in ['w.music_index=32','v.pos=-','v.res=-','v.status=-','a.slot=9','w.alive=9','a.owner_link=1;a.flags=128;a.owner=-1']:
        add('damage',basic,setup)
    for owner,hp in itertools.product([-1,0,1],[0,0xbf800000,0x42c80000]):
        add('retribution',[0x42c80000],spell('Retribution',owner=owner)+f';p1.hp={hp};stats=0,100,0;v.flash=1,2,3,4,99,6,7,8')
    for position in [0,0x41400000,0x44960000]:
        add('crit',[33,0],spell('CriticalHit',target=2,owner=2)+f';skill=2,53,7;stats=25,1,0;o.pos={position},0,0')
    for state,mode,attacker in itertools.product([8,11],[0,1],[-1,0,1]):
        add('damage',basic[:3]+[1,0,attacker],f'v.flags=96;v.state={state};w.pvp_a={mode};a.facing=32768')
    for op,args,setup in [
        ('bloodlust',[],spell('BloodLust',active=1)),
        ('spirit',[],spell('SpiritShield')),
        ('frost',[0x41200000,1],spell('FrostArmor')),
        ('thorns',[9,1],''),
        ('crit',[33,1],spell('CriticalHit',target=1,owner=1)),
        ('stun_chance',[1],''),
        ('stun_effect',[],''),
        ('retribution',[0x41200000],spell('Retribution',owner=1))]:
        for mode,fail in itertools.product([1,2],range(1,5)):
            add(op,args,setup+f';mode={mode};fail_at={fail};stats=3,100,100')
    rows=[];counts={}
    for config,(model,bias,nearest) in enumerate(itertools.product(r.Fpu.MODELS,[False,True],[False,True])):
        r.set_fpu(model,bias,nearest)
        for op,args,setup in cases:
            result,state=run(r,op,args,setup)
            rows.append('\t'.join(['damage',op,f'{config}|'+','.join(map(str,args))+'|'+setup,result,state]))
            counts[op]=counts.get(op,0)+1
    if source.read_bytes()!=raw:raise ValueError('Reference changed while generating')
    body=('\n'.join(rows)+'\n').encode('ascii');content=b'# combat_damage_vectors_v1\n'+body
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(content)
    manifest={'schema_version':1,'reference_commit':PIN,'reference_path':'tools/ref_combat.py','reference_sha256':SHA,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(content).hexdigest(),'expected_trace_sha256':hashlib.sha256(body).hexdigest(),'vector_count':len(rows),'op_counts':counts,'fpu_configs':16,'scope':'RTA authored synthetic damage inputs and complete ordered state/events, not original runtime or Norrath proof','exclusions':['Native recursion limit128 throws Unsupported; unbounded recursive reference inputs excluded from reference vectors and tested separately.','Object links require stable live pointees; callbacks may mutate fields and append, not destroy currently active records.']}
    output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(manifest,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--research-dir',type=Path,required=True);parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'native/tests/vectors/combat_damage.tsv');a=parser.parse_args();generate(a.research_dir.resolve(),a.output.resolve())
