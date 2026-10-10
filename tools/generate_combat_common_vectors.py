#!/usr/bin/env python3
"""Pinned combat common vectors, authored state only; never open a game executable."""
import argparse, hashlib, importlib.util, json, random, subprocess, sys
from pathlib import Path
from fractions import Fraction
PIN='b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'
COMMIT='7212ecfecbeae8dd2015f6c82897226f49a94695'
SCHEMA='rta-combat-common-v1'
MASK=(1<<64)-1
RAW_FIELDS={'pos','res','dot','field_dc','hp','max_hp','status','exp_value','f1c','f20','mana','max_mana','stagger','load_limit'}
REF_FIELDS={'owner','status_source','last_attacker','obj','defn','second_weapon'}

def nums(a):return ','.join(f'{x&MASK:016x}' for x in a) or '-'
def dencode(q,R):
    if q is R.NAN:return 0x7ff8000000000000
    if not q:return 0
    sign=(1<<63) if q<0 else 0;q=abs(q)
    e=q.numerator.bit_length()-q.denominator.bit_length()
    if q<Fraction(2)**e:e-=1
    if not -1022<=e<=1023:raise ValueError('unrepresentable double fixture')
    v=q/Fraction(2)**(e-52)
    if v.denominator!=1:raise ValueError('double fixture has too many bits')
    return sign|((e+1023)<<52)|(int(v)&((1<<52)-1))
def ddecode(x,R):
    e=(x>>52)&2047;m=x&((1<<52)-1)
    if e==2047:
        if m:return R.NAN
        raise ValueError('infinity fixture')
    if e==0:
        if m:raise ValueError('subnormal fixture')
        return Fraction(0)
    return (-1 if x>>63 else 1)*Fraction((1<<52)|m)*Fraction(2)**(e-1023-52)

def encode(x,refs):
    if x is None:return 'N'
    if isinstance(x,bool):return 'T' if x else 'F'
    if isinstance(x,int):return nums([x])
    if isinstance(x,str):return 'S'+','.join(f'{ord(c):08x}' for c in x)
    if isinstance(x,(list,tuple)):return '['+';'.join(encode(v,refs) for v in x)+']'
    return '@'+str(refs.index(x)+1)
