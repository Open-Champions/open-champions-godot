#!/usr/bin/env python3
"""Generate authored inputs and full expected state from immutable research Python."""
import argparse
import gzip
import hashlib
import importlib
import json
import pathlib
import random
import subprocess
import sys
import tempfile

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
 'tools/ref_skills_b.py': 'b71cbdd9e82cd72abb855b9f6ab99d11fb78a58613cb69d314a5041457bbdd95',
 'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
 'tools/ref_creature.py': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7',
 'tools/ref_arena_monsters.py': '84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16',
 'tools/difftest_skills_b.py': '06778b17758b7b12e30df0a0640706f456f4df58a3e39dda67cf28f3d1c39b47',
 'docs/spec/skills-35-68.md': 'a7092cba387542fd7e926145f245ddfcb5339a374442b478e1d810f52049eb6b',
 'docs/spec/skills-35-68-executed.md': '698400358c3ca2f221c9e79fa19bf64703006069925f01f2e983a5706fd681f5',
}
S, T, O, H, N, L, B = 0x10000, 0x20000, 0x30000, 0x40000, 0x41000, 0x50000, 0x60000
BODIES = {1:0x3c8218, 2:0x3c8220, 5:0x3c82a8, 8:0x3c82e4, 10:0x3c85fc, 11:0x3c8690,
          12:0x3c87f0, 13:0x3c8864, 14:0x3c88dc, 15:0x3c88e4}
MASK = 0xffffffff

def flat(records):
    return ';'.join(','.join(map(str, r)) for r in records)

def state(model, results, status):
    memory = flat(sorted(model.mem.bytes.items()))
    writes = flat(model.mem.log)
    calls = ';'.join(name + ',' + ','.join(str(a & MASK) for a in args) for name,args in model.calls)
    return '|'.join((status, ','.join(map(str,results)), str(model.draws), memory, writes, calls))

class CallbackFailure(Exception): pass
class Missing(Exception): pass

METHODS = {
 'stat':'skill_get_stat', 'tier':'skill_get_stat_tier2', 'for':'skill_get_stat_for',
 'handle':'skill_get_stat_for_handle', 'mana':'skill_mana_from_stat4', 'range':'skill_stat_range',
 'archery':'archery_fraction', 'lifetap':'lifetap_fraction', 'life1':'lifetap_stat1',
 'stance':'strongstance_stat0', 'resist':'naturalresistance_fraction', 'stun':'stunningstrike_stat0_for',
 'ctor':'spell_base_ctor', 'spellrand':'spell_rand', 'objectrand':'obj_rand',
 'shared':'draw', 'cosmetic':'cosmetic_draw', 'third':'seed3_draw',
 'adjust':'skill_adjust_level', 'can':'skill_can_use_base', 'cast':'skill_cast_base',
 'cost':'skill_mana_cost_base', 'virtual':'skill_call',
}

