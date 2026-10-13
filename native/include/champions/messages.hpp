// Authored translation of the MIT-licensed RTA reference contract. See GAME_MESSAGES.md.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <variant>

namespace champions::messages::rta {
using Bytes = std::vector<std::uint8_t>;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
struct Write { std::uint32_t address; unsigned size; std::uint64_t value; };
// Sparse reference memory: absent bytes are zero, including null addresses.
// A scalar base is raw32; its subsequent bytes do not wrap within the scalar.
struct Memory {
    std::map<std::uint64_t, std::uint8_t> bytes;
    std::vector<Write> writes;
    std::uint64_t load(std::uint32_t address, unsigned size) const;
    void poke(std::uint32_t address, unsigned size, std::uint64_t value);
    void store(std::uint32_t address, unsigned size, std::uint64_t value);
};
using Position = std::array<std::uint32_t, 3>;
using Argument = std::variant<std::uint64_t, Bytes, Position>;
struct Call { std::string name; Bytes payload; std::uint64_t argument;
    std::vector<std::uint64_t> arguments;
    std::vector<Argument> typed_arguments{};
    bool service_call = false; };
struct Callbacks {
    // Missing callbacks return zero / do nothing, exactly as the reference.
    std::function<std::uint32_t(std::uint32_t)> local_player_slot;
    std::function<void(const std::string&, const Bytes&, std::uint64_t)> external_handler;
    // Named scalar services; arguments are canonical raw integer/float patterns.
    // Calls are recorded before invocation. Missing services return zero.
    std::function<std::uint64_t(const std::string&, const std::vector<std::uint64_t>&)> service;
    // Built wide text and position tuples retain their argument type and order.
    std::function<std::uint64_t(const std::string&, const std::vector<Argument>&)> typed_service;
};
inline constexpr std::uint32_t handlers_address = 0x5d1498;
inline constexpr std::uint32_t network_mode_address = 0x402791;
inline constexpr std::uint32_t peers_address = 0x5bf838;
inline constexpr std::uint32_t peer_size = 0x44e0;
inline constexpr std::uint32_t ring_offset = 0x24ac;
inline constexpr std::uint32_t write_position_offset = 0x44ac;
inline constexpr std::uint32_t ring_mask = 0x1fff;

class Machine {
public:
    explicit Machine(Memory& memory, Callbacks callbacks = {});
    std::uint32_t register_handler(std::uint32_t handler);
    // Order: cutscene, voice, dialog, game, null, inventory, text, shop.
    std::vector<std::uint32_t> register_all();
    void send(std::uint32_t channel, const Bytes& data);
    // Supply the full caller buffer, including any explicit stale tail.
    // Inventory/shop bodies are refused before any state changes.
    void deliver(std::uint32_t channel, const Bytes& payload);
    // Player abilities and HUD controls. Inputs keep Python integer semantics;
    // methods apply signed32/16/8 conversion only where the reference does.
    std::int32_t skill_rank(std::int64_t slot, std::int64_t skill_id);
    void set_skill_rank(std::int64_t slot, std::int64_t skill_id, std::int64_t rank);
    std::int32_t passive_level(std::uint64_t slot, std::uint64_t mask);
    void raise_passive_level(std::int64_t slot, std::uint64_t mask, std::int64_t level);
    void rebuild_abilities(std::uint64_t mask, std::int64_t slot, std::int64_t index);
    void add_skill_ability(std::int64_t skill_id, std::int64_t slot);
    void on_quick_slot(std::int64_t slot, std::int64_t button, std::int64_t index);
    void on_assign_spell(std::int64_t slot, std::int64_t index, std::int64_t kind, std::uint64_t value);
    void mask_local_buttons(std::int64_t slot, std::int64_t frames, std::uint32_t mask);
    void on_menu_state(std::int64_t slot, std::int64_t state);
    void on_quick_cast(std::int64_t slot, std::int64_t index);
    // Complete HUD lifecycle plus recall's inventory-compaction dependencies.
    void hud_on_msg(const Bytes& payload);
    void recall(std::int64_t slot);
    void stop_stream();
    void on_player_left(std::int64_t slot);
    void on_disconnect_request(std::int64_t slot);
    void set_pending_level(const Bytes& name, bool clear_load_flag);
    void on_checksum(std::int64_t slot, std::uint64_t live, std::uint64_t saved);
    bool same_identity(std::uint32_t a, std::uint32_t b) const;
    std::uint32_t grid(std::int64_t panel, std::int64_t bag) const;
    void remove_item(std::int64_t slot, std::int64_t index, std::int64_t bag, std::int64_t panel);
    Bytes cstr(std::uint32_t address, std::int64_t limit = 4096) const;
    Bytes wstr(std::uint32_t address, std::int64_t limit = 4096) const;
    Position vec(std::uint32_t address) const;
    void copy(std::uint32_t dst, std::uint32_t src, std::int64_t size);
    std::vector<Call> calls;
private:
    std::uint32_t rec(std::int64_t slot) const;
    std::int32_t signed_load(std::uint32_t address, unsigned size) const;
    std::uint64_t call(const std::string& name, std::vector<std::uint64_t> arguments);
    std::uint64_t typed_call(const std::string& name, std::vector<Argument> arguments);
    void sound(std::uint32_t lump, std::uint32_t entry);
    Memory& memory_;
    Callbacks callbacks_;
};

enum class Kind {
    drop, equip, unequip, join_panel, weapon_class, stat_point, skill_point,
    combine, level_ack, use_potion, quick_slot, assign_spell, menu_state,
    recall, player_left, savepoint, disconnect_request, new_world,
    exit_countdown, checksum_start, checksum, load_level, medal_ack,
    quick_cast, buy, sell, shop_closed
};
struct Message { std::string family; Bytes payload; };
// Arguments in ref_messages.msg_* order, raw integer patterns with truncation at
// field width. new_world takes only name bytes; other kinds take only integers.
// All arguments are explicit (including quiet, flag, and sell mode).
Message encode(Kind kind, const std::vector<std::uint64_t>& arguments = {},
               const Bytes& name = {});
} // namespace champions::messages::rta
