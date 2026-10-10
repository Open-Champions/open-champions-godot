#!/usr/bin/env python3
"""Generate authored operation traces using an external RTA ref_core.py checkout.

No disc data or research source is copied. Native tests consume the committed TSV
without Python or a research checkout. Full pointer graphs use stable symbolic IDs.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys


def generate(research, output):
    source = research / 'tools/ref_core.py'
    source_bytes = source.read_bytes()
    pinned_commit = '41e847af5360724ab62d208594f8027ea3c1b686'
    pinned_hash = '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'
    pinned_bytes = subprocess.run(['git', '-C', str(research), 'show', pinned_commit + ':tools/ref_core.py'], check=True, capture_output=True).stdout
    if hashlib.sha256(source_bytes).hexdigest() != pinned_hash or pinned_bytes != source_bytes:
        raise RuntimeError('Expected the pinned ref_core.py bytes at ' + pinned_commit)
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('external_rta_core', source)
    r = importlib.util.module_from_spec(spec)
    exec(compile(source_bytes, str(source), 'exec'), r.__dict__)
    rows = []
    def emit(domain, op, args, result, state):
        rows.append('\t'.join((domain, op, ','.join(map(str, args)) or '-', str(result), state)))
    def hx(x): return format(x, 'x')
    def ints(xs): return ','.join(map(str, xs)) or '-'
    rngs = {}
    def rngstate(): return ';'.join(k + '=' + hx(v.state) for k, v in sorted(rngs.items())) or '-'
    for model in ('ps2', 'ps2g1', 'chop', 'nearest'):
        for bias in (False, True):
            for nearest in (False, True):
                r.set_fpu(model, mul_bias=bias, div_nearest=nearest)
                rngs.clear()
                emit('rng', 'config', [model, int(bias), int(nearest)], '-', rngstate())
                for kind, name, seed in [('SharedRandom','shared',0),('CosmeticRandom','cosmetic',0xffffffff),('EffectRandom','effect',0x80000000),('Lcg','lcg',1),('LibcRand','libc',0xffffffffffffffff)]:
                    rngs[name] = getattr(r,kind)(seed)
                    emit('rng','new',[kind,name,seed],'-',rngstate())
                for kind, name in [('ObjectRandom','object'),('SpellRandom','spell')]:
                    rngs[name] = getattr(r,kind)(rngs['shared'])
                    emit('rng','new',[kind,name,0],'-',rngstate())
                for i in range(30):
                    for name in ('shared','cosmetic','effect','lcg','object','spell'):
                        op = ('draw','unit','signed_unit','in_unit_sphere')[i % 4]
                        value = getattr(rngs[name],op)()
                        emit('rng',op,[name],ints(value) if isinstance(value,list) else value,rngstate())
                    emit('rng','rand',['libc'],rngs['libc'].rand(),rngstate())
                rngs['libc'].seed(0x80000000)
                emit('rng','seed',['libc',0x80000000],'-',rngstate())
                emit('rng','rand',['libc'],rngs['libc'].rand(),rngstate())
    p = r.Pool()
    def pstate():
        return 'base='+ints(p.base)+';head='+ints(p.head)+';last='+str(p.last)+';end='+str(p.end)+';links='+','.join(str(k)+':'+str(v) for k,v in sorted(p.link.items()))
    def pool(op,*args):
        try:
            if op=='reset': p.init(); value='-'
            elif op=='new': value=p.new(args[0])
            elif op=='batch': value=ints([p.new(args[0]) for _ in range(args[1])])
            elif op=='free': p.free(args[0]); value='-'
            elif op=='class': value=p.class_of(args[0])
            elif op=='list': value=ints(p.free_list(args[0]))
            elif op=='count': value=p.count_free()
            elif op=='erase': p.link.pop(args[0],None); value='-'
            else: raise AssertionError(op)
        except (r.PoolError,KeyError,IndexError) as e: value='!'+type(e).__name__
        emit('pool',op,args,value,pstate())
    pool('reset');pool('count');pool('list',2)
    for size in (0,1,0xffffffff,0x80000000,0x180,0x181,0x500,0x501,3000,3001,0x7fffffff): pool('new',size)
    for a in (0,p.BASE-1,p.BASE,p.base[1]-1,p.base[1],p.base[2]-1,p.base[2],p.end-1,p.end,0xffffffff): pool('class',a)
    pool('reset');pool('batch',1,513);pool('new',0x180);pool('batch',0x500,511);pool('batch',3000,32);pool('count');pool('new',1)
    pool('free',p.base[1]);pool('new',1);pool('free',p.base[2]);pool('new',0x501)
    pool('reset');pool('free',p.BASE);pool('list',0);pool('count')
    pool('reset');pool('erase',p.BASE);pool('new',1);pool('list',0);pool('free',p.base[1]);pool('count')
    pool('reset');pool('free',p.BASE-1);pool('new',1);pool('free',p.BASE+1);pool('new',1);pool('free',0);pool('new',1);pool('list',3)
    class Obj:
        def __init__(self,i): self.id=i;self.flags=0
    hp=None;cells=[];objects=[];its={}
    def token(v):
        if v is None:return '-'
        if isinstance(v,r.Node):return 'n'+str(v.index)
        if isinstance(v,Obj):return 'o'+str(v.id)
        if isinstance(v,r.Cell):return 'c'+str(cells.index(v))
        for i,it in its.items():
            if v is it:return 'i'+str(i)
        raise AssertionError(type(v))
    def ref(s):
        if s=='-':return None
        return {'n':hp.nodes,'c':cells,'o':objects,'i':its}[s[0]][int(s[1:])]
    def hstate():
        parts=['free='+token(hp.free)]
        parts += ['c'+str(i)+'='+token(c.next) for i,c in enumerate(cells)]
        for node in hp.nodes:
            parts.append(token(node)+'='+','.join(token(getattr(node,k)) for k in ('next','prev','partner','obj'))+','+str(node.tag))
        for i,it in sorted(its.items()):parts.append('i'+str(i)+'='+','.join(token(getattr(it,k)) for k in ('next','prev','partner','obj'))+','+str(it.mask))
        parts += ['o'+str(o.id)+'='+str(o.flags) for o in objects]
        return ';'.join(parts)
    def handle(op,*args):
        nonlocal hp,cells,objects,its
        try:
            if op=='reset':hp=r.HandlePool(args[0]);cells=[r.Cell() for _ in range(4)];objects=[Obj(i) for i in range(8)];its={};v='-'
            elif op=='init':hp.init();v='-'
            elif op=='add':v=token(hp.add(ref(args[0]),ref(args[1])))
            elif op=='insert':v=token(hp.insert(*(ref(a) for a in args)))
            elif op=='link':v=token(hp.link(*(ref(a) for a in args)))
            elif op=='both':hp.link_both(*(ref(a) for a in args));v='-'
            elif op=='remove':v=token(hp.remove(ref(args[0])))
            elif op=='clear':hp.clear(ref(args[0]));v='-'
            elif op=='take':v=token(hp._take())
            elif op=='release':hp._release(ref(args[0]));v='-'
            elif op=='push':hp._push(*(ref(a) for a in args));v='-'
            elif op=='unlink':hp._unlink(ref(args[0]));v='-'
            elif op=='tag':ref(args[0]).tag=args[1];v='-'
            elif op=='flags':ref(args[0]).flags=args[1];v='-'
            elif op=='count':v=hp.count_free()
            elif op=='items':v=','.join(token(o) for o in hp.items(ref(args[0]))) or '-'
            elif op=='walk':v=','.join(token(o) for o in r.walk(ref(args[0]),args[1])) or '-'
            elif op=='begin':its[args[0]]=r.Iterator(ref(args[1]),args[2]);v='-'
            elif op=='current':v=token(its[args[0]].current)
            elif op=='done':v=int(its[args[0]].done)
            elif op=='advance':its[args[0]].advance();v='-'
            elif op=='end':its[args[0]].end();v='-'
            else:raise AssertionError(op)
        except r.HandleError:v='!HandleError'
        emit('handle',op,args,v,hstate())
    handle('reset',6096);handle('count')
    handle('reset',0);handle('take');handle('add','c0','o0');handle('begin',0,'c0',0);handle('done',0);handle('current',0);handle('advance',0);handle('end',0)
    handle('reset',1);handle('link','c0','c1','o0');handle('count');handle('init');handle('both','c0','o0','c1','o1')
    handle('reset',8);handle('link','c0','c1','o0');handle('both','c0','o1','c2','o2');handle('items','c0');handle('items','c2');handle('remove','n0');handle('tag','n0',0xffffffff);handle('take');handle('release','n0');handle('clear','c0');handle('count')
    handle('reset',8);handle('link','c0','c0','o0');handle('clear','c0')
    handle('reset',16)
    for i in range(5):handle('add','c0','o'+str(i));handle('flags','o'+str(i),1<<i)
    handle('walk','c0',0);handle('walk','c0',0x8000001f);handle('begin',0,'c0',2);handle('current',0);handle('begin',1,'c0',2);handle('current',0);handle('current',1);handle('advance',0);handle('current',0);handle('end',1)
    handle('remove','n3');handle('current',0);handle('add','c0','o5');handle('insert','c0','n2','o6');handle('advance',0);handle('current',0);handle('advance',0);handle('advance',0);handle('done',0);handle('advance',0);handle('end',0)
    handle('reset',8);handle('add','c0','o0');handle('begin',0,'c0',0);handle('remove','n0');handle('current',0);handle('advance',0)
    handle('reset',8);handle('add','c0','o0');handle('add','c0','o1');handle('begin',0,'c0',0);handle('clear','c0')
    handle('reset',8);handle('add','c0','-');handle('add','c0','o0');handle('flags','o0',0x80000000);handle('items','c0');handle('walk','c0',0x80000000);handle('clear','c0')
    handle('reset',8);handle('take');handle('tag','n0',0xffffffff);handle('push','c0','n0');handle('unlink','n0');handle('release','n0');handle('take');handle('insert','c1','-','o0');handle('init')
    # Deterministically authored mutation workload; never traverse an intentionally
    # corrupted graph. Explicit cases above cover the reference's error states.
    rr=random.Random(0x525441)
    handle('reset',32)
    for _ in range(400):
        active=[n for n in hp.nodes if n.obj is not None and any(n in chain(c) for c in cells)]
        choice=rr.randrange(5)
        if choice==0 and active:handle('remove',token(rr.choice(active)))
        elif choice==1:handle('clear','c'+str(rr.randrange(4)))
        elif choice==2 and hp.count_free()>=2:
            a,b=rr.sample(range(4),2);handle('link','c'+str(a),'c'+str(b),'o'+str(rr.randrange(8)))
        elif hp.count_free():handle('add','c'+str(rr.randrange(4)),'o'+str(rr.randrange(8)))
        handle('count');handle('walk','c'+str(rr.randrange(4)),rr.randrange(256))
    content='# core_state_vectors_v1\n# domain\top\targs\texpected_result\texpected_complete_state\n'+'\n'.join(rows)+'\n'
    if source.read_bytes() != source_bytes:
        raise RuntimeError('Reference changed during vector generation; retry from a stable source')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(content,encoding='utf-8',newline='\n')
    def git(*args):return subprocess.run(['git','-C',str(research),*args],check=True,text=True,capture_output=True).stdout.strip()
    manifest={'schema':1,'reference':'tools/ref_core.py','reference_sha256':hashlib.sha256(source_bytes).hexdigest(),'research_head':git('rev-parse','HEAD'),'reference_last_commit':git('log','-1','--format=%H','--','tools/ref_core.py'),'reference_dirty':bool(git('status','--porcelain','--','tools/ref_core.py')),'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'rows':len(rows),'domain_rows':{d:sum(row.startswith(d+'\t') for row in rows) for d in ('rng','pool','handle')},'fpu_settings':{'models':['ps2','ps2g1','chop','nearest'],'mul_bias':[False,True],'div_nearest':[False,True]},'seed':0x525441,'scope':'RTA ref_core sections 9-11; symbolic complete state after each operation, synthetic inputs only; no original-runtime or Norrath claim'}
    output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(manifest,indent=2))


def chain(cell):
    node=cell.next
    while node is not None:
        yield node
        node=node.next


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=Path)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'native/tests/vectors/core_state.tsv')
    args=parser.parse_args();generate(args.research_dir.resolve(),args.output.resolve())
