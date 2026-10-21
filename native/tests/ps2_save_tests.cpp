// Synthetic PS2 memory-card slot and options images. No real save, disc or card
// file. Scenario builders mirror tools/test_ref_save.py and tools/difftest_save.py
// at t3/research-saves 8d43286; ps2_save_vectors.inc holds what ref_save produced.
#include "champions/ps2_save.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>

using namespace champions::simulation::rta;
using namespace champions::simulation::rta::ps2_save;

namespace {
unsigned checks = 0;
#define CHECK(...)                                                                                 \
  do {                                                                                             \
    ++checks;                                                                                      \
    if (!(__VA_ARGS__))                                                                            \
      throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " + #__VA_ARGS__); \
  } while (false)
template <class E = Error, class F> void refuses(F f) {
  bool caught = false;
  try {
    f();
  } catch (const E &) {
    caught = true;
  }
  CHECK(caught);
}

struct ImageVector {
  const char *name;
  std::size_t size;
  std::uint64_t fnv;
};
struct ChecksumVector {
  const char *name;
  int slot;
  std::uint32_t address;
  std::uint32_t seed;
  std::uint32_t expected;
};
#include "ps2_save_vectors.inc"

std::uint64_t fnv(const Bytes &data) {
  std::uint64_t h = 0xCBF29CE484222325ull;
  for (auto b : data) {
    h ^= b;
    h *= 0x100000001B3ull;
  }
  return h;
}
Bytes lcg_bytes(std::uint32_t x, std::size_t size) {
  Bytes out(size);
  for (auto &b : out) {
    x = x * 1103515245u + 12345u;
    b = static_cast<std::uint8_t>(x >> 16);
  }
  return out;
}
std::uint32_t f(float v) {
  std::uint32_t r;
  std::memcpy(&r, &v, 4);
  return r;
}
Bits3 fv(float x, float y, float z) { return {{f(x), f(y), f(z)}}; }
void put32(Bytes &b, std::size_t o, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b[o + i] = std::uint8_t(v >> (8 * i));
}
void put16(Bytes &b, std::size_t o, std::uint32_t v) {
  b[o] = std::uint8_t(v);
  b[o + 1] = std::uint8_t(v >> 8);
}
std::uint32_t get32(const Bytes &b, std::size_t o) {
  return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | std::uint32_t(b[o + 3]) << 24;
}
void name_into(Bytes &rec, const std::string &name) {
  std::copy(name.begin(), name.end(), rec.begin());
  rec[name.size()] = 0;
}

// test_ref_save._record
Bytes ref_record(const std::string &name = "Aria", std::uint32_t live_base = 0x4D8AF0,
                 std::uint32_t object_addr = 0x01100000) {
  Bytes rec(RECORD_SIZE, 0);
  name_into(rec, name);
  for (int i = 0; i < 4; ++i)
    rec[0x20 + i] = std::uint8_t(i + 1);
  put32(rec, 0x50, 1);
  put32(rec, 0x54, 6);
  rec[0x58] = 3;
  rec[0x59] = 4;
  put32(rec, 0x5C, 5000);
  put32(rec, 0x78, 46);
  put32(rec, 0x7C, 37);
  put32(rec, 0x80, 24);
  put32(rec, 0x84, 28);
  put32(rec, 0xE4, 777);
  put32(rec, 0xE8, 2);
  rec[0xEC] = 5;
  rec[0xEF] = 1;
  put32(rec, BAG_COUNT_AT, 1);
  put32(rec, BAG_ITEM_AT, definition_pointer(3));
  put16(rec, BAG_ITEM_AT + 4, 4);
  put32(rec, 0x43D8, object_addr);
  put32(rec, 0x43F0, live_base + BAG_ITEM_AT);
  put32(rec, 0x4434, live_base + BAG_ITEM_AT);
  put16(rec, 0x4B08, 7);
  put16(rec, 0x4B0A, 2);
  put16(rec, 0x4B0C, 0xFFFF);
  rec[0x4C1A] = 3;
  rec[0x4C1B] = 1;
  rec[0x4C1C] = 1;
  rec[0x4BBF + 13] = 3;
  return rec;
}
Bytes ref_camera() {
  std::vector<std::uint32_t> matrix(16);
  for (std::uint32_t i = 0; i < 16; ++i)
    matrix[i] = i; // test_ref_save passes ints, which are raw bits
  Bytes tail(0x60);
  for (std::size_t i = 0; i < tail.size(); ++i)
    tail[i] = std::uint8_t(i);
  return pack_camera(fv(10, 20, 30), matrix, 1, 2, 3, 4, fv(0.25f, 0.5f, 0.75f), tail);
}
// test_ref_save._inputs
SlotInputs ref_inputs() {
  SlotInputs in;
  in.level_name = "crypt";
  in.play_time = 12345;
  in.difficulty = 2;
  in.save_arg = 7;
  in.num_players = 1;
  in.players = {LiveCharacter{ref_record(), 0x4D8AF0, 0x1234, fv(1, 2, 3)}};
  in.minimap = 3;
  in.jump_count = 2;
  Bytes dest(JUMP_DEST_SIZE, 0);
  dest[0] = 0xAB;
  in.jump_dest = dest;
  in.quests = {{"opened", 7}, {"door", -2}};
  in.acts = {{{"crypt", 0x2000}}};
  in.camera = ref_camera();
  return in;
}
LiveCharacter ref_bram(std::uint32_t object = 0x01100200, std::uint16_t facing = 9, Bits3 pos = fv(4, 5, 6)) {
  const auto base = 0x4D8AF0 + std::uint32_t(RECORD_SIZE);
  return LiveCharacter{ref_record("Bram", base, object), base, facing, pos};
}

// difftest_save.make_record, _players, camera_parts and _common
constexpr std::uint32_t G_PLAYERS = 0x4D8AF0, G_SAVED = 0x879EC0, OBJ_BASE = 0x01100000;
Bytes diff_record(const std::string &name, std::int32_t gold, std::uint32_t live_base, std::uint32_t obj) {
  Bytes rec(RECORD_SIZE, 0);
  std::copy(name.begin(), name.end(), rec.begin());
  rec[0x20] = 4;
  rec[0x21] = 1;
  rec[0x22] = 2;
  rec[0x23] = 3;
  put32(rec, 0x50, 1);
  put32(rec, 0x54, 6);
  rec[0x58] = 3;
  rec[0x59] = 4;
  put32(rec, 0x5C, 5000);
  put32(rec, 0x78, 46);
  put32(rec, 0x7C, 37);
  put32(rec, 0x80, 24);
  put32(rec, 0x84, 28);
  put32(rec, 0xE4, std::uint32_t(gold));
  put32(rec, 0xE8, 2);
  for (int i = 0; i < 24; ++i)
    rec[0x90 + i] = std::uint8_t(i);
  put32(rec, 0xC0, f(10.0f));
  put32(rec, 0xC4, f(20.0f));
  put32(rec, 0xC8, f(1.5f));
  rec[0x3748] = 0x5A;
  rec[0x3749] = 0xA5;
  put32(rec, BAG_COUNT_AT, 1);
  put32(rec, BAG_ITEM_AT, definition_pointer(3));
  put16(rec, BAG_ITEM_AT + 4, 4);
  put32(rec, 0x43D8, obj);
  put32(rec, 0x43F0, live_base + BAG_ITEM_AT);
  put32(rec, 0x4434, live_base + BAG_ITEM_AT);
  put16(rec, 0x4B08, 7);
  put16(rec, 0x4B0A, 2);
  put16(rec, 0x4B0C, 0xFFFF);
  return rec;
}
Bytes diff_camera() {
  std::vector<std::uint32_t> matrix(16);
  for (int i = 0; i < 16; ++i)
    matrix[std::size_t(i)] = f(float(i));
  Bytes tail(0x60);
  for (std::size_t i = 0; i < tail.size(); ++i)
    tail[i] = std::uint8_t(i);
  return pack_camera(fv(10, 20, 30), matrix, 1, 2, 3, 4, fv(0.25f, 0.5f, 0.75f), tail);
}
struct Common {
  std::string level = "crypt", sub;
  bool complete = false;
  int players = 1;
  bool one = false;
  Quests quests;
  std::optional<Acts> acts;
  std::optional<Bytes> previous;
  int card = 0;
  std::uint32_t flag = 0;
  int alive = -1;
  Bytes grid;
};
SlotInputs common(const Common &c) {
  static const char *names[] = {"Aria", "Bram", "Cedric", "Dain"};
  static const std::int32_t golds[] = {777, 50, 1, 1};
  SlotInputs in;
  in.level_name = c.level;
  in.subname = c.sub;
  in.game_complete = c.complete;
  in.play_time = 12345;
  in.difficulty = 2;
  in.save_arg = 7;
  in.act = 0;
  in.one_player = c.one;
  in.num_players = c.players;
  for (int slot = 0; slot < c.players; ++slot) {
    const auto base = G_PLAYERS + std::uint32_t(slot) * std::uint32_t(RECORD_SIZE);
    in.players.push_back({diff_record(names[slot], golds[slot], base, OBJ_BASE + std::uint32_t(slot) * 0x200), base,
                          std::uint16_t(0x1234 + slot), fv(1.0f + float(slot), 2, 3)});
  }
  in.alive_first = c.alive;
  in.camera = diff_camera();
  in.jump_count = 2;
  Bytes dest(JUMP_DEST_SIZE, 0);
  dest[0] = 0xAB;
  dest[1] = 0xCD;
  in.jump_dest = dest;
  in.minimap = 3;
  in.quests = c.quests;
  in.acts = c.acts ? *c.acts : Acts{{{"crypt", 0x2000}}};
  in.memcard_state = c.card;
  in.block_flag = c.flag;
  in.automap_grid = c.grid;
  in.previous = c.previous;
  return in;
}
Bytes write_dir(Bytes previous, const std::string &name, std::uint32_t size) {
  std::copy(name.begin(), name.end(), previous.begin() + DIRECTORY_AT);
  previous[DIRECTORY_AT + name.size()] = 0;
  put32(previous, DIRECTORY_AT + 0x14, size);
  return previous;
}
Bytes does_not_fit_previous() {
  Bytes previous(FILE_SIZE, 0);
  for (int i = 0; i < 4; ++i)
    previous[DATA_AT + std::size_t(i)] = 0x22;
  return write_dir(previous, "old", 0x40000);
}
Bytes complete_previous() {
  Bytes previous(FILE_SIZE, 0);
  for (std::size_t i = 13; i < 0x14; ++i)
    previous[i] = 0x11;
  return previous;
}
Bytes test_grid() {
  Bytes grid(AUTOMAP_GRID_SPAN, 0);
  grid[1] = 0xF0;
  grid[5] = 0x17;
  grid[0x361] = 0x80;
  return grid;
}

std::map<std::string, std::function<Bytes()>> scenarios() {
  std::map<std::string, std::function<Bytes()>> s;
  s["ref.crypt"] = [] { return build_slot(ref_inputs()); };
  s["ref.one_player"] = [] {
    auto in = ref_inputs();
    in.num_players = 2;
    in.one_player = true;
    in.players.push_back(ref_bram());
    in.memcard_state = 4;
    return build_slot(in);
  };
  s["ref.hub"] = [] {
    auto in = ref_inputs();
    in.level_name = "hub";
    in.subname = "west";
    in.acts = {{{"town", 0x1800}, {"hub", 0x1000}}};
    return build_slot(in);
  };
  s["ref.game_complete"] = [] {
    auto in = ref_inputs();
    in.game_complete = true;
    in.previous = complete_previous();
    return build_slot(in);
  };
  s["ref.does_not_fit"] = [] {
    auto in = ref_inputs();
    in.previous = does_not_fit_previous();
    return build_slot(in);
  };
  s["ref.pose_fallback"] = [] {
    auto in = ref_inputs();
    in.memcard_state = 4;
    in.num_players = 2;
    in.players = {LiveCharacter{ref_record("Aria", 0x4D8AF0, 0), 0x4D8AF0, 1, {{9, 9, 9}}},
                  ref_bram(0x01100200, 0x2222, fv(8, 7, 6))};
    return build_slot(in);
  };
  s["ref.set_count"] = [] {
    auto in = ref_inputs();
    in.num_players = 2;
    in.one_player = true;
    in.memcard_state = 4;
    in.players.push_back(in.players[0]);
    return set_player_count(build_slot(in), 1);
  };
  s["ref.saved_record"] = [] { return encode_saved_record(ref_record(), 0x4D8AF0); };
  s["ref.loaded_record"] = [] { return decode_loaded_record(encode_saved_record(ref_record(), 0x4D8AF0), 0x4D8AF0); };
  s["diff.build.crypt"] = [] {
    Common c;
    c.quests = {{"opened", 7}, {"door", -2}};
    c.grid = test_grid();
    c.players = 2;
    return build_slot(common(c));
  };
  s["diff.build.unknown_level"] = [] {
    Common c;
    c.level = "nope";
    c.previous = Bytes(FILE_SIZE, 0x5A);
    return build_slot(common(c));
  };
  s["diff.build.tail_left_in_place"] = [] {
    Common c;
    c.card = 4;
    c.previous = Bytes(FILE_SIZE, 0x11);
    c.quests = {{"opened", 7}};
    return build_slot(common(c));
  };
  s["diff.build.game_complete"] = [] {
    Common c;
    c.complete = true;
    c.previous = complete_previous();
    return build_slot(common(c));
  };
  s["diff.build.one_player"] = [] {
    Common c;
    c.players = 2;
    c.one = true;
    c.card = 4;
    return build_slot(common(c));
  };
  s["diff.build.card_busy"] = [] {
    Common c;
    c.card = 4;
    return build_slot(common(c));
  };
  s["diff.build.flag_blocks_store"] = [] {
    Common c;
    c.flag = 0x800;
    return build_slot(common(c));
  };
  s["diff.build.level_does_not_fit"] = [] {
    Common c;
    c.previous = does_not_fit_previous();
    return build_slot(common(c));
  };
  s["diff.build.pose_fallback"] = [] {
    SlotInputs in;
    in.level_name = "crypt";
    in.acts = {{{"crypt", 0x2000}}};
    in.num_players = 2;
    const auto other_base = G_PLAYERS + std::uint32_t(RECORD_SIZE);
    in.players = {LiveCharacter{diff_record("Aria", 777, G_PLAYERS, 0), G_PLAYERS, 1, {{9, 9, 9}}},
                  LiveCharacter{diff_record("Bram", 50, other_base, OBJ_BASE + 0x200), other_base, 0x2222, fv(8, 7, 6)}};
    in.alive_first = -1;
    in.camera = diff_camera();
    in.jump_count = 0;
    in.jump_dest = Bytes(JUMP_DEST_SIZE, 0);
    in.play_time = 1;
    in.difficulty = 1;
    in.save_arg = 1;
    in.memcard_state = 4;
    return build_slot(in);
  };
  s["diff.build.hub_row0"] = [] {
    Common c;
    c.level = "hub";
    c.sub = "west";
    c.acts = Acts{{{"town", 0x1800}, {"hub", 0x1000}}};
    return build_slot(common(c));
  };
  s["diff.count.both_fields"] = [] {
    Common c;
    c.players = 2;
    c.one = true;
    c.card = 4;
    return set_player_count(build_slot(common(c)), 1);
  };
  s["diff.load.record"] = [] {
    Common c;
    c.quests = {{"opened", 7}, {"door", -2}};
    c.players = 2;
    c.grid = Bytes(8, 0);
    const auto image = build_slot(common(c));
    return decode_loaded_record(Bytes(image.begin() + PLAYERS_AT + POSE_SIZE, image.begin() + PLAYERS_AT + BLOCK_SIZE),
                                G_PLAYERS);
  };
  s["diff.camera"] = [] { return diff_camera(); };
  s["automap.pattern"] = [] {
    Bytes grid(AUTOMAP_GRID_SPAN, 0);
    grid[1] = 0xF0;
    grid[5] = 0x07;
    grid[9] = 0x10;
    grid[13] = 0xA0;
    grid[0x361] = 0x20;
    grid[0x365] = 0x30;
    return pack_automap(grid);
  };
  s["automap.lcg"] = [] { return pack_automap(lcg_bytes(0x16865C, AUTOMAP_GRID_SPAN)); };
  s["automap.short"] = [] { return pack_automap(lcg_bytes(7, 0x400)); };
  s["config.scheme1"] = [] { return config_set_defaults(-1, 1); };
  s["config.scheme0"] = [] { return config_set_defaults(-1, 0); };
  s["config.one_row"] = [] {
    const Bytes dirty(CONFIG_SIZE, 0xAB);
    return config_set_defaults(1, 2, &dirty);
  };
  s["config.scheme2_kept"] = [] {
    const auto image = lcg_bytes(0x2F8, CONFIG_SIZE);
    return config_set_defaults(-1, 2, &image);
  };
  return s;
}

void reference_vectors() {
  const auto all = scenarios();
  std::size_t seen = 0;
  for (const auto &v : IMAGE_VECTORS) {
    const auto found = all.find(v.name);
    if (found == all.end())
      throw std::runtime_error(std::string("no scenario for vector ") + v.name);
    const auto bytes = found->second();
    if (bytes.size() != v.size || fnv(bytes) != v.fnv)
      throw std::runtime_error(std::string("vector ") + v.name + " differs from ref_save");
    ++checks;
    ++seen;
  }
  CHECK(seen == all.size());
  for (const auto &v : CHECKSUM_VECTORS) {
    Bytes record;
    const std::string name = v.name;
    if (name == "zero")
      record = Bytes(RECORD_SIZE, 0);
    else if (name == "diff.live")
      record = diff_record("Aria", 777, G_PLAYERS, OBJ_BASE);
    else if (name == "diff.saved")
      record = encode_saved_record(diff_record("Aria", 777, G_PLAYERS, OBJ_BASE), G_PLAYERS);
    else
      record = lcg_bytes(v.seed, RECORD_SIZE);
    CHECK(player_checksum(v.slot, v.address, record) == v.expected);
  }
  // The values docs/save-format.md records as executed.
  CHECK(player_checksum(0, 0, Bytes(RECORD_SIZE, 0)) == 0xBF50531Au);
  CHECK(player_checksum(0, G_PLAYERS, diff_record("Aria", 777, G_PLAYERS, OBJ_BASE)) == 0x407D263Fu);
  CHECK(player_checksum(0, G_SAVED, encode_saved_record(diff_record("Aria", 777, G_PLAYERS, OBJ_BASE), G_PLAYERS)) ==
        0x41331236u);
}

// tools/test_ref_save.py, one to one.
void layout() {
  CHECK(PLAYERS_AT + 4 * BLOCK_SIZE == QUESTS_AT);
  CHECK((QUESTS_AT & 3) == 0);
  CHECK(DIRECTORY_AT == 0x16010);
  CHECK(DATA_AT == DIRECTORY_AT + (DIRECTORY_SLOTS + 1) * DIRECTORY_STRIDE);
  CHECK(pack_automap({}).size() == AUTOMAP_SIZE);
  CHECK(frame_level_state(Bytes(EMPTY_WORLD.begin(), EMPTY_WORLD.end()), Bytes(EMPTY_DECAL.begin(), EMPTY_DECAL.end()),
                          Bytes(AUTOMAP_SIZE, 0))
            .size() == 32 + AUTOMAP_SIZE);
  CHECK(round_reserved(0, 0x16C8) == 0x1A80);
  CHECK((round_reserved(0x100, 0x16C8) & 0x7F) == 0);
  Bytes grid(AUTOMAP_GRID_SPAN, 0);
  grid[1] = 0xF0;
  grid[5] = 0x07;
  grid[9] = 0x10;
  grid[13] = 0xA0;
  grid[0x361] = 0x20;
  grid[0x365] = 0x30;
  const auto packed = pack_automap(grid);
  CHECK(packed[0] == 0xF0);
  CHECK(packed[1] == 0x1A);
  CHECK(packed[54] == 0x23);
  CHECK(packed.back() == 0);
  CHECK(qualified_name("hub", "west") == "hub_west");
  CHECK(qualified_name("crypt", "") == "crypt");
  const Acts acts{{{"town", 1}}, {{"", 1}}};
  CHECK(find_act("town_a", 0, acts) == 0);
  CHECK(find_act("anything", 0, acts) == 1); // an empty table name matches everything
  CHECK(find_act("anything", -1, Acts{{{"x", 1}}}) == -1);
  CHECK(level_index("town", -1, acts) == -1);
  CHECK(pack_jump(0xFF, Bytes(JUMP_DEST_SIZE, 0))[3] == 0xFF); // the count byte is sign-extended
  refuses([] { pack_jump(0, Bytes(4, 0)); });
  refuses([] { pack_camera(fv(0, 0, 0), std::vector<std::uint32_t>(16), 0, 0, 0, 0, fv(0, 0, 0), Bytes(4, 0)); });
  refuses([] { pack_camera(fv(0, 0, 0), std::vector<std::uint32_t>(12), 0, 0, 0, 0, fv(0, 0, 0), Bytes(0x60, 0)); });
  // Thirteen source floats are enough for the sliding window.
  CHECK(pack_camera(fv(0, 0, 0), std::vector<std::uint32_t>(13), 0, 0, 0, 0, fv(0, 0, 0), Bytes(0x60, 0)).size() ==
        CAMERA_SIZE);
}

void checksum() {
  const Bytes zero(RECORD_SIZE, 0);
  CHECK(player_checksum(0, 0, zero) == 0xBF50531Au);
  CHECK(player_checksum(0, 0x4D8AF0, zero) != player_checksum(0, 0x879EC0, zero));
  CHECK(player_checksum(0, 0x4D8AF0, zero) != player_checksum(1, 0x4D8AF0, zero));
  Bytes left(RECORD_SIZE, 0), right(RECORD_SIZE, 0);
  left[0x90] = 0x3C;
  right[0x93] = 0x7F;
  CHECK(player_checksum(0, 0x1000, left) == player_checksum(0, 0x1000, right));
  right[0x94] = 1;
  CHECK(player_checksum(0, 0x1000, left) != player_checksum(0, 0x1000, right));
  refuses([] { player_checksum(0, 0, Bytes(0x4433, 0)); });
  // Outside the mix: weapon-set words, medals, hit points at +0x4a58.
  Bytes outside(RECORD_SIZE, 0);
  outside[0x4434] = outside[0x4C0D] = outside[0x4A58] = outside[0xED] = 0x55;
  CHECK(player_checksum(2, 0x2000, outside) == player_checksum(2, 0x2000, zero));
}

void item_indices() {
  CHECK(definition_pointer(3) == 0x3F7710u);
  CHECK(definition_index(0x3F7710u) == 3);
  CHECK(definition_index(0) == (static_cast<std::int32_t>(0u - ITEM_TABLE) >> 6));
  const std::uint32_t base = 0x4D8AF0;
  CHECK(pointer_to_index(0, base, {{1, 0, 0}}) == -1);
  CHECK(pointer_to_index(base + BAG_ITEM_AT, base, {{1, 0, 0}}) == 0);
  CHECK(pointer_to_index(base + BAG_ITEM_AT + BAG_STRIDE, base, {{0, 1, 0}}) == 1000);
  CHECK(pointer_to_index(base + BAG_ITEM_AT + BAG_STRIDE, base, {{1, 0, 0}}) == -1);
  CHECK(index_to_pointer(-1, base, {{1, 0, 0}}) == 0);
  CHECK(index_to_pointer(0, base, {{1, 0, 0}}) == base + BAG_ITEM_AT);
  CHECK(index_to_pointer(5, base, {{1, 0, 0}}) == 0);
  CHECK(index_to_pointer(2001, base, {{0, 0, 2}}) == base + BAG_ITEM_AT + 2 * BAG_STRIDE + ITEM_SIZE);
  const auto saved = encode_saved_record(ref_record(), base);
  CHECK(get32(saved, BAG_ITEM_AT) == 3);
  CHECK(get32(saved, 0x43F0) == 0);
  CHECK(get32(saved, 0x4434) == 0);
  CHECK(get32(saved, 0x443C) == 0xFFFFFFFFu);
  CHECK(get32(saved, 0x43D8) == 0x01100000u);
  const auto loaded = decode_loaded_record(saved, base);
  CHECK(get32(loaded, BAG_ITEM_AT) == 0x3F7710u);
  CHECK(get32(loaded, 0x43F0) == base + BAG_ITEM_AT);
  CHECK(get32(loaded, 0x43D8) == 0);
  refuses([] { encode_saved_record(Bytes(RECORD_SIZE - 1, 0), 0); });
}

void slot_round_trip() {
  const auto image = build_slot(ref_inputs());
  CHECK(image.size() == FILE_SIZE);
  const auto slot = parse_slot(image);
  CHECK(slot.level_name == "crypt");
  CHECK(slot.subname.empty());
  CHECK(slot.play_time == 12345);
  CHECK(slot.record_size == RECORD_SIZE);
  CHECK(slot.listed_count == 1);
  CHECK(slot.player_count == 1);
  CHECK(slot.difficulty == 2);
  CHECK(slot.save_arg == 7);
  CHECK(slot.act == 0);
  CHECK(slot.minimap_mode == 3);
  CHECK(slot.jump_count == 2);
  CHECK(slot.jump_destinations[0] == 0xAB);
  CHECK((slot.quests == Quests{{"opened", 7}, {"door", -2}}));
  CHECK(slot.camera == ref_camera());
  const auto hero = import_character(image);
  CHECK(hero.name == "Aria");
  CHECK(hero.gold == 777);
  CHECK(hero.experience == 5000);
  CHECK(hero.level == 4);
  CHECK(hero.acknowledged_level == 3);
  CHECK((hero.base_stats == std::array<std::int32_t, 4>{{46, 37, 24, 28}}));
  CHECK((hero.appearance == std::array<std::int32_t, 4>{{1, 2, 3, 4}}));
  CHECK(hero.class_index == 1);
  CHECK(hero.model_index == 6);
  CHECK(hero.skill_points == 2);
  CHECK(hero.stat_points == 5);
  CHECK(hero.passives[0] == 1);
  CHECK(hero.bags[0].size() == 1);
  CHECK(hero.bags[1].empty() && hero.bags[2].empty());
  CHECK(hero.bags[0][0].definition_index == 3);
  CHECK(hero.bags[0][0].quantity == 4);
  CHECK(hero.bags[0][0].raw.size() == ITEM_SIZE);
  CHECK(hero.equipment[0] == 0);
  CHECK(hero.equipment[1] == -1);
  CHECK(hero.weapon_sets[0] == 0);
  CHECK(hero.weapon_sets[1] == -1);
  CHECK(hero.offhand == -1);
  CHECK((hero.skills == std::vector<std::pair<std::int16_t, std::int16_t>>{{7, 2}}));
  CHECK(hero.kind == 1);
  CHECK(hero.evil_bits == 3);
  CHECK(hero.decision_bits == 1);
  CHECK(hero.mission_flags.size() == 0x4E && hero.mission_flags[13] == 3);
  CHECK(hero.medals.size() == 13);
  CHECK(hero.facing == 0x1234);
  CHECK(hero.position == fv(1, 2, 3));
  CHECK(hero.object_pointer == 0x01100000u); // the file still holds it; load clears it
  CHECK(hero.raw.size() == RECORD_SIZE);
  CHECK(slot.levels.size() == 1);
  const auto &level = slot.levels[0];
  CHECK(level.name == "crypt");
  CHECK(level.size == 0x2000);
  CHECK((*level.world == Bytes(EMPTY_WORLD.begin(), EMPTY_WORLD.end())));
  CHECK((*level.decal == Bytes(EMPTY_DECAL.begin(), EMPTY_DECAL.end())));
  CHECK(level.automap.size() == AUTOMAP_SIZE);
  CHECK(level.world_refused.empty() && level.decal_refused.empty() && level.note.empty());
  require_known_world(level);
  require_known_decal(level);
  CHECK(std::string(image.begin() + QUESTS_AT, image.begin() + QUESTS_AT + 6) == "opened");
  CHECK(image[QUESTS_AT + 6] == 0);
  CHECK(get32(image, QUESTS_AT + 0x20) == 7);
  CHECK(get32(image, QUESTS_AT + QUEST_STRIDE + 0x20) == 0xFFFFFFFEu);
  refuses([&] { import_character(image, 1); });
  refuses([&] { import_character(image, -1); });
}

void slot_paths() {
  {
    auto in = ref_inputs();
    in.num_players = 2;
    in.one_player = true;
    in.players.push_back(ref_bram());
    in.memcard_state = 4;
    const auto slot = parse_slot(build_slot(in));
    CHECK(slot.listed_count == 1);
    CHECK(slot.player_count == 2);
    CHECK(slot.characters.size() == 2 && slot.characters[0].name == "Aria" && slot.characters[1].name == "Bram");
    CHECK(slot.levels.empty());
  }
  {
    auto in = ref_inputs();
    in.level_name = "hub";
    in.subname = "west";
    in.acts = {{{"town", 0x1800}, {"hub", 0x1000}}};
    const auto slot = parse_slot(build_slot(in));
    CHECK(slot.level_name == "hub" && slot.subname == "west");
    CHECK(slot.levels.size() == 1 && slot.levels[0].name == "hub_west" && slot.levels[0].size == 0x1800);
    // "hu" is not "hub": it searches the act and misses.
    in.level_name = "hu";
    in.subname = "";
    in.acts = {{{"h", 0x1800}}};
    const auto miss = parse_slot(build_slot(in));
    CHECK(miss.levels.size() == 1 && miss.levels[0].name == "hu");
  }
  {
    const Bytes previous(FILE_SIZE, 0x5A);
    auto in = ref_inputs();
    in.level_name = "nope";
    in.previous = previous;
    CHECK(build_slot(in) == previous);
  }
  {
    auto in = ref_inputs();
    in.game_complete = true;
    in.previous = complete_previous();
    const auto image = build_slot(in);
    CHECK(std::string(image.begin(), image.begin() + 12) == "gameComplete" && image[12] == 0);
    CHECK(std::all_of(image.begin() + 13, image.begin() + 0x14, [](std::uint8_t b) { return b == 0x11; }));
    CHECK(image[DIRECTORY_AT] == 0);
    const auto slot = parse_slot(image);
    CHECK(slot.level_name == "gameComplete");
    CHECK(slot.levels.empty());
  }
  {
    auto in = ref_inputs();
    in.previous = does_not_fit_previous();
    const auto image = build_slot(in);
    CHECK(std::string(image.begin() + DIRECTORY_AT, image.begin() + DIRECTORY_AT + 3) == "old");
    CHECK(std::string(image.begin() + DIRECTORY_AT + DIRECTORY_STRIDE,
                      image.begin() + DIRECTORY_AT + DIRECTORY_STRIDE + 5) == "crypt");
    CHECK(get32(image, DATA_AT) == 0x22222222u);
    refuses([&] { parse_slot(image); }); // the directory sizes do not fit the file
    CHECK(parse_slot(image, false).levels.empty());
  }
  {
    auto in = ref_inputs();
    in.memcard_state = 4;
    in.num_players = 2;
    in.players = {LiveCharacter{ref_record("Aria", 0x4D8AF0, 0), 0x4D8AF0, 1, {{9, 9, 9}}},
                  ref_bram(0x01100200, 0x2222, fv(8, 7, 6))};
    const auto hero = import_character(build_slot(in), 0);
    CHECK(hero.name == "Aria");
    CHECK(hero.facing == 0x2222);
    CHECK(hero.position == fv(8, 7, 6));
    // alive_first is preferred when that player has an object.
    in.players.push_back(LiveCharacter{ref_record("Cedric", 0, 0x01100400), 0, 0x3333, fv(5, 5, 5)});
    in.alive_first = 2;
    const auto other = import_character(build_slot(in), 0);
    CHECK(other.facing == 0x3333);
    // No player with an object leaves a zero pose.
    in.players = {LiveCharacter{ref_record("Aria", 0x4D8AF0, 0), 0x4D8AF0, 1, {{9, 9, 9}}}};
    in.num_players = 1;
    in.alive_first = -1;
    const auto none = import_character(build_slot(in), 0);
    CHECK(none.facing == 0 && none.position == Bits3{{0, 0, 0}});
  }
  {
    auto in = ref_inputs();
    in.num_players = 2;
    in.one_player = true;
    in.memcard_state = 4;
    in.players.push_back(in.players[0]);
    const auto slot = parse_slot(set_player_count(build_slot(in), 1));
    CHECK(slot.listed_count == 1 && slot.player_count == 1 && slot.characters.size() == 1);
  }
  {
    // Card state 4 and flag 0x800 write the header and skip the directory.
    auto in = ref_inputs();
    in.block_flag = 0x800;
    CHECK(parse_slot(build_slot(in)).levels.empty());
    // An exact name updates the existing slot and keeps its size.
    in.block_flag = 0;
    in.previous = write_dir(Bytes(FILE_SIZE, 0), "crypt", 0x3000);
    const auto slot = parse_slot(build_slot(in));
    CHECK(slot.levels.size() == 1 && slot.levels[0].size == 0x3000);
    // A missing player record below the count is refused, not invented.
    in.num_players = 2;
    refuses([&] { build_slot(in); });
    // Names that do not fit are refused.
    in = ref_inputs();
    in.level_name = std::string(0x14, 'a');
    in.acts = {{{"a", 0x2000}}};
    refuses([&] { build_slot(in); });
    in = ref_inputs();
    in.quests = {{std::string(0x20, 'q'), 1}};
    refuses([&] { build_slot(in); });
  }
}

void slot_refusals() {
  const auto good = build_slot(ref_inputs());
  refuses([&] { parse_slot(Bytes(good.begin(), good.end() - 1)); });
  refuses([&] {
    auto image = good;
    image.push_back(0);
    parse_slot(image);
  });
  auto image = good;
  image[0x68] = 0;
  refuses([&] { parse_slot(image); });
  CHECK(parse_slot(image, false).record_size == 0x4C00);
  image = good;
  put32(image, 0x31C, 5);
  refuses([&] { parse_slot(image); });
  put32(image, 0x31C, 0xFFFFFFFFu);
  refuses([&] { parse_slot(image); });
  CHECK(parse_slot(image, false).characters.empty());
  image = good;
  std::fill(image.begin() + QUESTS_AT, image.begin() + QUESTS_AT + 0x20, 0x41);
  refuses([&] { parse_slot(image); });
  image = good;
  // A bag count outside 0..64.
  put32(image, PLAYERS_AT + POSE_SIZE + BAG_COUNT_AT, 65);
  refuses([&] { parse_slot(image); });
  image = good;
  // A character name with no NUL.
  std::fill(image.begin() + PLAYERS_AT + POSE_SIZE, image.begin() + PLAYERS_AT + POSE_SIZE + 0x20, 'x');
  refuses([&] { parse_slot(image); });
  image = good;
  // A level name with no NUL.
  std::fill(image.begin(), image.begin() + 0x14, 'L');
  refuses([&] { parse_slot(image); });
  image = good;
  // A directory name with no NUL.
  std::fill(image.begin() + DIRECTORY_AT, image.begin() + DIRECTORY_AT + 0x14, 'd');
  refuses([&] { parse_slot(image); });
  image = good;
  // A quest record that would cross the directory.
  std::size_t at = QUESTS_AT;
  for (; at + QUEST_STRIDE <= DIRECTORY_AT; at += QUEST_STRIDE) {
    image[at] = 'q';
    image[at + 1] = 0;
  }
  CHECK(at == 0x15FF8); // the 313th record would land on the directory
  image[at] = 'q';
  refuses([&] { parse_slot(image); });
  image = good;
  // Native rule: the unexecuted eleventh stride must be zero.
  image[ELEVENTH_DIRECTORY_AT + 3] = 1;
  refuses<Unknown>([&] { parse_slot(image); });
  refuses<Unknown>([] { eleventh_directory_name(); });
  refuses<Unknown>([] { file_checksum(); });
  refuses<Unknown>([] { apply_camera(); });
}

void unknown_level_bodies() {
  auto in = ref_inputs();
  in.world_body = Bytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  in.decal_body = Bytes(20, 0x10);
  const auto image = build_slot(in);
  const auto slot = parse_slot(image); // the character is still importable
  CHECK(slot.levels.size() == 1);
  const auto &level = slot.levels[0];
  CHECK(level.world->size() == 12 && level.decal->size() == 20);
  CHECK(!level.world_refused.empty() && !level.decal_refused.empty());
  refuses<Unknown>([&] { require_known_world(level); });
  refuses<Unknown>([&] { require_known_decal(level); });
  // Framing that does not fit is reported on the level.
  auto broken = image;
  put32(broken, DATA_AT, 0x7FFFFFFFu);
  const auto bad = parse_slot(broken);
  CHECK(bad.levels[0].note == "world length does not fit");
  refuses<Unknown>([&] { require_known_world(bad.levels[0]); });
  const auto refusals = import_into(image).refusals;
  CHECK(std::count_if(refusals.begin(), refusals.end(),
                      [](const std::string &s) { return s.rfind("level crypt", 0) == 0; }) == 2);
  // A body that would run past the file is refused, not written.
  in = ref_inputs();
  in.previous = write_dir(Bytes(FILE_SIZE, 0), "big", std::uint32_t(FILE_SIZE - DATA_AT - 0x100));
  in.acts = {{{"crypt", 0x100}}};
  refuses<Unknown>([&] { build_slot(in); });
}

void config() {
  const auto image = config_set_defaults(-1, 1);
  CHECK(image.size() == CONFIG_SIZE);
  const auto parsed = parse_config(image);
  CHECK(parsed.brightness == f(0.5f) && parsed.music == f(0.5f) && parsed.effects == f(0.5f));
  CHECK(parsed.stereo == 1 && parsed.vibration == 1 && parsed.unnamed_18 == 1);
  CHECK(parsed.subtitles == 1 && parsed.damage_numbers == 1 && parsed.blood == 0);
  CHECK(parsed.markers == 1 && parsed.combat_camera == 1 && parsed.reverse_yaw == 0);
  CHECK(parsed.buttons[0][0] == 0x40);
  CHECK(parsed.buttons[3] == parsed.buttons[0]);
  CHECK(parsed.voice.size() == 0x14);
  const auto other = config_set_defaults(-1, 0);
  CHECK(get32(other, 0x34) == 0x20 && get32(other, 0x38) == 0x10 && get32(other, 0x3C) == 0x40);
  CHECK(get32(image, 0xC8) == 0x3E199999u);
  Bytes odd(CONFIG_SIZE, 0);
  put32(odd, 0x34, 0x11111111u);
  const auto kept = config_set_defaults(-1, 2, &odd);
  CHECK(get32(kept, 0x34) == 0x11111111u);
  CHECK(get32(kept, 4) == f(0.5f));
  const Bytes shorter(CONFIG_SIZE - 1, 0);
  refuses([&] { config_set_defaults(-1, 1, &shorter); });
  auto profile = image;
  const char entry[] = "hint\0\x05";
  std::copy(entry, entry + 6, profile.begin() + 0xF8);
  const auto with_profile = parse_config(profile);
  CHECK((with_profile.profile == Quests{{"hint", 5}}));
  refuses([&] { parse_config(Bytes(profile.begin(), profile.end() - 1)); });
  CHECK(config_field_name(0x04) == "brightness");
  CHECK(config_field_name(0x30) == "reverse_yaw");
  CHECK(config_field_name(0x34 + 0x24 + 8) == "buttons[1][2]");
  CHECK(config_field_name(0x100) == "profile");
  refuses<Unknown>([] { config_field_name(0x00); });
  refuses<Unknown>([] { config_field_name(0x18); });
  refuses<Unknown>([] { config_field_name(0xC8); });
  refuses<Unknown>([] { config_field_name(0xE0); });
  refuses<Unknown>([] { voice_global_pairing(); });
  refuses<Unknown>([] { config_open_name(); });
  // The image goes to g_savedConfig, which puts the profile table at PROFILE_VARS.
  auto memory = std::make_shared<builtins::Memory>();
  builtins::Variables variables(memory);
  apply_config(*memory, profile);
  CHECK(memory->read(SAVED_CONFIG, CONFIG_SIZE) == profile);
  CHECK(builtins::PROFILE_VARS == SAVED_CONFIG + 0xF8);
  CHECK(variables.profile_get("hint") == 5);
  refuses([&] { apply_config(*memory, Bytes(10, 0)); });
}

std::string cstr(const builtins::Memory &m, std::uint32_t at) { return m.cstr(at, 0x80); }

// difftest_save.test_load, applied to native memory.
void native_import() {
  Common c;
  c.quests = {{"opened", 7}, {"door", -2}};
  c.players = 2;
  c.grid = Bytes(8, 0);
  const auto image = build_slot(common(c));
  auto imported = import_into(image);
  auto &m = *imported.memory;
  CHECK(cstr(m, G_PLAYERS) == "Aria");
  CHECK(m.s32(G_PLAYERS + 0xE4) == 777);
  CHECK(m.s32(G_PLAYERS + 0x5C) == 5000);
  CHECK(m.word(G_PLAYERS + BAG_ITEM_AT) == definition_pointer(3));
  CHECK(m.word(G_PLAYERS + 0x43F0) == G_PLAYERS + BAG_ITEM_AT);
  CHECK(m.word(G_PLAYERS + 0x43D8) == 0);
  CHECK(m.s32(NUM_PLAYERS) == 2 && m.s32(CURRENT_ACT) == 0 && m.u8(DIFFICULTY) == 2);
  CHECK(m.word(PLAY_TIME) == 12345 && m.u8(LOADED_FROM_SAVE) == 1 && m.word(AUTOSAVE_COUNTDOWN) == 99);
  CHECK(cstr(m, PENDING_LEVEL) == "crypt" && cstr(m, LOADED_NAME) == "crypt" && m.u8(PENDING_MARKER) == 0xFD);
  CHECK(m.u16(SPAWN_FACING) == 0x1234 && m.u16(SPAWN_FACING + 2) == 0x1235);
  CHECK(m.word(SPAWN_POS) == f(1) && m.word(SPAWN_POS + 4) == f(2) && m.word(SPAWN_POS + 8) == f(3));
  CHECK(m.word(SPAWN_POS + 0xC) == f(2));
  const Bytes saved(image.begin() + PLAYERS_AT + POSE_SIZE, image.begin() + PLAYERS_AT + BLOCK_SIZE);
  CHECK(m.read(G_PLAYERS, RECORD_SIZE) == decode_loaded_record(saved, G_PLAYERS));
  const auto second = G_PLAYERS + std::uint32_t(RECORD_SIZE);
  CHECK(cstr(m, second) == "Bram" && m.word(second + 0x43F0) == second + BAG_ITEM_AT);
  CHECK(static_cast<std::int8_t>(m.u8(builtins::QUEST_VARS + 7)) == 7);
  CHECK(static_cast<std::int8_t>(m.u8(builtins::QUEST_VARS + 7 + 1 + 5)) == -2);
  CHECK((imported.live_quests == Quests{{"opened", 7}, {"door", -2}}));
  CHECK(imported.variables->quest_get("door") == -2);
  CHECK(m.u8(JUMP_COUNT) == 2 && m.u8(JUMP_DESTS) == 0xAB && m.u8(JUMP_DESTS + 1) == 0xCD);
  CHECK(m.word(MINIMAP) == 3);
  CHECK(m.read(SAVE_HEADER_COPY, HEADER_SIZE) == Bytes(image.begin(), image.begin() + HEADER_SIZE));
  CHECK(imported.refusals.size() == 1 && imported.refusals[0].rfind("camera:", 0) == 0);
  // The native player view.
  CHECK(imported.player.class_id == 1 && imported.player.level == 4);
  CHECK(imported.player.strength == 46 && imported.player.intelligence == 37);
  CHECK(imported.player.dexterity == 24 && imported.player.stamina == 28);
  CHECK(imported.player.skill_rank(7) == 2);
  // Native mission and inventory readers see the imported record.
  CHECK(imported.variables->player_record(0) == imported.record_address);
  // The netplay mix of the live record, at its g_players address.
  const auto live = decode_loaded_record(saved, G_PLAYERS);
  CHECK(imported.checksum == player_checksum(0, G_PLAYERS, live));

  // The second character goes through its own slot and address.
  const auto bram = import_into(image, 1);
  CHECK(bram.character.name == "Bram" && bram.record_address == second);
  CHECK(bram.checksum == player_checksum(1, second, decode_loaded_record(bram.character.raw, second)));

  // An expected mix that matches imports; one that does not refuses before any write.
  CHECK(import_into(image, 0, PROFILE, imported.checksum).character.name == "Aria");
  refuses([&] { import_into(image, 0, PROFILE, imported.checksum ^ 1u); });
  // Every other profile refuses, Norrath included.
  refuses([&] { import_into(image, 0, "norrath-unvalidated"); });
  refuses([&] { import_into(image, 0, "rta"); });
  refuses([&] { require_profile(""); });
  require_profile(PROFILE);
  refuses([&] { import_into(image, 2); });

  // A subname: both strings are name_subname.
  c = Common{};
  c.level = "hub";
  c.sub = "west";
  c.acts = Acts{{{"town", 0x1800}, {"hub", 0x1000}}};
  const auto hub = import_into(build_slot(common(c)));
  CHECK(cstr(*hub.memory, PENDING_LEVEL) == "hub_west" && cstr(*hub.memory, LOADED_NAME) == "hub_west");

  // save_load ignores the record size; the strict reader refuses it.
  auto resized = build_slot(common(Common{}));
  put16(resized, 0x68, 0x1111);
  refuses([&] { import_into(resized); });

  // apply_load: the share word and a negative difficulty refuse before writes.
  auto memory = std::make_shared<builtins::Memory>();
  builtins::Variables variables(memory);
  memory->word(SHARE_QUEST_ITEMS, 1);
  refuses<Unknown>([&] { apply_load(variables, image); });
  CHECK(memory->word(NUM_PLAYERS) == 0 && memory->read(G_PLAYERS) == 0);
  memory->word(SHARE_QUEST_ITEMS, 0);
  auto negative = image;
  negative[0x6C] = 0x80;
  refuses<Unknown>([&] { apply_load(variables, negative); });
  CHECK(memory->read(G_PLAYERS) == 0);
  // Quest values above a byte clamp, as vars_set does.
  auto big = build_slot([] {
    auto in = ref_inputs();
    in.quests = {{"Opened", 300}};
    return in;
  }());
  const auto clamped = import_into(big);
  CHECK(clamped.variables->quest_get("opened") == 127);
  // A long qualified name that would reach the marker is refused.
  auto lng = ref_inputs();
  lng.level_name = std::string(0x13, 'a');
  lng.subname = std::string(0x40, 'b');
  lng.acts = {{{"a", 0x2000}}};
  refuses([&] { import_into(build_slot(lng)); });
}

void native_inventory() {
  items::Tables tables;
  auto definition = std::make_shared<items::Definition>();
  definition->addr = definition_pointer(3);
  definition->index = 3;
  definition->name = "synthetic";
  tables.defs.push_back(definition);
  const auto imported = import_into(build_slot(ref_inputs()));
  const auto bags = inventory(tables, imported);
  CHECK(bags[0].size() == 1 && bags[1].empty() && bags[2].empty());
  CHECK(bags[0][0].definition == definition);
  CHECK(bags[0][0].count == 4);
  items::Tables empty;
  refuses([&] { inventory(empty, imported); });
  refuses([&] { inventory(tables, ImportedCharacter{}); });
}
} // namespace

int main() {
  try {
    reference_vectors();
    layout();
    checksum();
    item_indices();
    slot_round_trip();
    slot_paths();
    slot_refusals();
    unknown_level_bodies();
    config();
    native_import();
    native_inventory();
  } catch (const std::exception &error) {
    std::cerr << "ps2-save: " << error.what() << '\n';
    return 1;
  }
  std::cout << checks << " PS2 save checks passed; " << std::size(IMAGE_VECTORS) << " image and "
            << std::size(CHECKSUM_VECTORS) << " checksum reference vectors\n";
  return 0;
}
