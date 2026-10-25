#include "champions/ps2_save.hpp"

#include "champions/game_profile.hpp"

#include <algorithm>
#include <cstring>

// Port of tools/ref_save.py at t3/research-saves 8d43286. Comments name the
// original function each piece follows; docs/save-format.md has the evidence.
namespace champions::simulation::rta::ps2_save {
namespace {
constexpr std::uint32_t CHECK_XOR = 0xC18DA3B1;
constexpr std::uint32_t LEVEL_XOR = 0x023D5097;
constexpr char COMPLETE_NAME[] = "gameComplete"; // 13 bytes with the NUL
constexpr std::size_t COMPLETE_NAME_SIZE = sizeof(COMPLETE_NAME);

std::uint32_t u32_at(const std::uint8_t *data, std::size_t offset) {
  return std::uint32_t(data[offset]) | std::uint32_t(data[offset + 1]) << 8 |
         std::uint32_t(data[offset + 2]) << 16 | std::uint32_t(data[offset + 3]) << 24;
}
std::uint32_t u32_at(const Bytes &data, std::size_t offset) { return u32_at(data.data(), offset); }
std::int32_t s32_at(const Bytes &data, std::size_t offset) {
  return static_cast<std::int32_t>(u32_at(data, offset));
}
std::uint16_t u16_at(const Bytes &data, std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset] | data[offset + 1] << 8);
}
std::int16_t s16_at(const Bytes &data, std::size_t offset) {
  return static_cast<std::int16_t>(u16_at(data, offset));
}
std::int32_t s8(std::uint8_t value) { return static_cast<std::int8_t>(value); }
void put_u32(Bytes &data, std::size_t offset, std::uint32_t value) {
  for (int i = 0; i < 4; ++i)
    data[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void put_u16(Bytes &data, std::size_t offset, std::uint32_t value) {
  data[offset] = static_cast<std::uint8_t>(value);
  data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}
void append_u32(Bytes &out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
std::uint32_t float_bits(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, 4);
  return bits;
}

// _write_cstr: the text and a NUL, refused when it does not fit in limit.
void write_cstr(Bytes &buf, std::size_t offset, const std::string &text, std::size_t limit) {
  if (text.size() >= limit)
    throw Error("string of " + std::to_string(text.size()) + " bytes does not fit in " +
                std::to_string(limit));
  if (offset + text.size() + 1 > buf.size())
    throw Error("string runs past the end of the image");
  std::copy(text.begin(), text.end(), buf.begin() + static_cast<std::ptrdiff_t>(offset));
  buf[offset + text.size()] = 0;
}
// _read_cstr: nullopt when there is no NUL inside limit.
std::optional<std::string> read_cstr(const Bytes &buf, std::size_t offset, std::size_t limit) {
  const std::size_t end = std::min(buf.size(), offset + limit);
  for (std::size_t i = offset; i < end; ++i)
    if (buf[i] == 0)
      return std::string(buf.begin() + static_cast<std::ptrdiff_t>(offset),
                         buf.begin() + static_cast<std::ptrdiff_t>(i));
  return std::nullopt;
}
std::optional<std::string> directory_name(const Bytes &buf, std::size_t slot) {
  return read_cstr(buf, DIRECTORY_AT + slot * DIRECTORY_STRIDE, 0x14);
}
bool starts_with(const std::string &text, const std::string &prefix) {
  return text.compare(0, prefix.size(), prefix) == 0 && text.size() >= prefix.size();
}
std::string hex(std::uint64_t value) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  do {
    out.insert(out.begin(), digits[value & 15]);
    value >>= 4;
  } while (value);
  return "0x" + out;
}

std::array<std::int32_t, 3> bag_counts(const Bytes &record) {
  return {{s32_at(record, BAG_COUNT_AT), s32_at(record, BAG_COUNT_AT + BAG_STRIDE),
           s32_at(record, BAG_COUNT_AT + 2 * BAG_STRIDE)}};
}
// The seventeen equipment words, the two weapon-set words and the offhand.
std::array<std::size_t, 20> equipment_offsets() {
  std::array<std::size_t, 20> out{};
  for (std::size_t i = 0; i < 17; ++i)
    out[i] = EQUIPMENT_AT + 4 * i;
  out[17] = 0x4434;
  out[18] = 0x4438;
  out[19] = 0x443C;
  return out;
}

std::uint32_t mul32(std::uint32_t left, std::uint32_t right) {
  return static_cast<std::uint32_t>(std::int64_t(static_cast<std::int32_t>(left)) *
                                    std::int64_t(static_cast<std::int32_t>(right)));
}
std::uint32_t times35(std::uint32_t v) { return (((v << 3) + v) << 2) - v; }
std::uint32_t times3(std::uint32_t v) { return (v << 1) + v; }
std::uint32_t mix_word(const Bytes &record, std::size_t offset) {
  return mix_bytes(record.data() + offset, 4);
}

std::uint32_t object_pointer(const Bytes &record) { return u32_at(record, OBJECT_POINTER_AT); }
const LiveCharacter *pose_source(const std::vector<LiveCharacter> &players, std::size_t index,
                                 std::int32_t num_players, std::int32_t alive_first) {
  if (object_pointer(players[index].record) != 0)
    return &players[index];
  if (alive_first >= 0 && std::size_t(alive_first) < players.size() &&
      object_pointer(players[std::size_t(alive_first)].record) != 0)
    return &players[std::size_t(alive_first)];
  const auto limit = std::min<std::int64_t>(num_players, std::int64_t(players.size()));
  for (std::int64_t other = 0; other < limit; ++other)
    if (object_pointer(players[std::size_t(other)].record) != 0)
      return &players[std::size_t(other)];
  return nullptr;
}

// The part of save_storeLevelState 0x21777c that changes the buffer.
void store_level(Bytes &buf, const std::string &name, const SlotInputs &inputs) {
  if (inputs.memcard_state == 4 || (inputs.block_flag & 0x800))
    return;
  std::uint32_t reserved;
  if (inputs.game_complete) {
    reserved = COMPLETE_LEVEL_SIZE;
  } else {
    // strncmp(name, "hub", 3) == 0 selects row 0 and does not search.
    int index;
    if (name.size() >= 3 && name.compare(0, 3, "hub") == 0) {
      index = 0;
    } else {
      index = level_index(name, inputs.act, inputs.acts);
      if (index < 0)
        return;
    }
    if (inputs.act < 0 || std::size_t(inputs.act) >= inputs.acts.size() ||
        std::size_t(index) >= inputs.acts[std::size_t(inputs.act)].size())
      return;
    reserved = inputs.acts[std::size_t(inputs.act)][std::size_t(index)].size;
  }
  std::uint32_t prior = 0;
  std::optional<std::size_t> chosen;
  bool fresh = false;
  for (std::size_t slot = 0; slot < DIRECTORY_SLOTS; ++slot) {
    const auto offset = DIRECTORY_AT + slot * DIRECTORY_STRIDE;
    if (buf[offset] == 0) {
      chosen = slot;
      fresh = true;
      break;
    }
    if (directory_name(buf, slot) == name) {
      chosen = slot;
      break;
    }
    prior += u32_at(buf, offset + 0x14);
  }
  if (!chosen)
    return;
  const auto entry = DIRECTORY_AT + *chosen * DIRECTORY_STRIDE;
  if (fresh) {
    write_cstr(buf, entry, name, 0x14);
    put_u32(buf, entry + 0x14, reserved);
  } else {
    reserved = u32_at(buf, entry + 0x14);
  }
  // Unsigned 32-bit sum, then an unsigned compare. A new name is left in place.
  const std::uint32_t total = std::uint32_t(DATA_AT) + prior + reserved;
  if (FILE_SIZE < total)
    return;
  const Bytes world = inputs.world_body ? *inputs.world_body : Bytes(EMPTY_WORLD.begin(), EMPTY_WORLD.end());
  const Bytes decal = inputs.decal_body ? *inputs.decal_body : Bytes(EMPTY_DECAL.begin(), EMPTY_DECAL.end());
  const Bytes blob = frame_level_state(world, decal, pack_automap(inputs.automap_grid));
  const std::size_t start = DATA_AT + prior;
  if (start + blob.size() > buf.size())
    throw Unknown("level body of " + std::to_string(blob.size()) + " bytes at " + hex(start) +
                  " runs past the file; what the writers do with that budget is not documented");
  std::copy(blob.begin(), blob.end(), buf.begin() + static_cast<std::ptrdiff_t>(start));
}

std::vector<Item> parse_items(const Bytes &record, int bag, bool strict) {
  auto count = s32_at(record, BAG_COUNT_AT + std::size_t(bag) * BAG_STRIDE);
  if (strict && !(0 <= count && count <= BAG_CAPACITY))
    throw Error("bag " + std::to_string(bag) + " count " + std::to_string(count) + " is outside 0..64");
  count = std::clamp(count, 0, BAG_CAPACITY);
  std::vector<Item> items;
  const auto base = BAG_ITEM_AT + std::size_t(bag) * BAG_STRIDE;
  for (std::int32_t index = 0; index < count; ++index) {
    const auto offset = base + std::size_t(index) * ITEM_SIZE;
    Item item;
    item.definition_index = s32_at(record, offset);
    item.quantity = s16_at(record, offset + 4);
    item.raw.assign(record.begin() + static_cast<std::ptrdiff_t>(offset),
                    record.begin() + static_cast<std::ptrdiff_t>(offset + ITEM_SIZE));
    items.push_back(std::move(item));
  }
  return items;
}

Character parse_character(const Bytes &data, std::size_t block_at, bool strict) {
  const Bytes record(data.begin() + static_cast<std::ptrdiff_t>(block_at + POSE_SIZE),
                     data.begin() + static_cast<std::ptrdiff_t>(block_at + BLOCK_SIZE));
  Character out;
  auto name = read_cstr(record, 0, 0x20);
  if (!name) {
    if (strict)
      throw Error("character name has no terminator");
    name = std::string();
  }
  out.name = *name;
  for (std::size_t index = 0; index < 48; ++index) {
    const auto offset = 0x4B08 + index * 4;
    const auto ident = s16_at(record, offset);
    if (ident == -1)
      break;
    out.skills.emplace_back(ident, s16_at(record, offset + 2));
  }
  for (std::size_t i = 0; i < 4; ++i)
    out.appearance[i] = s8(record[0x20 + i]);
  out.class_index = s32_at(record, 0x50);
  out.model_index = s32_at(record, 0x54);
  out.acknowledged_level = s8(record[0x58]);
  out.level = s8(record[0x59]);
  out.experience = s32_at(record, 0x5C);
  for (std::size_t i = 0; i < 4; ++i)
    out.base_stats[i] = s32_at(record, 0x78 + 4 * i);
  out.gold = s32_at(record, 0xE4);
  out.skill_points = s32_at(record, 0xE8);
  out.stat_points = s8(record[0xEC]);
  for (std::size_t i = 0; i < 48; ++i)
    out.passives[i] = s8(record[0xEF + i]);
  for (int bag = 0; bag < BAGS; ++bag)
    out.bags[std::size_t(bag)] = parse_items(record, bag, strict);
  for (std::size_t i = 0; i < 17; ++i)
    out.equipment[i] = s32_at(record, EQUIPMENT_AT + 4 * i);
  out.weapon_sets = {{s32_at(record, 0x4434), s32_at(record, 0x4438)}};
  out.offhand = s32_at(record, 0x443C);
  out.mission_flags.assign(record.begin() + 0x4BBF, record.begin() + 0x4C0D);
  out.medals.assign(record.begin() + 0x4C0D, record.begin() + 0x4C1A);
  out.evil_bits = record[0x4C1A];
  out.decision_bits = record[0x4C1B];
  out.kind = record[0x4C1C] & 0x1F;
  out.facing = u16_at(data, block_at);
  for (std::size_t i = 0; i < 3; ++i)
    out.position[i] = u32_at(data, block_at + 2 + 4 * i);
  out.object_pointer = u32_at(record, OBJECT_POINTER_AT);
  out.raw = record;
  return out;
}

// _split_frame. Lengths are compared without wrapping.
void split_frame(LevelBlob &level) {
  const Bytes &blob = level.data;
  if (blob.size() < 16) {
    level.note = "shorter than a header";
    return;
  }
  const std::uint64_t world_len = u32_at(blob, 0);
  const std::size_t world_at = 8;
  if (world_len > blob.size() - world_at) {
    level.note = "world length does not fit";
    return;
  }
  const std::size_t decal_len_at = world_at + std::size_t(world_len);
  if (decal_len_at + 8 > blob.size()) {
    level.note = "decal header does not fit";
    return;
  }
  const std::uint64_t decal_len = u32_at(blob, decal_len_at);
  const std::size_t decal_at = decal_len_at + 8;
  if (decal_len > blob.size() - decal_at) {
    level.note = "decal length does not fit";
    return;
  }
  const std::size_t auto_at = decal_at + std::size_t(decal_len);
  level.world = Bytes(blob.begin() + world_at, blob.begin() + static_cast<std::ptrdiff_t>(decal_len_at));
  level.decal = Bytes(blob.begin() + static_cast<std::ptrdiff_t>(decal_at),
                      blob.begin() + static_cast<std::ptrdiff_t>(auto_at));
  if (auto_at < blob.size())
    level.automap.assign(blob.begin() + static_cast<std::ptrdiff_t>(auto_at),
                         blob.begin() + static_cast<std::ptrdiff_t>(std::min(blob.size(), auto_at + AUTOMAP_SIZE)));
}

void classify_level(LevelBlob &level) {
  if (!level.note.empty()) {
    level.world_refused = level.decal_refused = "framing: " + level.note;
    return;
  }
  if (*level.world != Bytes(EMPTY_WORLD.begin(), EMPTY_WORLD.end()))
    level.world_refused = "populated world object records are unknown (world_writeRecords 0x14e140 was not executed)";
  if (*level.decal != Bytes(EMPTY_DECAL.begin(), EMPTY_DECAL.end()))
    level.decal_refused = "non-empty decal fields are unknown (decal_serialize 0x19ee10 was not executed with a decal)";
}

std::optional<std::array<std::uint32_t, 9>> button_row(int scheme) {
  if (scheme == 1)
    return std::array<std::uint32_t, 9>{{0x40, 0x20, 0x10, 0x80, 0x08, 0x02, 0x01, 0x400, 0x04}};
  if (scheme == 0)
    return std::array<std::uint32_t, 9>{{0x20, 0x10, 0x40, 0x80, 0x08, 0x02, 0x01, 0x400, 0x04}};
  return std::nullopt;
}

Quests profile_entries(const Bytes &data) {
  Quests entries;
  std::size_t cursor = 0xF8;
  while (cursor < CONFIG_SIZE && data[cursor] != 0) {
    std::size_t end = cursor;
    while (end < CONFIG_SIZE && data[end] != 0)
      ++end;
    if (end >= CONFIG_SIZE || end + 1 >= CONFIG_SIZE)
      break;
    entries.emplace_back(std::string(data.begin() + static_cast<std::ptrdiff_t>(cursor),
                                     data.begin() + static_cast<std::ptrdiff_t>(end)),
                         s8(data[end + 1]));
    cursor = end + 2;
  }
  return entries;
}
} // namespace

