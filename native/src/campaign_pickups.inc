// Members of Fight::Impl. RTA loot-drop 77c2f70, with runtime-only private
// resource/descriptor lookup. Kept separate from the combat/animation adapter.
  std::shared_ptr<const items::Tables> item_tables;
  struct LoadedResource { resources::Handle handle; std::vector<std::uint8_t> bytes; };
  std::map<Address, LoadedResource> item_resources;
  std::map<std::pair<std::string, std::string>, Address> item_resource_cache;
  Address resource_next = 0xc0000000; // host virtual address domain, not game data
  std::vector<Address> drop_handles;
  Address last_drop = 0;
  Address prompt_object = 0;
  bool pending_use = false;
  struct ChestState {
    std::shared_ptr<builtins::Obj> owner;
    std::vector<std::string> props;
    creature_anim::StateId animation = 0;
    creature_anim::ClipId clip = 0;
  };
  std::map<Address, ChestState> chests;

  std::string runtime_string(Address at) const {
    std::string text;
    for (unsigned k = 0; k < 256; ++k) {
      auto c = std::uint8_t(world->read(at + k, 1));
      if (!c) return text;
      text += char(c);
    }
    throw C::Unsupported("runtime resource string exceeds 255 bytes");
  }
  void image_string(Address at) {
    const auto text = exe->string(at);
    std::vector<std::uint8_t> bytes(text.begin(), text.end());
    bytes.push_back(0);
    world->load_runtime_bytes(at, bytes);
  }
  Raw item_resource(const std::string &lump, const std::string &entry) {
    if (!host || lump.empty() || entry.empty()) return 0;
    auto key = std::make_pair(lump, entry);
    auto old = item_resource_cache.find(key);
    if (old != item_resource_cache.end()) {
      if (host->resources().bytes(item_resources.at(old->second).handle)) return old->second;
      throw C::Unsupported("item resource generation changed during play");
    }
    auto handle = host->resources().find(lump, entry);
    if (!handle) return 0;
    const auto bytes = host->resources().bytes(*handle);
    if (!bytes) return 0;
    if (bytes->size > 16 * 1024 * 1024 || resource_next > 0xd0000000 - bytes->size)
      throw C::Unsupported("item resource host address budget exceeded");
    const auto token = resource_next;
    resource_next += Address((bytes->size + 31) & ~std::size_t(15));
    LoadedResource resource{*handle, {bytes->data, bytes->data + bytes->size}};
    world->load_runtime_bytes(token, resource.bytes);
    item_resources.emplace(token, std::move(resource));
    item_resource_cache[key] = token;
    return token;
  }
  void prepare_items() {
    if (item_tables) return;
    const auto reader = [this](Address a, std::size_t n) {
      std::vector<std::uint8_t> bytes;
      bytes.reserve(n);
      for (std::size_t k = 0; k < n; ++k) bytes.push_back(exe->byte(a + Address(k)));
      world->load_runtime_bytes(a, bytes);
      return bytes;
    };
    item_tables = script_items::image_tables(reader);
    host->items().bind_tables(item_tables);
    for (auto a : {0x520030u, 0x5326b0u, 0x549528u, 0x537db8u, 0x537dc8u,
                   0x537e08u, 0x537e10u, 0x538028u, 0x538038u, 0x538048u}) image_string(a);
    // Passive table and skill-list terminators are read by weight and equip.
    reader(0x4ef538, 7 * 0xa88);
    auto &service = host->items();
    service.delegates["pickup_notify"] = [this](const std::vector<std::uint64_t> &) {
      // No level in the traced disc defines this optional hook. Record the
      // sound/HUD boundary; it does not replace inventory insertion.
      refuse("pickup material sound and optional notification presentation are not bound");
      return std::uint64_t(0);
    };
  }
  std::array<Address, 3> item_names(const items::Item &item, Address lump) {
    const auto index = items::resource_index(item);
    if (index < 0) throw C::Unsupported("negative item resource index");
    const Address row = 0x3e9ce0 + 16 * Address(index);
    const auto model = exe->word(row), texture = exe->word(row + 4);
    image_string(lump); image_string(model); image_string(texture);
    world->write(row, 4, model); world->write(row + 4, 4, texture);
    // Resolve before publishing the constructor, rather than give an absent
    // service a fabricated resource pointer or model bounds.
    if (!item_resource(exe->string(lump), exe->string(model)) ||
        !item_resource(exe->string(lump), exe->string(texture)))
      throw C::Unsupported("item model/texture is absent from the loaded disc resources");
    return {lump, model, texture};
  }
  Address make_drop(const std::string &kind, items::Item &item, items::Rng &rng,
                    Vec3 position, Address lump) {
    const auto names = item_names(item, lump);
    world->write(C::SHARED_SEED, 4, rng.seed);
    const auto object = world->spawn_item(position, names[0], names[1], names[2], item.pack());
    rng.seed = read(C::SHARED_SEED);
    if (object) ++rng.draws; // exactly GameObject_ctor's one shared draw
    GroundDrop drop;
    drop.kind = kind; drop.category = item.definition->category; drop.count = item.count;
    drop.spawned = object != 0; drop.lost = object == 0; drop.address = object; drop.position = position;
    ground_drops.push_back(drop);
    drop_handles.push_back(object ? world->retain(object) : 0);
    last_drop = object;
    return object;
  }
  bool item_object(Address object) const {
    const auto table = read(object + 0x48);
    return table == 0x511c38 || (table == 0x511ba8 && read(object + 0x238, 1));
  }
  C::InteractionPrompt pickup_prompt(Address object, Raw slot) {
    ++counts.prompts;
    prompt_object = object;
    if (item_object(object)) return {1, read(object + 0x148) != 0};
    return chest_prompt(object, slot);
  }
  void pickup_use(Address object, Raw slot) {
    if (!item_object(object)) { chest_use(object, slot); return; }
    prepare_items();
    std::array<std::uint8_t, 0x48> record{};
    for (unsigned k = 0; k < record.size(); ++k) record[k] = std::uint8_t(read(object + 0x148 + k, 1));
    const auto recipients = pickups::take_item(host->items(), host->variables().memory, *item_tables, record, slot);
    if (recipients.empty()) return;
    ++counts.pickups; counts.pickup_recipients += recipients.size();
    for (auto &drop : ground_drops) if (drop.address == object) drop.picked_up = true;
    world->destroy(object);
  }
  std::optional<std::string> chest_property(const ChestState &chest, Address key, unsigned nth = 0) {
    auto props = std::make_shared<std::vector<std::string>>(chest.props);
    return objects::props_find(props, exe->string(key), nth);
  }
  Raw chest_draw(Address a) {
    const auto seed = Raw(std::uint64_t(read(a + 0x44)) * 0x19660d + 0x3c6ef35f);
    world->write(a + 0x44, 4, seed);
    return seed >> 16;
  }
  void build_chest(Address a, const level_flow::Record &record, unsigned variant, SharedRandom &seed) {
    if (chests.count(a) || authority->find(a))
      throw C::Unsupported("Chest address already has a live owner or state");
    if (variant > 4) throw C::Unsupported("Chest variant outside 0..4");
    const auto globals = host->level_load_globals();
    if (!globals) throw C::Unsupported("Chest level inputs unavailable");
    const auto descriptor = exe->word(0x501698 + 4 * variant);
    const auto lump = exe->string(exe->word(descriptor));
    std::array<Raw, 3> resources{};
    for (unsigned k = 0; k < 3; ++k)
      resources[k] = item_resource(lump, exe->string(exe->word(descriptor + 8 + 8 * k)));
    if (!resources[0] || !resources[1] || !resources[2])
      throw C::Unsupported("Chest descriptor model/texture/opening clip unavailable");
    const auto &clip_bytes = item_resources.at(resources[2]).bytes;
    ChestState chest;
    chest.props = record.props;
    chest.animation = anim->add_state(1);
    chest.clip = anim->add_clip(clip_bytes);
    // Preflight the model before taking the base constructor's shared draw.
    const auto &model_bytes = item_resources.at(resources[0]).bytes;
    const auto model = decodeRtaModel({model_bytes.data(), model_bytes.size()});
    std::vector<std::uint8_t> table;
    for (unsigned k = 0; k < 0x80; ++k) table.push_back(exe->byte(0x5118d8 + k));
    world->load_runtime_bytes(0x5118d8, table);
    auto &memory = host->variables().memory;
    memory.write(a, std::vector<std::uint8_t>(0x88, 0));
    chest.owner = host->variables().new_object({}, 0, record.position, record.angle, "5Chest",
      std::make_shared<std::vector<std::string>>(record.props), a, 0x88);
    memory.word(a + 0x44, seed.draw());
    memory.word(a + 0x24, descriptor);
    memory.word(a + 0x48, 0x5118d8);
    memory.word(a + 0x1c, 0x41b00000); memory.word(a + 0x20, 0x41c80000);
    memory.word(a + 0xc, 0x100018); memory.byte(a + 0x59, std::uint8_t(variant));
    memory.byte(a + 0x61, 1);
    bool trap = chest_draw(a) < 2000;
    const auto &level = host->loaded_level_name();
    if (level == exe->string(0x549110) || level == exe->string(0x549518)) trap = false;
    Raw worth = 0x3fc00000;
    if (const auto p = chest_property(chest, 0x5497c0)) worth = float_bits(std::strtof(p->c_str(), nullptr));
    if (const auto p = chest_property(chest, 0x5497c8)) if (objects::atoi(*p)) trap = true;
    const auto r1 = chest_draw(a), r2 = chest_draw(a);
    const auto factor = mul(globals->monster_level, 0x3ecccccc);
    const auto value = mul(mul(from_int(r2 % 24 + 12), mul(worth, from_int(host->campaign().act() + 1))), factor);
    auto gold = std::int32_t(r1 % 100) + s32(to_int(value)) + 50;
    if (trap || level == exe->string(0x549518)) gold = 0;
    const auto stored = std::int16_t(gold);
    const auto bits = trap ? 8u : stored == 0 ? 0u : stored < 40 ? 1u : stored < 80 ? 2u : 4u;
    memory.w16(a + 0x4c, std::uint16_t(stored)); memory.byte(a + 0x58, trap ? 1 : 0);
    const int selection = bits == 8 ? 3 : bits == 4 ? 2 : bits == 2 ? 1 : bits == 1 ? 0 : -1;
    memory.word(a + 0x54, Raw(modelVisibilityMask(model, selection)));
    for (unsigned k = 0; k < 3; ++k) memory.word(descriptor + 12 + 8 * k, resources[k]);
    if (!chests.emplace(a, std::move(chest)).second)
      throw C::Unsupported("Chest state address insertion failed");
    ++counts.chests_built;
  }
  static Raw float_bits(float f) { Raw r; std::memcpy(&r, &f, 4); return r; }
  bool live_chest(Address a) const {
    const auto it = chests.find(a);
    return it != chests.end() && it->second.owner && it->second.owner->alive &&
           it->second.owner->type_name == "5Chest" && authority->is_factory(a) &&
           authority->find(a) == it->second.owner.get() && read(a + 0x48) == 0x5118d8;
  }
  C::InteractionPrompt chest_prompt(Address a, Raw) {
    if (!live_chest(a)) throw C::Unsupported("interactable class has no live Chest prompt slot 13");
    // The zero-gold opened branch returns status 1 without writing text.
    return {1, read(a + 0x84) == 0 || std::int16_t(read(a + 0x4c, 2)) > 0};
  }
  void chest_use(Address a, Raw slot) {
    auto it = chests.find(a);
    if (!live_chest(a)) throw C::Unsupported("interactable class has no live Chest slot 14");
    if (read(a + 0x84)) {
      world->write(a + 0x54, 4, 1);
      world->add_gold_floater(slot, {read(a + 0x10), read(a + 0x14), read(a + 0x18)},
                              Raw(std::int16_t(read(a + 0x4c, 2))));
      world->write(a + 0x4c, 2, 0);
      return;
    }
    prepare_items();
    auto &chest = it->second;
    anim->anim_push(chest.animation, chest.clip, 0x204);
    world->write(a + 0x84, 4, 1); world->write(a + 0x50, 4, read(C::FRAME));
    if (std::int16_t(read(a + 0x4c, 2)) <= 0) world->write(a + 0xc, 4, read(a + 0xc) & ~0x100000u);
    // Use the host's already linked object and its run head.
    host->variables().world->run_every_frame(*host->variables().obj_at(a));
    ++counts.chests_opened;
    chest_loot(a, chest);
  }
  bool chest_run(Address a) {
    auto it = chests.find(a);
    if (!live_chest(a)) return false;
    auto &chest = it->second;
    if (read(a + 0x58, 1)) {
      refuse("trapped Chest run: explosion/action path not executed by loot-drop probes");
      return true;
    }
    anim->advance(ONE, chest.animation, false);
    world->write(a + 0x62, 2, anim->state(chest.animation).flags);
    if (anim->state(chest.animation).flags & 0x80) {
      world->write(a + 0x84, 4, 2);
      host->variables().world->stop_running(*host->variables().obj_at(a));
      // The interpreter recorded the action service, not arbitrary action
      // scripts. Keep that boundary explicit rather than silently skipping it.
      refuse("Chest completion action scripts and rendered lid remain unverified");
    }
    return true;
  }
  items::Game item_game() const {
    const auto globals = host->level_load_globals();
    if (!globals) throw C::Unsupported("item generation level inputs unavailable");
    items::Game game;
    game.level = globals->monster_level; game.difficulty = globals->difficulty;
    game.num_players = std::int32_t(read(C::NUM_PLAYERS));
    game.classes.clear();
    for (unsigned slot = 0; slot < 4; ++slot)
      game.classes.push_back(std::int32_t(read(PLAYERS + slot * RECORD + 0x50)));
    return game;
  }
  void chest_loot(Address a, const ChestState &chest) {
    const auto game = item_game();
    Vec3 pos{read(a + 0x10), read(a + 0x14), add(read(a + 0x18), 0x41400000)};
    items::Rng rng(read(C::SHARED_SEED));
    const auto launch = [&](items::Item &item) {
      const auto object = make_drop("chest", item, rng, pos, 0x549528);
      ++counts.loot_items;
      if (object) {
        const auto angle = read(a + 0x38, 2) + ((chest_draw(a) & 0x3fff) - 0x2000);
        world->throw_item(object, angle, ONE);
      }
    };
    for (unsigned n = 0;; ++n) {
      const auto descriptor = chest_property(chest, 0x549828, n);
      if (!descriptor) break;
      auto item = items::parse_descriptor(*item_tables, *descriptor, -1, false, &rng);
      if (item.definition) launch(item);
    }
    if (const auto p = chest_property(chest, 0x549830))
      for (int n = 0, count = objects::atoi(*p); n < count; ++n) {
        auto item = items::generate_consumable(*item_tables, game, rng); launch(item);
      }
    for (unsigned n = 0;; ++n) {
      const auto worth = chest_property(chest, 0x549348, n);
      if (!worth) break;
      auto budget = mul(mul(TWO, float_bits(std::strtof(worth->c_str(), nullptr))), game.level_times_difficulty());
      const auto spread = add(0x3f733333, mul(mul(from_int(chest_draw(a)), TWO_M16), 0x3dcccccc));
      budget = mul(mul(budget, spread), 0x44160000); // 600
      const auto category = std::int32_t(exe->word(0x5016b8 + 4 * (chest_draw(a) % 3)));
      auto item = items::generate_with_affixes(*item_tables, game, rng, s32(to_int(budget)), category, -1, 1);
      if (item) launch(*item);
    }
    world->write(C::SHARED_SEED, 4, rng.seed);
    C::LootDrop drop;
    drop.position = {read(a + 0x10), read(a + 0x14), read(a + 0x18)};
    drop.angle = read(a + 0x38, 2); drop.z = 0x42100000; drop.throw_speed = TWO;
    drop.quality_row = 0x40a00000; drop.item_chance = 0x3f733333;
    roll_loot(drop);
  }
  void refresh_drops() {
    for (std::size_t k = 0; k < ground_drops.size(); ++k) {
      auto &drop = ground_drops[k];
      const auto object = drop_handles[k] ? world->target(drop_handles[k]) : 0;
      if (object) drop.position = {read(object + 0x10), read(object + 0x14), read(object + 0x18)};
      else if (drop.spawned && !drop.picked_up) drop.lost = true;
    }
  }
  void item_settled(Address from, Address replacement) {
    // Publish identity replacement before any later actor's interaction in
    // this same tick, including pickup of the newly settled object.
    for (std::size_t k=0;k<ground_drops.size();++k) if (ground_drops[k].address==from) {
      ground_drops[k].address=replacement;
      if (drop_handles[k]) world->clear_handle(drop_handles[k]);
      drop_handles[k]=replacement ? world->retain(replacement) : 0;
      if (!replacement) ground_drops[k].lost=true;
    }
  }
