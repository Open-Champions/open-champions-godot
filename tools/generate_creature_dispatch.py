#!/usr/bin/env python3
"""Compile pinned numeric research tables and synthetic reference vectors.
No retail bytes, emulator, native output, or unpinned checkout imports.
"""
import argparse, hashlib, importlib, json, pathlib, subprocess, sys, tempfile, gzip
ROOT=pathlib.Path(__file__).resolve().parents[1]
PINS=ROOT/'native/tests/vectors/creature_dispatch_sources.json'
def write_if_changed(path,data):
    data=data.encode() if isinstance(data,str) else data
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
def quoted(v): return json.dumps(v)
def nums(v): return '{'+','.join(str(x) for x in v)+'}'
def ops(v):
    rows=[]
    for o in v:
        args=[]
        for a in o[1:]:
            if isinstance(a,(list,tuple)):args.extend(a)
            else:args.append(-1 if a is None else a)
        rows.append('{'+quoted(o[0])+','+nums(args)+'}')
    return '{'+','.join(rows)+'}'
def selection(o,s):return '{'+ops(o)+','+str(s)+'}'
def slots(rows):return '{{'+','.join('{%d,%d}'%(t,m) for t,m in rows)+'}}'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--research',required=True);args=ap.parse_args()
    pins=json.loads(PINS.read_text())
    with tempfile.TemporaryDirectory(prefix='creature-dispatch-') as tmp:
        for name,sha in pins['sha256'].items():
            b=subprocess.check_output(['git','-C',args.research,'show',pins['commit']+':'+name])
            if hashlib.sha256(b).hexdigest()!=sha:raise ValueError('pin mismatch '+name)
            if name.startswith('tools/'):(pathlib.Path(tmp)/pathlib.Path(name).name).write_bytes(b)
        sys.path.insert(0,tmp)
        D=importlib.import_module('ref_creature_dispatch');T=importlib.import_module('ref_creature_dispatch_data');A=importlib.import_module('ref_anim');C=importlib.import_module('ref_core')
        lines=['// Numeric facts from '+pins['commit']+'. Regenerate with tools/generate_creature_dispatch.py.']
        cases=[];groups={}
        for table_index,(name,info) in enumerate(sorted(T.DISPATCH.items())):
            group=D.group_of(info);groups[group]=groups.get(group,0)+1
            lines+=['static DispatchTable make_dispatch_%d() { DispatchTable t; t.group='%table_index+quoted(group)+';']
            # 'none' has no clip rows. 'special' is the ten own-machine classes;
            # their setState rows are the round-3 table the run switch requests.
            if group == 'none':lines+=['return t; }'];continue
            for state,c in sorted(info['states'].items()):
                lines+=['{ auto &c=t.states[%d]; c.baseline=%s;'%(state,selection(c['ops'],c['stored']))]
                if c.get('rand'):
                    for i,o in enumerate(c['rand'][1]):lines+=['c.residues.push_back('+selection(o,c.get('rand_stored',[c['stored']]*c['rand'][0])[i])+');']
                for r,o,s in c.get('rand_points',[]):lines+=['c.points[%d]=%s;'%(r,selection(o,s))]
                for cond,o,s in c.get('when',[]):
                    ret=cond[0]=='ret';field=0 if ret else cond[0]
                    lines+=['c.conditions.push_back({{%s,%d,%d},%s});'%('true' if ret else 'false',field,cond[1],selection(o,s))]
                lines+=['}']
                choices=[(0,0,0,0)]
                if 'rand' in c:choices=[(r,0,0,0) for r in range(c['rand'][0])]+[(65535,0,0,0)]
                elif 'rand_points' in c:choices=[(r,0,0,0) for r,_,_ in c['rand_points']]
                choices += [(0,2 if cond[0]=='ret' else 1,0 if cond[0]=='ret' else cond[0],cond[1]) for cond,_,_ in c.get('when',[])]
                for r,kind,field,value in choices:
                    kw={'rand':r}
                    if kind==1:kw['field']=(field,value)
                    if kind==2:kw['ret']=value
                    o,stored=D.select(c,**kw)
                    # Every accepted selection is applied to independent authored clips.
                    system=A.AnimSystem(8,3); system.sim_frame=17
                    st=A.AnimState();clips={i:A.Clip(A.encode_clip([((0,0,0),(4096,0,0,0))],120)) for i in range(64)}
                    A.anim_push(st,clips[0],1)
                    calls=[]
                    kinds=('start','blend','push','push_w','push_chained','stop','sync','jump_end','snap_end','randomize','clear','drop','vslot','holder')
                    for x in o:
                        if x[0] not in kinds:raise ValueError('unknown dispatch op %r in %s %d'%(x[0],name,state))
                    # A None entry is a runtime clip pointer (Critter species
                    # table, Skeleton). The row is emitted in dispatch_data;
                    # applying it is an explicit refusal, and that is the vector.
                    if any(len(x)>1 and x[1] is None for x in o):
                        cases.append({'input':'select %s %d %d %d %d %d'%(name,state,r,kind,field,value),'expected':'REFUSED dispatch uses an unresolved dynamic resource'})
                        continue
                    D.apply_ops(system,st,clips,o,on_vslot=lambda *a:calls.append(a))
                    flat=[stored,len(o)]
                    for op in o:
                        aa=[]
                        for a in op[1:]:aa.extend(a if isinstance(a,(tuple,list)) else [a])
                        flat += [op[0],len(aa),*aa]
                    flat += [st.count,st.flags,system.cosmetic_seed]
                    for tr in st.tracks:
                        flat += [next((i+1 for i,cl in clips.items() if tr.clip is cl),0),tr.weight,tr.rate,tr.life,tr.life_rate,tr.time,tr.flags]
                    cases.append({'input':'select %s %d %d %d %d %d'%(name,state,r,kind,field,value),'expected':' '.join(map(str,flat))})
            for entry,ev in T.EVENTS.get(name,{}).items():lines+=['t.events[%d]=%s;'%(entry,slots(ev))]
            for variant,entries in T.EVENT_VARIANTS.get(name,{}).items():
                for entry,ev in entries.items():lines+=['t.event_variants[%s][%d]=%s;'%(quoted(variant),entry,slots(ev))]
            lines+=['t.constructor='+ops(T.CTOR_OPS.get(name,[]))+';']
            for state,masks in T.CONSUMERS.get(name,{}).items():
                for mask,names in masks.items():lines+=['t.consumers[%d][%d]={%s};'%(state,mask,','.join(map(quoted,names)))]
            for state,(a,b) in T.END_TRANSITIONS.get(name,{}).items():lines+=['t.transitions[%d]={%s,%s};'%(state,nums(a),nums(b))]
            lines+=['return t; }']
        lines+=['const std::map<std::string, DispatchTable> &dispatch_tables() {', 'static const auto tables=[] { std::map<std::string, DispatchTable> out;']
        for i,name in enumerate(sorted(T.DISPATCH)):lines+=['out.emplace(%s,make_dispatch_%d());'%(quoted(name),i)]
        lines+=['return out; }(); return tables; }']
        write_if_changed(ROOT/'native/src/creature_anim/dispatch_data.inc','\n'.join(lines)+'\n')
        # Authored event-timing sequences for every installed class entry.
        for name,info in sorted(T.DISPATCH.items()):
            if D.group_of(info) == 'none':continue
            for entry,ev in T.EVENTS.get(name,{}).items():
                cl=A.Clip(A.encode_clip([((0,0,0),(4096,0,0,0))],180));cl.events=[list(v) for v in ev]
                sy=A.AnimSystem();st=A.AnimState();A.anim_push(st,cl,1)
                words=[A.anim_advance(sy,C.ONE,st,sound=False)[0] for _ in range(185)]
                cases.append({'input':'events %s %d'%(name,entry),'expected':' '.join(map(str,words))})
        from creature_dispatch_player_vectors import vectors, skill_vectors, precedence_vectors
        cases += vectors(C,A,D)
        # Skill states and two-field precedence, from the pinned modules.
        cases += skill_vectors(importlib.import_module('ref_skill_anim'))
        cases += precedence_vectors(importlib.import_module('ref_field_precedence'))
        payload=json.dumps(cases,separators=(',',':')).encode();out=ROOT/'native/tests/vectors/creature_dispatch.json.gz';write_if_changed(out,gzip.compress(payload,mtime=0))
        write_if_changed(ROOT/'native/tests/vectors/creature_dispatch.json',json.dumps({'commit':pins['commit'],'groups':groups,'cases':len(cases),'generator_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['tools/generate_creature_dispatch.py','tools/creature_dispatch_player_vectors.py']},'sources_sha256':hashlib.sha256(PINS.read_bytes()).hexdigest(),'payload_sha256':hashlib.sha256(payload).hexdigest(),'data_sha256':hashlib.sha256((ROOT/'native/src/creature_anim/dispatch_data.inc').read_bytes()).hexdigest()},indent=2)+'\n')
        print(groups,'cases',len(cases))
if __name__=='__main__':main()