def snap(obj,refs):return encode([getattr(obj,n) for n in obj.FIELDS],refs)
def record_fixture(R,which,seed):
    refs=[R.Obj(),R.Obj(),R.Spell(),R.Spell(),R.ItemDef(),R.Item(),R.Item()]
    cls=[R.Spell,R.Obj,R.ItemDef,R.Item,R.PlayerRec][which];obj=cls()
    def n(k):return R.s32(seed+k*0x9e3779b9)
    for k,(name,default) in enumerate(cls.FIELDS.items(),1):
        if name in REF_FIELDS:
            setattr(obj,name,None if seed&1 else refs[4 if name=='defn' else 5 if name=='second_weapon' else k%2]);continue
        if isinstance(default,str):v='Test'+chr(0x3a3+k)+chr(0xff+k)
        elif isinstance(default,list):
            if name=='spells':v=[refs[2],refs[3],refs[2]]
            elif name in ('bag2','equip','weapon_sets'):v=[refs[5],None,refs[6]]
            elif name=='mods':v=[(n(k),n(k+1),n(k+2),n(k+3)),(-1,0,0,0)]
            elif name=='skills':v=[(n(k),n(k+1)),(-1,5),(2,8)]
            else:v=[(n(k+i)&0xffffffff) if name in RAW_FIELDS else n(k+i) for i in range((seed+k)%6)]
        elif isinstance(default,bool):v=bool(seed&2)
        elif name in RAW_FIELDS or name=='own_seed':v=n(k)&0xffffffff
        elif name in ('flags','cflags','dmg_type'):v=((seed<<32)&MASK)|(n(k)&0xffffffff)
        else:v=n(k)
        setattr(obj,name,v)
    return obj,refs

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',type=Path,required=True);p.add_argument('--output',type=Path,default=Path('native/tests/vectors/combat_common.tsv'));a=p.parse_args()
    source=a.research_dir/'tools/ref_combat.py';data=source.read_bytes()
    if hashlib.sha256(data).hexdigest()!=PIN:p.error('model SHA mismatch')
    if hashlib.sha256(subprocess.check_output(['git','-C',str(a.research_dir),'show',COMMIT+':tools/ref_combat.py'])).hexdigest()!=PIN:p.error('commit mismatch')
    sys.dont_write_bytecode=True;s=importlib.util.spec_from_file_location('combat_reference',source);R=importlib.util.module_from_spec(s);s.loader.exec_module(R)
    rng=random.Random(0x434f4d42);rows=[];counts={};errors=0
    def put(op,args,result,state='-'):
        nonlocal errors
        result=result if isinstance(result,str) else nums(result)
        rows.append('\t'.join(['combat-common',op,args,result,state]));counts[op]=counts.get(op,0)+1;errors+=result in ('error','unsupported')
    edges=[0,1,0x7fffff,0x800000,0x80000000,0x807fffff,0x80800000,0x3f800000,0xbf800000,0x3f000000,0x40000000,0x7f800000,0xff800000,0x7fffffff,0xffffffff]
    unary=['s8','s16','s32','itof','ftoi','fsqrt','sqrtf','round_half_away','cos16','sin16']
    binary=['fadd','fsub','fmul','fdiv','flt','fle','feq','fmin','fmax','powf']
    for config in range(16):
        R.set_fpu(('ps2','ps2g1','chop','nearest')[config//4],bool(config&2),bool(config&1))
        for x in edges+[rng.getrandbits(32) for _ in range(256)]:
            for op in unary:put(op,nums([config,x]),[getattr(R,op)(x)])
        pairs=[(x,y) for x in edges for y in edges]+[(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(256)]
        for x,y in pairs:
            for op in binary:put(op,nums([config,x,y]),[getattr(R,op)(x,y)])
        for i in range(256):
            x=rng.getrandbits(32);n=rng.choice([-2147483648,-50001,-256,-128,-1,0,1,128,256,50001,2147483647])
            put('scalbnf',nums([config,x,n]),[R.scalbnf(x,n)])
        for i in range(32):
            seed=rng.getrandbits(32);s=R.Seed(seed)
            points=[v for _ in range(4) for v in s.in_unit_sphere()]
            put('sphere',nums([config,seed]),points,nums([s.state]+s.draws))
    R.set_fpu()
    for angle in range(65536):put('angles',nums([0,angle]),[R.cos16(angle),R.sin16(angle)])
    for x in range(65536):put('signed16',nums([0,x]),[R.s16(x)])
    for i in range(2048):
        x=rng.getrandbits(32);y=rng.getrandbits(32)
        put('c_rem',nums([0,x,y]),[R.c_rem(R.s32(x),R.s32(y))])
        put('dbl_from_int',nums([0,x]),[dencode(R.dbl_from_int(x),R)])
        put('dbl_from_unsigned',nums([0,x]),[dencode(Fraction(x),R)])
        put('dbl_from_single',nums([0,x]),[dencode(R.dbl_from_single(x),R)])
        da=R.dbl_from_single(x);db=R.dbl_from_single(y)
        for op in ['dbl_add','dbl_mul','dbl_fmod']:
            out=getattr(R,op)(da,db);put(op,nums([0,dencode(da,R),dencode(db,R)]),[dencode(out,R)])
        put('dbl_to_single',nums([0,dencode(da,R)]),[R.dbl_to_single(da)])
    for x,y in [(0,0),(0x80000000,0xffffffff),(0x80000000,0),(7,0),(-7,0)]:put('c_rem',nums([0,x,y]),[R.c_rem(R.s32(x),R.s32(y))])
    for d in [0,1<<63,0x7ff8000000000000,0xfff8000000000001,0x3ff0000000000000,0x47f0000000000000,0x37f0000000000000]:
        for op in ['dbl_to_single']:
            put(op,nums([0,d]),[R.dbl_to_single(ddecode(d,R))])
    for op in ['dbl_add','dbl_mul','dbl_fmod']:
        for x,y in [(1<<63,0),(0xbff0000000000000,0x3ff0000000000000),(0x7ff8000000000000,0x3ff0000000000000),(0x3ff0000000000000,0)]:
            # canonical NaN, including direct zero divisor
            x=x&((1<<64)-1)
            put(op,nums([0,x,y]),[dencode(getattr(R,op)(ddecode(x,R),ddecode(y,R)),R)])
    # Exact Fraction-chop borrow survives even when the operand lies below the
    # core soft-double library's alignment cutoff. Sweep both signs and orders.
    for gap in range(301):
        for frac in [0,1,0x543210fedcba9,0xfffffffffffff]:
            big=(1200<<52)|frac;small=((1200-gap)<<52)|0x123456789abcd
            for sign in [0,1<<63]:
                for opposite in [0,1<<63]:
                    x=big|sign;y=small|(sign^opposite)
                    for x,y in [(x,y),(y,x)]:
                        put('dbl_add',nums([0,x,y]),[dencode(R.dbl_add(ddecode(x,R),ddecode(y,R)),R)])
    for i in range(128):
        seed=rng.getrandbits(32);s=R.Seed(seed);out=[s.draw() for _ in range(32)];put('seed',nums([0,seed]),out,nums([s.state]+s.draws))
    for size in range(7):
        for position in [-2147483648,-9,-size-1,-size,-1,0,size-1,size,9,2147483647]:
            try:out=[list(range(size))[position]]
            except IndexError:out='error'
            put('index',nums([0,size,position]),out)
    for i in range(512):
        desc=[tuple(R.s32(rng.getrandbits(32)) for _ in range(4)) for _ in range(i%6)]
        if i%7==0:desc.append(())
        kind=R.s16(desc[0][0]) if desc and desc[0] and i%2 else i%31;field=i%8-3
        try:out=[R.desc_get(desc,kind,field)]
        except IndexError:out='error'
        arg='|'.join([nums([0,kind,field])]+[nums(row) for row in desc]);put('descriptor',arg,out)
        rec=R.PlayerRec(skills=[(1,2),(0x10001,0x1ffff),(-1,55),(2,99)] if i%2 else [(2,3),(1,0xffff)])
        put('rank',nums([0,i%2,i%5-1]),[rec.rank(i%5-1)])
    for which in range(5):
        obj,refs=record_fixture(R,which,0)
        cls=[R.Spell,R.Obj,R.ItemDef,R.Item,R.PlayerRec][which]
        put('defaults',nums([0,which]),snap(cls(),refs))
        for i in range(128):
            seed=rng.getrandbits(32);obj,refs=record_fixture(R,which,seed);copy=obj.copy()
            before=snap(copy,refs)
            for name,default in obj.FIELDS.items():
                if isinstance(default,list):getattr(copy,name).clear()
            put('copy',nums([0,which,seed]),before,snap(obj,refs)+'|'+snap(copy,refs))
    for selector in [-1,0,1,2,3]:
        refs=[R.Spell(kind='A'),R.Spell(kind='B'),R.Spell(kind='A')];obj=R.Obj(spells=refs[:max(selector,0)])
        for kind in ['A','B','missing']:
            result=obj.spell(kind);put('spell',nums([0,selector])+'|'+kind,[0 if result is None else refs.index(result)+1])
    for i in range(128):
        w=R.World(i,~i);victim=R.Obj(tag='victim');attacker=R.Obj(tag='attacker');refs=[victim,attacker]
        w.stat_fn=lambda a,b,c:0x100000000+a-b+c
        w.stat_for_fn=lambda a,b,c,o,d:0xffffffff+a+b+c+d+(7 if o is attacker else 0)
        w.adjust_fn=lambda a,b,c:0x80000000+a+b+c
        w.pool_fn=lambda size:size%2==0
        result=[w.stat(i,2,3),w.stat_for(1,2,3,attacker,4),w.adjust(i,3,4),int(w.pool(i))]
        def take(v,amount,a,flags):
            v.hp=amount;v.last_attacker=a;w.log('callback',v.tag,flags);w.seed.draw()
        w.take_damage_fn=take;w.take_damage(victim,0x3f800000+i,None if i&1 else attacker,0xf0000000+i)
        put('world',nums([0,i]),result,encode(w.events,refs)+'|'+nums([victim.hp,0 if victim.last_attacker is None else 2,w.seed.state]+w.seed.draws))
    out=('# '+SCHEMA+'\n# domain\top\targs\tresult\tstate\n'+'\n'.join(rows)+'\n').encode();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(out)
    manifest={'schema':SCHEMA,'rows':len(rows),'operations':counts,'errors':errors,'reference':{'commit':COMMIT,'path':'tools/ref_combat.py','sha256':PIN},'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vectors_sha256':hashlib.sha256(out).hexdigest(),'scope':'Synthetic records, supplied tables, selected arithmetic, callback events and identity only; no executable input. Finite normal binary64 helpers plus canonical NaN.'}
    a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n');print(len(rows),'vectors',len(counts),'operations')
if __name__=='__main__':main()