std::uint32_t round_reserved(std::uint32_t original, std::uint32_t automap_half) {
  return (original + (automap_half & 0xFFFF) + 0x3E8) & 0xFF80;
}

// automap_packExplored 0x16865c. 108 rows by 54 columns, no length prefix.
Bytes pack_automap(const Bytes &grid) {
  Bytes out;
  out.reserve(AUTOMAP_SIZE);
  std::size_t row = 0;
  for (int outer = 0; outer < 0xD8 / 2; ++outer) {
    std::size_t offset = row << 1;
    for (int column = 4;; column += 4) {
      const std::uint8_t high = offset + 1 < grid.size() ? grid[offset + 1] : 0;
      const std::uint8_t low = offset + 5 < grid.size() ? grid[offset + 5] : 0;
      out.push_back(static_cast<std::uint8_t>((high & 0xF0) + (low >> 4)));
      offset += 8;
      if (column >= 0xD8)
        break;
    }
    row += 0x1B0;
  }
  return out;
}

// save_writeLevelState 0x218600: length, four unstored bytes, body, twice, then the automap.
Bytes frame_level_state(const Bytes &world_body, const Bytes &decal_body, const Bytes &automap) {
  Bytes out;
  append_u32(out, std::uint32_t(world_body.size()));
  append_u32(out, 0);
  out.insert(out.end(), world_body.begin(), world_body.end());
  append_u32(out, std::uint32_t(decal_body.size()));
  append_u32(out, 0);
  out.insert(out.end(), decal_body.begin(), decal_body.end());
  out.insert(out.end(), automap.begin(), automap.end());
  return out;
}

