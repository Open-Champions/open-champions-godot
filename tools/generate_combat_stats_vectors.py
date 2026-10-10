#!/usr/bin/env python3
"""Pinned published RTA combat stats, authored tables only; no game image input."""
import argparse, collections, hashlib, json, pathlib, subprocess, types, unicodedata
COMMIT='7212ecfecbeae8dd2015f6c82897226f49a94695'
PIN='b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'

def unicode_tables(path):
    if unicodedata.unidata_version!='16.0.0':raise RuntimeError('Unicode16 generator required')
    mappings=[];ranges=[];start=last=kind=0
    for cp in range(0x110000):
        c=chr(cp);low=c.lower()
        if low!=c:
            assert len(low)<=2
            mappings.append((cp,ord(low[0]),ord(low[1]) if len(low)>1 else 0))
        # Public str.lower context probes derive the two classes needed by
        # Final_Sigma. Ignorables take precedence over cased characters.
        before=('A'+c+'Σ').lower().endswith('ς')
        after=('AΣ'+c).lower()[1]=='σ'
        k=2 if after else 1 if before else 0
        if k!=kind:
            if kind:ranges.append((start,cp-1,kind))
            start=cp;kind=k
        if cp==0x10ffff and kind:ranges.append((start,cp,kind))
    s='// Generated from Python str.lower, Unicode16.0.0; no game data.\n#pragma once\n#include <cstdint>\nnamespace champions::simulation::rta::combat::unicode_data {\nstruct Lower { char32_t from,a,b; };\ninline constexpr Lower lower[]={\n'
    s+=''.join('{0x%x,0x%x,0x%x},\n'%r for r in mappings)+'};\nstruct Context { char32_t first,last; int kind; };\ninline constexpr Context context[]={\n'
    s+=''.join('{0x%x,0x%x,%d},\n'%r for r in ranges)+'};\n}\n'
    path.write_text(s,encoding='utf-8',newline='\n')
    return mappings,ranges

