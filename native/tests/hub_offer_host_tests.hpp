#pragma once
#include "dialog_host_tests.hpp"
#include <set>

namespace hub_offer_host_tests {
using namespace dialog_host_tests;
namespace q = champions::simulation::rta::quests;
// Independent authored AMX/dbin fixtures. Predicates deliberately return zero:
// regressions must exercise the production live-state binding, not fixture logic.
// Offsets and expected outcomes: research-mission-unlocks@950b2538
// docs/spec/mission-unlocks.md:53-79,88-125. No shipped bytes/names/text.
inline constexpr std::array<int,31> offsets{{
  0x3f0,0x11c,0x11c,0x198,0x498,0x498,0x680,0x734,0x898,
  0x9a0,0x9a0,0xb04,0xc0c,0xd70,0xe78,0xfdc,0x10e4,0x10e4,
  0x1248,0x1350,0x14b4,0x15bc,0x1638,0x16b4,0x16b4,0x1818,
  0x1920,0x1a84,0x1b8c,0x1b8c,0x1cf0}};
inline constexpr std::array<int,12> pool{{
  0x6c8,0x6d8,0x6e8,0x6f8,0x708,0x718,0x728,0x738,0x748,0x758,0x76c,0x780}};
struct Fixture {
  Script script;
  sc::DialogLevel level;
  World world;
  std::vector<std::uint8_t> objects;
  Fixture() {
    std::vector<a::Cell> code(0x4000/4,a::Cell(a::opcode_number("ZERO.pri")));
    a::ImageOptions options;
    options.data.resize(0x2000);
    options.natives={"startDialog","setMissionAvailable","activateStore"};
    auto put=[&](int at,const std::string &name,const std::vector<a::AsmLine> &lines) {
      const auto assembly=a::assemble(lines);
      std::copy(assembly.cells.begin(),assembly.cells.end(),code.begin()+at/4);
      options.publics.emplace_back(name,at);
    };
    std::set<int> unique(offsets.begin(),offsets.end());
    for(int at:unique) put(at,"p"+std::to_string(at),{
      a::AsmInstruction{"PROC",{}, {},{}}, a::AsmInstruction{"CONST.pri",{0},{},{}},
      a::AsmInstruction{"RETN",{}, {},{}}});
    auto action=[&](int at,const std::string &name,int mission,bool store=false){
      std::vector<a::AsmLine> lines{
        a::AsmInstruction{"PROC",{}, {},{}},
        a::AsmInstruction{"PUSH.C",{1},{},{}}, a::AsmInstruction{"PUSH.C",{mission},{},{}},
        a::AsmInstruction{"PUSH.C",{8},{},{}}, a::AsmInstruction{"SYSREQ.C",{1},{},{}},
        a::AsmInstruction{"STACK",{12},{},{}}};
      if(store) {
        lines.push_back(a::AsmInstruction{"PUSH.C",{0},{},{}});
        lines.push_back(a::AsmInstruction{"PUSH.C",{4},{},{}});
        lines.push_back(a::AsmInstruction{"SYSREQ.C",{2},{},{}});
        lines.push_back(a::AsmInstruction{"STACK",{8},{},{}});
      }
      lines.push_back(a::AsmInstruction{"RETN",{}, {},{}});
      put(at,name,lines);
    };
    action(0x7b0,"second-a",2);
    action(0x2064,"second-b",2,true);
    action(0x3000,"first",1);
    action(0x3080,"unproved",3);
    put(0x3100,"talk",{
      a::AsmInstruction{"PROC",{}, {},{}}, a::AsmInstruction{"PUSH.C",{0},{},{}},
      a::AsmInstruction{"PUSH.C",{0},{},{}}, a::AsmInstruction{"PUSH.C",{400},{},{}},
      a::AsmInstruction{"PUSH.C",{12},{},{}}, a::AsmInstruction{"SYSREQ.C",{0},{},{}},
      a::AsmInstruction{"STACK",{16},{},{}}, a::AsmInstruction{"RETN",{}, {},{}}});
    script.image=a::build_image(code,options);
    script.strings={{400,"npc"}};
    for(std::size_t i=0;i<pool.size();++i)
      script.strings.emplace_back(pool[i],"v"+std::to_string(i+1));
    std::vector<dl::Node> nodes(31);
    for(std::size_t i=0;i<nodes.size();++i) {
      nodes[i].condition="p"+std::to_string(offsets[i]);
      nodes[i].text="body";
      nodes[i].choices={{"a","","unproved",-1}};
    }
    nodes[1].choices={{"a","","",2}};
    nodes[2].choices={{"a","","",3}};
    nodes[3].choices={{"a","","first",-1},{"b","","first",-1}};
    nodes[7].choices={{"a","","second-a",-1},{"b","","second-b",-1}};
    nodes[8].choices={{"a","","unproved",-1},{"b","","",-1}};
    for(auto &n:nodes) n.raw_choices=int(n.choices.size());
    level.level="synthetic";
    level.files["npc.dbin"]=dl::build_dbin(nodes);
    for(const char *key:{"nm","body","a","b","Affirmation","dialogMore"}) world.texts[key]=wide("authored");
    objects=objects_ob({{"NPC",0,{}, {"name=npc","showName=nm","dialogScript=talk","dialogOffset=0,0,0,0"}}});
  }
  void load_into(sc::Host &host,Check check) const {
    check(load(host,script,objects),"hub: authored host loads");
    host.attach_dialog(level,world.services());
    std::vector<q::MissionRow> rows(13);
    rows[0].story="authored-first"; rows[1].story="authored-second";
    host.campaign().set_rows(rows);
    host.campaign().set_num_players(1);
  }
};
inline void ready(sc::Host &host) {
  for(int i=0;i<160;++i) {
    sc::DialogInputs in; in.camera_arrived=1;
    host.dialog_tick(in);
  }
}
inline void confirm(sc::Host &host,int choice=0) {
  ready(host);
  if(choice) {
    sc::DialogInputs move; move.pad=q::BTN_DOWN; move.camera_arrived=1;
    host.dialog_tick(move); ready(host);
  }
  sc::DialogInputs in; in.pad=q::BTN_CROSS; in.camera_arrived=1;
  host.dialog_tick(in);
}
inline void open(sc::Host &host) { host.use(0,{0x41f00000u,0,0}); }
namespace table { inline void run(Check check,std::string &where); }
inline void run(Check check,std::string &where) {
  where="bounded hub offers";
  Fixture fixture;
  // Exact composed opening table :99-105, all supported campaign difficulties.
  constexpr int expected[4][4]={{1,1,1,1},{3,3,3,3},{1,1,1,1},{7,8,7,9}};
  int cases=0;
  for(int d=1;d<=5;++d) for(int first=0;first<4;++first) for(int second=0;second<4;++second)
    for(int choice=0;choice<2;++choice) {
      sc::Host source; fixture.load_into(source,check);
      auto record=source.campaign().record(0);
      record.missions[q::mission_index(d,1)]=std::uint8_t(first);
      record.missions[q::mission_index(d,2)]=std::uint8_t(second);
      source.campaign().set_record(0,record); source.campaign().set_difficulty(d);
      source.load_campaign(source.campaign().save());
      const auto saved=source.save_campaign();
      sc::Host host; fixture.load_into(host,check); host.load_campaign(saved);
      const auto before=host.campaign().record(0).missions;
      open(host);
      check(host.dialog().node==expected[first][second],"hub: saved quest state selects documented opening");
      check(host.campaign().record(0).missions==before,"hub: opening never writes availability");
      const int start=expected[first][second];
      auto after=before;
      if(start==1) {
        confirm(host); check(host.dialog().node==2,"hub: first traversal reaches node 2");
        check(host.campaign().record(0).missions==before,"hub: first confirm has no grant");
        confirm(host); check(host.dialog().node==3,"hub: first traversal reaches node 3");
        check(host.campaign().record(0).missions==before,"hub: second confirm has no grant");
        confirm(host,choice); after[q::mission_index(d,1)]|=1;
      } else if(start==7) {
        confirm(host,choice); after[q::mission_index(d,2)]|=1;
      } else if(start==8 && choice==1) confirm(host,1);
      check(host.campaign().record(0).missions==after,"hub: choice preserves completion and other difficulty rows");
      check(!host.dialog_stalled(),"hub: documented path does not stall");
      if(start==1 || start==7) {
        const int granted=start==1 ? 1 : 2;
        ready(host); run_to_close(host);
        host.campaign().open_menu(0);
        for(int i=0;i<17;++i)host.tick_mission_menu(0);
        // The menu initially selects its highest available row; constructed
        // states can already have mission 2 available when offer 1 is granted.
        if(granted==1 && (second&1))host.tick_mission_menu(q::BTN_UP);
        check(host.campaign().menu().row==granted-1,"hub: granted offer surfaces in production mission menu");
        host.tick_mission_menu(q::BTN_CROSS);
        check(host.campaign().menu().state==5,
              "hub: production menu accepts travel for the granted offer");
        sc::Host restored; fixture.load_into(restored,check); restored.load_campaign(host.save_campaign());
        check(restored.campaign().mission_available(granted),"hub: choice grant survives actual campaign save/load");
      }
      ++cases;
    }
  check(cases==160,"hub: 160 saved-state opening/choice cases");
  // All 31 predicates against real restored mission/variable state. Independent
  // expected true-node sets transcribed from the research table, not the port's
  // metadata. Completion suppresses pending/active even with nonzero variables;
  // unavailability is not suppressed by a variable without an active mission.
  for(int raw=0;raw<4;++raw) for(int value:{-128,-1,0,1,2,127}) {
    sc::Host host; fixture.load_into(host,check);
    auto record=host.campaign().record(0);
    for(int mission=1;mission<=12;++mission)
      record.missions[q::mission_index(1,mission)]=std::uint8_t(raw);
    host.campaign().set_record(0,record);
    host.load_campaign(host.campaign().save());
    for(int mission=1;mission<=12;++mission)
      host.variables().quest_set("v"+std::to_string(mission),value);
    host.load_campaign(host.save_campaign());
    const auto &bytes=fixture.level.files.at("npc.dbin");
    auto binding=champions::simulation::rta::hub_offers::Binding::match(
        *host.machine(),dl::parse_dbin(bytes.data(),bytes.size()));
    check(bool(binding),"hub: full resource/public structure binds");
    if(!binding)continue;
    std::set<int> true_nodes;
    if(raw==0 || raw==2)true_nodes={1,2,7,9,10,12,14,16,17,19,21,22,23,24,26,28,29};
    else if(raw==1 && value==0)true_nodes={3,8,11,13,15,18,20,25,27,30};
    else if(raw==1)true_nodes={0};
    for(int node=0;node<31;++node)
      check(binding->evaluate(std::size_t(node),host.variables())==bool(true_nodes.count(node)),
            "hub: each researched predicate matches independent saved-state expectations");
    open(host);
    const int start=true_nodes.empty() ? 0 : *true_nodes.begin();
    check(host.dialog().node==start,"hub: all-node truth sets compose with production start priority");
  }
  // Each of twelve signed variables must suppress ALL offers at node zero.
  // Test state is saved and restored before entering the real dialog path.
  for(int mission=1;mission<=12;++mission) for(int value:{-128,-1,1,2,127}) {
    sc::Host host; fixture.load_into(host,check);
    auto record=host.campaign().record(0);
    record.missions[q::mission_index(1,1)]=3;
    record.missions[q::mission_index(1,mission)]=1;
    host.campaign().set_record(0,record);
    host.load_campaign(host.campaign().save());
    host.variables().quest_set("v"+std::to_string(mission),value);
    host.load_campaign(host.save_campaign());
    const auto before=host.campaign().record(0).missions;
    open(host);
    check(host.dialog().node==0,"hub: every signed nonzero active variable suppresses offer");
    confirm(host);
    check(host.dialog_stalled() && host.campaign().record(0).missions==before,
          "hub: active branch refuses unproved action without a grant");
  }
  for(int control=1;control<=3;++control) {
    sc::Host host; fixture.load_into(host,check);
    host.variables().memory.byte(host.variables().mission_byte(0,1,1),3);
    if(control&1)host.variables().memory.byte(0x41b51d,1);
    if(control&2)host.campaign().open_menu(0);
    const auto vars=host.variables().quest_vars();
    const auto count=host.dialog_counts().scripts_run;
    open(host);
    check(!host.dialog().is_open() && host.dialog_counts().scripts_run==count+1,
          "hub: recall/menu block before predicate dispatch");
    check(host.variables().quest_vars()==vars && !host.campaign().mission_available(2),
          "hub: admission suppressors neither create quest variables nor grant");
  }
  // Run-task admission. research-mission-unlocks@e5c6176
  // docs/spec/mission-unlocks.md:143-180,226-231 and
  // research-mission2-route@6dc8968 docs/mission2-compose.md:99-112.
  // A registered task blocks the offer. Counter 0 is not removal. The next
  // negative step, and the state-8 body, drop the task. A new host loading
  // the image does not gain a task. 127-16*n never lands on exact zero, so
  // that registration is a constructed counter, as in the research.
  int menu_cases=0;
  auto offer_state=[&](sc::Host &host,int mission,bool stale) {
    fixture.load_into(host,check);
    auto record=host.campaign().record(0);
    // The other authored row can launch while this offer remains unavailable.
    record.missions[q::mission_index(1,3-mission)]=3;
    host.campaign().set_record(0,record);
    host.load_campaign(host.campaign().save());
    if(stale) {
      open(host);
      check(host.dialog().node==(mission==1 ? 1 : 7),"hub: menu control starts with an admitted offer");
      if(mission==1) { confirm(host); confirm(host); }
      check(host.dialog().node==(mission==1 ? 3 : 7),"hub: menu control reaches the real grant choice");
      ready(host);
    }
  };
  auto blocked_menu=[&](sc::Host &host,bool stale,int choice) {
    const auto before=host.save_campaign();
    const auto vars=host.variables().quest_vars();
    const auto scripts=host.dialog_counts().scripts_run;
    const auto refused=host.dialog_counts().refused.size();
    const auto gates=host.dialog_counts().open_gate_refused;
    if(stale) {
      confirm(host,choice);
      check(host.dialog_stalled() && host.dialog_stall_reason()==
            "confirm: hub offer admission suppressed",
            "hub: a registered run task suppresses a stale offer choice");
    } else {
      open(host);
      check(!host.dialog().is_open() && host.dialog_counts().open_gate_refused==gates+1 &&
            host.dialog_counts().refused.size()==refused,
            "hub: a registered run task does not enter the offer script");
    }
    check(host.dialog_counts().scripts_run==scripts+1,
          "hub: a registered run task stops predicate dispatch and the grant");
    check(host.variables().quest_vars()==vars && host.save_campaign()==before,
          "hub: a registered run task preserves mission bytes and quest variables");
    ++menu_cases;
  };
  auto admitted_menu=[&](sc::Host &host,bool stale,int choice,int mission) {
    const auto before=host.campaign().record(0).missions;
    if(stale) {
      const auto taken=host.dialog_counts().choices_taken;
      confirm(host,choice);
      auto after=before;
      after[q::mission_index(1,mission)]|=1;
      check(!host.dialog_stalled() && host.dialog_counts().choices_taken==taken+1 &&
            host.campaign().record(0).missions==after,
            "hub: a removed run task lets the already-open grant choice write");
    } else {
      open(host);
      check(host.dialog().is_open() && !host.dialog_stalled() &&
            host.dialog().node==(mission==1 ? 1 : 7) &&
            host.campaign().record(0).missions==before,
            "hub: a removed run task admits the offer and does not write on open");
    }
    ++menu_cases;
  };
  auto menu_case=[&](int mission,bool stale,int choice,const auto &drive) {
    sc::Host host; offer_state(host,mission,stale);
    drive(host);
    const bool running=host.campaign().menu_run_registered();
    const auto saved=host.save_campaign();
    const auto menu=host.campaign().menu();
    if(running) blocked_menu(host,stale,choice);
    else admitted_menu(host,stale,choice,mission);
    sc::Host restored; offer_state(restored,mission,stale);
    restored.load_campaign(saved);
    check(!restored.campaign().menu_run_registered() && restored.campaign().menu_open() &&
          restored.campaign().menu().state==menu.state &&
          restored.campaign().menu().counter==menu.counter &&
          restored.campaign().menu().pending==menu.pending,
          "hub: a new host restores menu fields and does not resurrect the run task");
    admitted_menu(restored,stale,choice,mission);
  };
  auto ready_menu=[&](sc::Host &host) {
    host.campaign().open_menu(0);
    for(int i=0;i<17;++i)host.tick_mission_menu(0);
    check(host.campaign().menu().state==2 && host.campaign().menu().counter==127,
          "hub: production menu reaches the researched input state");
  };
  auto launch_menu=[&](sc::Host &host,int ticks) {
    ready_menu(host);
    host.tick_mission_menu(q::BTN_CROSS);
    check(host.campaign().menu().state==5,"hub: available authored row enters launch");
    q::MenuInputs input; input.fade=128; // docs/quests.md:194-197
    for(int i=0;i<ticks;++i)host.tick_mission_menu(0,input);
    check(host.campaign().menu().state==(ticks<3 ? 5+ticks : 8) &&
          host.campaign().menu().pending.empty()==(ticks<4),
          "hub: launch control distinguishes entering state 8 from publishing a destination");
  };
  for(int mission:{1,2}) for(bool stale:{false,true}) for(int choice:{0,1}) {
    for(int ticks:{0,4,7,8,9,17})
      menu_case(mission,stale,choice,[&](sc::Host &host) {
        ready_menu(host);
        host.tick_mission_menu(q::BTN_CIRCLE);
        for(int i=0;i<ticks;++i)host.tick_mission_menu(0);
        // Eight decrements from 127: seven nonnegative, then 15-16 removes.
        const bool running=ticks<8;
        const int counter=running ? 127-16*ticks : 0;
        check(host.campaign().menu().state==4 && host.campaign().menu().counter==counter &&
              host.campaign().menu_run_registered()==running,
              "hub: cancel keeps the run task through the last nonnegative counter");
      });
    menu_case(mission,stale,choice,[&](sc::Host &host) {
      ready_menu(host); host.tick_mission_menu(q::BTN_CIRCLE);
      host.campaign().menu().counter=0; // constructed; 127-16*n never stores exact zero
      host.load_campaign(host.save_campaign());
      check(host.campaign().menu().state==4 && host.campaign().menu().counter==0 &&
            host.campaign().menu_run_registered(),
            "hub: exact zero without the negative step keeps the run task");
    });
    for(int ticks:{0,1,2,3,4,5})
      menu_case(mission,stale,choice,[&](sc::Host &host) { launch_menu(host,ticks); });
    for(int ticks:{0,1,17})
      menu_case(mission,stale,choice,[&](sc::Host &host) {
        launch_menu(host,4);
        const auto destination=host.campaign().menu().pending;
        // Save/restore BEFORE reopen as well as AFTER it in menu_case.
        host.load_campaign(host.save_campaign());
        host.campaign().open_menu(0);
        for(int i=0;i<ticks;++i)host.tick_mission_menu(0);
        check(host.campaign().menu().state==(ticks==0 ? 0 : ticks==1 ? 1 : 2) &&
              !destination.empty() && host.campaign().menu().pending==destination,
              "hub: reopened menu retains an old destination in states 0, 1 and 2");
      });
  }
  check(menu_cases==256,"hub: 256 live and fresh-host menu admission controls");
  // Counter 16 is the constructed last positive value. One tick stores 0 and
  // keeps the task; the next tick removes it. Reopen then selects node 8 and
  // choice 1 does not grant. research-mission2-route@6dc8968
  // docs/mission2-compose.md:101-112.
  {
    sc::Host host; fixture.load_into(host,check);
    auto record=host.campaign().record(0);
    record.missions[q::mission_index(1,1)]=3;
    host.campaign().set_record(0,record);
    host.load_campaign(host.campaign().save());
    open(host);
    check(host.dialog().node==7,"hub: mission 2 offer opens at node 7");
    confirm(host,0);
    check(host.campaign().mission_available(2)==1,"hub: choice 0 grants mission 2 once");
    const auto granted=host.campaign().record(0).missions;
    ready(host); run_to_close(host);
    check(!host.dialog().is_open(),"hub: the offer dialog closes before the menu");
    ready_menu(host);
    host.tick_mission_menu(q::BTN_CIRCLE);
    host.campaign().menu().counter=16;
    host.tick_mission_menu(0);
    check(host.campaign().menu().state==4 && host.campaign().menu().counter==0 &&
          host.campaign().menu_run_registered() && host.campaign().menu().paused==1,
          "hub: storing counter 0 leaves the run task registered");
    const auto at_zero=host.save_campaign();
    host.load_campaign(at_zero);
    check(host.campaign().menu_run_registered() && host.campaign().menu().counter==0,
          "hub: same-process load keeps the counter-0 run task");
    const auto scripts=host.dialog_counts().scripts_run;
    const auto vars=host.variables().quest_vars();
    open(host);
    check(!host.dialog().is_open() && host.dialog_counts().scripts_run==scripts+1 &&
          host.variables().quest_vars()==vars && host.campaign().record(0).missions==granted,
          "hub: counter 0 queries the run task and does not enter the offer");
    sc::Host fresh_zero; fixture.load_into(fresh_zero,check);
    fresh_zero.load_campaign(at_zero);
    check(!fresh_zero.campaign().menu_run_registered() && fresh_zero.campaign().menu().state==4 &&
          fresh_zero.campaign().menu().counter==0 && fresh_zero.campaign().menu_open(),
          "hub: the image of a counter-0 menu does not create a run task");
    host.tick_mission_menu(0);
    check(!host.campaign().menu_run_registered() && host.campaign().menu().state==4 &&
          host.campaign().menu().counter==0 && host.campaign().menu().paused==0,
          "hub: the next negative step removes the run task");
    const auto removed=host.save_campaign();
    host.load_campaign(removed);
    check(!host.campaign().menu_run_registered() && host.campaign().menu().state==4,
          "hub: same-process load does not resurrect a removed run task");
    open(host);
    check(host.dialog().node==8,"hub: reopen after removal selects node 8");
    const auto before_choice=host.campaign().record(0).missions;
    confirm(host,1);
    check(!host.dialog_stalled() && host.campaign().record(0).missions==before_choice,
          "hub: node 8 choice 1 does not grant again");
    sc::Host fresh_removed; fixture.load_into(fresh_removed,check);
    fresh_removed.load_campaign(removed);
    check(!fresh_removed.campaign().menu_run_registered(),"hub: fresh load after removal stays unregistered");
    open(fresh_removed);
    check(fresh_removed.dialog().node==8,"hub: fresh load after removal selects node 8");
    const auto fresh_before=fresh_removed.campaign().record(0).missions;
    confirm(fresh_removed,1);
    check(!fresh_removed.dialog_stalled() && fresh_removed.campaign().record(0).missions==fresh_before,
          "hub: fresh load choice 1 does not grant again");
  }
  // Admission/state is rechecked at the choice, not cached at opening.
  for(int control=0;control<3;++control) {
    sc::Host host; fixture.load_into(host,check);
    host.variables().memory.byte(host.variables().mission_byte(0,1,1),3);
    open(host); check(host.dialog().node==7,"hub: second offer initially admitted");
    if(control==0)host.variables().memory.byte(0x41b51d,1);
    else if(control==1)host.campaign().open_menu(0);
    else {
      host.variables().memory.byte(host.variables().mission_byte(0,1,12),1);
      host.variables().quest_set("v12",-1);
    }
    confirm(host);
    check(host.dialog_stalled() && !host.campaign().mission_available(2),
          "hub: changed recall/menu/quest state suppresses a stale offer choice");
  }
  // First-offer traversal must also revalidate admission before its final writer.
  for(int control=0;control<3;++control) {
    sc::Host host; fixture.load_into(host,check);
    open(host); confirm(host); confirm(host);
    check(host.dialog().node==3,"hub: first offer reaches final choice");
    if(control==0)host.variables().memory.byte(0x41b51d,1);
    else if(control==1)host.campaign().open_menu(0);
    else {
      host.variables().memory.byte(host.variables().mission_byte(0,1,12),1);
      host.variables().quest_set("v12",-1);
    }
    const auto before=host.campaign().record(0).missions;
    confirm(host);
    check(host.dialog_stalled() && host.campaign().record(0).missions==before,
          "hub: stale first-offer admission cannot write availability");
  }
  {
    sc::Host host; fixture.load_into(host,check);
    auto level=fixture.level;
    const auto &bytes=level.files.at("npc.dbin");
    auto nodes=dl::parse_dbin(bytes.data(),bytes.size());
    nodes[1].condition="p408"; // a known public, but the wrong documented branch
    level.files["npc.dbin"]=dl::build_dbin(nodes);
    host.attach_dialog(level,fixture.world.services());
    open(host);
    check(!host.dialog().is_open() && !host.campaign().mission_available(1) &&
          !host.dialog_counts().refused.empty(),"hub: a mismatched resource refuses without VM fallback");
  }
  // Explicit unknown later offer and absent-zero side effect.
  {
    sc::Host host; fixture.load_into(host,check);
    for(int mission:{1,2}) host.variables().memory.byte(host.variables().mission_byte(0,1,mission),3);
    open(host); check(host.dialog().node==9,"hub: later predicate is evaluated but not a grant");
    confirm(host);
    check(host.dialog_stalled() && !host.campaign().mission_available(3),"hub: later offer action refuses");
  }
  {
    sc::Host host; fixture.load_into(host,check);
    host.variables().memory.byte(host.variables().mission_byte(0,1,1),1);
    open(host);
    check(host.dialog().node==3 && host.variables().quest_vars().size()==1 &&
          host.variables().quest_vars()[0].second==0,"hub: an absent variable read creates zero");
  }
  table::run(check,where);
}

// ---- Researched hub / hub2 table (research-hub-npcs@fbbfe65 docs/hub-services.md) ----
// Authored AMX with the researched public offsets and native call shapes only.
// Conditions return 0: the production binding must answer from live state. No
// shipped bytes, names, text keys or dialogue.
namespace hd = champions::simulation::rta::hub_dialog;
namespace table {
enum N { START_DIALOG, AVAIL, STORE, SWOOP, QUEST, SETV, GETV, CALL_SCRIPT, GOLD, DEFER, PAUSED, MENU, ENTRANCE, TRIGGER };
inline const std::vector<std::string> natives{"startDialog","setMissionAvailable","activateStore","changeSwoop",
  "addQuest","setv","getv","callScript","givePlayerGold","addDeferedCall","gamePaused","activateMissionMenu","setEntrance",
  "trigger"};
// Unpacked strings: four bytes per character, so slots are 0x40 apart.
constexpr a::Cell S_STARTED=0x800, S_INTRO=0x500, S_DEMO=0x540, S_QUEST=0x580, S_SCAN=0x5C0, S_POLL=0x600,
                  S_ENT=0x640, S_SHARD=0x680, S_BLOOD=0x6C0, S_RING=0x700, S_USE=0x740, S_HELP=0x780,
                  S_WARN=0x7C0, UNPAUSED=0x404;
using L=std::vector<a::AsmLine>;
inline a::AsmInstruction I(const std::string &m,std::vector<a::AsmValue> o={}) { return {m,std::move(o),{},{}}; }
inline void call(L &l,int native,const std::vector<a::Cell> &args) {
  for(auto it=args.rbegin();it!=args.rend();++it) l.push_back(I("PUSH.C",{*it}));
  l.push_back(I("PUSH.C",{a::Cell(4*args.size())}));
  l.push_back(I("SYSREQ.C",{a::Cell(native)}));
  l.push_back(I("STACK",{a::Cell(4*(args.size()+1))}));
}
struct Options { bool gold_first=false; int wrong_avail=0; };
struct Fixture {
  Script script; sc::DialogLevel level; World world; std::vector<std::uint8_t> objects;
  hd::Hub hub;
  Fixture(hd::Hub h,Options o={}):hub(h) {
    std::vector<a::Cell> code(0x6000/4,a::Cell(a::opcode_number("ZERO.pri")));
    a::ImageOptions options; options.data.resize(0x2000); options.natives=natives;
    std::map<int,int> used; // start -> end of each placed public
    // Assembled at offset 0, then placed: code-operand jumps are moved by `at`.
    auto put=[&](int at,const std::string &name,L lines) {
      L body{I("PROC")}; body.insert(body.end(),lines.begin(),lines.end()); body.push_back(I("RETN"));
      auto assembly=a::assemble(body);
      check_free(used,at,int(4*assembly.cells.size()));
      std::size_t cell=0;
      for(const auto &line:body) {
        const auto *ins=std::get_if<a::AsmInstruction>(&line);
        if(!ins) continue;
        if(ins->mnemonic=="JNZ"||ins->mnemonic=="JZER"||ins->mnemonic=="JUMP") assembly.cells[cell+1]+=at;
        cell+=1+ins->operands.size();
      }
      std::copy(assembly.cells.begin(),assembly.cells.end(),code.begin()+at/4);
      options.publics.emplace_back(name,at);
    };
    const auto &rows=h==hd::Hub::Hub ? std::vector<hd::NodeRow>(hd::HUB_NODES.begin(),hd::HUB_NODES.end())
                                     : std::vector<hd::NodeRow>(hd::HUB2_NODES.begin(),hd::HUB2_NODES.end());
    const auto &acts=h==hd::Hub::Hub ? std::vector<hd::ActionRow>(hd::HUB_ACTIONS.begin(),hd::HUB_ACTIONS.end())
                                     : std::vector<hd::ActionRow>(hd::HUB2_ACTIONS.begin(),hd::HUB2_ACTIONS.end());
    const int var_cond=h==hd::Hub::Hub ? 0x498 : 0x28B4;
    std::set<int> conds;
    for(const auto &r:rows) { conds.insert(r.cond); for(int j=0;j<r.count;++j) if(r.choices[j].cond!=hd::NONE) conds.insert(r.choices[j].cond); }
    for(int at:conds) {
      L l;
      if(at==rows[0].cond) call(l,CALL_SCRIPT,{S_SCAN});
      if(at==var_cond) call(l,GETV,{h==hd::Hub::Hub ? S_DEMO : S_INTRO});
      l.push_back(I("CONST.pri",{0}));
      put(at,"c"+std::to_string(at),l);
    }
    for(const auto &act:acts) {
      L l; const auto &e=act.effect;
      if(o.gold_first && e.avail==2) call(l,GOLD,{0,5});
      if(e.avail) call(l,AVAIL,{o.wrong_avail && e.avail==2 ? o.wrong_avail : e.avail,1});
      if(e.quest) call(l,QUEST,{S_QUEST,S_QUEST});
      if(e.store) { call(l,SWOOP,{0}); call(l,STORE,{0}); }
      if(e.var!=hd::Var::None) call(l,SETV,{e.var==hd::Var::Demo ? S_DEMO : S_INTRO,1});
      put(act.offset,"x"+std::to_string(act.offset),l);
    }
    L scan; for(int m=0;m<12;++m) call(scan,GETV,{S_STARTED+0x40*m});
    put(0x5400,"scan",scan);
    put(0x5600,"talk",{I("PUSH.C",{0}),I("PUSH.C",{0}),I("PUSH.C",{400}),I("PUSH.C",{12}),
                       I("SYSREQ.C",{START_DIALOG}),I("STACK",{16})});
    // start queues two checks with bounds 9 and 12 (HS:329-337 shape).
    L start; call(start,DEFER,{S_SHARD}); call(start,DEFER,{S_BLOOD}); put(0x4D00,"start",start);
    put(0x4E00,"shard",{I("CONST.pri",{0}),I("BOUNDS",{9}),I("BOUNDS",{9})});
    put(0x4F00,"blood",{I("CONST.pri",{0}),I("BOUNDS",{12}),I("BOUNDS",{12})});
    // The mission-map warning chain at the researched publics (HS:32-34,
    // 373-383), resolved by the production code from literals and offsets:
    // the PushTrigger action 0x54 calls K.use; K.use tests the warning (here a
    // getv stands in for K.warn) and either calls K.help or writes the entrance
    // and opens the menu; K.help restarts the count and queues K.poll; K.poll
    // calls gamePaused once per run, counts unpaused runs, calls
    // activateMissionMenu on the 6th, never writes the entrance, and re-queues
    // itself. The tests count Script-task ticks outside the script.
    const bool hub1=h==hd::Hub::Hub;
    const int k_use=hub1 ? 0x49C0 : 0x2210, k_help=hub1 ? 0x4A80 : 0x22D0, k_poll=hub1 ? 0x4B0C : 0x235C;
    L action; call(action,CALL_SCRIPT,{S_USE}); put(hd::PUSH_ACTION,"pa",action);
    L use; call(use,GETV,{S_WARN});
    use.push_back(I("JZER",{std::string("go")}));
    call(use,CALL_SCRIPT,{S_HELP});
    use.push_back(I("JUMP",{std::string("done")}));
    use.push_back(a::Label{"go:"});
    call(use,ENTRANCE,{S_ENT}); call(use,MENU,{});
    use.push_back(a::Label{"done:"});
    put(k_use,"kuse",use);
    L help{I("CONST.pri",{0}),I("STOR.pri",{UNPAUSED})};
    call(help,DEFER,{S_POLL}); put(k_help,"khelp",help);
    L poll; call(poll,PAUSED,{});
    poll.push_back(I("JNZ",{std::string("requeue")}));
    poll.push_back(I("LOAD.pri",{UNPAUSED})); poll.push_back(I("ADD.C",{1})); poll.push_back(I("STOR.pri",{UNPAUSED}));
    poll.push_back(I("EQ.C.pri",{hd::WARN_POLL_RUNS}));
    poll.push_back(I("JZER",{std::string("requeue")}));
    call(poll,MENU,{});
    poll.push_back(a::Label{"requeue:"});
    call(poll,DEFER,{S_POLL});
    put(k_poll,"kpoll",poll);
    // K.walk's dispatch shape: trigger(ring record name, 0) (HS:395-399).
    L walk; call(walk,TRIGGER,{S_RING,0}); put(0x5300,"walk",walk);
    L walk1; call(walk1,TRIGGER,{S_RING,1}); put(0x5380,"walk1",walk1);
    script.image=a::build_image(code,options);
    script.strings={{400,"npc"},{S_QUEST,"q"},{S_SCAN,"scan"},{S_POLL,"kpoll"},{S_ENT,"ent"},
                    {S_USE,"kuse"},{S_HELP,"khelp"},{S_WARN,"kw"},
                    {S_SHARD,"shard"},{S_BLOOD,"blood"},{S_INTRO,"vi"},{S_DEMO,"vd"},{S_RING,"ring"}};
    for(int m=0;m<12;++m) script.strings.emplace_back(S_STARTED+0x40*m,"s"+std::to_string(m+1));
    std::vector<dl::Node> nodes(rows.size());
    for(std::size_t i=0;i<rows.size();++i) {
      const auto &r=rows[i];
      nodes[i].condition="c"+std::to_string(r.cond); nodes[i].text="body";
      for(int j=0;j<r.count;++j) {
        const auto &c=r.choices[std::size_t(j)];
        const auto role=c.next!=-1 ? "m" : (c.action!=hd::NONE &&
            std::any_of(acts.begin(),acts.end(),[&](const auto &x){return x.offset==c.action && x.effect.store;})) ? "s" : "e";
        nodes[i].choices.push_back({role,c.cond==hd::NONE ? "" : "c"+std::to_string(c.cond),
                                    c.action==hd::NONE ? "" : "x"+std::to_string(c.action),c.next});
      }
      nodes[i].raw_choices=r.count;
    }
    level.level="synthetic";
    level.require_hub_services=h==hd::Hub::Hub2;
    level.hub_level=true;
    level.files["npc.dbin"]=dl::build_dbin(nodes);
    dl::Node line; line.text="body";
    level.files["cut.dbin"]=dl::build_dbin({line});
    for(const char *key:{"nm","body","m","e","s","Affirmation","dialogMore"}) world.texts[key]=wide("authored");
    objects=objects_ob({{"NPC",0,{}, {"name=npc","showName=nm","dialogScript=talk","dialogOffset=0,0,0,0"}},
                        {h==hd::Hub::Hub ? "apprings" : "fpprings",0,{},{"name=ring"}}});
  }
  static void check_free(std::map<int,int> &used,int at,int size) {
    for(const auto &[from,to]:used)
      if(at<to && from<at+size) throw std::logic_error("fixture publics overlap");
    used.emplace(at,at+size);
  }
  void load_into(sc::Host &host,Check check) const {
    check(load(host,script,objects),"hub table: authored host loads");
    host.attach_dialog(level,world.services());
    std::vector<q::MissionRow> rows(13);
    for(int i=0;i<13;++i) rows[std::size_t(i)].story="authored-"+std::to_string(i);
    host.campaign().set_rows(rows);
    host.campaign().set_num_players(1);
  }
};
inline void set_mission(sc::Host &host,int n,int raw) {
  host.variables().memory.byte(host.variables().mission_byte(0,1,n),std::uint8_t(raw));
}
inline int mission(sc::Host &host,int n) { return host.variables().memory.read(host.variables().mission_byte(0,1,n)); }

// research-hub-npcs@fbbfe65 docs/hub-services.md:373-383 (HSF-2: Script-task
// runs, not frames). The tests count client Script-task ticks (on_ready_tick)
// themselves and read gamePaused's executed count as the per-run record. The
// action queues K.poll outside a tick, into the back half of the
// double-buffered queue (research-loading-readiness@d6f035fa
// docs/loading-arrival-r2.md:11-43): tick 1 only promotes it, and from tick 2
// on each tick runs the one queued copy once. So run r is tick r + 1. The entrance field 0x4f8468 holds a
// sentinel; the same PushTrigger path writes it when there is no warning.
inline void warn_poll(Check check,const Fixture &h1,const Fixture &h2) {
  const auto paused_runs=[](sc::Host &host) { return host.counts().executed[std::size_t(command_index("gamePaused"))]; };
  const auto entrance_writes=[](sc::Host &host) { return host.counts().executed[std::size_t(command_index("setEntrance"))]; };
  const auto menu_calls=[](sc::Host &host) { return host.counts().executed[std::size_t(command_index("activateMissionMenu"))]; };
  const auto warned=[&](const Fixture &f,sc::Host &host) {
    f.load_into(host,check);
    host.variables().memory.string(0x4f8468,"prior");
    host.variables().quest_set("kw",1);
    return host.run("pa");
  };
  for(const Fixture *f:{&h1,&h2}) {
    sc::Host probe; f->load_into(probe,check);
    check(probe.hub_research().warn_entries,"hub warn: K.use, K.help and K.poll resolve at the researched publics");
    // Control: no warning, so the same path writes the entrance and opens the menu.
    probe.variables().memory.string(0x4f8468,"prior");
    check(probe.run("pa")==sc::Stop::Complete && probe.variables().memory.cstr(0x4f8468)=="ent" &&
          entrance_writes(probe)==1 && probe.campaign().menu_open(),
          "hub warn: without a warning the action writes the entrance and opens the menu");
  }
  // HPS-2: the menu opens on the 6th unpaused run; runs listed in `paused`
  // (1-based) are paused and not counted; recall blocks the call's menu.
  // `want` is the tick the menu opens on (run + 1), -1 for none.
  struct Pattern { std::set<int> paused; bool recall; int want; const char *what; };
  const Pattern patterns[]={
    {{},false,7,"hub warn: menu on the 6th unpaused run (tick 7)"},
    {{2,3,4},false,10,"hub warn: runs 2-4 paused move the menu to run 9 (tick 10)"},
    {{1,5,6,9},false,11,"hub warn: scattered paused runs are not counted (run 10, tick 11)"},
    {{},true,-1,"hub warn: recall blocks the 6th-run menu"},
  };
  for(const Fixture *f:{&h1,&h2})
    for(const auto &p:patterns) {
      sc::Host host;
      const auto stop=warned(*f,host);
      if(p.recall) host.variables().memory.byte(0x41b51d,1);
      check(stop==sc::Stop::Wait && !host.campaign().menu_open() && entrance_writes(host)==0 &&
            host.variables().deferred_pending("kpoll")==1 && paused_runs(host)==0,
            "hub warn: the warning queues one K.poll and writes nothing");
      int opened=-1, menu_tick=-1;
      bool one_run_per_tick=true;
      for(int tick=1;tick<=14;++tick) {
        host.variables().memory.word(0x41b4f4,p.paused.count(tick-1) ? 1u : 0u);
        const auto runs=paused_runs(host), menus=menu_calls(host);
        host.on_ready_tick();
        one_run_per_tick=one_run_per_tick && paused_runs(host)==runs+(tick==1 ? 0u : 1u) &&
                         host.variables().deferred_pending("kpoll")==1;
        if(menu_calls(host)!=menus && menu_tick<0) menu_tick=tick;
        if(opened<0 && host.campaign().menu_open()) opened=tick;
      }
      host.variables().memory.word(0x41b4f4,0);
      check(one_run_per_tick,"hub warn: tick 1 promotes, each later tick runs K.poll once, one copy stays queued");
      check(opened==p.want,p.what);
      if(p.recall) check(menu_tick==7,"hub warn: with recall the 6th run still calls activateMissionMenu");
      check(host.variables().memory.cstr(0x4f8468)=="prior" && entrance_writes(host)==0,
            "hub warn: K.poll writes no entrance");
      check(!host.stopped_on_refusal() && host.dialog_counts().hub_warn_refused.empty(),
            "hub warn: one copy and no help box run unrefused");
    }
  // HPS-1: a second warning while K.poll is pending refuses (HS:382-383),
  // whether it comes before or after the first tick.
  for(const Fixture *f:{&h1,&h2})
    for(int before:{0,2}) {
      sc::Host host;
      warned(*f,host);
      for(int t=0;t<before;++t) host.on_ready_tick();
      const auto runs=paused_runs(host);
      const auto again=host.run("pa");
      check(again==sc::Stop::Refused && host.stopped_on_refusal() &&
            host.dialog_counts().hub_warn_refused.size()==1 && host.variables().deferred_pending("kpoll")==1,
            "hub warn: a second K.poll copy while one is pending refuses");
      for(int t=0;t<10;++t) host.on_ready_tick();
      check(paused_runs(host)==runs && !host.campaign().menu_open() && entrance_writes(host)==0,
            "hub warn: after that refusal K.poll no longer runs and no menu opens");
    }
  // HPS-1: K.poll counting while a help box is up refuses (HS:381-382: that
  // the box pauses is Inferred). The help box is opened through the HUD state
  // the client's help box sets.
  // The box is opened before run `open_at` (tick open_at + 1) and closed two
  // ticks later; runs before it count normally.
  for(const Fixture *f:{&h1,&h2})
    for(int open_at:{1,4}) {
      sc::Host host;
      warned(*f,host);
      for(int tick=1;tick<=12;++tick) {
        if(tick==open_at+1) host.hud().help_open=true;
        if(tick==open_at+3) host.hud().help_open=false;
        host.on_ready_tick();
      }
      check(host.stopped_on_refusal() && host.dialog_counts().hub_warn_refused.size()==1,
            "hub warn: a K.poll run while the help box is up refuses");
      check(paused_runs(host)==std::uint64_t(open_at-1) && menu_calls(host)==0 && !host.campaign().menu_open() &&
            entrance_writes(host)==0,"hub warn: that run is not counted and no later run opens the menu");
    }
  // Other help-box and deferred uses are unaffected: start's two checks queue
  // freely and gamePaused outside K.poll is not refused.
  {
    sc::Host host; h2.load_into(host,check);
    host.hud().help_open=true;
    check(host.run("start")==sc::Stop::Wait && host.dialog_counts().hub_warn_refused.empty(),
          "hub warn: start's deferred checks are not K.poll copies");
  }
}

inline void run(Check check,std::string &where) {
  where="researched hub tables";
  const Fixture h2(hd::Hub::Hub2), h1(hd::Hub::Hub);
  // Field-by-field binding; the narrower legacy table does not bind.
  {
    sc::Host host; h2.load_into(host,check);
    const auto &bytes=h2.level.files.at("npc.dbin");
    const auto nodes=dl::parse_dbin(bytes.data(),bytes.size());
    std::string why;
    const auto b=hd::Binding::match(*host.machine(),nodes,&why);
    check(b && b->hub==hd::Hub::Hub2 && why.empty(),"hub table: hub2 26-node table binds");
    check(b && b->started[0]=="s1" && b->started[11]=="s12" && b->intro=="vi",
          "hub table: started and intro variables resolve from literal getv sites");
    check(b && b->actions.size()==19,"hub table: hub2 binds its 19 actions");
    hub_offer_host_tests::Fixture old;
    const auto &ob=old.level.files.at("npc.dbin");
    sc::Host oh; old.load_into(oh,check);
    check(!hd::Binding::match(*oh.machine(),dl::parse_dbin(ob.data(),ob.size())),
          "hub table: the legacy partial fixture does not bind the researched table");
    sc::Host hh; h1.load_into(hh,check);
    const auto &hb=h1.level.files.at("npc.dbin");
    const auto b1=hd::Binding::match(*hh.machine(),dl::parse_dbin(hb.data(),hb.size()));
    check(b1 && b1->hub==hd::Hub::Hub && b1->actions.size()==22 && b1->demo=="vd",
          "hub table: hub 31-node table binds its 22 actions");
  }
  // HS:298, hub2 intro: C(1) shows only the second MORE; it sets the intro
  // variable and continues to the mission-2 offer, whose END sets A(2).
  {
    sc::Host host; h2.load_into(host,check);
    set_mission(host,1,3);
    const auto gold=host.variables().memory.word(host.variables().player_record(0)+0xe4);
    open(host);
    check(host.dialog().node==1 && host.hub_services() && host.hub_services_start()==1,
          "hub table: hub2 intro opens H2.N01");
    check(host.dialog().visible.size()==1 && host.dialog().visible[0]==1,"hub table: only the C(1) choice is visible");
    confirm(host);
    check(host.dialog().node==3 && host.variables().quest_get("vi")==1,"hub table: intro set, mission-2 offer shown");
    confirm(host,0);
    check((mission(host,2)&1)==1 && !host.dialog_stalled(),"hub table: END on the offer sets A(2)");
    check(host.variables().memory.word(host.variables().player_record(0)+0xe4)==gold,"hub table: no gold write");
    check(host.dialog_counts().hub_service_actions==2 && host.dialog_counts().hub_service_refused.empty(),
          "hub table: two researched actions, no fence refusal");
  }
  // !C(1): the first MORE leads to H2.N02 (store/close).
  {
    sc::Host host; h2.load_into(host,check);
    open(host);
    check(host.dialog().node==1 && host.dialog().visible.size()==1 && host.dialog().visible[0]==0,
          "hub table: without C(1) only the first MORE is visible");
    confirm(host);
    check(host.dialog().node==2 && (mission(host,2)&1)==0,"hub table: N02 reached with no mission write");
  }
  // SHOP on an offer: A(3), quest none, swoop 0, store deferred until close (HS:247-268).
  {
    sc::Host host; h2.load_into(host,check);
    set_mission(host,1,3); set_mission(host,2,3); host.variables().quest_set("vi",1);
    host.variables().memory.word(0x4fdce4,7);
    open(host);
    check(host.dialog().node==5,"hub table: mission-3 offer opens H2.N05");
    confirm(host,1);
    check((mission(host,3)&1)==1,"hub table: SHOP sets A(3)");
    check(host.dialog_counts().shop_requests==0 && host.dialog_counts().deferred_shops==0 &&
          host.dialog().store_pending==1 && host.variables().memory.word(0x4fdce4)==0,
          "hub table: inside the dialog the store is only marked pending and the swoop is 0");
    ready(host); run_to_close(host);
    check(!host.dialog().is_open() && host.dialog_counts().deferred_shops==1 &&
          host.dialog_counts().shop_requests==0,"hub table: the store opens at dialog end");
  }
  // Every reminder: A(k), not started -> PENDING(k) node, close only.
  for(int k:{2,3,4,5,6,7,10,11,12}) {
    sc::Host host; h2.load_into(host,check);
    host.variables().quest_set("vi",1);
    for(int m=1;m<k;++m) set_mission(host,m,3);
    set_mission(host,k,1);
    open(host);
    int pending=-1;
    for(std::size_t i=0;i<hd::HUB2_NODES.size();++i)
      if(hd::HUB2_NODES[i].kind==hd::Kind::Pending && hd::HUB2_NODES[i].mission==k) { pending=int(i); break; }
    check(host.dialog().node==pending,"hub table: hub2 pending mission opens its reminder");
    host.variables().quest_set("s"+std::to_string(k),1);
    run_to_close(host);
    open(host);
    check(host.dialog().node==0,"hub table: a started incomplete mission opens node 0");
  }
  // Hub: mission 10 offer is the two-node MORE chain then END/SHOP with a quest.
  {
    sc::Host host; h1.load_into(host,check);
    for(int m=1;m<=9;++m) set_mission(host,m,3);
    const auto quests=host.campaign().record(0).quest_count;
    open(host);
    check(host.dialog().node==23,"hub table: hub mission-10 offer opens H1.N23");
    confirm(host);
    check(host.dialog().node==24 && (mission(host,10)&1)==0,"hub table: MORE writes nothing");
    confirm(host,0);
    check((mission(host,10)&1)==1 && host.campaign().record(0).quest_count==quests+1,"hub table: END sets A(10) and adds a quest");
  }
  // The five-native fence (HS:112-142): a gold native, or a different mission,
  // stops the action before it writes.
  for(int variant=0;variant<2;++variant) {
    Options o; if(variant==0) o.gold_first=true; else o.wrong_avail=5;
    Fixture f(hd::Hub::Hub2,o);
    sc::Host host; f.load_into(host,check);
    set_mission(host,1,3); host.variables().quest_set("vi",1);
    open(host);
    check(host.dialog().node==3,"hub table: fence fixture opens the mission-2 offer");
    confirm(host,0);
    check(host.dialog_stalled() && (mission(host,2)&1)==0 && (mission(host,5)&1)==0,
          "hub table: the fence refuses before any write");
    check(host.dialog_counts().hub_service_refused.size()==1,"hub table: the fence records one reason");
  }
  // hub2 must bind the table; a dialog that differs refuses (no VM fallback).
  {
    auto f=h2;
    const auto &bytes=f.level.files.at("npc.dbin");
    auto nodes=dl::parse_dbin(bytes.data(),bytes.size());
    std::swap(nodes[3].choices[0].action,nodes[3].choices[1].action);
    f.level.files["npc.dbin"]=dl::build_dbin(nodes);
    sc::Host host; f.load_into(host,check);
    set_mission(host,1,3); host.variables().quest_set("vi",1);
    open(host);
    check(!host.dialog().is_open() && !host.hub_services() && !host.hub_services_mismatch().empty(),
          "hub table: a hub2 dialog that differs from the table refuses");
  }
  // Static facts: tables of 10 and 13; one cutscene line list (HS:64-69, 329-337).
  {
    sc::Host host; h1.load_into(host,check);
    const auto facts=host.hub_research();
    check(facts.dedupe && facts.dedupe->shard_cells==hd::SHARD_TABLE && facts.dedupe->blood_cells==hd::BLOOD_TABLE,
          "hub table: deferred checks have 10- and 13-cell bounds");
    check(facts.cutscene_line_lists==1 && facts.dialog_files==2,"hub table: the one-node line list is classified");
    dl::Node with; with.text="body"; with.choices={{"e","","",-1}}; with.raw_choices=1;
    check(!hd::cutscene_line_list({with}) && hd::cutscene_line_list({dl::Node{}}),"hub table: line-list shape rule");
  }
  // Walk-in (HS:395-401): trigger(ring, 0) is admitted with the dialog attached
  // and only counted; another value, or a level that is not a hub, refuses.
  for(int variant=0;variant<3;++variant) {
    auto f=h1;
    if(variant==2) f.level.hub_level=false;
    sc::Host host; f.load_into(host,check);
    const auto stop=host.run(variant==1 ? "walk1" : "walk");
    if(variant==0)
      check(stop==sc::Stop::Complete && host.portal_trigger_requests()==1 && !host.stopped_on_refusal(),
            "hub table: walk-in trigger(ring, 0) is counted once");
    else
      check(host.portal_trigger_requests()==0 && host.stopped_on_refusal(),
            "hub table: other trigger values and non-hub levels refuse");
  }
  warn_poll(check,h1,h2);
}
} // namespace table
} // namespace hub_offer_host_tests
