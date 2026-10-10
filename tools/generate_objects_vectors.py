#!/usr/bin/env python3
"""Generate asset-free RTA object runtime expectations from immutable Python models.

Stable authored identities describe pointer graphs; no host pointer is serialized.
"""
import argparse
import collections
import gzip
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import types

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'objects': {'commit': '0dd02ce416389dd440540119727d946636507703', 'path': 'tools/ref_objects.py', 'sha256': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
COMPOSITION_POLICY = 'rta-objects-runtime-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')


def load_references(research_dir):
    modules = {}
    old_path = list(sys.path)
    previous = sys.modules.get('ref_core')
    try:
        for name in ('core', 'objects'):
            pin = REFERENCES[name]
            source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']], capture_output=True, check=True, timeout=30).stdout
            if hashlib.sha256(source).hexdigest() != pin['sha256']:
                raise ValueError('immutable reference hash mismatch: ' + name)
            module = types.ModuleType('pinned_object_' + name)
            module.__file__ = '/__verified_pinned_models__/ref_' + name + '.py'
            exec(compile(source, module.__file__, 'exec'), module.__dict__)
            modules[name] = module
            if name == 'core':
                sys.modules['ref_core'] = module
    finally:
        sys.path[:] = old_path
        if previous is None:
            sys.modules.pop('ref_core', None)
        else:
            sys.modules['ref_core'] = previous
    return modules['core'], modules['objects']


def sc(value):
    if value is None:
        return '_'
    if isinstance(value, bool):
        return str(int(value))
    if isinstance(value, bytes):
        return 'b' + value.hex()
    if isinstance(value, str):
        return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)):
        return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int):
        return str(value)
    raise TypeError('unowned or noninteger object trace value')

SCENARIOS = ('primitives', 'names', 'tasks', 'construct', 'lifetime', 'grid', 'queries', 'activity', 'reaper', 'run_mutation', 'tasks_run', 'visibility', 'free_spot', 'marker', 'records', 'walkers', 'init', 'errors', 'callbacks')
GLOBALS = 'cursor15 cursor60 frame network_mode cull_by_camera pool_free_count tearing_down lump_generation menu_lockout menu_lockout_hold warmup_frames start_rank exclude_mask'.split()
OBJECT_FIELDS = 'addr flags pos radius height vz angle seed vtable draw_half_width draw_height class_name type_name alive tag'.split()


class CallbackFailure(Exception):
    pass


