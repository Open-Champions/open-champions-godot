#!/usr/bin/env python3
"""Author item-rule vectors from a pinned external model; never load an executable.
Five tab fields: domain, operation, typed argument fields, result, state.
Numbers are 16 lowercase hex digits (low 64 bits); bytes are lowercase hex;
commas join numeric lists and | separates typed fields. '-' is empty.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import struct
import subprocess
import sys

PIN = '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e'
COMMIT = '14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512'
SCHEMA = 'rta-items-v1'
FIELDS = ('level','weight','count','damage_min','damage_max','price','armor','attack',
          'slot','category','subtype','damage_bits','flags','model_row')

def nums(values):
    return ','.join(f'{x & ((1<<64)-1):016x}' for x in values) or '-'
def raw(data):
    return data.hex() or '-'

def synthetic_tables(R):
    # Bypass both executable-loading constructors. Every field below is authored.
    t = object.__new__(R.Tables)
    def row(i,preset=False):
        d=R.Definition();d.addr=(R.A_PRESETS if preset else R.A_DEFS)+64*i
        d.index=i;d.preset=preset;d.name=(f'preset{i}+m00{2+i}' if preset else f'sample{i}').encode()
        if not preset and i==3:d.name=b'thrown-sample'
        if not preset and i==4:d.name=b'band-sample'
        d.level=i+1;d.weight=R.f_bits(i+1.25);d.count=i+1
        d.damage_min=i+2;d.damage_max=i+7;d.price=0;d.armor=i*3;d.attack=i*5
        d.slot=i;d.category=i%3;d.subtype=i%7;d.damage_bits=1<<(i%4)
        d.flags=((i%4)<<11)|(0x80000 if i==4 else 0);d.picture=None;d.model_row=i*2
        return d
    t.defs=[row(i) for i in range(8)];t.presets=[row(i,True) for i in range(3)]
    t.by_addr={d.addr:d for d in t.defs+t.presets}
    t.formats=[]
    for i,(_,code) in enumerate(R.TOKEN_FORMATS):
        count=1 if code is None else i%3+1
        t.formats.append((f'm{i:02}'.encode()+b','.join([b'%d']*count),code))
    t.thrown_stem=b'thrown';t.circlet_stem_equip=b'band';t.potion_stem=b'vial'
    multipliers=[R.f_bits((i+2)/4) for i in range(7)]
    def weapon_mult(i):
        if not 0<=i<len(multipliers):raise ValueError('supplied multiplier index')
        return multipliers[i]
    t.weapon_mult=weapon_mult
    return t

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--research-dir',type=Path,required=True)
    p.add_argument('--output',type=Path,default=Path('native/tests/vectors/items.tsv'))
    a=p.parse_args();source=a.research_dir/'tools/ref_items.py';data=source.read_bytes()
    if hashlib.sha256(data).hexdigest()!=PIN:p.error('pinned item model SHA256 mismatch')
    committed=subprocess.check_output(['git','-C',str(a.research_dir),'show',COMMIT+':tools/ref_items.py'])
    if hashlib.sha256(committed).hexdigest()!=PIN:p.error('pinned commit does not match model')
    sys.dont_write_bytecode=True
    spec=importlib.util.spec_from_file_location('reference_items',source);R=importlib.util.module_from_spec(spec);spec.loader.exec_module(R)
    rng=random.Random(0x4954454d);counts={};error_counts={}
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('w',encoding='ascii',newline='\n') as output:
        output.write('# '+SCHEMA+'\n# domain\top\targuments\tresult\tstate\n')
        def emit(op,args,call,encode=nums,state=lambda:'-'):
            try:result=encode(call())
            except NotImplementedError:result='unsupported'
            except (ValueError,IndexError,KeyError,struct.error,AttributeError):result='error'
            counts[op]=counts.get(op,0)+1
            if result in ('error','unsupported'):error_counts[op]=error_counts.get(op,0)+1
            output.write('\t'.join(('items',op,'|'.join(args),result,state()))+'\n')
        edges=[0,1,0x80000000,0x7fffff,0x807fffff,0x800000,0x80800000,0x3f800000,0xbf800000,0x3f000000,0x40000000,0x7fffffff,0xffffffff,0x7f800000,0xff800000,0x4f000000,0xcf000000]
        for op in ('s8','s16','s32','fint','ftrunc'):
            for i in range(65536 if op=='s16' else 2048):
                x=i if op=='s16' or i<256 else rng.getrandbits(32)
                emit(op,[nums([x])],lambda op=op,x=x:[getattr(R,op)(x)])
        for op in ('fadd','fsub','fmul','fdiv','flt','fle','fmax','powf'):
            pairs=[(x,y) for x in edges for y in edges]+[(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(2048)]
            for x,y in pairs:emit(op,[nums([x,y])],lambda op=op,x=x,y=y:[getattr(R,op)(x,y)])
        for k in range(4096):
            x=rng.getrandbits(32);d=rng.getrandbits(64)
            if (d>>52)&2047==2047:d^=1<<52
            value=struct.unpack('<d',struct.pack('<Q',d))[0]
            emit('float_times_double',[nums([x,d])],lambda:[R.float_times_double(x,value)])
        for seed in [0,1,0xffffffff]+[rng.getrandbits(32) for _ in range(125)]:
            r=R.Rng(seed)
            emit('rng',[nums([seed,32])],lambda:[r.next() for _ in range(32)],state=lambda:nums([r.seed,r.draws]))
        for k in range(512):
            players=k%6-1;classes=[rng.randrange(-1,8) for _ in range(4)];difficulty=k%8-1;pvp=k%3;level=rng.choice(edges) if k<64 else R.f_bits(rng.uniform(-10,100))
            g=R.Game(players,classes,difficulty,0,pvp,level_raw=level)
            emit('game',[nums([players,*classes,difficulty,pvp,level])],lambda:[g.level_times_0_4(),g.level_times_difficulty(),g.cloth_fraction()])
        t=synthetic_tables(R)
        for definition in t.defs+t.presets:
            i=R.Item(definition)
            emit('definition_index',[nums([definition.addr])],lambda:[R.encode_definition_index(t,i),R.decode_definition_index(t,R.encode_definition_index(t,i)).addr])
            for seed in (0,0xffffffff):
                r=R.Rng(seed);i.flags=definition.flags|0xff0000000000
                emit('randomize_variant',[nums([definition.addr,i.flags,seed])],lambda:(R.randomize_variant(i,r),[i.flags,i.variant])[1],state=lambda:nums([r.seed,r.draws]))
        for name in (b'sample0',b'sample7',b'unknown',b'preset0+m002',b''):
            for presets in (False,True):emit('find_definition',[raw(name),nums([presets])],lambda:[R.find_definition(t,name,presets).addr])
        forms=[b'%d',b'%d,%d',b'%d,%d,%d',b'literal%d',b' %d\t%d',b'bad%x',b'%']
        texts=[b'',b' ',b'+',b'-',b'0',b'-1',b'  +42 ',b'1,2,3tail',b'literal',b'liter',b'literal+9',b'9'*80,b'-'+b'9'*80,b'9223372036854775808',b'\xff']
        for form in forms:
            for text in texts:emit('scan',[raw(form),raw(text)],lambda:[R.scan(form,text)[0],*R.scan(form,text)[1]])
        descriptors=[b'',b'unknown',b'sample1',b'sample1+',b'sample1++',b'sample1\0+m003',b'x'*207,b'x'*208,b'x'*209]
        for j,(form,code) in enumerate(t.formats):
            prefix=f'm{j:02}'.encode()
            for tail in (b'',b' ',b'-32769',b'32768,2,3',b'+',b'1,2,3tail',b'9'*40):descriptors.append(b'sample1+'+prefix+tail)
        descriptors += [b'sample1+m001+m012,3+m022,3,4+m034+m045,6',b'preset0+m002']
        for text in descriptors:
            for variant in (-1,0,32,-2147483648):
                r=R.Rng(0x12345678);presets=text.startswith(b'preset')
                emit('parse_descriptor',[raw(text),nums([variant,presets,r.seed])],lambda:R.parse_descriptor(t,text,variant,presets,r).pack(bytes(range(72))),raw,state=lambda:nums([r.seed,r.draws]))
        for seed in (0,1,0xffffffff):
            r=R.Rng(seed);emit('load_resources',[nums([seed])],lambda:[R.load_resources(t,r)],state=lambda:nums([r.seed,r.draws]))
        def random_item(t):
            i=R.Item(t.defs[0]);i.count=rng.randrange(-32768,32768);i.flags=rng.getrandbits(64)
            i.a=[rng.randrange(-32768,32768) for _ in range(4)];i.b=[rng.randrange(-32768,32768) for _ in range(4)]
            i.prefix=rng.randrange(256);i.suffix=rng.randrange(256)
            i.mods=[None if rng.randrange(5)==0 else [rng.randrange(45),*[rng.randrange(-32768,32768) for _ in range(3)]] for _ in range(4)]
            return i
        for k in range(1024):
            t=synthetic_tables(R);d=t.defs[0]
            values=[rng.randrange(-32768,32768),rng.choice(edges) if k<128 else R.f_bits(rng.uniform(0,50)),rng.randrange(-32768,32768),rng.randrange(-32768,32768),rng.randrange(-32768,32768),rng.choice([0,1,-1,2147483647]),rng.randrange(-32768,32768),rng.randrange(-32768,32768),rng.randrange(-3,20),k%4,k%7,rng.getrandbits(32),rng.getrandbits(64),rng.randrange(-32768,32768)]
            for field,value in zip(FIELDS,values):setattr(d,field,value)
            item=random_item(t)
            if k<45:item.mods[0]=[k,10,20,30]
            record=item.pack(bytes(rng.randrange(256) for _ in range(72)))
            args=[nums(values),raw(record)]
            for op in ('item_weight','item_armor','item_attack_rating','resource_index'):
                emit(op,args,lambda op=op:[getattr(R,op)(item)])
            for mode in (False,True):emit('item_damage_range',args+[nums([mode])],lambda:R.item_damage_range(item,mode))
            emit('item_value',args,lambda:[R.item_value(t,item)])
            emit('record',args,lambda:R.Item.unpack(t,record).pack(record),raw)
            emit('undefined_offsets',args,lambda:item.undefined_offsets())
            code=k%45;which=k%3
            emit('get_modifier',args+[nums([code,which])],lambda:[item.get(code,which)])
            changed=item.copy();change=[k%47,k*100,-k*100,65535]
            emit('set_modifier',args+[nums(change)],lambda:[changed.set_modifier(*change)]+list(changed.pack(record)))
            other=random_item(t);other_data=other.pack(record)
            emit('same_identity',args+[raw(other_data)],lambda:[R.same_identity(item,other)])
            emit('copy_mutable',args+[raw(other_data)],lambda:(R.copy_mutable(item,other),item.pack(record))[1],raw)
            player=R.Player(k%8,k%256-128,*[rng.randrange(-32768,32768) for _ in range(4)],skills={0x3c:k%6,0x3d:k%7,0x5c:k%2})
            pv=[player.class_id,player.level,player.strength,player.intelligence,player.dexterity,player.stamina,player.skill_rank(0x3c),player.skill_rank(0x3d),player.skill_rank(0x5c)]
            # copy_mutable changed item above: recreate the serialized input for this call.
            item=R.Item.unpack(t,record)
            emit('equip_restriction',args+[nums(pv)],lambda:[R.equip_restriction(t,player,item)])
        for value in [-2147483648,-1,0,1199,1200,6199,6200,2147483647]+[R.s32(rng.getrandbits(32)) for _ in range(1024)]:emit('required_level_for_value',[nums([value])],lambda:[R.required_level_for_value(value)])
        for size in range(72):emit('short_record',[raw(bytes(size))],lambda:R.Item.unpack(t,bytes(size)).pack(),raw)
    manifest={'schema':SCHEMA,'reference':{'commit':COMMIT,'path':'tools/ref_items.py','sha256':PIN},'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vectors_sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),'rows':sum(counts.values()),'operations':dict(sorted(counts.items())),'error_cases':error_counts,'seed':'0x4954454d','scope':'Authored supplied tables and exact pinned model behavior; no original data, executable or console proof.'}
    a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8')
    print(f'Generated {sum(counts.values())} rows across {len(counts)} operations')

if __name__=='__main__':main()
