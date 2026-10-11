#!/usr/bin/env python3
"""Independent terrain-file expectations from immutable original Python parsers.

All input bytes are authored with the pinned model builder or explicit mutations.
No native output, original asset, working-tree model or decompiler text is input.
"""
import argparse
import collections
import copy
import gzip
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import types

REFERENCES={
 'core':{'commit':'41e847af5360724ab62d208594f8027ea3c1b686','path':'tools/ref_core.py','sha256':'0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
 'world':{'commit':'6831458ee7ee12bb166d1dce4eb8e4340095bc48','path':'tools/ref_world.py','sha256':'30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8'},
 'world_info':{'commit':'2f937fca823488bc97b9680fde9d957c3fa9907a','path':'tools/world_info.py','sha256':'2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5'},
}
REFERENCE_SHA256=hashlib.sha256(json.dumps(REFERENCES,sort_keys=True,separators=(',',':')).encode()).hexdigest()
HELPERS={'world_queries':{'path':'tools/generate_world_queries_vectors.py','sha256':'293626ad96ed54022dbc1bb30953ba0b0c91b562d70ddae53d40695cd263c1cf'}}
SPECIFICATION_EVIDENCE={'commit':'b6881f52457bd715058248c34d65888c797ba657','path':'docs/spec/world-queries.md','sha256':'1527e9f32e315ce72a9a2706fe39d3d7c65fd6fc568eceb4ec0629fd695c35c2'}
FORMAT_EVIDENCE={'commit':'9787e606c5c1f723dcef62701c67e607c5cff617','path':'docs/formats/world.md','sha256':'c76f22f8f2ff7de6c3e0e62ab38a50040d652a7530a1ae14bae1b945956c935f'}
COMPOSITION_POLICY='rta-terrain-file-v1'
MODELS=('ps2','ps2g1','chop','nearest')
LEVEL_LIMITS=[65536,65536,1048576,4194304,4194304,1048576,16777216]
PARSE_LIMITS=[67108864,65535,10000,8388608,16777216,LEVEL_LIMITS]


def load_helper():
    pin=HELPERS['world_queries'];path=pathlib.Path(__file__).resolve().parents[1]/pin['path'];source=path.read_bytes()
    if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('authored helper hash mismatch')
    m=types.ModuleType('terrain_file_helper');m.__file__=str(path);sys.modules[m.__name__]=m;exec(compile(source,str(path),'exec'),m.__dict__);return m


def load_references(research_dir):
    out={};old=sys.modules.get('world_info')
    try:
        for name in ('world_info','world','core','specification','format'):
            pin=SPECIFICATION_EVIDENCE if name=='specification'else FORMAT_EVIDENCE if name=='format'else REFERENCES[name]
            data=subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],capture_output=True,check=True,timeout=30).stdout
            if hashlib.sha256(data).hexdigest()!=pin['sha256']:raise ValueError('immutable reference hash mismatch: '+name)
            if name not in REFERENCES:continue
            m=types.ModuleType('terrain_file_pinned_'+name);m.__file__='/__verified_models__/ref_'+name+'.py';sys.modules[m.__name__]=m;path=sys.path[:]
            try:exec(compile(data,m.__file__,'exec'),m.__dict__)
            finally:sys.path[:]=path
            out[name]=m
            if name=='world_info':sys.modules[name]=m
    finally:
        if old is None:sys.modules.pop('world_info',None)
        else:sys.modules['world_info']=old
    return out


def u32(data,offset):return struct.unpack_from('<I',data,offset)[0]
def put(data,offset,fmt,value):
    out=bytearray(data);struct.pack_into('<'+fmt,out,offset,value);return bytes(out)


def base_file(w,pieces=(),even=False):
    fields=[(-2,-1,4,3,[-32000,-31999,-17,-16,-1,0,1,2,7,15,16,32767],-2000,2047),
            (1,-1,2,2,[-32,-18,0,34]if even else[-32,-17,0,33],-123,321)]
    elements=[(1,0x10004,7,-9,12,-32768,1234,-16,-16,16,16),
              (0,0x80000005,-3,4,-12,23170,-23170,-16,-16,16,16),
              (1,6,12,-13,14,32767,0,-16,-16,16,16)]
    return w.build_world_file((-1,-1),(2,2),fields,elements,path=(-3,7,2,1,[0x12345678,0xfedcba98]),pieces=pieces)


def query_file(w,variant):
    heights=[0]*25;flags=[0]*25
    if variant==1:heights=[0 if i%5<3 else 20 for i in range(25)]
    if variant==2:flags=[1 if i%5==3 else 2 if i%5==4 else 0 for i in range(25)]
    if variant==3:heights=[-4+(i%5)for i in range(25)]
    if variant==4:flags=[1 if i%6==0 else 0 for i in range(25)]
    if variant==5:heights=[-2000]*25
    cosine,sine=(23170,-23170)if variant==3 else(32767,0)
    elements=[(0,5 if variant==3 else 4,0,0,0,cosine,sine,-16,-16,16,16),
              (0,4,4,0,3,32767,0,-16,-16,16,16)]
    return w.build_world_file((-1,-1),(2,2),[(-2,-2,5,5,[(a<<4)|b for a,b in zip(heights,flags)])],elements,pieces=(0,0)if variant==4 else())


