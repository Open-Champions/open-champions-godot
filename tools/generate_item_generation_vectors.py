#!/usr/bin/env python3
"""Authored item generation/loot cases against a pinned external Python model.
Only synthetic tables are supplied; no executable or original data table is read.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import types

COMMIT='14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512'
PIN='513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e'
SEED=0x4954454d


def generate(research,output):
    source=subprocess.run(['git','-C',str(research),'show',COMMIT+':tools/ref_items.py'],check=True,capture_output=True).stdout
    if hashlib.sha256(source).hexdigest()!=PIN: raise RuntimeError('reference SHA mismatch')
    r=types.ModuleType('pinned_ref_items');exec(compile(source,'pinned_ref_items.py','exec'),r.__dict__)
    class SyntheticTables(r.Tables):
        def prefix_text(self,p,c): return self.prefixes[p].fixture_texts[c] if 0<=c<3 else None
        def suffix_text(self,p,c): return self.suffixes[p].fixture_texts[c] if 0<=c<3 else None
        def weapon_mult(self,s): return self.weapon_multipliers[s]
        def quality_row(self,i): return self.quality_rows[i]
    class Prefix(r.Prefix): pass
    class Suffix(r.Suffix): pass
    class PotionMemory:
        def __init__(self,mana,heal): self.mana=mana;self.heal=heal
        def u32(self,address):
            # Authored synthetic token pointers, not addresses into a game image.
            if r.A_HEAL_POTIONS<=address<r.A_HEAL_POTIONS+48:return 200+(address-r.A_HEAL_POTIONS)//4
            if r.A_MANA_POTIONS<=address<r.A_MANA_POTIONS+48:return 100+(address-r.A_MANA_POTIONS)//4
            raise ValueError('outside synthetic potion table')
        def cstr(self,p):return self.mana[p-100] if 100<=p<112 else self.heal[p-200]
    def tables(config):
        t=object.__new__(SyntheticTables)
        t.defs=[];t.presets=[];t.prefixes=[];t.suffixes=[]
        def definition(name,category,subtype=0,slot=0,flags=0,level=1,price=100,picture=b'ore',preset=False):
            d=object.__new__(r.Definition);rows=t.presets if preset else t.defs
            d.index=len(rows);d.addr=(r.A_PRESETS if preset else r.A_DEFS)+64*d.index;d.preset=preset;d.name=name
            d.level=level;d.weight=0x3f800000;d.count=1;d.damage_min=2;d.damage_max=5;d.price=price;d.armor=4;d.attack=3
            d.slot=slot;d.category=category;d.subtype=subtype;d.damage_bits=8 if subtype==3 else 1;d.flags=flags;d.picture=picture;d.model_row=0
            rows.append(d);return d
        for i,sub in enumerate([6,3,0]):definition(('weapon'+str(i)).encode(),0,sub,flags=(3<<11)|2,price=100+i*25)
        for i,slot in enumerate([11,4,9,12,3,0]):
            definition(('armor'+str(i)).encode(),1,slot=slot,flags=(2<<11),price=70+i)
            definition(('cloth'+str(i)).encode(),1,slot=slot,flags=(2<<11)|0x80000,price=70+i)
        definition(b'circlet_demo',1,slot=11,price=75)
        definition(b'other',2,flags=3<<11,price=55)
        definition(b'ore_low',2,flags=(2<<11)|0x800000000,level=1,price=40)
        definition(b'ore_high',2,flags=0x800000000|0x10000,level=38,price=90)
        definition(b'figure_low',2,flags=0x200000000000,level=1,price=35)
        definition(b'figure_high',2,flags=0x200000000000|(4<<11),level=38,price=95)
        for group in ['m','h']:
            for i in range(12):definition(('p_'+group+str(i)).encode(),2,price=10+i)
        definition(b'arrows',0,subtype=6,price=2)
        definition(b'recall',2,price=30)
        for i in range(12):definition(('preset'+str(i)+'+boost2').encode(),i%3,flags=(3<<11)|(0x80000 if i==4 else 0),level=1 if i<9 else 40,price=100+(i//3)*20,preset=True)
        for i,level in enumerate([1,50,1,2]):
            p=Prefix();p.addr=0;p.index=i;p.key=('pre'+str(i)).encode();p.level=level;p.mask=[0,1,2,0][i];p.damage_mask=[0,0,8,8][i]
            p.fixture_texts=[b'+boost'+str(i+1).encode()]*3
            if i==2:p.fixture_texts[1]=None
            t.prefixes.append(p)
        for i,level in enumerate([1,40,2,1]):
            s=Suffix();s.addr=0;s.index=i;s.key=('suf'+str(i)).encode();s.level=level;s.fixture_texts=[b'+worth'+str(i*5).encode()]*3
            if i==2:s.fixture_texts[1]=None
            t.suffixes.append(s)
        t.formats=[(b'boost%d',0),(b'worth%d',0x25),(b'guard%d',0x0d)];t.token_words=[b'boost',b'worth',b'guard']
        t.potion_stem=b'p_';t.thrown_stem=b'thrown_';t.circlet_stem=b'circlet';t.circlet_stem_equip=b'circlet';t.arrows=b'arrows';t.recall=b'recall'
        t.name_key_format=b'%s';t.name_space=b' ';t.of_key=b'of'
        t.gold_scale_multiplayer=struct.unpack('<d',struct.pack('<Q',0x4003333333333333))[0]
        t.weapon_multipliers=[0x3f800000]*8;t.quality_rows=[[0x3e800000,0x3f000000,0x3e800000] if i%2==0 else [0x3d800000,0x3f400000,0x3e400000] for i in range(12)]
        t.mana=[('p_m'+str(i)).encode() for i in range(12)];t.heal=[('p_h'+str(i)).encode() for i in range(12)]
        # The original table addresses overlap at mana slots 10/11. Make the
        # synthetic supplied lists consistent without changing reference globals.
        t.mana[10:12]=t.heal[:2]
        t.exe=PotionMemory(t.mana,t.heal)
        if config==1:t.defs=[];t.presets=[]
        if config==2:t.prefixes=[]
        if config==3:t.suffixes=[]
        if config==4:
            for s in t.suffixes:s.level=99
        if config==5:t.defs[17].picture=None
        if config==6:
            for d in t.defs+t.presets:d.flags|=0x10000
        if config==7:t.suffixes[0].fixture_texts[2]=None
        if config==9:
            for d in t.defs+t.presets:d.flags&=~(31<<11)
        if config==10:t.defs=[t.defs[2]];t.presets=[]
        if config==11:t.quality_rows=[]
        if config==12:
            for d in t.defs:
                if d.category==2 and not (d.flags&(0x800000000|0x200000000000)):d.name=b'p_'+d.name
        if config==13:
            for d in t.defs+t.presets:d.price=0
        if config==14:
            for d in t.defs:d.flags&=~(0x800000000|0x200000000000)
        t.by_addr={d.addr:d for d in t.defs+t.presets}
        return t
    def itemtext(item):return '-' if item is None else item.pack().hex()
    def picture(code):return [None,b'ore',b'absent',b''][code]
    class World(r.World):
        def __init__(self,mode):super().__init__();self.mode=mode;self.events=[]
        def spawn_gold(self,amount,rng):
            self.events.append('gold:'+str(amount))
            if self.mode in (1,3,4):self.events.append('ctor:'+str(rng.next()))
            super().spawn_gold(amount,rng)
        def spawn_item(self,kind,item,rng):
            self.events.append('item:'+kind+':'+itemtext(item))
            if self.mode==2 or (self.mode==3 and kind=='equipment'):
                self.events.append('fail:'+kind);return False
            if self.mode in (1,3,4,5):self.events.append('ctor:'+str(rng.next()))
            if self.mode==4:item.count=77;item.flags^=1;item.set_modifier(0x0e,71)
            if self.mode==5:self.events.append('exception:item');raise ValueError('authored callback failure after draw')
            return super().spawn_item(kind,item,rng)
        def throw(self,item,angle):self.events.append('throw:'+str(angle));super().throw(item,angle)
    rows=[];counts=Counter();outcomes=Counter();events_seen=Counter();shop_buffer=Counter()
    def emit(op,args):
        config,seed,players,diff,pa,pb,level,classbits=args[:8];tail=args[8:]
        t=tables(config);rng=r.Rng(seed);g=r.Game(num_players=players,classes=tuple(4 if classbits&(1<<i) else 0 for i in range(4)),difficulty=diff,pvp_a=pa,pvp_b=pb,level_raw=level)
        world=World(tail[7] if op=='loot' else (tail[0] if op=='world' else (tail[1] if op=='named' and len(tail)>1 else 0)));stock=[];result='-'
        try:
            if op=='preset':result=itemtext(r.generate_preset(t,g,rng,*tail))
            elif op=='affixes':result=itemtext(r.generate_with_affixes(t,g,rng,*tail))
            elif op=='reagent':result=itemtext(r.generate_reagent(t,g,rng,tail[0],picture(tail[1])))
            elif op=='figurine':result=itemtext(r.generate_figurine(t,g,rng,tail[0]))
            elif op=='consumable':result=itemtext(r.generate_consumable(t,g,rng))
            elif op=='shop':result='~'.join(itemtext(r.shop_append_unique(t,g,rng,stock,tail[0],tail[1])) for _ in range(tail[2]))
            elif op=='shop_buffer':
                probe=r.Rng(seed)
                first=r.generate_with_affixes(t,g,probe,100000,0,-1,1)
                second=r.generate_with_affixes(t,g,probe,100000,0,-1,1)
                shop_buffer['first_empty' if first is None else ('second_empty' if second is None else 'both_made')]+=1
                if first is not None:stock.append(first.copy())
                result=itemtext(r.shop_append_unique(t,g,rng,stock,100000,0))
            elif op=='named':result=itemtext(r.spawn_named_item(t,rng,world,t.defs[tail[0]].name+b'+boost3'))
            elif op=='classify':result=str(r.weapon_group(t.defs[tail[0]]))+','+str(r.armor_group(t.defs[tail[0]]))
            elif op=='half':result=str(r.half_level(g))
            elif op=='world':
                item=r.parse_descriptor(t,t.defs[0].name,-1,False,rng)
                world.spawn_gold(-3,rng);ok=world.spawn_item('named',item,rng);world.throw(item,-12);result=str(int(ok))
            elif op=='scatter':
                calls=0
                def ground(x,y,z):
                    nonlocal calls
                    calls+=1;world.events.append('terrain:'+','.join(map(str,[x,y,z])))
                    mode=tail[3]
                    if mode==0:return z
                    if mode==1:return z if calls==10 else r.fadd(z,0x40c00000)
                    if mode==2:return r.fadd(z,0x40e00000)
                    if mode==3:return r.fsub(z,0x40bfffff)
                    return r.fsub(z,0x3f800000) if calls>2 else r.fsub(z,0x40c00000)
                result=','.join(map(str,r.gold_scatter(rng,ground,*tail[:3])))
            elif op=='loot':result=r.loot_generate_drops(t,g,rng,world,*tail[:5],reagent_filter=picture(tail[5]),force_preset=bool(tail[6]))
            else:raise AssertionError(op)
        except NotImplementedError:result='!Unsupported'
        except (ValueError,TypeError,IndexError,KeyError):result='!Error'
        drops='|'.join(k+':'+(itemtext(v) if isinstance(v,r.Item) else str(v)) for k,v in world.drops) or '-'
        state='seed='+str(rng.seed)+';draws='+str(rng.draws)+';stock='+('~'.join(map(itemtext,stock)) or '-')+';events='+('|'.join(world.events) or '-')+';drops='+drops
        rows.append('\t'.join(['items',op,','.join(map(str,args)),result,state]));counts[op]+=1
        if op=='loot':outcomes[result]+=1
        for event in world.events:events_seen[event.split(':')[0]]+=1
    def args(seed=0,cfg=0,players=1,diff=1,pa=0,pb=0,level=0x42480000,classes=0):return [cfg,seed,players,diff,pa,pb,level,classes]
    for i in range(len(tables(0).defs)):emit('classify',args()+[i])
    for raw in [0,1,0x80000000,0x3f800000,0xbf800000,0x40400000,0xc0400000,0x4effffff,0x4f000000,0xcf000000,0x7fffffff,0xffffffff]:emit('half',args(level=raw))
    for seed in [0,1,2,17,100,999,0x80000000,0xffffffff]:
        for category in [-1,0,1,2]:
            for cap in [1,20,80]:
                emit('preset',args(seed,players=2,classes=1)+[100000,category,cap])
                for suffix in [-1,0,1]:emit('affixes',args(seed,players=2,classes=1)+[100000,category,suffix,cap])
    for cfg in [1,2,3,4,5,6,7,9,10,12,13,14]:
        for seed in [0,1,17,999]:
            for cat in [0,1,2]:
                emit('preset',args(seed,cfg)+[100000,cat,80])
                emit('affixes',args(seed,cfg)+[100000,cat,0,80])
                emit('shop',args(seed,cfg)+[100000,cat,3])
    for seed in [0,17,999,0xffffffff]:
        for diff in [0,1,2,3,4,5]:
            for cap in [0,1,17,27,32,38,80,0xffff,0x8000]:
                for cfg in [0,5,6]:
                    for pic in [0,1,2,3]:emit('reagent',args(seed,cfg,diff=diff)+[cap,pic])
                emit('figurine',args(seed,diff=diff)+[cap])
    rr=random.Random(SEED)
    for i in range(256):
        seed=rr.getrandbits(32)
        emit('consumable',args(seed,diff=i%6,pb=i%2,level=[0x3f800000,0x41480000,0x41c80000,0x42480000][i%4]))
        emit('scatter',args(seed)+[0x41400000,0xc0800000,0x40a00000,i%5])
    for seed in [0,17,999]:
        for idx in [0,16,17,45]:
            for mode in [0,1,2,4,5]:emit('named',args(seed)+[idx,mode])
        for mode in range(6):emit('world',args(seed)+[mode])
        for cat in [0,1,2]:emit('shop',args(seed)+[100000,cat,5])
    for i in range(2048):
        seed=rr.getrandbits(32)
        emit('loot',args(seed,cfg=[0,0,0,14,1][i%5],players=1+i%4,diff=1+i%5,pa=int(i%13==0),pb=int(i%17==0),level=[0x3f800000,0x42480000,0x42a00000][i%3],classes=i%16)+[0x40000000,0x3f000000,[0xbf800000,0x3f000000,0x41180000][i%3],0x40000000,0x7fffffff if i%2 else -0x80000000,i%4,int(i%11==0),i%4])
    for seed in range(64):emit('shop_buffer',args(seed,cfg=10))
    for seed in [0,1,999]:
        emit('loot',args(seed,cfg=11)+[0,0,0,0,0,0,0,0])
        emit('preset',args(seed)+[-1,0,80]);emit('affixes',args(seed)+[-1,0,1,80])
    content='# item_generation_v1\n# domain\top\targs\tresult\tstate\n'+'\n'.join(rows)+'\n'
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(content.encode('ascii'))
    manifest={'schema':1,'reference':'tools/ref_items.py','reference_commit':COMMIT,'reference_sha256':PIN,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'rows':len(rows),'expected_trace_sha256':hashlib.sha256(('\n'.join(rows)+'\n').encode('ascii')).hexdigest(),'operations':dict(counts),'loot_outcomes':dict(outcomes),'world_events':dict(events_seen),'shop_buffer_cases':dict(shop_buffer),'seed':SEED,'scope':'Authored tables and callbacks; RTA published Python model only, no retail data or console/Norrath claim. World events are owned snapshots. Synthetic potion lists preserve the shared slots at the published pointer-table boundary.'}
    output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=Path);p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'native/tests/vectors/item_generation.tsv');a=p.parse_args();generate(a.research_dir,a.output)
