// Synthetic checks for the created-character appearance contracts. Expected
// values come from research-char-appearance@153d1c3 (docs/spec/
// created-character-appearance.md "CCA:n", tools/ref_char_attachment.py
// "REF:n", audits research-char-appearance-r2 / -r3-fix). Templates, tokens
// and handles are synthetic; no retail name or byte is used.
#include "champions/char_appearance.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace ca = champions::simulation::rta::char_appearance;
namespace ac = champions::simulation::rta::animation_clock;
using champions::simulation::rta::ONE;
using champions::simulation::rta::Raw;
using champions::simulation::rta::ZERO;

namespace {
int checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << what << "\n";
    std::exit(1);
  }
}
template <class F> void refuses(F &&f, const std::string &what) {
  bool threw = false;
  try {
    f();
  } catch (const ca::Refused &) {
    threw = true;
  }
  check(threw, what);
}

// Synthetic templates with the conversion order of the six constructor
// formats (prefix; prefix, number; prefix, number, letter).
ca::Templates synthetic_templates() {
  return {"%s_h%d.vif", "%s_h%d%c.tex", "%s_b%d.vif", "%s_b%d%c.tex", "%s_body.vif",
          "%s_body%d.tex"};
}

void formats() {
  check(ca::format_resource("%s_h%d%c.tex", "xx", 2, 97 + 1) == "xx_h2b.tex", "printf subset");
  check(ca::format_resource("100%%_%s", "p") == "100%_p", "literal percent");
  refuses([] { ca::format_resource("%s_%x", "p", 1); }, "unsupported conversion refuses");
  refuses([] { ca::format_resource("%d_%s", "p", 1); }, "number before prefix refuses");
  refuses([] { ca::format_resource("%s_%d", "p"); }, "missing number refuses");
  refuses([] { ca::format_resource("%s", "p", 1); }, "unused number refuses");
  refuses([] { ca::format_resource("%s%", "p"); }, "dangling percent refuses");
  check(ca::selection_archive(1, false) == ca::FIRST_ARCHIVE, "single player first archive");
  check(ca::selection_archive(2, false) == ca::SECOND_ARCHIVE, "two players second archive");
  check(ca::selection_archive(1, true) == ca::SECOND_ARCHIVE, "override second archive");
}

void requests_and_handles() {
  const auto t = synthetic_templates();
  // CCA:158-161 order and offsets, style+1 and colour+97.
  auto r = ca::constructor_requests("pp", t, {1, 2, 0, 3});
  check(r.size() == 6, "six requests with both layers");
  const std::array<std::uint32_t, 6> offsets{{0x548, 0x558, 0x54c, 0x554, 0x544, 0x550}};
  const std::array<std::string, 6> names{
      {"pp_h2.vif", "pp_h2c.tex", "pp_b1.vif", "pp_b1c.tex", "pp_body.vif", "pp_body4.tex"}};
  for (std::size_t i = 0; i < 6; ++i)
    check(r[i].object_offset == offsets[i] && r[i].name == names[i], "request order " + std::to_string(i));
  check(r[0].format_address == ca::HAIR_MESH_FORMAT && r[5].format_address == ca::BODY_TEXTURE_FORMAT,
        "template addresses");
  check(ca::constructor_requests("pp", t, {-1, 0, 2, 0}).size() == 4, "hair -1 omits its pair");
  check(ca::constructor_requests("pp", t, {0, 0, -1, 0}).size() == 4, "beard -1 omits its pair");
  const auto body_only = ca::constructor_requests("pp", t, {-1, 0, -1, 1});
  check(body_only.size() == 2 && body_only[0].layer == ca::Layer::Body, "body pair always requested");

  // Null pairs: every lookup pattern over the six requests, from poisoned
  // prior handles (CCA:162-164).
  const ca::Handles poison{{0xdead, 0xbeef}, {0xdead, 0xbeef}, {0xdead, 0xbeef}};
  for (unsigned present = 0; present < 64; ++present) {
    std::set<std::string> have;
    for (unsigned i = 0; i < 6; ++i)
      if (present & (1u << i))
        have.insert(names[i]);
    const auto h = ca::constructor_handles(
        "pp", t, {1, 2, 0, 3},
        [&](const ca::Request &q) { return have.count(q.name) ? 0x1000 + q.object_offset : 0; }, poison);
    auto want = [&](unsigned i) { return (present & (1u << i)) ? 0x1000 + offsets[i] : 0u; };
    check(h.hair.mesh == want(0) && h.hair.texture == want(1) && h.beard.mesh == want(2) &&
              h.beard.texture == want(3) && h.body.mesh == want(4) && h.body.texture == want(5),
          "null lookups stay null over poison");
  }
  const auto skipped = ca::constructor_handles(
      "pp", t, {-1, 0, -1, 0}, [](const ca::Request &) { return 7u; }, poison);
  check(!skipped.hair.mesh && !skipped.hair.texture && !skipped.beard.mesh && !skipped.beard.texture,
        "style -1 clears both handles");
  check(skipped.body.mesh == 7 && skipped.body.texture == 7, "body pair still resolved");
}