class Oracle:
    def __init__(self, core, objects, config, variant):
        self.r, self.o, self.variant = core, objects, variant
        core.set_fpu(MODELS[config//4], bool(config&2), bool(config&1))
        self.retained, self.iterators, self.extra = [], [], []
        self.live_iterators, self.completed_iterators = {}, set()
        self.original_object, self.original_iterator = objects.GameObject, core.Iterator
        outer = self
        class RecordedObject(self.original_object):
            def __init__(self, addr):
                super().__init__(addr)
                outer.retained.append(self)
        class RecordedIterator(self.original_iterator):
            def __init__(self, head, mask=0):
                super().__init__(head, mask)
                outer.iterators.append(self)
                outer.live_iterators[len(outer.iterators)] = self
            def end(self):
                super().end()
                outer.completed_iterators.add(outer.iterators.index(self) + 1)
                outer.prune_iterators()
        objects.GameObject, core.Iterator = RecordedObject, RecordedIterator
        self.w = objects.World()
        self.w.shared.state = (0, 1, 0xdeadbeef, 0xffffffff)[variant]
        self.events, self.results = [], []

    def prune_iterators(self):
        # Completed sentinels have no runtime lifetime unless a stale retained
        # link still references them. This authored ownership policy preserves
        # Python's reachable graph without retaining every empty walk forever.
        referenced = set()
        for link in self.w.handles.nodes + list(self.live_iterators.values()):
            for key in ('next', 'prev', 'partner'):
                value = getattr(link, key, None)
                if isinstance(value, self.r.Iterator):
                    referenced.add(id(value))
        for serial in sorted(self.completed_iterators):
            if id(self.live_iterators[serial]) not in referenced:
                del self.live_iterators[serial]
        self.completed_iterators.intersection_update(self.live_iterators)

    def close(self):
        self.o.GameObject, self.r.Iterator = self.original_object, self.original_iterator

    def oid(self, obj):
        if obj is None:
            return 0
        return self.retained.index(obj) + 1

    def ids(self, objects):
        return [self.oid(x) for x in objects]

    def pos(self, x=0, y=0, z=0):
        return [self.r.from_int(v) for v in (x, y, z)]

    def new(self, x=0, y=0, z=0, props=None, class_def=None):
        return self.w.new_object(0x180, self.pos(x, y, z), class_def, props)

    def destroy(self, obj, flags):
        self.events.append(['destroy', self.oid(obj), flags, self.w.tearing_down])
        self.w.destroy(obj, flags)

    def snapshot(self):
        w, r = self.w, self.r
        cells = w.grid + w.grid_b + [w.off_grid, w.always_draw, w.delete_list, w.run_active] + w.run15 + w.run60
        ids = {id(c): c.name for c in cells}
        for i, obj in enumerate(self.retained, 1):
            for c in obj.heads:
                ids[id(c)] = (i << 32) | c.name
        for c in self.extra:
            ids[id(c)] = c.name
        for n in w.handles.nodes:
            ids[id(n)] = r.HandlePool.NODE_BASE + 20*n.index
        for i, it in self.live_iterators.items():
            ids[id(it)] = (1 << 63) | i
        def ref(x):
            return 0 if x is None else ids[id(x)]
        roots = cells + [c for obj in self.retained for c in obj.heads] + self.extra + w.handles.nodes
        roots += [it for serial,it in self.live_iterators.items() if serial not in self.completed_iterators]
        pending, seen, reachable_iterators = list(roots), set(), set()
        while pending:
            link = pending.pop()
            if id(link) in seen: continue
            seen.add(id(link))
            if isinstance(link, r.Iterator): reachable_iterators.add(id(link))
            for key in ('next','prev','partner'):
                value=getattr(link,key,None)
                if value is not None: pending.append(value)
        links = []
        for c in cells:
            if c.next is not None:
                links.append([ref(c), 0, ref(c.next), 0, 0, 0, 0])
        for c in [c for obj in self.retained for c in obj.heads] + self.extra:
            links.append([ref(c), 0, ref(c.next), 0, 0, 0, 0])
        for n in w.handles.nodes:
            row = [ref(n), 1, ref(n.next), ref(n.prev), ref(n.partner), self.oid(n.obj), n.tag]
            default_next = r.HandlePool.NODE_BASE + 20*(n.index+1) if n.index+1 < len(w.handles.nodes) else 0
            if row != [ref(n), 1, default_next, 0, 0, 0, 0]:
                links.append(row)
        for it in self.live_iterators.values():
            if id(it) not in reachable_iterators: continue
            links.append([ref(it), 2, ref(it.next), ref(it.prev), ref(it.partner), self.oid(it.obj), it.mask])
        props_ids, class_ids = {}, {}
        def alias(value, mapping):
            if value is None:
                return 0
            if id(value) not in mapping:
                mapping[id(value)] = len(mapping) + 1
            return mapping[id(value)]
        objs = []
        for i, obj in enumerate(self.retained, 1):
            cd = obj.class_def
            objs.append([i] + [getattr(obj, k) for k in OBJECT_FIELDS] +
                        [alias(obj.props, props_ids), obj.props, alias(cd, class_ids),
                         None if cd is None else [cd.lump, cd.generation, cd.entries]])
        # Lossless pool-link deltas from the public default pool, including removed keys.
        baseline = r.Pool()
        pool_delta = [[k, w.pool.link.get(k)] for k in sorted(set(baseline.link) | set(w.pool.link)) if baseline.link.get(k) != w.pool.link.get(k)]
        names = [[i, k, self.oid(w.names.objs[i])] for i, k in enumerate(w.names.keys) if k is not None or w.names.objs[i] is not None]
        return [[w.pool.base, w.pool.head, w.pool.last, w.pool.end, pool_delta], w.shared.state,
                ref(w.handles.free), len(self.iterators), [6096, 28879], sorted(links), objs,
                [[k, self.oid(v)] for k, v in sorted(w.objects.items())], names, w.names.count,
                [[[t.fn, t.priority, t.name, t.flags] for t in w.tasks.tasks], w.tasks.cursor],
                [getattr(w, k) for k in GLOBALS], w.camera_focus, w.centres,
                [[p.held, p.pressed, p.edge2] for p in w.pads], w.spawn_pos, w.spawn_facing,
                1, [True]*5]

    def run(self, scenario):
        w, r, o, v, out = self.w, self.r, self.o, self.variant, self.results
        if scenario == 'primitives':
            props = [b'name="One"tail', b'Name=Upper', b'name=Two', b'n= -4294967297x', b'name=Three', b'\xff=7']
            out.extend([o.props_find(props, b'NAME', n) for n in (-1, 0, 1, 2, 3)])
            out.extend([o.props_find(None, b'x'), o.props_int(props, b'N'), o.props_int(None, b'n')])
            for s in (b' \t-2147483649junk', b'+4294967296', b'--2', b'\xff99', b'00017', b'"MiXeD"rest', b'"open'):
                parts = []
                for fn in (o.atoi, o.name_hash, o.strip_quotes):
                    try: parts.append(fn(s))
                    except o.ObjectError: parts.append('ObjectError')
                out.append(parts)
            for x in (0, 0x80000000, 0x3f7fffff, 0xbf7fffff, 0xc66fff00, 0x466fff00, 0x7fffffff, 0xffffffff):
                try: out.append([x, o.cell_coord(x)])
                except o.ObjectError: out.append([x, 'ObjectError'])
        elif scenario == 'names':
            a, b, c = self.new(), self.new(1), self.new(2)
            out.extend([w.names.add(b'"Alpha"tail', a), w.names.add(b'ALPHA', b), w.names.add(b'Beta', c)])
            out.append(self.ids(w.names.find(b'alpha', 8)))
            out.append(w.names.remove(b'wrong-key', a))
            out.append(w.names.add(b'alpha', c))
            out.extend([self.oid(w.names.find_first(b'ALPHA')), w.names.remove(b'Beta', b), self.ids(w.names.find(b'alpha', 2+v))])
            if v == 3:
                w.names.clear()
            out.append(w.names.count)
        elif scenario == 'tasks':
            t = w.tasks
            for fn, priority, flags in ((10, 2, 0), (20, 1, 1), (30, 2, 2), (40, 3, 3)):
                out.append(t.add(fn, priority, b't'+str(fn).encode(), flags))
            out.extend([t.add(10, -99, b'duplicate', 7), t.is_running(b't10'), t.is_running(b'T10')])
            changed = [False]
            def invoke(fn):
                self.events.append(['task', fn, t.cursor])
                if not changed[0] and fn == (20 if v == 1 else 10):
                    changed[0] = True
                    if v == 0: t.remove(fn)
                    elif v == 1: t.add(50, 1, b'insert', 0)
                    elif v == 2: t.remove(30)
                    else: t.add(60, 4, b'late', 2)
            t.call(0 if v == 1 else 1, -1, invoke)
            t.call(0, 1, lambda fn: self.events.append(['draw', fn, t.cursor]))
            out.append(t.remove(999))
        elif scenario == 'construct':
            props = [b'name="Twin"suffix', b'name=Second', b'hide=0']
            cd = o.ClassDef(b'authored.lmp', [b'one', b'two'], 0)
            def resolve(lump, name):
                self.events.append(['resource', lump, name, cd.generation])
                return 0x1000 + len(self.events)*4
            w.res_find = resolve
            addr = w.pool.new(0x180)
            a = w.construct(addr, self.pos(), cd, props, [b'hide=1'] if v&1 else None)
            b = self.new(256, props=props, class_def=cd)
            w.lump_generation += 1
            c = self.new(512, props=props if v&2 else list(props), class_def=cd)
            out.extend([a.rand(), b.rand(), c.rand(), self.ids(w.names.find(b'twin', 9)), w.is_off_grid(a)])
            out.append(self.oid(w.new_object(4000, self.pos())))
        elif scenario == 'lifetime':
            a, b, c = [self.new(i, props=[b'name=x']) for i in range(3)]
            for obj in (a, b, c): w.run_every_frame(obj)
            w.on_destroy = self.destroy
            w.delete(a)
            w.delete(b)
            if v == 1:
                b.flags |= 0x400; w.exclude_mask = 0x400
            if v == 2:
                def destroy(obj, flags):
                    self.destroy(obj, flags)
                    if obj is b: w.delete(c)
                w.on_destroy = destroy
            w.flush_delete_list()
            if v == 3: w.destroy(c, 2)
            d = self.new(7)
            out.append(self.oid(d))
            w.exclude_mask = 0
            w.flush_delete_list()
        elif scenario == 'grid':
            a, b = self.new(), self.new(16000)
            out.append(w.is_off_grid(b))
            a.flags |= 0x800; w.update_grid_cell(a)
            out.append(w.home_cell(a, True).name)
            a.flags |= 2; w.update_grid_cell(a)
            out.append(w.home_cell(a, True).name)
            w.move_off_grid(a); out.append(w.is_off_grid(a))
            w.link_to_grid(a); out.append(w.is_off_grid(a))
            a.flags &= ~2; w.update_grid_cell(a)
            w.add_to_always_draw(a); w.add_to_always_draw_once(a); w.add_to_always_draw_once(b)
            if v&1: w.exclude_mask = 0x800
            w.clear_always_draw()
            if v&2: b.pos=self.pos(-15361); w.update_grid_cell(b)
        elif scenario == 'queries':
            objs = [self.new(x, y, z) for x,y,z in ((0,0,0),(24,0,0),(0,24,80),(300,0,0),(-260,0,0),(0,0,-72))]
            objs[1].flags = (0, 1, 8, 0x20000)[v]
            objs[2].flags |= 0x400; w.exclude_mask = 0x400 if v&1 else 0
            objs[3].flags |= 0x800; w.update_grid_cell(objs[3])
            out.append(self.ids(w.collect_in_box(self.pos(-25,-25,-1), self.pos(25,25,80), (1,2,4,9)[v])))
            head = r.Cell(0x90000000); self.extra.append(head)
            out.append(self.ids(w.collect_in_sphere(r.from_int(100), self.pos(), head)))
            out.append(self.ids(r.HandlePool.items(head)))
            w.handles.clear(head)
            for i, obj in enumerate(objs):
                obj.type_name = b'authored' if i%2==0 else b'other'
                (w.run_every_frame, w.run_every_15, w.run_every_60)[i%3](obj)
            out.append(self.ids(w.find_by_class(b'authored', 3+v)))
        elif scenario == 'activity':
            a = self.new(100, 0, 10)
            w.centres = [self.pos()]
            a.flags |= 0x80000 if v==3 else 0
            out.append([w.is_active(a, n) for n in (0, 100, 101, 46341)])
            w.run_every_frame(a)
            out.append(w.update_active(a, 1, 10))
            w.centres = [list(a.pos)]
            out.append(w.update_active(a, 0, 100))
            w.stop_running(a)
            w.cursor15=14; w.cursor60=59
            w.run_every_15(a); w.run_every_60(a)
        elif scenario == 'reaper':
            w.frame = (0, 7, 899, -1)[v]
            indices = w.reap_cells(); out.append(indices)
            for index in indices[:2]:
                row,col = divmod(index,120)
                for j in range(2):
                    obj=self.new(col*256-15360+10, row*256-15360+10)
                    obj.flags=0x1800; w.update_grid_cell(obj)
            w.camera_focus=list(self.retained[-1].pos)
            w.pool_free_count=(199,200,299,200)[v]; w.network_mode=int(v==3)
            w.on_destroy=self.destroy; w.exclude_mask=0x1000
            out.append(self.ids(w.reap()))
        elif scenario == 'run_mutation':
            a,b,c=self.new(),self.new(1),self.new(2)
            w.frame=1; w.run_every_frame(a); w.run_every_frame(b)
            c.flags=0x8000 if v==3 else 0
            w.run_every_15(c)
            once=[False]
            def run(obj):
                self.events.append(['run',self.oid(obj),w.frame])
                if not once[0]:
                    once[0]=True
                    if v==0:
                        w.destroy(a)  # next node keeps the active iterator in stale prev
                        w.destroy(obj)
                    elif v==1: w.run_every_60(obj)
                    elif v==2:
                        d=self.new(3); w.run_every_60(d); w.frame=1
                    else: w.exclude_mask=0x8000
            w.on_run=run; w.on_destroy=self.destroy
            w.run_objects()
        elif scenario == 'tasks_run':
            w.tasks.add(10,1,b'run',0); w.tasks.add(20,2,b'draw',1)
            w.network_mode=int(v==1); w.warmup_frames=int(v==2)
            w.menu_lockout=4; w.menu_lockout_hold=int(v==3); w.frame=0x7fffffff if v==0 else 7
            for i,pad in enumerate(w.pads): pad.held=1 if v!=2 else 0x00f00000; pad.pressed=i+1;pad.edge2=i+9
            def invoke(fn): self.events.append(['task',fn,w.frame,[[p.pressed,p.edge2] for p in w.pads]])
            def end():
                self.events.append(['end',w.frame,[[p.pressed,p.edge2] for p in w.pads]])
                if v==3: w.pads[0].pressed=99
            w.end_of_tick=end
            w.is_frozen=lambda: v==3
            w.tasks_run(2 if v!=2 else -1, invoke)
        elif scenario == 'visibility':
            a,b,c=self.new(),self.new(100,100,10000),self.new(-5)
            b.flags=8; players=[a,None,b,c]
            w.network_mode=int(v==1);w.camera_focus=self.pos(500,500)
            w.set_object_vis(c if v==3 else None, v%2, int(v==2),players)
            out.append(w.is_near_view(self.pos(24,24,-10000),r.from_int(24),players))
            def local(slot): self.events.append(['local',slot]);return slot%2==v%2
            out.append(w.alive_players(players,local))
        elif scenario == 'free_spot':
            if v==3: self.new()
            def blocked(radius,pos):
                self.events.append(['blocked',radius,list(pos)])
                return r.from_int(1) if v==1 else 0
            def height(radius,pos):
                self.events.append(['height',radius,list(pos)])
                return r.from_int(6 if v==2 else -1)
            out.append(w.find_free_spot(self.pos(),0 if v==3 else 36,24,False,blocked,height))
            if v==2: out.append(w.find_free_spot(self.pos(),1,1,True,blocked,height))
        elif scenario == 'marker':
            props=([b'from=old'],[b'from=gate'],[b'name=door'],[b'default=1'])[v]
            w.start_rank=(0,2,1,1)[v]
            def blocked(radius,pos): self.events.append(['blocked',radius,list(pos)]); return 0
            def height(radius,pos): self.events.append(['height',radius,list(pos)]); return pos[2]
            a=w.player1_marker(w.pool.new(0x180),self.pos(10,-20,3),-40000,props,b'old',b'door',v==1,blocked,height)
            out.append(self.oid(a))
            if v in (0,3):
                if v==3: w.start_rank=3
                b=w.player1_marker(w.pool.new(0x180),self.pos(10,-20,3),0,None,b'old',b'door',False,blocked,height)
                out.append(self.oid(b))
        elif scenario == 'records':
            props=[b'name="Shared"tail',b'x=1']
            records=[o.Record(b'one',-32769,self.pos(1,2,3),props,b'\x01\x02'),o.Record(None,65537,self.pos(-4),props,b'\x00'),o.Record(b'skip',7,self.pos(8),None,b'')]
            blob=o.RecordBlob(records,v&1)
            out.append(blob.build()); out.append(blob.size)
            def factory(name,pos,angle,pr):
                self.events.append(['factory',name,list(pos),angle, None if pr is None else list(pr)])
                if name is None: return None
                obj=w.new_object(0x180,pos,None,pr)
                if name==b'skip': w.delete(obj)
                return obj
            def filt(name): self.events.append(['filter',name]); return name==b'one'
            def state(obj,data): self.events.append(['state',self.oid(obj),data]);obj.tag=len(data)
            out.append(w.load_records(blob,factory,v>=2,filt if v==2 else None,state))
            out.extend([blob.relocated,blob._size,blob.build()])
            out.append(self.oid(o.create_object(b'created',self.pos(2),65535,None,factory)))
            out.append(self.oid(o.create_object(None,self.pos(),0,None,None)))
        elif scenario == 'walkers':
            a,b,c=self.new(),self.new(16000),self.new(256)
            c.flags=0x800;w.update_grid_cell(c)
            for obj in (a,b,c):w.run_every_frame(obj)
            a.flags |= 0x400; w.exclude_mask=0x400 if v&1 else 0
            w.for_each_grid_object(lambda obj:self.events.append(['grid',self.oid(obj)]))
            w.for_each_run_object(lambda obj:self.events.append(['run',self.oid(obj)]))
            w.on_destroy=self.destroy
            if v&2:w.delete(b)
            w.delete_all_objects()
        elif scenario == 'init':
            a=self.new(props=[b'name=before']);w.run_every_frame(a);w.add_to_always_draw(a)
            w.init();w.init()
            out.append(self.ids(w.names.find(b'before',2)))
            out.append(w.tasks.is_running(b'runObjects'))
            if v&1:w.destroy(a)
        elif scenario == 'errors':
            a,b=self.new(),self.new(1)
            if v==0:
                w.names.find(b'x',1)
            elif v==1:
                for i in range(33):w.tasks.add(i,0,b'x',0)
            elif v==2:
                w.frame=-1;w.run_every_frame(a)
                w.on_run=lambda obj:self.events.append(['run',self.oid(obj)])
                w.run_objects()
            else:
                w.run_every_frame(a);w.run_every_frame(b)
                def fail(obj):self.events.append(['throw',self.oid(obj)]);obj.tag=77;raise CallbackFailure()
                w.on_run=fail;w.run_objects()
        elif scenario == 'callbacks':
            if v == 0:
                cd = o.ClassDef(b'callback.lmp', [b'first', b'second'])
                def resource(lump, name):
                    self.events.append(['resource', lump, name])
                    if name == b'second': raise CallbackFailure()
                    return 0x10203040
                w.res_find = resource
                self.new(class_def=cd)
            elif v == 1:
                blob = o.RecordBlob([o.Record(b'one', 0, self.pos(), [b'name=x'], b'abc')], True)
                def factory(name,pos,angle,props): return w.new_object(0x180,pos,None,props)
                def load(obj,data):
                    self.events.append(['state',self.oid(obj),data]);obj.tag=42;w.delete(obj);raise CallbackFailure()
                try: w.load_records(blob,factory,False,None,load)
                finally: out.append([blob.relocated,blob._size])
            elif v == 2:
                a=self.new()
                def blocked(radius,pos):
                    self.events.append(['blocked',radius,list(pos)]);a.flags=1;raise CallbackFailure()
                w.find_free_spot(self.pos(),36,24,False,blocked,lambda radius,pos:0)
            else:
                w.pads[0].pressed=11;w.pads[0].edge2=12;w.menu_lockout=5
                w.tasks.add(1,0,b'one',0)
                def end():
                    self.events.append(['end',w.frame]);w.pads[0].pressed=99;raise CallbackFailure()
                w.end_of_tick=end
                w.tasks_run(2,lambda fn:self.events.append(['task',fn]))
        else:
            raise AssertionError('unknown authored scenario')


def run_scenario(core, objects, config, variant, name):
    oracle=Oracle(core,objects,config,variant)
    status='ok'
    try:
        try:
            oracle.run(name)
        except (objects.ObjectError, core.HandleError, CallbackFailure) as error:
            status=type(error).__name__
        return status, [oracle.results,oracle.events,oracle.snapshot()]
    finally:
        oracle.close()


def generate(research_dir, output):
    core,objects=load_references(research_dir)
    if not output.name.endswith('.tsv.gz'):raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    vh,th=hashlib.sha256(),hashlib.sha256();fnv=14695981039346656037;counts=collections.Counter();number=0
    with output.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
        header=b'# objects_vectors_v1\n';stream.write(header);vh.update(header)
        for config in range(16):
            for variant in range(4):
                for name in SCENARIOS:
                    status,value=run_scenario(core,objects,config,variant,name)
                    encoded=sc(value)
                    row=('\t'.join([name,str(config),f'{number};{variant}',status,encoded])+'\n').encode('ascii')
                    stream.write(row);vh.update(row);th.update(row)
                    for byte in row:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
                    counts[name]+=1;number+=1
    manifest={'schema_version':1,'suite':'objects-runtime','reference_sha256':REFERENCE_SHA256,'references':REFERENCES,'composition_policy':COMPOSITION_POLICY,
              'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'vector_count':number,'vector_sha256':vh.hexdigest(),
              'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'expected_trace_sha256':th.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
              'operations':dict(sorted(counts.items())),'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
              'trace_columns':['operation','configuration','id;variant','status','results_events_owned_snapshot'],
              'composition_contract':{'identity':'Object allocation serials, virtual cells/nodes, retained iterator creation serials; shared props/class definitions use first-seen alias IDs.',
               'snapshot':'Full owned fields with lossless sparse default-link deltas, retained freed-node fields, pool removed keys, retired objects, callbacks presence and ordered callback effects. Executable callback code and external captures are not serialized.',
               'scope':'Pinned RTA object/core model only. Authored callbacks and virtual addresses; no assets, original derived-class behavior, rendering, gameplay, Norrath or original-client parity claim.',
               'refusals':'Pinned model refusal types retained with partial state. Additional bounded native resource/ownership policy is independently tested, not a Python game-behavior claim.'}}
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='ascii',newline='\n')
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/objects_runtime.tsv.gz'))
    args=parser.parse_args();m=generate(args.research_dir,args.output)
    print(json.dumps({k:m[k] for k in ('vector_count','vector_sha256','expected_trace_sha256','expected_trace_fnv1a64')}))

if __name__=='__main__':main()
