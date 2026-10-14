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
template<class T> std::string join(const T &values) {
  std::string result;
  for (const auto &value : values) {
    if (!result.empty()) result += ',';
    result += value;
  }
  return result.empty() ? "-" : result;
}
struct Runner {
  std::unique_ptr<b::Variables> variables;
  std::unique_ptr<a::Machine> machine;
  unsigned mode = 0;
  std::vector<std::string> events;
  std::vector<std::string> bindings;
  bool player_mode=false, scene_mode=false, menu_mode=false, creature_mode=false, item_mode=false, effects_mode=false;
  void reset(bool extended=false, bool with_players=false, bool with_scene=false, bool with_menus=false, bool with_creatures=false, bool with_items=false, bool with_effects=false) {
    effects_mode=with_effects;item_mode=with_items;creature_mode=with_creatures;menu_mode=with_menus;player_mode=with_players;scene_mode=with_scene;
    bindings=names;
    if (extended) bindings.insert(bindings.end(),campaign.begin(),campaign.end());
    if(with_players) bindings.insert(bindings.end(),players.begin(),players.end());
    if(with_scene) bindings.insert(bindings.end(),scene.begin(),scene.end());
    if(with_menus) bindings.insert(bindings.end(),menus.begin(),menus.end());
    if(with_creatures) bindings.insert(bindings.end(),creature_names.begin(),creature_names.end());
    if(with_effects) bindings.insert(bindings.end(),effects_names.begin(),effects_names.end());
    if(with_items) bindings.insert(bindings.end(),item_names.begin(),item_names.end());
    variables = std::make_unique<b::Variables>();
    a::ImageOptions options;
    options.data.resize(4096);
    options.natives = bindings;
    options.stack_cells = 128;
    machine = std::make_unique<a::Machine>(a::build_image({0}, options), variables->natives());
    require(machine->load() == 0, "fixture machine load");
    variables->request_config_save = [this]() {
      events.push_back("memcard_requestConfigSave");
      if (mode == 1) throw CallbackFault("authored callback exception");
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
  std::string snapshot() const {
    std::vector<std::string> memory, writes, slots, registers;
    for (const auto &entry : variables->memory.bytes)
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
    require(mode <= 3, "callback mode");
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
      else require(op == "reset" || op == "resetcampaign" || op == "resetplayers" || op == "resetscene" || op == "resetmenus" || op == "resetcreatures" || op == "resetitems" || op == "reseteffects", "unknown fixture operation");
    } catch (const b::StackGarbage &) { result = "StackGarbage"; }
    catch (const champions::simulation::rta::objects::ObjectError &) { result = "CallbackFault"; }
    catch (const CallbackFault &) { result = "CallbackFault"; }
    catch (const a::HostFault &) { result = "HostFault"; }
    return result;
  }
};
void api_tests() {
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
  require(variables.natives().size() == 155, "subset binding count");
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
