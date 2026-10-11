#!/usr/bin/env python3
"""Execute immutable RTA terrain-query models on authored post-fixup Levels.

No retail bytes or native outputs are oracle inputs. Unported rays/pathfinding,
object push-out/touch and malformed host topology are outside this fixture scope.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import pathlib
import random
import subprocess
import sys
import types

REFERENCES={
 'core':{'commit':'41e847af5360724ab62d208594f8027ea3c1b686','path':'tools/ref_core.py','sha256':'0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
 'world':{'commit':'6831458ee7ee12bb166d1dce4eb8e4340095bc48','path':'tools/ref_world.py','sha256':'30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8'},
 'world_info':{'commit':'2f937fca823488bc97b9680fde9d957c3fa9907a','path':'tools/world_info.py','sha256':'2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5'},
}
REFERENCE_SHA256=hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
SPECIFICATION_EVIDENCE={'commit':'b6881f52457bd715058248c34d65888c797ba657','path':'docs/spec/world-queries.md','sha256':'1527e9f32e315ce72a9a2706fe39d3d7c65fd6fc568eceb4ec0629fd695c35c2'}
COMPOSITION_POLICY='rta-supplied-terrain-v1'
MODELS=('ps2','ps2g1','chop','nearest')
OPERATIONS={'collect':16,'height':24,'height_below':24,'height_point':24,'flag1':24,'sweep':32,'slide':16,'ground':16,'object_move':16,'piece':12,'mask':12}
LIMITS=[65536,65536,1048576,4194304,4194304,1048576,16777216]


def sc(value):
    if value is None:return '_'
    if isinstance(value,bool):return str(int(value))
    if isinstance(value,int):return str(value)
    if isinstance(value,str):return 's'+value.encode('utf-8').hex()
    if isinstance(value,(list,tuple)):return '['+','.join(map(sc,value))+']'
    raise TypeError('unexpected oracle value')


def load_references(research_dir):
    result={};prior=sys.modules.get('world_info')
    try:
        for name in ('world_info','world','core','specification'):
            pin=SPECIFICATION_EVIDENCE if name=='specification' else REFERENCES[name]
            source=subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],capture_output=True,check=True,timeout=30).stdout
            if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('immutable source hash mismatch: '+name)
            if name=='specification':continue
            m=types.ModuleType('world_queries_pinned_'+name);m.__file__='/__verified_models__/ref_'+name+'.py';sys.modules[m.__name__]=m
            oldpath=sys.path[:]
            try:exec(compile(source,m.__file__,'exec'),m.__dict__)
            finally:sys.path[:]=oldpath
            result[name]=m
            if name=='world_info':sys.modules[name]=m
    finally:
        if prior is None:sys.modules.pop('world_info',None)
        else:sys.modules['world_info']=prior
    return result


def arithmetic_probe(modules):
    """Independent original world vs original core arithmetic, all16 contexts."""
    w,c=modules['world'],modules['core'];rng=random.Random(0x47524944)
    edge=[0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x80800000,0x3effffff,0x3f000000,0x3f000001,0x3f800000,0xbf800000,0x3f800001,0x4b000001,0x7f7fffff,0x7f800000,0x7fffffff,0xff800000,0xffffffff]
    pairs=[(a,b) for a in edge for b in edge]+[(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(128)]
    digest=hashlib.sha256();count=0
    binary=(('add',w.fadd,c.add),('sub',w.fsub,c.sub),('mul',w.fmul,c.mul),('div',w.fdiv,c.div),('rsqrt',w.frsqrt,c.rsqrt),('lt',w.flt,c.lt),('le',w.fle,c.le),('min',w.fmin,c.fmin),('max',w.fmax,c.fmax))
    unary=(('sqrt',w.fsqrt,c.sqrt),('to_int',w.f2i,c.to_int),('neg',w.fneg,c.neg),('abs',w.fabs_,c.fabs))
    for config in range(16):
        w.set_fpu(MODELS[config//4],bool(config&2),bool(config&1));c.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
        for name,wfn,cfn in binary:
            for a,b in pairs:
                expected,actual=wfn(a,b),cfn(a,b)
                if expected!=actual:raise AssertionError(('arithmetic mismatch',config,name,a,b,expected,actual))
                digest.update(sc([config,name,a,b,expected]).encode());count+=1
        for name,wfn,cfn in unary:
            for a in edge:
                expected,actual=wfn(a),cfn(a)
                if expected!=actual:raise AssertionError(('arithmetic mismatch',config,name,a,expected,actual))
                digest.update(sc([config,name,a,expected]).encode());count+=1
        for a in (-(1<<40),-(1<<31),-65537,-1,0,1,65537,(1<<31)-1,1<<31,0xffffffff,1<<40):
            expected,actual=w.i2f(a),c.from_int(a)
            if expected!=actual:raise AssertionError(('integer conversion mismatch',config,a,expected,actual))
            digest.update(sc([config,'from_int',a,expected]).encode());count+=1
    return {'comparisons':count,'sha256':digest.hexdigest(),'scope':'Original pinned ref_world arithmetic vs original pinned ref_core, 16 configurations, raw edge Cartesian product and seeded raw pairs; no host floating point'}


def fixture(w,kind):
    """Eight small supplied post-fixup Levels; offsets are labels, not pointers."""
    level=w.Level()
    # Opaque path metadata is retained even though no path method runs.
    level.pg_x0=-3;level.pg_y0=7;level.pg_w=2;level.pg_h=1;level.path_cells=[0x12345678,0xfedcba98]
    if kind==0:return level
    level.tg_x0=level.tg_y0=-1;level.tg_w=level.tg_h=2
    heights=[0]*25;flags=[0]*25;lo=hi=0
    if kind==2:
        flags=[1 if i%5==3 else 2 if i%5==4 else 0 for i in range(25)]
    elif kind==3:
        heights=[(-2000,0,11,12,30)[i%5] for i in range(25)];lo=-2000;hi=30;flags=[i%8 for i in range(25)]
    elif kind==6:
        # Values are already piece-fixed: flag1 samples have height2000.
        heights=[2000 if i%6==0 else -4 for i in range(25)];flags=[1 if i%6==0 else 0 for i in range(25)];lo=-4;hi=2000
    else:
        heights=[4+(i%5) for i in range(25)];lo=4;hi=8
    field=w.HeightField(-2,-2,5,5,lo,hi,[(h<<4)|f for h,f in zip(heights,flags)],0x400)
    unused=w.HeightField(11,-12,1,1,-2000,-2000,[-32000],0x400)
    level.heightfields=[field,unused]
    cosine,sine=(0,32767) if kind==4 else (23170,-23170) if kind==5 else (32767,0)
    mirror=1 if kind==4 else 0
    first=w.Element(77,-16,-16,16,16,field,4|mirror,0,0,0,cosine,sine)
    second=w.Element(77,-16,-16,16,16,field,4,4,0,3,32767,0)
    ignored=w.Element(-9,-16,-16,16,16,unused,2|4,0,0,0,32767,0)
    level.elements=[first,second,ignored]
    level.grid=[[0,1,0,2],[-3,1],[1,0],[-1,0,1]]
    level.pieces=[0,1,-2]
    if kind==5:
        # Cached rectangle is an independently supplied runtime field.
        first.f_max_x=w.i2f(1);first.f_min_y=w.i2f(-1)
    if kind==7:
        level.stamp=0xffffffff;first.stamp=0;second.stamp=0xffffffff;ignored.stamp=7
    return level


def snapshot(level):
    fields=level.heightfields
    data=[level.stamp,[level.tg_x0,level.tg_y0,level.tg_w,level.tg_h],copy.deepcopy(level.grid),
          [[f.x0,f.y0,f.w,f.h,f.lo,f.hi,list(f.samples),f.offset] for f in fields],
          [[e.index,e.min_x,e.min_y,e.max_x,e.max_y,fields.index(e.hf),e.flags,e.x,e.y,e.z,e.cos,e.sin,e.stamp,e.f_min_x,e.f_min_y,e.f_max_x,e.f_max_y]for e in level.elements],
          list(level.pieces),[level.pg_x0,level.pg_y0,level.pg_w,level.pg_h],list(level.path_cells)]
    return [data,list(LIMITS)]


def run_case(modules,operation,config,variant):
    w=modules['world'];w.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
    kind=variant%8;style=variant//8;level=fixture(w,1 if operation=='collect' and kind==0 else kind);I=w.i2f
    pos=[I(0),I(0),I(4)];radius=I(6);delta=[I(16),I(3),0]
    if style==1:pos=[0xc0800001,I(4),I(12)];radius=I(4);delta=[I(-12),I(7),I(-3)]
    elif style==2:pos=[I(160),I(-160),0] if operation=='height_point' else [0,0,0];radius=0;delta=[0,0,0]
    elif style==3:pos=[I(4),I(1),I(30)];radius=I(5);delta=[I(2),I(-6),I(13)]
    result=None;status='ok'
    try:
        if operation=='collect':
            if variant!=0 and kind!=7:level.stamp=1
            bounds=[I(-8),I(-8),I(8),I(8)] if style==0 else [I(-161),I(-1),I(160),I(1)]
            limit=(0,1,2,3,64,256,2,3)[kind]
            found=w.collect_elements(level,*bounds,limit);result=[level.elements.index(e) for e in found]
            if variant==13:
                # Caller does not advance the stamp: second query must deduplicate.
                result=[result,[level.elements.index(e)for e in w.collect_elements(level,*bounds,limit)]]
                w._next_stamp(level)
                result.append([level.elements.index(e)for e in w.collect_elements(level,*bounds,limit)])
        elif operation=='height':result=w.terrain_height(level,radius,pos)
        elif operation=='height_below':result=w.terrain_height_below(level,radius,pos)
        elif operation=='height_point':result=w.terrain_height_at_point(level,pos)
        elif operation=='flag1':result=w.terrain_test_flag1(level,radius,pos)
        elif operation=='sweep':
            mask=(1,3,5,0)[style];want=bool(variant&1)
            result=w.terrain_sweep_circle(level,radius,pos,delta,mask,want)
        elif operation=='slide':result=w.world_slide_move(level,radius,pos,delta,3 if style==0 else 5)
        elif operation=='ground':
            vz=I(-20) if style==0 else I(60);stand=w.K_MINUS_100000 if style==0 else I(18)
            result=w.ground_step(level,radius,pos,vz,stand)
        elif operation=='object_move':
            velocity=[I(600),I(120),I(-10)] if style==0 else [0,0,0]
            result=w.object_move(level,radius,pos,velocity,I(-20),(0,0x20,0x40,0x60)[kind%4])
        elif operation=='piece':
            # A common nonempty shared-field fixture; every intermediate return is observable.
            level=fixture(w,6);piece=(-1,0,1,2,3,-2,0,1,2,0,1,0)[variant]
            value=(0x3fc00000,0xbfc00000,0x7fffffff,0xffffffff)[variant%4]
            result=[level.get_piece_height(piece)]
            level.set_piece_collision(piece,bool(variant&1));level.set_piece_height(piece,value);result.append(level.get_piece_height(piece))
            result.append(w.terrain_height_at_point(level,[0,0,0]))
            if variant==11:
                # A legitimate first mutation remains after an out-of-range later operation.
                level.set_piece_height(0,I(17));level.set_piece_collision(99,True)
        elif operation=='mask':result=w.mover_mask((0,0x20,0x40,0x60,0x80,0x10000000,0xffffffff,0x20000,0x21,0x41,0x61,0x80000000)[variant])
        else:raise ValueError('unknown operation')
    except (IndexError,RuntimeError) as e:
        status=type(e).__name__;result=None
    return status,[result,snapshot(level)]


def generate(research_dir,output):
    modules=load_references(research_dir);probe=arithmetic_probe(modules);output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end with .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=0;size=0;maxline=0
    with output.open('wb')as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0)as f:
        header=b'# world_queries_vectors_v1\n';f.write(header);vh.update(header);size+=len(header)
        for name,variants in OPERATIONS.items():
            for config in (0,) if name=='mask' else range(16):
                for variant in range(variants):
                    status,value=run_case(modules,name,config,variant);again=run_case(modules,name,config,variant)
                    if (status,sc(value))!=(again[0],sc(again[1])):raise AssertionError('fresh case replay differs')
                    line=('\t'.join((name,str(config),f'{number};{variant}',status,sc(value)))+'\n').encode('ascii');f.write(line);vh.update(line);th.update(line);size+=len(line);maxline=max(maxline,len(line))
                    for b in line:fnv=((fnv^b)*1099511628211)&0xffffffffffffffff
                    counts[name]+=1;number+=1
    if size>64*1024*1024 or maxline>1024*1024:raise AssertionError('fixture exceeds transport budget')
    manifest={'schema_version':1,'suite':'world-queries','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'composition_policy':COMPOSITION_POLICY,'specification_evidence':SPECIFICATION_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'recipe_configurations':{n:[0] if n=='mask' else list(range(16))for n in OPERATIONS},'recipe_variants':{n:list(range(v))for n,v in OPERATIONS.items()},'uncompressed_bytes':size,'maximum_line_bytes':maxline,'arithmetic_probe':probe}
    manifest['composition_contract']={'scope':'RTA terrain collection, height, point, flag1, sweep, slide, ground, mask, isolated object move and piece methods; no rays/pathfinding/file parser/Session integration','inputs':'Eight synthetic caller-supplied post-fixup Levels; shared field identity distinct from offset and element labels; no original bytes','oracle':'Original immutable Python bodies, executed twice from fresh Level instances; no native expected-output input or host-float gameplay arithmetic','snapshot':'All Level fields, heightfields, alias indices, element cachedbounds/stamps, grid/pieces/path metadata and explicit native host limits','bounds':'Native malformed-topology/work-budget/owned-lifetime refusals are separate hand tests, not original Python behavior; generated IndexError preserves actual original mutation prefix'}
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8',newline='\n');return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/world_queries.tsv.gz'));a=p.parse_args();print(json.dumps(generate(a.research_dir,a.output),sort_keys=True))
if __name__=='__main__':main()
