// Created-character appearance contracts. Citations: see char_appearance.hpp
// (CCA = research-char-appearance@7931517 docs/spec/created-character-appearance.md,
//  REF = tools/ref_char_attachment.py at that commit, SE4 =
//  research-loading-readiness@366b65a docs/starting-equipment-r4.md).
#include "champions/char_appearance.hpp"

namespace champions::simulation::rta::char_appearance {
namespace {
// The shell and draw-argument constants run in the R5900 truncating model
// (CCA:303-305, 313-316).
const Fpu PS2{Fpu::Model::Ps2};
} // namespace

std::uint32_t selection_archive(std::int32_t players, bool override_byte) {
  // CCA:146-149: two players, or one player with 0x506040 set, select the
  // second pointer; ordinary single player selects the first.
  return (players >= 2 || override_byte) ? SECOND_ARCHIVE : FIRST_ARCHIVE;
}

std::string format_resource(const std::string &format, const std::string &prefix,
                            std::optional<std::int32_t> number,
                            std::optional<std::int32_t> letter) {
  std::string out;
  unsigned used_s = 0, used_d = 0, used_c = 0;
  for (std::size_t i = 0; i < format.size(); ++i) {
    const char ch = format[i];
    if (ch != '%') {
      out.push_back(ch);
      continue;
    }
    if (i + 1 >= format.size())
      throw Refused("resource template ends inside a conversion");
    const char spec = format[++i];
    if (spec == '%') {
      out.push_back('%');
    } else if (spec == 's') {
      if (used_s++ || used_d || used_c)
        throw Refused("resource template %s is not the first argument");
      out += prefix;
    } else if (spec == 'd') {
      if (!used_s || used_d++ || used_c || !number)
        throw Refused("resource template %d does not follow the prefix");
      out += std::to_string(*number);
    } else if (spec == 'c') {
      if (!used_d || used_c++ || !letter)
        throw Refused("resource template %c does not follow the number");
      out.push_back(static_cast<char>(static_cast<unsigned char>(*letter)));
    } else {
      throw Refused("resource template uses an unsupported conversion");
    }
  }
  if (!used_s || bool(number) != bool(used_d) || bool(letter) != bool(used_c))
    throw Refused("resource template argument count does not match the request");
  return out;
}

std::vector<Request> constructor_requests(const std::string &prefix,
                                          const Templates &t, Choices c) {
  // CCA:158-161: hair pair, beard pair, then body pair.
  std::vector<Request> out;
  auto optional_layer = [&](Layer layer, std::int8_t style, const std::string &mesh,
                            std::uint32_t mesh_at, std::uint32_t mesh_format,
                            const std::string &texture, std::uint32_t texture_at,
                            std::uint32_t texture_format) {
    if (style == -1)
      return; // CCA:159: style -1 skips both requests
    out.push_back({layer, Kind::Mesh, mesh_at, mesh_format,
                   format_resource(mesh, prefix, std::int32_t(style) + 1)});
    out.push_back({layer, Kind::Texture, texture_at, texture_format,
                   format_resource(texture, prefix, std::int32_t(style) + 1,
                                   std::int32_t(c.colour) + 97)});
  };
  // CCA:154-155 object offsets.
  optional_layer(Layer::Hair, c.hair, t.hair_mesh, 0x548, HAIR_MESH_FORMAT, t.hair_texture,
                 0x558, HAIR_TEXTURE_FORMAT);
  optional_layer(Layer::Beard, c.beard, t.beard_mesh, 0x54c, BEARD_MESH_FORMAT,
                 t.beard_texture, 0x554, BEARD_TEXTURE_FORMAT);
  // CCA:156, 160: fixed body mesh; skin + 1 selects the texture.
  out.push_back({Layer::Body, Kind::Mesh, 0x544, BODY_MESH_FORMAT,
                 format_resource(t.body_mesh, prefix)});
  out.push_back({Layer::Body, Kind::Texture, 0x550, BODY_TEXTURE_FORMAT,
                 format_resource(t.body_texture, prefix, std::int32_t(c.skin) + 1)});
  return out;
}

Handles constructor_handles(const std::string &prefix, const Templates &templates,
                            Choices choices, const Lookup &lookup, Handles prior) {
  if (!lookup)
    throw Refused("constructor handles need a resource lookup");
  Handles h = prior;
  // CCA:159: a skipped optional layer clears both of its handles.
  if (choices.hair == -1)
    h.hair = {};
  if (choices.beard == -1)
    h.beard = {};
  for (const auto &r : constructor_requests(prefix, templates, choices)) {
    // CCA:162-164: null lookup results stay null, replacing poisoned values.
    const auto value = lookup(r);
    Pair &pair = r.layer == Layer::Hair ? h.hair : r.layer == Layer::Beard ? h.beard : h.body;
    (r.kind == Kind::Mesh ? pair.mesh : pair.texture) = value;
  }
  return h;
}

bool cover_flag(std::int32_t model, std::int32_t slot, bool prior, bool paired,
                CoverHits hits) {
  // CCA:182-191; REF:25-33.
  if (!paired)
    return prior;
  const bool head = slot == 11 && !hits.name_a && (model != 7 || !hits.picture_b);
  const bool body = slot == 4 && ((model == 7 && hits.picture_c) ||
                                  (model == 9 && (hits.name_d || hits.picture_e)));
  return prior || head || body;
}

Gates appearance_gates(std::int32_t model, std::int32_t skin, bool covered,
                       bool head_present, bool head_exception) {
  // CCA:193-196; REF:36-40.
  Gates g;
  g.hair = !covered || model == 11 || (model == 10 && skin == 4);
  g.beard = !covered || model == 11 || (head_present && (head_exception || model == 6));
  return g;
}

Draws appearance_draws(std::int32_t model, std::int32_t skin, bool covered,
                       bool head_present, bool head_exception, const Handles &h) {
  // CCA:196-198: each layer independently requires mesh and texture.
  const auto g = appearance_gates(model, skin, covered, head_present, head_exception);
  return {g.hair && h.hair.both(), g.beard && h.beard.both()};
}

std::uint64_t variant_show_mask(std::uint64_t default_hidden, bool enabled,
                                std::uint32_t selection,
                                const std::array<VariantRecord, 32> &records) {
  // CCA:203-205; REF:55-62.
  std::uint64_t show = ~default_hidden;
  if (enabled)
    for (unsigned bit = 0; bit < 32; ++bit)
      if (selection & (std::uint32_t(1) << bit))
        show = (show | records[bit].show) & ~records[bit].hide;
  return show;
}

std::uint64_t initial_body_mask(std::uint32_t counter, std::uint64_t default_hidden,
                                bool enabled,
                                const std::array<VariantRecord, 32> &records) {
  // CCA:201-203; REF:65-70. Signed compare against 0x111.
  return s32(counter) < BODY_MASK_COUNTER_LIMIT
             ? ALL_SHOWN
             : variant_show_mask(default_hidden, enabled, 1, records);
}

void BodyMask::part_drawn(std::uint64_t part_hide) {
  if (!open_)
    throw Refused("a part was drawn outside an equipment pass");
  pass_ &= ~part_hide; // CCA:205-206
}

void BodyMask::end_equipment_pass() {
  if (!open_)
    throw Refused("no equipment pass is open");
  stored_ = pass_; // object +0x220, read by the next body draw (CCA:430-433)
  open_ = false;
}

std::vector<std::pair<std::uint32_t, std::uint32_t>>
mesh_runs(const std::vector<std::uint32_t> &boundaries, std::uint64_t show) {
  // REF:96-122.
  if (boundaries.size() < 2 || boundaries.size() > 65)
    throw Refused("mesh run walk is outside the authored 1..64 mesh boundary");
  const std::size_t count = boundaries.size() - 1;
  if (count == 64) {
    // Original EE shift edge (CCA:230-232): not a correction.
    show &= ~std::uint64_t(1);
    if (show & (std::uint64_t(1) << 63)) {
      unsigned tail = 63;
      while (tail > 0 && (show & (std::uint64_t(1) << (tail - 1))))
        --tail;
      show &= (std::uint64_t(1) << tail) - 1;
    }
  }
  std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
  std::optional<std::uint32_t> start;
  for (std::size_t i = 0; i < boundaries.size(); ++i) {
    const bool shown = i < count && ((show >> i) & 1);
    if (shown && !start)
      start = boundaries[i];
    if (!shown && start) {
      if (boundaries[i] != *start)
        out.emplace_back(*start, boundaries[i] - *start);
      start.reset();
    }
  }
  return out;
}

std::optional<std::uint32_t> attachment_index(std::int32_t header, std::int32_t override_index) {
  // CCA:210-215; REF:73-80.
  if (header == -1)
    return std::nullopt;
  const std::int32_t index = override_index == -1 ? header : override_index;
  if (index < 0 || index >= 16)
    throw Refused("attachment index lies outside the authored 16-record table");
  return std::uint32_t(index);
}

std::uint32_t attachment_record_address(std::uint32_t parent, std::uint8_t byte22,
                                        std::uint32_t index) {
  if (index >= 16)
    throw Refused("attachment index lies outside the authored 16-record table");
  return parent + std::uint32_t(byte22) * 16 + index * 0x34; // CCA:216-218
}

std::uint32_t attachment_joint_word(std::int32_t joint) {
  return std::uint32_t(joint) * 4; // CCA:223-224
}

std::array<Raw, 12> attachment_matrix(const std::array<Raw, 12> &v, bool mirrored) {
  // REF:83-87: column-major packet, row 0 negated when mirrored.
  std::array<Raw, 12> out{};
  std::size_t k = 0;
  for (std::size_t col = 0; col < 3; ++col)
    for (std::size_t row = 0; row < 4; ++row)
      out[k++] = row == 0 ? (mirrored ? neg(v[col]) : v[col]) : v[3 * row + col];
  return out;
}

StartingVisuals starting_visual_slots(std::int32_t body) {
  // SE4:44-56 (REF:132-136): body -> (class index, weapon pointer slot).
  static constexpr std::array<std::array<std::int32_t, 2>, 12> map{{
      {0, 0}, {0, 0}, {3, 2}, {3, 2}, {1, 0}, {1, 0},
      {4, 2}, {4, 2}, {2, 0}, {2, 0}, {5, 0}, {6, 0}}};
  if (body < 0 || body >= 12)
    throw Refused("body lies outside the published 12-body starting map");
  const auto &row = map[std::size_t(body)];
  return {row[0], {std::uint32_t(row[1]), 4, 10, 12}};
}

std::uint32_t shield_variant_bit(std::uint32_t model) {
  // CCA:262-271; executed jump table 0x535e00.
  static constexpr std::array<std::uint32_t, 12> bits{{1, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9}};
  return model < 12 ? bits[model] : 0;
}

std::uint32_t shield_selection(std::uint32_t model) {
  return std::uint32_t(1) << shield_variant_bit(model);
}

std::int32_t equipped_override(std::uint32_t pointer_slot) {
  return pointer_slot == 1 ? 1 : -1; // CCA:284-291
}

PartDraw equipped_part_draw(std::uint32_t pointer_slot, std::uint32_t definition_slot,
                            std::uint32_t body_model, const PartHeader &part,
                            bool both_handles) {
  PartDraw d;
  d.drawn = both_handles;
  d.attach_override = equipped_override(pointer_slot);
  // CCA:262-273: only definition slot 3 calls 0x179550 on the part model with
  // the body-model bit; every other slot keeps the all-ones argument.
  if (definition_slot == 3) {
    d.variant_called = true;
    d.show_mask = variant_show_mask(part.default_hidden, part.table_enabled,
                                    shield_selection(body_model), part.records);
  }
  // CCA:292-296: 0x179978 whenever the override is not -1; model 2 also
  // calls 0x179860 (the mirrored matrix copy) before the draw.
  d.shell_reset = d.attach_override != -1;
  d.mirrored = body_model == 2 && d.attach_override != -1;
  if (d.drawn)
    d.attach_index = attachment_index(part.attach_0x13, d.attach_override);
  return d;
}

ShellState shell_fields(bool flag33, Raw argument) {
  // CCA:300-311; REF:295-315.
  ShellState s{SHELL_BLOCK, 0, std::nullopt};
  if (!flag33)
    return s;
  const ScopedFpu fpu(PS2);
  s.count = 2;
  Raw scale = add(mul(from_int(7), 0x3ccccccc), 0x3f199999);
  if (lt(ONE, scale))
    scale = ONE;
  ShellBlock b;
  b.byte3 = std::uint8_t(to_int(mul(scale, 0x42800000)) & 0xff);
  b.byte11 = std::uint8_t(to_int(mul(scale, 0x42900000)) & 0xff);
  b.float4 = mul(argument, 0x43480000);
  b.float12 = mul(argument, 0x42c80000);
  s.block = b;
  return s;
}

Raw equipped_draw_argument(Raw projection) {
  // CCA:313-317; REF:272-287.
  const ScopedFpu fpu(PS2);
  const Raw magnitude = fabs(projection);
  Raw argument = 0x40a00000;
  if (lt(magnitude, 0x43b40000)) {
    argument = sub(0x40a00000, mul(sub(0x43b40000, magnitude), 0x3c93e93e));
    if (lt(argument, ONE))
      argument = ONE;
  }
  return argument;
}

void reset_shell(ShellState &s) {
  s.pointer = SHELL_RESET_POINTER; // CCA:294-295
  s.count = 0;
}

bool shell_pass_gate(std::uint16_t flags, std::int32_t count) {
  return (flags & 0x20) && count > 0; // CCA:322-323, 437-439
}

std::optional<CreationStart> creation_restart(std::uint16_t h11a, std::uint8_t c118,
                                              bool import_literal, std::int8_t race) {
  // CCA:331-342.
  if (!(h11a & 0x80) || c118 >= 2)
    return std::nullopt;
  CreationStart s{(h11a & 0x08) ? CREATION_FORMAT_MARKED : CREATION_FORMAT, std::nullopt};
  if (import_literal) {
    const std::int32_t slot = std::int32_t(race) + 1;
    // Only bytes -1..11 (slots 0..12) were executed (CCA:339-342; the
    // creation rows check that the copied address exists,
    // tools/difftest_char_picture.py:409-410, 483-486). The reference keeps
    // the input name otherwise (REF:345-350), but no out-of-range byte was
    // ever run, so it is refused rather than guessed. It is not the
    // gate-failed "no restart".
    if (slot < 0 || slot >= std::int32_t(CREATION_RACE_LITERALS.size()))
      throw Refused("race byte outside the executed -1..11 copy range");
    s.race_literal = CREATION_RACE_LITERALS[std::size_t(slot)];
  }
  return s;
}

std::string creation_clip_name(const std::string &name, const std::string &format) {
  if (format.size() < 2 || format.compare(0, 2, "%s") != 0 ||
      format.find('%', 2) != std::string::npos)
    throw Refused("creation format is not a single leading substitution");
  return name + format.substr(2);
}

anim_clock::Track *install_creation_clip(anim_clock::AnimState &state, anim_clock::ClipPtr clip,
                                         Raw blend_word) {
  return anim_clock::anim_start(blend_word, state, std::move(clip), CREATION_FLAGS);
}

std::uint32_t inventory_lump_address(std::int32_t model) {
  if (model < 0 || model >= 12)
    throw Refused("inventory lump table covers models 0..11 only");
  return INVENTORY_LUMPS[std::size_t(model)];
}

std::uint32_t inventory_idle_address(std::int32_t weapon_class) {
  // CCA:390-392; REF:336-338.
  switch (weapon_class) {
  case 1: return 0x538538;
  case 2: return 0x538548;
  case 3: return 0x538558;
  case 4: return 0x538568;
  case 5: return 0x538578;
  case 7: return 0x538588;
  default: return 0x538598;
  }
}

anim_clock::Track *inventory_select_idle(anim_clock::AnimState &state, anim_clock::ClipPtr clip,
                                         Raw blend_word) {
  // CCA:393-396: an already-current clip keeps its frame.
  if (const auto *top = state.top(); top && clip && top->clip == clip)
    return nullptr;
  return anim_clock::anim_start(blend_word, state, std::move(clip), INVENTORY_FLAGS);
}

std::int32_t inventory_draw_step(anim_clock::AnimSystem &system, anim_clock::AnimState &state) {
  return anim_clock::anim_advance(system, ONE, state); // CCA:397-400
}

std::uint32_t display_idle_address(std::int32_t weapon_class) {
  // CCA:407-409; REF:339-340.
  switch (weapon_class) {
  case 1: return 0x5375d8;
  case 2: return 0x5375e8;
  case 3: return 0x5375f8;
  case 4: return 0x537608;
  case 7: return 0x537618;
  default: return DISPLAY_IDLE_DEFAULT;
  }
}

std::array<std::uint32_t, 4> idle_cell_offsets(std::int32_t weapon_class) {
  std::array<std::uint32_t, 4> out{};
  for (std::size_t i = 0; i < 4; ++i)
    out[i] = std::uint32_t(weapon_class) * IDLE_ROW_SIZE + IDLE_CELL_OFFSETS[i];
  return out;
}

bool reset_idle_anim(anim_clock::AnimState &state, bool player_present, Raw hp,
                     bool display_only, const IdleClips &clips) {
  // CCA:403-405: no player, or hit points not greater than 0, writes nothing.
  if (!player_present || !lt(ZERO, hp))
    return false;
  if (state.capacity < 4)
    throw Refused("player_resetIdleAnim needs four animation cells");
  if (display_only && !clips.display)
    throw Refused("player_resetIdleAnim: display idle clip unavailable");
  for (std::size_t i = 0; i < 4; ++i) {
    auto &cell = state.tracks[i];
    cell.clip = (i == 0 && display_only) ? clips.display : clips.table[i];
    cell.flags = IDLE_FLAGS;
    // cell 0 weight and life 1 (CCA:406-407); cells 1..3 are 0
    // (player-animation-selection.md:61). time and rate untouched (CCA:409-410).
    cell.weight = i == 0 ? ONE : ZERO;
    cell.life = i == 0 ? ONE : ZERO;
  }
  state.count = 4;
  return true;
}

} // namespace champions::simulation::rta::char_appearance