// Camera_saveState 0x203918. The 16 matrix floats are the sliding window 0..3, 3..6, 6..9, 9..12.
Bytes pack_camera(const Bits3 &position, const std::vector<std::uint32_t> &matrix, std::uint16_t half_68,
                  std::uint16_t half_6a, std::uint32_t word_54, std::uint32_t word_58, const Bits3 &floats,
                  const Bytes &tail) {
  if (tail.size() != 0x60)
    throw Error("camera tail is " + std::to_string(tail.size()) + " bytes");
  Bytes out;
  for (auto v : position)
    append_u32(out, v);
  std::size_t row = 0, column = 0;
  for (int i = 0; i < 4; ++i) {
    const std::size_t start = row + column;
    if (start + 4 > matrix.size())
      throw Error("camera matrix window is short");
    for (std::size_t k = 0; k < 4; ++k)
      append_u32(out, matrix[start + k]);
    column += 1;
    row += 2;
  }
  out.push_back(static_cast<std::uint8_t>(half_68));
  out.push_back(static_cast<std::uint8_t>(half_68 >> 8));
  out.push_back(static_cast<std::uint8_t>(half_6a));
  out.push_back(static_cast<std::uint8_t>(half_6a >> 8));
  append_u32(out, word_54);
  append_u32(out, word_58);
  for (auto v : floats)
    append_u32(out, v);
  out.insert(out.end(), tail.begin(), tail.end());
  if (out.size() != CAMERA_SIZE)
    throw Error("camera image is " + std::to_string(out.size()) + " bytes");
  return out;
}