void gates() {
  // CCA:182-191 cover rules, one row per condition.
  const ca::CoverHits none{};
  check(!ca::cover_flag(0, 11, false, false, none), "unpaired part cannot set cover");
  check(ca::cover_flag(0, 11, true, false, {true, true, true, true, true}), "cover is sticky");
  check(ca::cover_flag(0, 11, false, true, none), "head part without token A covers");
  check(!ca::cover_flag(0, 11, false, true, {true, false, false, false, false}), "token A uncovers head");
  check(!ca::cover_flag(7, 11, false, true, {false, true, false, false, false}), "model 7 picture B uncovers");
  check(ca::cover_flag(6, 11, false, true, {false, true, false, false, false}), "B only matters for model 7");
  check(ca::cover_flag(7, 4, false, true, {false, false, true, false, false}), "model 7 body token C");
  check(!ca::cover_flag(6, 4, false, true, {false, false, true, false, false}), "C only for model 7");
  check(ca::cover_flag(9, 4, false, true, {false, false, false, true, false}), "model 9 name D");
  check(ca::cover_flag(9, 4, false, true, {false, false, false, false, true}), "model 9 picture E");
  check(!ca::cover_flag(9, 4, false, true, none), "model 9 body without D/E");
  check(!ca::cover_flag(9, 3, false, true, {true, true, true, true, true}), "other slots never cover");

  // CCA:193-198 over models, skins, covers, head states and 16 handle patterns.
  for (int model = 0; model < 12; ++model)
    for (int skin = 0; skin < 5; ++skin)
      for (int bits = 0; bits < 8; ++bits) {
        const bool covered = bits & 1, head = bits & 2, exception = bits & 4;
        const auto g = ca::appearance_gates(model, skin, covered, head, exception);
        const bool hair = !covered || model == 11 || (model == 10 && skin == 4);
        const bool beard = !covered || model == 11 || (head && (exception || model == 6));
        check(g.hair == hair && g.beard == beard, "gate row");
        for (unsigned pattern = 0; pattern < 16; ++pattern) {
          ca::Handles h;
          h.hair = {(pattern & 1) ? 1u : 0u, (pattern & 2) ? 2u : 0u};
          h.beard = {(pattern & 4) ? 3u : 0u, (pattern & 8) ? 4u : 0u};
          const auto d = ca::appearance_draws(model, skin, covered, head, exception, h);
          check(d.hair == (hair && (pattern & 3) == 3) && d.beard == (beard && (pattern & 12) == 12),
                "each layer needs gate and both handles");
        }
      }
}

