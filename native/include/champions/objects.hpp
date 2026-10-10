#pragma once
#include "champions/core_state.hpp"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace champions::simulation::rta::objects {
using Vec3 = std::array<Raw, 3>;
using Bytes = std::vector<std::uint8_t>;
using Props = std::shared_ptr<std::vector<std::string>>;
inline constexpr std::size_t GRID_DIM = 120, MAX_TASKS = 32,
                             MAX_RETAINED = 65536, MAX_WORK = 1000000,
                             MAX_BYTES = 16 * 1024 * 1024;
inline constexpr std::uint32_t G_RUN_ACTIVE = 0x5db3a8, G_RUN_15 = 0x5db3b0,
                               G_RUN_60 = 0x5db3f0, G_DELETE_LIST = 0x5db4e0,
                               G_ALWAYS_DRAW = 0x5db4e8, G_GRID = 0x5db4f0,
                               G_GRID_B = 0x5e95f0, G_OFF_GRID = 0x5f76f0,
                               VT_GAMEOBJECT = 0x5122a8, VT_PLAYER1 = 0x512028;
inline constexpr std::uint32_t FLAG_NOT_SOLID = 1, FLAG_NOT_GRID_DRAWN = 2,
                               FLAG_DEAD = 8, FLAG_GRID_B = 0x800,
                               FLAG_EXPENDABLE = 0x1000, FLAG_NOT_RUN = 0x8000,
                               FLAG_PREDICTION_COPY = 0x20000,
                               FLAG_FAR_ACTIVE = 0x80000,
                               FLAG_OWN_PLAYER = 0x200000;
struct ObjectError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : ObjectError {
  using ObjectError::ObjectError;
};
std::int32_t cell_coord(Raw);
std::optional<std::string> props_find(const Props &, const std::string &,
                                      std::int64_t nth = 0);
std::int32_t atoi(const std::string &);
std::int32_t props_int(const Props &, const std::string &,
                       std::int64_t nth = 0);
std::uint32_t name_hash(const std::string &);
std::string strip_quotes(const std::string &);
// Shared owners intentionally preserve Python aliasing. Entry names/container
// must remain stable during resource callbacks; resolved values and lump may
// change.
struct ClassDef {
  std::string lump;
  std::int64_t generation = 0;
  std::vector<std::pair<std::string, std::uint32_t>> entries;
  ClassDef(std::string lump = {}, const std::vector<std::string> &names = {},
           std::int64_t generation = 0);
};
struct GameObject : rta::Object {
  const std::uint32_t addr;
  const std::uint64_t serial;
  std::array<Cell, 3> heads;
  Vec3 pos{};
  Raw radius = 0x41c00000, height = 0x42900000;
  std::shared_ptr<ClassDef> class_def;
  std::int64_t draw_half_width = 25, draw_height = 73;
  Props props;
  Raw vz = 0;
  std::optional<std::string> class_name;
  std::uint32_t angle = 0, seed = 0, vtable = VT_GAMEOBJECT;
  std::string type_name = "10GameObject";
  bool alive = true;
  std::optional<std::int64_t> tag;
  explicit GameObject(std::uint32_t address, std::uint64_t identity = 0);
  std::uint32_t rand();
};
struct NameTable {
  std::array<std::optional<std::string>, 512> keys{};
  std::array<GameObject *, 512> objs{};
  std::int64_t count = 0;
  void clear();
  std::size_t add(const std::string &, GameObject *);
  std::optional<std::size_t> remove(const std::string &, GameObject *);
  std::vector<GameObject *> find(const std::string &, std::int64_t limit) const;
  GameObject *find_first(const std::string &) const;
};
struct Task {
  std::uint32_t fn;
  std::int64_t priority;
  std::string name;
  std::int64_t flags;
};
struct TaskList {
  std::vector<Task> tasks;
  std::int64_t cursor = 0;
  std::optional<std::size_t> add(std::uint32_t, std::int64_t,
                                 const std::string &, std::int64_t);
  std::optional<std::size_t> remove(std::uint32_t);
  bool is_running(const std::string &) const;
  void call(std::int64_t exclude, std::int64_t include,
            const std::function<void(std::uint32_t)> &invoke);
};
struct Pad {
  std::int64_t held = 0, pressed = 0, edge2 = 0;
};
struct Record {
  std::optional<std::string> class_name;
  std::int32_t angle;
  Vec3 pos;
  Props props;
  Bytes state;
  Record(std::optional<std::string>, std::int64_t, Vec3, Props = {},
         Bytes = {});
};
// Records and their fields remain stable during load callbacks; property
// contents may change in place. Unsupported structural mutation is refused
// after the call.
struct RecordBlob {
  std::vector<Record> records;
  bool has_state = false, relocated = false;
  std::optional<std::size_t> cached_size;
  explicit RecordBlob(std::vector<Record> records = {}, bool has_state = false);
  std::size_t size();
  Bytes build();
};
using Factory = std::function<GameObject *(const std::optional<std::string> &,
                                           Vec3, std::int64_t, Props)>;
