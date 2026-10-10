#!/usr/bin/env python3
"""Independent, asset-free combat to Creature HP composition vectors.

Only supplied memory/callback recipes are authored here. HP arithmetic, pointer
selection, write order and callback dispatch execute the pinned Python method.
"""
import argparse
import collections
import gzip
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import types

REFERENCES = {
    'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686', 'path': 'tools/ref_core.py', 'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
    'combat': {'commit': '7212ecfecbeae8dd2015f6c82897226f49a94695', 'path': 'tools/ref_combat.py', 'sha256': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
    'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'tools/ref_creature.py', 'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
}
REFERENCE_SHA256 = hashlib.sha256(json.dumps(REFERENCES, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
SPECIFICATION_EVIDENCE = {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9', 'path': 'docs/spec/creature-tick.md', 'sha256': 'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
COMPOSITION_POLICY = 'rta-combat-creature-v1'
MODELS = ('ps2', 'ps2g1', 'chop', 'nearest')

def sc(value):
    if value is None: return '_'
    if isinstance(value, bool): return str(int(value))
    if isinstance(value, bytes): return 'b' + value.hex()
    if isinstance(value, str): return 's' + value.encode('utf-8').hex()
    if isinstance(value, (list, tuple)): return '[' + ','.join(map(sc, value)) + ']'
    if isinstance(value, int): return str(value)
    raise TypeError('noninteger or unowned creature state')


def load_references(research_dir, scratch_dir):
    """Never import a live research working-tree file or follow its module path."""
    scratch_dir = pathlib.Path(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)
    modules = {}
    previous, old_path = sys.modules.get('ref_core'), sys.path[:]
    try:
        with tempfile.TemporaryDirectory(prefix='pinned-', dir=scratch_dir) as directory:
            for name in ('core', 'combat', 'creature', 'specification'):
                pin = SPECIFICATION_EVIDENCE if name == 'specification' else REFERENCES[name]
                source = subprocess.run(['git', '-C', str(research_dir), 'show', pin['commit'] + ':' + pin['path']],
                                        capture_output=True, check=True, timeout=30).stdout
                if hashlib.sha256(source).hexdigest() != pin['sha256']:
                    raise ValueError('immutable reference hash mismatch: ' + name)
                path = pathlib.Path(directory) / pathlib.Path(pin['path']).name
                path.write_bytes(source)
                if name == 'specification': continue
                module = types.ModuleType('pinned_combat_creature_' + name)
                module.__file__ = str(path)
                sys.modules[module.__name__] = module
                exec(compile(source, str(path), 'exec'), module.__dict__)
                modules[name] = module
                if name == 'core': sys.modules['ref_core'] = module
    finally:
        sys.path[:] = old_path
        if previous is None: sys.modules.pop('ref_core', None)
        else: sys.modules['ref_core'] = previous
    return modules


class CallbackFailure(Exception):
    pass


# All fields are serialized, even fields not read by a particular recipe.
OBJECT_FIELDS = ('tag flags pos facing owner_link owner cls innoruuk_immune res flash cflags kind dot field_dc armor stamp_f0 hp max_hp status poison_a poison_b status_source spells counter2 counter4 counter11 counter12 slot state attack_start attack_chain bonus model_points boss_rank exp_value own_seed last_attacker').split()
PLAYER_FIELDS = ('cls level58 level59 exp base eff hp max_hp mana max_mana attack dmg_min dmg_max stagger skill_points stat_points passives load_limit bag2 obj equip weapon_sets second_weapon active_set skills').split()
SPELL_FIELDS = 'kind skill level owner f18 f1c f20 f2c'.split()
WORLD_FIELDS = ('frame pvp_a cutscene numbers_option network_mode monster_level focus music music_index num_players alive medal_mission difficulty level_name load_cheat').split()
WORLD_CALLBACKS = 'stat_fn stat_for_fn adjust_fn pool_fn enemy_fn take_damage_fn'.split()
SCENARIOS = ('normal', 'zero', 'negative', 'repeat_lethal', 'inert', 'ignore', 'combat_status',
             'projectile', 'effect', 'null_node', 'null_target', 'stale_target', 'shared_handles',
             'self', 'reflection', 'frost', 'thorns', 'dot_delivery', 'dot_pending', 'miss',
             'raw_hp_mutation', 'raw_route_mutation', 'clear_throw', 'link_throw',
             'nested_callback', 'nested_throw', 'ignore_status', 'custom', 'no_projection',
             'status_publish', 'callback_rng', 'frame_mutation', 'effect_null_node',
             'effect_null_target', 'reflection_throw', 'nested_damage')
ADDRESSES = (0x10000, 0x20000, 0x30000)
REGIONS = ((0x50000, 16, 'owner'), (0x50100, 16, 'effect'),
           (0x51000, 48, 'last_handles'))
SEEDS = (1, 0xdeadbeef)


class Oracle:
    """Authored storage boundary only; all combat/HP arithmetic uses pinned bodies.

    Scalar projection never guesses pointer-node storage. Every handler below is
    an explicit synthetic backend, not a reconstructed game handle allocator.
    """
    def __init__(self, modules, config, variant, scenario):
        self.core, self.r, self.ref = modules['core'], modules['combat'], modules['creature']
        settings = (MODELS[config // 4], bool(config & 2), bool(config & 1))
        self.core.set_fpu(*settings); self.r.set_fpu(*settings)
        for cls, fields in ((self.r.Obj, OBJECT_FIELDS), (self.r.PlayerRec, PLAYER_FIELDS),
                            (self.r.Spell, SPELL_FIELDS)):
            if list(cls.FIELDS) != fields: raise AssertionError('record contract changed')
        self.world = self.r.World(SEEDS[variant], SEEDS[1 - variant])
        self.objects = [self.r.Obj(tag=n, flags=32, hp=0x42c80000, max_hp=0x42c80000,
                                  pos=[self.core.from_int(i + 1), self.core.from_int(variant), 0],
                                  own_seed=0xabcdef00+i) for i, n in enumerate(('victim', 'attacker', 'owner'))]
        self.spells = []
        self.bindings = [[a, i+1, 0x3b2, 0, 0, ''] for i,a in enumerate(ADDRESSES)]
        self.regions = [list(x) for x in REGIONS]
        self.callbacks = [['handle_clear', 'authored_clear'], ['handle_link', 'authored_link']]
        self.effects, self.pokes, self.results = [], [], []
        self.raw_depth = self.depth = self.callback_count = 0
        self.scenario, self.variant = scenario, variant
        self.nested = False
        owner = self
        class Traced(self.ref.Model):
            def _w(model, address, size, value):
                owner.effects.append(['write', address, size, value & ((1 << (8*size))-1)])
                return super()._w(address, size, value)
            def call(model, name, *args):
                owner.effects.append(['call', name, list(args)])
                return super().call(name, *args)
        self.model = Traced(self.ref.Memory())
        self.model.cb = {name: (lambda *args, name=name: self.raw_scope(self.handle, name, args))
                         for name in ('handle_clear', 'handle_link')}
        self.world.take_damage_fn = self.dispatch
        self.world.stat_fn = lambda skill, stat, level: self.stat('stat', skill, stat, level)
        self.world.stat_for_fn = lambda skill, stat, level, caster, cap: self.stat('stat_for', skill, stat, level, self.ident(caster), cap)
        self.world.adjust_fn = lambda skill, level, n: level
        self.world.pool_fn = lambda size: False
        self.world.enemy_fn = lambda a,b: 1
        self.world.frame = 32 + variant
        self.world.numbers_option = 0
        self.world.level_name = 'authored'
        self.project()
        for i, a in enumerate(ADDRESSES):
            for offset, size, value in ((0xc8,4,0x3f000000),(0x231,1,0),(0x2dc,4,variant),
                                        (0x2e0,2,0x1234),(0x2e4,4,0),(0x304,4,7),(0x3b0,2,9),
                                        (0x4c,4,0),(0x360,4,0)):
                self.model.mem.poke(a+offset,size,value)
        for address,size,_ in self.regions:
            for offset in range(0,size,4): self.model.mem.poke(address+offset,4,0)

    def ident(self, obj):
        if obj is None: return None
        for i, candidate in enumerate(self.objects):
            if candidate is obj: return i
        raise ValueError('unregistered authored object identity')

    def address(self, obj):
        return 0 if obj is None else self.bindings[self.ident(obj)][0]

    def project(self):
        p = self.model.mem.poke
        for obj, binding in zip(self.objects, self.bindings):
            if binding[3]: continue
            address = binding[0]
            p(address+0xc,4,obj.flags)
            for i,value in enumerate(obj.pos): p(address+0x10+4*i,4,value)
            p(address+0xa8,8,obj.cflags); p(address+0x228,4,obj.hp)
            for i,value in enumerate(obj.status): p(address+0x340+4*i,4,value)
        p(0x44fdcc,4,self.world.frame)

    def publish(self):
        load = self.model.mem.load
        for obj, binding in zip(self.objects, self.bindings):
            if binding[3]: continue
            address = binding[0]
            obj.flags = load(address+0xc,4)
            obj.pos = [load(address+0x10+4*i,4) for i in range(3)]
            obj.cflags = load(address+0xa8,8); obj.hp = load(address+0x228,4)
            obj.status = [load(address+0x340+4*i,4) for i in range(5)]
        self.world.frame = load(0x44fdcc,4)

    def poke(self, address, size, value):
        value &= (1 << (8*size))-1
        self.pokes.append([self.callback_count,address,size,value])
        self.effects.append(['poke',address,size,value])
        self.model.mem.poke(address,size,value)

    def raw_scope(self, function, *args):
        self.publish(); self.raw_depth += 1
        try: return function(*args)
        finally:
            self.publish(); self.raw_depth -= 1

    def stat(self, name, skill, stat, level, *args):
        self.effects.append([name,skill,stat,level,*args])
        if self.scenario == 'frost': return (0,25,100)[stat % 3]
        if self.scenario == 'thorns': return (3,100,0)[stat % 3]
        return 0

    def handle(self, name, args):
        self.callback_count += 1
        self.effects.append(['handler',name,self.callback_count,list(args)])
        head = args[0]
        if name == 'handle_clear':
            self.poke(head,4,0)
            if self.scenario == 'raw_hp_mutation':
                self.poke(ADDRESSES[0]+0x228,4,0x41200000)
                self.poke(ADDRESSES[0]+0xc8,4,0x40000000)
            if self.scenario == 'raw_route_mutation':
                self.poke(ADDRESSES[1]+0xc,4,0xa0)
                self.poke(ADDRESSES[1]+0x4c,4,REGIONS[0][0])
                self.poke(REGIONS[0][0]+0xc,4,ADDRESSES[2])
            if self.scenario == 'clear_throw': raise CallbackFailure()
            if self.scenario in ('nested_callback','nested_throw','nested_damage') and not self.nested:
                self.nested = True
                self.poke(ADDRESSES[1]+0x228,4,0x41a00000)
                if self.scenario == 'nested_damage':
                    desc=[(0x15,2,3,0),(-1,0,0,0),(-1,0,0,0),(-1,0,0,0)]
                    self.invoke('nested',lambda:self.damage(self.objects[1],self.objects[2],flags=0x40,desc=desc),catch=False)
                else:
                    self.invoke('nested', lambda: self.take(self.objects[1],0x40000000,self.objects[2],0x40), catch=False)
            if self.scenario == 'frame_mutation': self.poke(0x44fdcc,4,99+self.variant)
        else:
            index = (head-0x2e4-ADDRESSES[0])//0x10000
            node = 0x51000 + 16*index
            self.poke(node+0xc,4,args[1]); self.poke(head,4,node)
            if self.scenario in ('link_throw','reflection_throw') or (self.scenario == 'nested_throw' and self.nested):
                raise CallbackFailure()
        if self.scenario == 'callback_rng':
            self.world.seed.draw(); self.world.cosmetic.draw()

    def before(self, obj, amount, attacker, flags):
        if self.scenario != 'status_publish': return
        self.callback_count += 1
        self.effects.append(['before',self.ident(obj),amount,self.ident(attacker),flags])
        node = REGIONS[1][0]
        self.poke(node+0xc,4,self.address(obj.status_source))
        self.poke(self.address(obj)+0x360,4,node)

    def dispatch(self, obj, amount, attacker, flags):
        if self.bindings[self.ident(obj)][4]:
            self.callback_count += 1
            self.effects.append(['custom',self.ident(obj),amount,self.ident(attacker),flags])
            if self.bindings[self.ident(obj)][3]:
                self.model.take_damage(self.address(obj),self.address(attacker),flags,amount)
            else: obj.hp = self.r.fsub(obj.hp,amount)
            return
        self.project()
        try:
            if self.scenario == 'status_publish': self.raw_scope(self.before,obj,amount,attacker,flags)
            self.model.take_damage(self.address(obj),self.address(attacker),flags,amount)
        finally: self.publish()

    def take(self, obj, amount, attacker, flags):
        if self.raw_depth: self.publish()
        self.depth += 1
        try: return self.world.take_damage(obj,amount,attacker,flags)
        finally:
            if self.raw_depth: self.project()
            self.depth -= 1

    def damage(self, obj, attacker, minimum=0x40000000, maximum=0x40a00000, attack=100000, flags=0x40, desc=None):
        if self.raw_depth: self.publish()
        self.depth += 1
        try: return self.r.damage_apply(self.world,obj,attacker,minimum,maximum,attack,flags,desc,1)
        finally:
            if self.raw_depth: self.project()
            self.depth -= 1

    def invoke(self, label, function, catch=True):
        try:
            value = function(); self.results.append([label,'ok',value])
            return value
        except CallbackFailure:
            self.results.append([label,'CallbackFailure',None])
            if not catch: raise

    def snapshot(self):
        def record(obj, fields):
            result=[]
            for name in fields:
                value=getattr(obj,name)
                if name in ('owner','status_source','last_attacker','obj'): value=self.ident(value)
                elif name == 'spells': value=[self.spells.index(s) for s in value]
                result.append(value)
            return result
        world = [[self.world.seed.state,self.world.seed.draws],
                 [self.world.cosmetic.state,self.world.cosmetic.draws],
                 [getattr(self.world,n) for n in WORLD_FIELDS],
                 [record(p,PLAYER_FIELDS) for p in self.world.players],self.world.events,
                 [bool(getattr(self.world,n)) for n in WORLD_CALLBACKS]]
        mem=self.model.mem
        return [self.results,world,[record(o,OBJECT_FIELDS) for o in self.objects],
                [record(s,SPELL_FIELDS) for s in self.spells],[],[],
                self.bindings,self.regions,sorted(self.callbacks),
                sorted([a,v] for a,v in mem.bytes.items()), self.model.writes,mem.log,
                [[name,list(args)] for name,args in self.model.calls],self.effects,self.pokes,
                self.model.draws,self.depth,self.raw_depth]


def run_case(modules, config, variant, scenario):
    o=Oracle(modules,config,variant,scenario)
    r=o.r; v,a,owner=o.objects; V,A,O=ADDRESSES
    p=o.model.mem.poke
    amount=(0x40800000,0x40a00000)[variant]
    flags=0x40
    def hit(): return o.take(v,amount,a,flags)
    def fight(): return o.damage(v,a,flags=flags)
    action=hit
    if scenario == 'zero': amount=(0,0x80000000)[variant]
    elif scenario == 'negative': amount=(0xbf800000,0xc1200000)[variant]
    elif scenario == 'repeat_lethal':
        v.hp=0x3f800000
        action=lambda: [o.take(v,amount,a,flags),o.take(v,amount,owner,flags)]
    elif scenario == 'inert': p(V+0x231,1,(1,255)[variant])
    elif scenario == 'ignore': v.cflags |= 0x1000000000000;v.status=[0x3f800000+i for i in range(5)]
    elif scenario in ('combat_status','ignore_status'):
        if scenario == 'ignore_status': v.cflags |= 0x1000000000000
        desc=[(0x15,2,3,0),(-1,0,0,0),(-1,0,0,0),(-1,0,0,0)]
        action=lambda:o.damage(v,a,flags=flags,desc=desc)
    elif scenario in ('projectile','null_node','null_target','stale_target','raw_route_mutation'):
        a.flags |= 0x80;a.owner_link=True;a.owner=owner;a.cls='Projectile';owner.cls='Owner'
        p(A+0x4c,4,0 if scenario=='null_node' else 0x50000)
        p(0x5000c,4,0 if scenario=='null_target' else 0xdead1000 if scenario=='stale_target' else O)
        flags |= 0x10000000
        p(V+0x360,4,0x50100);p(0x5010c,4,V)
        if scenario=='raw_route_mutation':a.flags=32;p(A+0x4c,4,0)
    elif scenario in ('effect','shared_handles','effect_null_node','effect_null_target'):
        flags |= 0x10000000;v.status_source=owner
        p(V+0x360,4,0 if scenario=='effect_null_node' else 0x50100)
        p(0x5010c,4,0 if scenario=='effect_null_target' else O)
        if scenario=='shared_handles':
            p(A+0x360,4,0x50100);a.status_source=owner
            action=lambda:[o.take(v,amount,a,flags),o.take(a,amount,v,flags)]
    elif scenario=='self': action=lambda:o.take(v,amount,v,flags)
    elif scenario in ('reflection','reflection_throw'): v.counter12=1;a.kind=0x10;action=fight
    elif scenario=='frost':
        spell=r.Spell(kind='FrostArmor',skill=7,level=1,owner=v)
        o.spells.append(spell);v.spells.append(spell);flags=1;action=fight
    elif scenario=='thorns':v.cflags |= r.CF_THORNS;flags=1;action=fight
    elif scenario in ('dot_delivery','dot_pending'):
        o.world.frame=(32 if scenario=='dot_delivery' else 33)+32*variant
        flags=0x800000;v.dot=0x3fc00000;action=fight
    elif scenario=='miss':
        v.armor=1;action=lambda:o.damage(v,a,attack=0,flags=flags)
    elif scenario=='custom':
        o.bindings[0][4]=1;o.bindings[0][5]='authored_custom';v.flags=0
        action=fight
    elif scenario=='no_projection':
        o.bindings[0][3]=1;o.bindings[0][4]=1;o.bindings[0][5]='authored_raw_custom'
        # Raw and typed HP intentionally differ; publication must not collapse them.
        p(V+0x228,4,0x41200000);v.hp=0x42f00000
    elif scenario=='status_publish':
        o.callbacks.append(['before_base','authored_status'])
        r.set_status(o.world,v,0,0x40a00000,owner)
        flags=0x10000000
    elif scenario in ('raw_hp_mutation','clear_throw','link_throw','nested_callback','nested_throw','nested_damage','callback_rng','frame_mutation'):
        pass
    elif scenario!='normal':raise ValueError('unknown recipe')
    o.invoke('entry',action)
    return o.snapshot()


def validate_anchors(modules):
    """Small independently calculated branch checks, not native observations."""
    def case(name): return run_case(modules,0,0,name)
    def hp(state, index=0): return state[2][index][OBJECT_FIELDS.index('hp')]
    normal=case('normal')
    assert hp(normal)==0x42c00000  # 100-4=96
    assert hp(case('repeat_lethal'))==0xc0e00000  # 1-4-4=-7
    assert case('inert')[10]==[]
    ignored=case('ignore')
    assert hp(ignored)==0x42c80000 and ignored[2][0][OBJECT_FIELDS.index('status')]==[0]*5
    assert case('miss')[0]==[['entry','ok',0]]
    assert case('stale_target')[12][-1][1][1:]==[0xdead1000,0xdead1000]
    assert len(case('effect_null_target')[12])==1 and len(case('effect_null_node')[12])==2
    assert len(case('dot_delivery')[12])==4 and len(case('dot_pending')[12])==2
    raw=case('no_projection');memory=dict(raw[9])
    assert hp(raw)==0x42f00000
    assert sum(memory[ADDRESSES[0]+0x228+i]<<(8*i) for i in range(4))==0x40c00000
    nested=case('nested_damage')
    assert nested[2][1][OBJECT_FIELDS.index('status')]==[0x40000000,0,0,0,0]
    failed=case('reflection_throw')
    assert hp(failed)==0x42c80000 and hp(failed,1)==0x42c80000
    assert failed[2][1][OBJECT_FIELDS.index('counter11')]==0


def generate(research_dir, output, scratch_dir=None):
    if scratch_dir is None: scratch_dir=pathlib.Path(__file__).resolve().parents[1]/'work'/'combat-creature-generator'
    modules=load_references(research_dir,scratch_dir)
    validate_anchors(modules)
    output=pathlib.Path(output)
    if not output.name.endswith('.tsv.gz'): raise ValueError('output must end in .tsv.gz')
    output.parent.mkdir(parents=True,exist_ok=True)
    vector_hash,trace_hash=hashlib.sha256(),hashlib.sha256()
    fnv=14695981039346656037;counts=collections.Counter();number=0
    with output.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as stream:
        header=b'# combat_creature_vectors_v1\n';stream.write(header);vector_hash.update(header)
        for config in range(16):
            for variant in range(2):
                for scenario in SCENARIOS:
                    first=sc(run_case(modules,config,variant,scenario))
                    second=sc(run_case(modules,config,variant,scenario))
                    if first!=second:raise AssertionError('fresh independent executions differ')
                    row=('\t'.join((scenario,str(config),f'{number};{variant}','ok',first))+'\n').encode('ascii')
                    stream.write(row);vector_hash.update(row);trace_hash.update(row)
                    for b in row:fnv=((fnv^b)*1099511628211)&0xffffffffffffffff
                    counts[scenario]+=1;number+=1
    manifest={
        'schema_version':1,'suite':'combat-creature','composition_policy':COMPOSITION_POLICY,
        'reference_sha256':REFERENCE_SHA256,'references':REFERENCES,'specification_evidence':SPECIFICATION_EVIDENCE,
        'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'vector_count':number,'vector_sha256':vector_hash.hexdigest(),
        'compressed_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
        'expected_trace_sha256':trace_hash.hexdigest(),'expected_trace_fnv1a64':f'{fnv:016x}',
        'operations':dict(sorted(counts.items())),'replicas_per_case':2,
        'settings':{'models':list(MODELS),'mul_bias':[False,True],'div_nearest':[False,True]},
        'trace_columns':['operation','configuration','id;variant','status','complete_composed_state'],
        'fields':{'objects':OBJECT_FIELDS,'players':PLAYER_FIELDS,'spells':SPELL_FIELDS,'world':WORLD_FIELDS,'world_callbacks':WORLD_CALLBACKS},
        'composition_contract':{
            'pins':'All modules and specification are read from immutable Git objects, SHA256 verified and temporarily loaded under ignored scratch. No C++ results or live research imports feed expectations.',
            'authority':'Each base HP call projects flags32/pos3/cflags64/hp/status5 and frame32 from typed records. All bindings publish on normal and exceptional exits. Raw callbacks publish before/after; nested operations publish first and project resulting legitimate combat changes on exit.',
            'handles':'No automatic owner/status_source/last_attacker pointer projection. Explicit authored node backing preserves absent/null/stale/shared values; before_base status recipe publishes an explicitly selected typed source. This is not the game HandlePool.',
            'identities':'Three stable owned object records use authored addresses and insertion generations, no removal/reuse. Every field and registered spelling identity is serialized, including fields untouched by this primitive. No external mutable record is used.',
            'traces':'Full sparse byte map including materialized zero bytes, Model writes, memory logs, calls, interleaved authored/model effects, callback pokes, complete combat World/records and separate shared/cosmetic draws. No native addresses or weak hashes substitute for state.',
            'bounds':'Native representability, region overlap, history/depth limits and unbound record refusals are separately hand-tested wrapper policies. Synthetic callbacks throw without rollback or a latch.',
            'scope':'Base Creature HP only, explicit custom dispatch and projection opt-out. No subclass virtual lookup, death processing, loot, Session integration, original pool implementation, retail-runtime or Norrath claim.',
        },
    }
    output.with_name(output.name[:-7]+'.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='ascii',newline='\n')
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--research-dir',type=pathlib.Path,required=True)
    p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/combat_creature.tsv.gz'))
    p.add_argument('--scratch-dir',type=pathlib.Path)
    args=p.parse_args();m=generate(args.research_dir,args.output,args.scratch_dir)
    print(json.dumps({k:m[k] for k in ('vector_count','vector_sha256','expected_trace_sha256','expected_trace_fnv1a64')}))

if __name__=='__main__':main()
