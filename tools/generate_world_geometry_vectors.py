#!/usr/bin/env python3
"""Authored WORLD geometry cases evaluated by immutable published world_info.

Binary64 coordinates are presentation data, never simulation inputs. Extra raw
VIF inventory observes supplied bytes; decoded strips/transforms/triangles come
only from original World methods. No live research imports or private assets.
"""
import argparse
import collections
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

REFERENCES={'world_info':{'commit':'2f937fca823488bc97b9680fde9d957c3fa9907a','path':'tools/world_info.py','sha256':'2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5'}}
REFERENCE_SHA256=REFERENCES['world_info']['sha256']
HELPERS={}
SPECIFICATION_EVIDENCE={'commit':'abf07ea411ac78e4cacb1da3e85b388a1839928a','path':'docs/spec/render-pipeline.md','sha256':'3eb890d80325c7b04a584de04af8360fc8f4c0e412d3a21656b4e99db570a04b'}
FORMAT_EVIDENCE={'commit':'f2b996dded898f1b0b558b901cab875f775cfdc7','path':'docs/formats/world.md','sha256':'81a7e067e859eaaae171d0422b5388510691d289ae37d6e42a25e5549d24cd20'}
COMPOSITION_POLICY='rta-world-geometry-v1'
LIMITS=[67108864,65535,65535,262144,2000000,4000000,4000000,4000000,4000000,4000000,4000000]


def load_references(research_dir):
    sys.dont_write_bytecode=True
    module=None
    for name,pin in [('world_info',REFERENCES['world_info']),('specification',SPECIFICATION_EVIDENCE),('format',FORMAT_EVIDENCE)]:
        source=subprocess.run(['git','-C',str(research_dir),'show',pin['commit']+':'+pin['path']],capture_output=True,check=True,timeout=30).stdout
        if hashlib.sha256(source).hexdigest()!=pin['sha256']:raise ValueError('immutable source hash mismatch: '+name)
        if name=='world_info':
            module=types.ModuleType('verified_world_geometry_reference');module.__file__='/__verified_models__/world_info.py'
            exec(compile(source,module.__file__,'exec'),module.__dict__)
    return module


def sc(value):
    if value is None:return '_'
    if isinstance(value,bool):return '1'if value else'0'
    if isinstance(value,int):return str(value)
    if isinstance(value,(list,tuple)):return '['+','.join(map(sc,value))+']'
    raise TypeError(type(value))
def bits(value):return struct.unpack('<Q',struct.pack('<d',value))[0]
def command(cmd,num=0,imm=0):return struct.pack('<I',(cmd<<24)|(num<<16)|imm)
def unpack(cmd,rows,fmt,unsigned=False):
    flat=[v for row in rows for v in row];body=struct.pack('<'+str(len(flat))+fmt,*flat)
    return command(cmd,len(rows)&255,0x4000 if unsigned else 0)+body+b'\0'*((-len(body))%4)