// jumpgate_saveDestinations 0x27dcf4: the count byte sign-extended to a word, then 0x1e0 bytes.
Bytes pack_jump(std::int32_t count, const Bytes &destinations) {
  if (destinations.size() != JUMP_DEST_SIZE)
    throw Error("jump destinations are " + std::to_string(destinations.size()) + " bytes");
  Bytes out;
  append_u32(out, static_cast<std::uint32_t>(s8(static_cast<std::uint8_t>(count))));
  out.insert(out.end(), destinations.begin(), destinations.end());
  return out;
}

std::string qualified_name(const std::string &level_name, const std::string &subname) {
  return subname.empty() ? level_name : level_name + "_" + subname;
}

// save_findAct 0x217d30. A table name matches when it is a prefix of name.
int find_act(const std::string &name, int current, const Acts &acts) {
  if (current != -1 && current >= 0 && std::size_t(current) < acts.size())
    for (const auto &row : acts[std::size_t(current)])
      if (starts_with(name, row.name))
        return current;
  for (std::size_t index = 0; index < acts.size(); ++index)
    for (const auto &row : acts[index])
      if (starts_with(name, row.name))
        return int(index);
  return -1;
}

// save_levelIndexInAct 0x2187a0. The current act only.
int level_index(const std::string &name, int act, const Acts &acts) {
  if (act < 0 || std::size_t(act) >= acts.size())
    return -1;
  const auto &rows = acts[std::size_t(act)];
  for (std::size_t index = 0; index < rows.size(); ++index)
    if (starts_with(name, rows[index].name))
      return int(index);
  return -1;
}

// item_encodeDefinitionIndex 0x107ea4 and item_decodeDefinitionIndex 0x107ec0.
std::int32_t definition_index(std::uint32_t pointer) {
  return static_cast<std::int32_t>(pointer - ITEM_TABLE) >> 6;
}
std::uint32_t definition_pointer(std::int32_t index) {
  return ITEM_TABLE + (static_cast<std::uint32_t>(index) << 6);
}
std::uint32_t item_address(std::uint32_t record_base, int bag, int index) {
  return record_base + std::uint32_t(BAG_ITEM_AT) + std::uint32_t(bag) * std::uint32_t(BAG_STRIDE) +
         std::uint32_t(index) * std::uint32_t(ITEM_SIZE);
}

// save_itemPtrToIndex 0x216bf8: bag * 1000 + slot, or -1.
std::int32_t pointer_to_index(std::uint32_t pointer, std::uint32_t record_base,
                              const std::array<std::int32_t, 3> &counts) {
  if (pointer == 0)
    return -1;
  for (int bag = 0; bag < BAGS; ++bag)
    for (std::int32_t index = 0; index < counts[std::size_t(bag)]; ++index)
      if (item_address(record_base, bag, index) == pointer)
        return bag * 1000 + index;
  return -1;
}

// save_itemIndexToPtr 0x216e10. -1 and any index that is not a counted slot become 0.
std::uint32_t index_to_pointer(std::int32_t index, std::uint32_t record_base,
                               const std::array<std::int32_t, 3> &counts) {
  if (index == -1)
    return 0;
  std::int64_t base = 0;
  for (int bag = 0; bag < BAGS; ++bag) {
    for (std::int32_t slot = 0; slot < counts[std::size_t(bag)]; ++slot)
      if (slot + base == index)
        return item_address(record_base, bag, slot);
    base += 1000;
  }
  return 0;
}

// save_snapshotPlayer 0x216c80: a copy, then equipment indices and definition row numbers.
Bytes encode_saved_record(const Bytes &live, std::uint32_t live_base) {
  if (live.size() < RECORD_SIZE)
    throw Error("record is " + std::to_string(live.size()) + " bytes");
  Bytes saved(live.begin(), live.begin() + RECORD_SIZE);
  const auto counts = bag_counts(saved);
  for (auto offset : equipment_offsets())
    put_u32(saved, offset, std::uint32_t(pointer_to_index(u32_at(saved, offset), live_base, counts)));
  for (int bag = 0; bag < BAGS; ++bag)
    for (std::int32_t index = 0; index < counts[std::size_t(bag)]; ++index) {
      const auto offset = BAG_ITEM_AT + std::size_t(bag) * BAG_STRIDE + std::size_t(index) * ITEM_SIZE;
      if (offset + 4 > saved.size())
        break;
      put_u32(saved, offset, std::uint32_t(definition_index(u32_at(saved, offset))));
    }
  return saved;
}

