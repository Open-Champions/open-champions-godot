// Authored AMX fixtures only. No shipped mission script or object payload.
namespace mission2_host {
void test_complete_command_boundary() {
  auto image = build_calls({
      {"start", {{"setMissionAvailable", {2, 1}}, {"addQuest", {2000, 2100}},
                 {"addDeferedCall", {2200}}, {"cutScene", {2300}},
                 {"grantMission", {2}}, {"newWorld", {2400}}}},
      {"later", {{"grantMission", {2}}}}},
      {{2000, "authored-title"}, {2100, "authored-body"}, {2200, "later"},
       {2300, "authored-cut"}, {2400, "authored-next"}});
  sc::Host host;
  plant(host, image);
  host.require_complete_commands();
  host.start_single_player();
  check(host.run_start() == sc::Stop::Refused, "incomplete story stops at an ignored command");
  check(host.campaign().mission_available(2) && host.campaign().record(0).quest_count == 1,
        "documented mission and journal writes preceding the boundary run");
  check(!host.campaign().mission_complete(2) && host.variables().memory.cstr(0x41b6d0).empty(),
        "a refused prerequisite cannot grant the mission or request its exit");
  for (int tick = 0; tick < 4; ++tick) host.on_ready_tick();
  check(!host.campaign().mission_complete(2) && host.counts().executed[at("grantMission")] == 0,
        "a refusal also stops deferred grants on later ready ticks");
  check(host.run("later") == sc::Stop::Refused, "direct re-entry cannot bypass a refused prerequisite");
  check(host.last_refusal() == "refused cutScene" && host.counts().refusal_stops == 1 &&
            host.counts().faults == 0, "the missing command has a persistent counted diagnostic");

  sc::Host legacy;
  plant(legacy, image);
  legacy.start_single_player();
  legacy.run_start();
  check(!legacy.requires_complete_commands() && legacy.campaign().mission_complete(2),
        "the existing opt-in measurement policy is preserved for other levels");
}

void test_null_world_boundary() {
  // Plant a readable address-zero string: interpreting it as the null call's
  // destination would silently choose a fixture word as a level name.
  auto image = build_calls({{"exit", {{"newWorld", {0}}, {"grantMission", {2}}}}},
                           {{0, "unproved-destination"}});
  sc::Host host;
  plant(host, image);
  host.start_single_player();
  host.require_complete_commands();
  const auto objects = gate_objects();
  host.adopt_objects(objects.data(), objects.size());
  check(host.run_object_entry(0, "exit") == sc::Stop::Refused, "null newWorld refuses before string decoding");
  check(host.variables().memory.cstr(0x41b6d0).empty() && !host.campaign().mission_complete(2),
        "null exit neither requests a destination nor grants a mission");
  check(host.last_refusal().find("null destination is Unknown") != std::string::npos,
        "null exit reports the missing evidence");
  check(host.variables().memory.word(SELECTED_WORD) == 0 &&
            host.variables().memory.word(0x4d8ae4) == 0, "refused exit restores object and player context");

  sc::Host named;
  auto explicit_image = build_calls({{"exit", {{"newWorld", {2000}}}}}, {{2000, "authored-next"}});
  plant(named, explicit_image);
  named.start_single_player();
  named.require_complete_commands();
  named.adopt_objects(objects.data(), objects.size());
  check(named.run_object_entry(0, "exit") == sc::Stop::Complete &&
            named.variables().memory.cstr(0x41b6d0) == "authored-next",
        "an explicit script destination still runs the documented binder");
}

void test_second_story_menu() {
  namespace q = champions::simulation::rta::quests;
  auto image = build_calls({{"complete-first", {{"grantMission", {1}}}},
                            {"offer-second", {{"setMissionAvailable", {2, 1}}}}}, {});
  sc::Host host;
  plant(host, image);
  host.start_single_player();
  std::vector<q::MissionRow> rows(13);
  rows[0].story = "authored-first";
  rows[1].story = "authored-second";
  host.campaign().set_rows(rows);
  host.run("complete-first");
  check(!host.campaign().mission_available(2), "mission 1 completion alone never unlocks mission 2");
  host.run("offer-second");
  check(host.campaign().mission_available(2), "the separate hub action can offer mission 2");
  host.campaign().open_menu(0);
  for (int tick = 0; tick < 17; ++tick) host.tick_mission_menu(0);
  check(host.campaign().menu().row == 1 && host.campaign().menu().state == 2,
        "the menu selects the highest available mission, row 1, after its fade-in");
  host.tick_mission_menu(q::BTN_CROSS);
  host.tick_mission_menu(0);
  check(host.campaign().menu().state == 6, "confirmation waits for the real fade service");
  for (int tick = 0; tick < 31; ++tick) {
    q::MenuInputs inputs;
    inputs.fade = int(host.callees().step_fade(4));
    check(!host.tick_mission_menu(0, inputs), "menu cannot launch before fade reaches 128");
  }
  check(host.variables().memory.cstr(0x41b6d0).empty(), "no early pending-world write");
  q::MenuInputs inputs;
  inputs.fade = int(host.callees().step_fade(4));
  check(!host.tick_mission_menu(0, inputs), "fade completion enters state 7 without launching");
  check(!host.tick_mission_menu(0, inputs), "state 7 only advances to state 8");
  check(host.tick_mission_menu(0, inputs) && host.level().pending.name == "authored-second" &&
            host.variables().memory.cstr(0x41b6d0) == "authored-second" &&
            host.variables().memory.read(0x41b720) == 255,
        "state 8 writes the second row's supplied story pointer and orientation");
  const auto saved = host.save_campaign();
  sc::Host replacement;
  replacement.load_campaign(saved);
  check(replacement.campaign().mission_available(2) && replacement.campaign().mission_complete(1) &&
            replacement.campaign().rows()[1].story == "authored-second",
        "campaign replacement preserves separate completion and availability state");
}
void test_act_entry() {
  namespace q = champions::simulation::rta::quests;
  q::ActTables tables;
  tables.names = {{"authored-home", "authored-same"}, {"authored-next"}};
  tables.flow = {{{"authored-home", 0}, {"authored-same", 0}}, {{"authored-next", 0}}};
  auto image = build_calls({{"start", {{"addQuest", {2000, 2100}}, {"setv", {2200, 7}},
                                       {"grantMission", {1}}, {"setMissionAvailable", {2, 1}}}}},
                           {{2000, "authored-title"}, {2100, "authored-body"}, {2200, "authored-progress"}});
  sc::Host source;
  plant(source, image);
  source.start_single_player();
  source.run_start();
  source.campaign().set_act(0);
  auto record = source.campaign().record(0);
  record.level = 3; record.exp = 123; record.gold = 37;
  source.campaign().set_record(0, record);
  source.load_campaign(source.campaign().save());
  const auto before = source.save_campaign();
  source.variables().memory.w32(si::bag_count_at(0, 0), 1);
  bool refused = false;
  try { sc::require_campaign_act_transfer(source, tables, "authored-next", false); }
  catch (const std::runtime_error &) { refused = true; }
  check(refused && source.save_campaign() == before, "nonempty live inventory refuses without changing campaign state");
  source.variables().memory.w32(si::bag_count_at(0, 0), 0);
  refused = false;
  try { sc::require_campaign_act_transfer(source, tables, "authored-next", true); }
  catch (const std::runtime_error &) { refused = true; }
  check(refused, "unknown imported inventory refuses even with zero modeled bag counts");
  sc::Host same;
  same.load_campaign(before);
  const auto same_entry = sc::prepare_campaign_entry(same, &source, tables, "authored-same", true, false);
  check(!same_entry.reset && same.campaign().record(0).quest_count == 1 &&
            same.variables().quest_get("authored-progress") == 7,
        "same-act entry preserves journal and quest variable values");
  sc::Host next;
  next.load_campaign(before);
  const auto changed = sc::prepare_campaign_entry(next, &source, tables, "authored-next", true, false);
  check(changed.changed && changed.reset && changed.act == 1, "cross-act entry uses the native act-change contract");
  check(next.campaign().record(0).quest_count == 0 && next.variables().quest_get("authored-progress") == 0,
        "act change clears both journal and binder quest variable value");
  check(next.campaign().mission_available(2) && next.campaign().mission_complete(1), "act reset preserves mission availability and completion");
  const auto &after = next.campaign().record(0);
  check(after.level == 3 && after.exp == 123 && after.gold == 37,
        "act reset preserves player level, experience and gold");
  source.campaign().set_act(-1);
  refused = false;
  try { sc::require_campaign_act_transfer(source, tables, "authored-next", false); }
  catch (const std::runtime_error &) { refused = true; }
  check(refused, "unbound source act refuses instead of guessing an act");
}

std::vector<std::uint8_t> inventory_fixture() {
  namespace q = champions::simulation::rta::quests;
  q::Campaign campaign;
  campaign.set_num_players(1);
  campaign.set_act(0);
  std::vector<q::MissionRow> rows(q::MISSIONS);
  rows[0].story = "blood1"; rows[1].story = "innovat1";
  campaign.set_rows(std::move(rows));
  campaign.mission_set_available(1, 1);
  campaign.mission_set_complete(1, 1);
  campaign.mission_set_available(2, 1);
  auto record = campaign.record(0);
  record.level = 1;
  record.bags[0].push_back({"authored-item", std::uint64_t(1) << q::QUEST_ITEM_BIT});
  campaign.set_record(0, std::move(record));
  campaign.open_menu(0);
  return campaign.save();
}

void run() {
  test_complete_command_boundary();
  test_null_world_boundary();
  test_second_story_menu();
  test_act_entry();
}
} // namespace mission2_host
