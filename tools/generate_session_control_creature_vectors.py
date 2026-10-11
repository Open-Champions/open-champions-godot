#!/usr/bin/env python3
"""Pinned original creature movement behind explicit authored single-peer controls."""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import os
import pathlib
import sys
import tempfile
import types
sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PINS = {
 'session_creature': {'path':'tools/generate_session_creature_vectors.py','sha256':'561310e62d8dc7ccf7488193298fa33f0c7703cbc0ff495dbca22ba0825d152d'},
 'session_control': {'path':'tools/generate_session_control_vectors.py','sha256':'5f1c66378e6af01ba29c8479a21c945504d916f0370b65a72af9856b6ace10b5'}}
def load_helper(name):
    pin=PINS[name];path=ROOT/pin['path'];data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=pin['sha256']:raise ValueError('immutable authored helper mismatch: '+name)
    module=types.ModuleType('control_creature_'+name);module.__file__=str(path)
    sys.modules[module.__name__]=module;exec(compile(data,str(path),'exec'),module.__dict__)
    return module
CREATURE=load_helper('session_creature');CONTROL=load_helper('session_control')
BASE=CREATURE.BASE
REFERENCES=CREATURE.REFERENCES;REFERENCE_SHA256=CREATURE.REFERENCE_SHA256
HELPERS={**CREATURE.HELPERS,**PINS}
COMPOSITION_POLICY='rta-session-control-creature-v1'
SCENARIOS=('movement','edges','pause','focus','reset','limit','stop','supplied','invalid_input')
FPU_SCENARIOS={'movement','edges','supplied'}
VARIANTS={'movement':(0,1,2,3),'edges':(0,1),'pause':(0,),'focus':(0,),'reset':(0,),'limit':(0,),'stop':(0,),'supplied':(0,1,2),'invalid_input':(0,1)}

def commands(name,variant):
    if name=='supplied':return [('advance',17),('advance',17),('advance',0),('advance',0),('reset',0),('advance',16),('advance',0)]
    if name=='invalid_input':return [('advance',17),('advance',32 if variant==0 else 0xffffffff),('advance',0),('pause',1),('advance',32),('pause',0),('advance',16),('advance',0)]
    return CONTROL.commands(name,variant)

def terrain(world,variant):
    level=CONTROL.default_level(world)
    field=level.heightfields[0]
    if variant<2:
        height=3 if variant==0 else -3
        field.samples[:]=[height*16]*len(field.samples);field.lo=field.hi=height
    else:
        # Plain zero floor with one authored blocked column next to origin.
        for y in range(field.h):field.samples[y*field.w+65]=1
        level.elements[0].flags=4
    return level

def make_oracle(modules,helpers,config,variant,name):
    # Substitute only the authored Session factory dependency. Every immutable
    # original body and the pinned creature service adapter remain unchanged.
    saved={}
    def factory(_modules,_helpers,_config,_variant,_name,_local):
        o=CONTROL.make_oracle(_modules,_helpers,_config,0,_name,0)
        o.a.flags|=0x20;o.hp.objects[0].flags=o.a.flags;o.project_hp()
        if name=='supplied':o.level=terrain(modules['world'],variant)
        saved['task']=o.task
        return o
    globals_=dict(CREATURE.make_oracle.__globals__)
    globals_['BASE']=types.SimpleNamespace(make_oracle=factory)
    adapt=types.FunctionType(CREATURE.make_oracle.__code__,globals_,argdefs=CREATURE.make_oracle.__defaults__)
    o=adapt(modules,helpers,config,0,name)
    o.task=saved['task']
    return o

class Control(CONTROL.Control):
    def __init__(self,modules,helpers,config,name,variant):
        self.variant=variant
        super().__init__(modules,helpers,config,name)
    def reset(self):
        if self.o is not None:self.o.close()
        self.max_ready=2 if self.name=='limit' else 512
        self.o=make_oracle(self.modules,self.helpers,self.config,self.variant,self.name);self.o.control=self
        self.initial_terrain=copy.deepcopy(self.helpers['world_queries'].snapshot(self.o.level))
        self.held=0;self.pending=[];self.cutoff=1;self.submissions=self.ticks=self.hits=0
        self.paused=self.stopped=self.failed=self.busy=False;self.phase='idle';self.error=''
    def action(self,action,value):
        if action=='advance'and value&~31:raise CONTROL.Refusal('invalid command')
        return super().action(action,value)
    def observation(self):return super().observation()+[1,self.max_ready]
    def snapshot(self):
        value=super().snapshot();value[0].append(1);value[2]=self.name=='supplied';value[6]=0
        return value