void masks() {
  std::array<ca::VariantRecord, 32> records{};
  records[0] = {0x1, 0x2};
  records[1] = {0x4, 0x8};
  records[2] = {0x0f, 0x100};
  // REF:55-62 evaluated on this synthetic table: 18446744073709486335.
  check(ca::variant_show_mask(0xff00, true, 0b101, records) == UINT64_C(18446744073709486335),
        "ascending show then hide");
  check(ca::variant_show_mask(0xff00, false, 0b101, records) == ~std::uint64_t(0xff00),
        "disabled table keeps the complement");
  check(ca::variant_show_mask(0, true, 0b11, {{{0, 0x4}, {0x4, 0}}}) == ~std::uint64_t(0),
        "later bit re-shows an earlier hide");
  check(ca::variant_show_mask(0, true, 1, {{{0x4, 0x4}}}) == ~std::uint64_t(0x4),
        "a record's hide wins its own show");
  // CCA:201-203: signed counter compare.
  check(ca::initial_body_mask(0x110, 0xff, true, records) == ca::ALL_SHOWN, "below 0x111 all ones");
  check(ca::initial_body_mask(0xffffffff, 0xff, true, records) == ca::ALL_SHOWN, "negative counter all ones");
  check(ca::initial_body_mask(0x111, 0xff, true, records) == ca::variant_show_mask(0xff, true, 1, records),
        "at 0x111 variant bit one");

  // CCA:403-419: the body uses the previous pass's stored mask.
  ca::BodyMask object(0x0123456789abcdefULL);
  check(object.body_draw_argument() == 0x0123456789abcdefULL, "first body draw reads +0x220 as stored");
  object.begin_equipment_pass(ca::ALL_SHOWN);
  object.part_drawn(0xf0);
  check(object.body_draw_argument() == 0x0123456789abcdefULL, "same-frame update not visible to body");
  object.end_equipment_pass();
  check(object.body_draw_argument() == ~std::uint64_t(0xf0), "next frame uses the pass result");
  object.begin_equipment_pass(ca::ALL_SHOWN);
  object.end_equipment_pass();
  check(object.stored() == ca::ALL_SHOWN, "no drawn part clears nothing");
  refuses([&] { object.part_drawn(1); }, "part outside a pass refuses");

  // REF:96-122 mesh runs, including an empty mesh.
  using Runs = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
  const std::vector<std::uint32_t> b{0, 16, 32, 32, 48};
  check(ca::mesh_runs(b, 0b1011) == Runs{{0, 32}, {32, 16}}, "runs 0b1011");
  check(ca::mesh_runs(b, ca::ALL_SHOWN) == Runs{{0, 48}}, "runs all ones");
  check(ca::mesh_runs(b, 0).empty(), "runs none");
  check(ca::mesh_runs(b, 0b0101) == Runs{{0, 16}}, "runs skip empty mesh");
  check(ca::mesh_runs({0, 16}, 1) == Runs{{0, 16}}, "one mesh");
  std::vector<std::uint32_t> b64;
  for (std::uint32_t i = 0; i <= 64; ++i)
    b64.push_back(i * 16);
  check(ca::mesh_runs(b64, ca::ALL_SHOWN).empty(), "64-mesh all-ones edge never flushes");
  check(ca::mesh_runs(b64, 0b111) == Runs{{16, 32}}, "64-mesh edge clears bit zero");
  refuses([] { ca::mesh_runs({0}, 1); }, "zero meshes refuse");
}

void attachments() {
  // CCA:210-227.
  check(!ca::attachment_index(-1, 3), "header -1 ignores a nonnegative override");
  check(!ca::attachment_index(-1, -1), "header -1 no joint");
  for (int i = 0; i < 16; ++i) {
    check(*ca::attachment_index(i, -1) == std::uint32_t(i), "header index used");
    check(*ca::attachment_index(5, i) == std::uint32_t(i), "override replaces header");
  }
  refuses([] { (void)ca::attachment_index(16, -1); }, "no clamp beyond 16");
  refuses([] { (void)ca::attachment_index(2, 17); }, "override beyond table refuses");
  check(ca::attachment_record_address(0x1000, 0x40, 3) == 0x1000 + 0x400 + 3 * 0x34, "record address");
  check(ca::attachment_joint_word(9) == 36, "joint times four");
  std::array<Raw, 12> v{};
  for (unsigned i = 0; i < 12; ++i)
    v[i] = champions::simulation::rta::from_int(i + 1);
  // REF:83-87 on 1..12.
  const std::array<Raw, 12> plain{{0x3f800000, 0x40800000, 0x40e00000, 0x41200000, 0x40000000,
                                   0x40a00000, 0x41000000, 0x41300000, 0x40400000, 0x40c00000,
                                   0x41100000, 0x41400000}};
  check(ca::attachment_matrix(v, false) == plain, "3x4 copy");
  auto mirrored = plain;
  mirrored[0] |= 0x80000000u;
  mirrored[4] |= 0x80000000u;
  mirrored[8] |= 0x80000000u;
  check(ca::attachment_matrix(v, true) == mirrored, "first row mirrored");
}

