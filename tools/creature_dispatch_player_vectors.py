"""Independent authored cases using pinned PLAYER_DISPATCH plus ref_anim/core.
Movement and combo expectations transcribe the pinned documents' numeric tables.
"""
def vectors(C,A,D):
    out=[]
    def snapshot(sy,st,clips):
        v=[st.count,st.flags,sy.cosmetic_seed]
        for t in st.tracks:v += [next((i+1 for i,c in enumerate(clips) if t.clip is c),0),t.weight,t.rate,t.life,t.life_rate,t.time,t.flags]
        return v
    def selected(state,ops):
        v=[state,len(ops)]
        for op in ops:v += [op[0],len(op)-1,*op[1:]]
        return v
    prefixes=['ohw','dhs','dha','l','b','nw','ohw','dw']
    actions={'shot':(0,0),'script_idle':(0,1),'first_swing':(1,0),'heavy':(1,1),'close_attack':(1,2),'combo':(1,4),'combo_close':(1,5),'script_walk':(1,6),'stagger':(3,0),'knockdown':(3,1),'flinch':(3,2),'death':(4,0),'cast_loop':(5,0),'cast_oneshot':(5,3),'block_pose':(8,0),'block':(8,1),'block_reaction':(8,2),'ground_pound':(10,0),'evade':(11,0),'warcry':(12,0),'ride':(13,0)}
    C.set_fpu()
    for action,(st,idx) in actions.items():
      for cls in range(8):
       if action in ('shot','close_attack','combo_close') and cls not in (4,6):continue
       if action in ('first_swing','combo','warcry') and cls in (4,6):continue
       for model in (range(12) if action=='death' else [0]):
        for variant in (range(1,6) if action=='combo' else [1]):
         for rel in ([-32768,-24576,-8192,0,8192,8193,24576,32767] if action=='knockdown' else [0]):
          for draw in ([0xafc7,0xafc8] if action=='block_reaction' else [0]):
           for no_combat in ([0,1] if action=='script_idle' else [0]):
            ri=idx
            if action=='death':ri=0 if cls==2 and model not in (2,3,6,7) else 1
            if action=='block_reaction' and draw>0xafc7:ri=3
            row=D.PLAYER_DISPATCH[st][ri];key=row['k'];name='-'
            if action in ('close_attack','combo_close'):name='b_attackb.anm' if cls==4 else 'nw_atakseq3.anm'
            elif action=='combo':name=prefixes[cls]+'_atakseq%d.anm'%variant
            elif action=='knockdown':name=('knockdown2_' if cls in (2,3) else 'knockdown1_')+('a' if abs(rel)>=0x6000 else 'c' if rel<=-0x2000 else 'b' if rel<=0x2000 else 'd')+'.anm'
            elif action=='ground_pound':name='dha_quake.anm'
            elif action=='warcry':name=prefixes[cls]+'_warcry.anm'
            elif action=='ride':name='nw_seated.anm'
            if name!='-':entry=100
            elif action=='death':entry=2 if ri==0 else 1
            elif action=='script_idle' and no_combat:entry=4
            else:entry=D.player_entry(cls,key)
            kind='start' if row['fn']=='anim_start' else 'blend'
            args=(entry,row['blend_in'],row['flags']) if kind=='start' else (entry,row['blend_in'],row['blend_out'],row['flags'])
            ops=[(kind,)+args]
            if action=='script_walk':ops += [('sync',)]
            native_ops=ops if name=='-' else [('named_'+kind,101)+ops[0][2:]]
            sy=A.AnimSystem(8,3);sy.sim_frame=17;s=A.AnimState();clips=[A.Clip(A.encode_clip([((0,0,0),(4096,0,0,0))],120)) for _ in range(112)]
            A.anim_push(s,clips[0],1);D.apply_ops(sy,s,dict(enumerate(clips)),ops)
            want=selected(st,native_ops)+[name]+snapshot(sy,s,clips)
            out.append({'input':'player %s %d %d %d %d %d %d'%(action,cls,model,variant,rel,draw,no_combat),'expected':' '.join(map(str,want))})
    # Preserved stale-field reset and locomotion arithmetic are tested with a
    # sequence that crosses each knee in both directions under all 16 FP modes.
    for mode in range(4):
     for bias in range(2):
      for div in range(2):
       C.set_fpu(('ps2','ps2g1','chop','nearest')[mode],bool(bias),bool(div))
       sy=A.AnimSystem(8,3);sy.sim_frame=17;s=A.AnimState();clips=[A.Clip(A.encode_clip([((0,0,0),(4096,0,0,0))],120)) for _ in range(112)]
       s.count=4
       for i,t in enumerate(s.tracks[:4]):t.clip=clips[D.player_entry(0,0x44+8*i)];t.weight=t.life=C.ONE if i==0 else 0;t.flags=0x1205
       s.tracks[4].weight=s.tracks[4].life=0;A.anim_release_caches(s)
       speeds=[0,0x3c23d70a,0x3edc28f4,0x3edc28f5,0x3edc28f6,0x3f199998,0x3f199999,0x3f19999a,0x3f333332,0x3f333333,0x3f333334]+[0x3f400000]*12+[0]*12
       seq=[(x,0) for x in speeds]+[(C.ONE,m) for m in (1,2,3,4,5,6)]
       b=prev=0;want=[]
       for speed,m in seq:
        rate=0;bits=0
        if m in (1,4,6):
         b=0
         for t in s.tracks[1:4]:t.weight=t.life=0
        elif m==0:
         x=speed
         if C.lt(x,0x3edc28f5):rate=C.mul(x,0x4014d653)
         elif C.lt(x,0x3f333333):rate=C.add(C.mul(C.mul(C.sub(x,0x3edc28f5),0x406d097a),0x3eb33330),C.ONE)
         else:rate=C.add(C.mul(C.mul(C.sub(x,0x3f333333),0x40555554),0x3e1999a0),0x3faccccc)
         if C.lt(0x3c23d70a,rate):
          b=C.add(b,0x3dcccccc)
          if C.lt(C.ONE,b):b=C.ONE;s.tracks[0].time=0
         else:
          b=C.sub(b,0x3dcccccc)
          if C.lt(b,0):b=0
         idle=C.lt(rate,0x3c23d70a) and C.lt(b,0x3d4ccccc)
         ev=A.anim_advance(sy,C.ONE if idle else rate,s,sound=False)[0];bits=0 if idle else ev&(1 if C.lt(x,0x3f199999) else 4)
         if C.lt(C.sub(prev,x),0xbd4ccccc):x=C.add(prev,0x3d4ccccc)
         prev=x;s.tracks[1].weight=s.tracks[1].life=b
         for i,k in ((2,0x3fdccccccccccccc),(3,0x3fe6666666666666)):
          s.tracks[i].weight=s.tracks[i].life=C.d2f(C.dmul(C.dmul(C.f2d(b),C.dsub(C.f2d(x),k)),0x4008000000000000))
        want += [rate,b,prev,bits]+snapshot(sy,s,clips)
       out.append({'input':'move %d %d %d %d '%(mode,bias,div,len(seq))+' '.join('%d %d'%p for p in seq),'expected':' '.join(map(str,want))})
    # Independent transition tables from player-animation-dispatch section 3.4.
    def result(m,c,v,h):
      normal0={1:([2,4],[0x7fff]),2:([1,3,5],[0x5555,0xaaaa]),3:([2,4],[0x7fff]),4:([1,3,5],[0x5555,0xaaaa])}
      normal15={1:([2],[]),2:([1,3],[0x7fff]),3:([2],[])}
      normal23={1:([2,4],[0x7fff]),2:([1,3],[0x7fff]),3:([2,4],[0x7fff]),4:([1,3],[0x7fff])}
      if c in (4,6):return v
      if c==7:return 2 if v==1 else 1
      default=([v],[])
      if c==0:table=normal0;default=([2,4],[0xbb80])
      elif c in (1,5):table=normal15
      else:table=normal23
      if m==9 and c==0:
       table={1:([2,3,5],[0x5555,0xaaaa]),2:([1,3],[0x7fff]),3:([4],[]),4:([2,5],[0x7fff]),5:([4,3,1],[0x5555,0xaaaa])};default=([v],[])
      if m==9 and c==5:table={1:([2],[]),2:([3],[]),3:([1],[])}
      if m==11 and c==0:
       table={1:([2,4,5],[0x7000,0xe000]),2:([3,1,5],[0x6aa8,0xd334]),3:([4,2,5],[0x7000,0xe000]),4:([5,1,3],[0x53ed,0xaaaa])};default=([1,3],[0x7fff])
      if m==11 and c==2:table={**normal23,3:([2,4],[0xbb80])}
      if m==11 and c==5:
       table={2:([1,3,4],[0x6d60,0xdac0]),3:([2,4],[0xbb80]),4:([2,3],[0x7fff])};default=([2,4],[0x7fff])
      values,thresholds=table.get(v,default);return values[sum(h>=t for t in thresholds)]
    boundaries=sorted({0,65535}|{x+d for x in (0x53ed,0x5555,0x6aa8,0x6d60,0x7000,0x7fff,0xaaaa,0xbb80,0xd334,0xdac0,0xe000) for d in (-1,0,1)})
    inv=pow(0x19660d,-1,65536)
    for m in range(12):
     for c in range(8):
      if (m==9 and c==1) or (m==11 and c==3):continue
      for v in range(8):
       for h in boundaries:
        frame=((h-0x3c6ef35f)*inv)%65536
        out.append({'input':'combo %d %d %d %d'%(m,c,v,frame),'expected':str(result(m,c,v,h))})
    C.set_fpu()
    return out


