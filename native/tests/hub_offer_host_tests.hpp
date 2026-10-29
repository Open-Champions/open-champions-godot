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
  // PPR2-1: research-mission-unlocks@950b2538 docs/spec/mission-unlocks.md:120-125
  // suppresses a running menu. docs/quests.md:193,357-358 only follows four
  // closing ticks (127 -> 63); :197 proves publication, NOT task termination.
  // Later countdown ticks below exercise the port's refusal boundary, not a
  // newly established original lifecycle. 127 - 16*n skips exact zero, so the
  // zero case is an explicitly authored imported-state control as well.
  // Both opening and already-admitted first/second choices use real quest
  // records, menu ticks, VM/binder actions, and production campaign save/load.
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
  auto refused_menu=[&](sc::Host &host,bool stale,int choice) {
    const auto before=host.save_campaign();
    const auto vars=host.variables().quest_vars();
    const auto scripts=host.dialog_counts().scripts_run;
    if(stale) {
      confirm(host,choice);
      check(host.dialog_stalled() && host.dialog_stall_reason()==
            "confirm: mission menu admission refused: run-task termination is Unknown",
            "hub: stale choice explicitly refuses unresearched menu termination");
    } else {
      open(host);
      check(!host.dialog().is_open() && host.dialog_counts().open_gate_refused==1 &&
            host.dialog_counts().refused.count(
                "open: mission menu admission refused: run-task termination is Unknown")==1,
            "hub: opening explicitly refuses unresearched menu termination");
    }
    // Opening counts the NPC entry only; stale confirmation counts the refused
    // action attempt only. Neither may evaluate predicates/create-zero reads.
    check(host.dialog_counts().scripts_run==scripts+1,
          "hub: menu refusal stops predicate dispatch and VM grant execution");
    check(host.variables().quest_vars()==vars && host.save_campaign()==before,
          "hub: menu refusal preserves all saved mission bytes and quest variables");
    ++menu_cases;
  };
  auto menu_case=[&](int mission,bool stale,int choice,const auto &drive) {
    sc::Host host; offer_state(host,mission,stale);
    drive(host);
    const auto saved=host.save_campaign();
    const auto menu=host.campaign().menu();
    refused_menu(host,stale,choice);
    sc::Host restored; offer_state(restored,mission,stale);
    restored.load_campaign(saved);
    check(restored.campaign().menu_open() && restored.campaign().menu().state==menu.state &&
          restored.campaign().menu().counter==menu.counter && restored.campaign().menu().pending==menu.pending,
          "hub: replacement host restores the actual running-menu save state");
    refused_menu(restored,stale,choice);
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
        check(host.campaign().menu().state==4 && host.campaign().menu().counter==127-16*ticks,
              "hub: cancel checkpoint does not claim run-task completion");
      });
    menu_case(mission,stale,choice,[&](sc::Host &host) {
      ready_menu(host); host.tick_mission_menu(q::BTN_CIRCLE);
      host.campaign().menu().counter=0; // authored import; not an observed countdown tick
      host.load_campaign(host.save_campaign());
      check(host.campaign().menu().state==4 && host.campaign().menu().counter==0,
            "hub: exact-zero imported save remains a termination-unknown control");
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
  check(menu_cases==256,"hub: 256 live/replacement-host menu refusal controls");
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
}
} // namespace hub_offer_host_tests
