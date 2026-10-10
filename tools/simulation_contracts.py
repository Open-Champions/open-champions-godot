"""Immutable reference revisions and fixture identities for simulation gates."""
import hashlib
import json
CORE_REFERENCE = "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123"
ITEMS_REFERENCE = "513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e"
COMBAT_REFERENCE = "b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781"
AMX_REFERENCE = "091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f"
NETWIRE_REFERENCE = "4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42"
SESSION_REFERENCE = "4eb13e02ec6c1237ce7660794fd7e8e2f5870f15bd13a08784719a917fd2b14a"
OBJECTS_REFERENCE = "5b5c69873f8a42e0a184d22e21b0d49cc449ceece3c1e95686bca52b1df22c12"
CONTRACTS = {
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
}


def reference_identity(manifest, suite):
    reference_hash = manifest.get("reference_sha256") or manifest["reference"]["sha256"]
    if reference_hash != CONTRACTS[suite][1]:
        raise ValueError("unsupported reference revision")
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
    return reference_hash
