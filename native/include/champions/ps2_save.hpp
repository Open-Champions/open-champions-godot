#pragma once
#include "champions/items.hpp"
#include "champions/script_builtins.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::ps2_save {
// SLUS-20973 US v2.00 memory-card slot and options images. Layout, mixes and
// reader rules are the pinned research at t3/research-saves 8d43286
// (docs/save-format.md, tools/ref_save.py). This is not the authored OCNSAVE
// schema (docs/NATIVE_SAVE.md). Norrath and every other profile refuse.
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
// A field or path the research leaves unknown. Thrown instead of guessing.
struct Unknown : Error {
  using Error::Error;
};

inline constexpr std::size_t FILE_SIZE = 0x4BC78;
inline constexpr std::size_t RECORD_SIZE = 0x4C30;
inline constexpr std::size_t BLOCK_SIZE = 0x4C3E;
inline constexpr std::size_t POSE_SIZE = 0x0E;
inline constexpr std::size_t CAMERA_SIZE = 0xC4;
inline constexpr std::size_t JUMP_SIZE = 0x1E4;
inline constexpr std::size_t JUMP_DEST_SIZE = 0x1E0;
inline constexpr std::size_t HEADER_SIZE = 0x70;
inline constexpr std::size_t PLAYERS_AT = 0x320;
inline constexpr std::size_t QUESTS_AT = 0x13418;
inline constexpr std::size_t DIRECTORY_AT = 0x16010;
inline constexpr std::size_t DATA_AT = 0x16118;
inline constexpr std::size_t DIRECTORY_SLOTS = 10;
inline constexpr std::size_t DIRECTORY_STRIDE = 0x18;
inline constexpr std::size_t ELEVENTH_DIRECTORY_AT = DIRECTORY_AT + DIRECTORY_SLOTS * DIRECTORY_STRIDE;
inline constexpr std::size_t QUEST_STRIDE = 0x24;
inline constexpr int PLAYER_SLOTS = 4;
inline constexpr std::size_t CONFIG_SIZE = 0x2F8;
inline constexpr std::size_t AUTOMAP_SIZE = 0x16C8;
inline constexpr std::size_t AUTOMAP_GRID_SPAN = 0x16ACE;
inline constexpr std::uint32_t ITEM_TABLE = 0x3F7650;
inline constexpr std::size_t ITEM_SIZE = 0x48;
inline constexpr std::size_t BAG_ITEM_AT = 0x130;
inline constexpr std::size_t BAG_STRIDE = 0x1208;
inline constexpr std::size_t BAG_COUNT_AT = 0x1330;
inline constexpr int BAGS = 3;
inline constexpr int BAG_CAPACITY = 64;
inline constexpr std::uint32_t COMPLETE_LEVEL_SIZE = 0x88B8;
inline constexpr std::size_t OBJECT_POINTER_AT = 0x43D8;
inline constexpr std::size_t EQUIPMENT_AT = 0x43F0;
// Live addresses save_load stores to (docs/save-format.md "Load path").
inline constexpr std::uint32_t PLAYERS = 0x4D8AF0;
inline constexpr std::uint32_t NUM_PLAYERS = 0x4D8AE0;
inline constexpr std::uint32_t DIFFICULTY = 0x4F719C;
inline constexpr std::uint32_t CURRENT_ACT = 0x88D3D4;
inline constexpr std::uint32_t PLAY_TIME = 0x88D414;
inline constexpr std::uint32_t MINIMAP = 0x4F8400;
inline constexpr std::uint32_t SPAWN_FACING = 0x4F8410;
inline constexpr std::uint32_t SPAWN_POS = 0x4F70D0;
inline constexpr std::uint32_t PENDING_LEVEL = 0x41B6D0;
inline constexpr std::uint32_t PENDING_MARKER = 0x41B720;
inline constexpr std::uint32_t LOADED_NAME = 0x8E1A98;
inline constexpr std::uint32_t LOADED_FROM_SAVE = 0x8E1AD6;
inline constexpr std::uint32_t AUTOSAVE_COUNTDOWN = 0x88D40C;
inline constexpr std::uint32_t JUMP_COUNT = 0x8E3C30;
inline constexpr std::uint32_t JUMP_DESTS = 0x8E3A50;
inline constexpr std::uint32_t SAVE_HEADER_COPY = 0x879E38;
inline constexpr std::uint32_t SHARE_QUEST_ITEMS = 0x4F85B4;
inline constexpr std::uint32_t SAVED_CONFIG = 0x4F8F38;
inline constexpr std::uint8_t PENDING_MARKER_VALUE = 0xFD;
inline constexpr std::uint32_t AUTOSAVE_AFTER_LOAD = 99;
// Bytes between the pending-level address and the marker byte.
inline constexpr std::size_t PENDING_LEVEL_ROOM = PENDING_MARKER - PENDING_LEVEL;
// The registry ID (game_profile.hpp) whose executable the research measured.
inline constexpr const char *PROFILE = "rta-us-v2.00";