def run(SB, config, pokes, hooks, commands):
    SB.C.set_fpu(('ps2','ps2g1','chop','nearest')[config//4], bool(config&2), bool(config&1))
    model = SB.Model(SB.RC.Memory())
    for addr,size,value in pokes: model.mem.poke(addr,size,value)
    for name,ret,mutation,throws in hooks:
        def hook(*args, ret=ret, mutation=mutation, throws=throws):
            if mutation: model.mem.poke(mutation,4,ret)
            if throws == 1: raise CallbackFailure()
            if throws == 2: raise Missing()
            return ret
        model.cb[name] = hook
    results, status = [], 'ok'
    try:
        for name,*args in commands:
            if name == 'manacost':
                skill,sid,level=args
                fn=getattr(model,'mana_cost_'+str(sid),model.skill_mana_cost_base)
                result=fn(skill,level)
            else: result = getattr(model, METHODS[name])(*args)
            results.append(0 if result is None else result & MASK)
    except CallbackFailure: status = 'callback'
    except Missing: status = 'unsupported'
    return state(model, results, status)

def scenarios(SB):
    rng = random.Random(3568)
    steps = (0,0x3f000000,0xbf000000,0x3f800000,0x3c23d70a,0x4b800000,0x80000000,0x00800000)
    levels = (0,1,2,19,20,21,-1,-20,32767,65535,0x7fffffff,0x80000000)
    spreads = (0,0x3e4ccccc,0x3f000000,0x3f800000,0x40000000,0xbf800000,0x40400000)
    for config in range(16):
      for i in range(160):
        pokes = [(S+0x60,4,T),(O+0xc,4,(0,0x20,0x40,0x20040)[i%4]),
                 (H,4,0 if i%3==0 else N),(N+0xc,4,0 if i%5==0 else O),
                 (L,4,0xdeadbeef),(L+4,4,0xabcddcba),
                 (SB.G_RAND_SEED,4,rng.getrandbits(32)),(SB.G_COSMETIC_SEED,4,rng.getrandbits(32)),
                 (SB.G_SEED3,4,rng.getrandbits(32)),(SB.G_PVP_MODE_A,4,int(i%7==0)),
                 (B,4,0xaabbccdd),(B+0x10,4,rng.getrandbits(32)),(O+0x44,4,rng.getrandbits(32))]
        for slot,body in BODIES.items(): pokes += [(T+8*slot,2,0),(T+8*slot+4,4,body)]
        for stat in range(5):
          # Int16 first/base and raw single steps are all authored, never retail tables.
          first = (-20,-1,0,1,10,20,32767)[(i+stat)%7]
          base = (-32768,-101,-1,0,1,5,6,101,32767)[(i+stat)%9]
          first2 = (-32768,-1,0,1,10,20)[(i+stat)%6]
          pokes += [(S+0xe+2*stat,2,first & 65535),(S+0x18+2*stat,2,base & 65535),
                    (S+0x24+4*stat,4,steps[(i+stat)%len(steps)]),
                    (S+0x38+2*stat,2,first2 & 65535),(S+0x42+2*stat,2,(-base)&65535),
                    (S+0x4c+4*stat,4,steps[(i+stat+2)%len(steps)])]
        lv = levels[i%len(levels)] & MASK
        # Avoid Python arbitrary-width conversion beyond the documented native domain.
        if lv == 0x7fffffff: lv = 0x7fff0000
        stat = i%5
        cap = (0,1,10,20,-1)[i%5] & MASK
        hooks = [('creature_bossRank',(-1,0,1,2,3)[i%5]&MASK,0,0)]
        commands = [('stat',S,stat,lv),('tier',S,stat,lv,cap),('for',S,stat,lv,0 if i%6==0 else O,cap),
                    ('handle',S,stat,lv,H,cap),('mana',S,lv,i),('archery',S,lv),('lifetap',S,lv),
                    ('life1',S,lv),('stance',S,lv),('resist',S,lv),('stun',S,0 if i%9==0 else O,lv),
                    ('range',S,stat,lv,i%2,L,L if i%11==0 else L+4,spreads[i%7]),
                    ('ctor',B,lv,O,S,T),('spellrand',B),('objectrand',O),('shared',),('cosmetic',),('third',),
                    ('adjust',S,lv,i),('can',S,O),('cast',S,lv,O,L,0),('cost',S,lv)]
        # Explicitly scripted virtual callback return, mutation, and failure cases.
        if i%10 in (1,2,3,4,5):
          slot = 14 if i%10==1 else 10
          pokes.append((T+8*slot+4,4,0x777000+slot))
          hooks.append(('Skill.v'+str(slot),(-12,0,1,5,6,9)[i%6]&MASK,
                        SB.G_RAND_SEED if i%10==3 else 0, 1 if i%10==4 else 2 if i%10==5 else 0))
        # Signed this adjustments must point at an authored duplicate table.
        if i%10==6:
          for slot in (10,11):
            pokes.append((T+8*slot,2,(-0x100)&65535))
          for a,size,v in list(pokes):
            if S+0xe <= a < S+0x60: pokes.append((a-0x100,size,v))
        sid=35+(i%34)
        body=SB.MANA_COST.get(sid,(BODIES[5],))[0]
        pokes.append((T+44,4,body))
        commands += [('manacost',S,sid,lv),('virtual',S,5,lv)]
        if i%10 in (4,5): commands.insert(0,('ctor',B,lv,O,S,T))
        commands += [('virtual',S,10,stat,lv),('virtual',S,11,stat,lv,cap),
                     ('virtual',S,12,stat,lv,O,cap),('virtual',S,13,stat,lv,H,cap),
                     ('virtual',S,14,lv,20),('virtual',S,15,lv,20),
                     ('virtual',S,8,stat,lv,i%2,L,L+4,spreads[i%7])]
        yield config,pokes,hooks,commands
    # Output pointers can alias the shared seed or Skill table. Ordered writes
    # must influence later draws/reads rather than computing both outputs first.
    for config in range(16):
      for lo,hi in ((SB.G_RAND_SEED,L),(L,SB.G_RAND_SEED),(SB.G_RAND_SEED,SB.G_RAND_SEED),
                    (S+0x24,S+0x18),(L,L)):
        pokes = [(S+0x60,4,T),(T+84,4,BODIES[10]),(S+0xe,2,1),(S+0x18,2,7),
                 (S+0x24,4,0),(SB.G_RAND_SEED,4,0x12345678)]
        yield config,pokes,[],[('range',S,0,1,1,lo,hi,0x40400000),('stat',S,0,2)]
    # Targeted sign-probe cases: the first random draw decides rounding even
    # when the second draw gives a bound with the opposite sign.
    for config in range(16):
      for seed in range(64):
        pokes = [(S+0x60,4,T),(T+84,4,BODIES[10]),(S+0xe,2,1),(S+0x18,2,7),
                 (S+0x24,4,0),(SB.G_RAND_SEED,4,seed)]
        yield config,pokes,[],[('range',S,0,1,1,L,L+4,0x40400000)]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--research-dir',type=pathlib.Path,required=True)
    p.add_argument('--output',type=pathlib.Path,required=True)
    args = p.parse_args()
    with tempfile.TemporaryDirectory(prefix='skills-b-pinned-') as d:
        for path,digest in PINS.items():
            data = subprocess.run(['git','-C',str(args.research_dir),'show',COMMIT+':'+path],
                                  check=True,capture_output=True).stdout
            if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('pin mismatch: '+path)
            if path in ('tools/ref_core.py','tools/ref_creature.py','tools/ref_arena_monsters.py','tools/ref_skills_b.py'):
                (pathlib.Path(d)/pathlib.Path(path).name).write_bytes(data)
        sys.path.insert(0,d)
        SB = importlib.import_module('ref_skills_b')
        rows = []
        for config,pokes,hooks,commands in scenarios(SB):
            rows.append('\t'.join((str(config),flat(pokes),flat(hooks),flat(commands),run(SB,config,pokes,hooks,commands))))
    data = ('\n'.join(rows)+'\n').encode('ascii')
    supported = set('Skill_canUse_base Skill_cast_base Skill_manaCost_base Skill_getStatRange Skill_getStat Skill_getStatTier2 Skill_getStatFor Skill_getStatForHandle Skill_adjustLevel Skill_manaFromStat4 Spell_rand GameObject_rand SkillArchery_getStat0Fraction SkillLifeTap_getStat0Fraction SkillLifeTap_getStat1 SkillStrongStance_getStat0 SkillNaturalResistance_getStat0Fraction SkillStunningStrike_getStat0ForTarget'.split())
    supported.update('Skill%d_manaCost'%sid for sid in SB.MANA_COST)
    unknown = supported - set(SB.FUNCTIONS)
    if unknown: raise ValueError('unknown supported functions: '+str(unknown))
    catalog = {str(key): value for key,value in SB.SKILLS.items()}

    args.output.parent.mkdir(parents=True,exist_ok=True)
    if args.output.suffix == '.gz':
        with args.output.open('wb') as file:
            with gzip.GzipFile(fileobj=file,mode='wb',mtime=0,filename='') as archive: archive.write(data)
    else: args.output.write_bytes(data)
    manifest = {'schema':1,'rows':len(rows),'research_commit':COMMIT,'references':PINS,
                'vectors_sha256':hashlib.sha256(data).hexdigest(),
                'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                'supported_functions':sorted(supported),'unsupported_functions':sorted(set(SB.FUNCTIONS)-supported),
                'skill_catalog':catalog}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print('Generated',len(rows),'complete-state cases')
if __name__=='__main__': main()