void equipment() {
  // SE4:44-56.
  const std::array<std::array<int, 2>, 12> map{{{0, 0}, {0, 0}, {3, 2}, {3, 2}, {1, 0}, {1, 0},
                                                {4, 2}, {4, 2}, {2, 0}, {2, 0}, {5, 0}, {6, 0}}};
  for (int body = 0; body < 12; ++body) {
    const auto s = ca::starting_visual_slots(body);
    check(s.class_index == map[std::size_t(body)][0] &&
              s.pointer_slots == std::array<std::uint32_t, 4>{
                                     {std::uint32_t(map[std::size_t(body)][1]), 4, 10, 12}},
          "starting slots");
  }
  refuses([] { (void)ca::starting_visual_slots(12); }, "outside the 12-body map");
  // CCA:266-269 shield bits; unsigned < 12 test.
  const std::array<std::uint32_t, 12> bits{{1, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9}};
  for (std::uint32_t m = 0; m < 12; ++m)
    check(ca::shield_variant_bit(m) == bits[m] && ca::shield_selection(m) == (1u << bits[m]), "shield bit");
  for (std::uint32_t m : {0xffffffffu, 12u, 15u})
    check(ca::shield_variant_bit(m) == 0, "models outside 0..11 use bit 0");

  ca::PartHeader part;
  part.attach_0x13 = 6;
  part.default_hidden = 0xf;
  part.table_enabled = true;
  for (unsigned i = 0; i < 32; ++i)
    part.records[i] = {std::uint64_t(1) << (8 + i), std::uint64_t(1) << i};
  // CCA:283-290: published slots on every body, show mask all ones, header
  // attach index, no variant call, no mirror (also on model 2).
  for (int body = 0; body < 12; ++body)
    for (auto slot : ca::starting_visual_slots(body).pointer_slots) {
      const auto d = ca::equipped_part_draw(slot, slot, std::uint32_t(body), part, true);
      check(d.drawn && d.show_mask == ca::ALL_SHOWN && !d.variant_called && d.attach_override == -1 &&
                d.attach_index == std::optional<std::uint32_t>(6) && !d.mirrored && !d.shell_reset,
            "starting slot draw");
    }
  // CCA:291-296 off-hand: override 1 on every model, 0x179978 reset, model 2 mirrors.
  for (std::uint32_t body = 0; body < 12; ++body) {
    const auto d = ca::equipped_part_draw(1, 1, body, part, true);
    check(d.attach_override == 1 && d.attach_index == std::optional<std::uint32_t>(1) && d.shell_reset &&
              d.mirrored == (body == 2),
          "off-hand override");
    // Shield in that iteration keeps the body-model bit.
    const auto s = ca::equipped_part_draw(1, 3, body, part, true);
    check(s.variant_called &&
              s.show_mask == ca::variant_show_mask(0xf, true, 1u << bits[body], part.records),
          "shield uses body bit");
  }
  ca::PartHeader none = part;
  none.attach_0x13 = -1;
  check(!ca::equipped_part_draw(1, 1, 0, none, true).attach_index, "header -1 beats off-hand override");
  check(!ca::equipped_part_draw(4, 4, 0, part, false).drawn, "missing handle is not drawn");

  // A linked pass: drawn parts clear their hide masks (CCA:288-289).
  ca::BodyMask object(ca::ALL_SHOWN);
  object.begin_equipment_pass(ca::ALL_SHOWN);
  for (auto slot : ca::starting_visual_slots(4).pointer_slots) {
    ca::PartHeader p;
    p.hide = std::uint64_t(1) << slot;
    if (ca::equipped_part_draw(slot, slot, 4, p, slot != 10).drawn)
      object.part_drawn(p.hide);
  }
  object.end_equipment_pass();
  check(object.stored() == ~((1ull << 0) | (1ull << 4) | (1ull << 12)), "only drawn parts clear");
}

