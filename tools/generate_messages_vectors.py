#!/usr/bin/env python3
"""Synthetic RTA message transport/encoder vectors driven by the pinned model.
Never loads executables, original tables, or private inputs; research is read-only.
"""
import argparse
import ast
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import random
import subprocess
import sys
from types import SimpleNamespace

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_messages.py': 'af6ea5fea7ccb6a06ba9992547a87062d9c3070b7ab0e9737241274def52e12c',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_items.py': '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e',
    'tools/difftest_messages.py': 'efa79458e2bc239b8e6b83983d365b779ca09fb941b62241be470b6837b2e5ec',
    'docs/spec/game-messages.md': 'daa037829a8c182e4c925c32bd7c3b3647cc08a88af4230cdde922155de46fd8',
}
KINDS = [('drop',7),('equip',3),('unequip',2),('join_panel',3),('weapon_class',2),
         ('stat_point',2),('skill_point',3),('combine',4),('level_ack',2),('use_potion',5),
         ('quick_slot',3),('assign_spell',4),('menu_state',3),('recall',1),('player_left',1),
         ('savepoint',1),('disconnect_request',1),('new_world',0),('exit_countdown',0),
         ('checksum_start',0),('checksum',3),('load_level',0),('medal_ack',0),
         ('quick_cast',2),('buy',2),('sell',7),('shop_closed',1)]
SCHEMA = 'rta-messages-transport-v1'
SERVICES = ('net_isLocalSlot','res_find','sound_playOnObject','text_lookup','hud_showMessage',
    'dialog_isOpen','task_isRunning','pauseMenu_close','game_inNoSaveZone','pool_new',
    'RecallEffect_ctor','audio_streamStatus','stream_queueStop','net_closeLocalMenus',
    'Player_takeDamage_virtual','Player_processDamage','net_isSlotActive','player_respawn',
    'net_disconnectPlayer','Savepoint_startSavedEffect','player_resetIdleAnim','player_recalcStats', 'net_allPlayersState2','lobby_sendCharacter',
    'vars_get','vars_set','hud_showHelpMessage','player_recalcLoad','player_countPotions',
    'world_findFreeSpot','ItemProp_ctor2','inventory_selectIdleAnim','script_runByName',
    'hud_showPlayerNotice','Player_weaponSwapInput','inventory_refreshPanelModel',
    'inventory_combineGems','Player_usePotion')

def digest(data):
    return hashlib.sha256(data).hexdigest()

def bhex(data):
    return data.hex() or '-'

def action(op, *args):
    return ' '.join([op] + [format(x & ((1 << 64)-1), 'x') if isinstance(x,int) else x for x in args])

class CallbackError(Exception):
    pass