// save_fixupPlayer 0x216ea8, then save_load's second clear of the object pointer.
Bytes decode_loaded_record(const Bytes &saved, std::uint32_t live_base) {
  if (saved.size() < RECORD_SIZE)
    throw Error("record is " + std::to_string(saved.size()) + " bytes");
  Bytes live(saved.begin(), saved.begin() + RECORD_SIZE);
  put_u32(live, OBJECT_POINTER_AT, 0);
  const auto counts = bag_counts(live);
  for (int bag = 0; bag < BAGS; ++bag)
    for (std::int32_t index = 0; index < counts[std::size_t(bag)]; ++index) {
      const auto offset = BAG_ITEM_AT + std::size_t(bag) * BAG_STRIDE + std::size_t(index) * ITEM_SIZE;
      if (offset + 4 > live.size())
        break;
      put_u32(live, offset, definition_pointer(s32_at(live, offset)));
    }
  for (auto offset : equipment_offsets())
    put_u32(live, offset, index_to_pointer(s32_at(live, offset), live_base, counts));
  return live;
}

// First byte subtracted, every later byte added, each sign-extended. 32-bit wrap.
std::uint32_t mix_bytes(const std::uint8_t *data, std::size_t size) {
  std::uint32_t acc = 0;
  for (std::size_t i = 0; i < size; ++i) {
    const auto extended = static_cast<std::uint32_t>(s8(data[i]));
    acc = i == 0 ? acc - extended : acc + extended;
  }
  return acc;
}

// player_checksum 0x1bdc98 (docs/save-format.md "Checksum", steps 1..9).
std::uint32_t player_checksum(int slot, std::uint32_t address, const Bytes &record) {
  if (record.size() < 0x4434)
    throw Error("checksum needs the record through +0x4434");
  const unsigned shift = unsigned(slot) & 31u;
  const auto b = [&](std::size_t offset) { return static_cast<std::uint32_t>(s8(record[offset])); };
  const std::uint32_t appearance = mul32((b(0x20) + b(0x23)) + (b(0x22) + b(0x21)), 0x35);
  const std::uint32_t levels = (b(0x58) + b(0x59)) ^ LEVEL_XOR;
  std::uint32_t acc = (address << shift) ^ CHECK_XOR;
  acc = acc + appearance - levels;
  acc ^= mix_word(record, 0x5C);
  acc ^= times35(mix_word(record, 0xE4));
  for (std::uint32_t index = 0; index < 6; ++index)
    acc += mul32(mix_word(record, 0x90 + 4 * index), index);
  const auto mana = mix_word(record, 0xC0), maximum = mix_word(record, 0xC4), pending = mix_word(record, 0xC8);
  acc -= (mana + maximum) - pending;
  acc -= times3(mix_word(record, 0xE8));
  acc = static_cast<std::uint32_t>(static_cast<std::int32_t>(acc) >> shift);
  acc += mix_bytes(record.data() + 0x3748, 0xA88);
  acc += mix_bytes(record.data() + EQUIPMENT_AT, 0x44);
  return acc;
}

// The buffer save_build 0x221914 leaves, including the level directory when the store runs.
Bytes build_slot(const SlotInputs &inputs) {
  Bytes buf;
  if (!inputs.previous) {
    buf.assign(FILE_SIZE, 0);
  } else {
    if (inputs.previous->size() != FILE_SIZE)
      throw Error("previous image is " + std::to_string(inputs.previous->size()) + " bytes");
    buf = *inputs.previous;
  }
  const auto qualified = qualified_name(inputs.level_name, inputs.subname);
  if (!inputs.game_complete && find_act(qualified, inputs.act, inputs.acts) < 0)
    return buf;
  if (inputs.game_complete)
    std::copy(COMPLETE_NAME, COMPLETE_NAME + COMPLETE_NAME_SIZE, buf.begin());
  else
    write_cstr(buf, 0, inputs.level_name, 0x14);
  write_cstr(buf, 0x14, inputs.subname, 0x50);
  put_u32(buf, 0x64, inputs.play_time);
  put_u16(buf, 0x68, RECORD_SIZE);
  put_u16(buf, 0x6A, inputs.one_player ? 1u : std::uint32_t(inputs.num_players));
  buf[0x6C] = inputs.difficulty;
  buf[0x6D] = inputs.save_arg;
  put_u16(buf, 0x6E, std::uint32_t(inputs.act));
  const Bytes camera = inputs.camera ? *inputs.camera : Bytes(CAMERA_SIZE, 0);
  if (camera.size() != CAMERA_SIZE)
    throw Error("camera image is " + std::to_string(camera.size()) + " bytes");
  std::copy(camera.begin(), camera.end(), buf.begin() + HEADER_SIZE);
  const Bytes jump = pack_jump(inputs.jump_count, inputs.jump_dest ? *inputs.jump_dest : Bytes(JUMP_DEST_SIZE, 0));
  std::copy(jump.begin(), jump.end(), buf.begin() + 0x134);
  put_u32(buf, 0x318, inputs.minimap);
  put_u32(buf, 0x31C, std::uint32_t(inputs.num_players));
  std::size_t cursor = PLAYERS_AT;
  for (int slot = 0; slot < PLAYER_SLOTS; ++slot) {
    if (slot < inputs.num_players) {
      if (std::size_t(slot) >= inputs.players.size())
        throw Error("player " + std::to_string(slot) + " is below the player count and was not supplied");
      const auto &player = inputs.players[std::size_t(slot)];
      const auto *source = pose_source(inputs.players, std::size_t(slot), inputs.num_players, inputs.alive_first);
      if (!source) {
        std::fill(buf.begin() + static_cast<std::ptrdiff_t>(cursor),
                  buf.begin() + static_cast<std::ptrdiff_t>(cursor + POSE_SIZE), 0);
      } else {
        put_u16(buf, cursor, source->facing);
        for (std::size_t i = 0; i < 3; ++i)
          put_u32(buf, cursor + 2 + 4 * i, source->position[i]);
      }
      const auto saved = encode_saved_record(player.record, player.live_base);
      std::copy(saved.begin(), saved.end(), buf.begin() + static_cast<std::ptrdiff_t>(cursor + POSE_SIZE));
    }
    cursor += BLOCK_SIZE;
  }
  while (cursor & 3)
    ++cursor;
  for (const auto &[name, value] : inputs.quests) {
    if (cursor + QUEST_STRIDE > FILE_SIZE)
      throw Error("quest records run past the end of the image");
    write_cstr(buf, cursor, name, 0x20);
    put_u32(buf, cursor + 0x20, std::uint32_t(value));
    cursor += QUEST_STRIDE;
  }
  if (!inputs.game_complete)
    store_level(buf, qualified, inputs);
  return buf;
}