using Terrain = std::function<Raw(Raw, const Vec3 &)>;
GameObject *create_object(const std::optional<std::string> &, Vec3,
                          std::int64_t, Props, const Factory &);
enum class CallbackPolicy { RequireHandlers, ReferenceBase };
// Stable integer references: cell/node virtual address; object heads use
// (allocation serial << 32)|virtual address. Object references use serial.
// Iterator references use bit63 | creation serial. Zero denotes null.
struct LinkSnapshot {
  std::uint64_t id = 0, next = 0, prev = 0, partner = 0, object = 0;
  LinkKind kind = LinkKind::Cell;
  std::uint32_t tag = 0;
};
struct ObjectSnapshot {
  std::uint64_t serial = 0, props_id = 0, class_def_id = 0;
  std::uint32_t addr = 0, flags = 0;
  Vec3 pos{};
  Raw radius = 0, height = 0, vz = 0;
  std::uint32_t angle = 0, seed = 0, vtable = 0;
  std::int64_t draw_half_width = 0, draw_height = 0;
  std::optional<std::string> class_name;
  std::string type_name;
  bool alive = false;
  std::optional<std::int64_t> tag;
  std::optional<std::vector<std::string>> props;
  std::optional<ClassDef> class_def;
};
struct WorldSnapshot {
  Pool pool;
  std::uint32_t shared_seed = 0;
  std::uint64_t handle_free = 0, iterator_serial = 0;
  std::vector<LinkSnapshot> links;
  std::vector<ObjectSnapshot>
      objects; // Includes retired instances, serial order.
  std::map<std::uint32_t, std::uint64_t> live;
  std::array<std::optional<std::string>, 512> name_keys{};
  std::array<std::uint64_t, 512> name_objects{};
  std::int64_t name_count = 0;
  TaskList tasks;
  std::int64_t cursor15 = 0, cursor60 = 0, frame = 0, network_mode = 0,
               cull_by_camera = 0, pool_free_count = 0, tearing_down = 0,
               lump_generation = 0, menu_lockout = 0, menu_lockout_hold = 0,
               warmup_frames = 0, start_rank = 0;
  std::uint32_t exclude_mask = 0;
  Vec3 camera_focus{};
  std::vector<Vec3> centres;
  std::array<Pad, 4> pads{};
  std::array<Vec3, 4> spawn_pos{};
  std::array<std::uint32_t, 4> spawn_facing{};
  CallbackPolicy callback_policy = CallbackPolicy::RequireHandlers;
  std::array<bool, 5> callbacks{}; // run,destroy,frozen,res_find,end_tick
};
// Fixed heap allocation avoids the Windows default stack limit and keeps cells
// stable.
class CellGrid {
  std::unique_ptr<std::array<Cell, GRID_DIM * GRID_DIM>> cells_;

public:
  CellGrid()
      : cells_(std::make_unique<std::array<Cell, GRID_DIM * GRID_DIM>>()) {}
  Cell &operator[](std::size_t n) { return (*cells_)[n]; }
  const Cell &operator[](std::size_t n) const { return (*cells_)[n]; }
  auto begin() { return cells_->begin(); }
  auto end() { return cells_->end(); }
  auto begin() const { return cells_->begin(); }
  auto end() const { return cells_->end(); }
  constexpr std::size_t size() const { return GRID_DIM * GRID_DIM; }
};
// Callables are copied before invocation so self-replacement is safe. Mutable
// gameplay state belongs in World or reference-captured owned state, not
// mutable by-value functor captures. Callback code/captures and the
// caller-selected core FPU context are outside WorldSnapshot (traces record the
// FPU configuration).
class World {
  std::vector<std::unique_ptr<GameObject>> storage_;
  std::map<const rta::Object *, GameObject *> owned_objects_;
  GameObject &checked_object(rta::Object *) const;
  std::map<std::uint64_t, std::unique_ptr<Iterator>> iterators_;
  std::uint64_t iterator_serial_ = 0;
  std::set<std::uint64_t> completed_iterators_;
  void prune_iterators();
  std::map<std::uint32_t, std::unique_ptr<Cell>> extra_cells_;
  void walk_list(Cell &, std::uint32_t,
                 const std::function<void(GameObject &)> &);
  void call_run(GameObject &);
  void call_destroy(GameObject &, std::int64_t);
  bool frozen();
  void finish_tick();
  Link *grid_node(GameObject &);
  void leave_run_list(GameObject &);

public:
  Pool pool;
  HandlePool handles;
  SharedRandom shared;
  CellGrid grid, grid_b;
  Cell off_grid, always_draw, delete_list, run_active;
  std::array<Cell, 15> run15;
  std::array<Cell, 60> run60;
  std::int64_t cursor15 = 0, cursor60 = 0, frame = 0, network_mode = 0,
               cull_by_camera = 1, pool_free_count = 100000, tearing_down = 0,
               lump_generation = 1;
  std::uint32_t exclude_mask = 0;
  Vec3 camera_focus{};
  std::vector<Vec3> centres;
  NameTable names;
  TaskList tasks;
  std::map<std::uint32_t, GameObject *> objects;
  std::function<void(GameObject &)> on_run;
  std::function<void(GameObject &, std::int64_t)> on_destroy;
  std::function<bool()> is_frozen;
  std::function<std::uint32_t(const std::string &, const std::string &)>
      res_find;
  std::function<void()> end_of_tick;
  std::array<Pad, 4> pads{};
  std::int64_t menu_lockout = 0, menu_lockout_hold = 0, warmup_frames = 0,
               start_rank = 0;
  std::array<Vec3, 4> spawn_pos{};
  std::array<std::uint32_t, 4> spawn_facing{};
  CallbackPolicy callback_policy;
  explicit World(CallbackPolicy = CallbackPolicy::RequireHandlers);
  ~World();
  World(const World &) = delete;
  World &operator=(const World &) = delete;
  World(World &&) = delete;
  World &operator=(World &&) = delete;
  Cell &make_cell(std::uint32_t virtual_id);
  void init();
  Cell &home_cell(const GameObject &, bool honour_flag2);
  GameObject *construct(std::uint32_t, const Vec3 &,
                        std::shared_ptr<ClassDef> = {}, Props = {},
                        Props stale_props = {});
  GameObject *new_object(std::uint32_t, const Vec3 &,
                         std::shared_ptr<ClassDef> = {}, Props = {});
  void destroy(GameObject &, std::int64_t flags = 3);
  void delete_object(GameObject &);
  void flush_delete_list();
  void link_to_grid(GameObject &);
  void update_grid_cell(GameObject &);
  void move_off_grid(GameObject &);
  bool is_off_grid(const GameObject &);
  void add_to_always_draw(GameObject &);
  void add_to_always_draw_once(GameObject &);
  void clear_always_draw();
  void stop_running(GameObject &);
  void run_every_frame(GameObject &);
  void run_every_60(GameObject &);
  void run_every_15(GameObject &);
  int is_active(const GameObject &, std::int64_t radius) const;
  int update_active(GameObject &, std::int64_t was_active, std::int64_t radius);
  std::vector<GameObject *> collect_in_box(const Vec3 &, const Vec3 &,
                                           std::int64_t limit);
  std::vector<GameObject *> collect_in_sphere(Raw, const Vec3 &, Cell &);
  std::vector<GameObject *> find_by_class(const std::string &,
                                          std::int64_t limit);
  std::array<std::size_t, 16> reap_cells() const;
  std::vector<GameObject *> reap();
  void run_objects();
  void for_each_grid_object(const std::function<void(GameObject &)> &);
  void for_each_run_object(const std::function<void(GameObject &)> &);
  void delete_all_objects();
  void tasks_run(std::int64_t, const std::function<void(std::uint32_t)> &);
  void set_object_vis(GameObject *, std::int64_t camera_mode, bool cutscene,
                      const std::vector<GameObject *> &);
  int is_near_view(const Vec3 &, Raw, const std::vector<GameObject *> &) const;
  static std::tuple<std::vector<std::size_t>, std::vector<std::size_t>,
                    std::int64_t>
  alive_players(const std::vector<GameObject *> &,
                const std::function<bool(std::size_t)> &);
  std::pair<Vec3, int> find_free_spot(const Vec3 &, std::int64_t spread,
                                      std::int64_t radius, bool any_height,
                                      const Terrain &, const Terrain &);
  GameObject *player1_marker(std::uint32_t, const Vec3 &, std::int64_t, Props,
                             const std::string &prev_level,
                             const std::string &entrance, bool gate,
                             const Terrain &, const Terrain &);
  std::size_t load_records(
      RecordBlob &, const Factory &, bool filter_only = false,
      const std::function<bool(const std::optional<std::string> &)>
          &save_filter = {},
      const std::function<void(GameObject &, const Bytes &)> &load_state = {});
  WorldSnapshot snapshot() const;
};
} // namespace champions::simulation::rta::objects
