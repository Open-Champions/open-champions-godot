"""Immutable reference revisions and fixture identities for simulation gates."""
import hashlib
import json
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
TERRAIN_FILE_HELPERS = {'world_queries': {'path': 'tools/generate_world_queries_vectors.py',
                                        'sha256': '293626ad96ed54022dbc1bb30953ba0b0c91b562d70ddae53d40695cd263c1cf'}}
TERRAIN_FILE_FORMAT = {'commit': '9787e606c5c1f723dcef62701c67e607c5cff617',
                      'path': 'docs/formats/world.md',
                      'sha256': 'c76f22f8f2ff7de6c3e0e62ab38a50040d652a7530a1ae14bae1b945956c935f'}
CONTRACTS = {
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