void shell() {
  // CCA:300-311; audit r2 values.
  const auto clear = ca::shell_fields(false, 0x40a00000);
  check(clear.pointer == ca::SHELL_BLOCK && clear.count == 0 && !clear.block, "flag 33 clear");
  // REF:272-315 evaluated on synthetic projections.
  struct Row {
    Raw projection, argument, f4, f12;
  };
  const Row rows[] = {
      {0x00000000, 0x3f800000, 0x43480000, 0x42c80000}, {0x3f800000, 0x3f800000, 0x43480000, 0x42c80000},
      {0x42c80000, 0x3f800000, 0x43480000, 0x42c80000}, {0x43480000, 0x40071c73, 0x43d31c73, 0x43531c73},
      {0x43960000, 0x407aaaab, 0x4443d555, 0x43c3d555}, {0x43b38000, 0x409f6c16, 0x447918e2, 0x43f918e2},
      {0x43b40000, 0x40a00000, 0x447a0000, 0x43fa0000}, {0x43c80000, 0x40a00000, 0x447a0000, 0x43fa0000},
      {0xc3480000, 0x40071c73, 0x43d31c73, 0x43531c73}, {0xc4340000, 0x40a00000, 0x447a0000, 0x43fa0000}};
  for (const auto &row : rows) {
    const auto a = ca::equipped_draw_argument(row.projection);
    check(a == row.argument, "draw argument");
    const auto s = ca::shell_fields(true, a);
    check(s.pointer == ca::SHELL_BLOCK && s.count == 2 && s.block && s.block->byte3 == 49 &&
              s.block->byte11 == 55 && s.block->float4 == row.f4 && s.block->float12 == row.f12,
          "shell block");
  }
  check(ca::INVENTORY_DRAW_ARGUMENT == 0x3fc00000, "inventory caller 1.5");
  ca::ShellState state = ca::shell_fields(true, 0x40a00000);
  ca::reset_shell(state);
  check(state.pointer == ca::SHELL_RESET_POINTER && state.count == 0, "0x179978 reset");
  check(ca::shell_pass_gate(0x20, 2) && !ca::shell_pass_gate(0x20, 0) && !ca::shell_pass_gate(0, 2) &&
            !ca::shell_pass_gate(0x20, -1),
        "flag 0x20 and positive count");
}

ac::ClipPtr clip(std::int32_t length = 30) {
  auto c = std::make_shared<ac::Clip>();
  c->length = length;
  for (auto &e : c->events)
    e.seconds = 0x447a0000;
  return c;
}
void plant(ac::Track &t) {
  t.time = 0x55555555;
  t.weight = 0x66666666;
}