def skill_vectors(S):
    """Skill-driven player states from ref_skill_anim (player-animation-dispatch
    section 6). Only a weapon-class-0 player with object flag word 0x40 was
    executed; classes 4 and 6 and the flag word 0 are refusals."""
    out=[]
    actions={'SkillCyclone_cast':'cyclone','SpellBash_ctor':'bash','SpellAncestralCall_ctor':'ancestral_call',
             'SpellShieldBash_ctor':'shield_bash','SpellRoar_ctor':'roar','SpellBattleCry_ctor':'battle_cry',
             'SpellBloodLust_ctor':'blood_lust'}
    refused='REFUSED skill constructor not executed for this weapon class/flags'
    def ops(anim,clip):
        kind,_,flags,bin_,bout=anim
        if kind=='push':return ['named_push',2,clip,flags]
        return ['named_blend',4,clip,bin_,bout,flags]
    for row in S.STATE6:
        action=actions[row['name']]
        names=sorted(row['names'])
        clip=row['anim'][1]
        want=[row['state'],'-',1]+ops(row['anim'],clip)+[len(names)]+names
        out.append({'input':'skill %s 0 %d'%(action,0x40),'expected':' '.join(map(str,want))})
        for cls,flags in ((4,0x40),(6,0x40),(0,0)):
            out.append({'input':'skill %s %d %d'%(action,cls,flags),'expected':refused})
    for flags in (0x40,0):
        out.append({'input':'skill charge 0 %d'%flags,'expected':'%d - 0 0'%S.charge_state(flags)})
    want=[S.SHADOW_STATE,S.SHADOW_FLAGS_AFTER_CTOR,1,'named_push',2,S.SHADOW_CLIP,S.SHADOW_ANIM[2],1,S.SHADOW_CLIP]
    out.append({'input':'skill shadow_step 0 %d'%0x40,'expected':' '.join(map(str,want))})
    want=[S.SHADOW_END_STATE,S.SHADOW_FLAGS_AFTER_DTOR,0,0]
    out.append({'input':'skill shadow_step_end 0 %d'%S.SHADOW_FLAGS_AFTER_CTOR,'expected':' '.join(map(str,want))})
    return out


def precedence_vectors(F):
    """Two-field setState results and the single-field helpers from
    ref_field_precedence (creature-animation-dispatch section 7)."""
    out=[]
    for row in F.COMBINED:
        (f0,v0),(f1,v1)=row['fields']
        want=[row['stored'],len(row['ops'])]
        for fn,entry,flags in row['ops']:
            args=[a for a in (entry,flags) if a is not None]
            want+=[fn,len(args)]+args
        out.append({'input':'combined %s %d %d %d %d %d'%(row['cls'],row['state'],f0,v0,f1,v1),'expected':' '.join(map(str,want))})
    for flags in (0,0x40000,0x40001,0x80000,0x100000,0x200000,0x380000,0xffffffff):
        out.append({'input':'agony %d'%flags,'expected':str(F.agony_hit_entry(flags))})
    for form in range(7):  # executed for 0..6
        e=F.clockwork_idle_entry(form)
        out.append({'input':'clockwork %d'%form,'expected':'none' if e is None else str(e)})
    return out