def expected(R, script):
    mem = R.Memory()
    slot = 0
    backing_calls = []
    backing_on = False
    local_throw = external_throw = False
    local_mutation = False
    mutation = None
    services = {}
    tables = SimpleNamespace(by_addr={}, circlet_stem_equip=b'authored-circlet', thrown_stem=b'authored-thrown',
        weapon_mult=lambda _:0x3f800000)
    spot = None
    service_throw = None
    def local(_):
        if local_mutation and mutation:
            mem.poke(*mutation)
        if local_throw:
            raise CallbackError()
        return slot
    def external(*_):
        if mutation:
            mem.poke(*mutation)
        if external_throw:
            raise CallbackError()
    def service(name):
        def run(*args):
            if mutation:
                mem.poke(*mutation)
            if name == service_throw:
                raise CallbackError()
            return spot if name == "world_findFreeSpot" and spot is not None else services.get(name, 0)
        return run
    machine = R.Machine(mem, tables=tables, callbacks={'net_getLocalPlayerSlot':local,
        **{name:external for name in ('cutscene_onMsg','voice_onMsg','dialog_onMsg','text_onMsg')},
        **{name:service(name) for name in SERVICES}})
    def general_value(token):
        tag, value = token.split('=',1)
        if tag=='i': return int(value,16)
        if tag=='b': return bytes.fromhex(value) if value!='-' else b''
        if tag=='n': return None
        if tag=='t': return value
        raise AssertionError(tag)
    def general_text(value):
        if value is None: return 'n=-'
        if isinstance(value,int): return 'i='+format(value,'x')
        if isinstance(value,bytes): return 'b='+bhex(value)
        return 't='+value
    results = []
    for command in script.split(';'):
        t = command.split()
        op = t[0]
        n = lambda i: int(t[i],16)
        payload = lambda i: bytes.fromhex(t[i]) if t[i] != '-' else b''
        try:
            if op == 'backing':
                backing_on = True
                value=n(1)
                def backing(addr,size):
                    backing_calls.append((addr,size))
                    if tmode[0]=='throw': raise CallbackError()
                    if tmode[0]=='mutate': mem.poke(addr+1,1,0xee)
                    return value
                tmode=[t[2]]
                mem.under=SimpleNamespace(load=backing)
            elif op in ('gpoke','gstore'):
                getattr(mem,op[1:])(n(1),n(2),n(3))
            elif op == 'gload':
                results.append('integer='+format(mem.load(n(1),n(2)),'x'))
            elif op == 'gcall':
                mode=t[2]
                def callback(*values):
                    if mode=='throw': raise CallbackError()
                    return values[-1] if values else None
                if mode=='missing': machine.callbacks.pop(t[1],None)
                else: machine.callbacks[t[1]]=callback
                results.append('general='+general_text(machine.call(t[1],*(general_value(v) for v in t[3:]))))
            elif op == 'definition':
                d = R.RI.Definition()
                d.addr=n(1)&R.M32; d.category=R.s32(n(2)); d.slot=R.s32(n(3)); d.subtype=R.s32(n(4))
                d.price=R.s32(n(5)); d.damage_bits=n(6)&R.M32; d.flags=n(7); d.name=payload(8)
                d.level=d.weight=d.count=d.damage_min=d.damage_max=d.armor=d.attack=0
                tables.by_addr[d.addr]=d
            elif op == 'spot':
                spot=tuple(n(i)&R.M32 for i in range(1,4))
            elif op == 'sound':
                keys=('wood_large','wood_small','wood','chain','shield','plate','metal_large','metal_small','metal','leather','cloth','potion','other')
                result=R.sound_name(dict(zip(keys,(0x1000+17*i for i in range(13)))),n(1),R.s64(n(2)),n(3),n(4)!=0)
                results.append(format(result,'x'))
            elif op == 'shop':
                machine.shop_on_msg(payload(1)); results.append('ok')
            elif op == 'inv':
                machine.inv_dispatch(payload(1)); results.append('ok')
            elif op == 'signed64':
                results.append('signed='+str(R.s64(n(1))))
            elif op == 'poke':
                mem.poke(n(1)&R.M32,n(2),n(3))
            elif op == 'local':
                slot = n(1)&R.M32
            elif op == 'throwlocal':
                local_throw = True
            elif op == 'mutatelocal':
                local_mutation = True
            elif op == 'throwexternal':
                external_throw = True
            elif op == 'mutate':
                mutation = (n(1)&R.M32,n(2),n(3))
            elif op == 'service':
                services[t[1]] = n(2)
            elif op == 'throwservice':
                service_throw = t[1]
            elif op == 'invoke':
                values = [int(v,16) for v in t[2:]]
                # Scalar API uses signed64 patterns, except masks/value/passive slot.
                unsigned = {'passive_level': (0,1), 'raise_passive_level': (1,),
                    'rebuild_abilities': (0,), 'on_assign_spell': (3,), 'mask_local_buttons': (2,)}
                values = [v if i in unsigned.get(t[1],()) else R.s64(v) for i,v in enumerate(values)]
                result = getattr(machine,t[1])(*values)
                if t[1]=='item_object': result=result.pack()
                if isinstance(result,bytes):
                    results.append('bytes='+bhex(result))
                elif isinstance(result,tuple):
                    results.append('vec='+','.join(format(v & R.M64,'x') for v in result))
                else:
                    results.append('ok' if result is None else format(result & R.M64,'x'))
            elif op == 'register':
                results.append(format(machine.register_handler(n(1)), 'x'))
            elif op == 'level':
                machine.set_pending_level(payload(1),n(2)!=0)
                results.append('ok')
            elif op == 'hud':
                machine.hud_on_msg(payload(1))
                results.append('ok')
            elif op == 'all':
                results.append(','.join(format(v,'x') for v in machine.register_all().values()))
            elif op in ('send','deliver'):
                getattr(machine,op)(n(1),payload(2))
                results.append('ok')
            elif op == 'encode':
                name, _ = KINDS[n(1)]
                args = [int(v,16) for v in t[3:]]
                if name == 'new_world':
                    args = [payload(2)]
                family, data = getattr(R,'msg_'+name)(*args)
                results.append(family+':'+bhex(data))
            else:
                raise AssertionError(op)
        except CallbackError:
            results.append('callback-error')
        except ValueError:
            results.append('error')
    out = 'M'+''.join(';'+format(a,'x')+':'+format(b,'x') for a,b in sorted(mem.bytes.items()))
    out += '/W'+''.join(';'+format(a,'x')+':'+format(s,'x')+':'+format(v,'x') for a,s,v in mem.log)
    out += '/C'
    for name,args in machine.calls:
        if name.startswith('generic_'):
            out += ';'+name+':general'+''.join(':'+general_text(v) for v in args)
        elif name in ('net_getLocalPlayerSlot','cutscene_onMsg','voice_onMsg','dialog_onMsg','text_onMsg'):
            data, arg = (b'',args[0]) if name=='net_getLocalPlayerSlot' else args
            out += ';'+name+':'+bhex(data)+':'+format(arg,'x')
        else:
            def argument(v):
                if isinstance(v,bytes):
                    return 'bytes='+bhex(v)
                if isinstance(v,tuple):
                    return 'vec='+','.join(format(x & R.M64,'x') for x in v)
                return format(v & R.M64,'x')
            out += ';'+name+':args'+''.join(':'+argument(v) for v in args)
    if backing_on: out += '/B'+''.join(';'+format(a,'x')+':'+format(w,'x') for a,w in backing_calls)
    return out+'/R;'+'|'.join(results)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=Path('native/tests/vectors/messages.tsv.gz'))
    args = parser.parse_args()
    for path, sha in PINS.items():
        if digest((args.research_dir/path).read_bytes()) != sha:
            parser.error('working source SHA mismatch: '+path)
        pinned = subprocess.check_output(['git','-C',str(args.research_dir),'show',COMMIT+':'+path])
        if digest(pinned) != sha:
            parser.error('pinned commit SHA mismatch: '+path)
    sys.dont_write_bytecode = True
    sys.path.insert(0,str((args.research_dir/'tools').resolve()))
    spec = importlib.util.spec_from_file_location('pinned_messages',args.research_dir/'tools/ref_messages.py')
    R = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(R)
    rows = []
    counts = {}
    def add(group,commands):
        script = ';'.join(commands)
        rows.append(group+'\t'+script+'\t'+expected(R,script))
        counts[group] = counts.get(group,0)+1
    rng = random.Random(0x20973)
    edges = [0,1,0x7f,0x80,0xff,0x100,0x7fff,0x8000,0xffff,0x10000,
             0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffff,0xffffffffffffffff]
    for kind,(name,count) in enumerate(KINDS):
        for seed in edges+[rng.getrandbits(64) for _ in range(64)]:
            values = [seed]*count
            add('encode-'+name,[action('encode',kind,'-',*values)])
            if count > 1:
                values = [rng.getrandbits(64) for _ in range(count)]
                add('encode-'+name,[action('encode',kind,'-',*values)])
    for name in [b'',b'authored-level',b'a\0b',bytes(range(256)),b'x'*300]:
        add('encode-new-world',[action('encode',17,bhex(name))])
    # Duplicate, null, holes, full-table overflow, and repeated startup registration.
    add('registration',['all','all',action('register',0)])
    for occupied in range(17):
        base = [action('poke',R.G_MSG_HANDLERS+4*i,4,0x100000+i) for i in range(occupied)]
        add('registration',base+['all','all'])
        for handler in [0,0x100000,0x198f64,0xffffffff]:
            add('registration',base+[action('register',handler),action('register',handler)])
    for _ in range(64):
        add('registration',[action('poke',R.G_MSG_HANDLERS+4*i,4,rng.choice([0,0x198f64,0x31f354,0x123456])) for i in range(16)]+['all'])
    # No family execution except null and the four documented external callbacks.
    for channel in [0,1,2,4,6,15,16,0xffffffff,0x40000000]:
        for data in [b'',b'\x00',b'\xff\x00\x80',bytes(range(24))]:
            for op in ['deliver','send']:
                add('delivery',['all',action(op,channel,bhex(data))])
    for channel in [0,1,2,6]:
        add('callback-failure',['all','throwexternal',action('deliver',channel,'010203')])
        add('callback-mutation',['all',action('mutate',0x1234,4,0xff123456),action('send',channel,'aaff')])
    # Network mode uses any nonzero signed byte; raw slot patterns and wrap positions.
    for mode in [1,0x7f,0x80,0xff]:
        for slot in [0,1,3,0xffffffff,0x80000000]:
            peer = (R.G_PEERS+R.s32(slot)*R.PEER_SIZE)&R.M32
            for pos in [0,0x1ffe,0x1fff,0x2000,0xffffffff]:
                for size in [0,1,13,16,255,256,300]:
                    data = bytes(rng.getrandbits(8) for _ in range(size))
                    add('ring',[action('poke',R.G_NETWORK_MODE,1,mode),action('local',slot),
                        action('poke',peer+0x44ac,4,pos),action('send',0x123456ff,bhex(data))])
    # Ordered sequences write over earlier ring bytes, including a complete ring lap.
    for size in [8191,8192,8193]:
        add('ring-lap',[action('poke',R.G_NETWORK_MODE,1,1),action('send',5,bhex(bytes((i&255 for i in range(size))))),action('send',7,'aabbcc')])
    for _ in range(32):
        commands = ['all',action('poke',R.G_NETWORK_MODE,1,1),action('local',rng.randrange(4))]
        for _ in range(20):
            commands.append(action('send',rng.getrandbits(32),bhex(bytes(rng.getrandbits(8) for _ in range(rng.randrange(33))))))
        add('ring-sequence',commands)
    add('local-callback-failure',[action('poke',R.G_NETWORK_MODE,1,1),'throwlocal',action('send',5,'aabb')])
    # Player abilities / HUD controls. Every table/string is authored here.
    def poke(a, size, value):
        return action('poke', a & R.M32, size, value)
    def invoke(name, *args):
        return action('invoke', name, *args)
    def string(a, data):
        return [poke(a+i,1,b) for i,b in enumerate(data+b'\0')]
    def setup(slot=0):
        r = (R.G_PLAYERS+R.s32(slot)*R.REC_SIZE)&R.M32
        c = string(R.S_NEWSTYLE,b'newskill')+string(R.S_NEWSTYLE_HUD,b'newskill')+string(R.S_TABLE_END,b'END')
        c += string(R.G_OLD_ABILITIES+R.ABILITY_SIZE,b'END')
        c += string(R.G_OLD_ABILITIES,b'authored')
        c += [poke(r+R.R_ABILITIES+i*R.ABILITY_SIZE+0x20,2,-1) for i in range(15)]
        c += [poke(r+R.R_SKILL_RANKS+i*4,2,-1) for i in range(8)]
        return r,c
    for slot in [0,1,4,-1,0x80000000,0xffffffff]:
        r,c = setup(slot)
        for ident in [0,1,0x7fff,-2,0x10001,0x80000000]:
            for rank in [0,1,-1,0x7fff,0x8000,0x10001]:
                add('skill-ranks',c+[invoke('set_skill_rank',slot,ident,rank),invoke('skill_rank',slot,ident),
                    invoke('set_skill_rank',slot,ident,rank+1),invoke('skill_rank',slot,ident),invoke('skill_rank',slot,123)])
        for button in [0,1,-1,0x100000000]:
            for index in [0,1,-1,127,128,255,256]:
                for local in [0,1]:
                    add('quick-slot',c+[poke(r+R.R_QUICK_A,1,17),poke(r+R.R_QUICK_B,1,index),
                        action('service','net_isLocalSlot',local),action('service','res_find',0xabc),
                        invoke('on_quick_slot',slot,button,index)])
        for state in [0,1,2,-1,127,128]:
            for player in [0,0x10000,0xfffffff8]:
                for old in [0,59,60,61,0x80000000,0xffffffff]:
                    add('menu-state',c+[poke(r+R.R_OBJECT,4,player),poke(player+12,4,0xaaffffff),
                        poke(R.G_INPUT_LOCK+R.s32(slot)*12,4,old),invoke('on_menu_state',slot,state)])
        for frames in [-1,0,60,0x80000000,0x100000000]:
            add('button-lock',c+[poke(R.G_INPUT_LOCK+R.s32(slot)*12,4,0x80000000),
                invoke('mask_local_buttons',slot,frames,0xfedcba98)])
    for slot in [0,4,5,-1,0x100000000]:
        r,c=setup(slot)
        for cls in [0,1,-1]:
            table=(R.G_CLASS_SKILLS+cls*0xa88)&R.M32
            for entry in [0,23,47]:
                for level in [0,1,127,128,255]:
                    base=c+[poke(r+0x50,4,cls),poke(table+entry*0x38,8,0xabc000),poke(r+R.R_PASSIVES+entry,1,level)]
                    add('passives',base+[invoke('passive_level',slot,0xabc000),
                        invoke('raise_passive_level',slot,0xabc000,level+1),invoke('passive_level',slot,0xabc000),
                        invoke('raise_passive_level',slot,0xabc000,-2),invoke('passive_level',slot,0xddd)])
    float_patterns=[0,0x80000000,1,0x3f800000,0x40000000,0x41200000,0x7f7fffff,0x7f800000,0xffffffff,0xc1200000]
    for mask in [0,0x40,0x80,0x100,0x20000,0xffffffffffffffff]:
        for ident in [5,0x1d,-1]:
            for index in [-1,0,7,14,-2]:
                r,c=setup()
                base=c+[poke(R.G_OLD_ABILITIES+0x48,8,mask),poke(R.G_OLD_ABILITIES+0x20,2,ident)]
                # Fill every copied field with distinct authored patterns, leaving gaps intact.
                for offset,width in [(0x22,2),(0x24,1),(0x25,1),(0x26,2),(0x28,2),(0x2c,4),
                    *[(0x30+2*k,2) for k in range(8)],(0x40,4),*[(0x50+4*k,4) for k in range(5)]]:
                    base.append(poke(R.G_OLD_ABILITIES+offset,width,0x12340000+offset))
                for hp in float_patterns:
                    add('rebuild-abilities',base+[poke(r+R.R_HP,4,hp),poke(r+R.R_HP_MAX,4,0x40000000),
                        poke(r+R.R_MANA,4,hp),poke(r+R.R_MANA_MAX,4,0x40000000),poke(r+R.R_LOAD_LIMIT,4,hp),
                        invoke('rebuild_abilities',mask,0,index)])
                add('rebuild-owned',base+[poke(r+R.R_ABILITY_MASK,8,mask),invoke('rebuild_abilities',mask,0,-1)])
                add('rebuild-missing',base+[invoke('rebuild_abilities',0x123456,0,index)])
    r,c=setup()
    for free in [-1,0,7,14]:
        for ident in [1,-2,0x10001]:
            for kind in [0,1,3,0x10003]:
                base=c+[poke(r+R.R_ABILITIES+i*R.ABILITY_SIZE+0x20,2,9) for i in range(15)]
                if free!=-1: base += [poke(r+R.R_ABILITIES+free*R.ABILITY_SIZE+0x20,2,-1)]
                base += [poke(R.G_SKILL_TABLE+ident*4,4,0x10000),poke(0x10008,4,kind)]
                add('add-skill',base+[invoke('add_skill_ability',ident,0),invoke('add_skill_ability',ident,0)])
    for target in [0,3,7,14,-1]:
        for kind in [0,1,-1]:
            for value in [1,0x20000,0xffffffffffffffff]:
                for held in [-1,2]:
                    for name in [b'newskill',b'prefix-newskill-suffix',b'other']:
                        r,c=setup()
                        dest=(r+R.R_ABILITIES+target*R.ABILITY_SIZE)&R.M32
                        base=c+string(dest,name)+[poke(dest+0x20,2,held),
                            poke(R.G_OLD_ABILITIES+0x48,8,0x20000),poke(R.G_OLD_ABILITIES+0x20,2,8)]
                        for i in [0,2,7]:
                            a=r+R.R_ABILITIES+i*R.ABILITY_SIZE
                            base += [poke(a+0x20,2,value),poke(a+0x48,8,value)]
                        base += [poke(R.G_SKILL_TABLE+R.s32(value)*4,4,0x10000),poke(R.G_SKILL_TABLE+2*4,4,0x10100),
                            poke(0x10008,1,2),poke(0x10108,1,4),invoke('set_skill_rank',0,2,5),
                            action('service','net_isLocalSlot',1),action('service','res_find',0x1234)]
                        add('assign-spell',base+[invoke('on_assign_spell',0,target,kind,value)])
    for slot in [0,-1,0x80000000]:
        r,c=setup(slot)
        for index in [0,7,-1,0x7fffffff]:
            for player in [0,0x10000,0xfffffff8]:
                for queued in [0,1]:
                    for kind in [0,1,2,255]:
                        for state in [0,2,0xffffffff]:
                            a=(r+R.R_ABILITIES+R.s32(index)*R.ABILITY_SIZE)&R.M32
                            add('quick-cast',c+[poke(r+R.R_OBJECT,4,player),poke(player+0x488,4,queued),
                                poke(a+0x25,1,kind),poke(player+0x58c,4,state),action('service','text_lookup',0x1234),
                                invoke('on_quick_cast',slot,index)])
    for name in ['net_isLocalSlot','res_find','sound_playOnObject','text_lookup','hud_showMessage']:
        r,c=setup()
        base=c+[action('service','net_isLocalSlot',1),action('service','res_find',0xabc),
            action('service','text_lookup',0xdef),poke(r+R.R_ABILITIES+0x25,1,1)]
        op=invoke('on_quick_cast',0,0) if name in ['text_lookup','hud_showMessage'] else invoke('on_quick_slot',0,0,1)
        add('ability-callback-failure',base+[action('throwservice',name),op])
        add('ability-callback-mutation',base+[action('mutate',r+R.R_QUICK_B,1,1),op])
    # Allocation/search boundaries and aliasing of raw32 addresses.
    for free in [-1,0,1,14]:
        for owned in [0,1]:
            r,c=setup()
            base=c+[poke(r+R.R_ABILITIES+i*R.ABILITY_SIZE+0x20,2,2) for i in range(15)]
            if free!=-1: base += [poke(r+R.R_ABILITIES+free*R.ABILITY_SIZE+0x20,2,-1)]
            base += [poke(R.G_OLD_ABILITIES+0x48,8,0x20000),poke(r+R.R_ABILITY_MASK,8,0x20000 if owned else 0)]
            add('ability-allocation',base+[invoke('rebuild_abilities',0x20000,0,-1)])
    r,c=setup()
    # Solve 104*index = address-(R+abilities) modulo 2**32.
    def index_for(address):
        return (((address-r-R.R_ABILITIES)&R.M32)//8*pow(13,-1,1<<29))%(1<<29)
    add('ability-null-record',c+[invoke('on_quick_cast',0,index_for(0))])
    add('ability-row-alias',c+[poke(R.G_OLD_ABILITIES+0x48,8,0x20000),
        invoke('rebuild_abilities',0x20000,0,index_for(R.G_OLD_ABILITIES))])
    for trial in range(128):
        r,c=setup()
        base=c
        for i in range(3):
            row=R.G_OLD_ABILITIES+i*R.ABILITY_SIZE
            base += string(row,('authored-'+str(i)).encode())
            base += [poke(row+0x48,8,0x111 if i<2 else 0x222),poke(row+0x20,2,rng.randrange(32))]
            for offset,width in [(0x22,2),(0x24,1),(0x25,1),(0x26,2),(0x28,2),(0x2c,4),
                *[(0x30+2*k,2) for k in range(8)],(0x40,4),*[(0x50+4*k,4) for k in range(5)]]:
                base.append(poke(row+offset,width,rng.getrandbits(32)))
        base += string(R.G_OLD_ABILITIES+3*R.ABILITY_SIZE,b'END')
        add('ability-table-search',base+[invoke('rebuild_abilities',[0x111,0x222,0x333][trial%3],0,trial%15)])
    # No duplicate found, target outside the first eight, and exact marker duplicate refusal.
    for index in [0,7,14]:
        r,c=setup()
        add('assign-no-match',c+[poke(R.G_SKILL_TABLE+12,4,0x10000),poke(0x10008,4,2),
            invoke('set_skill_rank',0,3,0x7fff),invoke('on_assign_spell',0,index,1,3)])
    for entry in [0,14]:
        r,c=setup()
        a=r+R.R_ABILITIES+entry*R.ABILITY_SIZE
        add('add-skill-duplicate',c+string(a,b'newskill')+[poke(a+0x20,2,7),
            poke(R.G_SKILL_TABLE+28,4,0x10000),invoke('add_skill_ability',7,0)])
    for level in [-1,-2,0x80000000,0xffffffff,0x100000001]:
        r,c=setup()
        add('passive-signed-raise',c+[poke(R.G_CLASS_SKILLS,8,0x123),poke(r+R.R_PASSIVES,1,0xff),
            invoke('raise_passive_level',0,0x123,level),invoke('passive_level',0,0x123)])
    # Complete HUD lifecycle: authored strings, definitions, sparse objects and grids.
    def lifecycle(slot=0):
        r,c=setup(slot)
        c += string(R.S_HUB,b'authored-hub')+string(R.S_NONE,b'authored-none')
        c += string(R.G_LEVEL_NAME,b'authored-field')+string(r+0x24,b'authored-return')
        c += string(R.S_RECALL_NAME_2,b'authored-potion')
        c += [poke(r+R.R_HP,4,0x3f800000),poke(r+R.R_OBJECT,4,0x10000),
            poke(0x10010,4,0x3f123456),poke(0x10014,4,0x80000000),poke(0x10018,4,0x7fc01234),
            poke(0x10038,2,-15),poke(R.G_ALIVE_COUNT,4,1),poke(R.G_ALIVE_LIST,4,slot),
            poke(R.G_NUM_PLAYERS,4,3),action('service','text_lookup',0x20000),
            action('service','res_find',0x9876)]
        return r,c
    for slot in [0,1,4,-1,0x80000000,0xffffffff,0x100000000]:
        for panel in [0,1,-1,0x80000000]:
            for bag in [0,1,2,-1,0x80000000]:
                add('inventory-grid',[poke(R.G_ITEM_LIST_CELLS,4,0xfffffff0),invoke('grid',panel,bag)])
        r,c=lifecycle(slot)
        for status in [0,1,0x40,0x41,0x80,0xffffffffffffffff]:
            add('stream-stop',c+[action('service','audio_streamStatus',status),invoke('stop_stream')])
        for hp in [0,0x80000000,1,0x3f800000,0xbf800000,0x7f800000,0x7fc01234,0xff800000]:
            for zone in [0,1]:
                for hub in [False,True]:
                    for target in [b'authored-none',b'authored-return']:
                        add('recall-errors',c+string(R.G_LEVEL_NAME,b'x-authored-hub-y' if hub else b'field')+
                            string(r+0x24,target)+[poke(r+R.R_HP,4,hp),action('service','game_inNoSaveZone',zone),invoke('recall',slot)])
        for count in [-1,0,1,3]:
            for charges in [0,1,2,0xffff]:
                for match in [False,True]:
                    for obj in [0,0x10000]:
                        base=c+[poke(r+R.R_OBJECT,4,obj),poke(r+R.R_BAG_COUNT+2*R.BAG_SIZE,4,count)]
                        for i in range(max(count,0)):
                            it=r+R.R_BAGS+2*R.BAG_SIZE+i*R.ITEM_SIZE
                            base += [poke(it,4,0x30000+i*0x100),poke(0x30000+i*0x100,4,0x40000+i*0x100),
                                poke(it+4,2,charges),poke(it+0x22,2,i)]
                            base += string(0x40000+i*0x100,b'authored-potion' if match and i==count-1 else b'other')
                        add('recall-consumption',base+[invoke('recall',slot)])
        for count in [-1,0,1,3]:
            for block in [0,0x50000,0xffffffffffffffff]:
                for status in [0,0x40]:
                    base=c+string(R.G_LEVEL_NAME,b'authored-hub')+[poke(R.G_ALIVE_COUNT,4,count),
                        action('service','pool_new',block),action('service','audio_streamStatus',status)]
                    for i in range(max(count,0)):
                        base += [poke(R.G_ALIVE_LIST+4*i,4,i-1),poke(R.G_PLAYERS+(i-1)*R.REC_SIZE+R.R_OBJECT,4,0xfffffff8+i*0x100),
                            poke(0x8+i*0x100,4,0x12345678+i)]
                    add('recall-effects',base+[invoke('recall',slot)])
        for obj in [0,0x10000,0xfffffff8]:
            for offset in [0,1,-1,0x7fff,0x8000,0xffff]:
                for translated in [b'',b'\x34\x12\xff\x80',b'\x41\0\0\0\x42\0']:
                    base=c+string(r,b'authored-\x80-name')+string(R.S_LEFT_SPACE,b' ')
                    base += [poke(r+R.R_OBJECT,4,obj),poke(obj+0x48,4,0x30000),poke(0x30010,2,offset),poke(obj+0xc,4,0xffffffff)]
                    base += [poke(0x20000+i,1,b) for i,b in enumerate(translated+b'\0\0')]
                    add('player-left',base+[invoke('on_player_left',slot)])
        for local in [0,1]:
            for active in [0,1]:
                for alive in [-1,0,1,2]:
                    for players in [-1,0,1,2,4]:
                        add('disconnect-request',c+[action('service','net_isLocalSlot',local),
                            action('service','net_isSlotActive',active),poke(R.G_ALIVE_COUNT,4,alive),
                            poke(R.G_NUM_PLAYERS,4,players),invoke('on_disconnect_request',slot)])
    for name in [b'',b'a',b'a\0ignored',b'x'*79,b'y'*81,b'z'*300,bytes(range(1,256))]:
        for clear in [0,1]:
            for status in [0,0x40]:
                add('pending-level',[poke(R.G_LOAD_FLAG,1,9),action('service','audio_streamStatus',status),
                    action('level',bhex(name),clear)])
    for slot in [0,1,3,-1,0x80000000,0x100000000]:
        for players in [-1,0,1,2,4,127,128]:
            for old in [0,1,3,126,127,128,254,255]:
                for mismatch in [0,1,2,3]:
                    c=[poke(R.G_NUM_PLAYERS,4,players),poke(R.G_CHECKSUM_COUNT,1,old),action('local',1)]
                    # Received pairs for other players, without putting 127 long scans in every case.
                    for i in range(max(players,0)):
                        c += [poke(R.G_CHECKSUM_RECEIVED+i*8,4,0x123),poke(R.G_CHECKSUM_RECEIVED+i*8+4,4,0x456),
                            poke(R.G_CHECKSUM_EXPECTED+i*8,4,0x123^(1 if mismatch&1 else 0)),
                            poke(R.G_CHECKSUM_EXPECTED+i*8+4,4,0x456^(1 if mismatch&2 else 0))]
                    add('checksums',c+[invoke('on_checksum',slot,0x100000123,0x100000456)])
    # Sequential checksum arrival and duplicate-pair failure, with mutation on each local-slot callback.
    for local in [0,1,0xffffffff]:
        add('checksum-sequence',[poke(R.G_NUM_PLAYERS,4,2),action('local',local),
            invoke('on_checksum',0,1,2),invoke('on_checksum',1,3,4),invoke('on_checksum',1,3,4)])
        # Mutations happen at the local-slot callback, after the mismatch store.
        for addr in [R.G_NUM_PLAYERS,R.G_CHECKSUM_RECEIVED+4,R.G_CHECKSUM_EXPECTED+4]:
            add('checksum-mutation',[poke(R.G_NUM_PLAYERS,4,2),poke(R.G_CHECKSUM_COUNT,1,1),action('local',local),
                action('mutate',addr,4,0),'mutatelocal',invoke('on_checksum',0,1,2)])
    # Bag compaction: paired equipment slots, signed definition slots, pointer aliases and UI stamps.
    for bag in [0,1,2,-1]:
        for index in [-1,0,1,2,3]:
            for count in [-1,0,1,3]:
                for es in [-1,0,5,6,14,15,16,127,-128]:
                    r,c=setup(); base=(r+R.R_BAGS+bag*R.BAG_SIZE)&R.M32
                    it=(base+index*R.ITEM_SIZE)&R.M32
                    c += [poke(r+R.R_BAG_COUNT+bag*R.BAG_SIZE,4,count),poke(it,4,0x30000),poke(0x3001c,1,es)]
                    for i in range(4):
                        if i!=index: c += [poke(base+i*R.ITEM_SIZE,4,0x30100+i*0x100)]
                        c += [poke(base+i*R.ITEM_SIZE+8,8,0x1234567800000000+i),poke(base+i*R.ITEM_SIZE+0x20,4,i<<16),
                            poke(base+i*R.ITEM_SIZE+0x22,2,i)]
                    for k in range(17): c += [poke(r+R.R_EQUIP+4*k,4,it if k%2==0 else (it+R.ITEM_SIZE)&R.M32)]
                    c += [poke(r+R.R_SET_WEAPON,4,it),poke(r+R.R_SET_WEAPON+4,4,(it+R.ITEM_SIZE)&R.M32),
                        poke(r+R.R_SECOND_WEAPON,4,(it+R.ITEM_SIZE)&R.M32)]
                    add('inventory-compaction',c+[invoke('remove_item',0,index,bag,0)])
    for active in [-1,0,1,127,-128]:
        for local in [0,1]:
            for menu in [0,1,0x80]:
                for duplicate in [False,True]:
                    r,c=setup(); base=r+R.R_BAGS
                    c += [poke(r+R.R_BAG_COUNT,4,3),poke(r+R.R_ACTIVE_SET,1,active),
                        poke(r+R.R_SET_WEAPON+active*4,4,base),action('service','net_isLocalSlot',local),
                        poke(R.G_LOCAL_MENU_OPEN,1,menu),poke(R.G_ITEM_LIST_CELLS,4,0x60000)]
                    for i in range(3):
                        c += [poke(base+i*R.ITEM_SIZE,4,0x30000),poke(base+i*R.ITEM_SIZE+8,8,0x42),
                            poke(base+i*R.ITEM_SIZE+0x20,4,0 if duplicate else i),poke(0x3001c,1,5)]
                    for row,col in [(0,0),(74,8)]:
                        cell=0x60000+col*0x1518+row*R.ITEM_SIZE
                        c += [poke(cell,4,0x30000),poke(cell+8,8,0x42),poke(cell+0x20,4,0 if duplicate else 1)]
                    add('inventory-grid-refresh',c+[invoke('remove_item',0,0,0,0)])
    for a,b in [(0,0),(0xfffffff8,0x10000),(0x10000,0x10000)]:
        for definition in [0,0x20000]:
            for flags in [0,0xffffffffffffffff]:
                for word in [0,0xdeadbeef]:
                    add('item-identity',[poke(a,4,0x20000),poke(a+8,8,0),poke(a+0x20,4,0),
                        poke(b,4,definition),poke(b+8,8,flags),poke(b+0x20,4,word),invoke('same_identity',a,b)])
    # Every dispatch type, signed unknown types, zero-padding, stale tails, direct and offline paths.
    for kind in list(range(14))+[14,127,128,255]:
        for slot in [0,1,127,128,255]:
            for size in [0,1,2,4,8,12,16,32]:
                r,c=lifecycle(R.s8(slot))
                p=bytearray(size)
                if size: p[0]=kind
                if size>1: p[1]=slot
                if size>2: p[2]=1
                if size>8: p[8]=1
                if kind==1: # kind=1 skill assignment avoids undocumented table requirements
                    if size>4: p[4]=1
                if kind in [7,11] and size>=16:
                    start=1 if kind==7 else 4
                    p[start:start+4]=b'new\0'
                for route in ['hud','deliver','send']:
                    cmd=action('hud',bhex(p)) if route=='hud' else action(route,3,bhex(p))
                    add('hud-dispatch-'+route,c+['all',cmd])
    # Dialog/task short-circuit, countdown reread, exceptions at every reachable service prefix.
    r,c=lifecycle()
    for dialog in [0,1]:
        for task in [0,1]:
            for countdown in [0,1,0x80]:
                add('hud-recall-gates',c+[action('service','dialog_isOpen',dialog),action('service','task_isRunning',task),
                    poke(R.G_RECALL_COUNTDOWN,1,countdown),action('hud','0300')])
    paths={
        'recall': (c+string(R.G_LEVEL_NAME,b'authored-hub')+[action('service','pool_new',0x50000),
            action('service','audio_streamStatus',0x40)],invoke('recall',0),
            ['pauseMenu_close','game_inNoSaveZone','pool_new','RecallEffect_ctor','res_find','sound_playOnObject','audio_streamStatus','stream_queueStop']),
        'recall-error': (c+[action('service','game_inNoSaveZone',1)],invoke('recall',0),['text_lookup','hud_showMessage']),
        'left': (c,invoke('on_player_left',0),['text_lookup','hud_showMessage','Player_takeDamage_virtual','Player_processDamage']),
        'disconnect': (c+[action('service','net_isSlotActive',1)],invoke('on_disconnect_request',0),
            ['net_isLocalSlot','net_isSlotActive','player_respawn','net_disconnectPlayer']),
        'remove': (c+[poke(r+R.R_SET_WEAPON,4,r+R.R_BAGS),poke(r+R.R_BAG_COUNT,4,1)],invoke('remove_item',0,0,0,0),
            ['player_resetIdleAnim','net_isLocalSlot','player_recalcStats']),
        'level': (c+[action('service','audio_streamStatus',0x40)],action('level','616263',1),
            ['pauseMenu_close','audio_streamStatus','stream_queueStop']),
        'hud': (c,action('hud','0300'),['dialog_isOpen','task_isRunning']),
        'save': (c,action('hud','05000000efbeadde'),['Savepoint_startSavedEffect'])}
    for group,(base,op,names) in paths.items():
        for name in names:
            add('lifecycle-callback-failure-'+group,base+[action('throwservice',name),op])
        for address,width,value in [(r+R.R_OBJECT,4,0),(R.G_ALIVE_COUNT,4,0),(R.G_NUM_PLAYERS,4,0),
                (R.G_RECALL_COUNTDOWN,1,0),(R.G_STREAM_FLAG,1,9)]:
            add('lifecycle-callback-mutation-'+group,base+[action('mutate',address,width,value),op])
    add('checksum-local-failure',[poke(R.G_NUM_PLAYERS,4,1),'throwlocal',invoke('on_checksum',0,1,2)])
    add('checksum-disconnect-failure',[poke(R.G_NUM_PLAYERS,4,1),action('local',1),
        action('throwservice','net_disconnectPlayer'),invoke('on_checksum',0,1,2)])
    for address in [0,0x10000,0xfffffff8,0xffffffff]:
        base=[poke(address,8,0x0807060504030201),poke(address+8,8,0x100f0e0d0c0b0a09)]
        for size in [-1,0,1,7,8,9,16,17]:
            for delta in [-8,-1,0,1,8]:
                add('memory-copy',base+[invoke('copy',(address+delta)&R.M32,address,size)])
        for limit in [-1,0,1,2,3,7,8,15,16,4096]:
            add('memory-strings',base+[invoke('cstr',address,limit),invoke('wstr',address,limit)])
        add('memory-vector',base+[invoke('vec',address)])
    # Explicit maximum-length and odd wide-character limit behavior.
    for limit in [4095,4096,4097]:
        base=[poke(0x10000+i,1,0x41) for i in range(4100)]
        add('memory-string-limit',base+[invoke('cstr',0x10000,limit),invoke('wstr',0x10000,limit)])
    # Compaction temporaries cross raw32: pointer comparisons use the unwrapped src.
    for base in [0xffffffa0,0xfffffff8,0]:
        r,c=setup()
        bag=((((base-r-R.R_BAGS)&R.M32)//8)*pow(R.BAG_SIZE//8,-1,1<<29))%(1<<29)
        c += [poke(r+R.R_BAG_COUNT+bag*R.BAG_SIZE,4,3)]
        for i in range(3):
            a=(base+i*R.ITEM_SIZE)&R.M32
            c += [poke(a,4,0x30000),poke(0x3001c,1,5),poke(a+0x22,2,i),
                poke(r+R.R_EQUIP+i*4,4,a),poke(r+R.R_SET_WEAPON+i*4,4,a)]
        add('inventory-compaction-wrap',c+[invoke('remove_item',0,0,bag,0)])
    # Inventory ownership and player dispatch. Only authored tables and records.
    def inventory(slot=0, category=0, es=0, subtype=0, flags=0, charges=2):
        r,c=setup(slot)
        c += ['all',action('definition',0x30000,category,es,subtype,17,0,0,bhex(b'authored-item'))]
        c += string(0x40000,b'authored-item')+string(R.S_STACK_STEM_DROP,b'authored-thrown')
        c += string(R.S_RECALL_NAME,b'authored-recall')+string(R.S_UNIQUE_A,b'authored-unique-a')+string(R.S_UNIQUE_B,b'authored-unique-b')
        c += [poke(0x30000,4,0x40000),poke(0x3001c,1,es),poke(0x3001d,1,category),poke(0x3001e,1,subtype),
            poke(r+R.R_OBJECT,4,0x10000),poke(r+R.R_ACK_LEVEL,1,30),poke(r+0x90,4,100),poke(r+0x94,4,100),
            poke(r+0x98,4,100),poke(r+0x9c,4,100),poke(R.G_NUM_PLAYERS,4,1),poke(R.G_DIFFICULTY,4,1)]
        item=(r+R.R_BAGS+category*R.BAG_SIZE)&R.M32
        c += record(item,0x30000,flags,charges)+[poke(r+R.R_BAG_COUNT+category*R.BAG_SIZE,4,1)]
        return r,item,c
    def record(a,definition=0x30000,flags=0,charges=2):
        return [poke(a,4,definition),poke(a+4,2,charges),poke(a+8,8,flags),poke(a+0x20,4,0xffff)]+[poke(a+0x24+8*i,2,-1) for i in range(4)]
    keys=('wood_large','wood_small','wood','chain','shield','plate','metal_large','metal_small','metal','leather','cloth','potion','other')
    for flags in [0,0x800000,0x200000,0x400000,0x40000,0x100000,0x80000,0x1000000,0xffffffffffffffff]:
        for sizebit in [0,0x20000000]:
            for es in [0,3,-1,16,0x100000003]:
                for subtype in [0,1,2,3,4,5,6,255,256,0xffffffffffffffff]:
                    for short in [0,1]:
                        add('inventory-sound',[action('sound',flags|sizebit,es,subtype,short,0)])
    for slot in [0,1,4,-1,0x80000000,0x100000000]:
        for stat in [-0x80000000,-1,0,3,4,7,8,9,0x100000000]:
            for points in [0,1,2,255]:
                for local in [0,1]:
                    r,item,c=inventory(slot)
                    add('inventory-stat',c+[poke(r+R.R_STAT_POINTS,1,points),action('service','net_isLocalSlot',local),invoke('on_stat_point',slot,stat)])
        for is_skill in [0,1,-1]:
            for value in [1,3,-1,0x80000000,0x100000003]:
                for points in [-1,0,1,2]:
                    for rank in [-1,0,19,20,21]:
                        r,item,c=inventory(slot)
                        c += [poke(r+R.R_SKILL_POINTS,4,points),poke(R.G_SKILL_TABLE+R.s32(value)*4,4,0x20000),
                            poke(0x20008,4,3),poke(R.G_CLASS_SKILLS,8,R.s32(value)&R.M64),
                            poke(r+R.R_PASSIVES,1,rank),invoke('set_skill_rank',slot,R.s32(value),rank)]
                        add('inventory-skill-point',c+[invoke('on_skill_point',slot,is_skill,value)])
        for panel in [0,1,-1,0x80000000]:
            for level in [0,1,-1]:
                for net in [0,1,128]:
                    r,item,c=inventory(slot)
                    p=(R.G_ITEM_PANELS+R.s32(panel)*R.PANEL_SIZE)&R.M32
                    c += [poke(p+0x70,4,0xffffffff),poke(p+0x34,4,slot),poke(R.G_PANEL_ROWS,4,3),
                        poke(r+R.R_LEVEL,1,0xff),poke(R.G_NETWORK_MODE,1,net),action('local',slot)]
                    add('inventory-join-panel',c+[invoke('on_join_panel',panel,slot,level)])
    for slot in [-1,0,1,0x100000000]:
        for players in [-1,0,1,3]:
            for frame in [-1,0,1,17,0x7fffffff]:
                for local in [0,1,2,0xffffffff]:
                    c=[poke(R.G_NUM_PLAYERS,4,players),poke(R.G_SIM_FRAME,4,frame),poke(R.G_RAND_SEED,4,0xffffffff),
                        action('local',local),action('service','net_allPlayersState2',1),action('service','net_isSlotActive',1)]
                    add('inventory-lobby',c+[invoke('send_character_to_lobby',slot)])
        for service,word,value in [('net_allPlayersState2',R.G_PVP_MODE_B,0),('net_allPlayersState2',R.G_PVP_MODE_B,1),('net_allPlayersState2',R.G_MEDAL_MISSION,1)]:
            add('inventory-lobby-gates',[poke(word,4,value),action('service',service,0 if not value else 1),invoke('send_character_to_lobby',slot)])
    for net in [0,1,128]:
        for count in [-1,0,1,3,64]:
            for flags in [0,0x100]:
                for announce in [0,1,-1,0x100000000]:
                    for name in [b'authored-item',b'authored-recall',b'authored-unique-a',b'authored-unique-b']:
                        r,it,c=inventory(0,2,5,0,flags)
                        c += record(0x20000,flags=flags,charges=0xffff)+string(0x40000,name)
                        c += [poke(R.G_NETWORK_MODE,1,net),poke(r+R.R_BAG_COUNT+2*R.BAG_SIZE,4,count)]
                        add('inventory-give',c+[invoke('give_item',0,0x20000,announce)])
    for flags in [0,0x100]:
        for match in [False,True]:
            for index in [0,1,63]:
                r,it,c=inventory(0,2,5,0,flags)
                c += record(0x20000,flags=flags,charges=0xffff)+record(it+index*R.ITEM_SIZE,flags=flags)
                c += string(0x40000,b'authored-recall')+[poke(R.G_NUM_PLAYERS,4,3),poke(R.G_DIFFICULTY,4,1),
                    poke(r+R.R_FIRST_RECALL,1,0),poke(R.G_PLAYERS+2*R.REC_SIZE+R.R_FIRST_RECALL,1,int(match))]
                add('inventory-stack-hint',c+[invoke('give_item',0,0x20000,1)])
    # All equipment categories, pointer slots, sound precedence and local/quiet paths.
    for category,slots in [(0,[0,2,8,16,-1]),(1,[3,4,16]),(2,[5,7,14,16,-1]),(-1,[0])]:
        for es in slots:
            for subtype in [0,1,4,6,255]:
                for active in [0,1,-1]:
                    for dual in [0,1]:
                        r,it,c=inventory(0,category,es,subtype,0x800000)
                        c += [action('definition',0x30100,0,0,0,17,0,0,bhex(b'authored-first')),poke(0x3011c,1,0),poke(0x3011d,1,0)]
                        c += record(0x20000,0x30100)+[poke(r+R.R_ACTIVE_SET,1,active),poke(r+R.R_EQUIP,4,0x20000),
                            poke(r+R.R_EQUIP+8,4,0x20000),poke(r+R.R_EQUIP+20,4,0x20000),
                            poke(r+R.R_EQUIP+56,4,0x20000),poke(r+R.R_SECOND_EARRING,1,1),
                            poke(r+R.R_SET_WEAPON,4,0x20000),poke(r+R.R_SET_WEAPON+4,4,0x20000),
                            poke(R.G_CLASS_SKILLS,8,0x40000),poke(r+R.R_PASSIVES,1,dual),action('service','net_isLocalSlot',1)]
                        add('inventory-equip',c+[invoke('equip_item',0,it,0),invoke('unequip_item',0,it)])
                        add('inventory-equip-core',c+[invoke('_equip_item',0,it,1)])
    # The public wrapper masks item to raw32; its inner body compares the original
    # supplied integer when invoked directly. Address reads still wrap in both.
    for raw in [0,0x100000000,0xffffffffffffffff,0x100000130,0x8000000000000130]:
        r,it,c=inventory()
        raw=(raw&~R.M32)|it
        c += [poke(r+R.R_EQUIP,4,it),poke(r+R.R_SET_WEAPON,4,it)]
        add('inventory-equip-inner-raw',c+[invoke('_equip_item',0,raw,1)])
        add('inventory-equip-wrapper-raw',c+[invoke('equip_item',0,raw,1)])
    # Item wrapper field decoding, required-stat precedence, duplicate rank first-match.
    for trial in range(256):
        r,it,c=inventory()
        for i in range(R.ITEM_SIZE):
            if i>=4: c += [poke(it+i,1,rng.randrange(256))]
        add('inventory-item-object',c+[invoke('item_object',it)])
    for cls in [0,1,3,4,6,-1]:
        for code in [0xe,0xf,0x10,0x11,0x12,0x27,0x28,0x13]:
            for need in [-1,0,1,100,101,32767]:
                r,it,c=inventory(0,1,3,4)
                c += [poke(r+0x50,4,cls),poke(it+0x24,2,code),poke(it+0x26,2,need)]
                add('inventory-equip-restriction',c+[invoke('check_equip_allowed',0,it),invoke('equip_item',0,it,1)])
    for category in [0,1,2]:
        for es in [0,3,5,7,14]:
            for price in [0,1,17,0x7fffffff]:
                for charges in [-32768,-1,0,1,32767]:
                    r,it,c=inventory(0,category,es,5)
                    c += [action('definition',0x30000,category,es,5,price,0,0,bhex(b'authored-item')),poke(it+4,2,charges)]
                    # Definition replaces its prior identity in both table routes.
                    add('inventory-item-value',c+[invoke('item_value',it)])
    for category in [0,1,2]:
        for es in [0,3,5,8,14]:
            for charges in [-1,0,1,19,20,21,32767]:
                for mode in [-1,0,1,2]:
                    for local in [0,1]:
                        r,it,c=inventory(0,category,es,6,0x100,charges)
                        c += [action('service','net_isLocalSlot',local),action('service','pool_new',0x50000),
                            action('spot',0x80000000,0x7fc01234,0x12345678),poke(r+R.R_SET_WEAPON,4,it)]
                        add('inventory-drop',c+[invoke('drop_item',0,0,category,0,0,0,mode)])
    # Grid duplicate counts and clearing; nested offline promotion vs queued network send.
    for net in [0,1]:
        for which in [0,1,2]:
            r,it,c=inventory()
            c += record(0x20000)+[poke(R.G_NETWORK_MODE,1,net),poke(r+R.R_SET_WEAPON,4,it),
                poke(r+R.R_SECOND_WEAPON,4,0x20000),poke(r+R.R_WEAPON_CLASS,1,7)]
            add('inventory-unref',c+[invoke('unref_item',0,[it,0x20000,0xabcdef][which])])
            add('inventory-unequip-swap',c+[poke(r+R.R_SECOND_WEAPON,4,0),action('service','Player_weaponSwapInput',which),invoke('unequip_item',0,it)])
    for charges in [1,2,21]:
        r,it,c=inventory(0,2,5,0,0x100,charges)
        c += [action('service','net_isLocalSlot',1),poke(R.G_ITEM_LIST_CELLS,4,0x60000)]
        for row,col in [(0,0),(74,8),(1,0)]: c += record(0x60000+2*0xbdd8+col*0x1518+row*R.ITEM_SIZE,flags=0x100,charges=charges)
        add('inventory-drop-grid',c+[invoke('drop_item',0,0,2,0,0,0,2)])
    # Every inventory branch with short payloads and preserved stale tails on all routes.
    for kind in list(range(11))+[11,127,128,255]:
        for slot in [0,1,127,128,255]:
            for size in [0,1,2,4,8,12,16,24]:
                r,it,c=inventory(R.s8(slot))
                p=bytearray(size)
                if size:p[0]=kind
                if size>1:p[1]=slot
                if size>2:p[2]=slot if kind in [1,2,4] else 0
                if kind in [1,2] and size>=4:p[2:4]=(R.s8(slot)&0xffff).to_bytes(2,'little')
                if kind in [1,2] and size>=8:p[4:8]=it.to_bytes(4,'little')
                if kind==4 and size>1:p[1]=0
                # avoid acquiring skill from unprovided object in the short type-7 path (points=0)
                for route in ['inv','deliver','send']:
                    cmd=action('inv',bhex(p)) if route=='inv' else action(route,5,bhex(p))
                    add('inventory-dispatch-'+route,c+[cmd])
    # Every reachable delegated-service failure and mutation at read-after-call boundaries.
    r,it,c=inventory()
    paths={
        'equip': (c+[action('service','net_isLocalSlot',1)],invoke('equip_item',0,it,0),
            ['script_runByName','player_resetIdleAnim','inventory_selectIdleAnim','net_isLocalSlot','res_find','sound_playOnObject','player_recalcStats']),
        'drop': (c+[action('service','pool_new',0x50000),action('spot',1,2,3),action('service','net_isLocalSlot',1)],invoke('drop_item',0,0,0,0,0,0,0),
            ['net_isLocalSlot','res_find','sound_playOnObject','world_findFreeSpot','pool_new','ItemProp_ctor2','player_recalcLoad','player_recalcStats']),
        'give': (c+record(0x20000),invoke('give_item',0,0x20000,1),['player_countPotions','player_recalcStats']),
        'hint': (c,invoke('first_recall_hint',0,r),['vars_set','text_lookup','hud_showHelpMessage']),
        'join': (c+[poke(R.G_ITEM_PANELS+0x34,4,0),action('service','net_isLocalSlot',1)],invoke('on_join_panel',0,0,1),
            ['inventory_refreshPanelModel','net_isLocalSlot','res_find','sound_playOnObject']),
        'lobby': (c+[action('service','net_allPlayersState2',1),action('service','net_isSlotActive',1)],invoke('send_character_to_lobby',0),
            ['net_allPlayersState2','net_isSlotActive','lobby_sendCharacter'])}
    for group,(base,op,names) in paths.items():
        for name in names: add('inventory-failure-'+group,base+[action('throwservice',name),op])
        for address,width,value in [(r+R.R_ACTIVE_SET,1,1),(r+R.R_BAG_COUNT,4,2),(r+R.R_OBJECT,4,0),(R.G_NUM_PLAYERS,4,0),(it+4,2,0)]:
            add('inventory-mutation-'+group,base+[action('mutate',address,width,value),op])
    # Complete shop family: exact price patterns, signed counts and gold, cap/wrap,
    # inventory refusals, aliasing and callback prefixes. Append-only corpus.
    def stock(slot=0,category=0,es=0,price=100,charges=1,flags=0,df=0):
        r,it,c=inventory(slot,category,es,0,flags,charges)
        c += [action('definition',0x30000,category,es,0,price,0,df,bhex(b'authored-item')),
              poke(0x30028,8,df),poke(r+R.R_GOLD,4,1000)]
        c += string(R.S_STACK_STEM_SELL,b'authored-thrown')
        return r,it,c
    for category,es in [(0,0),(1,3),(2,5),(2,8)]:
        for flags in [0,0x10000000000,0x20000000000,0x30000000000]:
            for price in [-2147483648,-1,0,1,17,100,16777217,2147483647]:
                for gold in [-2147483648,-1,0,44,45,59,60,100,2147483647]:
                    r,it,c=stock(category=category,es=es,price=price,flags=flags)
                    add('shop-buy-price',c+[poke(r+R.R_GOLD,4,gold),invoke('shop_buy',0,it)])
    for category in [0,1,2]:
        for charges in [-32768,-1,0,1,19,20,21,32767]:
            for price in [-2147483648,-1,0,1,17,100,16777217,2147483647]:
                for df,es,stem in [(0,0,False),(0,8,False),(0x400000000000,5,False),
                        (0x40000000000,5,False),(0x80000000000,5,False),(0x100000000000,5,False),
                        (0,5,True),(0x5c0000000000,8,True)]:
                    r,it,c=stock(category=category,es=es,price=price,charges=charges,df=df)
                    if stem:c+=string(0x40000,b'authored-thrown-item')
                    add('shop-sell-price',c+[invoke('shop_sell',0,0,category,0,0,0,1)])
    for gold in [-2147483648,-1,0,99999998,99999999,100000000,2147483647]:
        r,it,c=stock(price=100)
        add('shop-sell-gold',c+[poke(r+R.R_GOLD,4,gold),invoke('shop_sell',0,0,0,0,0,0,1)])
    for count in [-1,0,1,63,64,65]:
        for name in [b'authored-item',b'authored-unique-a',b'authored-unique-b']:
            r,it,c=stock()
            c+=record(0x20000)+string(0x40000,name)+[poke(r+R.R_BAG_COUNT,4,count)]
            add('shop-buy-refusal',c+[invoke('shop_buy',0,0x20000)])
    # Raw32 item locations from signed index, unguarded bag and slot arguments.
    for slot in [-1,0,1,127,-128,0x100000000]:
        for index in [-2147483648,-1,0,1,0x100000000]:
            for bag in [-1,0,1,2,127,-128]:
                r,it,c=stock(slot)
                item=(r+R.R_BAGS+R.s32(index)*R.ITEM_SIZE+bag*R.BAG_SIZE)&R.M32
                c+=record(item)+[poke(r+R.R_BAG_COUNT+bag*R.BAG_SIZE,4,1)]
                add('shop-sell-address',c+[invoke('shop_sell',slot,index,bag,-1,-32768,-1,2)])
        for addr in [0,0xfffffff8,0xfffffffc,0x20000]:
            r,it,c=stock(slot)
            c+=record(addr)
            add('shop-buy-address',c+[invoke('shop_buy',slot,addr)])
    # All dispatch types through direct, offline and registered delivery. Empty
    # and partial buy buffers have an authored definition at the zero address.
    for kind in [0,1,2,127,128,255]:
        for slot in [0,1,127,128,255]:
            for size in [0,1,2,4,5,7,8,10,12,13,16,24]:
                r,it,c=stock(R.s8(slot))
                c += [action('definition',0,0,0,0,100,0,0,bhex(b'authored-null-definition'))]
                p=bytearray(size)
                if size:p[0]=kind
                if size>1:p[1]=slot
                if kind==0 and size>=8:p[4:8]=it.to_bytes(4,'little')
                if kind==1 and size>12:p[12]=1
                # Partially specified stock addresses are valid synthetic items.
                if kind==0 and size:
                    addr=int.from_bytes(p[4:8], 'little') if size>4 else 0
                    if addr!=it:c+=record(addr)
                for route in ['shop','deliver','send']:
                    op=action('shop',bhex(p)) if route=='shop' else action(route,7,bhex(p))
                    add('shop-dispatch-'+route,c+[op])
    r,it,c=stock(price=100)
    paths={
        'buy':(c+record(0x20000),invoke('shop_buy',0,0x20000),['player_countPotions','player_recalcStats']),
        'sell':(c+[action('service','net_isLocalSlot',1)],invoke('shop_sell',0,0,0,0,0,0,1),
            ['net_isLocalSlot','res_find','sound_playOnObject','player_recalcLoad','player_recalcStats']),
        'ground':(c+[action('service','pool_new',0x50000),action('spot',1,2,3)],invoke('shop_sell',0,0,0,0,0,0,0),
            ['world_findFreeSpot','pool_new','ItemProp_ctor2','player_recalcLoad','player_recalcStats'])}
    for group,(base,op,names) in paths.items():
        for name in names:add('shop-failure-'+group,base+[action('throwservice',name),op])
        for a,w,v in [(r+R.R_GOLD,4,1),(r+R.R_BAG_COUNT,4,64),(it+4,2,0),(r+R.R_OBJECT,4,0)]:
            add('shop-mutation-'+group,base+[action('mutate',a,w,v),op])
    # Complete scalar-wrapper family: boundary signs, truncation, sparse bytes,
    # overlapping writes, and addresses whose base alone wraps to raw32.
    edges = [0,1,0x7f,0x80,0xff,0x100,0x7fff,0x8000,0xffff,0x10000,
             0x7fffffff,0x80000000,0xffffffff,0x100000000,
             0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff]
    for value in edges + [rng.getrandbits(64) for _ in range(128)]:
        add('scalar-s64',[action('signed64',value)])
    for width in [1,2,4,8]:
        for address in [0,0x10000,0xfffffffd,0xffffffff,0x100000000,0x100010000,0xffffffffffffffff]:
            readers = {1:['u8','s8'],2:['u16','s16'],4:['u32','s32'],8:['u64']}[width]
            for value in edges + [rng.getrandbits(64) for _ in range(16)]:
                add('scalar-write-read', [invoke('w'+str(width*8),address,value)] +
                    [invoke(name,address) for name in readers])
            add('scalar-unwritten', [invoke(name,address) for name in readers])
            add('scalar-overlap', [invoke('w64',address,0xfedcba9876543210),
                invoke('w'+str(width*8),address+1,0x80000000ffffffff)] +
                [invoke(name,address) for name in readers])
            for byte in range(width):
                add('scalar-sparse', [poke((address+byte)&R.M32,1,0x80)] +
                    [invoke(name,address) for name in readers])
    # General memory boundary: no raw32 masking, any signed integer, any size.
    def g(op,*args): return ' '.join([op]+[format(v,'x') if isinstance(v,int) else v for v in args])
    for address in [-1,-(1<<96),0,0xffffffff,1<<64,(1<<128)-1]:
        for width in [-3,-1,0,1,3,7,9,16,33,65]:
            for value in [0,-1,-(1<<270),1<<270,(1<<300)-17]:
                add('general-memory',[g('gpoke',address,width,value),g('gload',address,width)])
                if width>=0: add('general-store',[g('gstore',address,width,value),g('gload',address,width)])
            for value in [0,0x123456789abcdef,-1,1<<90]:
                for mode in ['plain','mutate','throw']:
                    add('general-backing-whole',[g('backing',value,mode),g('gload',address,width)])
                    add('general-backing-mixed',[g('backing',value,mode),g('gpoke',address,1,0x80),g('gload',address,width)])
    for token in ['i=0','i=-1','i='+format(1<<300,'x'),'b=-','b=00ff80','n=-','t=authored']:
        for mode in ['echo','missing','throw']:
            add('general-call',[g('gcall','generic_callback',mode,'i=-10000000000000000',token)])
    add('general-call-empty',[g('gcall','generic_empty','echo')])
    tree = ast.parse((args.research_dir/'tools/ref_messages.py').read_text())
    units = [n.name for n in tree.body if isinstance(n,ast.FunctionDef)] + [
        c.name+'.'+n.name for c in tree.body if isinstance(c,ast.ClassDef)
        for n in c.body if isinstance(n,ast.FunctionDef) and c.name != 'Ext']
    assert len(units) == 101
    data = ('\n'.join(rows)+'\n').encode()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    compressed = io.BytesIO()
    with gzip.GzipFile(filename="",mode="wb",fileobj=compressed,mtime=0) as stream:
        stream.write(data)
    args.output.write_bytes(compressed.getvalue())
    manifest = {'schema':SCHEMA,'research_commit':COMMIT,'research_files':PINS,
        'generator_sha256':digest(Path(__file__).read_bytes()),'vectors_sha256':digest(data),
        'rows':len(rows),'groups':counts,'reference_units':len(units),'ported_units':101,'scope':'transport, encoders, abilities, complete HUD lifecycle, inventory ownership/player dispatch and shop transactions, complete scalar wrappers and s64, general backing memory and unrestricted callback dispatch'}
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(f'{len(rows)} exact message cases written')

if __name__ == '__main__':
    main()
