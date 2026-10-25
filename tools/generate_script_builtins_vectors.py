#!/usr/bin/env python3
"""Synthetic exact-state vectors for pinned RTA variable and campaign built-ins."""
import argparse
import ast
import gzip
import hashlib
import importlib.util
import json
import random
import subprocess
import sys
from pathlib import Path

COMMIT = '356add27d50e50972b2a671b31e8f4ec93e6778c'
PINS = {
    'tools/ref_natives.py': '5ba711f93299a6cdb684651ae5a3dda5c0ddc5370baeea3a54ff123f1cc44839',
    'docs/spec/script-builtins.md': '2beea1e91b3f71770a3d2ecd963fa856b9ee78a08647a2e218c03c53678cc2b4',
    'tools/difftest_natives.py': 'c5e3d4367db8919ce8dd73e106fd6b8e0510cc2889e801bdc17203e5eb83251b',
    'tools/ref_amx.py': '091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
    'tools/ref_objects.py': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2',
}
NATIVES = ['getv', 'setv', 'addv', 'getpv', 'setpv']
CAMPAIGN = ['getEvil', 'setEvil', 'getMadeDecision', 'setMadeDecision',
            'grantMission', 'getMissionComplete', 'setMissionAvailable',
            'getMissionAvailable', 'grantGold', 'grantSilver', 'grantBronze',
            'getMedal', 'addQuest', 'removeQuest']
PLAYERS = ['getNumPlayers', 'countPlayersAlive', 'checkPlayerAlive', 'activePlayerIsDwarf', 'activePlayerIsElf', 'isClericOrWizard', 'getMaxPlayerHP', 'healPlayer', 'giveFullHealth', 'zeroPlayerMana', 'refillPlayerMana', 'givePlayerExp', 'givePlayerGold', 'takePlayerGold', 'checkPlayerGold', 'checkPlayerMinGold', 'playerAdjustStat', 'setPlayerFlag', 'getPlayerFlag', 'piercePlayerEar', 'clearAllSkills', 'hidePlayers', 'decayPlayerLights', 'getManaUsed', 'getHealthUsed']
PORTED_HELPERS = ['s8', 's16', 's32', 'GameState.rec', 'GameState._lower', 'GameState._var_slot', 'GameState.var_get', 'GameState.var_set', 'GameState.vars_clear_all', 'GameState.var_by_index', 'GameState.profile_get', 'GameState.profile_set', 'GameState.mission_byte', 'GameState.mission_is_available', 'GameState.mission_set_available', 'GameState.mission_is_complete', 'GameState.mission_set_complete', 'GameState.mission_grant_medal', 'GameState.mission_get_medal', 'GameState.quest_add', 'GameState.quest_remove', 'GameState.player_set_flag', 'GameState.player_get_flag', 'GameState._difficulty_bit', 'GameState.get_evil', 'GameState._set_choice', 'GameState.set_evil', 'GameState.get_made_decision', 'GameState.set_made_decision', 'GameState.count_players_with_hp', 'Game._refill']
SCENE = ['newWorld', 'newWorldAlways', 'simpleNewWorld', 'newWorldPost', 'newWorldParameter', 'setEntrance', 'cutScene', 'startDialog', 'dialogRunning', 'forceMutexRelease', 'changeSwoop', 'startWithSwoopDone', 'leaveGame', 'endGame', 'exitToMainMenu', 'saveGame', 'saveEndConfig', 'saveDone', 'saveCheckPoint', 'loadCheckPoint', 'setSaveInhibit', 'getSaveInhibit', 'setAlternateDeath', 'preloadResource', 'clearRecall', 'playersLoaded', 'pauseGameControl']
SCENE_UNITS = ["Game.n_" + name for name in SCENE if name != "newWorldAlways"] + ["GameState.net_gate", "GameState.set_next_world", "GameState.dialog_start", "Game.new_world_no_gate"]
MENUS = ['activateMissionMenu', 'activateStore', 'storeRunning', 'setInfo', 'addHelpMessage', 'addHelpMessageF', 'helpMessageActive', 'showCompass', 'showBust', 'hudResetFadeScreen', 'hudFadeScreen', 'revealMap', 'showOnMap', 'runMedalSuccess', 'runMedalFail', 'setupPlayerSel', 'startPlayerSel', 'playerSelRunning']
MENU_UNITS = ["Game.n_" + name for name in MENUS] + ["Game._medal_gate", "Game._medal_prompt", "Game._medal_pad", "GameState.alive", "GameState.find", "GameState.find_first", "GameState.need"]
CREATURES = ['increaseEnemyRank', 'charmMonster', 'rootMonster', 'unRootMonster', 'selectSetMonsterVal', 'setMonsterVal', 'selectSetCFlag', 'setCFlag', 'selectSetOFlag', 'setOFlag', 'setWeaponType', 'setMonsterRange', 'killMonster', 'deleteMonster', 'hideMonster', 'showMonster', 'moveMonster', 'followPlayer', 'stopFollowingPlayer', 'followPath', 'selectFollowPath', 'stopFollowingPath', 'selectStopPath', 'friendly', 'unFriendly', 'reapEnemies', 'checkMonstersLeft', 'getHitPoints', 'setHitPoints', 'generateMonster', 'monstersInTrigger', 'initScriptSetParams', 'setCreatureParams', 'startNPCAnim', 'NPCAnimAtEnd', 'countLiveObjects']
CREATURE_UNITS = ["Game.n_" + name for name in CREATURES] + ['Game._creatures', 'Game.apply_monster_val', 'Game.apply_cflag', 'Game.apply_oflag', 'Game._follow_path', 'Game._stop_path', 'Game._scale', 'Game._start_anim', 'GameState.obj_at']
ITEMS = ['playerHasItem', 'onePlayerHasItem', 'countQuestItems', 'removePlayerItem', 'removeAllItems', 'removeAllWeapons', 'givePlayerItem', 'giveOnePlayerItem', 'givePlayerRandom', 'dropItem', 'selectDropItem']
ITEM_UNITS = ['Game.n_' + name for name in ITEMS] + ['Game._empty_bag', 'Game._drop_at', 'Game._give', 'Game._give_or_drop', 'GameState.player_obj', 'GameState.rand']
EFFECTS = ['trigger', 'autoPropSetState', 'autoPropGetState', 'startPropAnim', 'stopPropAnim', 'propAnimAtEnd', 'deleteObjects', 'setNoCollide', 'objInPlayerTrig', 'findDistance', 'setContainerProps', 'lightEffect', 'addExpandingPool', 'particleMorph', 'setParticle', 'addParticleOnObject', 'shakeCamera', 'setFarClip', 'setWaveDetail']
EFFECT_UNITS = ['Game.n_' + name for name in EFFECTS] + ['Game._particle_def']
VEHICLES = ['putNPCInVehicle', 'putPlayersInVehicle', 'entityRideVehicle', 'controlVehicle', 'holdVehicle', 'getPlayerVPosition', 'getVPosition', 'getPlayerVDir']
VEHICLE_UNITS = ["Game.n_" + name for name in VEHICLES] + ["Game._vehicle_of"]
INTERACTIONS = ['setBestActivePlayer', 'hitPlayers', 'closestPlayerDist', 'savePlayer', 'restorePlayer', 'hornOmen', 'planarSteel', 'playGotItemSound', 'explodePotionsOnly']
INTERACTION_UNITS = ["Game.n_" + name for name in INTERACTIONS] + ["Game._distance3", "Game._spell_effect", "GameState.save_player_record", "GameState.restore_player_record"]
UTILITIES = ['print', 'printVal', 'getRand', 'getScaledTime', 'sin1000', 'cos1000', 'gamePaused', 'demoRunning', 'networkMode', 'levelLoading', 'getLevelTimer', 'sound', 'soundSequence', 'startMusic', 'startAudio', 'endMusic']
SCRIPT_CONTROL = ['callScript', 'addDeferedCall', 'postAction']
SCRIPT_UNITS = ['Game.n_' + name for name in SCRIPT_CONTROL] + ['GameState.' + name for name in ('queue_deferred', 'run_deferred', 'clear_deferred', 'reset_script_state', 'set_post_action', 'run_post_action', 'set_script', 'run_script')]
MEMORY_UNITS = ['Mem.' + name for name in ('__init__', '_page', 'read', 'write', 'load', 'store', 'u8', 'u16', 'u32', 'u64', 's8', 's16', 's32', 'w8', 'w16', 'w32', 'w64', 'cstr', 'strcpy')] + ['_Global.__init__', '_Global.__get__', '_Global.__set__']
OBJECT_UNITS = ['Obj.' + name for name in ('__init__', 'flags', 'flags@setter', 'pos', 'pos@setter', 'angle', 'angle@setter', 'u', 'w', 'cflags', 'set_cflags', '__repr__')] + ['X.__init__']
HOST_UNITS = ['Host.' + name for name in ('__init__', 'call', 'default', '_obj', 'do_obj_moveOffGrid', 'do_obj_updateGridCell', 'do_obj_runEveryFrame', 'do_world_collectObjectsInBox', 'do_world_forEachGridObject', 'do_world_findObjectsByClass', 'do_virtual', 'do_game_rebuildAlivePlayers')]
ARGUMENT_UNITS = ['Args.' + name for name in ('__init__', 'address', '__getitem__', 'u', 'f', 's')] + ['GameState.' + name for name in ('natives', '_bind', '_missing')]
DIVISION_UNITS = ['_tdiv']
SETUP_UNITS = ['GameState.' + name for name in ('__init__', 'ext', 'virtual', 'rebuild_alive_players', 'new_object', 'quest_vars')]
FIELDS = ['cip', 'frm', 'hea', 'stk', 'stp', 'flags', 'curline', 'curfile',
          'dbgcode', 'dbgaddr', 'dbgparam', 'error', 'params']


def number(value):
    return f'{value & 0xffffffff:08x}'


