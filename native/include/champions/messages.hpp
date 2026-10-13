// Authored translation of the MIT-licensed RTA reference contract. See GAME_MESSAGES.md.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

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
struct Call { std::string name; Bytes payload; std::uint64_t argument; };
struct Callbacks {
    // Missing callbacks return zero / do nothing, exactly as the reference.
    std::function<std::uint32_t(std::uint32_t)> local_player_slot;
    std::function<void(const std::string&, const Bytes&, std::uint64_t)> external_handler;
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
    // Inventory/game/shop bodies are refused before any state changes.
    void deliver(std::uint32_t channel, const Bytes& payload);
    std::vector<Call> calls;
private:
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