// save_setPlayerCount 0x218520 writes the u16 and the u32 count.
Bytes set_player_count(const Bytes &image, std::int32_t count) {
  if (image.size() < PLAYERS_AT)
    throw Error("slot image is short");
  Bytes buf = image;
  put_u16(buf, 0x6A, std::uint32_t(count));
  put_u32(buf, 0x31C, std::uint32_t(count));
  return buf;
}

SlotFile parse_slot(const Bytes &data, bool strict) {
  if (data.size() != FILE_SIZE)
    throw Error("slot image is " + std::to_string(data.size()) + " bytes, the file is " + std::to_string(FILE_SIZE));
  SlotFile out;
  out.record_size = u16_at(data, 0x68);
  out.listed_count = u16_at(data, 0x6A);
  out.player_count = s32_at(data, 0x31C);
  if (strict && out.record_size != RECORD_SIZE)
    throw Error("record size field is " + hex(out.record_size));
  if (strict && !(0 <= out.player_count && out.player_count <= PLAYER_SLOTS))
    throw Error("player count " + std::to_string(out.player_count) + " is outside 0..4");
  const int count = std::clamp(out.player_count, 0, PLAYER_SLOTS);
  for (int slot = 0; slot < count; ++slot)
    out.characters.push_back(parse_character(data, PLAYERS_AT + std::size_t(slot) * BLOCK_SIZE, strict));
  std::size_t cursor = QUESTS_AT;
  while (cursor < DIRECTORY_AT) {
    if (data[cursor] == 0)
      break;
    if (cursor + QUEST_STRIDE > DIRECTORY_AT) {
      if (strict)
        throw Error("quest record at " + hex(cursor) + " runs into the level directory");
      break;
    }
    const auto name = read_cstr(data, cursor, 0x20);
    if (!name) {
      if (strict)
        throw Error("quest name at " + hex(cursor) + " has no terminator");
      break;
    }
    out.quests.emplace_back(*name, s32_at(data, cursor + 0x20));
    cursor += QUEST_STRIDE;
  }
  std::uint64_t consumed = 0;
  for (std::size_t slot = 0; slot < DIRECTORY_SLOTS; ++slot) {
    const auto offset = DIRECTORY_AT + slot * DIRECTORY_STRIDE;
    if (data[offset] == 0)
      break;
    const auto name = read_cstr(data, offset, 0x14);
    if (!name) {
      if (strict)
        throw Error("level name in directory slot " + std::to_string(slot) + " has no terminator");
      break;
    }
    const std::uint32_t size = u32_at(data, offset + 0x14);
    const std::uint64_t start = DATA_AT + consumed;
    if (start + size > FILE_SIZE) {
      if (strict)
        throw Error("level " + *name + " needs " + std::to_string(size) + " bytes past the end of the file");
      break;
    }
    LevelBlob level;
    level.name = *name;
    level.size = size;
    level.data.assign(data.begin() + static_cast<std::ptrdiff_t>(start),
                      data.begin() + static_cast<std::ptrdiff_t>(start + size));
    split_frame(level);
    classify_level(level);
    out.levels.push_back(std::move(level));
    consumed += size;
  }
  // Native reader rule: the stride a search reads past the ten slots was not executed.
  if (strict && std::any_of(data.begin() + ELEVENTH_DIRECTORY_AT, data.begin() + DATA_AT,
                            [](std::uint8_t byte) { return byte != 0; }))
    throw Unknown("the 0x18 bytes at +0x16100 are not zero; what an eleventh level name does there is not documented");
  const auto level_name = read_cstr(data, 0, 0x14);
  out.level_name = level_name.value_or("");
  out.subname = read_cstr(data, 0x14, 0x50).value_or("");
  if (strict && !level_name)
    throw Error("level name has no terminator");
  out.play_time = u32_at(data, 0x64);
  out.difficulty = data[0x6C];
  out.save_arg = data[0x6D];
  out.act = s16_at(data, 0x6E);
  out.camera.assign(data.begin() + HEADER_SIZE, data.begin() + HEADER_SIZE + CAMERA_SIZE);
  out.jump_count = s32_at(data, 0x134);
  out.jump_destinations.assign(data.begin() + 0x138, data.begin() + 0x138 + JUMP_DEST_SIZE);
  out.minimap_mode = u32_at(data, 0x318);
  out.raw = data;
  return out;
}

Character import_character(const Bytes &data, int index, bool strict) {
  auto slot = parse_slot(data, strict);
  if (index < 0 || std::size_t(index) >= slot.characters.size())
    throw Error("character " + std::to_string(index) + " is outside the " +
                std::to_string(slot.characters.size()) + " loaded record(s)");
  return std::move(slot.characters[std::size_t(index)]);
}