def packet(nreg=3,style=0,uv=True):
    """Three vertices plus optional ordered copies/restarts; no retail bytes."""
    vertices=[(-12,4,-8),(20,-16,12),(8,24,0)]
    normals=[(-127,0,0),(0,127,-1),(3,-4,126)]
    loc=[(nreg,0,nreg),(nreg,0,nreg)]
    slots=3
    if style==1: # row copy precedes placement; extra copy occurs last
        slots=5;loc += [(0x8000,0,0),(0x8000|nreg,0,0),(3*nreg,0,2*nreg)]
        extra=[(1,0,0,0),(nreg,4*nreg,0,0)]
    elif style==2: # restart at final position removes the only triangle
        loc += [(0x8000,0,0),(0x8000|nreg,0,0),(0x8000|2*nreg,0,0)];extra=[(0,0,0,0)]
    elif style==3: # repeated vertex stitching, then one real triangle
        slots=5;loc += [(0x8000,0,0),(0x8000|nreg,0,2*nreg),(4*nreg,nreg,3*nreg)];extra=[(0,0,0,0)]
    else:loc += [(0x8000,0,0),(0x8000|nreg,0,0),(2*nreg,0,0)];extra=[(0,0,0,0)]
    tags=[(slots|0x8000,(nreg<<28)|(0x15c<<15),0x413 if nreg==3 else 0x41f3,0)]
    if nreg==4:tags.append((slots|0x8000,(4<<28)|(0x35c<<15),0x413f,0))
    out=b'\0'*12+command(0x04,imm=3)+command(0x05,imm=1)+unpack(0x69,vertices,'h')+command(0x05)
    out+=unpack(0x6a,normals,'b')+unpack(0x69,loc,'H',True)+unpack(0x6d,extra,'H',True)+unpack(0x6c,tags,'I',True)
    out+=command(0x14,imm=0x46 if nreg==3 else 0x48)
    if uv:out+=command(0x01,imm=(1<<8)|nreg)+unpack(0x65,[(i*113,65535-i*17)for i in range(slots)],'H',True)+command(0x01,imm=0x101)
    out+=command(0x11)
    return out+b'\0'*((-len(out))%16)