def cases(w):
    """Names document inputs; the native runner needs only operation/config/bytes."""
    base=base_file(w);elem=u32(base,0x20);grid=u32(base,0x18);chunk=u32(base,0x24);field=u32(base,elem+12)
    good=[('empty',w.build_world_file((0,0),(0,0),[],[])),('signed_alias',base),('piece_shared',base_file(w,(0,0))),('piece_noflag',base_file(w,(0,2),True))]
    changed=bytearray(base)
    for i in range(3):struct.pack_into('<I',changed,elem+36*i+8,0xffffffff-i)
    good.append(('file_stamps',bytes(changed)))
    changed=bytearray(base);tail=len(changed);changed+=struct.pack('<5h',2,0,2,1,-1);struct.pack_into('<I',changed,0x78,tail)
    for i,v in enumerate((0,2,4,8)):struct.pack_into('<I',changed,grid+4*i,v)
    good.append(('ordered_shared_lists',bytes(changed)))
    changed=bytearray(base);tail=len(changed);changed+=b'\xff\xff';struct.pack_into('<I',changed,0x78,tail)
    for i in range(4):struct.pack_into('<I',changed,grid+4*i,0)
    good.append(('empty_lists',bytes(changed)))
    changed=bytearray(base);changed+=b'\x5a'if len(changed)%2==0 else b'\x5a\x5a';offset=len(changed);changed+=base[field:field+28]
    for i in (0,2):struct.pack_into('<I',changed,elem+36*i+12,offset)
    good.append(('unaligned_shared_field',bytes(changed)))
    changed=bytearray(base);offset=len(changed);changed+=base[field:field+28];struct.pack_into('<I',changed,elem+36+12,offset)
    good.append(('equal_distinct_fields',bytes(changed)))
    changed=bytearray(base);changed+=b'\x5a'if len(changed)%2==0 else b'\x5a\x5a';offset=len(changed);changed+=base[elem:elem+108];struct.pack_into('<I',changed,0x20,offset)
    good.append(('unaligned_elements',bytes(changed)))
    good.append(('path_absent',put(put(put(base,0x30,'I',0),0x34,'I',0),0x38,'I',0)))
    changed=bytearray(base);tree=len(changed);changed+=struct.pack('<4H',0x4567,0,0,0);struct.pack_into('<I',changed,4,tree)
    good.append(('optional_tree_marker',bytes(changed)))
    good.append(('trailing_bytes',base+b'authored-trailer\x00\xff'))
    for name,data in good:yield 'parse',name,0,data,'ok'
    for v in range(6):
        data=query_file(w,v)
        for config in range(16):yield 'queries','query_'+str(v),config,data,'ok'
    bad=[('short_header',base[:0x83]),('zero_chunks',put(base,0,'I',0)),('chunk_table_oob',put(base,0x24,'I',len(base))),
         ('chunk_payload_oob',put(base,chunk,'I',len(base))),('chunk_nan_bound',put(base,chunk+8,'I',0x7fc00000)),
         ('texture_negative_size',put(base,0x68,'I',0xbf800000)),('texture_nan_size',put(base,0x68,'I',0x7f800001)),
         ('texture_range_reversed',put(base,0x58,'i',1)),('texture_table_oob',put(base,0x64,'I',len(base))),
         ('terrain_table_oob',put(base,0x18,'I',len(base))),('terrain_tail_oob',put(base,0x78,'I',len(base))),
         ('element_table_oob',put(base,0x20,'I',len(base))),('field_header_oob',put(base,elem+12,'I',len(base)-19)),
         ('field_zero_width',put(base,field+8,'I',0)),('field_sample_extent',put(base,field+8,'I',4096)),
         ('path_table_oob',put(base,0x38,'I',len(base))),('automap_offset_oob',put(base,0x80,'I',len(base))),
         ('bitgrid_extent',put(put(put(base,0x44,'I',32),0x48,'I',32),0x4c,'I',len(base))),
         ('bad_tree_marker',put(base,4,'I',elem))]
    piece=base_file(w,(0,));po=u32(piece,0x54)
    bad.extend([('piece_table_oob',put(piece,0x54,'I',len(piece))),('piece_first_negative',put(piece,po,'i',-1)),('piece_element_oob',put(piece,po+24,'h',3))])
    changed=bytearray(base);tail=len(changed);changed+=struct.pack('<h',3);struct.pack_into('<I',changed,0x78,tail)
    for i in range(4):struct.pack_into('<I',changed,grid+4*i,0)
    bad.append(('list_element_oob',bytes(changed)))
    changed[-2:]=struct.pack('<h',0);bad.append(('list_unterminated',bytes(changed)))
    for name,data in bad:yield 'parse',name,0,data,'FormatError'