// config_setDefaults 0x1e6238. scheme is the byte at 0x41b69c.
Bytes config_set_defaults(int which, int scheme, const Bytes *image) {
  Bytes buf = image ? *image : Bytes(CONFIG_SIZE, 0);
  if (buf.size() < CONFIG_SIZE)
    throw Error("config image is short");
  std::vector<int> slots;
  if (which == -1) {
    const auto half = float_bits(0.5f);
    put_u32(buf, 0x04, half);
    put_u32(buf, 0x08, half);
    put_u32(buf, 0x0C, half);
    for (auto [offset, value] : std::initializer_list<std::pair<std::size_t, std::uint32_t>>{
             {0x00, 0}, {0x10, 1}, {0x14, 1}, {0x18, 1}, {0x1C, 1}, {0x20, 1}, {0x24, 0},
             {0x28, 1}, {0x2C, 1}, {0x30, 0}, {0xC4, 1}, {0xD0, 1}, {0xD4, 0}})
      put_u32(buf, offset, value);
    put_u32(buf, 0xC8, 0x3E199999);
    put_u32(buf, 0xCC, half);
    slots = {0, 1, 2, 3};
  } else if (which >= 0 && which < 4) {
    slots = {which};
  }
  if (const auto row = button_row(scheme))
    for (int slot : slots)
      for (std::size_t index = 0; index < 9; ++index)
        put_u32(buf, 0x34 + std::size_t(slot) * 0x24 + 4 * index, (*row)[index]);
  buf.resize(CONFIG_SIZE);
  return buf;
}

ConfigFile parse_config(const Bytes &data, bool) {
  if (data.size() != CONFIG_SIZE)
    throw Error("config image is " + std::to_string(data.size()) + " bytes, the file is " + std::to_string(CONFIG_SIZE));
  ConfigFile out;
  for (std::size_t slot = 0; slot < 4; ++slot)
    for (std::size_t index = 0; index < 9; ++index)
      out.buttons[slot][index] = u32_at(data, 0x34 + slot * 0x24 + 4 * index);
  out.brightness = u32_at(data, 0x04);
  out.music = u32_at(data, 0x08);
  out.effects = u32_at(data, 0x0C);
  out.stereo = u32_at(data, 0x10);
  out.vibration = u32_at(data, 0x14);
  out.unnamed_18 = u32_at(data, 0x18);
  out.subtitles = u32_at(data, 0x1C);
  out.damage_numbers = u32_at(data, 0x20);
  out.blood = u32_at(data, 0x24);
  out.markers = u32_at(data, 0x28);
  out.combat_camera = u32_at(data, 0x2C);
  out.reverse_yaw = u32_at(data, 0x30);
  out.voice.assign(data.begin() + 0xC4, data.begin() + 0xD8);
  out.profile = profile_entries(data);
  out.raw = data;
  return out;
}

void require_profile(const std::string &profile) {
  const auto *found = findGameProfile(profile);
  if (!found)
    throw Error("unknown profile '" + profile + "'; the PS2 save import needs " + PROFILE);
  if (found->title != GameTitle::ReturnToArms || profile != PROFILE)
    throw Error(std::string(found->display_name) + " (" + profile +
                ") has no documented memory-card format; only " + PROFILE + " is supported");
}

// save_load 0x221d60, steps 1..10 (docs/save-format.md "Load path").
std::vector<std::string> apply_load(builtins::Variables &variables, const Bytes &data) {
  const auto slot = parse_slot(data, true);
  auto &memory = variables.memory;
  // Refusals that must happen before any write.
  if (memory.word(SHARE_QUEST_ITEMS) == 1)
    throw Unknown("the word at 0x4f85b4 is 1, so save_load would call inventory_shareQuestItems(0), which is not modelled");
  const auto pending = qualified_name(slot.level_name, slot.subname);
  if (pending.size() + 1 > PENDING_LEVEL_ROOM)
    throw Error("qualified level name '" + pending + "' does not fit before the pending-level marker");
  if (slot.difficulty & 0x80)
    throw Unknown("difficulty byte " + hex(slot.difficulty) + " is negative; its sign-extended store width is not documented");
  std::vector<std::string> notes;
  // 1. The header copy.
  memory.write(SAVE_HEADER_COPY, Bytes(data.begin(), data.begin() + HEADER_SIZE));
  // 2. Camera (refused, not restored) and jump gates.
  notes.emplace_back("camera: the load side of Camera_saveState is not documented; the 0xc4 bytes were not applied");
  memory.write(JUMP_DESTS, slot.jump_destinations);
  memory.byte(JUMP_COUNT, static_cast<std::uint8_t>(slot.jump_count));
  // 3. Minimap mode and the u32 player count.
  memory.word(MINIMAP, slot.minimap_mode);
  memory.word(NUM_PLAYERS, std::uint32_t(slot.player_count));
  // 4. Pose into the spawn fields, record into g_players, fixup, object pointer zeroed.
  for (std::size_t i = 0; i < slot.characters.size(); ++i) {
    const auto block_at = PLAYERS_AT + i * BLOCK_SIZE;
    memory.w16(SPAWN_FACING + std::uint32_t(i) * 2, u16_at(data, block_at));
    memory.write(SPAWN_POS + std::uint32_t(i) * 0xC,
                 Bytes(data.begin() + static_cast<std::ptrdiff_t>(block_at + 2),
                       data.begin() + static_cast<std::ptrdiff_t>(block_at + POSE_SIZE)));
    const auto base = PLAYERS + std::uint32_t(i) * std::uint32_t(RECORD_SIZE);
    memory.write(base, decode_loaded_record(slot.characters[i].raw, base));
  }
  // 5. script_clearDeferred, vars_clearAll, then vars_set for each record.
  variables.clear_deferred();
  variables.quest_clear_all();
  for (const auto &[name, value] : slot.quests)
    variables.quest_set(name, value);
  // 6. Loaded flag, act, difficulty, play time.
  memory.byte(LOADED_FROM_SAVE, 1);
  memory.word(CURRENT_ACT, static_cast<std::uint32_t>(std::int32_t(slot.act)));
  memory.byte(DIFFICULTY, slot.difficulty);
  memory.word(PLAY_TIME, slot.play_time);
  // 7. Pending level and its marker. 8. The loaded name.
  memory.string(PENDING_LEVEL, pending);
  memory.byte(PENDING_MARKER, PENDING_MARKER_VALUE);
  memory.string(LOADED_NAME, pending);
  // 9. The share word is zero (checked above). 10. Autosave countdown.
  memory.word(AUTOSAVE_COUNTDOWN, AUTOSAVE_AFTER_LOAD);
  for (const auto &level : slot.levels) {
    if (!level.world_refused.empty())
      notes.push_back("level " + level.name + " world: " + level.world_refused);
    if (!level.decal_refused.empty())
      notes.push_back("level " + level.name + " decal: " + level.decal_refused);
  }
  return notes;
}