inline constexpr std::array<std::uint8_t, 8> EMPTY_WORLD{{0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00}};
inline constexpr std::array<std::uint8_t, 8> EMPTY_DECAL{};

using Bytes = std::vector<std::uint8_t>;
// Floats are carried as their little-endian bits so every value is exact.
using Bits3 = std::array<std::uint32_t, 3>;

struct LiveCharacter {
  Bytes record; // 0x4c30 live bytes, pointers included
  std::uint32_t live_base = 0;
  std::uint16_t facing = 0;
  Bits3 position{{0, 0, 0}};
};

struct LevelRow {
  std::string name;
  std::uint16_t size = 0;
};
using Acts = std::vector<std::vector<LevelRow>>;
using Quests = std::vector<std::pair<std::string, std::int32_t>>;

// Everything save_build and save_storeLevelState read (ref_save.SlotInputs).
struct SlotInputs {
  std::string level_name = "crypt";
  std::string subname;
  bool game_complete = false;
  std::uint32_t play_time = 0;
  std::uint8_t difficulty = 0;
  std::uint8_t save_arg = 0;
  std::int32_t act = 0;
  bool one_player = false;
  std::int32_t num_players = 1;
  std::vector<LiveCharacter> players;
  std::int32_t alive_first = -1;
  std::optional<Bytes> camera;
  std::int32_t jump_count = 0;
  std::optional<Bytes> jump_dest;
  std::uint32_t minimap = 0;
  Quests quests;
  Acts acts;
  std::int32_t memcard_state = 0;
  std::uint32_t block_flag = 0;
  Bytes automap_grid;
  std::optional<Bytes> previous;
  std::optional<Bytes> world_body;
  std::optional<Bytes> decal_body;
};

struct Item {
  std::int32_t definition_index = 0;
  std::int16_t quantity = 0;
  Bytes raw;
};

struct Character {
  std::string name;
  std::array<std::int32_t, 4> appearance{{0, 0, 0, 0}};
  std::int32_t class_index = 0;
  std::int32_t model_index = 0;
  std::int32_t acknowledged_level = 0;
  std::int32_t level = 0;
  std::int32_t experience = 0;
  std::array<std::int32_t, 4> base_stats{{0, 0, 0, 0}}; // str, int, dex, sta
  std::int32_t gold = 0;
  std::int32_t skill_points = 0;
  std::int32_t stat_points = 0;
  std::array<std::int32_t, 48> passives{};
  std::array<std::vector<Item>, 3> bags;
  std::array<std::int32_t, 17> equipment{};
  std::array<std::int32_t, 2> weapon_sets{{0, 0}};
  std::int32_t offhand = 0;
  std::vector<std::pair<std::int16_t, std::int16_t>> skills;
  Bytes mission_flags;
  Bytes medals;
  std::uint8_t evil_bits = 0;
  std::uint8_t decision_bits = 0;
  std::uint8_t kind = 0;
  std::uint16_t facing = 0;
  Bits3 position{{0, 0, 0}};
  std::uint32_t object_pointer = 0;
  Bytes raw;
};

struct LevelBlob {
  std::string name;
  std::uint32_t size = 0;
  Bytes data;
  std::optional<Bytes> world;
  std::optional<Bytes> decal;
  Bytes automap;
  std::string note; // framing that does not fit, as ref_save reports it
  // Empty when the body is the documented empty world or decal. Otherwise the
  // reason that body is not decoded.
  std::string world_refused;
  std::string decal_refused;
};

struct SlotFile {
  std::string level_name;
  std::string subname;
  std::uint32_t play_time = 0;
  std::uint16_t record_size = 0;
  std::uint16_t listed_count = 0;
  std::int32_t player_count = 0;
  std::uint8_t difficulty = 0;
  std::uint8_t save_arg = 0;
  std::int16_t act = 0;
  Bytes camera;
  std::int32_t jump_count = 0;
  Bytes jump_destinations;
  std::uint32_t minimap_mode = 0;
  std::vector<Character> characters;
  Quests quests;
  std::vector<LevelBlob> levels;
  Bytes raw;
};

struct ConfigFile {
  std::uint32_t brightness = 0; // float bits
  std::uint32_t music = 0;      // float bits
  std::uint32_t effects = 0;    // float bits
  std::uint32_t stereo = 0;
  std::uint32_t vibration = 0;
  std::uint32_t subtitles = 0;
  std::uint32_t damage_numbers = 0;
  std::uint32_t blood = 0;
  std::uint32_t markers = 0;
  std::uint32_t combat_camera = 0;
  std::uint32_t reverse_yaw = 0;
  std::uint32_t unnamed_18 = 0;
  std::array<std::array<std::uint32_t, 9>, 4> buttons{};
  Bytes voice; // +0xc4..+0xd7, pairing with the voice globals unknown
  Quests profile;
  Bytes raw;
};