def run_queries(w,level):
    I=w.i2f;position=[I(0),I(0),I(4)];radius=I(6);delta=[I(16),I(3),0];results=[]
    w._next_stamp(level);results.append([level.elements.index(e)for e in w.collect_elements(level,I(-8),I(-8),I(8),I(8),64)])
    results.append([level.elements.index(e)for e in w.collect_elements(level,I(-8),I(-8),I(8),I(8),64)])
    results += [w.terrain_height(level,radius,position),w.terrain_height_below(level,radius,position),w.terrain_height_at_point(level,position),w.terrain_test_flag1(level,radius,position)]
    results.append(w.terrain_sweep_circle(level,radius,position,delta,3,True))
    results.append(w.world_slide_move(level,radius,position,delta,3))
    results.append(w.ground_step(level,radius,position,I(-2)))
    results.append(w.object_move(level,radius,position,[I(120),I(30),0],I(-2),32))
    if level.pieces:
        level.set_piece_collision(0,False);level.set_piece_height(0,I(-7));results.append(level.get_piece_height(0));results.append(w.terrain_height(level,radius,position));level.set_piece_collision(0,True);results.append(w.terrain_height(level,radius,position))
    return results


def execute(modules,helper,operation,config,data):
    w=modules['world'];w.set_fpu(MODELS[config//4],bool(config&2),bool(config&1))
    try:
        level=w.Level(bytes(data));initial=helper.snapshot(level)
        results=run_queries(w,level)if operation=='queries'else[]
        return 'ok',[copy.deepcopy(PARSE_LIMITS),initial,results,helper.snapshot(level)]
    except modules['world_info'].FormatError:return 'FormatError',None


def generate(research_dir,output):
    helper=load_helper();modules=load_references(research_dir);output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);manifest_path=output.with_name(output.name[:-7]+'.json')
    vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();groups=[];size=0;longest=0;maxinput=0
    fd,temp=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent);temp=pathlib.Path(temp);manifest_temp=None
    try:
        with os.fdopen(fd,'wb')as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0)as zipped:
                header=b'# terrain_file_vectors_v1\n';zipped.write(header);vh.update(header);size+=len(header)
                for number,(op,name,config,data,expected_status)in enumerate(cases(modules['world'])):
                    first=execute(modules,helper,op,config,data);second=execute(modules,helper,op,config,data)
                    if first!=second:raise AssertionError('fresh parse disagreement: '+name)
                    if first[0]!=expected_status:raise AssertionError(('unexpected category',name,first[0]))
                    line=('\t'.join((op,str(config),str(number)+';'+data.hex(),first[0],helper.sc(first[1])))+'\n').encode('ascii')
                    zipped.write(line);vh.update(line);th.update(line);size+=len(line);longest=max(longest,len(line));maxinput=max(maxinput,len(data))
                    for byte in line:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[op]+=1;groups.append({'id':number,'case':name,'operation':op,'config':config,'input_sha256':hashlib.sha256(data).hexdigest(),'input_bytes':len(data),'status':first[0]})
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'terrain-file','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'helpers':HELPERS,'composition_policy':COMPOSITION_POLICY,'specification_evidence':SPECIFICATION_EVIDENCE,'format_evidence':FORMAT_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':len(groups),'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'cases':groups,'expanded_bytes':size,'max_line_bytes':longest,'max_input_bytes':maxinput,'parse_limits':PARSE_LIMITS}
        manifest['composition_contract']={'scope':'Explicit RTA unrelocated terrain projection only; no fullworld/render/script/player/Norrath loader','input':'Pinned original build_world_file synthetic bytes with explicit integer rectangles plus authored malformed/alias/unaligned mutations; TSV actual hex drives native decoder','oracle':'Original immutable Level(bytes) invokes original world_info.World and original fixups; complete postparse initial and postquery snapshots with all Limits and aliases; no native expected-output input','arithmetic':'Six binary query fixtures all16 contexts; existing immutable core is provenance dependency of previously verified arithmetic contract, not an alternative parser oracle','refusals':'Only original world_info.FormatError cases in corpus; additional native topology/resource safety boundaries covered separately','determinism':'Every case twice with fresh original Level; deterministic gzip; complete temporary file flushed/fsynced then atomic replace; manifest published last'}
        fd2,mp=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent);manifest_temp=pathlib.Path(mp)
        with os.fdopen(fd2,'w',encoding='utf-8',newline='\n')as f:json.dump(manifest,f,indent=2,sort_keys=True);f.write('\n');f.flush();os.fsync(f.fileno())
        os.replace(temp,output);os.replace(manifest_temp,manifest_path)
        return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--research-dir',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/terrain_file.tsv.gz'));a=p.parse_args();print(json.dumps(generate(a.research_dir,a.output),sort_keys=True))
if __name__=='__main__':main()