ImportedCharacter import_into(const Bytes &data, int index, const std::string &profile,
                              std::optional<std::uint32_t> expected_checksum) {
  require_profile(profile);
  ImportedCharacter out;
  out.slot = parse_slot(data, true);
  if (index < 0 || std::size_t(index) >= out.slot.characters.size())
    throw Error("character " + std::to_string(index) + " is outside the " +
                std::to_string(out.slot.characters.size()) + " loaded record(s)");
  out.index = index;
  out.character = out.slot.characters[std::size_t(index)];
  out.record_address = PLAYERS + std::uint32_t(index) * std::uint32_t(RECORD_SIZE);
  const auto live = decode_loaded_record(out.character.raw, out.record_address);
  out.checksum = player_checksum(index, out.record_address, live);
  if (expected_checksum && *expected_checksum != out.checksum)
    throw Error("player checksum " + hex(out.checksum) + " does not match the expected " + hex(*expected_checksum) +
                "; nothing was written");
  out.memory = std::make_shared<builtins::Memory>();
  out.variables = std::make_unique<builtins::Variables>(out.memory);
  out.refusals = apply_load(*out.variables, data);
  out.live_quests = out.variables->quest_vars();
  const auto &c = out.character;
  out.player.class_id = c.class_index;
  out.player.level = c.level;
  out.player.strength = c.base_stats[0];
  out.player.intelligence = c.base_stats[1];
  out.player.dexterity = c.base_stats[2];
  out.player.stamina = c.base_stats[3];
  for (const auto &[id, rank] : c.skills)
    out.player.skills[id] = rank;
  return out;
}

std::array<std::vector<items::Item>, 3> inventory(const items::Tables &tables, const ImportedCharacter &imported) {
  if (!imported.memory)
    throw Error("no imported character");
  std::array<std::vector<items::Item>, 3> out;
  const auto &memory = *imported.memory;
  for (int bag = 0; bag < BAGS; ++bag) {
    const auto count = memory.s32(imported.record_address + std::uint32_t(BAG_COUNT_AT + std::size_t(bag) * BAG_STRIDE));
    for (std::int32_t index = 0; index < count; ++index) {
      const auto at = item_address(imported.record_address, bag, index);
      auto item = items::Item::unpack(tables, memory.read(at, ITEM_SIZE));
      if (!item.definition)
        throw Error("bag " + std::to_string(bag) + " item " + std::to_string(index) + " names definition " +
                    hex(memory.word(at)) + ", which the loaded item tables do not hold");
      out[std::size_t(bag)].push_back(std::move(item));
    }
  }
  return out;
}

void apply_config(builtins::Memory &memory, const Bytes &data) {
  parse_config(data, true);
  memory.write(SAVED_CONFIG, data);
}

void file_checksum() {
  throw Unknown("the slot and options files carry no checksum (save_build and save_load compute none); "
                "player_checksum is the netplay mix and is checked only against a supplied value");
}
void voice_global_pairing() {
  throw Unknown("which option word at +0xc4..+0xd4 pairs with which voice global was not traced");
}
void config_open_name() {
  throw Unknown("whether the 760-byte options open uses BASLUS-20973 or BASLUS-20565_config is not documented");
}
void apply_camera() {
  throw Unknown("the load side of Camera_saveState 0x203918 is not documented");
}
void eleventh_directory_name() {
  throw Unknown("what an eleventh distinct level name does with the 0x18 bytes at +0x16100 was not executed");
}
void require_known_world(const LevelBlob &level) {
  if (!level.world_refused.empty())
    throw Unknown("level " + level.name + " world: " + level.world_refused);
}
void require_known_decal(const LevelBlob &level) {
  if (!level.decal_refused.empty())
    throw Unknown("level " + level.name + " decal: " + level.decal_refused);
}

std::string config_field_name(std::size_t offset) {
  switch (offset) {
  case 0x04: return "brightness";
  case 0x08: return "music";
  case 0x0C: return "effects";
  case 0x10: return "stereo";
  case 0x14: return "vibration";
  case 0x1C: return "subtitles";
  case 0x20: return "damage_numbers";
  case 0x24: return "blood";
  case 0x28: return "markers";
  case 0x2C: return "combat_camera";
  case 0x30: return "reverse_yaw";
  default: break;
  }
  if (offset >= 0x34 && offset < 0xC4 && offset % 4 == 0)
    return "buttons[" + std::to_string((offset - 0x34) / 0x24) + "][" + std::to_string((offset - 0x34) % 0x24 / 4) + "]";
  if (offset >= 0xF8 && offset < CONFIG_SIZE)
    return "profile";
  if (offset >= 0xC4 && offset < 0xD8)
    voice_global_pairing();
  throw Unknown("option offset " + hex(offset) + " has no documented name");
}
} // namespace champions::simulation::rta::ps2_save
