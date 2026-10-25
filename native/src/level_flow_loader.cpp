#include "champions/level_flow_loader.hpp"
#include <algorithm>
namespace champions::simulation::rta::level_flow {
namespace {
Bytes view(const Blob &b) { return {b.data(), b.size()}; }
bool suffix(const std::string &n, const std::string &s) {
  return n.size() >= s.size() &&
         n.compare(n.size() - s.size(), s.size(), s) == 0;
}
} // namespace
Loader::Loader(LoadServices s) : services_(std::move(s)) {
  if (!services_.disc || !services_.range || !services_.event ||
      !services_.model_lumps || !services_.player_lumps)
    throw Error("all load services must be supplied explicitly");
}
Lump Loader::parse_lump(Blob b, int keep) {
  Lump l;
  l.bytes = std::move(b);
  l.keep = keep;
  for (auto e : readLmp(view(l.bytes))) {
    if (e.name.compare(0, 4, "pal_") == 0)
      continue;
    if (e.name == "ntsc_dialog.bin")
      e.name = "dialog.bin";
    l.entries.push_back(std::move(e));
  }
  return l;
}
void Loader::resident(const std::string &name, Blob b, int keep) {
  lumps_[name] = parse_lump(std::move(b), keep);
}
const Lump &Loader::need_lump(const std::string &name, int keep,
                              const std::string &step) {
  opened_.push_back({step, name, "lump", true});
  auto it = lumps_.find(name);
  if (it != lumps_.end())
    return it->second;
  auto b = services_.disc(cd_file_name(name));
  if (!b)
    throw MissingResource(name + " at step " + step);
  resident(name, std::move(*b), keep);
  return lumps_.at(name);
}
std::optional<Blob> Loader::entry(const std::string &lump,
                                  const std::string &name) const {
  auto i = lumps_.find(lump);
  if (i == lumps_.end())
    return {};
  for (auto &e : i->second.entries)
    if (e.name == name) {
      auto b = payload(view(i->second.bytes), e);
      return Blob(b.data, b.data + b.size);
    }
  return {};
}
LoadedLevel Loader::load(const std::string &base, const std::string &subname,
                         const std::vector<LevelRow> &rows, int difficulty,
                         bool recall, bool pvp) {
  Pending checked;
  checked.request(qualified_name(base, subname));
  if (base.empty())
    throw Error("empty level base");
  opened_.clear();
  auto emit = [&](const std::string &e) { services_.event(e); };
  emit("loading_begin");
  emit("clear_level");
  for (auto i = lumps_.begin(); i != lumps_.end();)
    if (i->second.keep == 0)
      i = lumps_.erase(i);
    else
      ++i;
  if (base != "intro") {
    if (!lumps_.count("hud.lmp")) {
      emit("unload_to_keep_9");
      for (auto i = lumps_.begin(); i != lumps_.end();)
        if (i->second.keep < 9)
          i = lumps_.erase(i);
        else
          ++i;
      emit("item_resources");
      for (auto n : {"fx.lmp", "hud.lmp", "mapicons.lmp", "icons.lmp"})
        need_lump(n, 10, "4a");
    }
    if (services_.player_lumps()) {
      emit("clear_level_after_player_reload");
      for (auto i = lumps_.begin(); i != lumps_.end();)
        if (i->second.keep == 0)
          i = lumps_.erase(i);
        else
          ++i;
    }
    emit("real_level_loaded");
  } else {
    emit("intro_unload_to_keep_10");
    for (auto i = lumps_.begin(); i != lumps_.end();)
      if (i->second.keep < 10)
        i = lumps_.erase(i);
      else
        ++i;
    emit("release_equipment_cache");
  }
  auto pics = loading_picture_files(base);
  auto picture = services_.disc(cd_file_name(pics[0]));
  if (!picture)
    picture = services_.disc(cd_file_name(pics[0]));
  auto pn = pics[0];
  if (!picture) {
    pn = pics[1];
    picture = services_.disc(cd_file_name(pn));
  }
  opened_.push_back({"5", pn, "disc", false});
  if (picture)
    resident(pn, std::move(*picture), 0);
  need_lump("loadmisc.lmp", 0, "5");
  LoadedLevel out;
  out.row = find_level_row(rows, base, subname);
  out.monster_level = level_flow::monster_level(out.row, difficulty, base, pvp);
  out.base_light_level = level_flow::base_light_level(out.row.flags);
  auto gob = services_.disc(cd_file_name(base + ".gob"));
  if (!gob)
    throw MissingResource(base + ".gob");
  opened_.push_back({"7", base + ".gob", "disc", true});
  bool texture_read = false;
  for (auto &e : gob_entries(view(*gob))) {
    auto b = payload(view(*gob), e);
    resident(e.name, Blob(b.data, b.data + b.size), 0);
    for (auto &en : lumps_.at(e.name).entries)
      if (suffix(en.name, ".world")) {
        if (!texture_read) {
          out.texture_header =
              services_.range(cd_file_name(base + ".tex"), 0, 0x5000);
          if (out.texture_header.size() != 0x5000)
            throw MissingResource(base + ".tex header");
          opened_.push_back({"7", base + ".tex", "disc", true});
          texture_read = true;
        }
        emit("world_fixup");
      }
  }
  const auto &level = need_lump(base + ".lmp", 0, "7");
  // g_world: first `.world` in stored directory order (0x171fd8). The original
  // has no null test and faults on a lump without one; this refuses instead.
  auto world = first_entry_with_extension(level.entries, "world");
  if (!world)
    throw MissingResource("*.world in " + base + ".lmp");
  out.world_entry = world->name;
  auto world_bytes = payload(view(level.bytes), *world);
  out.world = Blob(world_bytes.data, world_bytes.data + world_bytes.size);
  opened_.push_back({"8", out.world_entry, base + ".lmp", true});
  emit("copy_cosmetic_seed");
  if (!recall) {
    opened_.push_back({"10", "dialog.bin", base + ".lmp", false});
    emit("entry_music");
  }
  opened_.push_back({"11", "ambient.bin", base + ".lmp", false});
  emit("ambient_parse");
  emit("apply_level_row");
  if (base != "intro")
    emit("allocate_water_pool");
  if (pvp) {
    need_lump("arena.lmp", 0, "14");
    need_lump("pvpfont.lmp", 0, "14");
  }
  if (base != "intro")
    for (auto n : {"gooddog.lmp", "skele_b.lmp"})
      need_lump(n, 0, "15");
  for (auto &n : services_.model_lumps(view(out.world)))
    need_lump(n + ".lmp", 0, "16");
  emit("world_prepare");
  opened_.push_back({"17", "dome.lmp", "gob", false});
  if (!lumps_.count("waterenv.lmp"))
    throw MissingResource("waterenv.lmp must already be resident from GOB");
  const auto &water = need_lump("waterenv.lmp", 0, "18");
  if (std::none_of(water.entries.begin(), water.entries.end(),
                   [](auto &e) { return suffix(e.name, ".tex"); }))
    throw MissingResource("waterenv.lmp first .tex entry");
  emit("water_setup");
  emit("reset_level_counters_and_sound");
  emit("level_bounds");
  if (base == "hub")
    need_lump("msel.lmp", 0, "26");
  emit("objects_level_reset");
  auto script = entry(base + ".lmp", "script.scr");
  if (script)
    out.script = *script;
  opened_.push_back({"28", "script.scr", base + ".lmp", false});
  auto objects = entry(base + ".lmp", "objects.ob");
  if (!objects)
    throw MissingResource("objects.ob");
  out.objects = *objects;
  opened_.push_back({"28", "objects.ob", base + ".lmp", true});
  out.manifest = opened_;
  return out; // script, records, unclaimed pieces and completion are adapter
              // steps
}
} // namespace champions::simulation::rta::level_flow