def text(value):
    return value.hex() or '-'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('native/tests/vectors/script_builtins.tsv.gz'))
    args = parser.parse_args()
    for path, pin in PINS.items():
        current = (args.research_dir / path).read_bytes()
        pinned = subprocess.check_output(['git', '-C', str(args.research_dir), 'show', COMMIT + ':' + path])
        if hashlib.sha256(current).hexdigest() != pin or hashlib.sha256(pinned).hexdigest() != pin:
            parser.error('reference pin mismatch: ' + path)
    # Imports are read-only, including their dependencies: never emit __pycache__.
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('natives_reference', args.research_dir / 'tools/ref_natives.py')
    R = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(R)
    A = sys.modules['ref_amx']
    rows = []
    counts = {}
    errors = {}
    mode = 0
    g = m = None
    bindings = NATIVES
    player_mode = False
    scene_mode = False
    menu_mode = False
    creature_mode = False
    item_mode = False
    vehicle_mode = False
    effects_mode = False
    events = []
    control_mode = False

    def reset(campaign=False, players=False, scene=False, menus=False, creatures=False, items=False, effects=False, vehicles=False, interactions=False, utilities=False, control=False):
        nonlocal control_mode, g, m, bindings, player_mode, scene_mode, menu_mode, creature_mode, item_mode, effects_mode, vehicle_mode
        control_mode = control
        vehicle_mode = vehicles
        effects_mode = effects
        item_mode = items
        creature_mode = creatures
        menu_mode = menus
        player_mode = players
        scene_mode = scene
        g = R.Game()
        bindings = NATIVES + CAMPAIGN if campaign else NATIVES
        if players: bindings = NATIVES + CAMPAIGN + PLAYERS
        if scene: bindings = NATIVES + CAMPAIGN + PLAYERS + SCENE
        if menus: bindings = NATIVES + CAMPAIGN + PLAYERS + SCENE + MENUS
        if creatures: bindings += CREATURES
        if control: bindings += SCRIPT_CONTROL
        if utilities: bindings += UTILITIES
        if interactions: bindings += INTERACTIONS
        if vehicles: bindings += VEHICLES
        if effects: bindings += EFFECTS
        if items: bindings += ITEMS
        if items:
            for service in ('inventory_countItemByName', 'inventory_findItemBySubstring', 'inventory_removeItem',
                            'item_parseDescriptor', 'player_itemWeight', 'inventory_giveItem', 'player_recalcLoad',
                            'inventory_equipItem', 'player_resetIdleAnim', 'pool_new', 'ItemProp_ctor2',
                            'level_monsterLevelTimesDifficulty', 'item_generateWithAffixes', 'loot_spawnNamedItem'):
                g.host.handlers[service] = lambda *args, service=service: inventory_service(service, args)
        image = A.build_image([0], data=bytes(4096), natives=bindings, stack_cells=128)
        m = A.Machine(image, natives=g.natives())
        assert m.load() == 0
        g.host.handlers['memcard_requestConfigSave'] = save
        g.host.handlers['net_getLocalPlayerSlot'] = local
        # Explicit host seam, not a port of nested script execution.
        if not control: g.run_script = reward
        for service in ('player_giveExp', 'inventory_onStatPoint', 'invmsg_sendStatPoint', 'sound_playOnObject'):
            g.host.handlers[service] = lambda *args, service=service: player_service(service, args)
        g.host.handlers['res_find'] = resource
        if utilities:
            g.host.handlers['res_find'] = lambda *args: scene_service('res_find', args)
            for service in ('sound_playSample', 'sound_playOnObject', 'sound_setPlaysWhileFrozen', 'music_playLevel', 'stream_playLevel', 'stream_queueStop'):
                g.host.handlers[service] = lambda *args, service=service: scene_service(service, args)
        if creatures:
            for service in ('Creature_setCharm', 'virtual', 'creature_startBossMusic', 'obj_moveOffGrid',
                            'obj_updateGridCell', 'obj_runEveryFrame', 'ScriptGenerator_spawn',
                            'MonsterTrigger_count', 'Creature_scaleStats', 'anim_clear', 'anim_push',
                            'anim_jumpTopToEnd', 'anim_releaseCaches', 'anim_start'):
                g.host.handlers[service] = lambda *args, service=service: scene_service(service, args)
            for service in ('world_collectObjectsInBox', 'world_forEachGridObject'):
                g.host.handlers[service] = lambda *args, service=service: object_query(service, args)
        if interactions:
            for service in ('virtual', 'pool_new', 'SpellHornOmenEffect_ctor', 'SpellPlanarSteelEffect_ctor', 'creature_addSpell', 'player_voice', 'save_playerCopy', 'game_rebuildAlivePlayers'):
                g.host.handlers[service] = lambda *args, service=service: scene_service(service, args)
        if vehicles:
            for service in ('Vehicle_attachNpcDriver', 'Vehicle_setHold', 'virtual', 'obj_updateGridCell'):
                g.host.handlers[service] = lambda *args, service=service: vehicle_service(service, args)
        if effects:
            for service in ('pool_new', 'LightEffect_ctor', 'decal_add', 'ParticleMorphEffect_ctor',
                            'particle_define', 'particle_spawn', 'camera_shake', 'camera_setFarClip'):
                g.host.handlers[service] = lambda *args, service=service: scene_service(service, args)
            g.host.handlers['world_findObjectsByClass'] = lambda *args: object_query('world_findObjectsByClass', args)
        for service in ('net_closeLocalMenus', 'net_isLocalSlot', 'text_lookup', 'hud_showMessage',
                        'cut_start', 'lump_load', 'Camera_update', 'Camera_getYaw', 'Camera_getPitch',
                        'net_sendGameMsg', 'dialog_isOpen', 'menuMutex_release', 'memcard_requestEndGameSave',
                        'memcard_requestSave', 'game_sendCharacterToLobby', 'net_dropAllRemotes',
                        'memcard_getState', 'dbg_printf', 'save_game', 'deathTask_schedule',
                        'net_firstActiveSlot', 'missionMenu_open', 'shop_openOrStock', 'hud_showHelpMessage',
                        'wstr_format', 'wstr_copy', 'hud_setCompassTarget', 'hud_setBust', 'hud_setFade',
                        'hud_stepFade', 'automap_revealAll', 'memcard_isBusy', 'hudmsg_sendMedalAck',
                        'game_openMissionMenu', 'text_begin', 'text_drawWrappedCentered', 'text_end',
                        'hud_drawMessageBox', 'playerSel_isRunning'):
            g.host.handlers[service] = lambda *args, service=service: scene_service(service, args)

    def inventory_service(service, args):
        def encode(v):
            if v == R.ITEM_BLANK: return 'blank:' + text(v[0]) + '/' + '/'.join(number(x) for x in v[1:])
            if isinstance(v, tuple): return '/'.join(number(x) for x in v)
            return text(v) if isinstance(v, bytes) else number(v)
        events.append(service + ''.join(':' + encode(v) for v in args))
        if mode == 1 or (mode == 3 and len(events) >= (g.mem.u32(0x1008) or 2)):
            raise RuntimeError('authored callback exception')
        if mode == 2:
            g.num_players = 1
            g.medal_player = 3
            g.medal_mission = 1
            m.set_cell(3008, -1)
            m.set_cell(3012, -1)
            g.mem.w32(g.rec(0) + R.P_OBJECT, 0x4000)
            g.mem.w32(0x4010, 0x7fffffff)
            g.mem.w32(g.rec(0) + R.P_BAGS, 0)
            g.mem.w32(g.rec(0) + R.P_BAGS + R.BAG_COUNT, 99)
            g.mem.w8(0x121d, -1)
        if service == 'inventory_findItemBySubstring': return (g.mem.u32(0x1110), g.mem.u32(0x1114))
        addresses = {'item_parseDescriptor':0x1100, 'player_itemWeight':0x1104, 'inventory_giveItem':0x1108,
                     'pool_new':0x110c, 'level_monsterLevelTimesDifficulty':0x1118,
                     'item_generateWithAffixes':0x111c, 'inventory_countItemByName':0x1120}
        return g.mem.u32(addresses.get(service, 0x1004))

    def vehicle_service(service, args):
        scene_service(service, args)
        if service == 'virtual' and args[0] == R.V_SEAT:
            return ([g.mem.u32(0x1200+4*k) for k in range(3)], g.mem.u32(0x120c))
        return g.mem.u32(0x1004)

    def object_query(service, args):
        scene_service(service, args)
        return [g.obj_at(g.mem.u32(0x1104 + 4*k)) for k in range(g.mem.u8(0x1100))]

    def scene_service(service, args):
        def encode(v):
            if isinstance(v, tuple): return '/'.join(number(x) for x in v)
            if isinstance(v, str): return text(v.encode())
            return text(v) if isinstance(v, bytes) else number(v)
        events.append(service + ''.join(':' + encode(v) for v in args))
        if mode == 1 or (mode == 3 and len(events) >= (g.mem.u32(0x1008) or 2)): raise RuntimeError('authored callback exception')
        if mode == 2:
            g.active_player = 3
            g.num_players = 1
            m.set_cell(3008, 0x1234)
            g.mem.w32(R.G_DIALOG_DATA, 0)
            if menu_mode:
                g.alive_count = 1
                g.mem.w32(R.G_ALIVE_LIST, 3)
                g.mem.w32(0x2000 + R.O_POS, 0x7fffffff)
                g.medal_ack = 1
                g.medal_player = 3
            if creature_mode:
                g.mem.w32(0x200c, 0x20)
                g.mem.w32(0x20a8, 0x88000001)
                g.mem.w32(0x2014, 0x80000000)
                m.set_cell(3012, -1)
        return g.mem.u32(0x100c) if menu_mode and service == "text_drawWrappedCentered" else g.mem.u32(0x1004)


    def player_service(service, args):
        events.append(service + ':' + ':'.join(number(v) for v in args))
        if mode == 1: raise RuntimeError('authored callback exception')
        if mode in (2, 3):
            g.num_players = 1 if mode == 2 else 2
            g.reward_player = 3
            m.set_cell(3004, 19)
        return 0

    def resource(lump, entry):
        events.append('res_find:' + text(lump) + ':' + text(entry))
        if mode == 1: raise RuntimeError('authored callback exception')
        if mode == 2: g.mem.w32(g.rec(0) + R.P_OBJECT, 0x1234)
        return 0x87654321

    def local(*args):
        events.append('local')
        if mode == 1:
            raise RuntimeError('authored callback exception')
        return R.s32(g.mem.u32(0x1000))

    def reward(name):
        events.append('script:' + name.decode('ascii'))
        if mode == 1:
            raise RuntimeError('authored callback exception')
        if mode == 2:
            g.num_players = 1
            g.mem.w8(g.rec(0) + R.P_MEDALS, 99)
        return 0


    def save():
        events.append('memcard_requestConfigSave')
        if mode == 1:
            raise RuntimeError('authored callback exception')
        if control_mode and mode == 2:
            g.defer_count = 0
            g.defer_flip = 0xffffffff
            g.queue_deferred(b'leaf')
            g.set_post_action(b'leaf')
        if control_mode and mode == 3:
            g.set_script(None)
        if mode in (2, 3):
            g.var_set(b'Callback', g.var_get(b'Callback') + 1)
            g.profile_set(b'Unsaved', 99, False)
        if mode == 3:
            g._slots.clear()
        return 0xdeadbeef  # ignored by profile_set

    def control_machine():
        # Authored bytecode, no game input. Every entry is an independent PROC.
        program = []
        def native(name, *args):
            for arg in reversed(args): program.append(('PUSH.C', arg))
            program.extend([('PUSH.C', 4*len(args)), ('SYSREQ.C', bindings.index(name)), ('STACK', 4*(len(args)+1))])
        def entry(name): program.extend([name+':', ('PROC',)])
        entry('leaf'); program.extend([('CONST.pri', 123), ('RETN',)])
        entry('initial'); program.append(('RETN',))
        entry('error'); program.extend([('CONST.pri', 1), ('ZERO.alt',), ('SDIV',), ('RETN',)])
        entry('halt'); program.extend([('CONST.pri', 77), ('HALT', 9)])
        entry('nested'); native('callScript', 2000); program.append(('RETN',))
        entry('missing'); native('callScript', 2040); program.append(('RETN',))
        entry('queue'); native('addDeferedCall', 2000); native('postAction', 2000); program.extend([('CONST.pri', 42), ('RETN',)])
        entry('profile'); native('setpv', 2080, 12); program.append(('RETN',))
        # Child uses a different string slot value; parent must recover its own.
        entry('reuse'); native('getv', 2080); native('callScript', 2120); native('getv', 0x7fffffff); program.append(('RETN',))
        entry('inner'); native('getv', 2160); program.append(('RETN',))
        code, labels = A.assemble(program)
        current = A.Machine(A.build_image(code, data=bytes(4096), publics=[(name.encode(), labels[name]) for name in ('leaf','initial','error','halt','nested','missing','queue','profile','reuse','inner')], natives=bindings, stack_cells=128))
        assert current.load() == 0
        for ptr, text_ in ((2000,b'leaf'),(2040,b'absent'),(2080,b'Outer'),(2120,b'inner'),(2160,b'Inner')): current.set_string(ptr,text_,False)
        return current

    def snap():
        memory = ','.join(f'{(page << 12) + i:08x}:{value:02x}'
                          for page, data in sorted(g.mem.pages.items())
                          for i, value in enumerate(data) if value) or '-'
        writes = ','.join(f'{address:08x}:{size:08x}' for address, size in g.mem.writes) or '-'
        slots = ','.join(f'{slot:08x}:{address & 0xffffffff:08x}' for slot, address in sorted(g._slots.items())) or '-'
        assert scene_mode or all(name in ('memcard_requestConfigSave', 'net_getLocalPlayerSlot', 'res_find', 'sound_playOnObject', 'player_giveExp', 'inventory_onStatPoint', 'invmsg_sendStatPoint') for name, params in g.host.log)
        event_text = ','.join(events) or '-'
        registers = ','.join(number(getattr(m, name)) for name in FIELDS)
        state = '|'.join([memory, writes, slots, event_text, registers, bytes(m.image).hex()])
        if scene_mode:
            props = ','.join(number(addr) + ':' + ('null' if obj.props is None else '/'.join(text(v) for v in obj.props))
                             for addr, obj in sorted(g.world.objects.items())) or '-'
            state += '|' + props
        if menu_mode:
            entries = ','.join(number(i) + ':' + text(k) + ':' + (number(g.world.names.objs[i].addr) if g.world.names.objs[i] else '-')
                               for i, k in enumerate(g.world.names.keys) if k is not None) or '-'
            state += '|' + number(g.world.names.count) + ':' + entries
        if creature_mode:
            state += '|' + (','.join(number(addr) + ':' + text(obj.type_name)
                                   for addr, obj in sorted(g.world.objects.items())) or '-')
        if item_mode: state += '|' + number(g.draws)
        if control_mode: state += "|" + number(g.machine is not None) + ":" + number(g.entry_pri)
        return state

    def emit(op, name=b'', value=0, pack=False, address=0, count=8, cb=0):
        nonlocal mode, m
        mode = cb
        if op == 'resetcontrol': reset(True, True, True, control=True)
        if op == 'resetutilities': reset(True, True, True, utilities=True)
        if op == 'resetinteractions': reset(True, True, True, True, interactions=True)
        if op == 'resetvehicles': reset(True, True, True, True, vehicles=True)
        if op in ('reset', 'resetcampaign', 'resetplayers', 'resetscene', 'resetmenus', 'resetcreatures', 'resetitems', 'reseteffects'):
            reset(op != 'reset', op in ('resetplayers', 'resetscene', 'resetmenus', 'resetcreatures', 'resetitems', 'reseteffects'), op in ('resetscene', 'resetmenus', 'resetcreatures', 'resetitems', 'reseteffects'), op in ('resetmenus','resetcreatures','resetitems','reseteffects'), op in ('resetcreatures','resetitems','reseteffects'), op == 'resetitems', op == 'reseteffects')
        g.mem.writes.clear()
        g.host.log.clear()
        events.clear()
        result = '-'
        try:
            if op == 'tdiv':
                def decode(token):
                    sign, magnitude = token.split(b':')
                    raw = b'' if magnitude == b'-' else bytes.fromhex(magnitude.decode())
                    return (-1 if sign == b'1' else 1) * int.from_bytes(raw, 'little')
                a, b = map(decode, name.split(b'/'))
                answer = R._tdiv(a, b)
                result = str(int(answer < 0)) + ':' + text(abs(answer).to_bytes((abs(answer).bit_length() + 7) // 8, 'little'))
            elif op == 'setupobject':
                obj = g.new_object([name], flags=value, pos=(address, 0, 0), angle=count,
                                   props=[b'name=Alias', b'Name=Ignored'] if pack else None,
                                   addr=address if cb else None, size=count & 0xffffffff)
                result = number(obj.addr)
            elif op == 'setupalive':
                g.mem.w32(R.G_NUM_PLAYERS, value)
                for slot in range(max(value, 0)):
                    g.mem.w32(g.rec(slot) + R.P_OBJECT, address if slot % 2 == 0 else 0)
                g.rebuild_alive_players()
            elif op == 'setupvars':
                result = ','.join(text(n) + ':' + number(v) for n, v in g.quest_vars()) or '-'
            elif op in ('setupext', 'setupvirtual'):
                g.host.handlers['virtual' if op == 'setupvirtual' else name.decode()] = lambda *args: value
                answer = g.virtual(g.obj_at(address), count, name) if op == 'setupvirtual' else g.ext(name.decode(), address, count)
                result = number(answer)
                for label, args_ in g.host.log:
                    events.append('forward:' + label + ':' + ';'.join(text(v) if isinstance(v, bytes) else number(v) for v in args_))
            elif op in bindings or op == 'nulcell':
                m.set_string(0, name, pack)
                if op == 'nulcell':
                    m.set_cell(0, 0x80000000)
                    m.set_cell(4, 0)
                if op == 'addQuest':
                    m.set_string(800, b'Body', pack)
                m.set_cell(3000, count)
                m.set_cell(3004, address)
                m.set_cell(3008, value)
                m.set_cell(3012, count if player_mode else 0xfacefeed)
                if menu_mode and op in MENUS:
                    m.set_string(800, b'Body', pack)
                    m.set_string(1200, b'Second', pack)
                    m.set_string(1600, b'Third', pack)
                    if op == 'runMedalSuccess':
                        for i, ptr in enumerate((0, 800, 1200, 1600)): m.set_cell(3012 + 4*i, ptr)
                    if op == 'addHelpMessageF':
                        m.set_cell(3012, 800)
                        m.set_cell(3016, count)
                    if op == 'showBust': m.set_cell(3016, -1)
                if creature_mode and op in CREATURES:
                    m.set_string(800, b'Body', pack)
                    m.set_string(1200, b'Second', pack)
                    if op == 'startNPCAnim': m.set_cell(3012,1200); m.set_cell(3016,count)
                    if op in ('initScriptSetParams','setCreatureParams'):
                        for k in range(3,6): m.set_cell(3004+4*k, [count,-2147483648,2147483647][k-3])
                if item_mode and op in ITEMS:
                    m.set_string(800, b'Body', pack)
                    m.set_cell(3016, value)
                if effects_mode and op in EFFECTS:
                    m.set_string(800, b'Body', pack)
                    for k in range(3, 15): m.set_cell(3004 + 4*k, (0, -1, 1000, -1000, 0x7fffffff, -0x80000000)[k % 6])
                if op in ('sound', 'soundSequence'): m.set_string(800, b'Body', pack)
                if op == 'hitPlayers': m.set_cell(3016, value)
                if vehicle_mode and op in VEHICLES:
                    m.set_string(800, b'Body', pack)
                    m.set_cell(3016, -2147483648)
                    m.set_cell(3020, 2147483647)
                    m.set_cell(3024, value)
                m.error = 7
                error, result = m._call_native(bindings.index('setv' if op == 'nulcell' else op), 3000)
                assert error == 0
                result = number(result)
            elif op in ('shortadd', 'shortset'):
                m.set_string(0, name, False)
                params = len(m.image) - m.data_offset - 8
                m.set_cell(params, 0)
                m.set_cell(params + 4, 0)
                m.error = 7
                m._call_native(NATIVES.index('addv' if op == 'shortadd' else 'setv'), params)
            elif op == 'get':
                result = number(g.var_get(name))
            elif op == 'set':
                g.var_set(name, value)
            elif op == 'pget':
                result = number(g.profile_get(name))
            elif op == 'pset':
                g.profile_set(name, value, pack)
            elif op == 'clear':
                g.vars_clear_all()
            elif op == 'index':
                entry = g.var_by_index(R.s32(value))
                result = '-' if entry is None else text(entry[0]) + ':' + number(entry[1])
            elif op == 'forget':
                g._slots.clear()
            elif op == 'unknown':
                R.GameState._missing('unported')(m, [])
            elif op in ('argread','argu','argf','argaddress','args','bindargs','missing','bindtable'):
                m.set_string(0, b'Argument bytes', pack)
                m.params = 3000
                m.set_cell(3000, 0) # adapter ignores pushed count
                m.set_cell(3004 + 4*count, address)
                adapter = R.Args(m, None if cb == 1 else g, name.decode('latin1'))
                if cb == 2: m.params = 2000 # captured base stays at 3004
                if op == 'argread': result = number(adapter[count])
                elif op == 'argu': result = number(adapter.u(count))
                elif op == 'argf': result = number(adapter.f(count))
                elif op == 'argaddress': result = number(adapter.address(count))
                elif op == 'args': result = text(adapter.s(count, value))
                elif op == 'missing': R.GameState._missing(name.decode('latin1'))(m, [123])
                elif op == 'bindtable':
                    table = g.natives()
                    result = number(len(table))
                    table[name.decode('latin1')](m, [123])
                else:
                    def method(machine, arguments):
                        events.append('bound:' + number(machine is m) + ':' + number(arguments.base))
                        if cb == 3: raise RuntimeError('authored bound failure')
                        return arguments.address(count)
                    result = number(g._bind(name.decode('latin1'), method)(m, [123]))
            elif op in ('hostdefault', 'hostcall', 'hostdirect', 'hostobject'):
                from types import SimpleNamespace
                first, second = R.O.GameObject(0x2000), R.O.GameObject(0x4000)
                def encode(v):
                    if v is None: return '-'
                    if isinstance(v, int): return number(v)
                    if isinstance(v, bytes): return text(v)
                    if isinstance(v, R.O.GameObject): return 'obj:' + number(v.addr)
                    if isinstance(v, list): return 'list:' + (','.join(number(o.addr) for o in v) or '-')
                    if isinstance(v, tuple): return '(' + ';'.join(encode(i) for i in v) + ')'
                    raise AssertionError(type(v))
                def delegate(label, *args):
                    events.append(label + ':' + ';'.join(encode(i) for i in args))
                    if cb == 5: raise RuntimeError('authored delegated failure')
                class World:
                    objects = {first.addr:first, second.addr:second}
                    def move_off_grid(self, obj): delegate('move',obj)
                    def update_grid_cell(self, obj): delegate('grid',obj)
                    def run_every_frame(self, obj): delegate('run',obj)
                    def collect_in_box(self, lo, hi, limit):
                        delegate('box',tuple(lo),tuple(hi),limit)
                        return [second,first,second]
                    def for_each_grid_object(self, visit):
                        delegate('walk'); visit(second); visit(first); visit(second)
                    def find_by_class(self, kind, limit):
                        delegate('class',kind,limit); return [first,second]
                    def on_destroy(self, obj, flags): delegate('destroy',obj,flags)
                owner=SimpleNamespace(world=World(), rebuild_alive_players=lambda:delegate('alive'))
                host=R.Host(owner)
                label=name.decode('ascii')
                args=()
                if label=='virtual': args=(value,address & R.M32,count)
                elif label.startswith('obj_'): args=(address & R.M32,)
                elif label=='world_collectObjectsInBox': args=((address & R.M32,0,0xffffffff),(0x80000000,value & R.M32,1),count)
                elif label=='world_forEachGridObject': args=(address,)
                elif label=='world_findObjectsByClass': args=(b'Authored',count)
                else: args=(address,value,count)
                if label=='game_rebuildAlivePlayers': args=()
                if cb in (1,2,4):
                    def override(*args):
                        events.append('override:' + ';'.join(encode(i) for i in args))
                        if cb==2: raise RuntimeError('authored override failure')
                        if cb==4: return host.call('res_find' if label=='net_firstActiveSlot' else 'net_firstActiveSlot')
                        return -7
                    host.handlers[label]=override
                elif cb==3: host.handlers[label]=None
                try:
                    if op=='hostobject': answer=host._obj(address & R.M32)
                    elif op=='hostdefault': answer=host.default(label,args)
                    elif op=='hostdirect': answer=getattr(host,'do_'+label)(*args)
                    else: answer=host.call(label,*args)
                    result=encode(answer)
                finally:
                    events.extend('log:' + n + ':' + ';'.join(encode(i) for i in a) for n,a in host.log)
            elif op.startswith('objmem'):
                obj = R.Obj(None if cb == 2 else g.mem, address & R.M32, b'10GameObject')
                integer = int.from_bytes(name, 'little') * (-1 if pack else 1)
                if op == 'objmeminit':
                    obj.type_name = name or b'10GameObject'
                    result = text(repr(obj).encode('latin-1'))
                elif op == 'objmemflags': result = number(obj.flags)
                elif op == 'objmemsetflags': obj.flags = integer
                elif op == 'objmempos': result = ':'.join(number(v) for v in obj.pos)
                elif op == 'objmemsetpos':
                    obj.pos = [int.from_bytes(name[i*17:(i+1)*17], 'little') * (-1 if pack else 1) for i in range(count)]
                elif op == 'objmemangle': result = number(obj.angle)
                elif op == 'objmemsetangle': obj.angle = integer
                elif op == 'objmemu':
                    integer = obj.u(value, count)
                    result = text(integer.to_bytes((integer.bit_length()+7)//8, 'little'))
                elif op == 'objmemw': obj.w(value, count, integer)
                elif op == 'objmemcflags': result = f'{obj.cflags():016x}'
                elif op == 'objmemsetcflags': obj.set_cflags(integer)
                else: raise AssertionError(op)
            elif op == 'descriptor':
                descriptor = R.X(address & R.M32) if pack else R.X(address & R.M32, name.split(b'|')[0].decode('latin-1'), value, count, name.split(b'|')[1].decode('latin-1'), name.split(b'|')[2].decode('latin-1'))
                result = ':'.join((number(descriptor.addr), text(descriptor.ints.encode('latin-1')), number(descriptor.floats), number(descriptor.stack), text(descriptor.ret.encode('latin-1')), text(descriptor.note.encode('latin-1'))))
            elif op == 'membacking':
                class Backing:
                    def read(self, addr, size):
                        events.append('backing:' + number(addr) + ':' + number(size))
                        if cb == 1: raise RuntimeError('authored backing failure')
                        return bytes(((addr >> 12) + i * 17 + value) & 255 if i < 8 else 0 for i in range(min(size,count) if pack else size))
                g.mem = R.Mem(Backing())
            elif op == 'mempage':
                page = g.mem._page(address & R.M32)
                page[value & 4095] = count & 255
            elif op == 'memread': result = text(g.mem.read(address, count))
            elif op == 'memload':
                integer = g.mem.load(address, count)
                result = text(integer.to_bytes((integer.bit_length()+7)//8, 'little'))
            elif op == 'memstore':
                integer = int.from_bytes(name, 'little') * (-1 if pack else 1)
                g.mem.store(address, count, integer)
            elif op == 'memwrite': g.mem.write(address, name)
            elif op == 'memtyped':
                kind = name.decode()
                if kind.startswith('w'): getattr(g.mem, kind)(address, value)
                else: result = f'{getattr(g.mem, kind)(address) & R.M64:016x}'
            elif op == 'memcstr': result = text(g.mem.cstr(address, count))
            elif op in ('globalget', 'globalset'):
                descriptor = R._Global(address, name.decode(), 'authored descriptor')
                if op == 'globalset': descriptor.__set__(g, value)
                else: result = f'{descriptor.__get__(g) & R.M64:016x}'
            elif op == 'controlmachine':
                m = control_machine()
                g.set_script(m)
            elif op == 'selectscript': g.set_script(m if value else None)
            elif op == 'entrypri': g.entry_pri = R.s32(value)
            elif op == 'runscript': result = number(g.run_script(None if pack else name))
            elif op == 'queuedeferred': g.queue_deferred(name)
            elif op == 'rundeferred': g.run_deferred()
            elif op == 'cleardeferred': g.clear_deferred()
            elif op == 'resetstate': g.reset_script_state()
            elif op == 'setpost': g.set_post_action(name)
            elif op == 'runpost': g.run_post_action()
            elif op == 'scriptcell':
                m.set_cell(address, value)
            elif op == 'poke32':
                g.mem.w32(address, value)
            elif op == 'poke8':
                g.mem.w8(address, value)
            elif op == 'pokestr':
                g.mem.strcpy(address, name)
            elif op == 'complete':
                g.mission_set_complete(R.s32(count), R.s32(address), R.s32(value))
            elif op == 'medal':
                result = number(g.mission_get_medal(R.s32(address), R.s32(value), int(pack)))
            elif op == 'choice':
                g.set_evil(value)
            elif op == 'playerflag':
                g.player_set_flag(R.s32(address), R.s32(value), R.s32(count))
            elif op == 'getplayerflag':
                result = number(g.player_get_flag(R.s32(address), R.s32(value)))
            elif op == 'objectprops':
                obj = g.world.objects.get(address & 0xffffffff) if effects_mode else None
                if obj is None: obj = R.Obj(g.mem, address & 0xffffffff)
                obj.props = None if value else ([] if not name else name.split(b'|'))
                g.world.objects[address & 0xffffffff] = obj
            elif op == 'objecttype':
                g.obj_at(address & 0xffffffff).type_name = name
            elif op == 'vehicleof': result = number(g._vehicle_of(R.s32(address)))
            elif op == 'playerobj':
                obj = g.player_obj(address)
                result = '-' if obj is None else number(obj.addr)
            elif op == 'randdraw':
                result = number(g.rand())
            elif op == 'emptybag':
                g._empty_bag(address, value)
            elif op == 'objat':
                obj = g.obj_at(address & 0xffffffff)
                result = '-' if obj is None else number(obj.addr)
            elif op == 'nameadd':
                obj = g.world.objects.get(address & 0xffffffff)
                if obj is None:
                    obj = R.Obj(g.mem, address & 0xffffffff)
                    g.world.objects[obj.addr] = obj
                g.world.names.add(name, obj)
            elif op == 'nameremove':
                g.world.names.remove(name, g.world.objects[address & 0xffffffff])
            elif op == 'menugate': result = number(g._medal_gate())
            elif op == 'menuprompt': result = number(g._medal_prompt())
            elif op == 'menupad': result = number(g._medal_pad())
            elif op == 'alive': result = number(g.alive(R.s32(address)))
            elif op == 'nextworld':
                g.set_next_world(name)
            elif op == 'netgate':
                result = number(g.net_gate())
            elif op == 'dialogstart':
                g.dialog_start(name, R.s32(address), R.s32(value))
            elif op == 'nogate':
                m.set_string(0, name, pack)
                m.params = 3000
                m.set_cell(3004, address)
                result = number(g.new_world_no_gate(m, R.Args(m, g, 'newWorld')))
            elif op == 'hpcount':
                result = number(g.count_players_with_hp())
            elif op not in ('resetcontrol', 'resetutilities', 'resetinteractions', 'resetvehicles', 'reset', 'resetcampaign', 'resetplayers', 'resetscene', 'resetmenus', 'resetcreatures', 'resetitems', 'reseteffects'):
                raise AssertionError(op)
        except ZeroDivisionError:
            result = 'ZeroDivisionError'
        except IndexError:
            result = 'IndexError'
        except R.StackGarbage:
            result = 'StackGarbage'
        except A.HostFault:
            result = 'HostFault'
        except RuntimeError:
            result = 'CallbackFault'
        inputs = '|'.join([text(name), number(value), str(int(pack)), number(address), number(count), str(cb)])
        rows.append('\t'.join([op, inputs, result, snap()]))
        counts[op] = counts.get(op, 0) + 1
        if result in ('StackGarbage', 'HostFault', 'CallbackFault', 'IndexError', 'ZeroDivisionError'):
            errors[result] = errors.get(result, 0) + 1

    # Stateful edge sequences: insertion on read; equal writes; case folding;
    # empty names, lower-bound/upper-bound clamp, signed32 overflow and callbacks.
    emit('reset')
    for name in (b'One', b'TWO', b'', b'A-Z_09 !', b'xyz'):
        emit('getv', name, count=0)
        for value in (-2147483648, -129, -128, -1, 0, 1, 127, 128, 2147483647):
            emit('setv', name, value, pack=True, count=4)
            emit('getv', name.lower(), count=12)
            emit('addv', name, value, count=-4)
        emit('setpv', name, 100, cb=2)
        emit('setpv', name.lower(), 100, cb=1)  # equal => callback not called
        emit('setpv', name, -200, cb=1)  # write prefix survives callback exception
        emit('getpv', name)
    emit('clear')
    for index in (-2147483648, -1, 0, 1, 2, 3, 4, 5, 2147483647):
        emit('index', value=index)
    # Known 0x20 slot survives these handlers, including rejected heap/top/negative
    # pointers; a rejected pointer before conversion or after forgetting is unknown.
    emit('reset')
    emit('getv', b'ignored', address=4096)
    emit('setv', b'Last', 70)
    for op in NATIVES:
        for address in (4096, 4608, -4):
            emit(op, b'Current', 5, address=address)
    emit('forget')
    emit('getpv', b'ignored', address=4096)
    emit('setpv', b'Nested', 12, cb=3)
    emit('getv', b'ignored', address=4096)
    emit('unknown')
    for address in (4, 8, 2, -4, 4604):
        emit('getv', b'Address', address=address)
    emit('getv', b'Count', count=10000)
    emit('shortadd', b'CreatedBeforeFault')
    emit('shortset', b'NotCreated')
    # Strings and faults retain conversion slot even when subsequent processing
    # refuses. 159-byte names fit; 160 do not. High bytes are explicitly refused.
    for pack in (False, True):
        for length in (0, 1, 2, 3, 4, 79, 159, 160, 161):
            emit('getv', b'Q' * length, pack=pack)
        for name in (b'\x80', b'x\xff', b'\xffabc', b'\x7fAZ'):
            emit('setv', name, 55, pack=pack)
    # Direct helpers, with profile save disabled and index traversal retained.
    emit('reset')
    for value in (-129, -128, 0, 127, 128):
        emit('set', b'Direct', value)
        emit('get', b'DIRECT')
        emit('pset', b'Profile', value, pack=False)
        emit('pget', b'PROFILE')
        emit('pset', b'Profile', value, pack=True, cb=1)
    emit('pset', b'NewZero', 0, pack=True)
    emit('pset', b'NewZero', 0, pack=True, cb=1)
    emit('clear')
    emit('index', value=0)
    emit('index', value=1)
    # Unpacked strings terminate on a zero cell, not a zero low byte. A negative
    # first cell can therefore yield an embedded NUL. Preserve the pinned model's
    # byte-string table semantics rather than guessing C-library behavior.
    emit('reset')
    emit('nulcell', value=55)
    for name in (b'a\0b', b'\0', b'A\0XYZ'):
        emit('set', name, 17)
        emit('get', name)
        emit('pset', name, -4, pack=True)
        emit('pget', name)
    # Table walks span pages and the quest table's nominal 0x2000-byte area.
    # The reference has no bounds check; no adjacent service is inferred here.
    emit('reset')
    for i in range(70):
        name = f'Entry{i:03d}_'.encode() + b'x' * 141
        emit('set', name, i - 35)
        emit('get', name.upper())
    for i in (-1, 0, 32, 69, 70):
        emit('index', value=i)
    emit('clear')
    # Seeded sequences use no game data. Include every native and host helper,
    # with packed/unpacked strings, stale slots, too few/too many pushed cells.
    rng = random.Random(0x4e415449)
    names = [b'Alpha', b'ALPHA', b'beta', b'Gamma', b'', b'__', b'a b', b'9']
    for block in range(12):
        emit('reset')
        for i in range(80):
            op = rng.choice(NATIVES + ['get', 'set', 'pget', 'pset', 'clear', 'index', 'forget'])
            value = rng.choice([-2147483648, -129, -128, -1, 0, 1, 127, 128, 2147483647, R.s32(rng.getrandbits(32))])
            emit(op, rng.choice(names), value, bool(rng.getrandbits(1)),
                 rng.choice([0, 0, 0, 4096]), rng.choice([-4, 0, 4, 8, 12]), rng.randrange(4))
    # Campaign cases append to the unchanged variable corpus. New AMX native
    # tables are used only after resetcampaign, preserving old image bytes.
    def setup(players=4, difficulty=0):
        emit('resetcampaign')
        emit('poke32', value=players, address=R.G_NUM_PLAYERS)
        emit('poke32', value=difficulty, address=R.G_DIFFICULTY)

    for players in (-1, 0, 1, 3, 4):
        for difficulty in (-1, 0, 2, 7, 8, 31, 32, -2147483648, 2147483647):
            setup(players, difficulty)
            for op in ('getEvil', 'getMadeDecision', 'setEvil', 'setMadeDecision'):
                for on in (0, 1, -1):
                    emit(op, address=on, count=0)
            emit('choice', value=0)
            emit('getEvil')
            for mission in (-2147483648, -1, 0, 12, 13, 2147483647):
                for on in (-2, -1, 0, 1, 2, 3):
                    emit('setMissionAvailable', address=mission, value=on, count=0)
                    emit('getMissionAvailable', address=mission)
                emit('grantMission', address=mission)
                emit('getMissionComplete', address=mission)
                emit('complete', address=mission, value=0, count=difficulty)
                emit('getMissionComplete', address=mission)
    # A negative mission can alias the player-count word. range() captures
    # the original count, even when its first write changes that global.
    setup(2)
    alias = R.G_NUM_PLAYERS - (R.G_PLAYERS + R.P_MISSIONS)
    emit('setMissionAvailable', address=alias, value=1)
    for value in (-2, -1, 0, 1, 2, 3):
        emit('complete', address=0, value=value, count=0)
        emit('getMissionComplete', address=0)
    for flag in (False, True):
        for network in (0, 1):
            emit('poke8', address=R.G_NETWORK_MODE, value=network)
            for level in (-128, -1, 0, 1, 3, 127):
                emit('medal', address=0, value=level, pack=flag)
    for have in (-128, -1, 0, 1, 2, 3, 127):
        for mission in (-1, 0, 12, 13):
            setup()
            for slot in range(4):
                emit('poke8', value=have, address=(g.rec(slot)+R.P_MEDALS+mission)&0xffffffff)
            for op in ('grantBronze', 'grantSilver', 'grantGold', 'getMedal'):
                emit(op, address=mission)
            emit('getMedal', address=mission, cb=1)
            emit('poke32', value=-1, address=0x1000)
            emit('getMedal', address=mission)
            emit('poke8', value=1, address=R.G_NETWORK_MODE)
            emit('medal', address=mission, value=2, pack=True, cb=1)
    for mode in (1, 2):
        setup()
        emit('grantGold', address=0, cb=mode)
        emit('getMedal', address=0)
    # Quest duplicate/full early returns; negative counts and overflowing strings
    # are retained rather than adding a speculative field-size bound.
    for count in (-1, 0, 7, 8):
        setup()
        emit('poke32', value=count, address=g.rec(1)+R.P_QUEST_COUNT)
        for title in (b'Alpha', b'Beta', b'Alpha', b'', b'X'*31, b'Y'*33, b'Z'*159, b'Z'*160):
            emit('addQuest', title, value=800)
        for title in (b'Alpha', b'Absent', b'Beta', b''):
            emit('removeQuest', title)
    setup(1)
    for i, title in enumerate((b'Dup', b'Dup', b'Tail')):
        emit('pokestr', title, address=g.rec(0)+R.P_QUESTS+64*i)
        emit('pokestr', b'Text', address=g.rec(0)+R.P_QUESTS+64*i+32)
    emit('poke32', value=3, address=g.rec(0)+R.P_QUEST_COUNT)
    emit('removeQuest', b'Dup')
    # Slot 0x30 is shared by both addQuest arguments, independent of 0x20.
    setup(1)
    emit('addQuest', b'Title', value=4096)
    emit('removeQuest', b'Title', address=4096)
    emit('addQuest', b'Title', value=800, pack=True)
    emit('addQuest', b'Current', address=4096, value=4096)
    emit('removeQuest', b'Current')
    emit('forget')
    emit('addQuest', b'Title', address=4096, value=800)
    emit('addQuest', b'Title', address=0, value=800, count=10000)
    for block in range(4):
        setup(4, block)
        for i in range(100):
            op=rng.choice(CAMPAIGN)
            mission=rng.choice([-1, 0, 1, 12, 13])
            emit(op, rng.choice([b'A', b'B', b'A', b'']),
                 value=800 if op=='addQuest' else rng.choice([-1, 0, 1, 2]),
                 address=0 if op in ('addQuest','removeQuest') else mission,
                 count=rng.choice([0, 4, 8]), pack=bool(rng.getrandbits(1)))
    # Player-record family: separate image table keeps all previous rows identical.
    def player_setup(players=4):
        emit('resetplayers')
        emit('poke32', address=R.G_NUM_PLAYERS, value=players)
        emit('pokestr', b'Synthetic', address=R.S_HEAL_LUMP)
        emit('pokestr', b'Sample', address=R.S_HEAL_SOUND)
        for slot in range(4):
            rec=g.rec(slot)
            for offset, value in ((R.P_HP, [0, 0x3f800000, 0xbf800000, 0x7fffffff][slot]),
                                  (R.P_MAX_HP, 0x41200000+slot), (R.P_MANA, 0x3f800000),
                                  (R.P_MAX_MANA, 0x41a00000+slot), (R.P_GOLD, [0, -1, 99999998, 2147483647][slot]),
                                  (R.P_CLASS, slot+1), (R.P_MODEL, slot), (R.P_OBJECT, slot+100)):
                emit('poke32', address=rec+offset, value=value)
    for players in (-1, 0, 1, 2, 4):
        player_setup(players)
        for op in PLAYERS:
            emit(op, address=0, value=4, count=0)
        for slot in (-2147483648, -2, -1, 0, 1, 3, 4, 2147483647):
            for op in ('getMaxPlayerHP','isClericOrWizard','piercePlayerEar'):
                emit(op, address=slot, count=0)
            emit('poke32', address=R.G_ACTIVE_PLAYER, value=slot)
            emit('activePlayerIsDwarf', count=0)
            for bit in (-1, 0, 7, 8, 31, 32, -2147483648):
                for on in (0, 1, -1):
                    emit('setPlayerFlag', address=slot, value=bit, count=on)
                    emit('getPlayerFlag', address=slot, value=bit, count=0)
                    emit('getplayerflag', address=slot, value=bit)
                emit('playerflag', address=slot, value=bit, count=1)
        emit('hpcount')
        for amount in (-2147483648, -1, 0, 1, 99999999, 2147483647):
            for op in ('givePlayerGold','takePlayerGold','checkPlayerGold','checkPlayerMinGold'):
                emit(op,address=amount,count=0)
    # Raw float boundaries, signed halfwords, skill reset write order and byte slots.
    player_setup()
    for raw in (0, 0x80000000, 1, 0x3f800000, 0xbf800000, 0x7f800000, 0x7fffffff, 0xffffffff):
        emit('poke32',address=g.rec(0)+R.P_HP,value=raw)
        emit('poke32',address=g.rec(0)+R.P_MAX_HP,value=raw)
        emit('getMaxPlayerHP',address=1,count=0)
        emit('checkPlayerAlive',address=0,count=0)
        emit('healPlayer',count=0)
        emit('giveFullHealth',count=0)
    for raw in (0, 1, 32767, 32768, 65535):
        for addr,op in ((R.G_RESPAWN_COUNTDOWN,'checkPlayerAlive'),(R.G_MANA_USED,'getManaUsed'),(R.G_HEALTH_USED,'getHealthUsed')):
            emit('poke32',address=addr,value=raw)
            emit(op,address=1,count=0)
    for slot in (0, 3, 255):
        emit('poke8',address=R.G_MEDAL_PLAYER,value=slot)
        emit('poke32',address=g.rec(slot)+R.P_SKILL_MASK,value=-1)
        emit('clearAllSkills',count=0)
    for value in (-256, -1, 0, 1, 256): emit('hidePlayers',address=value,count=0)
    for alive in (0, 2, 3):
        emit('poke32',address=R.G_ALIVE_COUNT,value=alive)
        for start in (0, 0x3f800000, 0xbf800000, 0x7fffffff):
            for step in (-100, -1, 0, 1, 100):
                emit('poke32',address=R.G_LIGHT_SCALE,value=start)
                emit('decayPlayerLights',address=50,value=-50,count=step)
    # Captured reward slot, live experience argument/count, callback failures.
    for cb in (0,1,2,3):
        player_setup()
        emit('givePlayerExp',address=-1,count=0,cb=cb)
        emit('healPlayer',count=0,cb=cb)
        for stat in (3,4,8,9,-2147483648):
            for slot in (-1,0,-2):
                emit('playerAdjustStat',address=slot,value=stat,count=3,cb=cb)
    player_setup()
    emit('setv',b'Stale',value=1)
    for op in PLAYERS: emit(op,address=0,value=4,count=0)
    emit('getv',b'Refused',address=4096)
    for _ in range(150):
        op=rng.choice(PLAYERS)
        emit(op,address=rng.choice([-1,0,1,3,4]),value=rng.choice([-1,0,4,8,31]),count=rng.choice([0,1,3]))
    # Complete scene-lifecycle family; old corpus remains an identical prefix.
    def scene_setup(players=4):
        emit('resetscene')
        emit('poke32', address=R.G_NUM_PLAYERS, value=players)
        emit('poke32', address=0x1004, value=-1)
        emit('pokestr', b'Synthetic', address=R.G_LEVEL_NAME)
        emit('pokestr', b'Blocked', address=R.S_NO_ZONE)
    for players in (-1, 0, 1, 4):
        scene_setup(players)
        for op in SCENE:
            emit(op, b'Scene', address=0, value=-1, count=0)
            emit(op, b'Scene', address=0, value=3, count=8, cb=1)
    scene_setup()
    for op in SCENE: emit(op, b'LateFailure', value=3, count=0, cb=3)
    # Network cooldown and local HUD gate, with post-action bypass and alias.
    for network in (0, 128):
        for active in (-1, 0, 3):
            for alive in (-1, 1, 2):
                for cooldown in (0, 1, 65535):
                    scene_setup()
                    emit('poke8', address=R.G_NETWORK_MODE, value=network)
                    emit('poke32', address=R.G_ACTIVE_PLAYER, value=active)
                    emit('poke32', address=R.G_ALIVE_COUNT, value=alive)
                    emit('poke32', address=(g.rec(active)+R.P_NEWWORLD_COOLDOWN)&0xffffffff, value=cooldown)
                    emit('netgate')
                    for op in ('newWorld', 'newWorldAlways', 'simpleNewWorld'):
                        emit(op, b'Zone')
                    emit('poke8', address=R.G_IN_POST_ACTION, value=255)
                    emit('newWorld', b'Post')
    scene_setup()
    for props in (None, b'', b'orientation=1|orientation= -258tail|level=First|level=Last',
                  b'Orientation=7|orientation=+|level=Empty', b'orientation=9999999999999999999999999999999'):
        emit('objectprops', props or b'', address=0x2000, value=int(props is None))
        emit('poke32', address=R.G_SELECTED_OBJ, value=0x2000)
        for length in (0, 94, 95, 111, 112):
            emit('nextworld', b'Z'*length)
        emit('newWorldParameter')
    for selected in (0, 0xdeadbeef):
        emit('poke32', address=R.G_SELECTED_OBJ, value=selected)
        emit('nextworld', b'Unknown')
        emit('newWorldParameter')
    emit('poke32', address=R.G_SELECTED_OBJ, value=0)
    emit('nogate', b'NoGate')
    # Shared slots 0x20 / 0x80 / 0xd0, stale pointers, buffer boundaries.
    for op in ('newWorld','newWorldAlways','simpleNewWorld','newWorldPost','setEntrance','preloadResource','startDialog','cutScene'):
        scene_setup()
        emit(op, b'Unknown', address=4096)
        for length in (0, 1, 3, 4, 94, 95, 111, 112, 159, 160):
            emit(op, b'X'*length, pack=bool(length&1), value=-1, count=0)
            emit(op, b'Stale', address=4096, value=258, count=0)
        emit('changeSwoop',address=-1)
        emit(op,b'Forgot',address=4096)
    # Dialog packet raw camera bytes, signed/truncated values, open/local gates,
    # ordered failures and callback mutations after lump load/camera update.
    for locality in (0, 1, -1):
        for opened in (0, 7):
            scene_setup()
            emit('poke32',address=0x1004,value=locality)
            emit('poke32',address=R.G_DIALOG_DATA,value=opened)
            emit('poke32',address=R.G_LOCAL_CAMERA,value=0xabcdef01)
            emit('poke32',address=R.G_DIALOG_MSG_ID,value=0x12345678)
            for i, raw in enumerate((0x7fffffff,0x80000000,1)):
                emit('poke32',address=R.G_CAMERA_POS+4*i,value=raw)
            for cb in (0,1,2):
                emit('startDialog',b'ABCDE',address=0,value=-257,count=8,cb=cb)
                emit('dialogstart',b'A\0B',address=-129,value=511,cb=cb)
    for fail_at in range(1, 7):
        scene_setup()
        emit('poke32',address=0x1008,value=fail_at)
        emit('startDialog',b'Packet',address=0,value=1,count=8,cb=3)
        emit('saveEndConfig',cb=3)
    for difficulty in (-1,0,1,2,3,4,5):
        for network in (0,1):
            scene_setup()
            emit('poke32',address=R.G_DIFFICULTY,value=difficulty)
            emit('poke8',address=R.G_NETWORK_MODE,value=network)
            emit('poke32',address=R.G_SAVED_CONFIG,value=0x80000001)
            emit('saveEndConfig',cb=2)
    for status in (0,2,34,-1):
        scene_setup()
        emit('poke32',address=g.rec(2)+R.P_STATUS,value=status)
        emit('saveGame')
    for state in (0,-1,-2,0x7fffffff):
        emit('poke32',address=0x1004,value=state)
        emit('saveDone')
    scene_setup()
    for _ in range(120):
        emit(rng.choice(SCENE),b'Random',address=rng.choice([0,4096]),value=rng.choice([-1,0,1]),count=rng.choice([0,8]),cb=rng.randrange(4))
    # Section 3.4 in its entirety, with new image tables only after resetmenus.
    def menu_setup(network=0, result=0, local=0, medal_player=0, ack=0):
        emit('resetmenus')
        for addr, val in ((R.G_NUM_PLAYERS,4), (R.G_ALIVE_COUNT,4), (0x1004,result),
                          (0x1000,local), (R.G_MEDAL_ACK,ack), (R.G_BTN_CONFIRM,1),
                          (R.G_BTN_CANCEL,2), (R.G_BTN_NAME_A,-2147483648),
                          (R.G_BTN_NAME_B,2147483647), (R.G_HUD_FONT,0x12345678)):
            emit('poke32',address=addr,value=val)
        emit('poke8',address=R.G_NETWORK_MODE,value=network)
        emit('poke8',address=R.G_MEDAL_PLAYER,value=medal_player)
        for i in range(4):
            emit('poke32',address=R.G_ALIVE_LIST+4*i,value=i)
            emit('poke8',address=g.rec(i)+R.P_MEDALS,value=3)
        for addr, key in ((R.S_MEDAL_PROMPT,b'Buttons'),(R.S_MEDAL_WON,b'Won'),
                          (R.S_MEDAL_FAIL,b'Fail'),(R.G_LEVEL_NAME,b'Synthetic')):
            emit('pokestr',key,address=addr)
    for network in (0,1,128):
        for result in (0,1,-1):
            menu_setup(network,result)
            for op in MENUS:
                emit(op,b'Alpha',value=800 if op in ('addHelpMessage','showCompass','showBust') else 0,count=0)
                emit(op,b'Alpha',value=800 if op in ('addHelpMessage','showCompass','showBust') else 0,count=0,cb=1)
                emit(op,b'Alpha',value=800 if op in ('addHelpMessage','showCompass','showBust') else 0,count=0,cb=2)
                emit(op,b'Alpha',value=800 if op in ('addHelpMessage','showCompass','showBust') else 0,count=0,cb=3)
    for store in (-2147483648,-1,0,20,21,65536,2147483647):
        for dialog in (0,1):
            menu_setup(result=dialog)
            emit('activateStore',address=store,count=0)
    for recall in (0,1,128,255):
        menu_setup()
        emit('poke8',address=R.G_RECALL_COUNTDOWN,value=recall)
        emit('activateMissionMenu')
    for val in (-2147483648,-1,0,1,127,128,255,2147483647):
        menu_setup(result=val)
        emit('poke32',address=R.G_SHOP_OPEN,value=val)
        emit('poke8',address=R.G_HELP_OPEN,value=val)
        for op in ('storeRunning','helpMessageActive','hudFadeScreen','playerSelRunning'): emit(op,address=val)
        emit('addHelpMessageF',b'Numbers',value=val,count=val if abs(val)<10000 else 0)
    # All network/local/card/ack/confirm/cancel gate combinations, including skip drawing.
    for network in (0,1,128):
        for local_slot in (-1,0,3):
            for player in (0,3,255):
                for ack in (0,1):
                    for busy in (0,1):
                        menu_setup(network,busy,local_slot,player,ack)
                        emit('menugate')
                        emit('menuprompt')
                        for pressed in (0,1,2,3):
                            emit('poke32',address=R.G_PADS+player*R.PAD_SIZE+12,value=pressed)
                            emit('menupad')
                            emit('runMedalFail',b'Failed',count=0)
                            emit('runMedalSuccess',b'Title',address=0,value=0,count=0)
    for old in (-2147483648,-128,-1,0,1,2,3,127,2147483647):
        for medal in (-128,-1,0,1,2,3,127):
            for height in (0,-17,-34,23,0x7fffffff,0x80000000):
                menu_setup(result=0)
                emit('poke8',address=g.rec(0)+R.P_MEDALS,value=medal)
                # Service result 0 at gate, then switched by an authored host rule.
                emit('poke32',address=0x100c,value=height)
                emit('runMedalSuccess',b'Title',value=old,count=0)
    # Failures at every service stage and live callback mutation.
    for fail_at in range(1,30):
        menu_setup()
        emit('poke32',address=0x1008,value=fail_at)
        emit('runMedalSuccess',b'Failure',count=0,cb=3)
        emit('runMedalFail',b'Failure',count=0,cb=3)
    for op in ('setInfo','addHelpMessage','addHelpMessageF','showCompass','showBust','showOnMap','runMedalSuccess','runMedalFail'):
        menu_setup()
        for length in (0,1,127,128,159,160):
            emit(op,b'X'*length,value=800 if op in ('addHelpMessage','showCompass','showBust') else 0,count=0,pack=bool(length&1))
            emit(op,b'Stale',address=4096,value=4096,count=0)
        emit('forget')
        emit(op,b'Unknown',address=4096,value=4096,count=0)
        emit('setv',b'Shared',value=5)
        emit('setInfo',b'Live',address=4096,count=0)
        emit('addQuest',b'Title',value=800)
        emit('addHelpMessage',b'Reuse',address=4096,value=4096,count=0)
        emit('helpMessageActive')
        emit(op,b'Unknown',address=4096,value=4096,count=0)
    # Existing exact NameTable, duplicate names, tombstones and limit-1 writes.
    menu_setup()
    emit('showCompass',b'Absent',value=800,count=0)
    for i in range(105):
        addr=0x2000+0x1000*i
        emit('nameadd',b'Alpha',address=addr)
        emit('poke32',address=addr+R.O_FLAGS,value=0x20 if i%3 else 0)
        emit('poke32',address=addr+R.C_FLAGS,value=-1)
        emit('poke32',address=addr+R.C_FLAGS+4,value=-1)
        for j,raw in enumerate((0x7fffffff,0x80000000,0x3f800000)):
            emit('poke32',address=addr+R.O_POS+4*j,value=raw)
    emit('nameadd',b'"ALPHA"',address=0x2000)
    emit('nameremove',b'Alpha',address=0x2000)
    for on in (0,1,-1): emit('showOnMap',b'aLPHa',value=on,count=0)
    for cb in (0,1,2,3): emit('showCompass',b'alpha',value=800,count=0,cb=cb)
    emit('showCompass',b'"Alpha"',value=800,count=0)
    for index in (-2147483648,-1,0,3,4,2147483647): emit('alive',address=index)
    menu_setup()
    for _ in range(200):
        emit(rng.choice(MENUS),b'Random',value=rng.choice([-1,0,800,4096]),
             address=rng.choice([0,0,4096]),count=rng.choice([0,4,8]),cb=rng.randrange(4))
    # Complete section 3.7: synthetic registry, raw fields and explicit services.
    def creature_setup():
        emit('resetcreatures')
        for addr, kind, flags in ((0x2000,b'10GameObject',0x20), (0x4000,b'10GameObject',0),
                                 (0x6000,b'15ScriptGenerator',0x28), (0x8000,b'14MonsterTrigger',0x60)):
            emit('nameadd',b'Alpha',address=addr)
            emit('objecttype',kind,address=addr)
            emit('poke32',value=flags,address=addr+R.O_FLAGS)
            emit('poke32',value=0x80000001,address=addr+R.C_FLAGS)
            emit('poke32',value=0x12345678,address=addr+R.C_FLAGS+4)
            emit('poke32',value=0x41f80000,address=addr+R.C_HP)
            emit('poke32',value=0x14 if addr==0x6000 else 0xffff,address=addr+R.C_AI_STATE)
            emit('poke32',value=0x98765432,address=addr+R.O_POS)
            emit('poke32',value=0x7fffffff,address=addr+R.O_POS+4)
            emit('poke32',value=0x80000000,address=addr+R.O_POS+8)
            emit('poke32',value=0xabcd,address=addr+R.O_ANGLE)
            emit('poke32',value=0x12340000,address=addr+R.C_WEAPON_OBJ)
        emit('nameadd',b'Body',address=0x4000)
        emit('poke32',value=0x2000,address=R.G_SELECTED_OBJ)
        emit('poke32',value=0x80000001,address=0x1004)
        emit('poke8',value=4,address=0x1100)
        for i,addr in enumerate((0x2000,0x4000,0x6000,0x8000)):
            emit('poke32',value=addr,address=0x1104+4*i)

    for cb in range(4):
        for op in CREATURES:
            creature_setup()
            # Stateful boundary sequence per handler/callback mode; avoid serializing
            # the identical 54-row setup for every argument value.
            for val in (-2147483648,-1,0,1,2,3,4,5,14,26,65537,2147483647):
                emit(op,b'Alpha',value=800 if op in ('moveMonster','generateMonster','startNPCAnim') else val,
                     address=val if op.startswith('selectSet') or op=='initScriptSetParams' else 0,
                     count=-1 if op=='startNPCAnim' else val,cb=cb,pack=bool(val&1))
                emit(op,b'Alpha',value=800 if op in ('moveMonster','generateMonster','startNPCAnim') else val,
                     address=0,count=val,cb=cb)
    # Null/stale/ordinary/dead selected objects and invalid string addresses.
    for ptr in (0,0xdeadbeef,0x2000,0x4000,0x6000):
        creature_setup()
        emit('objat',address=ptr)
        emit('poke32',value=ptr,address=R.G_SELECTED_OBJ)
        for op in CREATURES: emit(op,b'Missing',value=800,address=0,count=-1)
    creature_setup()
    for op in CREATURES:
        emit(op,b'Alpha',value=800,address=0,count=-1)
        emit(op,b'Alpha',value=4096,address=4096,count=0)
        emit('forget')
        emit(op,b'Alpha',value=4096,address=4096,count=0)
    # Shared-slot chains and boundary copies of all three NPC strings.
    for size in (0,159,160):
        creature_setup()
        for op in ('moveMonster','generateMonster','startNPCAnim'):
            emit(op,b'Z'*size,value=800,address=0,count=-1)
    # Intern-table signed count, case sensitivity, overflow beyond 24-byte entries.
    for count in (-128,-1,0,1,127):
        creature_setup()
        emit('poke8',value=count,address=R.G_GEN_COUNT)
        for value in (800,800,0,4096): emit('generateMonster',b'Alpha',value=value)
    # Name-table limit and tombstone order. 512 slots are authored, not game data.
    emit('resetcreatures')
    for i in range(512):
        emit('nameadd',b'Alpha',address=0x2000+i*0x1000)
        emit('poke32',value=0x28 if i%3==0 else 0x20,address=0x200c+i*0x1000)
    for op in ('countLiveObjects','increaseEnemyRank','getHitPoints'):
        emit(op,b'Alpha')
    emit('nameremove',b'Alpha',address=0x2000)
    emit('nameadd',b'Alpha',address=0x900000)
    emit('setHitPoints',b'Alpha',value=-2147483648)
    # Raw HP patterns including denormals, saturation, signs and exponent 255.
    creature_setup()
    for bits in (0,1,0x80000000,0x3f800000,0xbf800000,0x4f000000,0x7f800000,0xff800000,0x7fffffff):
        emit('poke32',value=bits,address=0x2228)
        emit('getHitPoints',b'Alpha')
    # Pointer-type branch and player exclusion in reapEnemies.
    for kind in (b'P6PhProp',b'P4Prop',b'P8ItemProp',b'6PhProp'):
        creature_setup()
        emit('objecttype',kind,address=0x4000)
        emit('reapEnemies')
    creature_setup()
    for _ in range(400):
        emit(rng.choice(CREATURES),b'Alpha',value=rng.choice([-1,0,4,14,26,800,4096]),
             address=rng.choice([0,0,4096]),count=rng.choice([-1,0,4,26]),cb=rng.randrange(4))
    # Complete section 3.6; prior vectors remain a byte-identical prefix.
    def item_setup(players=3):
        emit('resetitems')
        emit('poke32', address=R.G_NUM_PLAYERS, value=players)
        emit('poke8', address=R.G_MEDAL_PLAYER, value=0)
        emit('poke32', address=R.G_SELECTED_OBJ, value=0x2000)
        emit('poke32', address=R.G_ALIVE_LIST, value=1)
        for addr in (0x2000, 0x4000, 0x6000):
            emit('nameadd', b'Alpha', address=addr)
            emit('poke32', address=addr+R.O_FLAGS, value=0x20)
            for j, bits in enumerate((0x7fffffff, 0x80000000, 0x3f800000)):
                emit('poke32', address=addr+R.O_POS+4*j, value=bits)
        for slot in (-2,-1,0,1,2,3):
            emit('poke32', address=g.rec(slot)+R.P_OBJECT, value=0x2000 if slot==0 else 0x4000)
            emit('poke32', address=g.rec(slot)+R.P_CAPACITY, value=0x41200000)
            emit('poke32', address=g.rec(slot)+R.P_WEIGHT, value=0x3f800000)
            for bag in range(3):
                start=(g.rec(slot)+R.P_BAGS+R.BAG_SIZE*bag)&0xffffffff
                emit('poke32', address=start, value=0xdeadbeef)
                emit('poke32', address=start+R.BAG_COUNT, value=2)
            for k in range(3): emit('poke32', address=g.rec(slot)+R.P_EQUIPPED+4*k, value=-1)
        for addr,val in ((0x1100,0x1200),(0x1104,0x3f800000),(0x1108,2),(0x110c,0x8000),
                         (0x1110,0),(0x1114,2),(0x1118,0x40000000),(0x111c,1),(0x1120,0x80000001)):
            emit('poke32', address=addr,value=val)
        emit('pokestr', b'Plain', address=R.G_LEVEL_NAME)
        emit('pokestr', b'Special', address=R.S_SPECIAL_LEVEL)
        for addr in R.S_SPECIAL_ITEMS.values(): emit('pokestr', b'Authored item', address=addr)
        emit('poke32', address=R.G_LEVEL_MONSTER_LEVEL, value=0x40a00000)

    for cb in range(4):
        for op in ITEMS:
            item_setup()
            for val in (-2,-1,0,1,2,3,4,4096):
                emit(op,b'Alpha',address=val if op in ('onePlayerHasItem','removePlayerItem','giveOnePlayerItem') else 0,
                     value=800 if op in ('onePlayerHasItem','removePlayerItem','giveOnePlayerItem','dropItem') else val,
                     count=val,cb=cb)
    # Object gate precedes string conversion; negative indices are unchecked.
    for slot in (-2,-1,0,2,3):
        item_setup()
        for pointer in (0,0xdeadbeef,0x2000):
            emit('poke32',address=g.rec(slot)+R.P_OBJECT,value=pointer)
            emit('playerobj',address=slot)
            for flags in (0,8,0x28,0xffffffff):
                emit('poke32',address=0x200c,value=flags)
                for op in ('onePlayerHasItem','giveOnePlayerItem'):
                    emit(op,b'Alpha',address=slot,value=4096,count=1)
    # Bag count captured once; first entry tested live after each removal.
    for initial in (-2,0,1,3):
        for pointer in (0,1):
            for cb in (0,2):
                item_setup()
                for bag in range(3):
                    start=g.rec(0)+R.P_BAGS+bag*R.BAG_SIZE
                    emit('poke32',address=start,value=pointer)
                    emit('poke32',address=start+R.BAG_COUNT,value=initial)
                emit('emptybag',address=0,value=0,cb=cb)
                emit('removeAllItems',cb=cb)
                emit('removeAllWeapons',cb=cb)
    for index in (-2147483648,-1,0,2147483647):
        item_setup()
        emit('poke32',address=0x1110,value=index)
        emit('poke32',address=0x1114,value=-2)
        emit('removePlayerItem',b'Alpha',address=-1,value=800)
    # All raw weight patterns, exact capacity equality, signed definition bag/index.
    for weight in (0,1,0x80000000,0x3f800000,0xbf800000,0x7f800000,0xffffffff):
        for capacity in (0,0x3f800000,0x40000000,0x7fffffff):
            item_setup(1)
            emit('poke32',address=0x1104,value=weight)
            emit('poke32',address=g.rec(0)+R.P_CAPACITY,value=capacity)
            for bag in (-128,-1,0,2,127):
                emit('poke8',address=0x121d,value=bag)
                emit('poke32',address=0x1108,value=-2 if bag<0 else 2)
                emit('givePlayerItem',b'Alpha',value=1,count=-1)
                emit('giveOnePlayerItem',b'Alpha',address=0,value=800,count=1)
    # Drop resolves position only after nonzero allocation and live pointer changes.
    for block in (0,0x8000):
        for pointer in (0,0xdeadbeef,0x2000):
            item_setup(1)
            emit('poke32',address=g.rec(0)+R.P_OBJECT,value=pointer)
            emit('poke32',address=g.rec(0)+R.P_CAPACITY,value=0)
            emit('poke32',address=0x110c,value=block)
            for cb in (0,1,2): emit('givePlayerItem',b'Alpha',value=1,count=0,cb=cb)
    # Medal filter, live player-count reads, failures at every give/drop call site.
    for player in (0,1,3,255):
        for mission in (0,1,-1):
            item_setup()
            emit('poke8',address=R.G_MEDAL_PLAYER,value=player)
            emit('poke32',address=R.G_MEDAL_MISSION,value=mission)
            emit('givePlayerItem',b'Alpha',value=1,count=2)
    for op in ('givePlayerItem','giveOnePlayerItem','givePlayerRandom','removeAllWeapons'):
        for fail_at in range(1,12):
            item_setup()
            emit('poke32',address=0x1008,value=fail_at)
            emit(op,b'Alpha',address=0,value=800 if op=='giveOnePlayerItem' else 1,count=1,cb=3)
    # Seed wraps; random record is generated in 1 or 10 attempts, with or without
    # special-level fixed items. Callback reads can change all subsequent gates.
    for players in (-1,0,1,3):
        for seed in (0,1,0xffffffff,0x80000000):
            item_setup(players)
            emit('poke32',address=R.G_RAND_SEED,value=seed)
            emit('randdraw')
            for difficulty in (-1,0,1,2,3,4):
                for fragment in (b'Special',b'',b'Plain'):
                    emit('pokestr',fragment,address=R.S_SPECIAL_LEVEL)
                    emit('poke32',address=R.G_DIFFICULTY,value=difficulty)
                    for success in (0,1):
                        emit('poke32',address=0x111c,value=success)
                        emit('givePlayerRandom')
    for bits in (0,1,0x80000000,0xbf800000,0x7fffffff,0xffffffff):
        item_setup(1)
        emit('poke32',address=0x1118,value=bits)
        emit('poke32',address=R.G_LEVEL_MONSTER_LEVEL,value=bits)
        emit('givePlayerRandom',cb=2)
    # Shared slots and long strings, both argument positions, packed/unpacked.
    for op in ('playerHasItem','onePlayerHasItem','countQuestItems','removePlayerItem','givePlayerItem','giveOnePlayerItem','dropItem','selectDropItem'):
        item_setup()
        for size in (0,159,160):
            emit(op,b'Z'*size,address=0,value=800 if op in ('onePlayerHasItem','removePlayerItem','giveOnePlayerItem','dropItem') else 1,count=0,pack=bool(size&1))
            emit(op,b'Stale',address=4096,value=4096,count=0)
            emit('forget')
            emit(op,b'Unknown',address=4096,value=4096,count=0)
        for old in ('setv','addQuest','showBust','cutScene'):
            emit(old,b'Alpha',value=800 if old=='addQuest' else 0)
            emit(op,b'Refused',address=4096,value=4096,count=0)
        emit('getNumPlayers')
        emit(op,b'Forgotten',address=4096,value=4096,count=0)
    for pointer in (0,0xdeadbeef,0x2000):
        item_setup()
        emit('poke32',address=R.G_SELECTED_OBJ,value=pointer)
        emit('selectDropItem',b'Item')
        emit('dropItem',b'Absent',value=800)
        emit('dropItem',b'Alpha',value=800,cb=2)

    # Complete section 3.8 family. Historical records remain an exact prefix.
    def effects_setup():
        emit('reseteffects')
        for addr, kind, bits in ((0x2000,b'13AutoContainer',0x10000020),
                                 (0x4000,b'10GameObject',0), (0x6000,b'7Trigger',0x10000000)):
            emit('nameadd',b'Alpha',address=addr)
            emit('objecttype',kind,address=addr)
            emit('poke32',address=addr+R.O_FLAGS,value=bits)
            for k,word in enumerate((0x40400000,0x40800000,0x7fffffff)):
                emit('poke32',address=addr+R.O_POS+4*k,value=word)
            emit('poke32',address=addr+0x178,value=0x80000001)
            emit('poke32',address=addr+0x124,value=0x12345670)
            emit('poke32',address=addr+0x58,value=0xff800001)
        emit('nameadd',b'Body',address=0x4000)
        emit('nameadd',b'Creature',address=0x2000)
        emit('objectprops',b'lump=Test|lump=Ignored',address=0x2000)
        emit('pokestr',b'LuMP',address=R.S_PROP_LUMP_KEY)
        emit('poke32',address=0x1004,value=0x12345678)
        emit('poke8',address=0x1100,value=2)
        emit('poke32',address=0x1104,value=0x6000)
        emit('poke32',address=0x1108,value=0x4000)
        emit('poke32',address=0x6068,value=-1)
        for k,value in enumerate((-2147483648,2147483647,-1)):
            emit('scriptcell',address=2000+4*k,value=value)

    for cb in range(4):
        for op in EFFECTS:
            effects_setup()
            name=b'Alpha';addr=0;value=1;count=12
            if op in ('startPropAnim','findDistance','particleMorph'): value=800
            if op=='particleMorph': name=b'Creature'
            if op=='lightEffect': addr=2000;value=-2147483648;count=48
            if op=='setParticle': addr=2;value=0x80000000;count=60
            if op=='addParticleOnObject': addr=-1;value=0;count=32
            emit(op,name,address=addr,value=value,count=count,cb=cb)
    # Missing targets, stale string addresses, overlong and packed strings.
    for op in ('trigger','autoPropSetState','autoPropGetState','startPropAnim','stopPropAnim',
               'propAnimAtEnd','deleteObjects','setNoCollide','objInPlayerTrig','findDistance',
               'setContainerProps','addExpandingPool','particleMorph','addParticleOnObject'):
        effects_setup()
        emit(op,b'Absent',value=800,count=12)
        emit(op,b'Alpha',value=800,count=12,pack=True)
        emit(op,b'Ignored',address=4096,value=4096,count=0)
        emit('forget')
        emit(op,b'Ignored',address=4096,value=4096,count=0)
        emit(op,b'Z'*160,value=800,count=12)
    # Two independently retained slots, including reuse from earlier handlers.
    effects_setup()
    emit('setMonsterRange',b'Alpha',value=3)
    emit('particleMorph',b'Creature',value=4096,count=8)
    emit('particleMorph',b'Ignored',address=4096,value=4096,count=8)
    emit('showBust',b'Alpha')
    emit('lightEffect',address=4096,value=3,count=0)
    emit('setv',b'Alpha',value=7)
    emit('stopPropAnim',address=4096,count=0)
    # Property-key writes precede resource faults; mode -1 clears before lookup.
    for props in (None,b'',b'Lump=Wrong',b'lump=|lump=Later',b'lump=First|lump=Second'):
        for mode in (-1,0,1):
            effects_setup()
            emit('objectprops',props or b'',value=int(props is None),address=0x2000)
            emit('startPropAnim',b'Alpha',value=800,count=mode)
            emit('startPropAnim',b'Alpha',value=800,count=mode,cb=3)
    # Allocation zero suppresses construction, and invalid particle indices refuse.
    for allocation in (0,0xffffffff):
        effects_setup();emit('poke32',address=0x1004,value=allocation)
        emit('lightEffect',address=2000,value=100,count=48)
        emit('poke32',address=0x400c,value=0x20)
        emit('particleMorph',b'Creature',value=800,count=8)
    for slot in (-2147483648,-2,-1,0,1,2,3,2147483647):
        effects_setup()
        emit('setParticle',address=slot,value=0x7fffffff,count=0)
        emit('addParticleOnObject',b'Alpha',address=slot,value=0,count=0)
    for countdown in (0,1,0xffffffff):
        effects_setup();emit('poke32',address=R.G_MEDAL_TICKS,value=countdown)
        for value in (0,-1,-2147483648,2147483647):
            emit('shakeCamera',address=value,count=0)
            emit('setFarClip',address=value,count=0)
            emit('setWaveDetail',address=value,count=0)
    # Trigger enumeration and early return, including live reads after a callback.
    for contains in (0,1,0xffffffff):
        effects_setup();emit('poke32',address=0x1004,value=contains)
        emit('objInPlayerTrig',b'Alpha')
        emit('poke32',address=0x6068,value=0)
        emit('objInPlayerTrig',b'Alpha',cb=2)
    # Horizontal distance uses software raw-float arithmetic, ignores z.
    for x,y in ((0,0),(0x40400000,0x40800000),(0x7fffffff,0x80000000),
                (1,0x80000001),(0x7f800000,0xff800000)):
        effects_setup()
        emit('poke32',address=0x4010,value=x);emit('poke32',address=0x4014,value=y)
        emit('findDistance',b'Alpha',value=800)
    effects_setup()
    for which in (-1,0,1,2):
        for value in (0,1,-1): emit('setContainerProps',b'Alpha',value=which,count=value)
    emit('autoPropGetState',b'Body')
    for bits in (0,0x80000001,0xffffffff):
        emit('poke32',address=0x200c,value=bits)
        emit('setNoCollide',b'Alpha',value=0);emit('setNoCollide',b'Alpha',value=-1)
    # Name-table truncation: callbacks must stop at 24/99 and container writes at 511.
    emit('reseteffects')
    for k in range(512):
        emit('nameadd',b'Crowd',address=0x2000+0x200*k)
        emit('objecttype',b'13AutoContainer',address=0x2000+0x200*k)
        emit('poke32',address=0x200c+0x200*k,value=0x10000000)
    for op in ('trigger','autoPropSetState','deleteObjects'):
        emit(op,b'Crowd',value=-1,count=12)
    emit('setContainerProps',b'Crowd',value=1,count=0)

    # Complete section 3.10: eight vehicle handlers and their shared helper.
    def vehicle_setup():
        emit('resetvehicles')
        emit('nameadd',b'Car',address=0x2000)
        emit('nameadd',b'Body',address=0x4000)
        emit('nameadd',b'Entity',address=0x6000)
        emit('poke32',address=R.G_NUM_PLAYERS,value=3)
        emit('poke32',address=R.G_ACTIVE_PLAYER,value=1)
        emit('poke32',address=R.G_ALIVE_COUNT,value=3)
        for k in range(3):
            player=0x8000+0x1000*k
            emit('nameadd',b'Player',address=player)
            emit('poke32',address=g.rec(k)+R.P_OBJECT,value=player)
            emit('poke32',address=player+R.PLAYER_VEHICLE,value=0x2000)
            emit('poke32',address=R.G_ALIVE_LIST+4*k,value=k)
        for offset,word in ((0x348,0x40400000),(0x34c,0x3f800000),(0x90c,0x40000000),
                            (0x910,0x1100),(0x468,0xbf800000)):
            emit('poke32',address=0x2000+offset,value=word)
        emit('poke32',address=0x110c,value=0x3f800000)
        for k,word in enumerate((0x80000000,0x7fffffff,0xff800001,0xfffffff0)):
            emit('poke32',address=0x1200+4*k,value=word)

    for cb in range(4):
        for op in VEHICLES:
            vehicle_setup()
            emit(op,b'Entity' if op=='entityRideVehicle' else b'Car',value=800 if op in ('entityRideVehicle','putNPCInVehicle') else -1,count=24,cb=cb)
    for op in ('putNPCInVehicle','putPlayersInVehicle','entityRideVehicle','controlVehicle','holdVehicle','getVPosition'):
        vehicle_setup()
        emit(op,b'Missing',value=800,count=24)
        emit(op,b'Car',address=999999,value=999999,count=0)
        emit('forget');emit(op,b'Car',address=999999,value=800,count=0)
        for pack in (False,True):
            emit(op,b'Car',value=800,count=0,pack=pack)
            emit(op,b'x'*159,value=800,count=0,pack=pack)
            emit(op,b'x'*160,value=800,count=0,pack=pack)
    vehicle_setup()
    # Active player first even dead; other players skip dead, refuse null/stale.
    for active in (-1,0,1,3,-2):
        emit('poke32',address=R.G_ACTIVE_PLAYER,value=active)
        for flags in (0,R.OF_DEAD,0xffffffff):
            emit('poke32',address=0x8000+R.O_FLAGS,value=flags)
            emit('putPlayersInVehicle',b'Car',count=0)
    for ptr in (0,0xdead,0x8000):
        emit('poke32',address=g.rec(0)+R.P_OBJECT,value=ptr)
        emit('putPlayersInVehicle',b'Car',count=0)
        emit('vehicleof',address=0)
        emit('getPlayerVPosition',count=0);emit('getPlayerVDir',count=0)
    vehicle_setup()
    for slot in (-1,0,1,3,2147483647): emit('vehicleof',address=slot)
    # Position selection compares +90c against the previously kept +34c.
    emit('nameadd',b'OtherCar',address=0x5000)
    emit('poke32',address=0x9000+R.PLAYER_VEHICLE,value=0x5000)
    for best in (0,0x80000000,0x3f800000,0x7fffffff,0xffffffff):
        emit('poke32',address=0x2000+0x34c,value=best)
        emit('poke32',address=0x5000+0x34c,value=0x40800000)
        for raw in (0,0x80000000,0x3f000000,0x40000000,0x7fffffff,0xff800001):
            emit('poke32',address=0x5000+0x90c,value=raw)
            emit('getPlayerVPosition',count=0);emit('getVPosition',b'OtherCar',count=0)
    # Direction strict threshold, sign, cancellation, raw extremes and path zero.
    for threshold in (0,0x40000000,0x40000001,0x7fffffff,0xffffffff):
        emit('poke32',address=0x2000+0x348,value=threshold)
        for raw in (0,0x80000000,0x3f800000,0xbf800000,0x7fffffff,0xffffffff):
            emit('poke32',address=0x2000+0x468,value=raw)
            emit('getPlayerVDir',count=0)
    emit('poke32',address=0x2000+0x910,value=0)
    emit('getPlayerVDir',count=0)
    for count in (-1,0,1,3):
        emit('poke32',address=R.G_NUM_PLAYERS,value=count)
        emit('poke32',address=R.G_ALIVE_COUNT,value=count)
        emit('getPlayerVPosition',count=0);emit('getPlayerVDir',count=0)

    # Complete remaining section 3.5 player family (nine handlers, four helpers).
    def interaction_setup():
        emit('resetinteractions')
        emit('poke32',address=R.G_NUM_PLAYERS,value=3)
        emit('poke32',address=R.G_ALIVE_COUNT,value=3)
        emit('nameadd',b'Centre',address=0x2000)
        for slot in range(3):
            addr=0x3000+0x1000*slot
            emit('nameadd',b'Player',address=addr)
            emit('poke32',address=g.rec(slot)+R.P_OBJECT,value=addr)
            emit('poke32',address=R.G_ALIVE_LIST+4*slot,value=slot)
            emit('poke32',address=g.rec(slot)+R.P_HP,value=0x3f800000)
            emit('poke32',address=g.rec(slot)+R.P_MAX_HP,value=0x42c80000)
            emit('poke32',address=addr+R.O_POS,value=(0x40400000,0xc0400000,0)[slot])
            emit('poke32',address=addr+R.O_POS+4,value=0x40800000)
        emit('poke32',address=0x1004,value=0x100000)
        emit('poke32',address=R.SKILL_PTR_HORN,value=0xfedcba98)

    for count in (-1,0,1,2,3):
        for pointer,flags in ((0,0),(0x3000,0),(0x3000,R.OF_DEAD)):
            interaction_setup()
            emit('poke32',address=R.G_NUM_PLAYERS,value=count)
            emit('poke32',address=g.rec(0)+R.P_OBJECT,value=pointer)
            emit('poke32',address=0x300c,value=flags)
            emit('setBestActivePlayer',count=0)
            for slot in (-3,-1,0,1,2,4,2147483647):
                emit('poke32',address=R.G_ACTIVE_PLAYER,value=slot)
                emit('playGotItemSound',count=0)
            emit('explodePotionsOnly',count=0)
    for op in ('hornOmen','planarSteel'):
        for cb in (0,1,2,3):
            for allocation in (0,0x100000):
                interaction_setup()
                emit('poke32',address=0x1004,value=allocation)
                emit(op,count=0,cb=cb)
        interaction_setup()
        emit('poke32',address=g.rec(0)+R.P_OBJECT,value=0)
        emit('poke32',address=g.rec(1)+R.P_HP,value=0)
        emit('poke32',address=g.rec(2)+R.P_HP,value=0xbf800000)
        emit(op,count=0)
    for op in ('hitPlayers','closestPlayerDist'):
        for cb in (0,1,2,3):
            interaction_setup()
            emit(op,b'Centre',value=10,count=24,cb=cb)
        for packed in (False,True):
            interaction_setup()
            for name in (b'Centre',b'Missing',b'x'*159,b'x'*160):
                emit(op,name,value=10,count=24,pack=packed)
            emit(op,b'Centre',value=10,count=24)
            emit(op,b'Changed',address=0x7fffffff,value=10,count=24)
            emit('explodePotionsOnly',count=0)
            emit(op,b'Centre',address=0x7fffffff,value=10,count=24)
        for count in (-1,0,1,3):
            interaction_setup()
            emit('poke32',address=R.G_ALIVE_COUNT,value=count)
            emit(op,b'Missing',value=10,count=24)
            emit('poke32',address=g.rec(0)+R.P_OBJECT,value=0)
            emit(op,b'Centre',value=10,count=24)
        interaction_setup()
        for raw in (0,0x80000000,0x3f800000,0xbf800000,0x00800000,0x00000001,0x7f7fffff,0x7fffffff,0xffffffff):
            for component in range(3):
                emit('poke32',address=0x3010+4*component,value=raw)
                for radius in (-1,0,5,6,2147483647,-2147483648):
                    emit(op,b'Centre',value=radius,count=24)
    # Sparse authored record bytes across the whole 0x4c30 copy, plus every kept range.
    for slot in (0,1,3,255):
        for cb in (0,1,2,3):
            interaction_setup()
            emit('poke8',address=R.G_MEDAL_PLAYER,value=slot)
            rec=g.rec(slot)
            for offset in list(range(0,R.REC_SIZE,241))+list(range(0x78,0x90))+list(range(0x4c0d,0x4c1a))+[0x43d8,0x43d9,0x43da,0x43db,R.REC_SIZE-1]:
                emit('poke8',address=rec+offset,value=(offset*17+3)&255)
            emit('savePlayer',count=0,cb=cb)
            for offset in (0,0x77,0x78,0x8f,0x90,0x43d8,0x4c0c,0x4c0d,0x4c14,0x4c15,0x4c18,0x4c19,0x4c1a,R.REC_SIZE-1):
                emit('poke8',address=rec+offset,value=0xa5)
            emit('restorePlayer',count=0,cb=cb)
    # Copy overlaps, address wrapping and zero copy identities are legitimate owned bytes.
    for copy in (0,0xfffffff0,g.rec(0),g.rec(0)+1,g.rec(0)-1):
        interaction_setup()
        emit('poke32',address=0x1004,value=copy)
        emit('poke32',address=g.rec(0),value=0x12345678)
        emit('savePlayer',count=0)
        emit('restorePlayer',count=0)

    # Complete scene-utility/audio family, appended after the immutable old corpus.
    emit('resetutilities')
    for op in ('printVal','demoRunning','getRand','endMusic'):
        emit(op,count=0)
    for value in list(range(-65,66)) + [-2147483648,-2147483647,2147483647,16777215,16777216,16777217]:
        emit('getScaledTime',address=value,count=0)
    for angle in list(range(0,65536,257)) + [-1,-65536,65536,2147483647,-2147483648,16383,16384,32767,32768,49151,49152]:
        for op in ('sin1000','cos1000'): emit(op,address=angle,count=0)
    for addr,op in ((R.G_LEVEL_TIMER,'getLevelTimer'),(R.G_LEVEL_LOADING,'levelLoading'),(R.G_NETWORK_MODE,'networkMode')):
        for value in (0,1,59,60,61,127,128,255,256,16777215,16777217,0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff):
            emit('poke32',address=addr,value=value)
            emit(op,count=0)
    for seed in (0,1,0xffffffff,0x80000000,0x12345678):
        emit('poke32',address=R.G_RAND_SEED,value=seed)
        for _ in range(20): emit('getRand',count=0)
    for addr in (R.G_PAUSED,R.G_JUMPGATE_TRAVEL,R.G_CUTSCENE_ACTIVE):
        for a in (R.G_PAUSED,R.G_JUMPGATE_TRAVEL,R.G_CUTSCENE_ACTIVE): emit('poke32',address=a,value=0)
        for value in (0,1,-1,128,256,-2147483648):
            emit('poke32',address=addr,value=value)
            emit('gamePaused',count=0)
    for op in ('print','startMusic','startAudio','sound','soundSequence'):
        for packed in (False,True):
            emit('resetutilities')
            for name in (b'',b'Synthetic',b'x'*159,b'x'*160):
                emit(op,name,value=800,count=8,pack=packed)
            emit(op,b'Synthetic',value=800,count=8,pack=packed)
            emit(op,b'Changed',value=0x7fffffff,address=0x7fffffff,count=8,pack=packed)
            emit('printVal',count=0)
            emit(op,b'Changed',value=0x7fffffff,address=0x7fffffff,count=8,pack=packed)
        for cb in (0,1,2,3):
            for result in (0,0xffffffff,0x87654321):
                emit('resetutilities')
                emit('poke32',address=0x1004,value=result)
                emit(op,b'Synthetic',value=800,count=8,cb=cb)
    # Slots shared with already ported handlers, including a refused second pointer.
    emit('resetutilities')
    emit('addQuest',b'Synthetic',value=800,count=8)
    emit('sound',b'Changed',address=0x7fffffff,value=0x7fffffff,count=8)
    emit('getv',b'Synthetic',count=4)
    emit('startMusic',b'Changed',address=0x7fffffff,count=4)
    for cb in (0,1,2,3): emit('endMusic',count=0,cb=cb)

    # Script-control family: actual nested AMX runs, then deferred/post-action state.
    emit('resetcontrol')
    emit('poke32', address=R.G_DRAW_FRAME, value=100)
    emit('runscript', b'leaf')                 # no machine, no throttle write
    emit('runscript', b'leaf', pack=True)      # null name
    emit('controlmachine')                    # load unbound, register via set_script
    for stamp, frame in ((0,0),(0,1),(21,20),(21,21),(21,22),(-1,0),(0x7ffffffe,0x7fffffff),(0x80000000,0x80000001)):
        emit('poke32',address=R.G_MISSING_STAMP,value=stamp)
        emit('poke32',address=R.G_DRAW_FRAME,value=frame)
        for name in (b'absent',b'Leaf',b'',b'leaf\0suffix'):
            emit('runscript',name)
    for pri in (0,1,-1,-2147483648,2147483647):
        emit('entrypri',value=pri)
        for name in (b'leaf',b'initial',b'error',b'halt',b'nested',b'missing',b'reuse'):
            emit('getv',b'Outer',count=0)
            emit('runscript',name)
    emit('selectscript',value=0)
    emit('runscript',b'leaf')
    emit('selectscript',value=1)
    for op in SCRIPT_CONTROL:
        for packed in (False,True):
            emit('resetcontrol');emit('controlmachine')
            for name in (b'leaf',b'',b'absent',b'x'*79,b'x'*80,b'x'*159,b'x'*160):
                emit(op,name,pack=packed,count=0)
            emit(op,b'leaf',pack=packed,count=4)
            emit(op,b'changed',address=0x7fffffff,count=4)
            emit('getNumPlayers',count=0)
            emit(op,b'changed',address=0x7fffffff,count=4)
    # Capacity, double buffering, queueing during a run and persistence while paused.
    emit('resetcontrol');emit('controlmachine')
    emit('poke32',address=R.G_PAUSED,value=1)
    for _ in range(66): emit('queuedeferred',b'queue')
    for _ in range(4): emit('rundeferred')
    emit('runpost');emit('runpost')
    emit('setpost',b'queue');emit('runpost');emit('runpost')
    emit('setpost',b'leaf');emit('setpost',b'absent');emit('runpost')
    # Negative counts are unchecked, and all flip bits survive until wrap.
    for queued in (-2,-1,0,63,64,65,2147483647):
        for flip in (0,1,0xfffffffe,0xffffffff):
            emit('resetcontrol');emit('controlmachine')
            emit('poke32',address=R.G_DEFER_QUEUED,value=queued)
            emit('poke32',address=R.G_DEFER_FLIP,value=flip)
            emit('queuedeferred',b'leaf')
            emit('rundeferred')
            emit('cleardeferred')
    for count in (-2147483648,-1,0,1,2):
        emit('resetcontrol');emit('controlmachine')
        emit('poke32',address=R.G_DEFER_COUNT,value=count)
        emit('pokestr',address=R.G_DEFER_NAMES,name=b'queue')
        emit('pokestr',address=R.G_DEFER_NAMES+80,name=b'leaf')
        emit('rundeferred');emit('rundeferred')
    # Direct helpers have no invented 80/160-byte buffer cap or slot invalidation.
    for length in (0,1,79,80,81,159,160,240):
        emit('resetcontrol');emit('controlmachine')
        emit('getv',b'Keep',count=0)
        emit('queuedeferred',b'x'*length)
        emit('setpost',b'y'*length)
        emit('rundeferred');emit('rundeferred');emit('runpost')
        emit('resetstate')
    # Host failure prefixes; live count/flip mutation and current-machine replacement.
    for cb in (0,1,2,3):
        for op in ('runscript','runpost','rundeferred','callScript'):
            emit('resetcontrol');emit('controlmachine')
            emit('getv',b'Keep',count=0)
            emit('setpost',b'profile')
            emit('poke32',address=R.G_DEFER_COUNT,value=2)
            emit('pokestr',address=R.G_DEFER_NAMES,name=b'profile')
            emit('pokestr',address=R.G_DEFER_NAMES+80,name=b'leaf')
            emit(op,b'profile',count=0,cb=cb)
            emit('runpost');emit('rundeferred')
    rng_control = random.Random(0x143158)
    emit('resetcontrol');emit('controlmachine')
    for _ in range(350):
        op=rng_control.choice(('queuedeferred','rundeferred','setpost','runpost','cleardeferred','resetstate','runscript','callScript','addDeferedCall','postAction'))
        emit(op,rng_control.choice((b'leaf',b'queue',b'nested',b'error',b'halt',b'absent',b'reuse',b'')),count=0)

    # All memory/global reference units; independent synthetic inputs only.
    emit('reset')
    for addr in (0, 4093, 4095, 4096, 0xfffffffd, 0xffffffff):
        for size in (0,1,2,4,8,9,17,33):
            for negative in (False,True):
                emit('memstore',bytes(range(1,38)),pack=negative,address=addr,count=size)
                emit('memload',address=addr,count=size)
                emit('memread',address=addr,count=size)
    for kind in ('u8','u16','u32','u64','s8','s16','s32','w8','w16','w32','w64'):
        for value in (0,127,128,255,32767,32768,65535,-1,-2147483648,2147483647):
            emit('memstore',bytes([255])*8,address=4093,count=8)
            emit('memtyped',kind.encode(),address=4093,value=value)
    for kind in ('u8','s8','u16','s16','u32','s32'):
        for addr in (4095,0xffffffff):
            for value in (0,127,128,255,32768,-1,-2147483648,2147483647):
                emit('globalset',kind.encode(),address=addr,value=value)
                emit('globalget',kind.encode(),address=addr)
    for length in (0,1,15,16,17):
        emit('reset');emit('pokestr',b'x'*length,address=4090)
        for limit in (0,1,16,17,18): emit('memcstr',address=4090,count=limit)
    for cb in (0,1):
        emit('reset');emit('membacking',value=3,cb=cb)
        emit('memread',address=4093,count=12)
        emit('memread',address=4093,count=12)
        emit('memwrite',b'abc',address=8191)
        emit('memload',address=8190,count=8)
        emit('mempage',address=1,value=4095,count=99)
        emit('memread',address=8191,count=1)
    for length in (0,3,4090):
        emit('reset');emit('membacking',value=3,pack=True,count=length)
        emit('memread',address=4093,count=12)
        emit('memtyped',b'u32',address=4095)
        emit('memwrite',b'abc',address=4095)
        emit('memread',address=0,count=16)
    emit('reset');emit('memwrite',b'',address=0xffffffff)
    emit('memread',address=0xffffffff,count=0)

    # Complete Obj property/accessor and X metadata family. Constructor itself
    # writes nothing; each view reads live memory, including overlapping views.
    emit('reset')
    for addr in (0,4093,0xfffffff0,0xffffffff):
        for typename in (b'',b'8Creature',bytes(range(256))):
            emit('objmeminit',typename,address=addr)
        for negative in (False,True):
            magnitude=bytes(range(1,38))
            emit('objmemsetflags',magnitude,address=addr,pack=negative)
            emit('objmemflags',address=addr)
            emit('objmemsetangle',magnitude,address=addr,pack=negative)
            emit('objmemangle',address=addr)
            emit('objmemsetcflags',magnitude,address=addr,pack=negative)
            emit('objmemcflags',address=addr)
            for components in (0,1,2,3,4):
                emit('objmemsetpos',bytes(range(1,69)),address=addr,count=components,pack=negative)
                emit('objmempos',address=addr)
            for width in (0,1,2,4,8,9,17,33):
                for offset in (-169,-1,0,12,16,56,168,2147483647):
                    emit('objmemw',magnitude,address=addr,value=offset,count=width,pack=negative)
                    emit('objmemu',address=addr,value=offset,count=width)
        # Absent memory: properties return zero and setters ignore even short
        # inputs. Generic u/w/cflags require memory and are checked in API tests.
        for op in ('objmemflags','objmempos','objmemangle','objmemsetflags','objmemsetpos','objmemsetangle'):
            emit(op,b'\xff'*17,address=addr,count=0,cb=2)
    # Backing exceptions occur between position components and retain prefixes.
    emit('reset');emit('membacking',cb=1)
    emit('objmemsetpos',bytes(range(1,52)),address=4080,count=3)
    emit('reset');emit('membacking',pack=True,count=3)
    emit('objmemsetcflags',b'\xff'*17,address=4095)
    emit('objmemcflags',address=4095)
    emit('descriptor',address=0,pack=True)
    for addr in (0,0xffffffff):
        for floats,stack in ((0,0),(8,5),(-1,-2),(2147483647,-2147483648)):
            emit('descriptor',b'ivxsT|P|authored metadata',address=addr,value=floats,count=stack)
    rng_obj=random.Random(0x403370)
    emit('reset')
    for _ in range(200):
        addr=rng_obj.choice((0,4093,0xfffffff0))
        emit('objmemw',bytes(rng_obj.randrange(256) for _ in range(20)),address=addr,value=rng_obj.choice((12,16,20,24,56,168)),count=rng_obj.choice((2,4,8,17)),pack=bool(rng_obj.randrange(2)))
        for op in ('objmemflags','objmempos','objmemangle','objmemcflags'): emit(op,address=addr)

    # All twelve Host units. The fake World records the explicit delegation
    # boundary, preserving argument order, duplicates, return shape and failures.
    emit('reset')
    for label in R.EXT:
        emit('hostdefault',label.encode())
        for cb in (1,2,4): emit('hostcall',label.encode(),address=-1,value=17,count=-3,cb=cb)
        if not hasattr(R.Host,'do_'+label):
            emit('hostcall',label.encode(),cb=0)
            emit('hostcall',label.encode(),cb=3)
    for slot in R.VIRTUAL:
        emit('hostdefault',b'virtual',value=slot,address=0x2000,count=3)
        for cb in (0,1,2,3,4,5): emit('hostcall',b'virtual',value=slot,address=0x2000,count=3,cb=cb)
        emit('hostdirect',b'virtual',value=slot,address=0x4000,count=-1)
    for label in ('obj_moveOffGrid','obj_updateGridCell','obj_runEveryFrame',
                  'world_collectObjectsInBox','world_forEachGridObject',
                  'world_findObjectsByClass','game_rebuildAlivePlayers'):
        for count in (-2147483648,-1,0,1,2,2147483647):
            for op in ('hostcall','hostdirect'):
                for cb in (0,3,5): emit(op,label.encode(),address=0x2000,value=-1,count=count,cb=cb)
    for addr in (0x2000,0x4000): emit('hostobject',address=addr)

    # Full direct argument adapter and binder family, all slot metadata.
    emit('reset')
    for name in sorted(set(R.STRING_SLOT) | {'unknown'}):
        for k in (-1,0,1,2):
            emit('bindargs', name.encode(), address=0, count=k)
            emit('bindargs', name.encode(), address=0x7fffffff, count=k)
    for op in ('argread','argu','argf','argaddress','args'):
        for name in (b'getv',b'particleMorph',b'unknown'):
            for k in (-1,0,1,2,8):
                for cell in (0, -1, 4095, 4096, 0x7fffffff, -2147483648, 16777217):
                    for cb in (0,1,2):
                        emit(op,name,address=cell,count=k,value=160,cb=cb)
    for room in (-1,0,1,13,14,15,160):
        for packed in (False,True):
            emit('args',b'getv',address=0,count=0,value=room,pack=packed)
    for name in (b'getv',b'particleMorph',b'unknown'):
        emit('bindargs',name,address=0,count=0,cb=3)
        emit('missing',name,address=0,count=0)
    emit('reset')
    emit('bindtable',b'getv',address=0,count=0)
    emit('bindtable',b'getNumPlayers',address=0,count=0)

    # Append-only setup family: historical rows stay identical.
    emit('resetcreatures')
    for index in range(150):
        emit('setupobject', b'Setup' + str(index % 5).encode(), value=(0,2,8,0x800)[index % 4],
             address=(0,0x42c80000,0xc2c80000)[index % 3], count=index % 17, pack=index % 2 == 0)
    for index in range(8):
        emit('setupobject', b'Explicit', address=0x3000000 + index * 0x1000,
             value=8 if index % 2 else 0, count=65537, cb=1, pack=True)
        for count in (-1,0,1,4,5):
            emit('setupalive', value=count, address=0x3000000 + index * 0x1000)
        emit('setupvirtual', b'payload', value=index, address=0x3000000+index*0x1000,count=14)
    emit('setupalive',value=4,address=0xdeadbeef)
    for name in (b'dialog_isOpen',b'game_rebuildAlivePlayers'):
        emit('setupext',name,value=17,address=123,count=-1)
    emit('reset')
    emit('setupvars')
    for index in range(60):
        emit('set', b'Quest' + str(index).encode(), value=index-30)
        emit('setupvars')

    # The final family is append-only, including canonical arbitrary-width results.
    emit('reset')
    def division(a, b, padding=0, signed_zero=False):
        def encode(v):
            magnitude = abs(v).to_bytes((abs(v).bit_length() + 7) // 8, 'little') + bytes(padding)
            return (str(int(v < 0 or (v == 0 and signed_zero))) + ':' + text(magnitude)).encode()
        emit('tdiv', encode(a) + b'/' + encode(b))
    edges = [0, 1, 2, 3, 255, 256, 257, (1 << 31) - 1, 1 << 31,
             (1 << 63) - 1, 1 << 63, (1 << 64) + 1, (1 << 127) - 1,
             1 << 256, (1 << 1024) - 1]
    for a in edges:
        for b in edges:
            for sa, sb in ((1,1),(-1,1),(1,-1),(-1,-1)):
                division(sa*a, sb*b)
    rng = random.Random(0x54444956)
    for i in range(600):
        a = rng.getrandbits(rng.choice((8,32,64,128,256,1024,4096)))
        b = rng.getrandbits(rng.choice((1,8,32,64,128,256,1024,4096)))
        division(-a if i & 1 else a, -b if i & 2 else b, i % 4, i % 3 == 0)
    for padding in (0,1,17):
        division(0, 1, padding, True)
        division(1, 0, padding, True)
        division(-1, 2, padding)

    output = ('# rta-script-builtins-variables-v1\n# op\tinputs\tresult\tcomplete_state\n' + '\n'.join(rows) + '\n').encode('ascii')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('wb') as stream:
        with gzip.GzipFile(filename='', fileobj=stream, mode='wb', mtime=0) as compressed:
            compressed.write(output)
    tree = ast.parse((args.research_dir / 'tools/ref_natives.py').read_text())
    units = []
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name != '_main':
            units.append(node.name)
        elif isinstance(node, ast.ClassDef):
            units.extend(node.name + '.' + method.name + ('@setter' if any(
                             isinstance(d, ast.Attribute) and d.attr == 'setter'
                             for d in method.decorator_list) else '') for method in node.body
                         if isinstance(method, ast.FunctionDef))
    ported = PORTED_HELPERS + ['Game.n_' + name for name in NATIVES + CAMPAIGN + PLAYERS] + SCENE_UNITS + MENU_UNITS + CREATURE_UNITS + ITEM_UNITS + EFFECT_UNITS + VEHICLE_UNITS + INTERACTION_UNITS + ['Game.n_' + name for name in UTILITIES] + SCRIPT_UNITS + MEMORY_UNITS + OBJECT_UNITS + HOST_UNITS + ARGUMENT_UNITS + SETUP_UNITS + DIVISION_UNITS
    assert len(ported) == len(set(ported)) and set(ported) <= set(units)
    manifest = {
        'schema': 'rta-script-builtins-variables-v1', 'rows': len(rows),
        'reference_units': units, 'ported_units': ported,
        'progress': {'ported': len(ported), 'total': len(units)},
        'operations': counts, 'errors': errors, 'reference_commit': COMMIT,
        'reference_files': PINS, 'vectors_sha256': hashlib.sha256(output).hexdigest(),
        'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'scope': '191 RTA table names and all 324 reference units including arbitrary-width signed truncating division, state construction/setup, direct argument access/native binding, complete Host dispatch and explicit object-world delegates, memory-backed objects and call descriptors, complete sparse memory and global descriptors: variable tables, campaign state, player records, scene lifecycle and complete menus/HUD/medal results and creatures, items, objects/effects/camera and vehicles, plus complete player interactions/spells/snapshots and scene utilities/audio and complete script execution/deferred/post-action lifecycle. Synthetic complete memory/write/slot/AMX traces; exact owned property/name-table projections and explicit save/reward/local-player/resource/player/presentation service callbacks.',
        'unsupported_natives': [name for name in R.NATIVE_NAMES if name not in NATIVES + CAMPAIGN + PLAYERS + SCENE + MENUS + CREATURES + ITEMS + EFFECTS + VEHICLES + INTERACTIONS + UTILITIES + SCRIPT_CONTROL],
    }
    args.output.with_suffix('').with_suffix('.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(f'{len(rows)} exact state transitions, {len(counts)} operations; {errors}')


if __name__ == '__main__':
    main()