void poses() {
  // CCA:331-341 gate, format and race literal.
  check(!ca::creation_restart(0x00, 0, false, 0), "bit 0x80 required");
  check(!ca::creation_restart(0x80, 2, false, 0), "count below 2 required");
  check(ca::creation_restart(0x80, 1, false, 0)->format_address == ca::CREATION_FORMAT, "standing format");
  check(ca::creation_restart(0x88, 0, false, 0)->format_address == ca::CREATION_FORMAT_MARKED, "marked format");
  check(!ca::creation_restart(0x80, 0, false, 0)->race_literal, "descriptor name kept");
  for (int race = -1; race <= 11; ++race)
    check(ca::creation_restart(0x80, 0, true, std::int8_t(race))->race_literal ==
              ca::CREATION_RACE_LITERALS[std::size_t(race + 1)],
          "race literal");
  check(ca::CREATION_RACE_LITERALS[0] == ca::CREATION_RACE_LITERALS[1], "bytes -1 and 0 share an address");
  check(!ca::creation_restart(0x80, 0, true, 12) && !ca::creation_restart(0x80, 0, true, -2),
        "only slots 0..12 are copied");
  check(ca::creation_clip_name("race", "%s_stand.anm") == "race_stand.anm", "leading %s");
  refuses([] { ca::creation_clip_name("r", "x%s"); }, "format must lead with %s");

  // CCA:342-347 install: sentinels cleared; the blend is forced to 0 on an
  // empty state and kept at count 1. Blend words are synthetic here; the
  // original word at 0x225584 is caller input from the executable.
  const Raw blend = 0x3e800000; // synthetic 0.25
  ac::AnimState empty(8);
  plant(empty.tracks[0]);
  auto *t = ca::install_creation_clip(empty, clip(), blend);
  check(t && t->time == ZERO && t->weight == ZERO && t->rate == ONE && t->flags == 3 && empty.count == 1,
        "creation install on empty state");
  ac::AnimState one(8);
  one.count = 1;
  one.tracks[0].clip = clip();
  plant(one.tracks[1]);
  t = ca::install_creation_clip(one, clip(), blend);
  namespace rta = champions::simulation::rta;
  check(t == &one.tracks[1] && t->time == ZERO && t->weight == ZERO &&
            t->rate == rta::div(ONE, rta::mul(blend, ac::SIXTY)) && t->rate != ONE && one.count == 2,
        "creation install keeps the blend at count 1");

  // CCA:375-386 inventory stand.
  check(ca::inventory_lump_address(0) == 0x538488 && ca::inventory_lump_address(11) == 0x538528, "lump table");
  for (int c = 0; c < 9; ++c) {
    const bool own = c == 1 || c == 2 || c == 3 || c == 4 || c == 5 || c == 7;
    check(own ? ca::inventory_idle_address(c) != 0x538598 : ca::inventory_idle_address(c) == 0x538598,
          "inventory idle class");
  }
  ac::AnimState inv(8);
  plant(inv.tracks[0]);
  auto idle = clip();
  t = ca::inventory_select_idle(inv, idle, blend);
  check(t && t->time == ZERO && t->weight == ZERO && t->rate == ONE && t->flags == 0x43, "inventory install");
  inv.tracks[0].time = 0x11111111;
  check(!ca::inventory_select_idle(inv, idle, blend) && inv.tracks[0].time == 0x11111111, "same clip keeps its frame");
  ac::AnimState fresh(8);
  plant(fresh.tracks[0]);
  ca::inventory_select_idle(fresh, clip(), blend);
  ac::AnimSystem system;
  ca::inventory_draw_step(system, fresh);
  check(fresh.tracks[0].time == 0x3f7fffff && fresh.tracks[0].weight == ONE, "first inventory frame");

  // CCA:389-401 player_resetIdleAnim.
  check(ca::idle_cell_offsets(2) == std::array<std::uint32_t, 4>{{0xd0 + 0x44, 0xd0 + 0x4c, 0xd0 + 0x54, 0xd0 + 0x5c}},
        "row offsets");
  for (int c = 0; c < 9; ++c) {
    const bool own = c == 1 || c == 2 || c == 3 || c == 4 || c == 7;
    check(own == (ca::display_idle_address(c) != ca::DISPLAY_IDLE_DEFAULT), "display idle class");
  }
  ca::IdleClips clips{{clip(), clip(), clip(), clip()}, clip()};
  for (bool display : {false, true}) {
    ac::AnimState s(8);
    s.tracks[0].time = 0x22222222;
    s.tracks[0].rate = 0x33333333;
    check(ca::reset_idle_anim(s, true, ONE, display, clips), "reset applies");
    check(s.count == 4 && s.tracks[0].clip == (display ? clips.display : clips.table[0]), "cell 0 clip");
    for (std::size_t i = 1; i < 4; ++i)
      check(s.tracks[i].clip == clips.table[i] && s.tracks[i].weight == ZERO && s.tracks[i].life == ZERO,
            "upper cells");
    check(s.tracks[0].weight == ONE && s.tracks[0].life == ONE && s.tracks[0].flags == 0x1205, "cell 0");
    check(s.tracks[0].time == 0x22222222 && s.tracks[0].rate == 0x33333333, "time and rate not written");
  }
  ac::AnimState kept(8);
  kept.tracks[0].clip = idle;
  check(!ca::reset_idle_anim(kept, false, ONE, false, clips) && kept.tracks[0].clip == idle, "missing player");
  check(!ca::reset_idle_anim(kept, true, ZERO, false, clips) && kept.tracks[0].clip == idle, "no hit points");
  refuses([&] { ca::reset_idle_anim(kept, true, ONE, true, {clips.table, nullptr}); }, "display clip required");
}
} // namespace

int main() {
  formats();
  requests_and_handles();
  gates();
  masks();
  attachments();
  equipment();
  shell();
  poses();
  std::cout << checks << " char appearance checks passed\n";
  return 0;
}
