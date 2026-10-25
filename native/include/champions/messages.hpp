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
#include <any>
#include <utility>
#include "champions/items.hpp"

namespace champions::messages::rta {
// Signed conversion avoids implementation-defined unsigned-to-signed casts.
std::int64_t s64(std::uint64_t value);
using Bytes = std::vector<std::uint8_t>;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
// Arbitrary signed integers for the general Python-memory boundary. Hex input
// is sign/magnitude; storage and truncation use infinite two's complement.
class Integer {
public:
    Integer(std::uint64_t value = 0);
    static Integer from_hex(const std::string&);
    std::string hex() const;
    std::uint64_t low64() const;
    Integer next() const;
    Integer truncated(std::size_t size) const;
    Integer or_shift(const Integer&, std::size_t bytes) const;
    std::uint8_t byte(std::size_t index) const;
    bool operator<(const Integer&) const;
    bool operator==(const Integer&) const;
private:
    Bytes magnitude_;
    bool negative_ = false;
    void normalize();
    static Integer from_twos(Bytes, bool negative);
};
struct Write { Integer address; std::int64_t size; Integer value; };
struct Memory {
    using Backing = std::function<Integer(const Integer&, std::int64_t)>;
    explicit Memory(Backing backing = {}) : under(std::move(backing)) {}
    std::map<Integer, std::uint8_t> bytes;
    std::vector<Write> writes;
    Backing under;
    Integer load(const Integer& address, std::int64_t size) const;
    void poke(const Integer& address, std::int64_t size, const Integer& value);
    void store(const Integer& address, std::int64_t size, const Integer& value);
    // Existing scalar boundary retains raw32 bases and raw64 return patterns.
    std::uint64_t load(std::uint32_t address, unsigned size) const;
    void poke(std::uint32_t address, unsigned size, std::uint64_t value);
    void store(std::uint32_t address, unsigned size, std::uint64_t value);
};
namespace Items = champions::simulation::rta::items;
using SoundTable = std::array<std::uint32_t, 13>;
std::uint32_t sound_name(const SoundTable&, std::uint64_t flags, std::int64_t slot, std::uint64_t subtype, bool short_form = false);
using Position = std::array<std::uint32_t, 3>;
using Argument = std::variant<std::uint64_t, Bytes, Position>;
struct Call { std::string name; Bytes payload; std::uint64_t argument;
    std::vector<std::uint64_t> arguments;
    std::vector<Argument> typed_arguments{};
    bool service_call = false;
    std::vector<std::any> general_arguments{}; };
struct Callbacks {
    using General = std::function<std::any(const std::vector<std::any>&)>;
    // Missing callbacks return zero / do nothing, exactly as the reference.
    std::function<std::uint32_t(std::uint32_t)> local_player_slot;
    std::function<void(const std::string&, const Bytes&, std::uint64_t)> external_handler;
    // Named scalar services; arguments are canonical raw integer/float patterns.
    // Calls are recorded before invocation. Missing services return zero.
    std::function<std::uint64_t(const std::string&, const std::vector<std::uint64_t>&)> service;
    // Built wide text and position tuples retain their argument type and order.
    std::function<std::uint64_t(const std::string&, const std::vector<Argument>&)> typed_service;
    // Vector-return service. Missing/incorrect return refuses at the reference
    // tuple-conversion point, after pool_new if allocation succeeded.
    std::function<std::variant<std::uint64_t, Position>(const std::string&, const std::vector<Argument>&)> result_service{};
    // Lookup itself may mutate, reenter or throw; record before lookup.
    std::function<General(const std::string&)> lookup{};
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
    explicit Machine(Memory& memory, Callbacks callbacks = {}, const Items::Tables* tables = nullptr);
    std::any call(const std::string& name, std::vector<std::any> arguments = {});
    // Scalar wrappers mask only the base address to raw32, as in Python.
    std::uint64_t u8(std::uint64_t address) const;
    std::int32_t s8(std::uint64_t address) const;
    std::uint64_t u16(std::uint64_t address) const;
    std::int32_t s16(std::uint64_t address) const;
    std::uint64_t u32(std::uint64_t address) const;
    std::int32_t s32(std::uint64_t address) const;
    std::uint64_t u64(std::uint64_t address) const;
    void w8(std::uint64_t address, std::uint64_t value);
    void w16(std::uint64_t address, std::uint64_t value);
    void w32(std::uint64_t address, std::uint64_t value);
    void w64(std::uint64_t address, std::uint64_t value);
    std::uint32_t register_handler(std::uint32_t handler);
    // Order: cutscene, voice, dialog, game, null, inventory, text, shop.
    std::vector<std::uint32_t> register_all();
    void send(std::uint32_t channel, const Bytes& data);
    // Supply the full caller buffer, including any explicit stale tail.
    // All reference families execute; delegated services remain callbacks.
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
    void shop_on_msg(const Bytes& payload);
    void shop_buy(std::int64_t slot, std::uint32_t record);
    void shop_sell(std::int64_t slot, std::int64_t index, std::int64_t bag,
                   std::int64_t column, std::int64_t row, std::int64_t panel, std::int64_t mode);
    void inv_dispatch(const Bytes& payload);
    void send_character_to_lobby(std::int64_t slot);
    Items::Item item_object(std::uint32_t item) const;
    std::int32_t check_equip_allowed(std::int64_t slot, std::uint32_t item) const;
    std::int32_t item_value(std::uint32_t item) const;
    std::int32_t give_item(std::int64_t slot, std::uint32_t src, std::int64_t announce);
    void first_recall_hint(std::int64_t slot, std::uint32_t record);
    void unref_item(std::int64_t slot, std::uint64_t item);
    void drop_item(std::int64_t slot, std::int64_t index, std::int64_t bag,
                   std::int64_t column, std::int64_t row, std::int64_t panel, std::int64_t mode);
    void equip_item(std::int64_t slot, std::uint64_t item, std::int64_t quiet);
    void _equip_item(std::int64_t slot, std::uint64_t item, std::int64_t quiet);
    void unequip_item(std::int64_t slot, std::uint64_t item);
    void on_join_panel(std::int64_t panel, std::int64_t slot, std::int64_t level_up);
    void on_stat_point(std::int64_t slot, std::int64_t stat);
    void on_skill_point(std::int64_t slot, std::int64_t is_skill, std::int64_t value);
    std::uint64_t depth = 0;
    std::vector<Call> calls;
private:
    std::uint32_t rec(std::int64_t slot) const;
    std::int32_t signed_load(std::uint32_t address, unsigned size) const;
    std::uint64_t scalar_call(const std::string& name, std::vector<std::uint64_t> arguments);
    std::uint64_t typed_call(const std::string& name, std::vector<Argument> arguments);
    void sound(std::uint32_t lump, std::uint32_t entry);
    const Items::Tables* tables_;
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