def run_scenario(modules,helpers,config,variant,name):
    c=Control(modules,helpers,config,name,variant)
    try:
        initial=copy.deepcopy(c.snapshot());before=initial;steps=[];hp=helpers['session_hp'];initial_seed=c.o.w.shared.state
        for action,value in commands(name,variant):
            refused=False
            try:observation=c.action(action,value)
            except CONTROL.Refusal:
                refused=True;observation=c.observation()
            after=copy.deepcopy(c.snapshot());delta=hp.snapshot_delta(before,after)
            if hp.apply_delta(before,delta)!=after:raise AssertionError('lossless control delta failed')
            if refused:
                if after!=before:raise AssertionError('invalid input changed authoritative state')
                observation=['refused',observation]
            steps.append([[action,value],copy.deepcopy(observation),delta]);before=after
        if name=='edges':
            assert c.hits==2 and c.o.hp.objects[0].hp==0x42c00000,'100 ->98 ->96'
        if name=='movement'and variant==0:assert steps[1][1][0]==[0x3f7fffff,0,0],'literal creature displacement'
        if name=='movement'and variant==2:assert steps[1][1][0]==[0x3f7fffff,0xbf7fffff,0],'independent diagonal axes'
        if name=='movement'and variant==3:assert c.o.a.pos==[0,0,0],'opposed axes cancel'
        if c.o.w.shared.state!=initial_seed or c.o.hp.world.seed.draws or c.o.movement.draws:raise AssertionError('control RNG domains changed')
        assert c.o.network.num_peers==1
        return [initial,steps]
    finally:c.close()


def generate(research_dir,output):
    modules=BASE.load_references(research_dir);helpers=BASE.load_helpers();sc=helpers['session_objects'].sc
    output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);manifest_path=output.with_name(output.name[:-7]+'.json')
    fd,name=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent);temp=pathlib.Path(name);manifest_temp=None
    vh,th=hashlib.sha256(),hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=size=longest=0
    try:
        with os.fdopen(fd,'wb')as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0)as stream:
                header=b'# session_control_creature_vectors_v1\n';stream.write(header);vh.update(header);size+=len(header)
                for name in SCENARIOS:
                    for config in range(16)if name in FPU_SCENARIOS else(0,):
                        for variant in VARIANTS[name]:
                            first=sc(run_scenario(modules,helpers,config,variant,name));second=sc(run_scenario(modules,helpers,config,variant,name))
                            if first!=second:raise AssertionError('two fresh reference executions differ')
                            row=('\t'.join([name,str(config),f'{number};{variant}','ok',first])+'\n').encode('ascii')
                            stream.write(row);vh.update(row);th.update(row);size+=len(row);longest=max(longest,len(row));number+=1;counts[name]+=1
                            for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'session-control-creature','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'composition_policy':COMPOSITION_POLICY,'helpers':HELPERS,'ordering_evidence':BASE.ORDERING_EVIDENCE,'specification_evidence':BASE.SPECIFICATION_EVIDENCE,'creature_specification_evidence':BASE.CREATURE_SPECIFICATION_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'recipe_configurations':{n:list(range(16))if n in FPU_SCENARIOS else[0]for n in SCENARIOS},'recipe_variants':{n:list(VARIANTS[n])for n in SCENARIOS},'expanded_bytes':size,'max_line_bytes':longest,'composition_contract':{'scope':'Explicit dry singleton base Creature_moveBy behind authored local command queue; not original Player_move or Norrath.','state':'Complete controller config including movement policy and active cap, full Session/World/HP/Level, all creature fields and typed callbacks; borrowed memory only in HP. Lossless deltas replayed every action.','input':'Actual immutable Machine(1,0) and original net_main/tasks/movement/terrain/HP; no fabricated ready packets or native expected-state inputs.','authority':'Pinned authored control policy and Session-creature adapter composed by substituting only the factory dependency. No copied RNG state and no original-model body replacements.','boundaries':'Native-only invalid topology, workload budgets and unsafe identity refusal remain separate API tests; oracle covers input refusal without mutation and terminal ready-tick cap.','determinism':'Two fresh executions percase and three separate complete generations; atomic temporary files, manifest published last.'}}
        fd2,name=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent);manifest_temp=pathlib.Path(name)
        with os.fdopen(fd2,'w',encoding='ascii',newline='\n')as stream:json.dump(manifest,stream,sort_keys=True,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.replace(temp,output);os.replace(manifest_temp,manifest_path);return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--research-dir',required=True,type=pathlib.Path);parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_control_creature.tsv.gz'));args=parser.parse_args();print(json.dumps(generate(args.research_dir,args.output),sort_keys=True))
if __name__=='__main__':main()
