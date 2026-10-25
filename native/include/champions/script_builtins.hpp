#pragma once
#include "champions/amx.hpp"
#include "champions/objects.hpp"
#include "champions/world_authority.hpp"
#include <array>
#include <memory>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <variant>

namespace champions::simulation::rta::builtins {
inline constexpr std::uint32_t QUEST_VARS = 0x86da80, PROFILE_VARS = 0x4f9030;
class StackGarbage : public amx::HostFault {
public:
  using amx::HostFault::HostFault;
};
// Zero-backed, owned byte storage at reference addresses. These are identities,
// never pointers into the host process. Writes retain their original order/size.
struct Memory {
  using Page = std::vector<std::uint8_t>;
  using Backing = std::function<std::vector<std::uint8_t>(std::uint32_t, std::size_t)>;
  // Arbitrary-width integer magnitude, least significant byte first. A negative
  // store takes its two's complement modulo the requested width.
  struct Integer { std::vector<std::uint8_t> magnitude; bool negative = false; };
  explicit Memory(Backing backing = {}) : under(std::move(backing)) {}
  explicit Memory(creature_hp::Memory &shared) : canonical(&shared) {}
  Backing under;
  std::vector<std::pair<std::uint32_t, std::size_t>> writes;
  Page &page(std::uint32_t index);
  const std::map<std::uint32_t, std::uint8_t> &nonzero_bytes() const;
  std::uint8_t read(std::uint32_t) const;
  std::vector<std::uint8_t> read(std::uint32_t, std::size_t) const;
  Integer load(std::uint32_t, std::size_t) const;
  void store(std::uint32_t, std::size_t, const Integer &);
  void store(std::uint32_t, std::size_t, std::uint64_t);
  std::uint8_t u8(std::uint32_t a) const { return read(a); }
  std::uint16_t u16(std::uint32_t) const;
  std::uint32_t u32(std::uint32_t a) const { return word(a); }
  std::uint64_t u64(std::uint32_t) const;
  std::int32_t s8(std::uint32_t) const;
  std::int32_t s16(std::uint32_t) const;
  std::int32_t s32(std::uint32_t) const;
  void w8(std::uint32_t a, std::uint64_t v) { store(a,1,v); }
  void w16(std::uint32_t a, std::uint64_t v) { store(a,2,v); }
  void w32(std::uint32_t a, std::uint64_t v) { store(a,4,v); }
  void w64(std::uint32_t a, std::uint64_t v) { store(a,8,v); }
  std::string cstr(std::uint32_t, std::size_t limit = 0x4000) const;
  void write(std::uint32_t, const std::vector<std::uint8_t> &);
  void byte(std::uint32_t, std::uint8_t);
  std::uint32_t word(std::uint32_t) const;
  void word(std::uint32_t, std::uint32_t);
  void string(std::uint32_t, const std::string &);
private:
  creature_hp::Memory *canonical = nullptr;
  Page &ensure_page(std::uint32_t) const;
  mutable std::map<std::uint32_t, Page> pages;
  mutable std::map<std::uint32_t, std::uint8_t> bytes;
  // Exposed mutable pages may be changed after a snapshot; rescan these pages.
  mutable std::vector<std::uint32_t> exposed;
};
// Pinned _tdiv: arbitrary signed integer quotient, truncated toward zero.
// Inputs may have leading zero bytes or signed zero; output is canonical.
// Zero divisor raises std::domain_error (Python ZeroDivisionError).
Memory::Integer truncating_divide(const Memory::Integer &, const Memory::Integer &);
class Global {
public:
  explicit Global(std::uint32_t address, std::string kind = "s32", std::string doc = "");
  std::uint32_t addr;
  std::string kind;
  std::size_t size;
  std::string documentation;
  std::int64_t get(const Memory &) const;
  void set(Memory &, const Memory::Integer &) const;
  void set(Memory &, std::uint64_t) const;
  // The class-level Python descriptor read returns the descriptor itself.
  const Global &descriptor() const noexcept { return *this; }
};
// Obj's Python properties are explicit live accessors here. Inherited World
// fields are not an automatic memory bridge: World integration is separate.
class Obj : public objects::GameObject {
public:
  Memory *mem;
  explicit Obj(Memory *memory, std::uint32_t address,
               std::string type = "10GameObject");
  explicit Obj(Memory &memory, std::uint32_t address,
               std::string type = "10GameObject")
      : Obj(&memory, address, std::move(type)) {}
  ~Obj();
  Obj(const Obj &) = delete;
  Obj &operator=(const Obj &) = delete;
  // Recognize memory-backed objects without changing the independent World ABI.
  static const Obj *memory_backed(const objects::GameObject *);
  std::uint32_t flags() const;
  void flags(const Memory::Integer &);
  void flags(std::uint64_t);
  std::array<std::uint32_t,3> pos() const;
  // Like Python indexing, a short input fails after preceding component stores.
  void pos(const std::vector<Memory::Integer> &);
  void pos(const std::array<std::uint32_t,3> &);
  std::uint16_t angle() const;
  void angle(const Memory::Integer &);
  void angle(std::uint64_t);
  Memory::Integer u(std::uint32_t offset, std::size_t size) const;
  void w(std::uint32_t offset, std::size_t size, const Memory::Integer &);
  void w(std::uint32_t offset, std::size_t size, std::uint64_t);
  std::uint64_t cflags() const;
  void set_cflags(const Memory::Integer &);
  void set_cflags(std::uint64_t);
  std::string repr() const;
};
// Metadata only; does not implement or invoke the described engine service.
struct X {
  std::uint32_t addr;
  std::string ints;
  std::int64_t floats, stack;
  std::string ret, note;
  explicit X(std::uint32_t address, std::string integers = "",
             std::int64_t float_count = 0, std::int64_t stack_count = 0,
             std::string result = "n", std::string documentation = "");
};
// Host owns dispatch/logging, while ref_objects.World and GameState operations
// remain explicit delegates. Hooks must outlive this Host; handlers may reenter.
class Host {
public:
  using Object = objects::GameObject;
  using List = std::vector<Object *>;
  using Vec3 = std::array<std::uint32_t,3>;
  struct Seat { Vec3 position{}; std::int64_t facing = 0; };
  struct ItemBlank {
    std::array<std::uint8_t,18> bytes{};
    std::array<std::uint16_t,4> words{{65535,65535,65535,65535}};
    ItemBlank() {bytes[16]=bytes[17]=255;}
  };
  using Value = std::variant<std::monostate, std::int64_t, std::string, Vec3,
      Object *, List, std::pair<std::int64_t,std::int64_t>, Seat, ItemBlank, Memory::Integer>;
  using Arguments = std::vector<Value>;
  using Handler = std::function<Value(const Arguments &)>;
  struct Game {
    std::function<Object *(std::uint32_t)> object;
    std::function<void(Object &)> move_off_grid, update_grid_cell, run_every_frame;
    std::function<List(const Vec3 &, const Vec3 &, std::int64_t)> collect_in_box;
    std::function<void(const std::function<void(Object &)> &)> for_each_grid_object;
    std::function<List(const std::string &, std::int64_t)> find_by_class;
    std::function<void(Object &, std::int64_t)> on_destroy;
    std::function<void()> rebuild_alive_players;
  };
  Game *game;
  std::vector<std::pair<std::string,Arguments>> log;
  std::map<std::string,Handler> handlers;
  explicit Host(Game *owner = nullptr) : game(owner) {}
  Value call(std::string, const Arguments & = {});
  Value default_answer(const std::string &, const Arguments &) const;
  Object &object(std::uint32_t) const;
  void do_obj_moveOffGrid(std::uint32_t);
  void do_obj_updateGridCell(std::uint32_t);
  void do_obj_runEveryFrame(std::uint32_t);
  List do_world_collectObjectsInBox(const Vec3 &, const Vec3 &, std::int64_t);
  List do_world_forEachGridObject(const Value &);
  List do_world_findObjectsByClass(const std::string &, std::int64_t);
  Value do_virtual(std::int64_t, std::uint32_t, const Arguments & = {});
  void do_game_rebuildAlivePlayers();
};
class Variables;
// Reads the live AMX cells, ignoring the pushed argument count. The adapter
// captures params+4 at construction; Machine and Variables must outlive it.
class Args {
public:
  amx::Machine &m;
  const amx::Address base;
  Variables *game;
  explicit Args(amx::Machine &, Variables * = nullptr, const std::string &name = "");
  amx::Cell operator[](amx::Address) const;
  std::uint32_t u(amx::Address) const;
  std::uint32_t f(amx::Address) const;
  amx::Address address(amx::Address);
  std::string s(amx::Address, amx::Address room = 160);
private:
  std::map<amx::Address,unsigned> slot_of;
  std::optional<unsigned> fallback;
};
class Variables {
  std::shared_ptr<Memory> memory_owner;
  std::shared_ptr<WorldAuthority> authority_;
  Host::Game host_owner;
  std::vector<std::shared_ptr<Obj>> setup_objects;
  std::int64_t next_address = 0x01000000;
public:
  Memory &memory;
  std::shared_ptr<objects::World> world;
  std::shared_ptr<Host> host;
  // Required when profile_set actually requests a save. No persistence is
  // fabricated; absence refuses after the documented table write prefix.
  std::function<void()> request_config_save;
  std::optional<amx::Cell> string_slot;
  // Existing reward boundary: host may wire it to run_script, which owns nested
  // execution and slots. No host-provided reward behavior is replaced implicitly.
  std::function<void(const std::string &)> run_reward_script;
  // Owned current AMX machine; a running call retains its machine if a callback
  // selects another one. Loading remains the caller's responsibility.
  std::shared_ptr<amx::Machine> script_machine;
  amx::Address entry_pri = 0;
  void set_script(std::shared_ptr<amx::Machine>);
  amx::Address run_script(const std::optional<std::string> &);
  void queue_deferred(const std::string &);
  void run_deferred();
  void clear_deferred();
  void reset_script_state();
  void set_post_action(const std::string &);
  void run_post_action();
  std::function<amx::Cell()> local_player_slot;
  std::optional<amx::Cell> quest_string_slot; // handler stack slot 0x30
  // External services remain explicit; integer arguments/results are raw low32.
  std::function<std::uint32_t(const std::string &, const std::vector<std::uint32_t> &)> player_service;
  std::function<std::uint32_t(const std::string &, const std::string &)> find_resource;
  using ServiceValue = std::variant<std::uint32_t, std::string, std::array<std::uint32_t, 3>>;
  // Strings are arbitrary owned bytes (including dialog packets), not host pointers.
  std::function<std::uint32_t(const std::string &, const std::vector<ServiceValue> &)> scene_service;
  // Address registry projection used by level transitions and prop animation. Missing address
  // and absent property list are distinct reference faults; zero means no object.
  std::map<std::uint32_t, std::optional<std::vector<std::string>>> object_properties;
  std::optional<amx::Cell> dialog_string_slot, cutscene_string_slot;
  // Reuse the tested object name table. Owners keep named objects alive; only
  // Address identity and type_name are used; fields are read live from Memory.
  objects::NameTable &object_names;
  std::map<std::uint32_t, std::shared_ptr<objects::GameObject>> named_objects;
  std::map<unsigned, amx::Cell> menu_string_slots;
  // Grid enumeration belongs to the host. Returned objects must stay alive during
  // dispatch; their fields are read live from Memory, in the supplied order.
  std::function<std::vector<objects::GameObject *>(const std::string &,
      const std::vector<ServiceValue> &)> object_query;
  // The pinned model represents only the initialized portions of the temporary
  // item record. Other item contents belong to the host, as in the reference.
  struct ItemBlank {
    std::array<std::uint8_t, 18> bytes{};
    std::array<std::uint16_t, 4> words{{65535,65535,65535,65535}};
    ItemBlank() { bytes[16]=bytes[17]=255; }
  };
  using InventoryValue = std::variant<std::uint32_t, std::string,
      std::array<std::uint32_t,3>, ItemBlank>;
  using InventoryResult = std::variant<std::uint32_t, std::array<std::uint32_t,2>>;
  // findItemBySubstring returns {index, bag}; every other service returns a word.
  std::function<InventoryResult(const std::string &, const std::vector<InventoryValue> &)> inventory_service;
  // Virtual slot 19 supplies raw position words and integer facing. No seat
  // geometry is invented; absence refuses before entity writes.
  struct VehicleSeat { std::array<std::uint32_t,3> position; std::uint32_t facing; };
  std::function<VehicleSeat(std::uint32_t, const std::array<std::uint32_t,3> &)> vehicle_seat;
  // Record copies use scene_service("save_playerCopy", {slot}) as an owned
  // Memory address; alive-list rebuilding stays an explicit host callback.
  void save_player_record();
  void restore_player_record();
  amx::Cell spell_effect(const std::string &, std::uint32_t, bool);
  // Raw {distance, dx, dy}; ordered console arithmetic, no host float math.
  static std::array<std::uint32_t,3> distance3(const std::array<std::uint32_t,3> &, const std::array<std::uint32_t,3> &);
  std::uint32_t vehicle_of(amx::Cell) const;
  std::uint64_t draws = 0;
  // hideMonster / showMonster test object flag 0x20 and then touch +0x231. An
  // unbuilt stub has flags 0; this answers, per object, whether its class is a
  // Creature (decided per factory row from the research probe). Unset: the
  // flag word alone decides.
  std::function<bool(const objects::GameObject &)> stub_creature;
  std::uint32_t random_draw();
  objects::GameObject *player_object(amx::Cell) const;
  void empty_bag(amx::Cell, unsigned);
  void drop_at(std::function<objects::GameObject *()>);
  bool give(amx::Cell, const std::string &, amx::Cell, amx::Cell, bool, bool);
  void give_or_drop(amx::Cell);
  objects::GameObject *obj_at(std::uint32_t) const;
  void apply_monster_val(objects::GameObject *, amx::Cell, amx::Cell);
  void apply_cflag(objects::GameObject *, amx::Cell, bool);
  void apply_oflag(objects::GameObject *, amx::Cell, bool);
  void follow_path(objects::GameObject *);
  void stop_path(objects::GameObject *);
  amx::Cell alive(amx::Cell) const;
  std::vector<objects::GameObject *> find(const std::string &, std::int64_t) const;
  objects::GameObject *find_first(const std::string &) const;
  objects::GameObject *need(objects::GameObject *, const std::string &) const;
  amx::Cell medal_gate();
  std::uint32_t medal_prompt();
  amx::Cell medal_pad();
  amx::Cell net_gate();
  void set_next_world(const std::string &);
  void dialog_start(const std::string &, amx::Cell, amx::Cell);
  amx::Cell new_world_no_gate(amx::Machine &);
  void player_set_flag(amx::Cell, amx::Cell, amx::Cell);
  amx::Cell player_get_flag(amx::Cell, amx::Cell) const;
  amx::Cell count_players_with_hp() const;
  explicit Variables(std::shared_ptr<Memory> = {},
                     std::shared_ptr<objects::World> = {}, std::shared_ptr<Host> = {});
  explicit Variables(std::shared_ptr<WorldAuthority>);
  ~Variables();
  // Supplied worlds and named objects must not outlive this memory-backed owner.
  Host::Value ext(const std::string &, const Host::Arguments & = {});
  Host::Value virtual_call(const objects::GameObject &, std::int64_t,
                           const Host::Arguments & = {});
  void rebuild_alive_players();
  std::shared_ptr<Obj> new_object(const std::vector<std::string> &names = {},
      std::uint32_t flags = 0, const std::array<std::uint32_t,3> &pos = {},
      std::uint32_t angle = 0, const std::string &type = "10GameObject",
      objects::Props props = {}, std::optional<std::uint32_t> address = {},
      std::int64_t size = 0x1000);
  std::vector<std::pair<std::string, amx::Cell>> quest_vars() const;
  Variables(const Variables &) = delete;
  Variables &operator=(const Variables &) = delete;
  Variables(Variables &&) = delete;
  Variables &operator=(Variables &&) = delete;
  amx::Cell quest_get(const std::string &);
  void quest_set(const std::string &, amx::Cell);
  void quest_clear_all();
  std::optional<std::pair<std::string, amx::Cell>> quest_by_index(amx::Address) const;
  amx::Cell profile_get(const std::string &);
  void profile_set(const std::string &, amx::Cell, bool save);
  amx::Cell dispatch(const std::string &, amx::Machine &);
  // All 191 reference table names. The Variables instance must outlive bindings.
  amx::NativeMap natives();
  using Method = std::function<std::optional<amx::Address>(amx::Machine &, Args &)>;
  amx::Native bind(const std::string &, Method);
  static amx::Native missing(std::string);
  // Explicit access to the same owned slots used by existing built-ins.
  std::optional<amx::Cell> argument_slot(unsigned) const;
  void argument_slot(unsigned, amx::Cell);
  void preserve_argument_slots(const std::string &);
  // External stack boundaries must not preserve a known slot. run_script manages
  // fresh/nested frames automatically; direct host dispatch can reset explicitly.
  void forget_string_slot() noexcept { string_slot.reset(); quest_string_slot.reset(); dialog_string_slot.reset(); cutscene_string_slot.reset(); menu_string_slots.clear(); }
  std::uint32_t player_record(amx::Cell) const;
  std::uint32_t mission_byte(amx::Cell, amx::Cell, amx::Cell) const;
  amx::Cell mission_available(amx::Cell, amx::Cell) const;
  amx::Cell mission_complete(amx::Cell, amx::Cell) const;
  void mission_set_available(amx::Cell, amx::Cell, amx::Cell);
  void mission_set_complete(amx::Cell, amx::Cell, amx::Cell);
  void mission_grant_medal(amx::Cell, amx::Cell);
  amx::Cell mission_get_medal(amx::Cell, amx::Cell, amx::Cell);
  void quest_add(const std::string &, const std::string &);
  void quest_remove(const std::string &);
  amx::Cell choice_get(std::uint32_t) const;
  void choice_set(std::uint32_t, amx::Cell);
private:
  // Complete scene utilities/audio handlers; external audio remains scene_service.
  amx::Cell utility_dispatch(const std::string &, amx::Machine &);
  amx::Cell interaction_dispatch(const std::string &, amx::Machine &);
  amx::Cell vehicle_dispatch(const std::string &, amx::Machine &);
  amx::Cell effects_dispatch(const std::string &, amx::Machine &);
  static std::uint32_t particle_definition(amx::Cell);
  InventoryResult inventory_call(const std::string &, const std::vector<InventoryValue> & = {});
  std::uint32_t inventory_word(const std::string &, const std::vector<InventoryValue> & = {});
  amx::Cell inventory_dispatch(const std::string &, amx::Machine &);
  amx::Cell creature_dispatch(const std::string &, amx::Machine &);
  std::vector<objects::GameObject *> creatures(const std::string &, std::int64_t = 0x200) const;
  void scale(objects::GameObject *, amx::Machine &, unsigned);
  void start_anim(std::uint32_t, const std::function<std::uint32_t()> &, amx::Cell);
  amx::Cell menu_dispatch(const std::string &, amx::Machine &);
  const std::vector<std::string> *selected_properties() const;
  std::uint32_t scene_call(const std::string &, const std::vector<ServiceValue> & = {});
  amx::Cell scene_dispatch(const std::string &, amx::Machine &);
  void refill(bool);
  std::uint32_t service(const std::string &, const std::vector<std::uint32_t> &);
  amx::Cell player_dispatch(const std::string &, amx::Machine &);
  std::string string_argument(amx::Machine &, amx::Address, std::optional<amx::Cell> &);
  std::pair<std::uint32_t, bool> slot(std::uint32_t, const std::string &) const;
  amx::Cell get(std::uint32_t, const std::string &);
};
} // namespace champions::simulation::rta::builtins