def generate(research,output,unicode_output):
    source=subprocess.run(['git','-C',str(research),'show',COMMIT+':tools/ref_combat.py'],check=True,capture_output=True).stdout
    assert hashlib.sha256(source).hexdigest()==PIN
    r=types.ModuleType('reference');exec(compile(source,'pinned_ref_combat.py','exec'),r.__dict__)
    maps,ranges=unicode_tables(unicode_output)
    names=['other','BOSS','gAtE','paired','LaStA','lastB','ΟΣ','İ','АБВ','𐐀','AΣ\u0301','AΣA']
    def setup(n):
        t=r.Tables();t.exp_table=[i*i*100 for i in range(81)]
        t.class_formulas=[tuple(r.itof(x) for x in [5+i,2+i,3,7+i,4,6]) for i in range(7)];t.class_default=tuple(r.itof(x) for x in [1,2,3,4,5,6])
        t.passive_masks=[[0x40,0x40000,0x40]+[0]*45 for _ in range(7)]
        t.figurine_stats=('str','int','dex','sta');t.figurine_tiers=('tiny','small','big','huge')
        t.boss_names=('boss','ος','i\u0307','абв','𐐨','aς\u0301');t.boss_gate='gate';t.boss_pairs=(('paired','wanted'),);t.boss_last=('lasta','lastb');t.no_scale_name='NO_SCALE';t.fixed_levels=('fixed1','fixed2')
        w=r.World((n*0x1234567)&r.M32,(n*0x7654321)&r.M32)
        w.num_players=n%4;w.monster_level=r.itof(1+n%80);w.difficulty=n%7;w.level_name=['field','FIXED1','fixed1','fixed2'][n%4];w.medal_mission=(n//4)%2;w.load_cheat=(n//8)%2
        w.frame=50+n%80;w.pvp_a=(n//2)%2
        defs=[];items=[]
        for i in range(20):
            d=r.ItemDef(name=['strtiny','intsmall','dexbig','stahuge','strnone'][i%5],dmg_min=(n*7+i*3)%80-20,dmg_max=(n*11+i*2)%120-10,armor=(n*3+i)%90-15,attack=(n*13+i)%150-30,category=i%3,subtype=(n+i)%7,dmg_type=[1,4,8,2][(n+i)%4],flags=(1<<45) if i>=17 else 0)
            codes=[0x23,0x0d,0x15,0x16,0x18,0x17,0x26,0,1,2,3,4,5,6,7,8,9,10,0x1e,0x2a]
            mods=[(codes[(n+i+j*3)%len(codes)],(n*17+i*5+j)%161-40,(n*19+i*7+j)%171-20,0) for j in range(4)]
            item=r.Item(defn=d,count=(n+i)%7-2,mods=mods,flags=(1<<36 if n%5==0 else 0)|(1<<43 if n%7==0 else 0));defs.append(d);items.append(item)
        objs=[r.Obj(tag='p'+str(i),flags=0x60,slot=i,pos=[r.itof(i*10),r.itof(i*5),0],state=1 if n%3 else 0,attack_start=w.frame-(n%100),attack_chain=w.frame-(n%40),counter2=(n//4)%2,counter4=(n//8)%2,field_dc=r.itof(2)) for i in range(4)]
        for i in range(4):
            p=r.PlayerRec(cls=(n+i)%7,level58=1+(n*3+i)%79,level59=1+(n+i)%79,base=[(n*7+i*11+j*17)%200-30 for j in range(4)],eff=[(n*3+i*9+j*7)%150-20 for j in range(4)],hp=r.itof(0 if (n+i)%5==0 else 20),max_hp=r.itof(100),mana=r.itof(10),max_mana=r.itof(70),exp=0,skill_points=n,stat_points=n%128,passives=[n%140,(n*3)%140,77]+[0]*45,obj=objs[i] if (n+i)%11 else None)
            p.equip=[items[j] if (n+i+j)%3 else None for j in range(17)];p.weapon_sets=[items[0],items[1]];p.active_set=-1 if n%9==0 else n%2;p.second_weapon=items[1] if n%2 else None;p.bag2=[None,items[17],items[18],items[19]]
            p.skills=[(s,1+(n+s)%9) for s in [0x3f,0x3b,0x3c,0x3d,0x30,0x4f,0x5c,0x47]]+[(-1,0),(0x47,99)];w.players.append(p)
        spells=[r.Spell(kind='AncestralCall',skill=77,level=1+n%5,owner=objs[1]),r.Spell(kind='BloodLust',skill=78,level=2+n%7,owner=objs[0],f2c=n%2),r.Spell(kind='GiantGrowth',skill=79,level=3,owner=objs[2])]
        objs[1].spells=[spells[0]];objs[0].spells=spells[1:];w.alive=[1] if n%3 and w.players[1].obj is not None else []
        def stat(skill,stat,level):
            w.log('stat',skill,stat,level)
            if n%29==0:w.seed.draw();w.players[0].mana=r.fadd(w.players[0].mana,r.ONE)
            if n%97==0:raise ValueError('authored callback')
            return 100+(level*7)%90 if stat==0 else 10+(level*3)%20
        def adjust(skill,level,cap):w.log('adjust',skill,level,cap);return min(level,cap)
        def pool(size):w.log('pool_callback',size);return n%2==0
        def enemy(a,b):w.log('enemy',a.tag if a else None,b.tag if b else None);return n%5!=0
        w.stat_fn=stat;w.adjust_fn=adjust;w.pool_fn=pool;w.enemy_fn=enemy
        return t,w,items,objs
    def txt(s):return '.'.join(str(ord(c)) for c in s) or '-'
    def scalar(v):
        if v is None:return '_'
        if isinstance(v,bool):return str(int(v))
        if isinstance(v,str):return 's'+txt(v)
        if isinstance(v,(list,tuple)):return '['+','.join(scalar(x) for x in v)+']'
        return str(v)
    def state(w,objs):
        # Complete mutable records, links represented by stable tags/indices.
        def rec(x):
            out=[]
            for key in x.FIELDS:
                v=getattr(x,key)
                if isinstance(v,r.Obj):v='@'+v.tag
                elif isinstance(v,r.Item):v='item'+str(xitems.index(v))
                elif key in ('equip','weapon_sets','bag2'):v=[None if z is None else 'item'+str(xitems.index(z)) for z in v]
                elif key=='spells':v=[rec(z) for z in v]
                out.append(scalar(v))
            return '|'.join(out)
        return ';'.join([scalar([w.seed.state,w.seed.draws,w.cosmetic.state,w.cosmetic.draws]),scalar(w.events),scalar([w.frame,w.pvp_a,w.cutscene,w.numbers_option,w.network_mode,w.monster_level,w.focus,w.music,w.music_index,w.num_players,w.alive,w.medal_mission,w.difficulty,w.level_name,w.load_cheat])]+[rec(p) for p in w.players]+[rec(o) for o in objs])
    rows=[];counts=collections.Counter();xitems=[]
    def emit(op,mode,n,args=()):
        nonlocal xitems
        r.set_fpu(['ps2','ps2g1','chop','nearest'][mode//4],bool(mode&2),bool(mode&1))
        t,w,xitems,objs=setup(n);result='-'
        try:
            if op=='lower':result=txt(''.join(map(chr,args)).lower())
            elif op=='boss':result=str(r.boss_rank(t,n%5!=0,None if n%13==0 else names[n%len(names)],'WANTED' if n%2 else '',n))
            elif op=='level':result=str(r.level_scale(t,args[0],n%7,n%2,['other','fixed1','fixed2','FIXED1'][n%4]))
            elif op=='monster':
                v=r.monster_stats(t,w,[r.itof(1+(n+i)%3) for i in range(5)],n%3,[None,'other','no_scale'][n%3],bool(n%2),args[0],bool(n%7==0));result=scalar([v[k] for k in ['hp','max_hp','armor','dmg_low','dmg_high','attack','exp_value']])
            elif op=='monster_raw':
                w.monster_level=args[0]
                v=r.monster_stats(t,w,args[1:],n%3,'other',True,0xffffffff,bool(n%2));result=scalar([v[k] for k in ['hp','max_hp','armor','dmg_low','dmg_high','attack','exp_value']])
            elif op=='passive':result=str(r.passive_level(t,w.players[0],args[0],args[1]))
            elif op=='cap':result=str(r.exp_cap(t,args[0]))
            elif op=='for_exp':result=str(r.level_for_exp(t,args[0]))
            elif op=='item':result=scalar([r.item_armor_value(xitems[0]),r.item_attack_rating(xitems[0]),*r.item_damage_range(xitems[0],bool(n%2))])
            elif op=='armor':result=str(r.recalc_armor(t,w,args[0]))
            elif op=='stats':r.recalc_stats(t,w)
            elif op=='damage':result=scalar(r.recalc_damage(t,w,objs[0],None if n%8==0 else xitems[0],bool(n%2)))
            elif op=='give':
                p=w.players[0];p.exp=t.exp_table[p.level59-1];p.hp=0 if n%5==0 else r.itof(20)
                def level_up(slot):
                    w.log('callback_level',slot);w.cosmetic.draw()
                    if n%17==0:w.players[slot].mana=r.fadd(w.players[slot].mana,r.ONE)
                    if n%31==0:raise ValueError('callback level')
                result=str(r.give_exp(t,w,args[0],args[1],level_up))
            elif op=='kill':
                creature=r.Obj(tag='monster',exp_value=r.itof(args[0]),max_hp=r.itof(900))
                result=scalar(r.kill_experience(t,w,creature,args[1],args[2]))
            elif op=='edge':
                k=args[0];p=w.players[0];p.obj=objs[0];w.num_players=1
                if k==0:
                    p.eff=[(1<<63)-1,-(1<<63),-(1<<63),(1<<63)-1];result=str(r.recalc_armor(t,w,0))
                elif k==1:
                    p.exp=(1<<63)-1;p.stat_points=(1<<63)-1;result=str(r.give_exp(t,w,0,2147483647))
                elif k==2:
                    t.exp_table[0]=-(1<<63);t.exp_table[1]=(1<<63)-1;result=str(r.exp_cap(t,0))
                elif k==3:t.exp_table[80]=(1<<63)-1;result=str(r.exp_cap(t,80))
                elif k==4:result=str(r.passive_level(t,p,1<<32,0x40))
                elif k==5:
                    p.base=[(1<<63)-1,-(1<<63),(1<<40)+7,-(1<<40)-9];r.recalc_stats(t,w)
                elif k in (6,7):
                    xitems[0].defn.subtype=4 if k==6 else 6;p.active_set=0;result=str(r.recalc_armor(t,w,0))
                elif k in (8,9,10):
                    w.alive=[1];w.players[1].obj=objs[1];spell=objs[1].spells[0]
                    if k==8:spell.owner=objs[0]
                    if k==9:objs[0].pos=[r.itof(20000),r.itof(30000),0]
                    if k==10:spell.owner=None;w.pvp_a=0
                    result=scalar(r.recalc_damage(t,w,objs[0],xitems[0],True))
                elif k in (11,12,13,14):
                    w.alive=[];p.cls=5;objs[0].state=1
                    p.hp=r.itof(90 if k==11 else 20);p.max_hp=r.itof(100)
                    if k==12:objs[0].attack_chain=w.frame-27
                    if k==13:objs[0].attack_start=w.frame+60
                    if k==14:objs[0].spells=[];p.skills=[(0x47,0)]
                    result=scalar(r.recalc_damage(t,w,objs[0],xitems[0],True))
                elif k in (15,16):
                    w.alive=[];p.cls=6;objs[0].state=1;objs[2].field_dc=r.HALF if k==15 else r.itof(5)
                    result=scalar(r.recalc_damage(t,w,objs[0],xitems[0],True))
                elif k in (17,18):
                    w.alive=[];p.cls=4;xitems[0].defn.subtype=0;xitems[0].defn.dmg_type=8;xitems[0].flags=0 if k==17 else 1<<36
                    result=scalar(r.recalc_damage(t,w,objs[0],xitems[0],True))
                elif k==19:
                    p.level59=1;p.exp=0
                    def append_player(slot):w.players.append(r.PlayerRec());w.log('append',len(w.players))
                    result=str(r.give_exp(t,w,0,500,append_player))
                elif k==20:
                    xitems[0].defn.armor=-32768;xitems[0].defn.attack=32767;xitems[0].defn.dmg_min=-32768;xitems[0].defn.dmg_max=32767;xitems[0].mods=[(0x23,-32768,0,0),(0x0d,32767,0,0),(0x15,-123,234,0),(0x16,0,32767,0)]
                    result=scalar([r.item_armor_value(xitems[0]),r.item_attack_rating(xitems[0]),*r.item_damage_range(xitems[0],True)])
                elif k==21:
                    t.exp_table[80]=-(1<<63);p.exp=100;p.level59=80;result=str(r.give_exp(t,w,0,1))
                elif k==22:
                    def stat_zero(skill,stat,level):w.log('stat_zero',skill,stat,level);return 0
                    w.stat_fn=stat_zero
                    creature=r.Obj(tag='monster',exp_value=r.itof(300),max_hp=r.itof(20));result=scalar(r.kill_experience(t,w,creature,0,0))
                elif k==23:
                    w.num_players=3;w.medal_mission=0;w.level_name='field';w.monster_level=r.itof(9)
                    v=r.monster_stats(t,w,[r.ONE]*5,0,'no_scale',True,n,False);result=scalar([v[x] for x in ['hp','max_hp','armor','dmg_low','dmg_high','attack','exp_value']])
                elif k==24:
                    w.alive=[];p.cls=0;xitems[0].defn.subtype=4
                    def changing_stat(skill,stat,level):
                        w.log('changing_stat',skill,stat,level)
                        if skill==0x30:xitems[0].defn.subtype=6
                        if skill==0x4f:xitems[0].defn.subtype=0;xitems[0].defn.dmg_type=8;p.cls=4
                        return 25
                    w.stat_fn=changing_stat
                    result=scalar(r.recalc_damage(t,w,objs[0],xitems[0],True))
                elif k==25:
                    w.alive=[1];w.players[1].obj=objs[1];w.players[2].obj=objs[2]
                    def append_alive(skill,stat,level):
                        w.log('append_alive',skill,stat,level)
                        if skill==77 and stat==3:
                            more=objs[0].spells[1];more.kind='AncestralCall';more.skill=88;more.owner=objs[0];objs[2].spells=[more];w.alive.append(2)
                        return 200 if skill==88 else 100
                    w.stat_fn=append_alive
                    result=scalar(r.recalc_damage(t,w,objs[0],None,True))
                elif k in (26,27):
                    p.cls=8 if k==26 else -1;objs[0].slot=5 if k==26 else 0;r.recalc_stats(t,w)
                elif k==28:
                    w.num_players=3
                    def changing_count(skill,stat,level):w.log('changing_count',skill,stat,level);w.num_players=0;return 15
                    w.stat_fn=changing_count;r.recalc_stats(t,w)
            elif op=='error':
                k=args[0]
                if k==0:t.fixed_levels=[];result=str(r.level_scale(t,3,1,0,'x'))
                elif k==1:t.boss_last=[];result=str(r.boss_rank(t,True,'absent','',0))
                elif k==2:w.players[0].cls=-8;result=str(r.passive_level(t,w.players[0],0,0x40))
                elif k==3:w.players[0].passives=[];result=str(r.passive_level(t,w.players[0],0,0x40))
                elif k==4:t.exp_table=[];result=str(r.exp_cap(t,3))
                elif k==5:xitems[0].defn=None;result=str(r.item_armor_value(xitems[0]))
                elif k==6:w.players[0].equip=[];result=str(r.recalc_armor(t,w,0))
                elif k==7:w.players[0].eff=[];result=scalar(r.recalc_damage(t,w,objs[0],None,False))
                elif k==8:w.players[0].cls=-1;result=str(r.passive_level(t,w.players[0],0,0x40))
                elif k==9:w.players[0].active_set=-3;result=str(r.recalc_armor(t,w,0))
                elif k==10:w.alive=[-1];w.players[-1].obj=None;result=scalar(r.recalc_damage(t,w,objs[0],None,False))
                elif k==11:w.players[0].base=[];w.num_players=1;r.recalc_stats(t,w)
            else:raise AssertionError(op)
        except (IndexError,AttributeError,ValueError):result='!Error'
        st='-' if op=='lower' else state(w,objs)
        row='\t'.join(['combat_stats',op,','.join(map(str,[mode,n,*args])),result,st]);rows.append(row);counts[op]+=1
    for mode in range(16):
        for n in range(1,161):
            for op in ['boss','level','monster','passive','cap','for_exp','item','armor','stats','damage','give','kill']:
                args={'level':[(n*7)%256],'monster':[(n*123456789)&r.M32],'passive':[n%8-2,[0x40,0x40000,0,0xdead][n%4]],'cap':[[-128,-81,-80,-1,0,1,79,80,81,127,255][n%11]],'for_exp':[n*n*39-30],'armor':[[-1,0,1,4][n%4]],'give':[0,[0,-17,1234,600000,2147483647][n%5]],'kill':[100+(n*73)%20000,[-1,0,1][n%3],(n*3456789)&r.M32]}.get(op,[])
                emit(op,mode,n,args)
        for k in range(12):emit('error',mode,7,[k])
        for k in range(29):
            for n in [7,29,63]:emit('edge',mode,n,[k])
        raw_cases=[0,0x80000000,1,0x00800000,0x3f000001,0xbf800000,0x7f800000,0x7fffffff]
        for j,raw in enumerate(raw_cases):emit('monster_raw',mode,7+j,[raw,*[raw_cases[(j+i)%8] for i in range(5)]])
    for cp,_,_ in maps:
        emit('lower',0,1,[cp]);emit('lower',0,1,[65,cp,0x3a3]);emit('lower',0,1,[65,0x3a3,cp])
    for a,b,_ in ranges:
        for cp in {a,b}:emit('lower',0,1,[65,0x3a3,cp,65]);emit('lower',0,1,[65,cp,0x3a3])
    for s in ['AΣ\u0301','AΣ\u0301A','Σ','ΟΣ','İΣ','AΣ\u0345','AΣ\u0345A','AΣ\ud800','𐐀İΣ']:
        emit('lower',0,1,list(map(ord,s)))
    data=('# Authored RTA stats vectors; pinned reference, no retail tables\n'+'\n'.join(rows)+'\n').encode();output.write_bytes(data)
    meta=dict(schema=1,reference='tools/ref_combat.py',reference_commit=COMMIT,reference_sha256=PIN,generator_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),vector_sha256=hashlib.sha256(data).hexdigest(),rows=len(rows),operations=dict(counts),expected_trace_sha256=hashlib.sha256(('\n'.join(rows)+'\n').encode()).hexdigest(),unicode_version=unicodedata.unidata_version,unicode_header_sha256=hashlib.sha256(unicode_output.read_bytes()).hexdigest(),scope='Published RTA model; authored supplied tables; all16 FPU configurations; no executable/table extraction or Norrath claim.')
    output.with_suffix('.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--research-dir',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/combat_stats.tsv'));p.add_argument('--unicode-output',type=pathlib.Path,default=pathlib.Path('native/include/champions/combat_unicode_data.hpp'));a=p.parse_args();generate(a.research_dir,a.output,a.unicode_output)