def world_file(packets=None,chunks=None):
    """Empty collision tables; chunks=(packet_index,flags,translation,rotation,cell)."""
    packets=packets or [packet()]
    chunks=chunks or [(0,0,(-17,23,-31),(32767,0,0),1)]
    out=bytearray(0x84+60*len(chunks));struct.pack_into('<I',out,0,len(chunks));struct.pack_into('<I',out,0x24,0x84)
    offsets=[]
    for p in packets:
        out+=b'\0'*((-len(out))%16);offsets.append(len(out));out+=p
    for i,(p,flags,trans,rotation,cell)in enumerate(chunks):
        struct.pack_into('<II6fIIiii4h',out,0x84+60*i,offsets[p],len(packets[p])//16,-100.,-200.,-300.,100.,200.,300.,64*i,flags,*trans,cell,*rotation)
    # Coherent authored collision floor:5x5 signed zero samples, all four
    # coarse cells refer to its sole element. Independent of render heights.
    out+=b'\0'*((-len(out))%4);field=len(out)
    out+=struct.pack('<iiIIhh25h',-2,-2,5,5,0,0,*([0]*25))
    element=len(out);out+=struct.pack('<4hIIIiiihh',-8,-8,8,8,0,field,0,0,0,0,32767,0)
    grid=len(out);out+=struct.pack('<4I',0,0,0,0)
    tail=len(out);out+=struct.pack('<2h',0,-1)
    struct.pack_into('<2i5I',out,8,-1,-1,2,2,grid,1,element)
    struct.pack_into('<I',out,0x78,tail)
    return bytes(out)


def raw_batches(data,chunk):
    """Lossless raw-field inventory only, not a replacement strip decoder."""
    pos=chunk.data;end=pos+chunk.qwc*16;result={};current=None
    while pos<end:
        here=pos;word,=struct.unpack_from('<I',data,pos);pos+=4;cmd=(word>>24)&127;num=(word>>16)&255;imm=word&65535
        if cmd==0x50:pos+=(imm or 65536)*16;continue
        if cmd==0x14:
            if current is not None:current['program']=imm
            continue
        if cmd&0x60!=0x60:continue
        comp=((cmd>>2)&3)+1;width=(4,2,1)[cmd&3];n=num or 256;unsigned=(imm>>14)&1
        fmt=('iI','hH','bB')[(cmd&3)][unsigned];values=struct.unpack_from('<'+str(n*comp)+fmt,data,pos);pos+=(n*comp*width+3)&~3
        rows=[list(values[j:j+comp])for j in range(0,len(values),comp)]
        kind=(comp,width,unsigned)
        if kind==(3,2,0):current={};result[here]=current
        elif kind==(3,2,1):current['locations']=rows
        elif kind==(4,2,1):current['copies']=rows
        elif kind==(4,4,1):current['tags']=rows
    return result


def execute(model,operation,data):
    try:
        world=model.World(bytes(data))
        # Authored static-scene policy, deliberately separate from parser errors.
        if world.npieces or any(c.flags&(0x20|0x80)for c in world.chunks):return 'Unsupported',None
        packets=[];lookup={};chunks=[];omitted=[0,0,0]
        for c in world.chunks:
            packet_id=None;transformed=[]
            if c.flags&model.F_NOT_MESH:
                if c.flags&model.F_MODEL:omitted[0]+=1
                elif c.flags&model.F_EMITTER:omitted[1]+=1
                else:omitted[2]+=1
            else:
                key=(c.data,c.qwc)
                batches=world.batches(c)
                if key not in lookup:
                    lookup[key]=len(packets);raw=raw_batches(data,c);batch_rows=[]
                    for b in batches:
                        r=raw[b.offset];triangles=world.triangles(b)
                        triangle_slots=[(k-2,k-1,k)for k in range(2,len(b.strip))if not b.strip[k][1]and len({b.strip[j][0]for j in (k-2,k-1,k)})==3]
                        batch_rows.append([b.offset,b.verts,b.normals,r['locations'],r['copies'],r['tags'],b.strip,b.uvs,b.nreg,b.prim,r['program'],triangles,triangle_slots])
                    packets.append([c.data,c.qwc,batch_rows])
                packet_id=lookup[key]
                transformed=[[[bits(v)for v in p]for p in world.chunk_world_vertices(c,b)]for b in batches]
            bounds=list(struct.unpack_from('<6I',data,world.chunk_off+60*c.index+8))
            chunks.append([c.index,c.data,c.qwc,bounds,c.texoff,c.flags,[c.tx,c.ty,c.tz],c.cell,[c.rot0,c.rot1,c.rot2],packet_id,transformed])
        return 'ok',[LIMITS,[list(struct.unpack_from('<33I',data)),packets,chunks,*omitted]]
    except model.FormatError:return 'FormatError',None


def cases():
    for nr in (3,4):
        for style in range(4):yield 'decode',f'packet{nr}_{style}',world_file([packet(nr,style)],[(0,16 if nr==4 else 0,(-17,23,-31),(32767,0,0),1)]),'ok'
    for code in range(8):
        for mirror in (0,64):yield 'decode',f'orientation{code}_{mirror}',world_file(chunks=[(0,1|mirror,(-2147483648,2147483647,-123),(code|0x120,1234,-7),1)]),'ok'
    for co,si in ((0,32767),(0,-32768),(23170,23170),(-12345,23456),(-32768,-32768),(0,0)):
        for mirror in (0,64):yield 'decode',f'free{co}_{si}_{mirror}',world_file(chunks=[(0,mirror,(-37,59,-101),(co,si,17),1)]),'ok'
    p=packet();q=packet(4,1)
    yield 'decode','shared_packets',world_file([p,q,p],[(0,0,(0,0,0),(32767,0,0),1),(1,16,(2,-3,4),(23170,-23170,0),2),(0,1,(8,12,16),(5,0,0),0),(2,0,(0,0,0),(32767,0,0),-1)]),'ok'
    yield 'decode','multiple_batches',world_file([packet()+packet(4)],[(0,16,(0,0,0),(32767,0,0),1)]),'ok'
    yield 'decode','missing_uv',world_file([packet(uv=False)]),'ok'
    base=world_file()
    for name,offset,fmt,value in [('water',0x84+0x24,'I',128),('pieceflag',0x84+0x24,'I',32)]:
        b=bytearray(base);struct.pack_into('<'+fmt,b,offset,value);yield 'decode',name,bytes(b),'Unsupported'
    for cut in (0,1,131,150,len(base)-61):yield 'decode','truncate'+str(cut),base[:cut],'FormatError'
    for name,offset,value in [('no_chunks',0,0),('chunk_oob',0x24,len(base)),('payload_oob',0x84,len(base)),('packet_oob',0x88,0xffffffff),('nan_bounds',0x8c,0x7fc00000)]:
        b=bytearray(base);struct.pack_into('<I',b,offset,value);yield 'decode',name,bytes(b),'FormatError'
    yield 'decode','unsupported_vif',world_file([command(0x7f)+b'\0'*12]),'FormatError'



def packet_fields(data):
    pos=0;out=[]
    while pos<len(data):
        here=pos;word,=struct.unpack_from('<I',data,pos);pos+=4
        cmd=(word>>24)&127;n=(word>>16)&255;imm=word&65535
        if cmd==0x50:pos+=(imm or 65536)*16
        elif cmd&0x60==0x60:
            comp=((cmd>>2)&3)+1;vl=cmd&3;count=n or 256
            out.append(((comp,(4,2,1)[vl],(imm>>14)&1),here,pos,count))
            pos+=((count*comp*(4,2,1)[vl])+3)&~3
    return out


base_cases=cases

def cases():
    yield from base_cases()
    for nr in (3,4):
        p=packet(nr,1);fields={k:(command_at,payload,n)for k,command_at,payload,n in packet_fields(p)}
        mutations=[('bad_nreg',fields[(4,4,1)][1]+4,'I',2<<28),
          ('extra_count',fields[(4,2,1)][1],'H',2),
          ('strip_oob',fields[(3,2,1)][1]+12,'H',0x7fff),
          ('copy_unfilled',fields[(3,2,1)][1]+12+2,'H',4*nr),
          ('copy_destination',fields[(4,2,1)][1]+8+2,'H',nr),
          ('duplicate_slot',fields[(3,2,1)][1]+18,'H',0),
          ('uv_count',fields[(2,2,1)][0]+2,'B',4),
          ('normal_count',fields[(3,1,0)][0]+2,'B',2)]
        for name,off,fmt,v in mutations:
            b=bytearray(p);struct.pack_into('<'+fmt,b,off,v)
            yield 'decode',f'{name}_{nr}',world_file([bytes(b)]),'FormatError'
    # Packet payload is retained as independent authored bytes for omitted
    # kinds; original World does not parse these scenery services.
    yield 'decode','omitted_kinds',world_file([packet(),bytes(0xb0),bytes(0x48),bytes(0x30)],[(0,0,(0,0,0),(32767,0,0),1),(1,0x802,(12,-8,4),(1,2,3),2),(2,0x4002,(-9,8,7),(4,5,6),3),(3,0x8002,(0,0,0),(7,8,9),4)]),'ok'
    yield 'decode','only_omitted',world_file([bytes(0xb0)],[(0,0x802,(0,0,0),(0,0,0),1)]),'ok'
    p=packet();direct=command(0x50,imm=1)+bytes(range(16))+p
    direct+=b'\0'*((-len(direct))%16)
    yield 'decode','direct_state',world_file([direct]),'ok'
    # Raw bounds and uninterpreted flags survive even when not used in neutral
    # presentation. No coordinate normalization is allowed.
    b=bytearray(world_file(chunks=[(0,0x20404,(-1,0,1),(-32768,32767,-32768),0)]))
    struct.pack_into('<6I',b,0x8c,0x80000000,1,0x80800000,0x7f7fffff,0x3f800000,0xbf800000)
    yield 'decode','raw_bounds_flags',bytes(b),'ok'
    b=bytearray(world_file());piece=len(b);b+=struct.pack('<ihHiiihhhh',0,1,0,0,0,0,32767,0,0,-3)
    struct.pack_into('<II',b,0x50,1,piece)
    yield 'decode','moving_piece_table',bytes(b),'Unsupported'


def anchors(model):
    w=model.World(world_file(chunks=[(0,1,(4,8,12),(0,0,0),1)]));batch=w.batches(w.chunks[0])[0]
    if w.chunk_world_vertices(w.chunks[0],batch)!=[(-2.,3.,1.),(6.,-2.,6.),(3.,8.,3.)]:raise AssertionError('translation-before-rotation literal')
    if w.triangles(batch)!=[(0,1,2)]:raise AssertionError('literal source triangle')
    w=model.World(world_file([packet(3,1)]));batch=w.batches(w.chunks[0])[0]
    if batch.strip!=[(0,1),(1,1),(0,0),(2,0),(1,0)]or w.triangles(batch)!=[(1,0,2),(0,2,1)]:raise AssertionError('ordered copies/restarts literal')

def generate(research_dir,output):
    model=load_references(research_dir);anchors(model);output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True);manifest_path=output.with_name(output.name[:-7]+'.json')
    vh=hashlib.sha256();th=hashlib.sha256();fnv=14695981039346656037;groups=[];counts=collections.Counter();size=longest=maxinput=0
    fd,name=tempfile.mkstemp(prefix='.'+output.name+'.',dir=output.parent);temp=pathlib.Path(name);manifest_temp=None
    try:
        with os.fdopen(fd,'wb')as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0)as zipped:
                header=b'# world_geometry_vectors_v1\n';zipped.write(header);vh.update(header);size+=len(header)
                for number,(op,name,data,expected_status)in enumerate(cases()):
                    first=execute(model,op,data);second=execute(model,op,data)
                    if first!=second or first[0]!=expected_status:raise AssertionError((name,first,second,expected_status))
                    line=('\t'.join((op,'0',str(number)+';'+data.hex(),first[0],sc(first[1])))+'\n').encode('ascii')
                    zipped.write(line);vh.update(line);th.update(line);size+=len(line);longest=max(longest,len(line));maxinput=max(maxinput,len(data))
                    for byte in line:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[op]+=1;groups.append({'id':number,'case':name,'operation':op,'config':0,'status':first[0],'input_sha256':hashlib.sha256(data).hexdigest(),'input_bytes':len(data)})
            raw.flush();os.fsync(raw.fileno())
        manifest={'schema_version':1,'suite':'world-geometry','references':REFERENCES,'reference_sha256':REFERENCE_SHA256,'helpers':HELPERS,'composition_policy':COMPOSITION_POLICY,'specification_evidence':SPECIFICATION_EVIDENCE,'format_evidence':FORMAT_EVIDENCE,'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':len(groups),'vector_sha256':vh.hexdigest(),'compressed_sha256':hashlib.sha256(temp.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}','operations':dict(sorted(counts.items())),'cases':groups,'expanded_bytes':size,'max_line_bytes':longest,'max_input_bytes':maxinput,'limits':LIMITS,'composition_contract':{'arithmetic':'Presentation binary64 bits, original chunk_world_vertices evaluation order; no simulation arithmetic or game behavior claims','oracle':'Original immutable World.batches, chunk_world_vertices and triangles; raw VIF fields observed separately without interpreting strip semantics','refusals':'Original FormatError or explicit static presentation Unsupported (water/moving pieces); native safety/resource refusals separately tested','state':'All public owned header/packet/chunk fields, alias identities, binary64 position bits and triangle source/strip indices','publication':'Two fresh executions per case; atomic flushed/fsynced gzip and manifest-last replacement'}}
        fd,name=tempfile.mkstemp(prefix='.'+manifest_path.name+'.',dir=output.parent);manifest_temp=pathlib.Path(name)
        with os.fdopen(fd,'w',encoding='utf-8',newline='\n')as f:json.dump(manifest,f,sort_keys=True,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
        os.replace(temp,output);os.replace(manifest_temp,manifest_path);return manifest
    finally:
        temp.unlink(missing_ok=True)
        if manifest_temp is not None:manifest_temp.unlink(missing_ok=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--research-dir',required=True,type=pathlib.Path);parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/world_geometry.tsv.gz'));args=parser.parse_args();print(json.dumps(generate(args.research_dir,args.output),sort_keys=True))
if __name__=='__main__':main()
