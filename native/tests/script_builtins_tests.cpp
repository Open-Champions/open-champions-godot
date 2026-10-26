#include "champions/script_builtins.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>

namespace b = champions::simulation::rta::builtins;
namespace a = champions::simulation::rta::amx;
namespace {
const std::vector<std::string> names = {"getv", "setv", "addv", "getpv", "setpv"};
const std::vector<std::string> campaign = {"getEvil", "setEvil", "getMadeDecision", "setMadeDecision",
 "grantMission", "getMissionComplete", "setMissionAvailable", "getMissionAvailable",
 "grantGold", "grantSilver", "grantBronze", "getMedal", "addQuest", "removeQuest"};
const std::vector<std::string> players = {"getNumPlayers", "countPlayersAlive", "checkPlayerAlive", "activePlayerIsDwarf", "activePlayerIsElf", "isClericOrWizard", "getMaxPlayerHP", "healPlayer", "giveFullHealth", "zeroPlayerMana", "refillPlayerMana", "givePlayerExp", "givePlayerGold", "takePlayerGold", "checkPlayerGold", "checkPlayerMinGold", "playerAdjustStat", "setPlayerFlag", "getPlayerFlag", "piercePlayerEar", "clearAllSkills", "hidePlayers", "decayPlayerLights", "getManaUsed", "getHealthUsed"};
const std::vector<std::string> scene = {"newWorld", "newWorldAlways", "simpleNewWorld", "newWorldPost", "newWorldParameter", "setEntrance", "cutScene", "startDialog", "dialogRunning", "forceMutexRelease", "changeSwoop", "startWithSwoopDone", "leaveGame", "endGame", "exitToMainMenu", "saveGame", "saveEndConfig", "saveDone", "saveCheckPoint", "loadCheckPoint", "setSaveInhibit", "getSaveInhibit", "setAlternateDeath", "preloadResource", "clearRecall", "playersLoaded", "pauseGameControl"};
const std::vector<std::string> menus = {"activateMissionMenu", "activateStore", "storeRunning", "setInfo", "addHelpMessage", "addHelpMessageF", "helpMessageActive", "showCompass", "showBust", "hudResetFadeScreen", "hudFadeScreen", "revealMap", "showOnMap", "runMedalSuccess", "runMedalFail", "setupPlayerSel", "startPlayerSel", "playerSelRunning"};
const std::vector<std::string> creature_names = {"increaseEnemyRank", "charmMonster", "rootMonster", "unRootMonster", "selectSetMonsterVal", "setMonsterVal", "selectSetCFlag", "setCFlag", "selectSetOFlag", "setOFlag", "setWeaponType", "setMonsterRange", "killMonster", "deleteMonster", "hideMonster", "showMonster", "moveMonster", "followPlayer", "stopFollowingPlayer", "followPath", "selectFollowPath", "stopFollowingPath", "selectStopPath", "friendly", "unFriendly", "reapEnemies", "checkMonstersLeft", "getHitPoints", "setHitPoints", "generateMonster", "monstersInTrigger", "initScriptSetParams", "setCreatureParams", "startNPCAnim", "NPCAnimAtEnd", "countLiveObjects"};
const std::vector<std::string> item_names = {"playerHasItem", "onePlayerHasItem", "countQuestItems", "removePlayerItem", "removeAllItems", "removeAllWeapons", "givePlayerItem", "giveOnePlayerItem", "givePlayerRandom", "dropItem", "selectDropItem"};
const std::vector<std::string> effects_names = {"trigger", "autoPropSetState", "autoPropGetState", "startPropAnim", "stopPropAnim", "propAnimAtEnd", "deleteObjects", "setNoCollide", "objInPlayerTrig", "findDistance", "setContainerProps", "lightEffect", "addExpandingPool", "particleMorph", "setParticle", "addParticleOnObject", "shakeCamera", "setFarClip", "setWaveDetail"};
const std::vector<std::string> script_names = {"callScript", "addDeferedCall", "postAction"};
const std::vector<std::string> utility_names = {"print", "printVal", "getRand", "getScaledTime", "sin1000", "cos1000", "gamePaused", "demoRunning", "networkMode", "levelLoading", "getLevelTimer", "sound", "soundSequence", "startMusic", "startAudio", "endMusic"};
const std::vector<std::string> interaction_names = {"setBestActivePlayer", "hitPlayers", "closestPlayerDist", "savePlayer", "restorePlayer", "hornOmen", "planarSteel", "playGotItemSound", "explodePotionsOnly"};
const std::vector<std::string> vehicle_names = {"putNPCInVehicle", "putPlayersInVehicle", "entityRideVehicle", "controlVehicle", "holdVehicle", "getPlayerVPosition", "getVPosition", "getPlayerVDir"};
struct CallbackFault : std::runtime_error { using std::runtime_error::runtime_error; };
void require(bool ok, const std::string &message) {
  if (!ok) throw std::runtime_error(message);
}
std::string hex(std::uint64_t value, int width = 8) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(width) << value;
  return out.str();
}
std::string number(a::Address value) { return hex(static_cast<std::uint32_t>(value)); }
std::vector<std::string> split(const std::string &value, char sep) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    auto end = value.find(sep, start);
    result.push_back(value.substr(start, end - start));
    if (end == value.npos) return result;
    start = end + 1;
  }
}
std::uint32_t numeric(const std::string &value) {
  require(value.size() == 8, "input integer width");
  std::size_t consumed = 0;
  auto result = std::stoull(value, &consumed, 16);
  require(consumed == 8 && result <= UINT32_MAX, "input integer");
  return static_cast<std::uint32_t>(result);
}
std::string unhex(const std::string &value) {
  if (value == "-") return {};
  require(value.size() % 2 == 0, "input string width");
  std::string result;
  for (std::size_t i = 0; i < value.size(); i += 2) {
    auto part = value.substr(i, 2);
    std::size_t consumed = 0;
    auto ch = std::stoul(part, &consumed, 16);
    require(consumed == 2 && ch <= 255, "input byte");
    result.push_back(static_cast<char>(ch));
  }
  return result;
}
std::string text(const std::string &value) {
  std::string result;
  for (unsigned char ch : value) result += hex(ch, 2);
  return result.empty() ? "-" : result;
}
b::Memory::Integer integer(const std::string &token) {
  auto fields=split(token,':');
  require(fields.size()==2 && (fields[0]=="0" || fields[0]=="1"),"signed integer fixture");
  auto bytes=unhex(fields[1]);
  return {{bytes.begin(),bytes.end()},fields[0]=="1"};
}
std::string integer_text(const b::Memory::Integer &value) {
  return std::string(value.negative ? "1:" : "0:")+text({value.magnitude.begin(),value.magnitude.end()});
}
template<class T> std::string join(const T &values) {
  std::string result;
  for (const auto &value : values) {
    if (!result.empty()) result += ',';
    result += value;
  }
  return result.empty() ? "-" : result;
}
std::string host_value(const b::Host::Value &v) {
  if(std::holds_alternative<std::monostate>(v)) return "-";
  if(auto n=std::get_if<std::int64_t>(&v)) return number(*n);
  if(auto str=std::get_if<std::string>(&v)) return text(*str);
  if(auto obj=std::get_if<b::Host::Object *>(&v)) return "obj:"+number((*obj)->addr);
  if(auto list=std::get_if<b::Host::List>(&v)) {
    std::vector<std::string> entries;for(auto obj:*list) entries.push_back(number(obj->addr));
    return "list:"+join(entries);
  }
  if(auto vec=std::get_if<b::Host::Vec3>(&v))
    return "("+number((*vec)[0])+";"+number((*vec)[1])+";"+number((*vec)[2])+")";
  if(auto pair=std::get_if<std::pair<std::int64_t,std::int64_t>>(&v))
    return "("+number(pair->first)+";"+number(pair->second)+")";
  const auto &seat=std::get<b::Host::Seat>(v);
  return "("+host_value(seat.position)+";"+number(seat.facing)+")";
}
std::string host_arguments(const b::Host::Arguments &args) {
  std::string out;bool first=true;
  for(const auto &arg:args) {if(!first) out+=';';first=false;out+=host_value(arg);}
  return out;
}
struct Runner {
  std::unique_ptr<b::Variables> variables;
  std::shared_ptr<a::Machine> machine;
  unsigned mode = 0;
  std::vector<std::string> events;
  std::vector<std::string> bindings;
  bool control_mode=false;
  bool player_mode=false, scene_mode=false, menu_mode=false, creature_mode=false, item_mode=false, effects_mode=false, vehicle_mode=false;
  void reset(bool extended=false, bool with_players=false, bool with_scene=false, bool with_menus=false, bool with_creatures=false, bool with_items=false, bool with_effects=false, bool with_vehicles=false, bool with_interactions=false, bool with_utilities=false, bool with_control=false) {
    control_mode=with_control;
    vehicle_mode=with_vehicles;effects_mode=with_effects;item_mode=with_items;creature_mode=with_creatures;menu_mode=with_menus;player_mode=with_players;scene_mode=with_scene;
    bindings=names;
    if (extended) bindings.insert(bindings.end(),campaign.begin(),campaign.end());
    if(with_players) bindings.insert(bindings.end(),players.begin(),players.end());
    if(with_scene) bindings.insert(bindings.end(),scene.begin(),scene.end());
    if(with_menus) bindings.insert(bindings.end(),menus.begin(),menus.end());
    if(with_creatures) bindings.insert(bindings.end(),creature_names.begin(),creature_names.end());
    if(with_control) bindings.insert(bindings.end(),script_names.begin(),script_names.end());
    if(with_utilities) bindings.insert(bindings.end(),utility_names.begin(),utility_names.end());
    if(with_interactions) bindings.insert(bindings.end(),interaction_names.begin(),interaction_names.end());
    if(with_vehicles) bindings.insert(bindings.end(),vehicle_names.begin(),vehicle_names.end());
    if(with_effects) bindings.insert(bindings.end(),effects_names.begin(),effects_names.end());
    if(with_items) bindings.insert(bindings.end(),item_names.begin(),item_names.end());
    variables = std::make_unique<b::Variables>();
    a::ImageOptions options;
    options.data.resize(4096);
    options.natives = bindings;
    options.stack_cells = 128;
    machine = std::make_shared<a::Machine>(a::build_image({0}, options), variables->natives());
    require(machine->load() == 0, "fixture machine load");
    variables->request_config_save = [this]() {
      events.push_back("memcard_requestConfigSave");
      if (mode == 1) throw CallbackFault("authored callback exception");
      if(control_mode && mode==2) {
        variables->memory.word(0x86da78,0);variables->memory.word(0x4f8690,UINT32_MAX);
        variables->queue_deferred("leaf");variables->set_post_action("leaf");
      }
      if(control_mode && mode==3) variables->set_script(nullptr);
      if (mode == 2 || mode == 3) {
        auto value = variables->quest_get("Callback");
        variables->quest_set("Callback", value + 1);
        variables->profile_set("Unsaved", 99, false);
      }
      if (mode == 3) variables->forget_string_slot();
    };
    variables->scene_service = [this](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
      std::string event=service;
      for(const auto &arg:args) {
        event+=':';
        if(auto v=std::get_if<std::uint32_t>(&arg)) event+=number(*v);
        else if(auto v=std::get_if<std::string>(&arg)) event+=text(*v);
        else {const auto &pos=std::get<std::array<std::uint32_t,3>>(arg);event+=number(pos[0])+'/'+number(pos[1])+'/'+number(pos[2]);}
      }
      events.push_back(event);
      if(mode==1 || (mode==3 && events.size()>=(variables->memory.word(0x1008) ? variables->memory.word(0x1008) : 2))) throw CallbackFault("authored callback exception");
      if(mode==2) {
        variables->memory.word(0x4d8ae4,3);variables->memory.word(0x4d8ae0,1);
        machine->set_cell(3008,0x1234);variables->memory.word(0x8e2d60,0);
        if(menu_mode) {variables->memory.word(0x4f8420,1);variables->memory.word(0x4f8440,3);
          variables->memory.word(0x2010,0x7fffffff);variables->memory.word(0x4f7140,1);variables->memory.byte(0x4f7148,3);}
      }
      if(mode==2 && creature_mode) {
        variables->memory.word(0x200c,0x20);variables->memory.word(0x20a8,0x88000001);
        variables->memory.word(0x2014,0x80000000);machine->set_cell(3012,UINT32_MAX);
      }
      return variables->memory.word(menu_mode && service=="text_drawWrappedCentered" ? 0x100c : 0x1004);
    };
    variables->vehicle_seat = [this](std::uint32_t vehicle,const std::array<std::uint32_t,3> &offset) {
      variables->scene_service("virtual",{19u,vehicle,offset});
      return b::Variables::VehicleSeat{{variables->memory.word(0x1200),variables->memory.word(0x1204),variables->memory.word(0x1208)},variables->memory.word(0x120c)};
    };
    variables->inventory_service = [this](const std::string &service,const std::vector<b::Variables::InventoryValue> &args) -> b::Variables::InventoryResult {
      std::string event=service;
      for(const auto &arg:args) {
        event+=':';
        if(auto v=std::get_if<std::uint32_t>(&arg)) event+=number(*v);
        else if(auto v=std::get_if<std::string>(&arg)) event+=text(*v);
        else if(auto v=std::get_if<std::array<std::uint32_t,3>>(&arg)) event+=number((*v)[0])+'/'+number((*v)[1])+'/'+number((*v)[2]);
        else {
          const auto &blank=std::get<b::Variables::ItemBlank>(arg);
          event+="blank:";for(auto byte:blank.bytes) event+=hex(byte,2);
          for(auto word:blank.words) event+='/'+number(word);
        }
      }
      events.push_back(event);
      auto &mem=variables->memory;
      if(mode==1 || (mode==3 && events.size()>=(mem.word(0x1008) ? mem.word(0x1008) : 2))) throw CallbackFault("authored callback exception");
      if(mode==2) {
        mem.word(0x4d8ae0,1);mem.byte(0x4f7148,3);mem.word(0x4f713c,1);
        machine->set_cell(3008,UINT32_MAX);machine->set_cell(3012,UINT32_MAX);
        mem.word(variables->player_record(0)+0x43d8,0x4000);mem.word(0x4010,0x7fffffff);
        mem.word(variables->player_record(0)+0x130,0);mem.word(variables->player_record(0)+0x1330,99);mem.byte(0x121d,255);
      }
      if(service=="inventory_findItemBySubstring") return std::array<std::uint32_t,2>{mem.word(0x1110),mem.word(0x1114)};
      const std::map<std::string,std::uint32_t> addresses={{"item_parseDescriptor",0x1100},{"player_itemWeight",0x1104},
        {"inventory_giveItem",0x1108},{"pool_new",0x110c},{"level_monsterLevelTimesDifficulty",0x1118},
        {"item_generateWithAffixes",0x111c},{"inventory_countItemByName",0x1120}};
      auto it=addresses.find(service);return mem.word(it==addresses.end() ? 0x1004 : it->second);
    };
    variables->object_query = [this](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
      variables->scene_service(service,args);
      std::vector<champions::simulation::rta::objects::GameObject *> result;
      for(unsigned k=0;k<variables->memory.read(0x1100);++k) result.push_back(variables->obj_at(variables->memory.word(0x1104+4*k)));
      return result;
    };
    variables->player_service = [this](const std::string &service, const std::vector<std::uint32_t> &args) {
      std::string event=service;
      for(auto arg:args) event+=':'+number(arg);
      events.push_back(event);
      if(mode==1) throw CallbackFault("authored callback exception");
      if(mode==2 || mode==3) {variables->memory.word(0x4d8ae0,mode==2 ? 1 : 2);variables->memory.byte(0x4f7149,3);machine->set_cell(3004,19);}
      return 0u;
    };
    variables->find_resource = [this](const std::string &lump, const std::string &entry) {
      events.push_back("res_find:"+text(lump)+':'+text(entry));
      if(mode==1) throw CallbackFault("authored callback exception");
      if(mode==2) variables->memory.word(variables->player_record(0)+0x43d8,0x1234);
      return 0x87654321u;
    };
    if(with_utilities) variables->find_resource = [this](const std::string &lump,const std::string &entry) {
      return variables->scene_service("res_find",{lump,entry});
    };
    variables->local_player_slot = [this]() {
      events.push_back("local");
      if (mode==1) throw CallbackFault("authored callback exception");
      return a::s32(variables->memory.word(0x1000));
    };
    variables->run_reward_script = [this](const std::string &name) {
      events.push_back("script:"+name);
      if (mode==1) throw CallbackFault("authored callback exception");
      if (mode==2) {
        variables->memory.word(0x4d8ae0,1);
        variables->memory.byte(variables->player_record(0)+0x4c0d,99);
      }
    };
  }
  void control_machine() {
    std::vector<a::AsmLine> program;
    auto ins=[&](const std::string &name,std::vector<a::AsmValue> args=std::vector<a::AsmValue>{}) {
      program.push_back(a::AsmInstruction{name,std::move(args),{}, {}});
    };
    auto entry=[&](const std::string &name) {program.push_back(a::Label{name+":"});ins("PROC");};
    auto native=[&](const std::string &name,std::vector<a::Cell> args) {
      for(auto it=args.rbegin();it!=args.rend();++it) ins("PUSH.C",{*it});
      ins("PUSH.C",{a::Cell(4*args.size())});
      ins("SYSREQ.C",{a::Cell(std::find(bindings.begin(),bindings.end(),name)-bindings.begin())});
      ins("STACK",{a::Cell(4*(args.size()+1))});
    };
    entry("leaf");ins("CONST.pri",{a::Cell(123)});ins("RETN");
    entry("initial");ins("RETN");
    entry("error");ins("CONST.pri",{a::Cell(1)});ins("ZERO.alt");ins("SDIV");ins("RETN");
    entry("halt");ins("CONST.pri",{a::Cell(77)});ins("HALT",{a::Cell(9)});
    entry("nested");native("callScript",{2000});ins("RETN");
    entry("missing");native("callScript",{2040});ins("RETN");
    entry("queue");native("addDeferedCall",{2000});native("postAction",{2000});ins("CONST.pri",{a::Cell(42)});ins("RETN");
    entry("profile");native("setpv",{2080,12});ins("RETN");
    entry("reuse");native("getv",{2080});native("callScript",{2120});native("getv",{0x7fffffff});ins("RETN");
    entry("inner");native("getv",{2160});ins("RETN");
    const auto code=a::assemble(program);
    a::ImageOptions options;options.data.resize(4096);options.natives=bindings;options.stack_cells=128;
    for(const std::string name:{"leaf","initial","error","halt","nested","missing","queue","profile","reuse","inner"})
      options.publics.emplace_back(name,a::Cell(code.labels.at(name)));
    machine=std::make_shared<a::Machine>(a::build_image(code.cells,options));
    require(machine->load()==0,"control fixture load");
    for(const auto &pair:std::vector<std::pair<unsigned,std::string>>{{2000,"leaf"},{2040,"absent"},{2080,"Outer"},{2120,"inner"},{2160,"Inner"}})
      machine->set_string(pair.first,pair.second,false);
  }
  std::string snapshot() const {
    std::vector<std::string> memory, writes, slots, registers;
    for (const auto &entry : variables->memory.nonzero_bytes())
      if (entry.second) memory.push_back(hex(entry.first) + ':' + hex(entry.second, 2));
    for (const auto &entry : variables->memory.writes)
      writes.push_back(hex(entry.first) + ':' + hex(entry.second));
    for(const auto &entry:variables->menu_string_slots) slots.push_back(hex(entry.first)+':'+number(entry.second));
    if (variables->string_slot) slots.push_back("00000020:" + number(*variables->string_slot));
    if (variables->quest_string_slot) slots.push_back("00000030:" + number(*variables->quest_string_slot));
    if(variables->dialog_string_slot) slots.push_back("00000080:"+number(*variables->dialog_string_slot));
    if(variables->cutscene_string_slot) slots.push_back("000000d0:"+number(*variables->cutscene_string_slot));
    std::sort(slots.begin(),slots.end());
    const auto &m = *machine;
    for (auto value : {m.cip, m.frm, m.hea, m.stk, m.stp, a::Address(m.flags),
                       m.curline, m.curfile, m.dbgcode, m.dbgaddr, m.dbgparam, m.error, m.params})
      registers.push_back(number(value));
    std::string image;
    for (auto value : m.image) image += hex(value, 2);
    auto state = join(memory) + '|' + join(writes) + '|' + join(slots) + '|' + join(events) +
           '|' + join(registers) + '|' + image;
    if(scene_mode) {
      std::vector<std::string> props;
      for(const auto &entry:variables->object_properties) {
        std::string value=hex(entry.first)+':';
        if(!entry.second) value+="null";
        else {bool first=true;for(const auto &prop:*entry.second) {if(!first) value+='/';first=false;value+=text(prop);}}
        props.push_back(value);
      }
      state+='|'+join(props);
    }
    if(menu_mode) {
      std::vector<std::string> entries;
      const auto &table=variables->object_names;
      for(unsigned i=0;i<512;++i) if(table.keys[i]) entries.push_back(hex(i)+':'+text(*table.keys[i])+':'+(table.objs[i] ? hex(table.objs[i]->addr) : "-"));
      state+='|'+hex(std::uint32_t(table.count))+':'+join(entries);
    }
    if(creature_mode) {
      std::vector<std::string> types;
      for(const auto &entry:variables->named_objects) types.push_back(hex(entry.first)+':'+text(entry.second->type_name));
      state+='|'+join(types);
    }
    if(item_mode) state+='|'+number(variables->draws);
    if(control_mode) state+='|'+number(bool(variables->script_machine))+':'+number(variables->entry_pri);
    return state;
  }
  std::string run(const std::string &op, const std::string &inputs) {
    auto args = split(inputs, '|');
    require(args.size() == 6, "input field count");
    auto name = unhex(args[0]);
    auto value = a::s32(numeric(args[1]));
    require(args[2] == "0" || args[2] == "1", "pack/save flag");
    bool pack = args[2] == "1";
    auto address = a::s32(numeric(args[3]));
    auto count = a::s32(numeric(args[4]));
    mode = static_cast<unsigned>(std::stoul(args[5]));
    require(mode <= (op.rfind("host",0)==0 ? 5u : 3u), "callback mode");
    if(op=="resetcontrol") reset(true,true,true,false,false,false,false,false,false,false,true);
    if(op=="resetutilities") reset(true,true,true,false,false,false,false,false,false,true);
    if(op=="resetinteractions") reset(true,true,true,true,false,false,false,false,true);
    if(op=="resetvehicles") reset(true,true,true,true,false,false,false,true);
    if (op == "reset" || op == "resetcampaign" || op == "resetplayers" || op == "resetscene" || op == "resetmenus" || op == "resetcreatures" || op == "resetitems" || op == "reseteffects") reset(op != "reset",op == "resetplayers" || op == "resetscene" || op == "resetmenus" || op == "resetcreatures" || op == "resetitems" || op == "reseteffects",op == "resetscene" || op == "resetmenus" || op == "resetcreatures" || op == "resetitems" || op == "reseteffects",op == "resetmenus" || op == "resetcreatures" || op == "resetitems" || op == "reseteffects",op == "resetcreatures" || op == "resetitems" || op == "reseteffects",op == "resetitems",op == "reseteffects");
    require(bool(variables), "missing reset");
    variables->memory.writes.clear();
    events.clear();
    std::string result = "-";
    try {
      auto it = std::find(bindings.begin(), bindings.end(), op == "nulcell" ? "setv" : op);
      if (it != bindings.end()) {
        machine->set_string(0, name, pack);
        if (op == "nulcell") {
          machine->set_cell(0, 0x80000000);
          machine->set_cell(4, 0);
        }
        if (op=="addQuest") machine->set_string(800,"Body",pack);
        machine->set_cell(3000, static_cast<std::uint32_t>(count));
        machine->set_cell(3004, static_cast<std::uint32_t>(address));
        machine->set_cell(3008, static_cast<std::uint32_t>(value));
        machine->set_cell(3012, player_mode ? std::uint32_t(count) : 0xfacefeed);
        if(menu_mode && std::find(menus.begin(),menus.end(),op)!=menus.end()) {
          machine->set_string(800,"Body",pack);machine->set_string(1200,"Second",pack);machine->set_string(1600,"Third",pack);
          if(op=="runMedalSuccess") {unsigned i=0;for(unsigned ptr:{0u,800u,1200u,1600u}) machine->set_cell(3012+4*i++,ptr);}
          if(op=="addHelpMessageF") {machine->set_cell(3012,800);machine->set_cell(3016,std::uint32_t(count));}
          if(op=="showBust") machine->set_cell(3016,UINT32_MAX);
        }
        if(creature_mode && std::find(creature_names.begin(),creature_names.end(),op)!=creature_names.end()) {
          machine->set_string(800,"Body",pack);machine->set_string(1200,"Second",pack);
          if(op=="startNPCAnim") {machine->set_cell(3012,1200);machine->set_cell(3016,std::uint32_t(count));}
          if(op=="initScriptSetParams" || op=="setCreatureParams") {
            machine->set_cell(3016,std::uint32_t(count));machine->set_cell(3020,0x80000000);machine->set_cell(3024,0x7fffffff);
          }
        }
        if(item_mode && std::find(item_names.begin(),item_names.end(),op)!=item_names.end()) {
          machine->set_string(800,"Body",pack);machine->set_cell(3016,std::uint32_t(value));
        }
        if(effects_mode && std::find(effects_names.begin(),effects_names.end(),op)!=effects_names.end()) {
          machine->set_string(800,"Body",pack);
          const std::array<std::uint32_t,6> extra={0u,UINT32_MAX,1000u,std::uint32_t(-1000),0x7fffffffu,0x80000000u};
          for(unsigned k=3;k<15;++k) machine->set_cell(3004+4*k,extra[k%6]);
        }
        if(op=="sound" || op=="soundSequence") machine->set_string(800,"Body",pack);
        if(op=="hitPlayers") machine->set_cell(3016,std::uint32_t(value));
        if(vehicle_mode && std::find(vehicle_names.begin(),vehicle_names.end(),op)!=vehicle_names.end()) {
          machine->set_string(800,"Body",pack);machine->set_cell(3016,0x80000000);
          machine->set_cell(3020,0x7fffffff);machine->set_cell(3024,std::uint32_t(value));
        }
        machine->error = 7;
        auto called = machine->call_native(it - bindings.begin(), 3000);
        require(called.first == 0, "dispatcher error");
        result = number(called.second);
      } else if (op == "shortadd" || op == "shortset") {
        machine->set_string(0, name, false);
        auto params = static_cast<a::Address>(machine->image.size()) - machine->data_offset - 8;
        machine->set_cell(params, 0);
        machine->set_cell(params + 4, 0);
        machine->error = 7;
        machine->call_native(op == "shortadd" ? 2 : 1, params);
      } else if(op=="tdiv") {
        auto operands=split(name,'/');
        require(operands.size()==2,"division operands");
        result=integer_text(b::truncating_divide(integer(operands[0]),integer(operands[1])));
      } else if(op=="setupobject") {
        auto props=pack ? std::make_shared<std::vector<std::string>>(std::initializer_list<std::string>{"name=Alias","Name=Ignored"}) : nullptr;
        auto obj=variables->new_object({name},std::uint32_t(value),{std::uint32_t(address),0,0},
            std::uint32_t(count),"10GameObject",props,mode ? std::optional<std::uint32_t>(address) : std::nullopt,std::uint32_t(count));
        result=number(obj->addr);
      } else if(op=="setupalive") {
        variables->memory.w32(0x4d8ae0,value);
        for(a::Cell slot=0;slot<value;++slot)
          variables->memory.w32(variables->player_record(slot)+0x43d8,slot%2==0 ? address : 0);
        variables->rebuild_alive_players();
      } else if(op=="setupvars") {
        std::vector<std::string> entries;
        for(const auto &entry:variables->quest_vars()) entries.push_back(text(entry.first)+":"+number(entry.second));
        result=join(entries);
      } else if(op=="setupext" || op=="setupvirtual") {
        variables->host->handlers[op=="setupvirtual" ? "virtual" : name]=[&](const b::Host::Arguments &) {return std::int64_t(value);};
        auto answer=op=="setupvirtual" ? variables->virtual_call(*variables->obj_at(address),count,{name}) :
            variables->ext(name,{std::int64_t(address),std::int64_t(count)});
        result=host_value(answer);
        for(const auto &call:variables->host->log) events.push_back("forward:"+call.first+":"+host_arguments(call.second));
        variables->host->log.clear();
      } else if (op == "get") result = number(variables->quest_get(name));
      else if (op == "set") variables->quest_set(name, value);
      else if (op == "pget") result = number(variables->profile_get(name));
      else if (op == "pset") variables->profile_set(name, value, pack);
      else if (op == "clear") variables->quest_clear_all();
      else if (op == "forget") variables->forget_string_slot();
      else if (op == "index") {
        auto entry = variables->quest_by_index(value);
        if (entry) result = text(entry->first) + ':' + number(entry->second);
      } else if (op == "unknown") variables->dispatch("unported", *machine);
      else if(op=="argread" || op=="argu" || op=="argf" || op=="argaddress" || op=="args" || op=="bindargs" || op=="missing" || op=="bindtable") {
        machine->set_string(0,"Argument bytes",pack);
        machine->params=3000;machine->set_cell(3000,0);
        machine->set_cell(3004+4*a::Address(count),std::uint32_t(address));
        b::Args adapter(*machine,mode==1 ? nullptr : variables.get(),name);
        if(mode==2) machine->params=2000;
        if(op=="argread") result=number(adapter[count]);
        else if(op=="argu") result=number(adapter.u(count));
        else if(op=="argf") result=number(adapter.f(count));
        else if(op=="argaddress") result=number(adapter.address(count));
        else if(op=="args") result=text(adapter.s(count,value));
        else if(op=="missing") b::Variables::missing(name)(*machine,{123});
        else if(op=="bindtable") {
          auto table=variables->natives();result=number(table.size());table.at(name)(*machine,{123});
        } else {
          auto bound=variables->bind(name,[&](a::Machine &m,b::Args &args) {
            events.push_back("bound:"+number(&m==machine.get())+":"+number(args.base));
            if(mode==3) throw CallbackFault("authored bound failure");
            return a::s32(std::uint32_t(args.address(count)));
          });
          result=number(*bound(*machine,{123}));
        }
      }
      else if(op=="hostdefault" || op=="hostcall" || op=="hostdirect" || op=="hostobject") {
        b::Host::Object first(0x2000),second(0x4000);
        auto delegate=[&](const std::string &label,const b::Host::Arguments &args={}) {
          events.push_back(label+":"+host_arguments(args));
          if(mode==5) throw CallbackFault("authored delegated failure");
        };
        b::Host::Game owner;
        owner.object=[&](std::uint32_t addr)->b::Host::Object * {return addr==first.addr ? &first : addr==second.addr ? &second : nullptr;};
        owner.move_off_grid=[&](auto &obj) {delegate("move",{&obj});};
        owner.update_grid_cell=[&](auto &obj) {delegate("grid",{&obj});};
        owner.run_every_frame=[&](auto &obj) {delegate("run",{&obj});};
        owner.collect_in_box=[&](const auto &lo,const auto &hi,std::int64_t limit) {
          delegate("box",{lo,hi,limit});return b::Host::List{&second,&first,&second};
        };
        owner.for_each_grid_object=[&](const auto &visit) {delegate("walk");visit(second);visit(first);visit(second);};
        owner.find_by_class=[&](const auto &kind,std::int64_t limit) {
          delegate("class",{kind,limit});return b::Host::List{&first,&second};
        };
        owner.on_destroy=[&](auto &obj,std::int64_t flags) {delegate("destroy",{&obj,flags});};
        owner.rebuild_alive_players=[&]() {delegate("alive");};
        b::Host host(&owner);b::Host::Arguments params;
        if(name=="virtual") params={std::int64_t(value),std::int64_t(std::uint32_t(address)),std::int64_t(count)};
        else if(name.rfind("obj_",0)==0) params={std::int64_t(std::uint32_t(address))};
        else if(name=="world_collectObjectsInBox") params={b::Host::Vec3{std::uint32_t(address),0,UINT32_MAX},b::Host::Vec3{0x80000000,std::uint32_t(value),1},std::int64_t(count)};
        else if(name=="world_forEachGridObject") params={std::int64_t(address)};
        else if(name=="world_findObjectsByClass") params={std::string("Authored"),std::int64_t(count)};
        else if(name!="game_rebuildAlivePlayers") params={std::int64_t(address),std::int64_t(value),std::int64_t(count)};
        if(mode==1 || mode==2 || mode==4) host.handlers[name]=[&](const auto &args)->b::Host::Value {
          events.push_back("override:"+host_arguments(args));
          if(mode==2) throw CallbackFault("authored override failure");
          if(mode==4) return host.call(name=="net_firstActiveSlot" ? "res_find" : "net_firstActiveSlot");
          return std::int64_t(-7);
        };
        else if(mode==3) host.handlers[name]={};
        auto append_log=[&]() {for(const auto &call:host.log) events.push_back("log:"+call.first+":"+host_arguments(call.second));};
        try {
          b::Host::Value answer;
          if(op=="hostobject") answer=&host.object(std::uint32_t(address));
          else if(op=="argread" || op=="argu" || op=="argf" || op=="argaddress" || op=="args" || op=="bindargs" || op=="missing" || op=="bindtable") {
        machine->set_string(0,"Argument bytes",pack);
        machine->params=3000;machine->set_cell(3000,0);
        machine->set_cell(3004+4*a::Address(count),std::uint32_t(address));
        b::Args adapter(*machine,mode==1 ? nullptr : variables.get(),name);
        if(mode==2) machine->params=2000;
        if(op=="argread") result=number(adapter[count]);
        else if(op=="argu") result=number(adapter.u(count));
        else if(op=="argf") result=number(adapter.f(count));
        else if(op=="argaddress") result=number(adapter.address(count));
        else if(op=="args") result=text(adapter.s(count,value));
        else if(op=="missing") b::Variables::missing(name)(*machine,{123});
        else if(op=="bindtable") {
          auto table=variables->natives();result=number(table.size());table.at(name)(*machine,{123});
        } else {
          auto bound=variables->bind(name,[&](a::Machine &m,b::Args &args) {
            events.push_back("bound:"+number(&m==machine.get())+":"+number(args.base));
            if(mode==3) throw CallbackFault("authored bound failure");
            return a::s32(std::uint32_t(args.address(count)));
          });
          result=number(*bound(*machine,{123}));
        }
      }
      else if(op=="hostdefault") answer=host.default_answer(name,params);
          else if(op=="hostcall") answer=host.call(name,params);
          else if(name=="virtual") answer=host.do_virtual(value,std::uint32_t(address),{std::int64_t(count)});
          else if(name=="obj_moveOffGrid") host.do_obj_moveOffGrid(std::uint32_t(address));
          else if(name=="obj_updateGridCell") host.do_obj_updateGridCell(std::uint32_t(address));
          else if(name=="obj_runEveryFrame") host.do_obj_runEveryFrame(std::uint32_t(address));
          else if(name=="world_collectObjectsInBox") answer=host.do_world_collectObjectsInBox(std::get<b::Host::Vec3>(params[0]),std::get<b::Host::Vec3>(params[1]),count);
          else if(name=="world_forEachGridObject") answer=host.do_world_forEachGridObject(params[0]);
          else if(name=="world_findObjectsByClass") answer=host.do_world_findObjectsByClass("Authored",count);
          else if(name=="game_rebuildAlivePlayers") host.do_game_rebuildAlivePlayers();
          else throw std::runtime_error("unknown direct host operation");
          result=host_value(answer);
        } catch(...) {append_log();throw;}
        append_log();
      }
      else if(op.rfind("objmem",0)==0) {
        b::Obj obj(mode==2 ? nullptr : &variables->memory,std::uint32_t(address));
        b::Memory::Integer integer{{name.begin(),name.end()},pack};
        if(op=="objmeminit") {if(!name.empty()) obj.type_name=name;result=text(obj.repr());}
        else if(op=="objmemflags") result=number(obj.flags());
        else if(op=="objmemsetflags") obj.flags(integer);
        else if(op=="objmempos") {auto pos=obj.pos();result=number(pos[0])+":"+number(pos[1])+":"+number(pos[2]);}
        else if(op=="objmemsetpos") {
          std::vector<b::Memory::Integer> pos;
          for(int i=0;i<count;++i) {
            auto begin=std::min(name.size(),std::size_t(i)*17),end=std::min(name.size(),std::size_t(i+1)*17);
            pos.push_back({{name.begin()+begin,name.begin()+end},pack});
          }
          obj.pos(pos);
        }
        else if(op=="objmemangle") result=number(obj.angle());
        else if(op=="objmemsetangle") obj.angle(integer);
        else if(op=="objmemu") {auto data=obj.u(std::uint32_t(value),std::size_t(count)).magnitude;result=text(std::string(data.begin(),data.end()));}
        else if(op=="objmemw") obj.w(std::uint32_t(value),std::size_t(count),integer);
        else if(op=="objmemcflags") result=hex(obj.cflags(),16);
        else if(op=="objmemsetcflags") obj.set_cflags(integer);
        else throw std::runtime_error("unknown object accessor");
      }
      else if(op=="descriptor") {
        auto fields=split(name,'|');
        b::X descriptor=pack ? b::X(std::uint32_t(address)) : b::X(std::uint32_t(address),fields.at(0),value,count,fields.at(1),fields.at(2));
        result=number(descriptor.addr)+":"+text(descriptor.ints)+":"+number(descriptor.floats)+":"+number(descriptor.stack)+":"+text(descriptor.ret)+":"+text(descriptor.note);
      }
      else if(op=="membacking") {
        variables->memory=b::Memory([this,value,pack,count,fail=mode==1](std::uint32_t addr,std::size_t size) {
          events.push_back("backing:"+number(addr)+":"+number(size));
          if(fail) throw CallbackFault("authored backing failure");
          std::vector<std::uint8_t> data(pack ? std::min(size,std::size_t(count)) : size,0);
          for(std::size_t i=0;i<std::min(data.size(),std::size_t(8));++i) data[i]=std::uint8_t((addr>>12)+i*17+value);
          return data;
        });
      }
      else if(op=="mempage") variables->memory.page(std::uint32_t(address))[std::uint32_t(value)&4095]=std::uint8_t(count);
      else if(op=="memread") {
        auto data=variables->memory.read(std::uint32_t(address),std::size_t(count));
        result=text(std::string(data.begin(),data.end()));
      }
      else if(op=="memload") {
        auto data=variables->memory.load(std::uint32_t(address),std::size_t(count)).magnitude;
        result=text(std::string(data.begin(),data.end()));
      }
      else if(op=="memstore") variables->memory.store(std::uint32_t(address),std::size_t(count),b::Memory::Integer{{name.begin(),name.end()},pack});
      else if(op=="memwrite") variables->memory.write(std::uint32_t(address),{name.begin(),name.end()});
      else if(op=="memcstr") result=text(variables->memory.cstr(std::uint32_t(address),std::size_t(count)));
      else if(op=="globalget" || op=="globalset") {
        b::Global descriptor(std::uint32_t(address),name,"authored descriptor");
        if(op=="globalget") result=hex(std::uint64_t(descriptor.get(variables->memory)),16);
        else descriptor.set(variables->memory,std::uint64_t(std::int64_t(value)));
      }
      else if(op=="memtyped") {
        auto &mem=variables->memory;auto addr=std::uint32_t(address);
        std::uint64_t v=0;
        if(name=="u8") v=mem.u8(addr);else if(name=="u16") v=mem.u16(addr);
        else if(name=="u32") v=mem.u32(addr);else if(name=="u64") v=mem.u64(addr);
        else if(name=="s8") v=std::uint64_t(std::int64_t(mem.s8(addr)));
        else if(name=="s16") v=std::uint64_t(std::int64_t(mem.s16(addr)));
        else if(name=="s32") v=std::uint64_t(std::int64_t(mem.s32(addr)));
        else if(name=="w8") mem.w8(addr,std::uint64_t(value));
        else if(name=="w16") mem.w16(addr,std::uint64_t(value));
        else if(name=="w32") mem.w32(addr,std::uint64_t(value));
        else if(name=="w64") mem.w64(addr,std::uint64_t(value));
        else throw std::runtime_error("unknown memory accessor");
        if(name[0]!='w') result=hex(v,16);
      }
      else if(op=="controlmachine") {control_machine();variables->set_script(machine);}
      else if(op=="selectscript") variables->set_script(value ? machine : nullptr);
      else if(op=="entrypri") variables->entry_pri=value;
      else if(op=="runscript") result=number(variables->run_script(pack ? std::nullopt : std::optional<std::string>(name)));
      else if(op=="queuedeferred") variables->queue_deferred(name);
      else if(op=="rundeferred") variables->run_deferred();
      else if(op=="cleardeferred") variables->clear_deferred();
      else if(op=="resetstate") variables->reset_script_state();
      else if(op=="setpost") variables->set_post_action(name);
      else if(op=="runpost") variables->run_post_action();
      else if (op=="scriptcell") machine->set_cell(address,std::uint32_t(value));
      else if (op=="poke32") variables->memory.word(std::uint32_t(address),std::uint32_t(value));
      else if (op=="poke8") variables->memory.byte(std::uint32_t(address),std::uint8_t(value));
      else if (op=="pokestr") variables->memory.string(std::uint32_t(address),name);
      else if (op=="complete") variables->mission_set_complete(count,address,value);
      else if (op=="medal") result=number(variables->mission_get_medal(address,value,pack));
      else if (op=="choice") variables->choice_set(0x4c1a,value);
      else if(op=="playerflag") variables->player_set_flag(address,value,count);
      else if(op=="getplayerflag") result=number(variables->player_get_flag(address,value));
      else if(op=="objectprops") variables->object_properties[std::uint32_t(address)]=value ? std::nullopt : std::optional<std::vector<std::string>>(name.empty() ? std::vector<std::string>{} : split(name,'|'));
      else if(op=="objecttype") variables->obj_at(std::uint32_t(address))->type_name=name;
      else if(op=="vehicleof") result=number(variables->vehicle_of(address));
      else if(op=="playerobj") {auto obj=variables->player_object(address);if(obj) result=number(obj->addr);}
      else if(op=="randdraw") result=number(variables->random_draw());
      else if(op=="emptybag") variables->empty_bag(address,std::uint32_t(value));
      else if(op=="objat") {auto obj=variables->obj_at(std::uint32_t(address));if(obj) result=number(obj->addr);}
      else if(op=="nameadd") {
        auto addr=std::uint32_t(address);auto &obj=variables->named_objects[addr];
        if(!obj) {obj=std::make_shared<champions::simulation::rta::objects::GameObject>(addr);variables->object_properties[addr]=std::nullopt;}
        variables->object_names.add(name,obj.get());
      }
      else if(op=="nameremove") variables->object_names.remove(name,variables->named_objects.at(std::uint32_t(address)).get());
      else if(op=="menugate") result=number(variables->medal_gate());
      else if(op=="menuprompt") result=number(variables->medal_prompt());
      else if(op=="menupad") result=number(variables->medal_pad());
      else if(op=="alive") result=number(variables->alive(address));
      else if(op=="nextworld") variables->set_next_world(name);
      else if(op=="netgate") result=number(variables->net_gate());
      else if(op=="dialogstart") variables->dialog_start(name,address,value);
      else if(op=="nogate") {
        machine->set_string(0,name,pack);machine->params=3000;machine->set_cell(3004,std::uint32_t(address));
        result=number(variables->new_world_no_gate(*machine));
      }
      else if(op=="hpcount") result=number(variables->count_players_with_hp());
      else require(op == "resetcontrol" || op == "resetutilities" || op == "resetinteractions" || op == "resetvehicles" || op == "reset" || op == "resetcampaign" || op == "resetplayers" || op == "resetscene" || op == "resetmenus" || op == "resetcreatures" || op == "resetitems" || op == "reseteffects", "unknown fixture operation");
    } catch (const std::domain_error &) { result = "ZeroDivisionError"; }
    catch (const std::out_of_range &) { result = "IndexError"; }
    catch (const b::StackGarbage &) { result = "StackGarbage"; }
    catch (const champions::simulation::rta::objects::ObjectError &) { result = "CallbackFault"; }
    catch (const CallbackFault &) { result = "CallbackFault"; }
    catch (const a::HostFault &) { result = "HostFault"; }
    return result;
  }
};
void script_control_api_tests() {
  Runner r;
  r.reset(true,true,true,false,false,false,false,false,false,false,true);
  auto &v=*r.variables;
  require(v.run_script("leaf")==-1 && v.memory.writes.empty(),"no machine is a nonmutating miss");
  r.control_machine();
  auto original=r.machine;
  // set_script registers unbound natives; repeated registration preserves bindings.
  v.set_script(original);
  require(v.natives().size()==191 && v.run_script("nested")==123,"registered nested native run");
  v.string_slot=17;v.quest_string_slot=23;v.dialog_string_slot=31;v.cutscene_string_slot=47;
  v.menu_string_slots={{0x840,59},{0x10f0,61}};
  auto restored=[&]() {
    return v.string_slot==17 && v.quest_string_slot==23 && v.dialog_string_slot==31 &&
      v.cutscene_string_slot==47 && v.menu_string_slots==std::map<unsigned,a::Cell>{{0x840,59},{0x10f0,61}};
  };
  v.entry_pri=-123;
  require(v.run_script("initial")==-123 && restored(),"entry PRI and every outer string slot");
  require(v.run_script("error")==0 && restored(),"VM error returns zero and restores slots");
  require(v.run_script("halt")==77 && restored(),"HALT result survives nonzero error");
  v.request_config_save=[]() {throw CallbackFault("authored exception");};
  bool threw=false;
  try {v.run_script("profile");} catch(const CallbackFault &) {threw=true;}
  require(threw && restored() && v.profile_get("outer")==12,"exception restores slots after write prefix");
  // A throwing interpreter resource limit must also restore the outer frame.
  original->budget=0;threw=false;
  try {v.run_script("leaf");} catch(const a::Limit &) {threw=true;}
  require(threw && restored(),"execution budget exception restores outer slots");
  original->budget.reset();
  // Current-machine replacement during a native must not invalidate its active run.
  r.control_machine();
  auto replacement=r.machine;
  v.profile_set("outer",0,false);
  v.request_config_save=[&]() {v.set_script(replacement);};
  require(v.run_script("profile")==0 && v.script_machine==replacement && restored(),
          "active machine retained across current-machine replacement");
  require(v.run_script("nested")==123,"replacement registered its native bindings");
  v.set_script(nullptr);
  v.memory.writes.clear();
  require(v.run_script(std::nullopt)==-1 && v.run_script("leaf")==-1 && v.memory.writes.empty(),
          "null entry/current machine do not throttle missing names");
}
void object_api_tests() {
  b::Memory mem;
  mem.w32(0x100c,0xdeadbeef);mem.w16(0x1038,0x9876);
  mem.writes.clear();
  b::Obj obj(mem,0x1000);
  require(mem.writes.empty() && obj.flags()==0xdeadbeef && obj.angle()==0x9876,
          "object construction preserves preexisting fields without writes");
  require(obj.heads.size()==3 && obj.radius==0x41c00000 && obj.height==0x42900000 &&
          !obj.props && !obj.class_def && !obj.class_name && obj.vz==0 && obj.seed==0 &&
          obj.vtable==champions::simulation::rta::objects::VT_GAMEOBJECT && obj.alive && !obj.tag &&
          obj.draw_half_width==25 && obj.draw_height==73,
          "object retains base constructor defaults");
  b::Obj alias(mem,0x1000,"8Creature");
  obj.flags(0x100000001ULL);require(alias.flags()==1,"object aliases read live low32 fields");
  obj.angle(0x123456);require(alias.angle()==0x3456,"angle width");
  obj.pos(std::array<std::uint32_t,3>{0x80000000,0x7fffffff,0xffffffff});
  auto pos=obj.pos();pos[0]=0;
  require(obj.pos()[0]==0x80000000 && alias.pos()==obj.pos(),"position is a fresh read, not a retained alias");
  obj.w(0xa8,8,UINT64_MAX);require(alias.cflags()==UINT64_MAX,"generic write and creature read alias");
  obj.set_cflags(1);require(mem.u64(0x10a8)==1,"creature convenience store");
  obj.w(0,17,b::Memory::Integer{{1},true});
  require(obj.u(0,17).magnitude==std::vector<std::uint8_t>(17,255),"arbitrary width signed field store");
  b::Obj detached(nullptr,0xffffffff,std::string("A\0\xff",3));
  detached.flags(1);detached.angle(2);detached.pos(std::vector<b::Memory::Integer>{});
  require(detached.flags()==0 && detached.angle()==0 && detached.pos()==std::array<std::uint32_t,3>{},
          "detached properties return zero and ignore writes");
  require(detached.repr()==std::string("<Obj 0xffffffff ")+std::string("A\0\xff>",4),"repr preserves all Latin-1 type bytes");
  bool refused=false;try {detached.u(0,0);} catch(const a::HostFault &) {refused=true;}
  require(refused,"detached generic read faults even at zero width");
  refused=false;try {detached.w(0,0,0);} catch(const a::HostFault &) {refused=true;}
  require(refused,"detached generic write faults even at zero width");
  refused=false;try {detached.cflags();} catch(const a::HostFault &) {refused=true;}
  require(refused,"detached creature read faults");
  refused=false;try {detached.set_cflags(0);} catch(const a::HostFault &) {refused=true;}
  require(refused,"detached creature write faults");
  mem.writes.clear();refused=false;
  try {obj.pos(std::vector<b::Memory::Integer>{{{7},false},{{8},false}});} catch(const std::out_of_range &) {refused=true;}
  require(refused && mem.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{0x1010,4},{0x1014,4}} &&
          obj.pos()[0]==7 && obj.pos()[1]==8,"short position retains two stores before indexing failure");
  b::Memory failing([](std::uint32_t addr,std::size_t size) {
    if(addr==4096) throw CallbackFault("second page failure");
    return std::vector<std::uint8_t>(size,0);
  });
  b::Obj crossing(failing,4076);refused=false;
  try {crossing.pos(std::array<std::uint32_t,3>{7,8,9});} catch(const CallbackFault &) {refused=true;}
  require(refused && failing.u32(4092)==7 && failing.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{4092,4},{4096,4}},
          "position backing failure preserves first component and second write log");
  b::X defaults(0);require(defaults.addr==0 && defaults.ints.empty() && defaults.floats==0 &&
    defaults.stack==0 && defaults.ret=="n" && defaults.note.empty(),"call descriptor defaults");
  b::X custom(UINT32_MAX,"isvpxT",8,5,"P","authored metadata");
  require(custom.addr==UINT32_MAX && custom.ints=="isvpxT" && custom.floats==8 && custom.stack==5 &&
    custom.ret=="P" && custom.note=="authored metadata","call descriptor retains metadata");
}
void argument_api_tests() {
  Runner r;r.reset();auto &v=*r.variables;auto &m=*r.machine;
  m.params=3000;m.set_cell(3000,0);m.set_cell(3004,0x80000000);m.set_cell(3008,17);
  b::Args args(m,&v,"getv");m.params=2000;
  require(args.base==3004 && args[0]==INT32_MIN && args.u(0)==0x80000000u && args[1]==17,"captured base and unchecked argument count");
  require(args.f(0)==0xcf000000u,"argument signed float conversion");
  m.set_string(0,"abc",false);m.set_cell(3004,0);
  require(args.s(0,4)=="abc" && v.string_slot==0,"successful string stores slot");
  bool failed=false;try {args.s(0,3);} catch(const a::HostFault &) {failed=true;}
  require(failed && v.string_slot==0,"string limit fails after address store");
  m.set_cell(3004,0x7fffffff);require(args.s(0)=="abc","refused address reuses live slot");
  b::Args detached(m);m.set_cell(2004,0);require(detached.s(0)=="abc","detached valid address");
  m.set_cell(2004,0x7fffffff);failed=false;try {detached.address(0);} catch(const b::StackGarbage &) {failed=true;}
  require(failed,"detached frame never persists slots");
  v.argument_slot(0x840,7);v.argument_slot(0x10f0,9);v.argument_slot(0x20,11);
  auto bound=v.bind("particleMorph",[&](auto &,auto &) {require(!v.string_slot && v.argument_slot(0x840)==7 && v.argument_slot(0x10f0)==9,"binder filters before method");return 123;});
  require(*bound(m,{99})==123,"bound return ignores harness args");
  auto missing=v.missing("authored");failed=false;try {missing(m,{});} catch(const a::HostFault &e) {failed=std::string(e.what())=="built-in authored is not modelled";}
  require(failed && v.argument_slot(0x840)==7,"specified missing helper refuses without mutation");
  m.params=3000;m.set_cell(3004,0);m.set_cell(3008,0);m.set_cell(3012,0x7fffffff);
  b::Args morph(m,&v,"particleMorph");
  require(morph.address(0)==0 && morph.address(1)==0,"particle independent pointer slots");
  failed=false;try {morph.address(2);} catch(const b::StackGarbage &) {failed=true;}
  require(failed,"particle unspecified index has no fallback slot");
  m.set_cell(3004,23);require(args[0]==23,"adapter reads live cell");
  m.params=INT64_MAX;failed=false;try {b::Args overflow(m);} catch(const a::HostFault &) {failed=true;}
  require(failed,"address domain overflow explicitly refused");
  m.params=3000;
  a::NativeMap callbacks;
  callbacks["once"]=v.bind("getv",[&](auto &,auto &a) {callbacks.erase("once");return a[0];});
  require(*callbacks.at("once")(m,{999})==23 && callbacks.empty(),"self-erasing bound method retains active callback");
  auto wide=v.bind("getv",[](auto &,auto &) -> a::Address {return 0x100000001LL;});
  require(*wide(m,{})==0x100000001LL,"binder preserves method return without cell truncation");
  auto none=v.bind("getv",[](auto &,auto &) -> std::optional<a::Address> {return {};});
  require(!none(m,{}),"binder preserves absent method return");
  auto one=v.natives(),two=v.natives();one.erase("getv");require(one.size()==190 && two.size()==191,"fresh complete binding maps");
}
void host_api_tests() {
  using H=b::Host;
  H empty;
  require(!empty.game && empty.log.empty() && empty.handlers.empty(),"Host constructor defaults");
  require(std::get<std::int64_t>(empty.call("res_find",{std::string("A\0B",3)}))==0,"external default does not require game");
  require(std::get<std::string>(empty.log[0].second[0])==std::string("A\0B",3),"log preserves owned byte strings");
  bool failed=false;try {empty.call("unknown");} catch(const std::out_of_range &) {failed=true;}
  require(failed && empty.log.back().first=="unknown","unknown calls preserve log before default lookup failure");
  empty.handlers["unknown"]=[&](const H::Arguments &args)->H::Value {
    require(empty.log.back().first=="unknown","override sees already appended log");
    empty.handlers.erase("unknown");
    for(unsigned i=0;i<64;++i) empty.call("res_find");
    return args.at(0);
  };
  require(std::get<std::string>(empty.call("unknown",{std::string("retained")}))=="retained","self-erasing reentrant handler retains argument lifetime");
  empty.handlers["item_parseDescriptor"]=[](const H::Arguments &args)->H::Value {
    const auto &item=std::get<H::ItemBlank>(args.at(0));
    require(item.bytes[15]==0 && item.bytes[16]==255 && item.bytes[17]==255 &&
      item.words==std::array<std::uint16_t,4>{65535,65535,65535,65535},"item blank call-out argument");
    return args.at(1);
  };
  b::Memory::Integer large{{1,2,3,4,5,6,7,8,9,10},true};
  auto echoed=std::get<b::Memory::Integer>(empty.call("item_parseDescriptor",{H::ItemBlank{},large}));
  require(echoed.magnitude==large.magnitude && echoed.negative,"opaque arbitrary-width integers preserve callback and log values");
  empty.handlers["unknown"]={};failed=false;
  try {empty.call("unknown");} catch(const std::out_of_range &) {failed=true;}
  require(failed,"null override falls back");
  failed=false;try {empty.default_answer("virtual",{std::int64_t(99)});} catch(const std::out_of_range &) {failed=true;}
  require(failed,"unknown virtual slot does not invent behavior");
  require(std::get<H::List>(empty.default_answer("world_forEachGridObject",{})).empty(),"list default distinct from no result");
  require(std::get<std::pair<std::int64_t,std::int64_t>>(empty.default_answer("inventory_findItemBySubstring",{}))==std::make_pair(std::int64_t(-1),std::int64_t(0)),"missing inventory pair exact");
  require(std::get<H::Seat>(empty.do_virtual(19,0)).position==H::Vec3{},"non-destruction virtuals need no game");
  failed=false;try {empty.do_obj_runEveryFrame(0);} catch(const a::HostFault &) {failed=true;}
  require(failed,"delegation absence is explicit");
  failed=false;try {empty.call("obj_runEveryFrame",{std::int64_t(1),std::int64_t(2)});} catch(const a::HostFault &) {failed=true;}
  require(failed && empty.log.back().second.size()==2,"fixed-arity default methods reject after logging");
  empty.handlers["obj_runEveryFrame"]=[](const auto &args)->H::Value {return std::int64_t(args.size());};
  require(std::get<std::int64_t>(empty.call("obj_runEveryFrame",{}))==0,"override resolves before default arity checks");

  H::Game game;H host(&game);H::Object actor(0x2000);
  unsigned lookup=0,destruction=0;
  game.object=[&](std::uint32_t addr)->H::Object * {++lookup;return addr==actor.addr ? &actor : nullptr;};
  require(&host.object(actor.addr)==&actor && host.game==&game,"supplied game and registry identity");
  failed=false;try {host.object(0);} catch(const std::out_of_range &) {failed=true;}
  require(failed,"missing registry identity preserves lookup failure");
  game.on_destroy=[&](auto &obj,std::int64_t flags) {require(&obj==&actor && flags==-1,"destruction forwards identity and full integer");++destruction;};
  require(std::get<std::int64_t>(host.do_virtual(1,actor.addr,{std::int64_t(-1)}))==0 && destruction==1,"destroy delegates then returns unused zero");
  failed=false;try {host.do_virtual(1,0,{});} catch(const std::out_of_range &) {failed=true;}
  require(failed && destruction==1,"missing object fails before missing destroy argument");
  game.object=[&](auto)->H::Object * {game.object={};return &actor;};
  require(&host.object(actor.addr)==&actor && !game.object,"self-replacing registry delegate retains callable lifetime");
  game.rebuild_alive_players=[&]() {game.rebuild_alive_players={};};
  host.do_game_rebuildAlivePlayers();require(!game.rebuild_alive_players,"self-clearing state delegate retains callable lifetime");
  auto &aliased_name=empty.log.front().first;
  require(std::get<std::int64_t>(empty.call(aliased_name))==0,"call name can alias a reallocating log");
  // Live ref_objects ownership can be supplied without changing either module.
  champions::simulation::rta::objects::World world;
  auto obj=world.new_object(0,{});require(obj,"synthetic World allocation");
  game.object=[&](auto addr) {return world.objects.at(addr);};
  game.move_off_grid=[&](auto &o) {world.move_off_grid(o);};
  game.update_grid_cell=[&](auto &o) {world.update_grid_cell(o);};
  game.run_every_frame=[&](auto &o) {world.run_every_frame(o);};
  game.for_each_grid_object=[&](const auto &visit) {world.for_each_grid_object(visit);};
  host.do_obj_moveOffGrid(obj->addr);require(world.is_off_grid(*obj),"actual World off-grid delegation");
  host.do_obj_updateGridCell(obj->addr);require(!world.is_off_grid(*obj),"actual World grid delegation");
  host.do_obj_runEveryFrame(obj->addr);
  auto visits=host.do_world_forEachGridObject(std::int64_t(0));
  require(std::find(visits.begin(),visits.end(),obj)!=visits.end(),"actual World walk returns object identity");
}
void setup_api_tests() {
  auto mem=std::make_shared<b::Memory>();
  auto world=std::make_shared<champions::simulation::rta::objects::World>();
  auto host=std::make_shared<b::Host>();
  b::Variables game(mem,world,host);
  require(&game.memory==mem.get() && game.world==world && game.host==host && host->game,
          "setup supplied identities");
  require(!game.script_machine && game.entry_pri==0 && game.draws==0 && game.quest_vars().empty(),"setup defaults");
  auto props=std::make_shared<std::vector<std::string>>(std::initializer_list<std::string>{"name=Alias","Name=ignored"});
  auto obj=game.new_object({"Quoted"},0,{0,0,0},65537,"8Creature",props);
  props->push_back("name=late");
  require(obj->props->size()==2 && game.find_first("Alias")==obj.get() && !game.find_first("late"),"setup property copy/names");
  require(obj->angle()==1 && obj->addr==0x1000000 && obj->type_name=="8Creature" && game.draws==0,"setup fields/no random draw");
  require(champions::simulation::rta::HandlePool::items(world->home_cell(*obj,true))==std::vector<champions::simulation::rta::Object *>{obj.get()},"setup grid link");
  require(world->collect_in_box({0xc1200000u,0xc1200000u,0xc1200000u},
                                {0x41200000u,0x41200000u,0x41200000u},8).size()==1,
          "setup object is visible to the world's spatial query");
  auto next=game.new_object({},2,{},0,"10GameObject",{}, {},1);
  require(next->addr==0x1001000 && champions::simulation::rta::HandlePool::items(world->off_grid)==std::vector<champions::simulation::rta::Object *>{next.get()},"setup aligned allocation/off grid");
  mem->w32(0x4d8ae0,3);mem->w32(game.player_record(0)+0x43d8,obj->addr);
  mem->w32(game.player_record(2)+0x43d8,next->addr);mem->w32(0x4f8448,99);
  mem->writes.clear();game.rebuild_alive_players();
  require(mem->s32(0x4f8420)==2 && game.alive(0)==0 && game.alive(1)==2 && game.alive(2)==99,"setup alive ordered/no tail clear");
  obj->flags(8);host->call("game_rebuildAlivePlayers");
  require(mem->s32(0x4f8420)==1 && game.alive(0)==2,"setup alive live flags/host owner");
  mem->w32(game.player_record(1)+0x43d8,0xdeadbeef);mem->writes.clear();
  bool refused=false;try {game.rebuild_alive_players();} catch(const a::HostFault &) {refused=true;}
  require(refused && mem->writes.empty(),"setup stale pointer before stores");
  auto plain=std::make_unique<champions::simulation::rta::objects::GameObject>(0x4000000);
  plain->flags=8;world->objects[plain->addr]=plain.get();
  mem->w32(0x4d8ae0,1);mem->w32(game.player_record(0)+0x43d8,plain->addr);
  game.rebuild_alive_players();require(mem->s32(0x4f8420)==0,"setup supplied plain object flags");
  plain->flags=0;game.rebuild_alive_players();require(mem->s32(0x4f8420)==1,"setup supplied live base flags");
  auto overlap=game.new_object({},0,{},0,"10GameObject",{},obj->addr,-1);
  require(game.obj_at(obj->addr)==overlap.get() && game.find_first("Alias")==obj.get(),"setup duplicate address retains old name identity");
  auto aligned=game.new_object({},0,{},0,"10GameObject",{}, {},-1);
  require(game.new_object()->addr==aligned->addr,"setup negative size alignment");
  b::Obj borrowed(*mem,0x5000000);borrowed.flags(8);world->objects[borrowed.addr]=&borrowed;
  mem->w32(game.player_record(0)+0x43d8,borrowed.addr);game.rebuild_alive_players();
  require(mem->s32(0x4f8420)==0,"setup supplied memory-backed flags");
  b::Host::Game existing;auto supplied=std::make_shared<b::Host>(&existing);
  b::Variables other({}, {}, supplied);require(supplied->game==&existing,"setup existing host owner preserved");
  host->handlers["virtual"]=[&](const b::Host::Arguments &args) {
    require(std::get<std::int64_t>(args[0])==14 && std::get<std::int64_t>(args[1])==obj->addr && std::get<std::string>(args[2])=="tail","setup virtual forwarding");
    return std::string("result");
  };
  require(std::get<std::string>(game.virtual_call(*obj,14,{std::string("tail")}))=="result","setup unchanged return");
  host->handlers["reenter"]=[&](const b::Host::Arguments &) {game.quest_set("inside",7);return game.ext("dialog_isOpen");};
  require(std::get<std::int64_t>(game.ext("reenter"))==0 && game.quest_vars()[0].second==7,"setup forwarding reentry");
}
void external_world_lifetime_test() {
  auto world = std::make_shared<champions::simulation::rta::objects::World>();
  std::uint32_t address = 0;
  {
    b::Variables binder({}, world);
    auto object = binder.new_object({"Temporary"}, 0, {0, 0, 0}, 0);
    address = object->addr;
    require(world->names.find_first("Temporary") == object.get() &&
            world->objects.at(address) == object.get(), "external registration is live");
  }
  require(!world->names.find_first("Temporary") && !world->objects.count(address),
          "binder teardown removes external names and addresses");
  require(world->collect_in_box({0xc1200000u,0xc1200000u,0xc1200000u},
                                {0x41200000u,0x41200000u,0x41200000u},8).empty(),
          "surviving World does not walk destroyed binder objects");
  // HandlePool::clear rejects paired nodes in one head. External-owner
  // teardown must detach them and clear the surviving World's registries.
  {
    auto malformed_world = std::make_shared<champions::simulation::rta::objects::World>();
    std::shared_ptr<b::Obj> retained;
    {
      b::Variables binder({}, malformed_world);
      retained = binder.new_object({"Malformed"}, 0, {0, 0, 0}, 0);
      malformed_world->handles.link_both(retained->heads[0], retained.get(),
                                         retained->heads[0], retained.get());
    }
    require(retained != nullptr && !malformed_world->names.find_first("Malformed") &&
            !malformed_world->objects.count(retained->addr),
            "binder teardown removed malformed external links and registries");
    malformed_world.reset();
  }
  {
    auto iter_world = std::make_shared<champions::simulation::rta::objects::World>();
    std::shared_ptr<b::Obj> retained;
    std::unique_ptr<champions::simulation::rta::Iterator> iter;
    {
      b::Variables binder({}, iter_world);
      retained = binder.new_object({"Iterated"}, 0, {0, 0, 0}, 0);
      iter_world->handles.add(retained->heads[0], retained.get());
      iter = std::make_unique<champions::simulation::rta::Iterator>(retained->heads[0]);
    }
    require(iter->done() && !iter_world->names.find_first("Iterated") &&
            !iter_world->objects.count(retained->addr),
            "binder teardown invalidated a live iterator and removed its object");
  }
}
void api_tests() {
  for(int dividend=-128; dividend<=128; ++dividend) {
    for(int divisor=-17; divisor<=17; ++divisor) {
      b::Memory::Integer a{{std::uint8_t(std::abs(dividend)),0},dividend<0};
      b::Memory::Integer d{{std::uint8_t(std::abs(divisor)),0},divisor<0};
      const auto saved_a=a.magnitude, saved_d=d.magnitude;
      if(divisor==0) {
        bool threw=false;
        try {b::truncating_divide(a,d);} catch(const std::domain_error &) {threw=true;}
        require(threw,"division zero exception");
      } else {
        auto q=b::truncating_divide(a,d);
        const int expected=dividend/divisor;
        require(q.magnitude==(expected ? std::vector<std::uint8_t>{std::uint8_t(std::abs(expected))} : std::vector<std::uint8_t>{}) &&
                q.negative==(expected<0),"division exhaustive small signed quotients");
      }
      require(a.magnitude==saved_a && d.magnitude==saved_d,"division does not mutate operands");
    }
  }
  b::Memory::Integer power{std::vector<std::uint8_t>(513,0),true};
  power.magnitude.back()=1;
  auto q=b::truncating_divide(power,{{2},false});
  require(q.negative && q.magnitude.size()==512 && q.magnitude.back()==128 &&
          std::count(q.magnitude.begin(),q.magnitude.end(),0)==511,"division beyond fixed integer widths");
  require(integer_text(b::truncating_divide(power,power))=="0:01","division equal wide operands");
  require(integer_text(b::truncating_divide({{0,0},true},{{1},true}))=="0:-","division canonical signed zero");
  bool zero_threw=false;
  try {b::truncating_divide(power,{{},true});} catch(const std::domain_error &) {zero_threw=true;}
  require(zero_threw,"division empty signed zero divisor");
  setup_api_tests();
  external_world_lifetime_test();
  host_api_tests();
  argument_api_tests();
  object_api_tests();
  b::Memory mem;
  require(mem.read(0,0).empty() && mem.writes.empty(),"empty memory reads do not write");
  auto &page=mem.page(1);page[0]=7;
  require(&page==&mem.page(1) && mem.u8(4096)==7,"page identity and mutable alias");
  require(mem.nonzero_bytes().at(4096)==7,"snapshot mutable page");
  page[0]=0;require(mem.nonzero_bytes().empty(),"retained page alias updates snapshot");
  mem.store(0xffffffff,17,b::Memory::Integer{{1},true});
  require(mem.read(0xffffffff,17)==std::vector<std::uint8_t>(17,255),"arbitrary negative extension and address wrap");
  b::Global descriptor(4095,"s16","description");
  require(&descriptor.descriptor()==&descriptor && descriptor.size==2 && descriptor.documentation=="description","global descriptor metadata");
  descriptor.set(mem,b::Memory::Integer{{1},true});
  require(descriptor.get(mem)==-1 && mem.writes.back()==std::make_pair(4095u,std::size_t(2)),"global low-width negative store");
  unsigned backing_calls=0;
  b::Memory backed([&](std::uint32_t addr,std::size_t size) {
    ++backing_calls;require(addr==0 && size==4096,"backing page request");
    return std::vector<std::uint8_t>(size,42);
  });
  backed.w8(1,0);require(backed.u8(2)==42 && backing_calls==1 && backed.writes.size()==1,"copy on write caches complete backing page");
  backed.under={};require(backed.u8(2)==42 && backed.u8(4096)==0,"backing replacement leaves cached pages intact");
  b::Memory failed([](std::uint32_t,std::size_t)->std::vector<std::uint8_t> {throw CallbackFault("backing failure");});
  bool failed_store=false;try {failed.w32(4095,123);} catch(const CallbackFault &) {failed_store=true;}
  require(failed_store && failed.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{4095,4}},"backing failure keeps whole write log prefix");

  script_control_api_tests();
  b::Variables variables;
  bool refused = false;
  try { variables.profile_set("new", 12, true); }
  catch (const a::HostFault &) { refused = true; }
  require(refused && variables.profile_get("NEW") == 12 &&
          variables.memory.writes.size() == 2, "missing callback preserves write prefix");
  variables.profile_set("NEW", 12, true); // equal value does not require callback
  unsigned calls = 0;
  variables.request_config_save = [&]() { ++calls; variables.profile_set("Other", 7, false); };
  variables.profile_set("NEW", 13, true);
  require(calls == 1 && variables.profile_get("other") == 7, "reentrant save callback");
  variables.quest_set(std::string("a\0b", 3), 9);
  require(variables.memory.read(b::QUEST_VARS) == 'a' &&
          variables.memory.read(b::QUEST_VARS + 1) == 0 &&
          variables.memory.read(b::QUEST_VARS + 2) == 'b' &&
          variables.memory.read(b::QUEST_VARS + 3) == 0 &&
          variables.memory.read(b::QUEST_VARS + 4) == 9,
          "embedded NUL retains pinned raw-name write semantics");
  Runner runner;
  runner.reset();
  runner.machine->params = static_cast<a::Address>(runner.machine->image.size()) -
                           runner.machine->data_offset - 8;
  runner.machine->set_cell(runner.machine->params + 4, 0);
  runner.machine->set_string(0, "Prefix", false);
  refused = false;
  try { runner.variables->dispatch("addv", *runner.machine); }
  catch (const a::HostFault &) { refused = true; }
  require(refused && runner.variables->quest_by_index(0)->first == "prefix",
          "addv get inserts before invalid delta read");
  runner.machine->params = std::numeric_limits<a::Address>::max();
  refused = false;
  try { runner.variables->dispatch("getv", *runner.machine); }
  catch (const a::HostFault &) { refused = true; }
  require(refused, "host parameter arithmetic overflow refusal");
  // Real AMX bytecode invocation: no direct native runner shortcuts.
  a::ImageOptions options;
  options.data.resize(128);
  options.natives = {"setv", "addv"};
  options.publics = {{"start", 0}};
  auto assembled = a::assemble({a::AsmInstruction{"PROC", {}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(127)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(0)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(8)}, {}, {}},
      a::AsmInstruction{"SYSREQ.C", {a::Cell(0)}, {}, {}},
      a::AsmInstruction{"STACK", {a::Cell(12)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(2147483647)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(0)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(8)}, {}, {}},
      a::AsmInstruction{"SYSREQ.C", {a::Cell(1)}, {}, {}},
      a::AsmInstruction{"STACK", {a::Cell(12)}, {}, {}},
      a::AsmInstruction{"RETN", {}, {}, {}}});
  a::Machine machine(a::build_image(assembled.cells, options), variables.natives());
  require(machine.load() == 0, "bytecode fixture load");
  machine.set_string(0, "Bytecode", false);
  auto result = machine.execute(0);
  require(result.first == 0 && result.second == -2147483522 &&
          variables.quest_get("bytecode") == -128, "SYSREQ.C signed32 sum and clamp");
  require(variables.natives().size() == 191, "subset binding count");
  b::Variables campaign_state;
  campaign_state.memory.word(0x4d8ae0,2);
  refused=false;
  try { campaign_state.mission_grant_medal(0,3); }
  catch (const a::HostFault &) { refused=true; }
  require(refused && campaign_state.memory.writes.back().first==0x4f7149 &&
          campaign_state.memory.read(campaign_state.player_record(0)+0x4c0d)==0,
          "missing reward callback retains reward-player write and refuses medal store");
  refused=false;
  try { campaign_state.mission_get_medal(13,-1,0); }
  catch (const a::HostFault &) { refused=true; }
  require(refused,"local-player callback required even for rejected mission");
  campaign_state.memory.byte(0x402791,1);
  require(campaign_state.mission_get_medal(0,0,1)==1,"network flag bypasses local callback");
  unsigned reward_calls=0;
  campaign_state.run_reward_script=[&](const std::string &) {
    ++reward_calls;
    campaign_state.memory.word(0x4d8ae0,1);
    campaign_state.run_reward_script=[](const std::string &) {};
    campaign_state.quest_set("Reentry",7);
  };
  campaign_state.mission_grant_medal(0,3);
  require(reward_calls==1 && campaign_state.quest_get("reentry")==7 &&
          campaign_state.memory.read(campaign_state.player_record(0)+0x4c0d)==3 &&
          campaign_state.memory.read(campaign_state.player_record(1)+0x4c0d)==0,
          "reward reentry, callback replacement and live player-count termination");
  Runner player;
  player.reset(true,true);
  player.variables->memory.word(0x4d8ae0,2);
  player.machine->params=3000;
  player.machine->set_cell(3004,7);
  player.variables->player_service={};
  refused=false;
  try {player.variables->dispatch("givePlayerExp",*player.machine);} catch(const a::HostFault &) {refused=true;}
  require(refused,"experience service must be supplied");
  player.variables->find_resource={};
  refused=false;
  try {player.variables->dispatch("healPlayer",*player.machine);} catch(const a::HostFault &) {refused=true;}
  require(refused,"heal resource lookup must be supplied");
  player.variables->find_resource=[](const std::string &,const std::string &) {return 17u;};
  player.variables->memory.word(player.variables->player_record(0)+0xac,0x3f800000);
  player.variables->memory.word(player.variables->player_record(0)+0xb0,0x41200000);
  refused=false;
  try {player.variables->dispatch("healPlayer",*player.machine);} catch(const a::HostFault &) {refused=true;}
  require(refused && player.variables->memory.word(player.variables->player_record(0)+0xac)==0x3f800000,
          "missing sound callback refuses before health refill");
  unsigned experience_calls=0;
  player.variables->player_service=[&](const std::string &,const std::vector<std::uint32_t> &args) {
    require(args==std::vector<std::uint32_t>({0,7}),"experience callback arguments");
    ++experience_calls;
    player.variables->quest_set("PlayerReentry",9);
    player.variables->memory.word(0x4d8ae0,1);
    player.variables->player_service={};
    return 99u;
  };
  player.variables->dispatch("givePlayerExp",*player.machine);
  require(experience_calls==1 && player.variables->quest_get("PlayerReentry")==9,
          "service reentry and replacement preserve active callable and live loop count");
  // New numeric handlers invalidate both pointer slots before an argument fault.
  player.variables->string_slot=0;player.variables->quest_string_slot=800;
  player.machine->params=std::numeric_limits<a::Address>::max();
  refused=false;
  try {player.variables->dispatch("setPlayerFlag",*player.machine);} catch(const a::HostFault &) {refused=true;}
  require(refused && !player.variables->string_slot && !player.variables->quest_string_slot,
          "numeric handler clears slots before invalid params");
  Runner scene_runner;
  scene_runner.reset(true,true,true);
  auto &v=*scene_runner.variables;
  auto &m=*scene_runner.machine;
  m.params=3000;m.set_cell(3004,0);m.set_string(0,"Zone",false);
  v.scene_service={};
  refused=false;
  try {v.dispatch("newWorld",m);} catch(const a::HostFault &) {refused=true;}
  require(refused && v.memory.cstr(0x41b6d0).empty(),"missing scene service refuses before level store");
  v.scene_service=[](const std::string &,const std::vector<b::Variables::ServiceValue> &) {return 0u;};
  v.memory.word(0x4039f8,0x2000);
  v.object_properties[0x2000]=std::vector<std::string>{"level=Initial","orientation=2"};
  v.scene_service=[&](const std::string &,const std::vector<b::Variables::ServiceValue> &) {
    v.object_properties[0x2000]=std::vector<std::string>{"orientation=3"};
    v.scene_service={};return 0u;
  };
  v.dispatch("newWorldParameter",m);
  require(v.memory.cstr(0x41b6d0)=="Initial" && v.memory.read(0x41b720)==3,
          "transition callback replacement and live selected properties");
  v.scene_service=[](const std::string &,const std::vector<b::Variables::ServiceValue> &) {return 0u;};
  v.memory.byte(0x402791,0);v.memory.word(0x4f719c,4);v.request_config_save={};
  refused=false;
  try {v.dispatch("saveEndConfig",m);} catch(const a::HostFault &) {refused=true;}
  require(refused && v.memory.word(0x4f8f38)==64 && v.memory.word(0x41b4f4)==1,
          "missing config save preserves campaign end writes");
  v.dialog_string_slot=0;v.cutscene_string_slot=0;v.string_slot=0;
  m.params=std::numeric_limits<a::Address>::max();
  refused=false;
  try {v.dispatch("startDialog",m);} catch(const a::HostFault &) {refused=true;}
  require(refused && v.dialog_string_slot && !v.cutscene_string_slot && !v.string_slot,
          "dialog clears other slots before argument failure");
  Runner menu;
  menu.reset(true,true,true,true);
  auto &mv=*menu.variables;auto &mm=*menu.machine;
  mm.params=3000;mm.set_cell(3004,20);mm.set_cell(3008,800);mm.set_string(0,"Alpha",false);mm.set_string(800,"Body",false);
  mv.scene_service={};
  refused=false;
  try {mv.dispatch("activateStore",mm);} catch(const a::HostFault &) {refused=true;}
  require(refused && mv.memory.read(0x4f7150)==20 && mv.memory.writes.back().second==2,
          "store keeps index write before missing dialog service");
  mv.memory.writes.clear();
  refused=false;
  try {mv.medal_gate();} catch(const a::HostFault &) {refused=true;}
  require(refused && mv.memory.word(0x4f7114)==2 && mv.memory.word(0x41b4f4)==1,
          "offline medal gate retains tick/pause prefix before missing card service");
  mv.memory.byte(0x402791,1);mv.local_player_slot={};
  refused=false;
  try {mv.medal_gate();} catch(const a::HostFault &) {refused=true;}
  require(refused,"online medal gate requires local-slot callback");
  auto obj=std::make_shared<champions::simulation::rta::objects::GameObject>(0x2000);
  mv.named_objects[obj->addr]=obj;mv.object_names.add("Alpha",obj.get());
  require(mv.find_first("alpha")==obj.get() && mv.find("ALPHA",2).size()==1 &&
          mv.need(obj.get(),"test")==obj.get(),"name/need host helpers");
  refused=false;try {mv.need(nullptr,"test");} catch(const a::HostFault &) {refused=true;}
  require(refused,"null object needs explicit refusal");
  bool bad_limit=false;try {mv.find("Alpha",1);} catch(const champions::simulation::rta::objects::ObjectError &) {bad_limit=true;}
  require(bad_limit,"name query retains reference limit refusal");
  unsigned menu_calls=0;
  mv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    require(service=="text_lookup","compass first callback");++menu_calls;
    mv.memory.word(0x2010,0x7fffffff);
    mv.scene_service=[&](const std::string &next,const std::vector<b::Variables::ServiceValue> &args) {
      require(next=="hud_setCompassTarget" && std::get<std::array<std::uint32_t,3>>(args[0])[0]==0x7fffffff,
              "compass reads position after lookup callback");++menu_calls;return 0u;
    };
    mv.quest_set("MenuReentry",11);return 7u;
  };
  mm.set_cell(3004,0);mv.dispatch("showCompass",mm);
  require(menu_calls==2 && mv.quest_get("menureentry")==11,"presentation callback replacement and reentry");
  mv.menu_string_slots[0x70]=0;mv.menu_string_slots[0xc0]=0;mv.string_slot=0;
  mm.params=std::numeric_limits<a::Address>::max();
  mv.memory.byte(0x402791,0);mv.scene_service=[](const std::string &,const std::vector<b::Variables::ServiceValue> &) {return 0u;};
  refused=false;try {mv.dispatch("runMedalFail",mm);} catch(const a::HostFault &) {refused=true;}
  require(refused && mv.menu_string_slots.size()==1 && mv.menu_string_slots.count(0xc0) && !mv.string_slot,
          "medal string handler invalidates unrelated slots before argument failure");
  Runner creature;
  creature.reset(true,true,true,true,true);
  auto &cv=*creature.variables;auto &cm=*creature.machine;
  auto actor=std::make_shared<champions::simulation::rta::objects::GameObject>(0x2000);
  auto target=std::make_shared<champions::simulation::rta::objects::GameObject>(0x4000);
  cv.named_objects[actor->addr]=actor;cv.named_objects[target->addr]=target;
  cv.object_names.add("Alpha",actor.get());cv.object_names.add("Body",target.get());
  cv.memory.word(0x200c,0x20);cv.memory.word(0x4039f8,0x2000);
  cm.params=3000;cm.set_cell(3004,0);cm.set_cell(3008,0);cm.set_cell(3012,0);
  cm.set_string(0,"Alpha",false);cm.set_string(800,"Body",false);
  cv.scene_service={};cv.memory.writes.clear();
  refused=false;try {cv.dispatch("rootMonster",cm);} catch(const a::HostFault &) {refused=true;}
  require(refused && cv.memory.read(0x2314)==255 && cv.memory.writes.back()==std::make_pair(0x2314u,std::size_t(2)),
          "root stores before missing AI virtual callback");
  cv.memory.word(0x20a8,0x80000001);cv.memory.word(0x20ac,0xdeadbeef);
  cv.memory.writes.clear();
  refused=false;try {cv.apply_cflag(actor.get(),0,false);} catch(const a::HostFault &) {refused=true;}
  require(refused && cv.memory.word(0x20a8)==1 && cv.memory.word(0x20ac)==0xdeadbeef &&
          cv.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>({{0x41b6a0,4},{0x41b6a4,4},{0x20a8,8}}),
          "boss clear preserves exact write prefix before required music callback");
  unsigned creature_calls=0;
  cv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="creature_startBossMusic" && std::get<std::uint32_t>(args[0])==0x2000,"boss callback identity");
    ++creature_calls;cv.memory.word(0x20a8,0x40000001);cv.memory.word(0x20ac,0x12345678);
    cv.scene_service={};cv.quest_set("CreatureReentry",17);return 0u;
  };
  cv.apply_cflag(actor.get(),0,false);
  require(creature_calls==1 && cv.memory.word(0x20a8)==0x40000000 && cv.memory.word(0x20ac)==0x12345678 &&
          cv.quest_get("creaturereentry")==17,"boss final bit write reads live callback-mutated flags");
  cv.object_query={};cv.memory.writes.clear();
  refused=false;try {cv.dispatch("checkMonstersLeft",cm);} catch(const a::HostFault &) {refused=true;}
  require(refused && cv.memory.read(0x5d32c4)==1 && cv.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>({{0x5d32c4,1}}),
          "grid callback refusal retains scratch-byte prefix");
  cv.object_query=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="world_forEachGridObject" && args.size()==1 && std::get<std::uint32_t>(args[0])==0x144198,
            "grid callback arguments");
    cv.object_query={};return std::vector<champions::simulation::rta::objects::GameObject *>{actor.get(),target.get()};
  };
  require(cv.dispatch("checkMonstersLeft",cm)==1,"grid enumeration reads owned memory flags");
  cv.memory.word(0x20a8,0x08000000);cv.memory.word(0x2232,0x14);
  cv.scene_service={};cv.stop_path(actor.get());
  require(cv.memory.word(0x20a8)==0 && cv.memory.read(0x23c4)==0x7e && cv.memory.read(0x23c5)==4,
          "dead path stop skips virtual and resets radius");
  cv.memory.word(0x20a8,0x08000000);cv.memory.word(0x2232,0);cv.memory.word(0x2010,0x80000000);
  cv.memory.word(0x2014,0x7fffffff);cv.memory.word(0x2018,0x12345678);
  refused=false;try {cv.stop_path(actor.get());} catch(const a::HostFault &) {refused=true;}
  require(refused && cv.memory.word(0x23b4)==0x80000000 && cv.memory.word(0x23b8)==0x7fffffff && cv.memory.word(0x23bc)==0x12345678,
          "live path stop stores complete home before missing AI callback");
  cm.set_cell(3008,800);cv.memory.word(0x4010,1);
  cv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    require(service=="obj_updateGridCell","move grid service");cv.memory.word(0x4010,0x12345678);return 0u;
  };
  cv.object_names.add("Alpha",target.get());
  cv.dispatch("moveMonster",cm);
  require(cv.memory.word(0x2010)==1 && cv.memory.word(0x4010)==0x12345678,"move copies current target per iteration");
  cv.find_resource={};cv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    require(service=="anim_clear","NPC end restart clears before resource lookup");++creature_calls;return 0u;
  };
  cm.set_cell(3012,1200);cm.set_cell(3016,UINT32_MAX);cm.set_string(1200,"Clip",false);
  refused=false;try {cv.dispatch("startNPCAnim",cm);} catch(const a::HostFault &) {refused=true;}
  require(refused && creature_calls==2,"missing NPC resource callback occurs after animation clear");
  cv.scene_service={};
  for(auto entry:std::vector<std::pair<a::Cell,unsigned>>{{1,0x3ac},{2,0x3ae},{3,0x3b0},{5,0x3c4}}) {
    cv.memory.writes.clear();cv.apply_monster_val(actor.get(),entry.first,-65537);
    require(cv.memory.read(actor->addr+entry.second)==255 && cv.memory.read(actor->addr+entry.second+1)==255 &&
            cv.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{actor->addr+entry.second,2}},
            "all monster-value field selectors truncate and store two bytes");
  }
  cv.memory.writes.clear();cv.apply_monster_val(actor.get(),-1,17);cv.apply_cflag(actor.get(),44,true);cv.apply_oflag(actor.get(),13,true);
  require(cv.memory.writes.empty(),"unsupported flag and value selectors preserve all fields");
  cv.memory.word(0x211a,0xffff0080);cm.set_cell(3004,0);
  require(cv.dispatch("NPCAnimAtEnd",cm)==1,"NPC end status tests bit seven of low halfword");
  actor->type_name=target->type_name="14MonsterTrigger";
  unsigned trigger_calls=0;
  cv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    require(service=="MonsterTrigger_count","trigger count service");++trigger_calls;return 0x80000001u;
  };
  require(cv.dispatch("monstersInTrigger",cm)==2 && trigger_calls==2,"trigger sums wrap signed32 in name order");
  actor->type_name="15ScriptGenerator";target->type_name="10GameObject";
  cm.set_cell(3008,800);cm.set_string(800,std::string(25,'Z'),false);cv.memory.byte(0x4f8409,0);
  cv.memory.writes.clear();
  cv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="ScriptGenerator_spawn" && std::get<std::string>(args[1])==std::string(25,'Z'),"generator receives current intern bytes");
    return 0u;
  };
  cv.dispatch("generateMonster",cm);
  require(cv.memory.read(0x4f8409)==1 && cv.memory.cstr(0x5d32c8)==std::string(25,'Z') &&
          cv.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{0x4f8409,1},{0x5d32c8,26}},
          "class interning preserves count then over-stride string write");
  // Direct helper no-object semantics are part of the reference accounting.
  cv.scene_service={};cv.apply_monster_val(nullptr,4,1);cv.apply_cflag(nullptr,0,false);cv.apply_oflag(nullptr,14,true);
  cv.menu_string_slots[0x820]=0;cv.menu_string_slots[0x850]=800;cv.string_slot=0;
  cm.params=std::numeric_limits<a::Address>::max();
  refused=false;try {cv.dispatch("getHitPoints",cm);} catch(const a::HostFault &) {refused=true;}
  require(refused && cv.menu_string_slots.size()==1 && cv.menu_string_slots.count(0x820) && !cv.string_slot,
          "creature entry invalidates unrelated slots before argument read");

  // Inventory callbacks are required and copied for replacement/reentry. A failed
  // first random call retains the seed store and draw counter, without fake items.
  b::Variables iv;
  a::ImageOptions io;io.data.resize(4096);io.stack_cells=128;
  a::Machine im(a::build_image({0},io));require(im.load()==0,"inventory API machine");
  im.params=3000;im.set_string(0,"Alpha",false);im.set_cell(3004,0);
  iv.memory.word(0x4d8ae0,1);iv.memory.word(0x404464,0xffffffff);iv.memory.writes.clear();
  refused=false;try {iv.dispatch("givePlayerRandom",im);} catch(const a::HostFault &) {refused=true;}
  require(refused && iv.draws==1 && iv.memory.word(0x404464)==0x3c558d52 &&
          iv.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{0x404464,4}},
          "missing random service preserves exactly one shared seed draw");
  bool positioned=false;
  iv.inventory_service=[](const std::string &,const std::vector<b::Variables::InventoryValue> &) -> b::Variables::InventoryResult {return 0u;};
  iv.drop_at([&]() -> champions::simulation::rta::objects::GameObject * {positioned=true;throw std::runtime_error("unexpected position read");});
  require(!positioned,"zero pool allocation skips position lookup");
  auto ground=std::make_shared<champions::simulation::rta::objects::GameObject>(0x2000);
  std::function<champions::simulation::rta::objects::GameObject *()> original_position=[&] {positioned=true;return ground.get();};
  iv.inventory_service=[&](const std::string &service,const std::vector<b::Variables::InventoryValue> &args) -> b::Variables::InventoryResult {
    if(service=="pool_new") {original_position={};return 0x8000u;}
    require(service=="ItemProp_ctor2" && positioned && std::get<std::uint32_t>(args[0])==0x8000,
            "drop keeps its supplied position callable across allocation callback replacement");return 0u;
  };
  iv.drop_at(original_position);
  iv.inventory_service=[&](const std::string &service,const std::vector<b::Variables::InventoryValue> &) -> b::Variables::InventoryResult {
    require(service=="inventory_countItemByName","inventory active callable survives replacement");
    iv.inventory_service={};iv.quest_set("Reentered",5);return 9u;
  };
  require(iv.dispatch("countQuestItems",im)==9 && iv.quest_get("Reentered")==5,"inventory callback replacement and helper reentry");
  auto shared_authority=std::make_shared<champions::simulation::rta::WorldAuthority>();
  champions::simulation::rta::Object shared_player;
  shared_player.id=0x2000;
  shared_authority->register_object(shared_player);
  b::Variables shared_inventory(shared_authority);
  shared_inventory.inventory_service=[&](const std::string &service,const std::vector<b::Variables::InventoryValue> &) -> b::Variables::InventoryResult {
    require(service=="inventory_countItemByName","onePlayerHasItem counts from inventory");
    return 9u;
  };
  im.set_cell(3004,0);im.set_cell(3008,0);
  shared_inventory.memory.word(0x4d8ae0,1);
  shared_inventory.memory.word(shared_inventory.player_record(0)+0x43d8,shared_player.id);
  shared_inventory.memory.word(shared_player.id+0xc,0);
  require(shared_inventory.dispatch("onePlayerHasItem",im)==9,"shared player object flags are readable without a script GameObject identity");
  shared_inventory.memory.word(shared_player.id+0xc,8);
  require(shared_inventory.dispatch("onePlayerHasItem",im)==0,"dead shared player is excluded before inventory lookup");
  shared_authority->unregister_object(shared_player);
  refused=false;try {shared_inventory.dispatch("onePlayerHasItem",im);} catch(const a::HostFault &) {refused=true;}
  require(refused,"stale shared player identity still refuses inventory lookup");
  iv.inventory_service={};im.set_cell(3004,0);im.set_cell(3008,0);
  iv.memory.byte(0x4f7148,0);iv.memory.word(iv.player_record(0)+0x130,1);
  iv.memory.word(iv.player_record(0)+0x1330,1);iv.memory.word(iv.player_record(0)+0x4434,0xabcdef01);
  iv.memory.writes.clear();
  refused=false;try {iv.dispatch("removeAllWeapons",im);} catch(const a::HostFault &) {refused=true;}
  require(refused && iv.memory.writes.empty() && iv.memory.word(iv.player_record(0)+0x4434)==0xabcdef01,
          "failed bag removal precedes equipped pointer clears");
  iv.quest_string_slot=800;iv.menu_string_slots[0xb0]=0;iv.cutscene_string_slot=0;
  im.params=std::numeric_limits<a::Address>::max();
  refused=false;try {iv.dispatch("giveOnePlayerItem",im);} catch(const a::HostFault &) {refused=true;}
  require(refused && iv.menu_string_slots.size()==1 && iv.menu_string_slots.count(0xb0) && !iv.quest_string_slot && !iv.cutscene_string_slot,
          "inventory entry clears unrelated slots before argument read");

  // Effects preserve callback failure prefixes, live reads, and active callables.
  b::Variables ev;
  a::Machine em(a::build_image({0},io));require(em.load()==0,"effects API machine");
  em.params=3000;em.set_string(0,"Prop",false);em.set_string(800,"Clip",false);
  em.set_cell(3004,0);em.set_cell(3008,800);em.set_cell(3012,UINT32_MAX);
  auto prop=std::make_shared<champions::simulation::rta::objects::GameObject>(0x2000);
  auto prop2=std::make_shared<champions::simulation::rta::objects::GameObject>(0x4000);
  ev.named_objects[0x2000]=prop;ev.named_objects[0x4000]=prop2;
  ev.object_names.add("Prop",prop.get());ev.object_names.add("Prop",prop2.get());
  ev.object_properties[0x2000]=std::vector<std::string>{"lump=First","lump=Second"};
  ev.memory.string(0x531278,"LuMP");ev.memory.writes.clear();
  refused=false;try {ev.dispatch("startPropAnim",em);} catch(const a::HostFault &) {refused=true;}
  require(refused && ev.memory.writes.empty(),"missing anim_clear refuses before property key write");
  std::vector<std::string> effect_calls;
  ev.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    effect_calls.push_back(service);return 0u;
  };
  refused=false;try {ev.dispatch("startPropAnim",em);} catch(const a::HostFault &) {refused=true;}
  require(refused && effect_calls==std::vector<std::string>{"anim_clear"} &&
          ev.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{0x531278,4}} &&
          ev.memory.cstr(0x531278)=="lump","missing resource preserves clear then exact key write");
  ev.find_resource=[&](const std::string &lump,const std::string &anim) {
    require(lump=="First" && anim=="Clip","prop resource selects first case-sensitive property");
    ev.find_resource={};ev.quest_set("PropReentry",9);return 0x87654321u;
  };
  effect_calls.clear();ev.dispatch("startPropAnim",em);
  require(effect_calls==std::vector<std::string>{"anim_clear","anim_push","anim_jumpTopToEnd","anim_releaseCaches"} &&
          ev.quest_get("PropReentry")==9,"prop resource callback replacement and reentry");
  // Lookup captures object order, but subsequent calls reread argument cells.
  em.set_cell(3008,7);effect_calls.clear();
  ev.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="virtual" && std::get<std::uint32_t>(args[0])==3 &&
            std::get<std::uint32_t>(args[2])==7,"first trigger reads live argument");
    ev.object_names.remove("Prop",prop2.get());em.set_cell(3008,19);
    ev.scene_service=[&](const std::string &next,const std::vector<b::Variables::ServiceValue> &values) {
      require(next=="virtual" && std::get<std::uint32_t>(values[1])==0x4000 &&
              std::get<std::uint32_t>(values[2])==19,"captured trigger list survives name removal");
      effect_calls.push_back("second");return 0u;
    };
    effect_calls.push_back("first");return 0u;
  };
  ev.dispatch("trigger",em);
  require(effect_calls==std::vector<std::string>{"first","second"},"trigger callback replacement preserves captured iteration");
  ev.object_query={};ev.scene_service={};em.set_cell(3004,0);
  refused=false;try {ev.dispatch("objInPlayerTrig",em);} catch(const a::HostFault &) {refused=true;}
  require(refused,"object class enumeration requires callback");
  ev.memory.word(0x4068,UINT32_MAX);
  ev.object_query=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="world_findObjectsByClass" && std::get<std::string>(args[0])=="7Trigger" &&
            std::get<std::uint32_t>(args[1])==50,"trigger query exact class and limit");
    ev.object_query={};return std::vector<champions::simulation::rta::objects::GameObject *>{prop2.get()};
  };
  require(ev.dispatch("objInPlayerTrig",em)==0,"inactive triggers skip virtual service");
  // Zero allocation and countdown paths skip services; slots clear on numeric entry.
  ev.scene_service=[](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    require(service=="pool_new","zero light allocation skips constructor");return 0u;
  };
  ev.dispatch("lightEffect",em);
  ev.scene_service={};ev.memory.word(0x4f7114,1);ev.dispatch("shakeCamera",em);
  refused=false;try {ev.dispatch("setFarClip",em);} catch(const a::HostFault &) {refused=true;}
  require(refused && ev.menu_string_slots.empty(),"numeric camera entry clears slots before missing callback");
  em.set_cell(3004,3);refused=false;
  try {ev.dispatch("setParticle",em);} catch(const a::HostFault &) {refused=true;}
  require(refused,"particle pointer-table overrun explicitly refuses");
  ev.menu_string_slots[0x10f0]=0;ev.menu_string_slots[0x840]=800;ev.menu_string_slots[0x40]=0;
  em.params=std::numeric_limits<a::Address>::max();refused=false;
  try {ev.dispatch("particleMorph",em);} catch(const a::HostFault &) {refused=true;}
  require(refused && ev.menu_string_slots.size()==2 && ev.menu_string_slots.count(0x10f0) && ev.menu_string_slots.count(0x840),
          "particleMorph preserves exactly its two slots before argument failure");

  em.params=3000;em.set_cell(3004,UINT32_MAX-3);unsigned allocations=0;
  ev.scene_service=[&](const std::string &,const std::vector<b::Variables::ServiceValue> &) {++allocations;return 0u;};
  refused=false;try {ev.dispatch("lightEffect",em);} catch(const a::HostFault &) {refused=true;}
  require(refused && allocations==0 && ev.menu_string_slots.size()==1 && ev.menu_string_slots.at(0x40)==-4,
          "accepted negative light pointer records slot then faults before allocation");

  b::Variables vv;
  a::ImageOptions vi;vi.data.resize(4096);vi.stack_cells=128;
  a::Machine vm(a::build_image({0},vi));require(vm.load()==0,"vehicle API machine");
  vm.params=3000;vm.set_string(0,"Entity",false);vm.set_string(800,"Car",false);
  vm.set_cell(3004,0);vm.set_cell(3008,800);vm.set_cell(3012,1);
  vm.set_cell(3016,0x80000000);vm.set_cell(3020,0x7fffffff);vm.set_cell(3024,0x7fffffff);
  auto car=std::make_shared<champions::simulation::rta::objects::GameObject>(0x2000);
  auto entity=std::make_shared<champions::simulation::rta::objects::GameObject>(0x4000);
  vv.named_objects[car->addr]=car;vv.named_objects[entity->addr]=entity;
  vv.object_names.add("Car",car.get());vv.object_names.add("Entity",entity.get());
  vv.memory.writes.clear();refused=false;
  try {vv.dispatch("entityRideVehicle",vm);} catch(const a::HostFault &) {refused=true;}
  require(refused && vv.memory.writes.empty(),"missing seat callback precedes entity writes");
  unsigned seat_calls=0,grid_calls=0;
  vv.vehicle_seat=[&,retained=std::make_shared<int>(9)](std::uint32_t address,const std::array<std::uint32_t,3> &offset) {
    ++seat_calls;require(address==0x2000 && offset==std::array<std::uint32_t,3>{0x3f800000,0xcf000000,0x4effffff},"integer seat offset conversion");
    vv.vehicle_seat={};require(*retained==9,"seat callable survives replacement");
    vm.set_cell(3024,7); // live angle read follows the seat callback
    vv.dispatch("getPlayerVPosition",vm); // numeric reentry clears shared slots
    return b::Variables::VehicleSeat{{0x80000000,0x7fffffff,0xffffffff},0xfffffffcu};
  };
  vv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) -> std::uint32_t {
    ++grid_calls;require(service=="obj_updateGridCell" && std::get<std::uint32_t>(args[0])==0x4000,"seat relinks after writes");
    require(vv.memory.word(0x4010)==0x80000000 && vv.memory.read(0x4038)==3 && vv.memory.read(0x4039)==0,"live facing angle low16");
    throw CallbackFault("grid failure after seat writes");
  };
  bool callback_failed=false;
  try {vv.dispatch("entityRideVehicle",vm);} catch(const CallbackFault &) {callback_failed=true;}
  require(callback_failed && seat_calls==1 && grid_calls==1 && vv.memory.writes==std::vector<std::pair<std::uint32_t,std::size_t>>{{0x4010,4},{0x4014,4},{0x4018,4},{0x4038,2}},"grid failure retains ordered position/facing writes");
  // Missing hold/attach/interaction services refuse; direct control needs none.
  vv.scene_service={};vm.set_string(0,"Car",false);vm.set_cell(3004,0);vm.set_cell(3008,0xffffffff);vm.set_cell(3012,2);
  vv.memory.writes.clear();vv.dispatch("controlVehicle",vm);
  require(vv.memory.word(0x2410)==0xffffffff && vv.memory.word(0x22f0)==0x40000000,"vehicle control raw stores");
  refused=false;try {vv.dispatch("holdVehicle",vm);} catch(const a::HostFault &) {refused=true;}
  require(refused && vv.string_slot==0,"hold refusal preserves its shared pointer slot");
  vm.set_string(800,"Entity",false);vm.set_cell(3008,800);refused=false;
  try {vv.dispatch("putNPCInVehicle",vm);} catch(const a::HostFault &) {refused=true;}
  require(refused && vv.quest_string_slot==800,"missing attach callback after both string conversions");
  vv.memory.word(0x4d8ae4,0);refused=false;
  try {vv.dispatch("putPlayersInVehicle",vm);} catch(const a::HostFault &) {refused=true;}
  require(refused && vv.menu_string_slots.count(0x40),"missing active interaction service refuses");
  // Live callback changes select subsequent slots and limit the player loop.
  vv.memory.word(0x4d8ae0,3);vv.memory.word(0x4d8ae4,1);
  vv.memory.word(vv.player_record(0)+0x43d8,0x4000);
  unsigned interacts=0;
  vv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="virtual" && std::get<std::uint32_t>(args[0])==14 && std::get<std::uint32_t>(args[1])==0x2000,"vehicle interaction ABI");
    auto slot=std::get<std::uint32_t>(args[2]);require(slot==(interacts==0 ? 1u : 0u),"active first and live loop");
    ++interacts;vv.memory.word(0x4d8ae0,1);vv.memory.word(0x4d8ae4,3);return 0u;
  };
  vv.dispatch("putPlayersInVehicle",vm);require(interacts==2,"player loop rereads count and active slot");

  vm.set_string(800,"Missing",false);vm.set_cell(3008,800);
  unsigned attach_calls=0;
  vv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="Vehicle_attachNpcDriver" && std::get<std::uint32_t>(args[0])==0x2000 && std::get<std::uint32_t>(args[1])==0,"missing NPC attaches zero");
    ++attach_calls;return 0u;
  };
  vv.dispatch("putNPCInVehicle",vm);require(attach_calls==1,"missing NPC still calls attach");
  // Ordered three-term cancellation must use the reference arithmetic.
  vv.memory.word(vv.player_record(0)+0x43d8,0x4000);vv.memory.word(0x4540,0x2000);
  vv.memory.word(0x4f8420,1);vv.memory.word(0x4f8440,0);vv.memory.word(0x2348,0x40400000);
  vv.memory.word(0x2910,0x6000);
  for(unsigned k=0;k<3;++k) vv.memory.word(0x600c+4*k,0x3f800000);
  vv.memory.word(0x2468,0xbf800000);vv.memory.word(0x246c,0x3f800000);vv.memory.word(0x2470,0);
  require(vv.dispatch("getPlayerVDir",vm)==-1,"zero cancellation follows second direction test");
  vv.memory.word(0x2470,0xbf800000);require(vv.dispatch("getPlayerVDir",vm)==1,"positive third term after cancellation");
  vv.memory.word(0x4f8420,2);vv.memory.word(0x4f8440,3);vv.memory.word(0x4f8444,0);
  require(vv.dispatch("getPlayerVDir",vm)==1,"alive list skips null entries before first qualifying vehicle");

  b::Variables pv;
  a::Machine pm(a::build_image({0},vi));require(pm.load()==0,"player interaction API machine");
  pm.params=3000;pm.set_cell(3004,0);pm.set_cell(3008,10);pm.set_cell(3012,20);pm.set_cell(3016,30);
  pm.set_string(0,"Centre",false);
  for(auto address:{0x2000u,0x3000u,0x4000u}) {
    auto obj=std::make_shared<champions::simulation::rta::objects::GameObject>(address);
    pv.named_objects[address]=obj;
  }
  pv.object_names.add("Centre",pv.named_objects.at(0x2000).get());
  pv.memory.word(0x4d8ae0,1);pv.memory.word(pv.player_record(0)+0x43d8,0x3000);
  pv.memory.word(0x4f8420,1);pv.memory.word(0x4f8440,0);pv.memory.word(0x3010,0x3f800000);
  refused=false;try {pv.dispatch("hitPlayers",pm);} catch(const a::HostFault &) {refused=true;}
  require(refused && !pv.memory.word(0x32e8),"missing damage service refuses before push");
  unsigned damage_calls=0;
  pv.scene_service=[&,retained=std::make_shared<int>(7)](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    require(service=="virtual" && std::get<std::uint32_t>(args[0])==2 && std::get<std::uint32_t>(args[1])==0x3000 &&
      std::get<std::uint32_t>(args[4])==0x41a00000,"player damage ABI");
    ++damage_calls;pv.scene_service={};require(*retained==7,"damage callback survives self replacement");
    pv.quest_set("Reentry",9);pv.memory.word(pv.player_record(0)+0x43d8,0x4000);pm.set_cell(3016,50);
    pv.memory.word(0x4f8420,0);return 0u;
  };
  pv.dispatch("hitPlayers",pm);
  require(damage_calls==1 && pv.memory.word(0x42e8)==0x42480000 && pv.memory.word(0x32e8)==0,
    "damage rereads object and argument after callback, and live count stops loop");
  pv.memory.word(pv.player_record(0)+0xac,0x3f800000);pv.memory.word(pv.player_record(0)+0xb0,0x42c80000);
  refused=false;try {pv.dispatch("planarSteel",pm);} catch(const a::HostFault &) {refused=true;}
  require(refused && pv.memory.word(pv.player_record(0)+0xac)==0x42c80000,"missing spell allocation retains heal prefix");
  unsigned spell_calls=0;
  pv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    ++spell_calls;
    if(service=="pool_new") {require(std::get<std::uint32_t>(args[0])==0x20,"horn allocation size");return 0u;}
    require(service=="creature_addSpell" && std::get<std::uint32_t>(args[1])==0,"zero spell is still attached");return 0u;
  };
  pv.dispatch("hornOmen",pm);require(spell_calls==2,"zero allocation skips constructor only");
  pv.memory.byte(0x4f7148,0);pv.memory.word(0x4f85c0,0xfeed);
  pv.scene_service={};pv.memory.writes.clear();
  refused=false;try {pv.save_player_record();} catch(const a::HostFault &) {refused=true;}
  require(refused && pv.memory.writes.empty(),"missing copy service refuses before snapshot writes");
  const auto rec=pv.player_record(0);
  pv.memory.byte(rec+0x78,0xa5);pv.memory.byte(rec+0x4c0d,0x5a);
  pv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &) {
    if(service=="save_playerCopy") {pv.memory.byte(rec+0x78,0xff);pv.memory.byte(0x4f7148,3);pv.memory.byte(0x100000,0x42);return 0x100000u;}
    require(service=="game_rebuildAlivePlayers","restore rebuild ABI");throw CallbackFault("rebuild failure");return 0u;
  };
  callback_failed=false;try {pv.restore_player_record();} catch(const CallbackFault &) {callback_failed=true;}
  require(callback_failed && pv.memory.read(rec)==0x42 && pv.memory.read(rec+0x78)==0xa5 &&
    pv.memory.read(rec+0x4c0d)==0x5a && pv.memory.word(0x4f85c0)==0xfeed,
    "restore captures live ranges and original slot before callback; rebuild failure retains saved flag");
  pv.memory.word(0x4d8ae4,UINT32_MAX);pv.memory.word(0x4d8ae0,1);pv.scene_service={};
  refused=false;try {pv.dispatch("playGotItemSound",pm);} catch(const a::HostFault &) {refused=true;}
  require(refused && pv.memory.word(0x4d8ae4)==0,"missing voice service retains active clamp");
  const auto raw_distance=b::Variables::distance3({0x40400000,0x40800000,0},{0,0,0});
  require(raw_distance==std::array<std::uint32_t,3>{0x40a00000,0x40400000,0x40800000},"distance helper ordered sqrt and deltas");

  // No invented audio success; exact request ordering, callable retention and reentry.
  b::Variables uv; a::ImageOptions uo;uo.data.resize(4096);uo.stack_cells=128;
  a::Machine um(a::build_image({0},uo),uv.natives());
  require(um.load()==0,"utility fixture load");um.params=3000;
  um.set_cell(3004,0);um.set_cell(3008,800);um.set_string(0,"Lump",false);um.set_string(800,"Sample",false);
  refused=false;try {uv.dispatch("sound",um);} catch(const a::HostFault &) {refused=true;}
  require(refused && uv.quest_string_slot==800,"sound converts both strings before resource refusal");
  uv.find_resource=[](const std::string &,const std::string &) {return 0u;};
  uv.dispatch("sound",um); // A missing sample succeeds without an audio service.
  refused=false;try {uv.dispatch("soundSequence",um);} catch(const a::HostFault &) {refused=true;}
  require(refused,"sequence requests playback even when resource absent");
  unsigned audio_calls=0;
  uv.scene_service=[&](const std::string &service,const std::vector<b::Variables::ServiceValue> &args) {
    ++audio_calls;require(service=="sound_playOnObject" && std::get<std::uint32_t>(args[0])==0,"sequence playback request");
    uv.scene_service=[&](const std::string &next,const std::vector<b::Variables::ServiceValue> &values) {
      ++audio_calls;require(next=="sound_setPlaysWhileFrozen" && std::get<std::uint32_t>(values[0])==0xffffffff,"live replaced audio service");return 0u;
    };
    uv.dispatch("printVal",um); // Numeric reentry invalidates the shared string slot.
    return 0xffffffffu;
  };
  uv.dispatch("soundSequence",um);
  require(audio_calls==2 && !uv.quest_string_slot,"audio replacement and reentry");
  uv.find_resource=[&](const std::string &lump,const std::string &entry) {
    require(lump=="Lump" && entry=="Sample","owned converted audio strings");uv.find_resource={};return 0u;
  };
  uv.dispatch("sound",um);require(!uv.find_resource,"resource callable retained while replacing itself");
  um.params=std::numeric_limits<a::Address>::max();
  require(uv.dispatch("printVal",um)==0 && uv.dispatch("demoRunning",um)==0,"constant handlers read no arguments");

  std::cout << "script built-ins API PASS\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--api") { api_tests(); return 0; }
    require(argc == 3, "usage: script-builtins-tests vectors.tsv actual.tsv | --api");
    std::ifstream source(argv[1]);
    std::ofstream trace(argv[2]);
    require(bool(source) && bool(trace), "fixture/trace open");
    Runner runner;
    std::size_t rows = 0;
    std::string line;
    while (std::getline(source, line)) {
      if (line.empty() || line[0] == '#') continue;
      auto fields = split(line, '\t');
      require(fields.size() == 4, "fixture field count");
      auto result = runner.run(fields[0], fields[1]);
      auto state = runner.snapshot();
      ++rows;
      auto actual = fields[0] + '\t' + fields[1] + '\t' + result + '\t' + state;
      trace << actual << '\n';
      if (result != fields[2] || state != fields[3]) {
        auto expected_state = split(fields[3], '|');
        auto actual_state = split(state, '|');
        std::string difference;
        for (std::size_t i = 0; i < actual_state.size(); ++i)
          if (actual_state[i] != expected_state[i]) difference += " state[" + std::to_string(i) + "]";
        throw std::runtime_error("row " + std::to_string(rows) + " " + fields[0] +
                                 " expected " + fields[2] + " got " + result + difference);
      }
    }
    require(source.eof() && rows > 1000, "incomplete fixture input");
    trace.flush();
    require(bool(trace), "trace write");
    std::cout << "script built-ins exact PASS: " << rows << " complete state transitions\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "script built-ins FAIL: " << error.what() << '\n';
    return 1;
  }
}