// Layout helpers and builders, one to one with tools/ref_save.py.
std::uint32_t round_reserved(std::uint32_t original, std::uint32_t automap_half);
Bytes pack_automap(const Bytes &grid);
Bytes frame_level_state(const Bytes &world_body, const Bytes &decal_body, const Bytes &automap);
Bytes pack_camera(const Bits3 &position, const std::vector<std::uint32_t> &matrix,
                  std::uint16_t half_68, std::uint16_t half_6a, std::uint32_t word_54,
                  std::uint32_t word_58, const Bits3 &floats, const Bytes &tail);
Bytes pack_jump(std::int32_t count, const Bytes &destinations);
std::string qualified_name(const std::string &level_name, const std::string &subname);
int find_act(const std::string &name, int current, const Acts &acts);
int level_index(const std::string &name, int act, const Acts &acts);

std::int32_t definition_index(std::uint32_t pointer);
std::uint32_t definition_pointer(std::int32_t index);
std::uint32_t item_address(std::uint32_t record_base, int bag, int index);
std::int32_t pointer_to_index(std::uint32_t pointer, std::uint32_t record_base,
                              const std::array<std::int32_t, 3> &counts);
std::uint32_t index_to_pointer(std::int32_t index, std::uint32_t record_base,
                               const std::array<std::int32_t, 3> &counts);
Bytes encode_saved_record(const Bytes &live, std::uint32_t live_base);
Bytes decode_loaded_record(const Bytes &saved, std::uint32_t live_base);

// player_checksum 0x1bdc98, the netplay mix. Not stored in the file and not
// read by save_load. The file itself has no checksum (see file_checksum()).
std::uint32_t mix_bytes(const std::uint8_t *data, std::size_t size);
std::uint32_t player_checksum(int slot, std::uint32_t address, const Bytes &record);

Bytes build_slot(const SlotInputs &inputs);
Bytes set_player_count(const Bytes &image, std::int32_t count);
// strict is the reader's refusal list (docs/save-format.md), plus the native
// rule that the unexecuted eleventh directory stride at +0x16100 is zero.
SlotFile parse_slot(const Bytes &data, bool strict = true);
Character import_character(const Bytes &data, int index = 0, bool strict = true);

Bytes config_set_defaults(int which = -1, int scheme = 1, const Bytes *image = nullptr);
ConfigFile parse_config(const Bytes &data, bool strict = true);

// What save_load 0x221d60 writes, applied to native script memory. The camera
// is refused (its load is not documented) and listed in the returned notes;
// a set share-quest-items word refuses before any write. The slot is parsed
// strictly first, so nothing is written for an image the reader refuses.
std::vector<std::string> apply_load(builtins::Variables &variables, const Bytes &data);

// One imported character written into the native player record, inventory
// words and quest table by apply_load, plus the items::Player view of it.
struct ImportedCharacter {
  std::shared_ptr<builtins::Memory> memory;
  std::unique_ptr<builtins::Variables> variables;
  int index = 0;
  std::uint32_t record_address = 0;
  items::Player player;
  Character character;
  SlotFile slot;
  std::uint32_t checksum = 0; // player_checksum of the live record after load
  Quests live_quests;
  std::vector<std::string> refusals;
};

// profile must be PROFILE. expected_checksum, when set, is compared with the
// netplay mix of the record save_load would leave at g_players[index]; a
// mismatch refuses before any native write. Omitting it does not invent one.
ImportedCharacter import_into(const Bytes &data, int index = 0, const std::string &profile = PROFILE,
                              std::optional<std::uint32_t> expected_checksum = std::nullopt);
// The character's bag items as native items, from the live record in memory.
// A definition the tables do not hold refuses.
std::array<std::vector<items::Item>, 3> inventory(const items::Tables &tables,
                                                  const ImportedCharacter &imported);

// The options image is a raw g_savedConfig. Writes the 760 bytes there, which
// places the profile table at PROFILE_VARS. Which other globals a loaded image
// reaches is not documented, so nothing else is written.
void apply_config(builtins::Memory &memory, const Bytes &data);

// Explicit refusals for paths the research left unknown. These do not guess.
[[noreturn]] void file_checksum();
[[noreturn]] void voice_global_pairing();
[[noreturn]] void config_open_name();
[[noreturn]] void apply_camera();
[[noreturn]] void eleventh_directory_name();
void require_known_world(const LevelBlob &);
void require_known_decal(const LevelBlob &);
// Documented option names. Unknown offsets, including the voice words, throw.
std::string config_field_name(std::size_t offset);
void require_profile(const std::string &profile);
} // namespace champions::simulation::rta::ps2_save
