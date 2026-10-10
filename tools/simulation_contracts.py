"""Immutable reference revisions and fixture identities for simulation gates."""
CORE_REFERENCE = "0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123"
ITEMS_REFERENCE = "513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e"
COMBAT_REFERENCE = "b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781"
AMX_REFERENCE = "091f848c1c6f3d0ba8552e5d1ea7ab4ef577f65be9ca750b5b4a9fe5ec4a683f"
NETWIRE_REFERENCE = "4db335e986a62f34df860b7e23b72e763427000ba86e8ad24e54ccc52ebe3d42"
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
}
