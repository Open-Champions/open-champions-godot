"""Immutable reference revisions and fixture identities for simulation gates."""
import hashlib
import json
ANIMATION_CLOCK_REFERENCE = "b95d93863682476a7fdcddf86c0428b6fc36feb546c9fe1df835f7e07cb1aded"
ANIMATION_CLOCK_REFERENCES = {
    "anim": {"commit": "f516ac798bf28375554fe53ffb31b74802844136",
             "path": "tools/ref_anim.py",
             "sha256": "6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8"},
    "core": {"commit": "41e847af5360724ab62d208594f8027ea3c1b686",
             "path": "tools/ref_core.py",
             "sha256": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123"},
}
ANIMATION_CLOCK_SPECIFICATION = {
    "commit": "f2b996dded898f1b0b558b901cab875f775cfdc7",
    "path": "docs/spec/animation.md",
    "sha256": "be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab",
}
ANIMATION_CLOCK_GENERATOR = "8863600a495f0d104285b6029b6beb649d4de00604774fc469122f9011986cf4"
ANIMATION_DECODER_REFERENCE = ANIMATION_CLOCK_REFERENCE
ANIMATION_DECODER_REFERENCES = ANIMATION_CLOCK_REFERENCES
ANIMATION_DECODER_SPECIFICATION = ANIMATION_CLOCK_SPECIFICATION
ANIMATION_DECODER_GENERATOR = "1743257003d202363cb31344620b789e54a968cf82d06512c718475502b37301"
ANIMATION_DECODER_PROTOCOL = {
    "clip": "[id,nameBytes,dataBytes,joint_count,length,bind,depth,events4,sound_times_orNone,sound_group,joint_mask,words]",
    "columns": [
        "operation",
        "config",
        "id;setup-and-command-DSL",
        "status",
        "observation"
    ],
    "commands": "0 add_clip(nameBytes,fileBytes);1 alloc;2 touch(slot);3 decode(track);4 pos(slot,joint,time);5 quat(slot,joint,time);6 decode_clip(clip,time,flags);7 set_track(track,field,value);8 copy_track(dst,src);9 set_event(clip,index,seconds,mask);10 word(clip,index)",
    "identity": "Clip/track/slot IDs are 1-based, 0=null; slot.index is 0-based. decode_clip retains a newly constructed track even on failure.",
    "joint": "[PT,RT,PX,PY,PZ,VX,VY,VZ,QX,QY,QZ,QW,RX,RY,RZ,RW]",
    "limits": "[input_bytes,owned_bytes,clips,tracks,slots,key_records]",
    "observation": "[initial_snapshot,submitted_commands,[[result_or_error_name,complete_snapshot],...]]",
    "setup": "[system2,limits6,clips[[nameBytes,fileBytes]],tracks9,slots7,LRU,commands]",
    "slot": "[index0,clipID,cursor,tick,ownerTrackID,yaw,joints64x16]",
    "snapshot": "[system2,limits6,clips12,tracks[[id,track9]],slots7,LRU]",
    "system": "[sim_frame,cosmetic_seed]",
    "track": "[clipID,slotID,weight,rate,life,life_rate,time,flags,sound_idx]",
    "version": 1
}
ANIMATION_RUNTIME_REFERENCE = ANIMATION_CLOCK_REFERENCE
ANIMATION_RUNTIME_REFERENCES = ANIMATION_CLOCK_REFERENCES
ANIMATION_RUNTIME_SPECIFICATION = ANIMATION_CLOCK_SPECIFICATION
ANIMATION_RUNTIME_GENERATOR = '3f05cc05042a55f7a37714bd22f0180f45ce62eb3c7d681737ff852836df31cb'
ANIMATION_RUNTIME_PROTOCOL = {'clock_commands': '0 clear(s);1 push(s,c,f);2 weighted_push(s,c,f,w,r,l,lr);3 chained(s,c,f);4 '
                   'start(s,c,f,blend);5 start_blend(s,c,f,in,out);6 stop(s,c,blend);7 drop(s);8 '
                   'release(s);9 dtor(s);10 sync_frame(s);11 randomize(s);12 sync_below(s);13 '
                   'jump(s);14 snap(s);15 find(s,c);16 find_flag(s,mask);17 fade_in(s,c,blend);18 '
                   'fade_out(s,c,blend);19 weight(s,c,value);20 copy(s);21 advance(s,speed)',
 'delta': '[indexPath,replacement] at unequal scalar/type/list length; equal-length lists '
          'recursively visited in ascending index order; integer arrays represent bytes.',
 'domain': 'NonROOT/nonYAW across ALL cells,pose0,sound/root/yaw outputs disabled; full raw32 '
           'flags otherwise retained. No mirrors, repaired owners or native expectation input.',
 'identity': 'Clips then standalone tracks then states/capacity cells allocated in input order. '
             'One canonical Track registry, IDs1based/null0; slot.index0based. Copy adds a new '
             'state and fresh cells, dtor retains IDs, decode_clip retains its temporary even on '
             'failure.',
 'limits': '[decoder6,states,state_cells,max_loop_subtractions]',
 'observation': '[input,initial_snapshot,[[result_or_original_error_name,recursive_list_delta],...]]',
 'other_commands': '22 add_state(capacity);23 add_clip(nameBytes,fileBytes);24 '
                   'add_track(track9);25 set_track(id,field,value);26 '
                   'set_state(id,pose0,count,flags);27 event(clip,index,seconds,mask);28 '
                   'system(field0frame_or1cosmetic,value);29 alloc;30 touch(slot);31 '
                   'decode(track);32 pos(slot,joint,time);33 quat(slot,joint,time);34 '
                   'decode_clip(clip,time,flags);35 copy_track(dst,src);36 word(clip,index);37 '
                   'set_word(clip,index,u16)',
 'results': 'push/start/find None becomes0 ID; other voidNone; '
            'copy/add_state/newclip/newtrack/alloc/decode returnID; advance signedbyte; finite '
            'original errors classnames preserve prefix and continue. Native host-policy refusals '
            'are separate.',
 'setup': '[system2,limits4,clips[[nameBytes,fileBytes]],standaloneTracks9,states[[pose0,count,capacity,rawflags,capacityTrackRows9]],slots7,LRU,commands]',
 'slot': '[index0,clipID,cursor,tick,ownerTrackID,yaw,joints64x16]',
 'snapshot': '[limits4,states[[id,[pose0,count,capacity,rawflags,orderedTrackIDs]]],decoderSnapshot[system2,limits6,clips12,tracks[[id,track9]],slots7,LRU]]',
 'track': '[clipID,slotID,weight,rate,life,life_rate,time,raw32flags,sound_idx]',
 'version': 1}
WORLD_GEOMETRY_REFERENCE = "2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5"
WORLD_GEOMETRY_REFERENCES = {"world_info": {
    "commit": "2f937fca823488bc97b9680fde9d957c3fa9907a",
    "path": "tools/world_info.py", "sha256": WORLD_GEOMETRY_REFERENCE}}
WORLD_GEOMETRY_SPECIFICATION = {
    "commit": "abf07ea411ac78e4cacb1da3e85b388a1839928a",
    "path": "docs/spec/render-pipeline.md",
    "sha256": "3eb890d80325c7b04a584de04af8360fc8f4c0e412d3a21656b4e99db570a04b"}
WORLD_GEOMETRY_FORMAT = {
    "commit": "f2b996dded898f1b0b558b901cab875f775cfdc7",
    "path": "docs/formats/world.md",
    "sha256": "81a7e067e859eaaae171d0422b5388510691d289ae37d6e42a25e5549d24cd20"}
CORE_REFERENCE = "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123"
ITEMS_REFERENCE = "513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e"
COMBAT_REFERENCE = "b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781"
AMX_REFERENCE = "091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f"
NETWIRE_REFERENCE = "4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42"
SESSION_REFERENCE = "4eb13e02ec6c1237ce7660794fd7e8e2f5870f15bd13a08784719a917fd2b14a"
OBJECTS_REFERENCE = "5b5c69873f8a42e0a184d22e21b0d49cc449ceece3c1e95686bca52b1df22c12"
SESSION_OBJECTS_REFERENCE = "7ecb261a35003527dcadc58c4ae55d3cfba8ade014bd53b0d6b7327fcbcb59d9"
SESSION_OBJECTS_ORDERING = {
    "commit": "0dd02ce416389dd440540119727d946636507703",
    "path": "docs/mainloop.md",
    "sha256": "b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895",
}
CREATURE_HP_REFERENCE = "44e47186b857b04c9295b480cea89369af4697e2467e650aa6d20773f9ad8dfb"
CREATURE_HP_SPECIFICATION = {
    "commit": "a0d89502ce1431375a6fc045b91a73039114e6b9",
    "path": "docs/spec/creature-tick.md",
    "sha256": "bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e",
}
COMBAT_CREATURE_REFERENCE = "b4591b57f67333156c74aca29f6df104f6e11b61ea98028d4cbac88d080c8b41"
RAW_HANDLES_HELPERS = {
    "objects_snapshot": {
        "path": "tools/generate_objects_vectors.py",
        "sha256": "39d6b7385eeeba2a6a431aae8dfeb573d94c7c801a16bca45672f90b17f6b6be",
    },
}
RAW_HANDLES_REFERENCE = "13a979a1fc02c0ddefadc26efbf2b047adbd3199c7d5e94d20d18a1f10ca3967"
SESSION_HP_REFERENCE = "0cf11501f2e0ee698c1cc8ad40c73295451ed2035822365e1de012ce6e532149"
SESSION_HP_HELPERS = {
    "session_objects": {
        "path": "tools/generate_session_objects_vectors.py",
        "sha256": "05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4",
    },
    "combat_snapshot": {
        "path": "tools/generate_combat_creature_vectors.py",
        "sha256": "a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd",
    },
}
SESSION_COMBAT_REFERENCE = SESSION_HP_REFERENCE
SESSION_COMBAT_HELPERS = {
    **SESSION_HP_HELPERS,
    "session_hp": {
        "path": "tools/generate_session_hp_vectors.py",
        "sha256": "8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8",
    },
}
WORLD_QUERIES_REFERENCE = "255d83526e2e0c4a337697dbb0a7862f1c2da943321c3df224aae289b454187b"
WORLD_QUERIES_SPECIFICATION = {'commit': 'b6881f52457bd715058248c34d65888c797ba657', 'path': 'docs/spec/world-queries.md', 'sha256': '1527e9f32e315ce72a9a2706fe39d3d7c65fd6fc568eceb4ec0629fd695c35c2'}
SESSION_TERRAIN_REFERENCE = '55728419d534c1ba99c1f6857b500c7a3b82b59fd4323f95033d624addcdbfc3'
SESSION_TERRAIN_HELPERS = {'combat_snapshot': {'path': 'tools/generate_combat_creature_vectors.py',
                     'sha256': 'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
 'session_hp': {'path': 'tools/generate_session_hp_vectors.py',
                'sha256': '8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'},
 'session_objects': {'path': 'tools/generate_session_objects_vectors.py',
                     'sha256': '05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'},
 'world_queries': {'path': 'tools/generate_world_queries_vectors.py',
                   'sha256': '293626ad96ed54022dbc1bb30953ba0b0c91b562d70ddae53d40695cd263c1cf'}}
SESSION_CONTROL_REFERENCE = SESSION_TERRAIN_REFERENCE
SESSION_CONTROL_HELPERS = {
    **SESSION_TERRAIN_HELPERS,
    "session_terrain": {
        "path": "tools/generate_session_terrain_vectors.py",
        "sha256": "dfffb7ad22bacf5d62b2c3ed30617af6fcd3298982724fa5ced0ae9be01b527b",
    },
}
SESSION_CREATURE_REFERENCE = SESSION_TERRAIN_REFERENCE
SESSION_CREATURE_HELPERS = {**SESSION_CONTROL_HELPERS}
SESSION_CONTROL_CREATURE_REFERENCE = SESSION_CREATURE_REFERENCE
SESSION_CONTROL_CREATURE_HELPERS = {
    **SESSION_CREATURE_HELPERS,
    "session_creature": {
        "path": "tools/generate_session_creature_vectors.py",
        "sha256": "561310e62d8dc7ccf7488193298fa33f0c7703cbc0ff495dbca22ba0825d152d",
    },
    "session_control": {
        "path": "tools/generate_session_control_vectors.py",
        "sha256": "5f1c66378e6af01ba29c8479a21c945504d916f0370b65a72af9856b6ace10b5",
    },
}
SESSION_ANIMATION_REFERENCE = "3efcfbac316f4c2484f2a870e3a072ce705f2ea46425b96507c0a3bdd6ce1992"
SESSION_ANIMATION_REFERENCES = {
    "anim": {
        "commit": "f516ac798bf28375554fe53ffb31b74802844136",
        "path": "tools/ref_anim.py",
        "sha256": "6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8"
    },
    "combat": {
        "commit": "7212ecfecbeae8dd2015f6c82897226f49a94695",
        "path": "tools/ref_combat.py",
        "sha256": "b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781"
    },
    "core": {
        "commit": "41e847af5360724ab62d208594f8027ea3c1b686",
        "path": "tools/ref_core.py",
        "sha256": "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123"
    },
    "creature": {
        "commit": "a0d89502ce1431375a6fc045b91a73039114e6b9",
        "path": "tools/ref_creature.py",
        "sha256": "debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7"
    },
    "netwire": {
        "commit": "54258f9d7e281ef07f0675040e82e6264f9386cf",
        "path": "tools/ref_netwire.py",
        "sha256": "4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42"
    },
    "objects": {
        "commit": "0dd02ce416389dd440540119727d946636507703",
        "path": "tools/ref_objects.py",
        "sha256": "541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2"
    }
}
SESSION_ANIMATION_HELPERS = {**SESSION_COMBAT_HELPERS}
SESSION_ANIMATION_GENERATOR = "8c68a52d0cabb4dd60c822b4db0f6e95cc986bd61edd759d3eded263fd792b3c"
SESSION_ANIMATION_PROTOCOL = {
    "columns": [
        "operation",
        "config",
        "id;explicit-input-DSL",
        "status",
        "observation"
    ],
    "commands": "start[0,id,clip,blend,flags]; stop[1,id,clip,blend]; advance[2,id,speed,consume_mask,damage]; event[3,clip,index,seconds,mask]; destroy[4,id]; authored_failure[5]",
    "observation": "[input,initial_full_snapshot,[[result,lossless_state_delta],...],authored_effects]",
    "setup": "[seed_variant,frozen,limits5,clips[[length,jointmask,events4,sound]],bindings[[HpId,capacity]],ready_programs,submissions]",
    "submissions": "[sample8bytes,yaw,camera,frame_step,no_sim,hold,render,pending,inboxByteLists]",
    "version": 1
}
TERRAIN_FILE_HELPERS = {'world_queries': {'path': 'tools/generate_world_queries_vectors.py',
                                        'sha256': '293626ad96ed54022dbc1bb30953ba0b0c91b562d70ddae53d40695cd263c1cf'}}
TERRAIN_FILE_FORMAT = {'commit': '9787e606c5c1f723dcef62701c67e607c5cff617',
                      'path': 'docs/formats/world.md',
                      'sha256': 'c76f22f8f2ff7de6c3e0e62ab38a50040d652a7530a1ae14bae1b945956c935f'}
CREATURE_TURN_REFERENCE = CREATURE_HP_REFERENCE
CREATURE_TURN_SPECIFICATION = CREATURE_HP_SPECIFICATION
CREATURE_TURN_REFERENCES = {'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686',
          'path': 'tools/ref_core.py',
          'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
 'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9',
              'path': 'tools/ref_creature.py',
              'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'}}
CREATURE_TURN_GENERATOR = 'f15149019e78c7782133bd5e2ae84fabc4549ce7a2768fad453936e0c28cf1f7'
CREATURE_TURN_PROTOCOL = {'canonical': 'integer decimal, null _, strings sUTF8hex, bytes bHEX, lists [comma-separated]',
 'columns': ['operation', 'configuration', 'id;input', 'status', 'observation'],
 'commands': {'0': '[0,address,signed_mode,[xRaw,yRaw,zRaw]]',
              '1': '[1,address,size,value] (unlogged poke)',
              '2': '[2,address,size,value] (logged memory store)'},
 'configuration': 'model=config//4; mul_bias=bool(config&2); div_nearest=bool(config&1)',
 'defaults': {'borrowed_memory': True,
              'callbacks': [],
              'external': [],
              'limits': [1048576, 65536, 64, 64],
              'overlay_present': False,
              'policy': 0},
 'events': 'write=[write,address,size,maskedvalue]; call=[call,name,typed_arguments]',
 'header': '# creature_turn_vectors_v1',
 'initial_poke': '[address,size,value] (unlogged sparse write; size 0..8)',
 'input': '[initial_pokes,operations]',
 'observation': '[submitted_input,initial_snapshot,[[status,result,snapshot],...]]',
 'snapshot': ['sorted_sparse_byte_pairs',
              'memory_store_log',
              'model_writes',
              'model_calls',
              'model_events',
              'draws',
              'callback_names',
              'external_names',
              'policy',
              'borrowed_memory',
              'overlay_present'],
 'statuses': 'ok or actual original ValueError/IndexError/OverflowError; top-level ok means '
             'completed observation',
 'version': 1}

SESSION_TURN_REFERENCE = '0cf11501f2e0ee698c1cc8ad40c73295451ed2035822365e1de012ce6e532149'
SESSION_TURN_REFERENCES = {'combat': {'commit': '7212ecfecbeae8dd2015f6c82897226f49a94695',
            'path': 'tools/ref_combat.py',
            'sha256': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
 'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686',
          'path': 'tools/ref_core.py',
          'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
 'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9',
              'path': 'tools/ref_creature.py',
              'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
 'netwire': {'commit': '54258f9d7e281ef07f0675040e82e6264f9386cf',
             'path': 'tools/ref_netwire.py',
             'sha256': '4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'},
 'objects': {'commit': '0dd02ce416389dd440540119727d946636507703',
             'path': 'tools/ref_objects.py',
             'sha256': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'}}
SESSION_TURN_SPECIFICATION = {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9',
 'path': 'docs/spec/creature-tick.md',
 'sha256': 'bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e'}
SESSION_TURN_ORDERING = {'commit': '0dd02ce416389dd440540119727d946636507703',
 'path': 'docs/mainloop.md',
 'sha256': 'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'}
SESSION_TURN_HELPERS = {'combat_snapshot': {'path': 'tools/generate_combat_creature_vectors.py',
                     'sha256': 'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
 'session_hp': {'path': 'tools/generate_session_hp_vectors.py',
                'sha256': '8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'},
 'session_objects': {'path': 'tools/generate_session_objects_vectors.py',
                     'sha256': '05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'}}
SESSION_TURN_GENERATOR = 'a1ef9d149aee0066504e9f3a9d8d5b2e50a70cad0ee49f8bdcfb5509bb51a8be'
SESSION_TURN_PROTOCOL = {'actions': 'no-submit[0]; '
            'submit[1,sample8bytes,yaw,camera,frame_step,no_sim,hold,render,pending,inboxByteLists]',
 'columns': ['operation', 'config', 'id;explicit-input-DSL', 'status', 'observation'],
 'commands': 'turn[0,id,mode,pointRaw3]; null-attacker HP[1,id,amountRaw,flags]; World scalar '
             'assignment[2,id,angle,posRaw3]; destroy[3,id]; recreate[4,posRaw3] using '
             'new0x400/flags32/tag0; authored failure[5]',
 'effects': 'run/end/hp task labels use existing helper '
            '[name,args...,worldframe,netframe,sharedseed]; completed commands append '
            '[command,submittedCommand,result,worldframe]',
 'errors': 'Refusal=authored Session preflight/latch; TaskFailure=explicit supplied later '
           'task failure. Original finite turning remains unmodified and successful; native '
           'budget failures are separate API tests.',
 'factory': 'Exactly two input actors in table order, extent0x400/tag0; original World '
            'allocation/grid placement. Bind HP in table order. Six unlogged zero spans '
            '(c8,4),(231,1),(2dc,4),(2e0,2),(304,4),(3b0,2) per actor; then input setupWrites '
            'before turning bindings. Fixed task IDs f1000001..3 hp_pre/hp_apply/hp_post '
            'priority20/30/40.',
 'observation': '[input,initial_full_snapshot,[[result,lossless_state_delta],...],authored_effects]',
 'setup': '[network[peers,local,shared_seed,combat_seed,network_seed],frozen,actors[[posRaw3,flags,angle,hp,cflags,status5,runEveryFrame]],turnLimits2,bindings[[HpId,rateRaw,av16,slow8]],setupWrites[[HpId,offset,size,value]],programsByReadyTick[[pre,apply,post]],actions]',
 'snapshot': '[complete prior Session/World/HP/Terrain base fields,TurningSnapshot20 in '
             'declaration order]; no creature-movement or animation extension admitted',
 'version': 1}

SESSION_ANIM_CACHE_IDENTITY = {'composition_policy': 'rta-session-anim-cache-v1',
 'generator_sha256': 'd0efa1a0329612b30b9ac9ada7f8680cfa97eece276897923bae45c412e5247f',
 'helpers': {'animation_runtime': {'path': 'tools/generate_animation_runtime_vectors.py',
                                   'sha256': '3f05cc05042a55f7a37714bd22f0180f45ce62eb3c7d681737ff852836df31cb'},
             'combat_snapshot': {'path': 'tools/generate_combat_creature_vectors.py',
                                 'sha256': 'a16df1a8350c50b7868193381d509b061e42fc81665cf0df064a3e89fde72fcd'},
             'session_animation': {'path': 'tools/generate_session_animation_vectors.py',
                                   'sha256': '8c68a52d0cabb4dd60c822b4db0f6e95cc986bd61edd759d3eded263fd792b3c'},
             'session_hp': {'path': 'tools/generate_session_hp_vectors.py',
                            'sha256': '8eb2c5333677a0711eb253b42a2ab80fbf171a7cf668f37d998ec12232ba27f8'},
             'session_objects': {'path': 'tools/generate_session_objects_vectors.py',
                                 'sha256': '05f799cc96f968bceca5302f8727015669c395643f66b265d58a75a99109c3b4'}},
 'input_protocol': {'binding': '[HpBinding4,StateId,calls,advanced,lastEvent,lastDecoded]',
                    'columns': ['operation',
                                'config',
                                'id;explicit-input-DSL',
                                'status',
                                'observation'],
                    'commands': 'start[0,hp,clip,blend,flags]; stop[1,hp,clip,blend]; '
                                'advance[2,hp,speed,consume_mask,damage]; '
                                'event[3,clip,index,seconds,mask]; destroy[4,hp]; failure[5]; '
                                'decode[6,hp,liveTrackId]; reuse[7,hp]',
                    'decoded': '[TrackId,SlotId,rawTime,[[rawPosition3,rawQuaternion4],...]]',
                    'host_budgets': 'Trace hook before original LOOP subtraction and decoder delta '
                                    'read; executed original stores preserved. No rewritten '
                                    'arithmetic.',
                    'observation': '[input,initialSnapshot,[[result,losslessDelta],...],authoredEffects]',
                    'operation': '[kind,hp,clip,track,frame,rawParameter,rawFlags,eventIndex,event2,result,lastDecode,completed]',
                    'setup': '[seed_variant,frozen,[slots,runtimeLimits4,calls,history],clips[[nameBytes,fileBytes]],bindings[[HpId,capacity]],ready_programs,submissions]',
                    'snapshot': '[complete legacy Session '
                                'base,cache[limits4,runtimeSnapshot3,bindings6,policy,phase,depth,calls,operations12,runCompleted,priority]]',
                    'submissions': '[sample8bytes,yaw,camera,step,no_sim,hold,render,pending,inboxByteLists]',
                    'version': 1},
 'ordering_evidence': {'commit': '0dd02ce416389dd440540119727d946636507703',
                       'path': 'docs/mainloop.md',
                       'sha256': 'b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895'},
 'references': {'anim': {'commit': 'f516ac798bf28375554fe53ffb31b74802844136',
                         'path': 'tools/ref_anim.py',
                         'sha256': '6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8'},
                'combat': {'commit': '7212ecfecbeae8dd2015f6c82897226f49a94695',
                           'path': 'tools/ref_combat.py',
                           'sha256': 'b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781'},
                'core': {'commit': '41e847af5360724ab62d208594f8027ea3c1b686',
                         'path': 'tools/ref_core.py',
                         'sha256': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123'},
                'creature': {'commit': 'a0d89502ce1431375a6fc045b91a73039114e6b9',
                             'path': 'tools/ref_creature.py',
                             'sha256': 'debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7'},
                'netwire': {'commit': '54258f9d7e281ef07f0675040e82e6264f9386cf',
                            'path': 'tools/ref_netwire.py',
                            'sha256': '4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42'},
                'objects': {'commit': '0dd02ce416389dd440540119727d946636507703',
                            'path': 'tools/ref_objects.py',
                            'sha256': '541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2'}},
 'specification_evidence': {'commit': 'f2b996dded898f1b0b558b901cab875f775cfdc7',
                            'path': 'docs/spec/animation.md',
                            'sha256': 'be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab'}}

CONTRACTS = {
    "session-anim-cache": ("session_anim_cache", SESSION_ANIMATION_REFERENCE),
    "animation-runtime": ("animation_runtime", ANIMATION_RUNTIME_REFERENCE),
    "session-turn": ("session_turn", SESSION_TURN_REFERENCE),
    "creature-turn": ("creature_turn", CREATURE_TURN_REFERENCE),
    "animation-decoder": ("animation_decoder", ANIMATION_DECODER_REFERENCE),
    "session-animation": ("session_animation", SESSION_ANIMATION_REFERENCE),
    "animation-clock": ("animation_clock", ANIMATION_CLOCK_REFERENCE),
    # Presentation geometry has its own contract; binary64 never enters game rules.
    "world-geometry": ("world_geometry", WORLD_GEOMETRY_REFERENCE),
    "arithmetic": ("core_arithmetic", CORE_REFERENCE),
    "math": ("core_math", CORE_REFERENCE),
    "state": ("core_state", CORE_REFERENCE),
    "items": ("items", ITEMS_REFERENCE),
    "item-generation": ("item_generation", ITEMS_REFERENCE),
    "item-names": ("item_names", ITEMS_REFERENCE),
    "combat-common": ("combat_common", COMBAT_REFERENCE),
    "combat-damage": ("combat_damage", COMBAT_REFERENCE),
    "combat-stats": ("combat_stats", COMBAT_REFERENCE),
    "combat-items": ("combat_items", COMBAT_REFERENCE),
    "amx-image": ("amx_image", AMX_REFERENCE),
    "amx-execute": ("amx_execute", AMX_REFERENCE),
    "amx-host": ("amx_host", AMX_REFERENCE),
    "netwire-codec": ("netwire_codec", NETWIRE_REFERENCE),
    "netwire-session": ("netwire_session", NETWIRE_REFERENCE),
    "netwire-tick": ("netwire_tick", NETWIRE_REFERENCE),
    "session-replay": ("session_replay", SESSION_REFERENCE),
    "objects-runtime": ("objects_runtime", OBJECTS_REFERENCE),
    "session-objects": ("session_objects", SESSION_OBJECTS_REFERENCE),
    "creature-hp": ("creature_hp", CREATURE_HP_REFERENCE),
    "creature-movement": ("creature_movement", CREATURE_HP_REFERENCE),
    "combat-creature": ("combat_creature", COMBAT_CREATURE_REFERENCE),
    "raw-handles": ("raw_handles", RAW_HANDLES_REFERENCE),
    "session-hp": ("session_hp", SESSION_HP_REFERENCE),
    "session-combat": ("session_combat", SESSION_COMBAT_REFERENCE),
    "world-queries": ("world_queries", WORLD_QUERIES_REFERENCE),
    "session-terrain": ("session_terrain", SESSION_TERRAIN_REFERENCE),
    "session-creature": ("session_creature", SESSION_CREATURE_REFERENCE),
    "terrain-file": ("terrain_file", WORLD_QUERIES_REFERENCE),
    "session-control": ("session_control", SESSION_CONTROL_REFERENCE),
    "session-control-creature": ("session_control_creature", SESSION_CONTROL_CREATURE_REFERENCE),
}


def reference_identity(manifest, suite):
    reference_hash = manifest.get("reference_sha256") or manifest["reference"]["sha256"]
    if reference_hash != CONTRACTS[suite][1]:
        raise ValueError("unsupported reference revision")
    if suite == "session-anim-cache":
        for field, expected in SESSION_ANIM_CACHE_IDENTITY.items():
            if manifest.get(field) != expected:
                raise ValueError("unsupported session-anim-cache " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_ANIMATION_REFERENCE:
            raise ValueError("unsupported session-anim-cache reference map")
    if suite == "session-turn":
        for field, expected in (
                ("references", SESSION_TURN_REFERENCES),
                ("composition_policy", "rta-session-turn-v1"),
                ("specification_evidence", SESSION_TURN_SPECIFICATION),
                ("ordering_evidence", SESSION_TURN_ORDERING),
                ("helpers", SESSION_TURN_HELPERS),
                ("generator_sha256", SESSION_TURN_GENERATOR),
                ("input_protocol", SESSION_TURN_PROTOCOL)):
            if manifest.get(field) != expected:
                raise ValueError("unsupported session-turn " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_TURN_REFERENCE:
            raise ValueError("unsupported session-turn reference map")
    if suite == "creature-turn":
        for field, expected in (
                ("references", CREATURE_TURN_REFERENCES),
                ("composition_policy", "rta-creature-turn-v1"),
                ("specification_evidence", CREATURE_TURN_SPECIFICATION),
                ("helpers", {}), ("generator_sha256", CREATURE_TURN_GENERATOR),
                ("input_protocol", CREATURE_TURN_PROTOCOL)):
            if manifest.get(field) != expected:
                raise ValueError("unsupported creature-turn " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != CREATURE_TURN_REFERENCE:
            raise ValueError("unsupported creature-turn reference map")
    if suite == "animation-runtime":
        for field, expected in (
                ("references", ANIMATION_RUNTIME_REFERENCES),
                ("composition_policy", "rta-animation-runtime-v1"),
                ("specification_evidence", ANIMATION_RUNTIME_SPECIFICATION),
                ("helpers", {}), ("generator_sha256", ANIMATION_RUNTIME_GENERATOR),
                ("input_protocol", ANIMATION_RUNTIME_PROTOCOL)):
            if manifest.get(field) != expected:
                raise ValueError("unsupported animation-runtime " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != ANIMATION_RUNTIME_REFERENCE:
            raise ValueError("unsupported animation-runtime reference map")
    if suite == "animation-decoder":
        for field, expected in (
                ("references", ANIMATION_DECODER_REFERENCES),
                ("composition_policy", "rta-animation-decoder-v1"),
                ("specification_evidence", ANIMATION_DECODER_SPECIFICATION),
                ("helpers", {}), ("generator_sha256", ANIMATION_DECODER_GENERATOR),
                ("input_protocol", ANIMATION_DECODER_PROTOCOL)):
            if manifest.get(field) != expected:
                raise ValueError("unsupported animation-decoder " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != ANIMATION_DECODER_REFERENCE:
            raise ValueError("unsupported animation-decoder reference map")
    if suite == "session-animation":
        for field, expected in (
                ("references", SESSION_ANIMATION_REFERENCES),
                ("composition_policy", "rta-session-animation-v1"),
                ("specification_evidence", ANIMATION_CLOCK_SPECIFICATION),
                ("ordering_evidence", SESSION_OBJECTS_ORDERING),
                ("helpers", SESSION_ANIMATION_HELPERS),
                ("generator_sha256", SESSION_ANIMATION_GENERATOR),
                ("input_protocol", SESSION_ANIMATION_PROTOCOL)):
            if manifest.get(field) != expected:
                raise ValueError("unsupported session-animation " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_ANIMATION_REFERENCE:
            raise ValueError("unsupported session-animation reference map")
    if suite == "animation-clock":
        for field, expected in (
                ("references", ANIMATION_CLOCK_REFERENCES),
                ("composition_policy", "rta-animation-clock-v1"),
                ("specification_evidence", ANIMATION_CLOCK_SPECIFICATION),
                ("helpers", {}), ("generator_sha256", ANIMATION_CLOCK_GENERATOR)):
            if manifest.get(field) != expected:
                raise ValueError("unsupported animation-clock " + field)
        encoded = json.dumps(manifest["references"], sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != ANIMATION_CLOCK_REFERENCE:
            raise ValueError("unsupported animation-clock reference map")
    if suite == "world-geometry":
        for field, expected in (
                ("references", WORLD_GEOMETRY_REFERENCES),
                ("composition_policy", "rta-world-geometry-v1"),
                ("specification_evidence", WORLD_GEOMETRY_SPECIFICATION),
                ("format_evidence", WORLD_GEOMETRY_FORMAT), ("helpers", {})):
            if manifest.get(field) != expected:
                raise ValueError("unsupported world-geometry " + field)
    if suite == "session-replay":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_REFERENCE:
            raise ValueError("unsupported session reference map")
        if manifest.get("composition_policy") != "rta-authored-session-v1":
            raise ValueError("unsupported session composition policy")
    if suite == "objects-runtime":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != OBJECTS_REFERENCE:
            raise ValueError("unsupported object reference map")
        if manifest.get("composition_policy") != "rta-objects-runtime-v1":
            raise ValueError("unsupported object fixture policy")
    if suite == "session-objects":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_OBJECTS_REFERENCE:
            raise ValueError("unsupported session-object reference map")
        if manifest.get("composition_policy") != "rta-session-objects-v1":
            raise ValueError("unsupported session-object fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-object ordering evidence")
    if suite == "creature-hp":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != CREATURE_HP_REFERENCE:
            raise ValueError("unsupported creature hit-point reference map")
        if manifest.get("composition_policy") != "rta-creature-hp-v1":
            raise ValueError("unsupported creature hit-point fixture policy")
        if manifest.get("specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported creature hit-point specification evidence")
    if suite == "creature-movement":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != CREATURE_HP_REFERENCE:
            raise ValueError("unsupported creature-movement reference map")
        if manifest.get("composition_policy") != "rta-creature-movement-v1":
            raise ValueError("unsupported creature-movement fixture policy")
        if manifest.get("specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported creature-movement specification evidence")
    if suite == "combat-creature":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != COMBAT_CREATURE_REFERENCE:
            raise ValueError("unsupported combat-creature reference map")
        if manifest.get("composition_policy") != "rta-combat-creature-v1":
            raise ValueError("unsupported combat-creature fixture policy")
        if manifest.get("specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported combat-creature specification evidence")
    if suite == "raw-handles":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != RAW_HANDLES_REFERENCE:
            raise ValueError("unsupported raw-handles reference map")
        if manifest.get("composition_policy") != "rta-raw-handles-v1":
            raise ValueError("unsupported raw-handles fixture policy")
        if manifest.get("specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported raw-handles specification evidence")
        if manifest.get("helpers") != RAW_HANDLES_HELPERS:
            raise ValueError("unsupported raw-handles snapshot helper")
    if suite == "session-hp":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_HP_REFERENCE:
            raise ValueError("unsupported session-hp reference map")
        if manifest.get("composition_policy") != "rta-session-hp-v1":
            raise ValueError("unsupported session-hp fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-hp ordering evidence")
        if manifest.get("specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported session-hp specification evidence")
        if manifest.get("helpers") != SESSION_HP_HELPERS:
            raise ValueError("unsupported session-hp snapshot helpers")
    if suite == "session-combat":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_COMBAT_REFERENCE:
            raise ValueError("unsupported session-combat reference map")
        if manifest.get("composition_policy") != "rta-session-combat-v1":
            raise ValueError("unsupported session-combat fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-combat ordering evidence")
        if manifest.get("specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported session-combat specification evidence")
        if manifest.get("helpers") != SESSION_COMBAT_HELPERS:
            raise ValueError("unsupported session-combat snapshot helpers")
    if suite == "world-queries":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != WORLD_QUERIES_REFERENCE:
            raise ValueError("unsupported world-queries reference map")
        if manifest.get("composition_policy") != "rta-supplied-terrain-v1":
            raise ValueError("unsupported world-queries fixture policy")
        if manifest.get("specification_evidence") != WORLD_QUERIES_SPECIFICATION:
            raise ValueError("unsupported world-queries specification evidence")
    if suite == "session-terrain":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_TERRAIN_REFERENCE:
            raise ValueError("unsupported session-terrain reference map")
        if manifest.get("composition_policy") != "rta-session-terrain-v1":
            raise ValueError("unsupported session-terrain fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-terrain ordering evidence")
        if manifest.get("specification_evidence") != WORLD_QUERIES_SPECIFICATION:
            raise ValueError("unsupported session-terrain terrain specification")
        if manifest.get("creature_specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported session-terrain creature specification")
        if manifest.get("helpers") != SESSION_TERRAIN_HELPERS:
            raise ValueError("unsupported session-terrain snapshot helpers")
    if suite == "session-creature":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_CREATURE_REFERENCE:
            raise ValueError("unsupported session-creature reference map")
        if manifest.get("composition_policy") != "rta-session-creature-v1":
            raise ValueError("unsupported session-creature fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-creature ordering evidence")
        if manifest.get("specification_evidence") != WORLD_QUERIES_SPECIFICATION:
            raise ValueError("unsupported session-creature terrain specification")
        if manifest.get("creature_specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported session-creature creature specification")
        if manifest.get("helpers") != SESSION_CREATURE_HELPERS:
            raise ValueError("unsupported session-creature snapshot helpers")
    if suite == "session-control":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_CONTROL_REFERENCE:
            raise ValueError("unsupported session-control reference map")
        if manifest.get("composition_policy") != "rta-session-control-v1":
            raise ValueError("unsupported session-control fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-control ordering evidence")
        if manifest.get("specification_evidence") != WORLD_QUERIES_SPECIFICATION:
            raise ValueError("unsupported session-control terrain specification")
        if manifest.get("creature_specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported session-control creature specification")
        if manifest.get("helpers") != SESSION_CONTROL_HELPERS:
            raise ValueError("unsupported session-control snapshot helpers")
    if suite == "session-control-creature":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != SESSION_CONTROL_CREATURE_REFERENCE:
            raise ValueError("unsupported session-control-creature reference map")
        if manifest.get("composition_policy") != "rta-session-control-creature-v1":
            raise ValueError("unsupported session-control-creature fixture policy")
        if manifest.get("ordering_evidence") != SESSION_OBJECTS_ORDERING:
            raise ValueError("unsupported session-control-creature ordering evidence")
        if manifest.get("specification_evidence") != WORLD_QUERIES_SPECIFICATION:
            raise ValueError("unsupported session-control-creature terrain specification")
        if manifest.get("creature_specification_evidence") != CREATURE_HP_SPECIFICATION:
            raise ValueError("unsupported session-control-creature creature specification")
        if manifest.get("helpers") != SESSION_CONTROL_CREATURE_HELPERS:
            raise ValueError("unsupported session-control-creature snapshot helpers")
    if suite == "terrain-file":
        encoded = json.dumps(manifest.get("references"), sort_keys=True,
                             separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if hashlib.sha256(encoded).hexdigest() != WORLD_QUERIES_REFERENCE:
            raise ValueError("unsupported terrain-file reference map")
        if manifest.get("composition_policy") != "rta-terrain-file-v1":
            raise ValueError("unsupported terrain-file fixture policy")
        if manifest.get("specification_evidence") != WORLD_QUERIES_SPECIFICATION:
            raise ValueError("unsupported terrain-file specification evidence")
        if manifest.get("format_evidence") != TERRAIN_FILE_FORMAT:
            raise ValueError("unsupported terrain-file format evidence")
        if manifest.get("helpers") != TERRAIN_FILE_HELPERS:
            raise ValueError("unsupported terrain-file snapshot helper")
    return reference_hash
