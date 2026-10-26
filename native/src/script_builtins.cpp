#include "champions/script_builtins.hpp"
#include <algorithm>
#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
#include <limits>
#include <sstream>
#include <stdexcept>

namespace champions::simulation::rta::builtins {
Memory::Integer truncating_divide(const Memory::Integer &a, const Memory::Integer &b) {
  using Bytes = std::vector<std::uint8_t>;
  auto trim = [](Bytes &v) { while(!v.empty() && v.back()==0) v.pop_back(); };
  Bytes dividend=a.magnitude, divisor=b.magnitude;
  trim(dividend); trim(divisor);
  if(divisor.empty()) throw std::domain_error("integer division by zero");
  Bytes quotient(dividend.size(),0), remainder;
  // Binary long division uses only bounded byte arithmetic, with no host
  // signed division/overflow and no limit on the integer's numeric width.
  for(std::size_t byte=dividend.size(); byte>0; --byte) {
    for(unsigned bit=8; bit>0; --bit) {
      unsigned carry=(dividend[byte-1]>>(bit-1))&1u;
      for(auto &digit:remainder) {
        const unsigned shifted=unsigned(digit)*2+carry;
        digit=std::uint8_t(shifted); carry=shifted>>8;
      }
      if(carry) remainder.push_back(std::uint8_t(carry));
      const bool at_least = remainder.size()>divisor.size() ||
        (remainder.size()==divisor.size() &&
         !std::lexicographical_compare(remainder.rbegin(),remainder.rend(),divisor.rbegin(),divisor.rend()));
      if(at_least) {
        unsigned borrow=0;
        for(std::size_t i=0; i<remainder.size(); ++i) {
          const unsigned sub=(i<divisor.size() ? divisor[i] : 0u)+borrow;
          const unsigned digit=remainder[i];
          remainder[i]=std::uint8_t(digit-sub); borrow=digit<sub;
        }
        trim(remainder);
        quotient[byte-1]|=std::uint8_t(1u<<(bit-1));
      }
    }
  }
  trim(quotient);
  const bool negative=!quotient.empty() && a.negative!=b.negative;
  return {std::move(quotient),negative};
}
namespace {
std::map<const objects::GameObject *,const Obj *> &memory_objects() {
  static std::map<const objects::GameObject *,const Obj *> registry;
  return registry;
}
template<class F> F host_delegate(const F &fn) {
  if(!fn) throw amx::HostFault("Host object-world delegate required");
  return fn;
}
}
Host::Value Host::default_answer(const std::string &name, const Arguments &args) const {
  if(name=="virtual") {
    const auto slot=std::get<std::int64_t>(args.at(0));
    if(slot==19) return Seat{};
    if(slot!=1 && slot!=2 && slot!=3 && slot!=8 && slot!=14 &&
       slot!=17 && slot!=18 && slot!=27 && slot!=34)
      throw std::out_of_range("unknown virtual slot");
    return std::int64_t(0);
  }
  // Return classification from EXT in the pinned contract. Numeric/unused
  // results are both zero; no external engine behavior is synthesized.
  static const std::vector<std::string> known = {
    "net_isLocalSlot", "net_getLocalPlayerSlot", "net_firstActiveSlot",
    "net_closeLocalMenus", "net_dropAllRemotes", "net_sendGameMsg",
    "hudmsg_sendMedalAck", "invmsg_sendStatPoint", "game_sendCharacterToLobby",
    "dialog_isOpen", "Camera_update", "Camera_getYaw", "Camera_getPitch",
    "cut_start", "lump_load", "res_find", "text_lookup", "save_game",
    "dbg_printf", "deathTask_schedule", "memcard_requestSave",
    "memcard_requestEndGameSave", "memcard_requestConfigSave", "memcard_getState",
    "memcard_isBusy", "menuMutex_release", "missionMenu_open", "game_openMissionMenu",
    "shop_openOrStock", "playerSel_isRunning", "save_playerCopy", "game_rebuildAlivePlayers",
    "hud_showMessage", "hud_showHelpMessage", "hud_setCompassTarget", "hud_setBust",
    "hud_setFade", "hud_stepFade", "hud_drawMessageBox", "wstr_format", "wstr_copy",
    "text_begin", "text_end", "text_drawWrappedCentered", "automap_revealAll",
    "sound_playSample", "sound_playOnObject", "sound_setPlaysWhileFrozen",
    "music_playLevel", "stream_playLevel", "stream_queueStop", "creature_startBossMusic",
    "camera_shake", "camera_setFarClip", "particle_define", "particle_spawn",
    "decal_add", "player_voice", "anim_clear", "anim_push", "anim_jumpTopToEnd",
    "anim_releaseCaches", "anim_start", "inventory_countItemByName",
    "inventory_findItemBySubstring", "inventory_removeItem", "inventory_giveItem",
    "inventory_equipItem", "inventory_onStatPoint", "item_parseDescriptor",
    "player_itemWeight", "item_generateWithAffixes", "level_monsterLevelTimesDifficulty",
    "player_recalcLoad", "player_resetIdleAnim", "player_giveExp", "loot_spawnNamedItem",
    "obj_moveOffGrid", "obj_updateGridCell", "obj_runEveryFrame",
    "world_collectObjectsInBox", "world_forEachGridObject", "world_findObjectsByClass",
    "pool_new", "ItemProp_ctor2", "LightEffect_ctor", "SpellHornOmenEffect_ctor",
    "SpellPlanarSteelEffect_ctor", "ParticleMorphEffect_ctor", "creature_addSpell",
    "ScriptGenerator_spawn", "MonsterTrigger_count", "Creature_setCharm",
    "Creature_scaleStats", "Vehicle_attachNpcDriver", "Vehicle_setHold"
  };
  if(std::find(known.begin(),known.end(),name)==known.end())
    throw std::out_of_range("unknown external call");
  if(name=="world_collectObjectsInBox" || name=="world_forEachGridObject" ||
     name=="world_findObjectsByClass") return List{};
  if(name=="inventory_findItemBySubstring")
    return std::pair<std::int64_t,std::int64_t>{-1,0};
  return std::int64_t(0);
}
Host::Object &Host::object(std::uint32_t addr) const {
  if(!game) throw amx::HostFault("Host has no game");
  auto obj=host_delegate(game->object)(addr);
  if(!obj) throw std::out_of_range("Host registry has no object");
  return *obj;
}
void Host::do_obj_moveOffGrid(std::uint32_t addr) {
  if(!game) throw amx::HostFault("Host has no game");
  // Python resolves the bound World method before evaluating _obj(addr).
  auto fn=host_delegate(game->move_off_grid); fn(object(addr));
}
void Host::do_obj_updateGridCell(std::uint32_t addr) {
  if(!game) throw amx::HostFault("Host has no game");
  auto fn=host_delegate(game->update_grid_cell); fn(object(addr));
}
void Host::do_obj_runEveryFrame(std::uint32_t addr) {
  if(!game) throw amx::HostFault("Host has no game");
  auto fn=host_delegate(game->run_every_frame); fn(object(addr));
}
Host::List Host::do_world_collectObjectsInBox(const Vec3 &lo, const Vec3 &hi, std::int64_t limit) {
  if(!game) throw amx::HostFault("Host has no game");
  return host_delegate(game->collect_in_box)(lo,hi,limit);
}
Host::List Host::do_world_forEachGridObject(const Value &) {
  if(!game) throw amx::HostFault("Host has no game");
  List seen;
  host_delegate(game->for_each_grid_object)([&](Object &obj) { seen.push_back(&obj); });
  return seen;
}
Host::List Host::do_world_findObjectsByClass(const std::string &type, std::int64_t limit) {
  if(!game) throw amx::HostFault("Host has no game");
  return host_delegate(game->find_by_class)(type,limit);
}
Host::Value Host::do_virtual(std::int64_t slot, std::uint32_t addr, const Arguments &args) {
  if(slot==1) {
    if(!game) throw amx::HostFault("Host has no game");
    auto fn=host_delegate(game->on_destroy);
    auto &obj=object(addr);
    fn(obj,std::get<std::int64_t>(args.at(0)));
  }
  return default_answer("virtual",{slot});
}
void Host::do_game_rebuildAlivePlayers() {
  if(!game) throw amx::HostFault("Host has no game");
  host_delegate(game->rebuild_alive_players)();
}
Host::Value Host::call(std::string name, const Arguments &input) {
  // Copy the arguments before a handler can reenter/reallocate the log.
  const Arguments args=input;
  log.emplace_back(name,args);
  auto found=handlers.find(name);
  if(found!=handlers.end() && found->second) {
    const auto fn=found->second; return fn(args);
  }
  // Python's fixed-arity do_* methods reject malformed calls after logging,
  // before resolving the game/object delegates. Virtual accepts extra words.
  static const std::map<std::string,std::size_t> arity = {
    {"obj_moveOffGrid",1},{"obj_updateGridCell",1},{"obj_runEveryFrame",1},
    {"world_collectObjectsInBox",3},{"world_forEachGridObject",1},
    {"world_findObjectsByClass",2},{"game_rebuildAlivePlayers",0}
  };
  auto expected=arity.find(name);
  if((expected!=arity.end() && args.size()!=expected->second) ||
     (name=="virtual" && args.size()<2))
    throw amx::HostFault("malformed Host call argument count");
  auto integer=[&](std::size_t k) {return std::get<std::int64_t>(args.at(k));};
  if(name=="obj_moveOffGrid") {do_obj_moveOffGrid(std::uint32_t(integer(0)));return {};}
  if(name=="obj_updateGridCell") {do_obj_updateGridCell(std::uint32_t(integer(0)));return {};}
  if(name=="obj_runEveryFrame") {do_obj_runEveryFrame(std::uint32_t(integer(0)));return {};}
  if(name=="world_collectObjectsInBox")
    return do_world_collectObjectsInBox(std::get<Vec3>(args.at(0)),std::get<Vec3>(args.at(1)),integer(2));
  if(name=="world_forEachGridObject") return do_world_forEachGridObject(args.at(0));
  if(name=="world_findObjectsByClass") return do_world_findObjectsByClass(std::get<std::string>(args.at(0)),integer(1));
  if(name=="virtual") return do_virtual(integer(0),std::uint32_t(integer(1)),Arguments(args.begin()+2,args.end()));
  if(name=="game_rebuildAlivePlayers") {do_game_rebuildAlivePlayers();return {};}
  return default_answer(name,args);
}
namespace {
const std::vector<std::string> script_names = {"callScript", "addDeferedCall", "postAction"};
constexpr std::uint32_t DEFER_NAMES=0x86b278, DEFER_COUNT=0x86da78,
  DEFER_QUEUED=0x86da7c, DEFER_FLIP=0x4f8690, POST_ACTION=0x86fa80,
  IN_POST_ACTION=0x4f8698, MISSING_STAMP=0x4f8694, DRAW_FRAME=0x44fdd0;
amx::Cell signed_byte(std::uint8_t value) {
  return value < 128 ? value : static_cast<amx::Cell>(value) - 256;
}
std::string lower(std::string name) {
  for (char &ch : name) {
    auto value = static_cast<unsigned char>(ch);
    if (value >= 128)
      throw amx::HostFault("non-ASCII variable name");
    if (value >= 'A' && value <= 'Z')
      ch = static_cast<char>(value + 32);
  }
  return name;
}
amx::Cell clamp(amx::Cell value) {
  return std::max<amx::Cell>(-128, std::min<amx::Cell>(127, value));
}
const std::vector<std::string> utility_names = {"print", "printVal", "getRand", "getScaledTime", "sin1000", "cos1000", "gamePaused", "demoRunning", "networkMode", "levelLoading", "getLevelTimer", "sound", "soundSequence", "startMusic", "startAudio", "endMusic"};
const std::vector<std::string> interaction_names = {"setBestActivePlayer", "hitPlayers", "closestPlayerDist", "savePlayer", "restorePlayer", "hornOmen", "planarSteel", "playGotItemSound", "explodePotionsOnly"};
const std::vector<std::string> vehicle_names = {"putNPCInVehicle", "putPlayersInVehicle", "entityRideVehicle", "controlVehicle", "holdVehicle", "getPlayerVPosition", "getVPosition", "getPlayerVDir"};
const std::vector<std::string> effects_names = {"trigger", "autoPropSetState", "autoPropGetState", "startPropAnim", "stopPropAnim", "propAnimAtEnd", "deleteObjects", "setNoCollide", "objInPlayerTrig", "findDistance", "setContainerProps", "lightEffect", "addExpandingPool", "particleMorph", "setParticle", "addParticleOnObject", "shakeCamera", "setFarClip", "setWaveDetail"};
const std::vector<std::string> inventory_names = {"playerHasItem", "onePlayerHasItem", "countQuestItems", "removePlayerItem", "removeAllItems", "removeAllWeapons", "givePlayerItem", "giveOnePlayerItem", "givePlayerRandom", "dropItem", "selectDropItem"};
const std::vector<std::string> creature_names = {"increaseEnemyRank", "charmMonster", "rootMonster", "unRootMonster", "selectSetMonsterVal", "setMonsterVal", "selectSetCFlag", "setCFlag", "selectSetOFlag", "setOFlag", "setWeaponType", "setMonsterRange", "killMonster", "deleteMonster", "hideMonster", "showMonster", "moveMonster", "followPlayer", "stopFollowingPlayer", "followPath", "selectFollowPath", "stopFollowingPath", "selectStopPath", "friendly", "unFriendly", "reapEnemies", "checkMonstersLeft", "getHitPoints", "setHitPoints", "generateMonster", "monstersInTrigger", "initScriptSetParams", "setCreatureParams", "startNPCAnim", "NPCAnimAtEnd", "countLiveObjects"};
const std::vector<std::string> menus = {"activateMissionMenu", "activateStore", "storeRunning", "setInfo", "addHelpMessage", "addHelpMessageF", "helpMessageActive", "showCompass", "showBust", "hudResetFadeScreen", "hudFadeScreen", "revealMap", "showOnMap", "runMedalSuccess", "runMedalFail", "setupPlayerSel", "startPlayerSel", "playerSelRunning"};
const std::vector<std::string> campaign = {
 "getEvil", "setEvil", "getMadeDecision", "setMadeDecision", "grantMission",
 "getMissionComplete", "setMissionAvailable", "getMissionAvailable", "grantGold",
 "grantSilver", "grantBronze", "getMedal", "addQuest", "removeQuest"};
const std::vector<std::string> players = {"getNumPlayers", "countPlayersAlive", "checkPlayerAlive", "activePlayerIsDwarf", "activePlayerIsElf", "isClericOrWizard", "getMaxPlayerHP", "healPlayer", "giveFullHealth", "zeroPlayerMana", "refillPlayerMana", "givePlayerExp", "givePlayerGold", "takePlayerGold", "checkPlayerGold", "checkPlayerMinGold", "playerAdjustStat", "setPlayerFlag", "getPlayerFlag", "piercePlayerEar", "clearAllSkills", "hidePlayers", "decayPlayerLights", "getManaUsed", "getHealthUsed"};
const std::vector<std::string> scene = {"newWorld", "newWorldAlways", "simpleNewWorld", "newWorldPost", "newWorldParameter", "setEntrance", "cutScene", "startDialog", "dialogRunning", "forceMutexRelease", "changeSwoop", "startWithSwoopDone", "leaveGame", "endGame", "exitToMainMenu", "saveGame", "saveEndConfig", "saveDone", "saveCheckPoint", "loadCheckPoint", "setSaveInhibit", "getSaveInhibit", "setAlternateDeath", "preloadResource", "clearRecall", "playersLoaded", "pauseGameControl"};
constexpr std::uint32_t NUM_PLAYERS=0x4d8ae0, DIFFICULTY=0x4f719c;
bool supported(const std::string &name) {
  if(std::find(script_names.begin(),script_names.end(),name)!=script_names.end()) return true;
  return std::find(utility_names.begin(),utility_names.end(),name)!=utility_names.end() || name == "getv" || name == "setv" || name == "addv" ||
         name == "getpv" || name == "setpv" ||
         std::find(campaign.begin(), campaign.end(), name) != campaign.end() ||
         std::find(players.begin(), players.end(), name) != players.end() ||
         std::find(scene.begin(), scene.end(), name) != scene.end() ||
         std::find(menus.begin(), menus.end(), name) != menus.end() ||
         std::find(inventory_names.begin(), inventory_names.end(), name) != inventory_names.end() ||
         std::find(interaction_names.begin(), interaction_names.end(), name) != interaction_names.end() ||
         std::find(vehicle_names.begin(), vehicle_names.end(), name) != vehicle_names.end() ||
         std::find(effects_names.begin(), effects_names.end(), name) != effects_names.end() ||
         std::find(creature_names.begin(), creature_names.end(), name) != creature_names.end();
}
amx::Cell argument(const amx::Machine &machine, amx::Address offset) {
  if (machine.params > std::numeric_limits<amx::Address>::max() - offset)
    throw amx::HostFault("host parameter address arithmetic exceeds int64");
  return machine.cell(machine.params + offset);
}
} // namespace
Variables::Variables(std::shared_ptr<Memory> mem,
    std::shared_ptr<objects::World> supplied_world, std::shared_ptr<Host> supplied_host)
    : memory_owner(mem ? std::move(mem) : std::make_shared<Memory>()),
      memory(*memory_owner),
      world(supplied_world ? std::move(supplied_world) : std::make_shared<objects::World>()),
      host(supplied_host ? std::move(supplied_host) : std::make_shared<Host>()),
      object_names(world->names) {
  host_owner.object = [this](std::uint32_t addr) { return obj_at(addr); };
  host_owner.rebuild_alive_players = [this]() { rebuild_alive_players(); };
  // World services remain explicit delegates: memory-backed fields cannot be
  // silently replaced by the standalone World's cached base fields.
  if(!host->game) host->game = &host_owner;
}
Variables::Variables(std::shared_ptr<WorldAuthority> authority)
    : Variables(std::make_shared<Memory>(authority->memory), authority->objects) {
  authority_ = std::move(authority);
}
Variables::~Variables() {
  for (const auto &object : setup_objects) {
    try {
      world->unregister_external(*object);
    } catch (const HandleError &) {
      // HandlePool::clear rejects live iterators/paired nodes. Teardown must
      // not throw from this implicitly noexcept destructor.
    }
    if (authority_) authority_->unregister_object(*object);
  }
  if(host && host->game==&host_owner) host->game=nullptr;
}
Host::Value Variables::ext(const std::string &name, const Host::Arguments &args) {
  auto retained = host;
  return retained->call(name,args);
}
Host::Value Variables::virtual_call(const objects::GameObject &obj, std::int64_t slot,
                                    const Host::Arguments &args) {
  Host::Arguments forwarded{slot,std::int64_t(obj.addr)};
  forwarded.insert(forwarded.end(),args.begin(),args.end());
  return ext("virtual",forwarded);
}
void Variables::rebuild_alive_players() {
  // Resolve the entire list first: a stale later pointer faults before any store.
  auto count = memory.s32(0x4d8ae0);
  std::vector<objects::GameObject *> players;
  for(std::int64_t slot=0;slot<count;++slot) players.push_back(player_object(slot));
  std::vector<std::uint32_t> living;
  for(std::size_t slot=0;slot<players.size();++slot)
    if(players[slot]) {
      auto *obj=players[slot];
      bool supplied_base=world->objects.count(obj->addr) && !named_objects.count(obj->addr);
      auto backed=Obj::memory_backed(obj);
      auto bits=backed ? backed->flags() : supplied_base ? obj->flags : memory.u32(obj->addr+0xc);
      if(!(bits&8)) living.push_back(slot);
    }
  for(std::size_t index=0;index<living.size();++index)
    memory.w32(0x4f8440+4*index,living[index]);
  memory.w32(0x4f8420,living.size());
}
std::shared_ptr<Obj> Variables::new_object(const std::vector<std::string> &names,
    std::uint32_t object_flags, const std::array<std::uint32_t,3> &position,
    std::uint32_t facing, const std::string &type, objects::Props props,
    std::optional<std::uint32_t> address, std::int64_t size) {
  if(!address) {
    if(next_address<0 || next_address>UINT32_MAX) throw amx::HostFault("setup address exceeds owned low32 domain");
    address=std::uint32_t(next_address);
    if(size>INT64_MAX-15) throw amx::HostFault("setup size exceeds int64 domain");
    auto step=(size+15)&~INT64_C(15);
    if((step>0 && next_address>INT64_MAX-step) ||
       (step<0 && next_address<INT64_MIN-step))
      throw amx::HostFault("setup allocation arithmetic exceeds int64 domain");
    next_address+=step;
  }
  // An address a combat actor already holds is refused before any byte is written.
  if (authority_ && authority_->find(*address)) throw std::logic_error("World address already registered");
  auto obj=std::make_shared<Obj>(memory,*address,type);
  obj->flags(object_flags); obj->pos(position); obj->angle(facing);
  obj->props=props ? std::make_shared<std::vector<std::string>>(*props) : nullptr;
  // home_cell reads the standalone base projection only during initial linking.
  auto &base=static_cast<objects::GameObject &>(*obj);
  base.flags=object_flags; base.pos=position; base.angle=facing&65535;
  // Register first so an address collision throws before any state is added.
  if (authority_) authority_->register_object(*obj);
  world->register_external(base);
  try {
    world->handles.link(world->home_cell(base,true),base.heads[2],&base);
    // Keep failed name-insertion prefixes alive, too, as the linked world does.
    setup_objects.push_back(obj);
  } catch (...) {
    world->unregister_external(base);
    if (authority_) authority_->unregister_object(*obj);
    throw;
  }
  for(const auto &name:names) object_names.add(name,obj.get());
  if(obj->props) for(const auto &entry:*obj->props)
    if(entry.compare(0,5,"name=")==0) object_names.add(entry.substr(5),obj.get());
  world->objects[*address]=obj.get();
  named_objects[*address]=obj;
  object_properties[*address]=obj->props ? std::optional<std::vector<std::string>>(*obj->props) : std::nullopt;
  return obj;
}
std::vector<std::pair<std::string,amx::Cell>> Variables::quest_vars() const {
  std::vector<std::pair<std::string,amx::Cell>> out;
  for(amx::Address index=0;;++index) {
    auto entry=quest_by_index(index);
    if(!entry) return out;
    out.push_back(*entry);
  }
}
Obj::Obj(Memory *memory, std::uint32_t address, std::string type)
    : objects::GameObject(address), mem(memory) {
  type_name = std::move(type);
  memory_objects()[this]=this;
  // The base's initialization of the three properties is suppressed by the
  // Python constructor while _mem is None. Construction writes no bytes.
}
Obj::~Obj() { memory_objects().erase(this); }
const Obj *Obj::memory_backed(const objects::GameObject *obj) {
  auto it=memory_objects().find(obj);
  return it==memory_objects().end() ? nullptr : it->second;
}
std::uint32_t Obj::flags() const { return mem ? mem->u32(addr+0xc) : 0; }
void Obj::flags(const Memory::Integer &v) { if(mem) mem->store(addr+0xc,4,v); }
void Obj::flags(std::uint64_t v) { if(mem) mem->w32(addr+0xc,v); }
std::array<std::uint32_t,3> Obj::pos() const {
  if(!mem) return {};
  return {mem->u32(addr+0x10),mem->u32(addr+0x14),mem->u32(addr+0x18)};
}
void Obj::pos(const std::vector<Memory::Integer> &v) {
  if(mem) for(unsigned i=0;i<3;++i) mem->store(addr+0x10+4*i,4,v.at(i));
}
void Obj::pos(const std::array<std::uint32_t,3> &v) {
  if(mem) for(unsigned i=0;i<3;++i) mem->w32(addr+0x10+4*i,v[i]);
}
std::uint16_t Obj::angle() const { return mem ? mem->u16(addr+0x38) : 0; }
void Obj::angle(const Memory::Integer &v) { if(mem) mem->store(addr+0x38,2,v); }
void Obj::angle(std::uint64_t v) { if(mem) mem->w16(addr+0x38,v); }
Memory::Integer Obj::u(std::uint32_t offset,std::size_t size) const {
  if(!mem) throw amx::HostFault("object has no memory");
  return mem->load(addr+offset,size);
}
void Obj::w(std::uint32_t offset,std::size_t size,const Memory::Integer &v) {
  if(!mem) throw amx::HostFault("object has no memory");
  mem->store(addr+offset,size,v);
}
void Obj::w(std::uint32_t offset,std::size_t size,std::uint64_t v) {
  if(!mem) throw amx::HostFault("object has no memory");
  mem->store(addr+offset,size,v);
}
std::uint64_t Obj::cflags() const {
  const auto value=u(0xa8,8);
  std::uint64_t result=0;
  for(std::size_t i=0;i<value.magnitude.size();++i)
    result|=std::uint64_t(value.magnitude[i])<<(8*i);
  return result;
}
void Obj::set_cflags(const Memory::Integer &v) { w(0xa8,8,v); }
void Obj::set_cflags(std::uint64_t v) { w(0xa8,8,v); }
std::string Obj::repr() const {
  std::ostringstream out;
  out << "<Obj 0x" << std::hex << addr << ' ' << type_name << '>';
  return out.str();
}
X::X(std::uint32_t address,std::string integers,std::int64_t float_count,
     std::int64_t stack_count,std::string result,std::string documentation)
    : addr(address), ints(std::move(integers)), floats(float_count),
      stack(stack_count), ret(std::move(result)), note(std::move(documentation)) {}
namespace {
// Shared World memory errors are host faults on the script path, never the
// combat memory's own exception types.
std::uint8_t canonical_byte(const creature_hp::Memory &memory, std::uint32_t address) {
  try {
    return std::uint8_t(memory.load(address, 1));
  } catch (const std::runtime_error &error) {
    // creature_hp::Error and the combat overlay's own error type.
    throw amx::HostFault(error.what());
  }
}
} // namespace
Memory::Page &Memory::ensure_page(std::uint32_t index) const {
  auto found = pages.find(index);
  if (found != pages.end()) return found->second;
  Page data(4096,0);
  if (under) {
    auto callback = under;
    auto supplied = callback(index << 12, data.size());
    data=std::move(supplied);
  }
  auto &result = pages.emplace(index, data).first->second;
  for (std::size_t i=0; i<data.size(); ++i)
    if (data[i]) bytes[(index << 12) + std::uint32_t(i)] = data[i];
  return result;
}
Memory::Page &Memory::page(std::uint32_t index) {
  if (canonical) throw amx::HostFault("shared World memory has no mutable page alias");
  auto &result = ensure_page(index);
  if (std::find(exposed.begin(),exposed.end(),index)==exposed.end()) exposed.push_back(index);
  return result;
}
const std::map<std::uint32_t,std::uint8_t> &Memory::nonzero_bytes() const {
  if (canonical) {
    bytes.clear();
    for (const auto &entry : canonical->bytes())
      if (entry.second) bytes.insert(bytes.end(), entry);
    return bytes;
  }
  for (auto index : exposed) {
    const auto &data=pages.at(index);
    auto base=index<<12;
    bytes.erase(bytes.lower_bound(base),bytes.upper_bound(base+4095));
    for (std::size_t i=0;i<data.size();++i) {
      auto address=(index<<12)+std::uint32_t(i);
      if(data[i]) bytes[address]=data[i]; else bytes.erase(address);
    }
  }
  return bytes;
}
std::uint8_t Memory::read(std::uint32_t address) const {
  if (canonical) return canonical_byte(*canonical, address);
  const auto &data=ensure_page(address >> 12);
  auto offset=address & 4095;
  return offset<data.size() ? data[offset] : 0;
}
std::vector<std::uint8_t> Memory::read(std::uint32_t address, std::size_t size) const {
  if (canonical) {
    std::vector<std::uint8_t> result;
    result.reserve(size);
    // Addresses wrap at 32 bits exactly as the original paged store did.
    for (std::size_t i = 0; i < size; ++i)
      result.push_back(canonical_byte(*canonical, address + std::uint32_t(i)));
    return result;
  }
  std::vector<std::uint8_t> result;
  result.reserve(size);
  while(size) {
    const auto &data=ensure_page(address>>12);
    auto offset=address & 4095;
    auto take=std::min(size,std::size_t(4096-offset));
    auto begin=std::min(std::size_t(offset),data.size());
    auto end=std::min(std::size_t(offset)+take,data.size());
    result.insert(result.end(),data.begin()+begin,data.begin()+end);
    address+=std::uint32_t(take);size-=take;
  }
  return result;
}
Memory::Integer Memory::load(std::uint32_t address,std::size_t size) const {
  Integer result{read(address,size),false};
  while(!result.magnitude.empty() && !result.magnitude.back()) result.magnitude.pop_back();
  return result;
}
void Memory::store(std::uint32_t address,std::size_t size,const Integer &value) {
  std::vector<std::uint8_t> data(size,0);
  std::copy_n(value.magnitude.begin(),std::min(size,value.magnitude.size()),data.begin());
  if(value.negative) {
    unsigned carry=1;
    for(auto &byte:data) {auto v=unsigned(std::uint8_t(~byte))+carry;byte=std::uint8_t(v);carry=v>>8;}
  }
  write(address,data);
}
void Memory::store(std::uint32_t address,std::size_t size,std::uint64_t value) {
  Integer integer;
  while(value) {integer.magnitude.push_back(std::uint8_t(value));value>>=8;}
  store(address,size,integer);
}
std::uint16_t Memory::u16(std::uint32_t a) const {
  std::uint16_t value=0;auto data=read(a,2);
  for(std::size_t i=0;i<data.size();++i) value|=std::uint16_t(data[i])<<(8*i);
  return value;
}
std::uint64_t Memory::u64(std::uint32_t a) const {
  std::uint64_t value=0;auto data=read(a,8);
  for(std::size_t i=0;i<data.size();++i) value|=std::uint64_t(data[i])<<(8*i);
  return value;
}
std::int32_t Memory::s8(std::uint32_t a) const {auto v=read(a);return v>=128 ? int(v)-256 : v;}
std::int32_t Memory::s16(std::uint32_t a) const {auto v=u16(a);return v>=32768 ? int(v)-65536 : v;}
std::int32_t Memory::s32(std::uint32_t a) const {return amx::s32(word(a));}
std::string Memory::cstr(std::uint32_t address,std::size_t limit) const {
  std::string result;
  for(std::size_t i=0;i<limit;++i) {
    auto value=read(address++);
    if(!value) return result;
    result.push_back(static_cast<char>(value));
  }
  throw amx::HostFault("unterminated memory string");
}
void Memory::write(std::uint32_t address,const std::vector<std::uint8_t> &data) {
  if(data.empty()) return;
  if (canonical) {
    writes.emplace_back(address, data.size());
    // The original store wrapped a write crossing 0xffffffff to address 0.
    auto head = std::min<std::size_t>(data.size(), UINT64_C(0x100000000) - address);
    try {
      canonical->write_host_bytes(address, {data.begin(), data.begin() + head});
      if (head < data.size())
        canonical->write_host_bytes(0, {data.begin() + head, data.end()});
    } catch (const creature_hp::Error &error) {
      throw amx::HostFault(error.what());
    }
    return;
  }
  writes.emplace_back(address,data.size());
  std::size_t pos=0;
  while(pos<data.size()) {
    auto &dest=ensure_page(address>>12);
    auto offset=address & 4095;
    auto take=std::min(data.size()-pos,std::size_t(4096-offset));
    if(dest.size()==4096) {
      for(std::size_t i=0;i<take;++i) {
        auto value=data[pos+i];dest[offset+i]=value;
        if(value) bytes[address+std::uint32_t(i)]=value;else bytes.erase(address+std::uint32_t(i));
      }
    } else {
      auto begin=std::min(std::size_t(offset),dest.size());
      auto end=std::min(std::size_t(offset)+take,dest.size());
      dest.erase(dest.begin()+begin,dest.begin()+end);
      dest.insert(dest.begin()+begin,data.begin()+pos,data.begin()+pos+take);
      auto base=address & 0xfffff000u;
      for(std::size_t i=0;i<dest.size();++i)
        if(dest[i]) bytes[base+std::uint32_t(i)]=dest[i];else bytes.erase(base+std::uint32_t(i));
    }
    address+=std::uint32_t(take);pos+=take;
  }
}
void Memory::byte(std::uint32_t a,std::uint8_t v) {write(a,{v});}
std::uint32_t Memory::word(std::uint32_t a) const {
  std::uint32_t value=0;
  auto data=read(a,4);
  for(std::size_t i=0;i<data.size();++i) value|=std::uint32_t(data[i])<<(8*i);
  return value;
}
void Memory::word(std::uint32_t a,std::uint32_t v) {write(a,{std::uint8_t(v),std::uint8_t(v>>8),std::uint8_t(v>>16),std::uint8_t(v>>24)});}
void Memory::string(std::uint32_t a,const std::string &text) {
  std::vector<std::uint8_t> data(text.begin(),text.end());data.push_back(0);write(a,data);
}
Global::Global(std::uint32_t address,std::string type,std::string doc)
  :addr(address),kind(std::move(type)),size(0),documentation(std::move(doc)) {
  if(kind!="u8" && kind!="s8" && kind!="u16" && kind!="s16" && kind!="u32" && kind!="s32")
    throw amx::HostFault("unsupported global kind");
  size=std::stoul(kind.substr(1))/8;
}
std::int64_t Global::get(const Memory &memory) const {
  std::uint64_t value=0;
  auto data=memory.read(addr,size);
  for(std::size_t i=0;i<data.size();++i) value|=std::uint64_t(data[i])<<(8*i);
  if(kind[0]=='s' && (value & (std::uint64_t(1)<<(8*size-1))))
    return std::int64_t(value)-std::int64_t(std::uint64_t(1)<<(8*size));
  return std::int64_t(value);
}
void Global::set(Memory &memory,const Memory::Integer &value) const {memory.store(addr,size,value);}
void Global::set(Memory &memory,std::uint64_t value) const {memory.store(addr,size,value);}
std::pair<std::uint32_t, bool> Variables::slot(std::uint32_t address,
                                             const std::string &name) const {
  // Authored traversal guard, not a claimed original table capacity.
  for (unsigned i = 0; i < 1024 * 1024; ++i) {
    if (!memory.read(address))
      return {address, false};
    auto entry = memory.cstr(address);
    if (entry == name)
      return {address, true};
    address += static_cast<std::uint32_t>(entry.size() + 2);
  }
  throw amx::ResourceLimit("variable table traversal limit");
}
amx::Cell Variables::get(std::uint32_t table, const std::string &input) {
  auto name = lower(input);
  auto found = slot(table, name);
  auto value_address = found.first + static_cast<std::uint32_t>(name.size()) + 1;
  if (!found.second) {
    memory.string(found.first, name);
    memory.byte(value_address, 0);
    return 0;
  }
  return signed_byte(memory.read(value_address));
}
amx::Cell Variables::quest_get(const std::string &name) { return get(QUEST_VARS, name); }
void Variables::quest_set(const std::string &input, amx::Cell value) {
  auto name = lower(input);
  auto found = slot(QUEST_VARS, name);
  if (!found.second)
    memory.string(found.first, name);
  memory.byte(found.first + static_cast<std::uint32_t>(name.size()) + 1,
              static_cast<std::uint8_t>(clamp(value)));
}
void Variables::quest_clear_all() {
  auto address = QUEST_VARS;
  for (unsigned i = 0; i < 1024 * 1024; ++i) {
    if (!memory.read(address))
      return;
    auto length = static_cast<std::uint32_t>(memory.cstr(address).size());
    memory.byte(address + length + 1, 0);
    address += length + 2;
  }
  throw amx::ResourceLimit("variable table traversal limit");
}
std::optional<std::pair<std::string, amx::Cell>>
Variables::quest_by_index(amx::Address index) const {
  auto address = QUEST_VARS;
  for (unsigned i = 0; i < 1024 * 1024; ++i) {
    if (!memory.read(address))
      return std::nullopt;
    auto name = memory.cstr(address);
    if (index == 0)
      return std::make_pair(name, signed_byte(memory.read(
          address + static_cast<std::uint32_t>(name.size()) + 1)));
    // Avoid signed overflow for an invalid, negative host-sized index.
    if (index > 0)
      --index;
    address += static_cast<std::uint32_t>(name.size() + 2);
  }
  throw amx::ResourceLimit("variable table traversal limit");
}
amx::Cell Variables::profile_get(const std::string &name) { return get(PROFILE_VARS, name); }
void Variables::profile_set(const std::string &input, amx::Cell value, bool save) {
  value = clamp(value);
  auto name = lower(input);
  auto found = slot(PROFILE_VARS, name);
  auto address = found.first + static_cast<std::uint32_t>(name.size()) + 1;
  if (found.second && signed_byte(memory.read(address)) == value)
    return;
  if (!found.second)
    memory.string(found.first, name);
  memory.byte(address, static_cast<std::uint8_t>(value));
  if (save) {
    // Retain the active callable if a reentrant callback replaces its binding.
    auto callback = request_config_save;
    if (!callback)
      throw amx::HostFault("memcard_requestConfigSave callback required");
    callback();
  }
}
std::uint32_t Variables::player_record(amx::Cell slot) const {
  return 0x4d8af0u + std::uint32_t(slot)*0x4c30u;
}
std::uint32_t Variables::mission_byte(amx::Cell slot, amx::Cell difficulty, amx::Cell mission) const {
  return player_record(slot)+0x4bbfu+std::uint32_t(difficulty)*13u+std::uint32_t(mission);
}
amx::Cell Variables::mission_available(amx::Cell d, amx::Cell m) const {
  return m<13 ? memory.read(mission_byte(0,d,m)) & 1 : 0;
}
amx::Cell Variables::mission_complete(amx::Cell d, amx::Cell m) const {
  return m<13 ? (memory.read(mission_byte(0,d,m))>>1) & 1 : 0;
}
void Variables::mission_set_available(amx::Cell d, amx::Cell m, amx::Cell on) {
  if (m>=13) return;
  const auto players=amx::s32(memory.word(NUM_PLAYERS));
  for (amx::Cell slot=0; slot<players; ++slot) {
    auto where=mission_byte(slot,d,m);
    memory.byte(where, (memory.read(where)&0xfe) | (std::uint32_t(on)&1));
  }
}
void Variables::mission_set_complete(amx::Cell d, amx::Cell m, amx::Cell value) {
  if (m>=13) return;
  for (amx::Cell slot=0; slot<4; ++slot) {
    auto where=mission_byte(slot,d,m);
    auto byte=memory.read(where);
    memory.byte(where, value==-1 ? byte|2 : (byte&0xfd)|((std::uint32_t(value)&1)<<1));
  }
}
void Variables::mission_grant_medal(amx::Cell m, amx::Cell level) {
  if (m>=13) return;
  for (amx::Cell slot=0; slot<amx::s32(memory.word(NUM_PLAYERS)); ++slot) {
    auto where=player_record(slot)+0x4c0du+std::uint32_t(m);
    std::int64_t have=signed_byte(memory.read(where));
    if (have>=level) continue;
    memory.byte(0x4f7149, std::uint8_t(slot));
    do {
      auto callback=run_reward_script;
      if (!callback) throw amx::HostFault("script_runByName callback required");
      callback(have==0 ? "firstBronze" : have==1 ? "firstSilver" : "firstGold");
      ++have;
    } while (have<level);
    memory.byte(where, std::uint8_t(level));
  }
}
amx::Cell Variables::mission_get_medal(amx::Cell m, amx::Cell level, amx::Cell flag) {
  amx::Cell slot=0;
  if (flag==0 || memory.read(0x402791)==0) {
    auto callback=local_player_slot;
    if (!callback) throw amx::HostFault("net_getLocalPlayerSlot callback required");
    slot=callback();
  }
  if (m>=13) return 0;
  auto medal=signed_byte(memory.read(player_record(slot)+0x4c0du+std::uint32_t(m)));
  return level==-1 ? medal : medal>=level;
}
void Variables::quest_add(const std::string &title, const std::string &text) {
  for (amx::Cell slot=0; slot<amx::s32(memory.word(NUM_PLAYERS)); ++slot) {
    auto rec=player_record(slot);
    auto count=amx::s32(memory.word(rec+0x43d0));
    if (count>7) return;
    for (amx::Cell i=0; i<count; ++i)
      if (memory.cstr(rec+0x41d0+std::uint32_t(i)*0x40)==title) return;
    auto entry=rec+0x41d0+std::uint32_t(count)*0x40;
    memory.string(entry,title);
    memory.string(entry+0x20,text);
    memory.word(rec+0x43d0,std::uint32_t(count)+1);
  }
}
void Variables::quest_remove(const std::string &title) {
  const auto players=amx::s32(memory.word(NUM_PLAYERS));
  for (amx::Cell slot=0; slot<players; ++slot) {
    auto rec=player_record(slot);
    amx::Cell last=-1;
    for (amx::Cell i=0; i<amx::s32(memory.word(rec+0x43d0)); ++i) {
      auto entry=rec+0x41d0+std::uint32_t(i)*0x40;
      if (memory.cstr(entry)==title) {
        // The placeholder already includes its terminator: exactly nine bytes.
        memory.string(entry,"NoQuests"); memory.string(entry+0x20,"NoQuests"); last=i;
      }
    }
    if (last==-1) continue;
    while (std::int64_t(last)<std::int64_t(amx::s32(memory.word(rec+0x43d0)))-1) {
      auto entry=rec+0x41d0+std::uint32_t(last)*0x40;
      memory.string(entry,memory.cstr(entry+0x40));
      memory.string(entry+0x20,memory.cstr(entry+0x60));
      ++last;
    }
    memory.word(rec+0x43d0,memory.word(rec+0x43d0)-1);
  }
}
amx::Cell Variables::choice_get(std::uint32_t offset) const {
  auto mask=1u<<(memory.word(DIFFICULTY)&31);
  return memory.read(player_record(0)+offset)&mask;
}
void Variables::choice_set(std::uint32_t offset, amx::Cell on) {
  auto mask=std::uint8_t(1u<<(memory.word(DIFFICULTY)&31));
  const auto players=amx::s32(memory.word(NUM_PLAYERS));
  for (amx::Cell slot=0; slot<players; ++slot) {
    auto where=player_record(slot)+offset;
    auto byte=memory.read(where);
    memory.byte(where,on!=0 ? byte|mask : byte&~mask);
  }
}
std::uint32_t Variables::service(const std::string &name, const std::vector<std::uint32_t> &args) {
  auto callback=player_service;
  if (!callback) throw amx::HostFault(name+" callback required");
  return callback(name,args);
}
void Variables::player_set_flag(amx::Cell slot, amx::Cell bit, amx::Cell on) {
  auto mask=std::uint8_t(1u<<(std::uint32_t(bit)&31));
  const auto count=amx::s32(memory.word(NUM_PLAYERS));
  auto set=[&](amx::Cell i) {
    auto where=player_record(i)+0x4c28;
    auto byte=memory.read(where);
    memory.byte(where,on ? byte|mask : byte&~mask);
  };
  if (slot==-1) { for(amx::Cell i=0;i<count;++i) set(i); }
  else set(slot);
}
amx::Cell Variables::player_get_flag(amx::Cell slot, amx::Cell bit) const {
  return memory.read(player_record(slot)+0x4c28)&(1u<<(std::uint32_t(bit)&31));
}
amx::Cell Variables::count_players_with_hp() const {
  const auto count=amx::s32(memory.word(NUM_PLAYERS));
  amx::Cell result=0;
  for(amx::Cell i=0;i<count;++i) if(lt(0,memory.word(player_record(i)+0xac))) ++result;
  return result;
}
void Variables::refill(bool living) {
  for(amx::Cell i=0;i<amx::s32(memory.word(NUM_PLAYERS));++i) {
    auto rec=player_record(i);
    if(!living || lt(0,memory.word(rec+0xac))) memory.word(rec+0xac,memory.word(rec+0xb0));
  }
}
amx::Cell Variables::player_dispatch(const std::string &name, amx::Machine &m) {
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto count=[&]() {return amx::s32(memory.word(NUM_PLAYERS));};
  auto half=[&](std::uint32_t a) {return s16(memory.read(a)|(std::uint32_t(memory.read(a+1))<<8));};
  if(name=="getNumPlayers") return count();
  if(name=="countPlayersAlive") return amx::s32(memory.word(0x4f8420));
  if(name=="checkPlayerAlive") return arg(0) ? half(0x4f7194)>0 : count_players_with_hp();
  if(name=="activePlayerIsDwarf") return memory.word(player_record(amx::s32(memory.word(0x4d8ae4)))+0x54)==0;
  if(name=="activePlayerIsElf") return 0;
  if(name=="isClericOrWizard") {
    auto slot=std::uint32_t(arg(0));
    if(slot<4 && std::int64_t(slot)<=count()) {
      auto cls=memory.word(player_record(amx::Cell(slot))+0x50); return cls==1 || cls==4;
    }
    return 0;
  }
  if(name=="getMaxPlayerHP") {
    auto slot=amx::s32(std::uint32_t(arg(0))-1);
    return slot<0 || slot>=count() ? -1 : to_int(memory.word(player_record(slot)+0xb0));
  }
  if(name=="healPlayer") {
    auto callback=find_resource;
    if(!callback) throw amx::HostFault("res_find callback required");
    auto lump=memory.cstr(0x531398), sound=memory.cstr(0x5313a0);
    auto resource=callback(lump,sound);
    service("sound_playOnObject",{resource,memory.word(player_record(0)+0x43d8),ONE});
    refill(true); return 0;
  }
  if(name=="giveFullHealth") {refill(false);return 0;}
  if(name=="zeroPlayerMana" || name=="refillPlayerMana" || name=="givePlayerExp" ||
     name=="givePlayerGold" || name=="takePlayerGold") {
    for(amx::Cell i=0;i<count();++i) {
      auto rec=player_record(i);
      if(name=="zeroPlayerMana") memory.word(rec+0xc0,0);
      else if(name=="refillPlayerMana") memory.word(rec+0xc0,memory.word(rec+0xc4));
      else if(name=="givePlayerExp") service("player_giveExp",{std::uint32_t(i),std::uint32_t(arg(0))});
      else {
        // Widen before division; signed32 overflow occurs only at the result store.
        auto amount=arg(0); auto divisor=count();
        if(!divisor) throw amx::HostFault("zero player count in gold division");
        auto share=std::int64_t(amount)/divisor;
        auto old=memory.word(rec+0xe4);
        auto gold=amx::s32(name=="givePlayerGold" ? old+std::uint32_t(share) : old-std::uint32_t(share));
        if(name=="givePlayerGold") {
          memory.word(rec+0xe4,std::uint32_t(gold));
          if(gold>99999999) memory.word(rec+0xe4,99999999);
        } else memory.word(rec+0xe4,std::uint32_t(std::max(gold,0)));
      }
    }
    return 0;
  }
  if(name=="checkPlayerGold") {
    std::uint32_t total=0;
    for(amx::Cell i=0;i<count();++i) total+=memory.word(player_record(i)+0xe4);
    return amx::s32(total);
  }
  if(name=="checkPlayerMinGold") {
    auto least=amx::s32(memory.word(player_record(0)+0xe4));
    for(amx::Cell i=1;i<count();++i) least=std::min(least,amx::s32(memory.word(player_record(i)+0xe4)));
    return least;
  }
  if(name=="playerAdjustStat") {
    auto player=arg(0),stat=arg(1),points=arg(2);
    auto reward=memory.read(0x4f7149);
    if(std::uint32_t(stat)-4<5) for(amx::Cell i=0;i<points;++i)
      service(player==-1 ? "inventory_onStatPoint" : "invmsg_sendStatPoint",
              {player==-1 ? std::uint32_t(reward) : std::uint32_t(player),std::uint32_t(stat)});
    return 0;
  }
  if(name=="setPlayerFlag") {auto slot=arg(0),bit=arg(1),on=arg(2);player_set_flag(slot,bit,on);return 0;}
  if(name=="getPlayerFlag") {auto slot=arg(0),bit=arg(1);return player_get_flag(slot,bit)!=0;}
  if(name=="piercePlayerEar") {
    auto slot=std::uint32_t(arg(0));
    if(slot==UINT32_MAX) for(amx::Cell i=0;i<count();++i) memory.byte(player_record(i)+0x4c26,1);
    else if(slot<4) memory.byte(player_record(amx::Cell(slot))+0x4c26,1);
    return 0;
  }
  if(name=="clearAllSkills") {
    auto rec=player_record(memory.read(0x4f7148));
    memory.write(rec+0x128,std::vector<std::uint8_t>(8,0));
    for(unsigned k=0;k<48;++k) memory.byte(rec+0xef+k,0);
    for(unsigned k=0;k<48;++k) {
      memory.write(rec+0x4b08+4*k,{255,255}); memory.write(rec+0x4b0a+4*k,{255,255});
    }
    memory.word(rec+0xe8,0);memory.byte(rec+0x4a85,1);memory.word(rec+0x4a80,0);memory.byte(rec+0x4a84,0);
    for(unsigned k=0;k<15;++k) {
      memory.write(rec+0x4460+0x68*k,{255,255});memory.byte(rec+0x4464+0x68*k,0);
    }
    return 0;
  }
  if(name=="hidePlayers") {memory.byte(0x4f83f8,std::uint8_t(arg(0)));return 0;}
  if(name=="decayPlayerLights") {
    Fpu fpu; // pinned default R5900 arithmetic, independent of ambient context
    auto target=fpu.mul(fpu.from_int(std::uint32_t(arg(memory.word(0x4f8420)==2 ? 1 : 0))),0x3c23d70a);
    auto step=fpu.mul(fpu.from_int(std::uint32_t(arg(2))),0x3c23d70a);
    auto value=fpu.add(memory.word(0x4f83f4),step);memory.word(0x4f83f4,value);
    if((lt(0,step)&&le(target,value)) || (lt(step,0)&&le(value,target))) {memory.word(0x4f83f4,target);return 1;}
    return 0;
  }
  if(name=="getManaUsed") return half(0x4f85c8);
  if(name=="getHealthUsed") return half(0x4f85ca);
  throw amx::HostFault("unsupported player-record handler");
}
std::string Variables::string_argument(amx::Machine &machine, amx::Address offset,
                                      std::optional<amx::Cell> &slot) {
  auto address=argument(machine,offset);
  if (machine.get_addr(address)==amx::ERR_NONE) slot=address;
  else if (!slot) throw StackGarbage("unknown handler string slot");
  auto text=machine.get_string(*slot);
  if (text.size()>=160) throw amx::HostFault("string overruns 160-byte handler frame");
  return text;
}
namespace {
std::optional<unsigned> argument_slot_number(const std::string &name, amx::Address k) {
  const bool slot20=name=="callScript" || name=="addDeferedCall" || name=="postAction" || name=="print" || name=="startMusic" || name=="startAudio" || name=="removeQuest" || name=="getv" || name=="setv" ||
    name=="addv" || name=="getpv" || name=="setpv" || name=="newWorld" ||
    name=="newWorldAlways" || name=="simpleNewWorld" || name=="newWorldPost" ||
    name=="setEntrance" || name=="preloadResource" || name=="setInfo" ||
    name=="closestPlayerDist" || name=="stopPropAnim" || name=="propAnimAtEnd" || name=="setNoCollide" ||
    name=="controlVehicle" || name=="holdVehicle" || name=="getVPosition" ||
    name=="onePlayerHasItem" || name=="countQuestItems" || name=="NPCAnimAtEnd" || name=="setHitPoints";
  if(slot20) return 0x20;
  if(name=="sound" || name=="soundSequence" || name=="addQuest" || name=="addHelpMessage" || name=="playerHasItem" || name=="removePlayerItem" || name=="findDistance" || name=="putNPCInVehicle") return 0x30;
  if(name=="startDialog") return 0x80;
  if(name=="cutScene" || name=="givePlayerItem") return 0xd0;
  if(name=="particleMorph") {
    if(k==0) return 0x10f0;
    if(k==1) return 0x840;
    return {};
  }
  static const std::map<std::string,unsigned> menu_slots={{"showBust",0x40},{"showCompass",0x50},
    {"hitPlayers",0x60},{"putPlayersInVehicle",0x40},{"entityRideVehicle",0x70},
    {"lightEffect",0x40},{"startPropAnim",0x40},{"addExpandingPool",0x40},
    {"addParticleOnObject",0x50},{"deleteObjects",0xa0},{"objInPlayerTrig",0x110},
    {"autoPropSetState",0x1c0},{"autoPropGetState",0x1c0},{"trigger",0x1d0},
    {"setContainerProps",0x840},
    {"selectDropItem",0x40},{"dropItem",0x60},{"giveOnePlayerItem",0xb0},
    {"countLiveObjects",0x220},
    {"increaseEnemyRank",0x820},{"unRootMonster",0x820},{"stopFollowingPlayer",0x820},
    {"friendly",0x820},{"unFriendly",0x820},{"getHitPoints",0x820},
    {"charmMonster",0x830},{"rootMonster",0x830},{"setMonsterVal",0x830},
    {"setCFlag",0x830},{"killMonster",0x830},{"deleteMonster",0x830},
    {"hideMonster",0x830},{"showMonster",0x830},{"followPlayer",0x830},
    {"followPath",0x830},{"stopFollowingPath",0x830},{"setCreatureParams",0x830},
    {"setOFlag",0x840},{"setWeaponType",0x840},{"setMonsterRange",0x840},{"moveMonster",0x840},
    {"monstersInTrigger",0x840},{"startNPCAnim",0x850},{"generateMonster",0x850},
    {"runMedalSuccess",0x70},{"runMedalFail",0xc0},{"showOnMap",0x1c0},{"addHelpMessageF",0x440}};
  auto it=menu_slots.find(name);
  return it==menu_slots.end() ? std::optional<unsigned>{} : it->second;
}
amx::Address checked_address(amx::Address base, amx::Address index) {
  if(index > (INT64_MAX-4)/4 || index < INT64_MIN/4)
    throw amx::HostFault("argument index exceeds native address domain");
  const auto delta=index*4;
  if((delta>0 && base>INT64_MAX-delta) || (delta<0 && base<INT64_MIN-delta))
    throw amx::HostFault("argument address exceeds native address domain");
  return base+delta;
}
}
Args::Args(amx::Machine &machine, Variables *owner, const std::string &name)
    : m(machine), base(checked_address(machine.params,1)), game(owner) {
  if(name=="particleMorph") {slot_of={{0,0x10f0},{1,0x840}};}
  else fallback=argument_slot_number(name,0);
}
amx::Cell Args::operator[](amx::Address k) const { return m.cell(checked_address(base,k)); }
std::uint32_t Args::u(amx::Address k) const {return std::uint32_t((*this)[k]);}
std::uint32_t Args::f(amx::Address k) const {return rta::from_int((*this)[k]);}
amx::Address Args::address(amx::Address k) {
  const auto value=(*this)[k];
  auto found=slot_of.find(k);
  auto slot=found==slot_of.end() ? fallback : std::optional<unsigned>(found->second);
  if(m.get_addr(value)==0) {
    if(game && slot) game->argument_slot(*slot,value);
    return value;
  }
  if(game && slot) {auto saved=game->argument_slot(*slot);if(saved) return *saved;}
  throw StackGarbage("refused argument address has no initialized handler slot");
}
std::string Args::s(amx::Address k, amx::Address room) {
  auto text=m.get_string(address(k));
  if(room<=0 || std::uint64_t(text.size())>=std::uint64_t(room))
    throw amx::HostFault("string argument overruns handler stack buffer");
  return text;
}
std::optional<amx::Cell> Variables::argument_slot(unsigned slot) const {
  if(slot==0x20) return string_slot;
  if(slot==0x30) return quest_string_slot;
  if(slot==0x80) return dialog_string_slot;
  if(slot==0xd0) return cutscene_string_slot;
  auto it=menu_string_slots.find(slot);
  return it==menu_string_slots.end() ? std::optional<amx::Cell>{} : it->second;
}
void Variables::argument_slot(unsigned slot, amx::Cell value) {
  if(slot==0x20) string_slot=value;
  else if(slot==0x30) quest_string_slot=value;
  else if(slot==0x80) dialog_string_slot=value;
  else if(slot==0xd0) cutscene_string_slot=value;
  else menu_string_slots[slot]=value;
}
void Variables::preserve_argument_slots(const std::string &name) {
  const auto first=argument_slot_number(name,0), second=argument_slot_number(name,1);
  auto keep=[&](unsigned slot) {return first==slot || second==slot;};
  if(!keep(0x20)) string_slot.reset();
  if(!keep(0x30)) quest_string_slot.reset();
  if(!keep(0x80)) dialog_string_slot.reset();
  if(!keep(0xd0)) cutscene_string_slot.reset();
  for(auto it=menu_string_slots.begin();it!=menu_string_slots.end();)
    if(!keep(it->first)) it=menu_string_slots.erase(it); else ++it;
}
amx::Native Variables::bind(const std::string &name, Method method) {
  return [this,name,method=std::move(method)](amx::Machine &m,const std::vector<amx::Cell> &)->std::optional<amx::Address> {
    preserve_argument_slots(name);
    Args args(m,this,name);
    auto active=method;
    return active(m,args);
  };
}
amx::Native Variables::missing(std::string name) {
  return [name=std::move(name)](amx::Machine &,const std::vector<amx::Cell> &)->std::optional<amx::Address> {
    throw amx::HostFault("built-in "+name+" is not modelled");
  };
}

amx::Cell Variables::dispatch(const std::string &name, amx::Machine &machine) {
  if (!supported(name))
    throw amx::HostFault("unsupported RTA script built-in: " + name);
  preserve_argument_slots(name);
  if(std::find(script_names.begin(),script_names.end(),name)!=script_names.end()) {
    auto text=string_argument(machine,4,string_slot);
    if(name=="callScript") return amx::s32(run_script(text));
    if(name=="addDeferedCall") queue_deferred(text);
    else set_post_action(text);
    return 0;
  }
  if(std::find(utility_names.begin(),utility_names.end(),name)!=utility_names.end()) return utility_dispatch(name,machine);
  if(std::find(interaction_names.begin(),interaction_names.end(),name)!=interaction_names.end()) return interaction_dispatch(name,machine);
  if(std::find(vehicle_names.begin(),vehicle_names.end(),name)!=vehicle_names.end()) return vehicle_dispatch(name,machine);
  if(std::find(effects_names.begin(),effects_names.end(),name)!=effects_names.end()) return effects_dispatch(name,machine);
  if(std::find(inventory_names.begin(),inventory_names.end(),name)!=inventory_names.end()) return inventory_dispatch(name,machine);
  if(std::find(creature_names.begin(),creature_names.end(),name)!=creature_names.end()) return creature_dispatch(name,machine);
  if(std::find(menus.begin(),menus.end(),name)!=menus.end()) return menu_dispatch(name,machine);
  if(std::find(scene.begin(),scene.end(),name)!=scene.end()) return scene_dispatch(name,machine);
  if (std::find(players.begin(),players.end(),name)!=players.end()) return player_dispatch(name,machine);
  auto difficulty=amx::s32(memory.word(DIFFICULTY));
  if (name=="getEvil") return choice_get(0x4c1a);
  if (name=="getMadeDecision") return choice_get(0x4c1b);
  if (name=="setEvil") { auto on=argument(machine,4); if(on) choice_set(0x4c1a,on); return 0; }
  if (name=="setMadeDecision") { choice_set(0x4c1b,1); return 0; }
  if (name=="grantMission") { mission_set_complete(difficulty,argument(machine,4),-1); return 0; }
  if (name=="getMissionComplete") return mission_complete(difficulty,argument(machine,4));
  if (name=="getMissionAvailable") return mission_available(difficulty,argument(machine,4));
  if (name=="setMissionAvailable") {
    auto mission=argument(machine,4); auto on=argument(machine,8);
    mission_set_available(difficulty,mission,on); return 0;
  }
  if (name=="grantGold" || name=="grantSilver" || name=="grantBronze") {
    mission_grant_medal(argument(machine,4),name=="grantGold" ? 3 : name=="grantSilver" ? 2 : 1); return 0;
  }
  if (name=="getMedal") return mission_get_medal(argument(machine,4),-1,0);
  if (name=="addQuest") {
    auto title=string_argument(machine,4,quest_string_slot);
    auto text=string_argument(machine,8,quest_string_slot);
    quest_add(title,text); return 0;
  }
  if (name=="removeQuest") { quest_remove(string_argument(machine,4,string_slot)); return 0; }
  // All five handlers share stack pointer slot 0x20. The count and callback's
  // argument vector are deliberately ignored: read the live parameter cells.
  auto address = argument(machine, 4);
  if (machine.get_addr(address) == amx::ERR_NONE)
    string_slot = address;
  else if (!string_slot)
    throw StackGarbage("refused string address with unknown handler stack slot");
  auto text = machine.get_string(*string_slot);
  if (text.size() >= 160)
    throw amx::HostFault("string overruns 160-byte handler frame");
  if (name == "getv")
    return quest_get(text);
  if (name == "getpv")
    return profile_get(text);
  if (name == "addv") {
    auto previous = quest_get(text);
    auto value = argument(machine, 8);
    auto total = amx::s32(static_cast<std::uint32_t>(previous) +
                         std::uint64_t(static_cast<std::uint32_t>(value)));
    quest_set(text, total);
    return total;
  }
  auto value = argument(machine, 8);
  if (name == "setv") {
    quest_set(text, value);
    return 0;
  }
  if (name == "setpv") {
    profile_set(text, value, true);
    return 0;
  }
  throw amx::HostFault("unsupported RTA script built-in");
}

std::uint32_t Variables::scene_call(const std::string &name, const std::vector<ServiceValue> &args) {
  auto callback=scene_service;
  if(!callback) throw amx::HostFault(name+" scene callback required");
  return callback(name,args);
}
const std::vector<std::string> *Variables::selected_properties() const {
  auto address=memory.word(0x4039f8);
  if(!address) return nullptr;
  auto it=object_properties.find(address);
  if(it==object_properties.end()) throw amx::HostFault("selected address is not a known object");
  if(!it->second) throw amx::HostFault("selected object has no property list");
  return &*it->second;
}
amx::Cell Variables::net_gate() {
  if(memory.read(0x402791) && memory.word(0x4d8ae4)!=UINT32_MAX) {
    auto slot=memory.word(0x4d8ae4);
    auto cooldown=player_record(amx::s32(slot))+0x4bca;
    if((memory.read(cooldown) || memory.read(cooldown+1)) && amx::s32(memory.word(0x4f8420))>=2) {
      if(scene_call("net_isLocalSlot",{slot})) {
        auto text=scene_call("text_lookup",{memory.cstr(0x531280)});
        auto callback=local_player_slot;
        if(!callback) throw amx::HostFault("net_getLocalPlayerSlot callback required");
        auto local=std::uint32_t(callback());
        scene_call("hud_showMessage",{text,180u,local,0u});
      }
      return 1;
    }
    memory.write(cooldown,{0x58,0x02});
  }
  return 0;
}
void Variables::set_next_world(const std::string &name) {
  if(name.size()>=0x70) throw amx::HostFault("level name overruns local buffer");
  memory.string(0x41b6d0,name);
  bool found=false;
  auto props=selected_properties();
  if(props) for(const auto &entry:*props) if(entry.compare(0,12,"orientation=")==0) {
    found=true;
    std::size_t i=12;
    while(i<entry.size() && std::string(" \t\n\v\f\r").find(entry[i])!=std::string::npos) ++i;
    bool negative=false;
    if(i<entry.size() && (entry[i]=='+' || entry[i]=='-')) negative=entry[i++]=='-';
    std::uint32_t value=0;
    while(i<entry.size() && entry[i]>='0' && entry[i]<='9') value=value*10+unsigned(entry[i++]-'0');
    memory.byte(0x41b720,std::uint8_t(negative ? 0u-value : value));
  }
  if(!found) {
    if(name.size()+17>=0x70) throw amx::HostFault("level debug suffix overruns local buffer");
    memory.byte(0x41b720,255);
  }
}
void Variables::dialog_start(const std::string &name, amx::Cell a, amx::Cell b) {
  auto camera=memory.word(0x4f8450);
  scene_call("Camera_update",{camera});
  memory.word(0x4fdce8,0);
  if(scene_call("net_isLocalSlot",{memory.word(0x4d8ae4)}) && !memory.word(0x8e2d60)) {
    std::string packet;
    packet.push_back(0);packet.push_back(char(name.size()));packet+=name;
    while(packet.size()&3) packet.push_back(char(255));
    packet.push_back(char(memory.word(0x4d8ae4)));packet.push_back(char(a));packet.push_back(char(b));packet.push_back(0);
    for(unsigned i=0;i<12;++i) packet.push_back(char(memory.read(0x44f038+i)));
    auto yaw=scene_call("Camera_getYaw",{camera});
    packet.push_back(char(yaw));packet.push_back(char(yaw>>8));
    auto pitch=scene_call("Camera_getPitch",{camera});
    packet.push_back(char(pitch));packet.push_back(char(pitch>>8));
    scene_call("net_sendGameMsg",{memory.word(0x8e2e94),packet});
  }
}
amx::Cell Variables::new_world_no_gate(amx::Machine &m) {
  auto name=string_argument(m,4,string_slot);
  scene_call("net_closeLocalMenus",{0u});set_next_world(name);return 0;
}
amx::Cell Variables::scene_dispatch(const std::string &name, amx::Machine &m) {
  auto arg=[&](unsigned k) {return std::uint32_t(argument(m,4+4*k));};
  auto str=[&]() {return string_argument(m,4,string_slot);};
  auto count=[&]() {return amx::s32(memory.word(NUM_PLAYERS));};
  if(name=="newWorld" || name=="newWorldAlways" || name=="simpleNewWorld") {
    auto level=str();
    if(memory.read(0x4f8698) || !net_gate()) {
      scene_call("net_closeLocalMenus",{0u});
      if(name=="simpleNewWorld") {memory.byte(0x41b720,255);memory.string(0x41b6d0,level);}
      else set_next_world(level);
    }
  } else if(name=="newWorldPost") memory.string(0x4fdcf0,str());
  else if(name=="setEntrance") memory.string(0x4f8468,str());
  else if(name=="newWorldParameter") {
    auto props=selected_properties();
    if(!props) throw amx::HostFault("newWorldParameter needs a selected object");
    const auto entries=*props; // retain the old list across callback replacement
    for(const auto &entry:entries) if(entry.compare(0,6,"level=")==0) {
      auto level=entry.substr(6);scene_call("net_closeLocalMenus",{0u});set_next_world(level);break;
    }
  } else if(name=="cutScene") {
    auto text=string_argument(m,4,cutscene_string_slot);
    auto lump=memory.cstr(0x41b721)+".lmp", cut=text+".cut";
    auto callback=find_resource;
    if(!callback) throw amx::HostFault("res_find callback required");
    auto resource=callback(lump,cut);scene_call("cut_start",{resource,cut});
  } else if(name=="startDialog") {
    auto text=string_argument(m,4,dialog_string_slot);
    scene_call("lump_load",{memory.cstr(0x41b721)+".lmp"});
    auto a=arg(1),b=arg(2);dialog_start(text,amx::s32(a),amx::s32(b));
  } else if(name=="dialogRunning") return amx::s32(scene_call("dialog_isOpen"));
  else if(name=="forceMutexRelease") scene_call("menuMutex_release");
  else if(name=="changeSwoop") memory.word(0x4fdce4,arg(0));
  else if(name=="startWithSwoopDone") memory.word(0x4fdce8,1);
  else if(name=="leaveGame") memory.word(0x41b508,arg(0)==0 ? 1 : 0x65);
  else if(name=="endGame") scene_call("memcard_requestEndGameSave",{0u,1u});
  else if(name=="exitToMainMenu") memory.word(0x41b508,1);
  else if(name=="saveGame") {
    for(amx::Cell i=0;i<count();++i) if((memory.word(player_record(i)+0x4c1c)&31)==2) return 0;
    auto callback=local_player_slot;
    if(!callback) throw amx::HostFault("net_getLocalPlayerSlot callback required");
    auto local=callback();scene_call("memcard_requestSave",{std::uint32_t(local)});
  } else if(name=="saveEndConfig") {
    scene_call("game_sendCharacterToLobby",{UINT32_MAX});
    memory.byte(0x4d8a08,0);memory.byte(0x4039c0,0);memory.word(0x41b4f8,1);
    memory.byte(0x4f85c5,1);memory.word(0x41b4f4,1);
    if(memory.read(0x402791)) scene_call("net_dropAllRemotes");
    auto difficulty=amx::s32(memory.word(DIFFICULTY));
    if(difficulty>=1 && difficulty<=4) memory.word(0x4f8f38,memory.word(0x4f8f38)|(4u<<difficulty));
    auto callback=request_config_save;
    if(!callback) throw amx::HostFault("memcard_requestConfigSave callback required");
    callback();
  } else if(name=="saveDone") return scene_call("memcard_getState")==UINT32_MAX;
  else if(name=="saveCheckPoint") {scene_call("dbg_printf",{0x531250u});scene_call("save_game",{0u,0u,1u});}
  else if(name=="loadCheckPoint") scene_call("deathTask_schedule",{0u});
  else if(name=="setSaveInhibit") memory.word(0x50605c,arg(0));
  else if(name=="getSaveInhibit") return amx::s32(memory.word(0x50605c));
  else if(name=="setAlternateDeath") memory.word(0x4f712c,1);
  else if(name=="preloadResource") scene_call("lump_load",{str()});
  else if(name=="clearRecall") {for(amx::Cell i=0;i<count();++i) memory.string(player_record(i)+0x24,"None");}
  else if(name=="playersLoaded") {for(amx::Cell i=0;i<count();++i) if(memory.word(player_record(i)+0x43d8)) return 1;}
  else if(name=="pauseGameControl") {memory.word(0x41b4fc,arg(0));memory.word(0x41b500,1);}
  else throw amx::HostFault("unsupported scene handler");
  return 0;
}

amx::Cell Variables::alive(amx::Cell index) const {
  return amx::s32(memory.word(0x4f8440+4u*std::uint32_t(index)));
}
std::vector<objects::GameObject *> Variables::find(const std::string &name, std::int64_t limit) const {
  return object_names.find(name,limit);
}
objects::GameObject *Variables::find_first(const std::string &name) const {
  return object_names.find_first(name);
}
objects::GameObject *Variables::need(objects::GameObject *obj, const std::string &what) const {
  if(!obj) throw amx::HostFault(what+": no such object");
  return obj;
}
amx::Cell Variables::medal_gate() {
  memory.word(0x4f7114,2);
  if(!memory.read(0x402791)) {
    memory.word(0x41b4f4,1);
    return scene_call("memcard_isBusy") ? 0 : 1;
  }
  auto player=memory.read(0x4f7148);
  auto callback=local_player_slot;
  if(!callback) throw amx::HostFault("net_getLocalPlayerSlot callback required");
  if(player==std::uint32_t(callback())) {
    if(scene_call("memcard_isBusy")) return 0;
    if(!memory.word(0x4f7140)) {scene_call("hudmsg_sendMedalAck");return 0;}
    return 1;
  }
  if(!memory.word(0x4f7140)) return 0;
  return scene_call("memcard_isBusy") ? 2 : 1;
}
std::uint32_t Variables::medal_prompt() {
  bool show=!memory.read(0x402791);
  if(!show) {
    auto callback=local_player_slot;
    if(!callback) throw amx::HostFault("net_getLocalPlayerSlot callback required");
    auto local=std::uint32_t(callback()); // player byte read after the callback
    show=local==memory.read(0x4f7148);
  }
  if(!show) return 0;
  auto text=scene_call("text_lookup",{memory.cstr(0x531230)});
  Fpu fpu;
  scene_call("wstr_format",{0x4f6848u,text,fpu.from_int(memory.word(0x41b978)),
    fpu.from_int(memory.word(0x41b97c)),0u,0u,0u,0u,0u,0u});
  return 0x4f6848;
}
amx::Cell Variables::medal_pad() {
  auto pressed=memory.word(0x41bfd8+0x58u*memory.read(0x4f7148)+0xc);
  if(pressed & memory.word(0x41bfbc)) {
    memory.string(0x41b6d0,memory.cstr(0x41b721));memory.byte(0x41b720,255);return 1;
  }
  if(pressed & memory.word(0x41bfb8)) {scene_call("game_openMissionMenu");return 1;}
  return 0;
}
amx::Cell Variables::menu_dispatch(const std::string &name, amx::Machine &m) {
  auto arg=[&](unsigned k) {return std::uint32_t(argument(m,4+4*k));};
  auto str=[&](unsigned k, unsigned room=160) {
    std::optional<amx::Cell> *slot=nullptr;
    unsigned key=0;
    if(name=="setInfo") slot=&string_slot;
    else if(name=="addHelpMessage") slot=&quest_string_slot;
    else key=name=="showBust" ? 0x40 : name=="showCompass" ? 0x50 :
      name=="runMedalSuccess" ? 0x70 : name=="runMedalFail" ? 0xc0 :
      name=="showOnMap" ? 0x1c0 : 0x440;
    auto address=amx::s32(arg(k));
    if(m.get_addr(address)==amx::ERR_NONE) {
      if(slot) *slot=address;else menu_string_slots[key]=address;
    } else if(slot ? !*slot : !menu_string_slots.count(key)) throw StackGarbage("unknown handler string slot");
    auto text=m.get_string(slot ? **slot : menu_string_slots.at(key));
    if(text.size()>=room) throw amx::HostFault("string overruns handler frame");
    return text;
  };
  if(name=="activateMissionMenu") {
    if(!scene_call("dialog_isOpen") && !memory.read(0x41b51d)) {
      auto player=!memory.read(0x402791) ? memory.word(0x4d8ae4) : scene_call("net_firstActiveSlot");
      scene_call("missionMenu_open",{player});
    }
  } else if(name=="activateStore") {
    if(arg(0)<21) {auto value=arg(0);memory.write(0x4f7150,{std::uint8_t(value),std::uint8_t(value>>8)});}
    if(!scene_call("dialog_isOpen")) {
      if(!memory.read(0x402791)) scene_call("shop_openOrStock",{memory.word(0x4d8ae4),0u,1u});
      else for(amx::Cell index=0;index<amx::s32(memory.word(0x4f8420));++index) {
        if(scene_call("net_isLocalSlot",{std::uint32_t(alive(index))})) {
          scene_call("net_closeLocalMenus",{0u});
          scene_call("shop_openOrStock",{std::uint32_t(alive(index)),0u,1u});
        }
      }
    } else memory.byte(0x4fdd30,1);
  } else if(name=="storeRunning") return amx::s32(memory.word(0x41b50c));
  else if(name=="setInfo") {auto key=str(0);auto text=scene_call("text_lookup",{key});scene_call("hud_showMessage",{text,180u,UINT32_MAX,0u});}
  else if(name=="addHelpMessage") {
    auto title=str(0),body=str(1);
    auto t=scene_call("text_lookup",{title});auto b=scene_call("text_lookup",{body});
    scene_call("hud_showHelpMessage",{UINT32_MAX,t,b});
  } else if(name=="addHelpMessageF") {
    auto key1=str(0),key2=str(2);Fpu fpu;
    auto t1=scene_call("text_lookup",{key1});
    scene_call("wstr_format",{std::string("stack"),t1,fpu.from_int(arg(1)),0u,0u,0u,0u,0u,0u,0u});
    auto t2=scene_call("text_lookup",{key2});
    scene_call("wstr_format",{std::string("stack"),t2,fpu.from_int(arg(3)),0u,0u,0u,0u,0u,0u,0u});
    scene_call("wstr_copy",{0x5d3048u,std::string("stack")});scene_call("wstr_copy",{0x5d3088u,std::string("stack")});
    scene_call("hud_showHelpMessage",{UINT32_MAX,0x5d3048u,0x5d3088u});
  } else if(name=="helpMessageActive") return signed_byte(memory.read(0x41b928));
  else if(name=="showCompass") {
    auto object=str(0),key=str(1);auto obj=need(find_first(object),name);
    auto address=obj->addr; // keep identity across callback lifetime changes
    auto text=scene_call("text_lookup",{key});
    std::array<std::uint32_t,3> pos={memory.word(address+0x10),memory.word(address+0x14),memory.word(address+0x18)};
    scene_call("hud_setCompassTarget",{pos,text});
  } else if(name=="showBust") {
    auto texture=str(0),key=str(1);auto text=scene_call("text_lookup",{key});auto a=arg(2),b=arg(3);
    scene_call("hud_setBust",{texture,text,a,b});
  } else if(name=="hudResetFadeScreen") scene_call("hud_setFade",{arg(0)});
  else if(name=="hudFadeScreen") return amx::s32(scene_call("hud_stepFade",{arg(0)}));
  else if(name=="revealMap") scene_call("automap_revealAll");
  else if(name=="showOnMap") {
    auto objects=find(str(0),100);
    for(auto obj:objects) if(memory.word(obj->addr+0xc)&0x20) {
      auto address=obj->addr+0xa8;
      auto flags=std::uint64_t(memory.word(address))|(std::uint64_t(memory.word(address+4))<<32);
      if(!arg(1)) flags&=~(UINT64_C(1)<<43);else flags|=UINT64_C(1)<<43;
      std::vector<std::uint8_t> bytes;for(unsigned i=0;i<8;++i) bytes.push_back(std::uint8_t(flags>>(8*i)));
      memory.write(address,bytes);
    }
  } else if(name=="runMedalSuccess" || name=="runMedalFail") {
    auto gate=medal_gate();if(!gate) return 0;
    if(gate==1) {
      if(name=="runMedalSuccess") {
        scene_call("text_begin",{memory.word(0x4f7104),6u,0x80323c3cu,UINT32_MAX});
        auto old=amx::s32(arg(1));auto medal=mission_get_medal(amx::s32(arg(0)),-1,0);
        std::array<std::string,4> keys;std::array<std::uint32_t,4> heights{};std::uint32_t total=0;
        for(unsigned k=0;k<4;++k) {
          keys[k]=str(2+k,128);
          if(!k || (old<medal && old<amx::Cell(k) && medal>=amx::Cell(k))) {
            auto text=scene_call("text_lookup",{keys[k]});
            auto height=scene_call("text_drawWrappedCentered",{0x140u,0xdcu,text,0x15eu,100u,100u,0x11u});
            heights[k]=height+(k ? 0x11 : 0x22);total+=heights[k]+0x11;
          }
        }
        auto y=0xd8u-std::uint32_t(std::int64_t(amx::s32(total))/2);
        auto text=scene_call("text_lookup",{keys[0]});
        scene_call("text_drawWrappedCentered",{0x140u,y,text,0x15eu,std::uint32_t(-100),0x244u,0x11u});
        y+=heights[0];
        for(unsigned k:{3u,2u,1u}) if(heights[k]) {
          auto line=scene_call("text_lookup",{keys[k]});
          scene_call("text_drawWrappedCentered",{0x140u,y,line,0x15eu,std::uint32_t(-100),0x244u,0x11u});y+=heights[k];
        }
        scene_call("text_end");auto prompt=medal_prompt();auto title=scene_call("text_lookup",{memory.cstr(0x531220)});
        scene_call("hud_drawMessageBox",{title,0u,prompt,0x80u,400u,total,0u,UINT32_MAX,0u,0u});
      } else {
        auto key=str(0);auto prompt=medal_prompt();auto title=scene_call("text_lookup",{memory.cstr(0x531240)});
        auto body=scene_call("text_lookup",{key});scene_call("hud_drawMessageBox",{title,body,prompt,0x80u,0u,0u,0u,UINT32_MAX,0u,0u});
      }
    }
    return medal_pad();
  } else if(name=="playerSelRunning") return amx::s32(scene_call("playerSel_isRunning"));
  else if(name!="setupPlayerSel" && name!="startPlayerSel") throw amx::HostFault("unsupported menu handler");
  return 0;
}

namespace {
std::uint64_t field(const Memory &m, std::uint32_t address, unsigned size) {
  std::uint64_t result=0;
  for(unsigned i=0;i<size;++i) result|=std::uint64_t(m.read(address+i))<<(8*i);
  return result;
}
void store(Memory &m, std::uint32_t address, unsigned size, std::uint64_t value) {
  std::vector<std::uint8_t> bytes;
  for(unsigned i=0;i<size;++i) bytes.push_back(std::uint8_t(value>>(8*i)));
  m.write(address,bytes);
}
std::array<std::uint32_t,3> position(const Memory &m,objects::GameObject *obj) {
  return {m.word(obj->addr+0x10),m.word(obj->addr+0x14),m.word(obj->addr+0x18)};
}
std::uint32_t flags(const Memory &m, objects::GameObject *obj) {return m.word(obj->addr+0xc);}
std::uint64_t cflags(const Memory &m, objects::GameObject *obj) {return field(m,obj->addr+0xa8,8);}
void cflags(Memory &m, objects::GameObject *obj, std::uint64_t value) {store(m,obj->addr+0xa8,8,value);}
}
objects::GameObject *Variables::obj_at(std::uint32_t addr) const {
  if(!addr) return nullptr;
  auto owned=world->objects.find(addr);
  if(owned!=world->objects.end() && owned->second) return owned->second;
  auto it=named_objects.find(addr);
  if(it==named_objects.end() || !it->second) throw amx::HostFault("unknown object identity");
  return it->second.get();
}
std::vector<objects::GameObject *> Variables::creatures(const std::string &name,std::int64_t limit) const {
  auto result=find(name,limit);
  result.erase(std::remove_if(result.begin(),result.end(),[&](auto obj) {return !(flags(memory,obj)&0x20);}),result.end());
  return result;
}
void Variables::apply_monster_val(objects::GameObject *obj,amx::Cell which,amx::Cell value) {
  if(!obj) return;
  if(which==4) {scene_call("virtual",{34u,obj->addr,std::uint32_t(value)&0xffff});return;}
  const std::map<amx::Cell,unsigned> offsets={{1,0x3ac},{2,0x3ae},{3,0x3b0},{5,0x3c4}};
  auto it=offsets.find(which);
  if(it!=offsets.end()) store(memory,obj->addr+it->second,2,std::uint32_t(value));
}
void Variables::apply_cflag(objects::GameObject *obj,amx::Cell bit,bool on) {
  if(!obj) return;
  if(bit==0) {
    if(!on && (cflags(memory,obj)&(UINT64_C(1)<<31))) {
      memory.word(0x41b6a0,0);memory.word(0x41b6a4,0);
      cflags(memory,obj,cflags(memory,obj)&~(UINT64_C(1)<<31));
    }
    scene_call("creature_startBossMusic",{obj->addr});
  } else if(bit!=26) return;
  auto value=cflags(memory,obj),mask=UINT64_C(1)<<bit;
  cflags(memory,obj,on ? value|mask : value&~mask);
}
void Variables::apply_oflag(objects::GameObject *obj,amx::Cell which,bool on) {
  if(obj && which==14) memory.word(obj->addr+0xc,on ? flags(memory,obj)|0x4000 : flags(memory,obj)&~0x4000u);
}
void Variables::follow_path(objects::GameObject *obj) {
  if((flags(memory,obj)&0x20) && !(cflags(memory,obj)&(UINT64_C(1)<<27))) {
    cflags(memory,obj,cflags(memory,obj)|(UINT64_C(1)<<27));
    store(memory,obj->addr+0x3c4,2,44000);
    scene_call("obj_runEveryFrame",{obj->addr});
  }
}
void Variables::stop_path(objects::GameObject *obj) {
  if((flags(memory,obj)&0x20) && (cflags(memory,obj)&(UINT64_C(1)<<27))) {
    store(memory,obj->addr+0x3c4,2,0x47e);
    cflags(memory,obj,cflags(memory,obj)&~(UINT64_C(1)<<27));
    std::array<std::uint32_t,3> pos;
    for(unsigned i=0;i<3;++i) pos[i]=memory.word(obj->addr+0x10+4*i);
    for(unsigned i=0;i<3;++i) memory.word(obj->addr+0x3b4+4*i,pos[i]);
    if(field(memory,obj->addr+0x232,2)!=0x14) scene_call("virtual",{27u,obj->addr,0u});
  }
}
void Variables::scale(objects::GameObject *obj,amx::Machine &m,unsigned first) {
  const Fpu fpu;
  std::vector<ServiceValue> args={obj->addr};
  for(unsigned k=0;k<5;++k) args.push_back(fpu.mul(fpu.from_int(std::uint32_t(argument(m,4+4*(first+k)))),0x3c23d70a));
  scene_call("Creature_scaleStats",args);
}
void Variables::start_anim(std::uint32_t state,const std::function<std::uint32_t()> &anim,amx::Cell mode) {
  if(mode==-1) {
    scene_call("anim_clear",{state});
    auto resource=anim();scene_call("anim_push",{state,resource,6u});
    scene_call("anim_jumpTopToEnd",{state});scene_call("anim_releaseCaches",{state});
  } else {
    auto resource=anim();scene_call("anim_start",{state,resource,std::uint32_t(mode),0x3e999999u});
  }
}
amx::Cell Variables::creature_dispatch(const std::string &name,amx::Machine &m) {
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto text=[&](unsigned k) {
    if(name=="NPCAnimAtEnd" || name=="setHitPoints") return string_argument(m,4+4*k,string_slot);
    unsigned key=0x830;
    if(name=="countLiveObjects") key=0x220;
    else if(name=="increaseEnemyRank" || name=="unRootMonster" || name=="stopFollowingPlayer" || name=="friendly" || name=="unFriendly" || name=="getHitPoints") key=0x820;
    else if(name=="setOFlag" || name=="setWeaponType" || name=="setMonsterRange" || name=="moveMonster" || name=="monstersInTrigger") key=0x840;
    else if(name=="startNPCAnim" || name=="generateMonster") key=0x850;
    auto address=arg(k);
    if(m.get_addr(address)==amx::ERR_NONE) menu_string_slots[key]=address;
    else if(!menu_string_slots.count(key)) throw StackGarbage("unknown creature string slot");
    auto result=m.get_string(menu_string_slots.at(key));
    if(result.size()>=160) throw amx::HostFault("creature string overruns handler frame");
    return result;
  };
  auto virtual_call=[&](objects::GameObject *obj,unsigned slot,std::uint32_t value) {scene_call("virtual",{slot,obj->addr,value});};
  if(name=="selectSetMonsterVal" || name=="selectSetCFlag" || name=="selectSetOFlag" || name=="selectFollowPath" || name=="selectStopPath" || name=="initScriptSetParams") {
    auto obj=obj_at(memory.word(0x4039f8));
    if(name=="selectSetMonsterVal") obj=need(obj,name);
    if(!obj) return 0;
    if(name=="selectSetOFlag") {auto which=arg(0);auto on=arg(1);apply_oflag(obj,which,on!=0);}
    else if(name=="selectFollowPath") follow_path(obj);
    else if(name=="selectStopPath") stop_path(obj);
    else if(flags(memory,obj)&0x20) {
      if(name=="initScriptSetParams") scale(obj,m,0);
      else {auto which=arg(0);auto value=arg(1);
        if(name=="selectSetCFlag") apply_cflag(obj,which,value!=0);else apply_monster_val(obj,which,value);}
    }
    return 0;
  }
  if(name=="reapEnemies" || name=="checkMonstersLeft") {
    if(name=="checkMonstersLeft") memory.byte(0x5d32c4,1);
    if(!object_query) throw amx::HostFault("grid query callback required");
    auto query=object_query;
    auto list=name=="reapEnemies" ? query("world_collectObjectsInBox",{std::array<std::uint32_t,3>{0xc76a6000,0xc76a6000,0xc76a6000},std::array<std::uint32_t,3>{0x476a6000,0x476a6000,0x476a6000},0x200u}) : query("world_forEachGridObject",{0x144198u});
    for(auto obj:list) {
      obj=need(obj,"grid query");auto value=flags(memory,obj);
      if(name=="checkMonstersLeft") {if(!(value&0x40) && (value&0x28)==0x20) memory.byte(0x5d32c4,0);}
      else if(((value&0x28)==0x28 || obj->type_name=="P6PhProp" || obj->type_name=="P4Prop" || obj->type_name=="P8ItemProp") && !(value&0x40)) virtual_call(obj,1,3);
    }
    return name=="checkMonstersLeft" && memory.read(0x5d32c4)==0 ? 1 : 0;
  }
  if(name=="generateMonster") {
    auto generator=text(0);auto class_name=text(1);
    auto count=signed_byte(memory.read(0x4f8409));amx::Cell index=-1;
    for(amx::Cell k=0;k<count;++k) if(memory.cstr(0x5d32c8+24*k)==class_name) {index=k;break;}
    if(index==-1) {index=count;memory.byte(0x4f8409,std::uint8_t(count+1));memory.string(0x5d32c8+24*index,class_name);}
    for(auto obj:find(generator,0x200)) if(obj->type_name=="15ScriptGenerator") scene_call("ScriptGenerator_spawn",{obj->addr,memory.cstr(0x5d32c8+24*index)});
    return 0;
  }
  if(name=="startNPCAnim") {
    auto object_name=text(0);auto lump=text(1);auto anim=text(2);
    for(auto obj:find(object_name,0x200)) if(flags(memory,obj)&0x20) {
      auto mode=arg(3);
      start_anim(obj->addr+0x114,[&]() {if(!find_resource) throw amx::HostFault("resource callback required");auto fn=find_resource;return fn(lump,anim);},mode);
    }
    return 0;
  }
  if(name=="NPCAnimAtEnd" || name=="setHitPoints") {
    auto obj=need(find_first(text(0)),name);
    if(name=="NPCAnimAtEnd") return (field(memory,obj->addr+0x11a,2)>>7)&1;
    if(flags(memory,obj)&0x20) {const Fpu fpu;memory.word(obj->addr+0x228,fpu.from_int(std::uint32_t(arg(1))));}
    return 0;
  }
  if(name=="getHitPoints") {
    auto list=find(text(0),0x200);
    for(auto obj:list) if(flags(memory,obj)&0x20) return Fpu::to_int(memory.word(obj->addr+0x228));
    return 0;
  }
  if(name=="countLiveObjects") {
    auto list=find(text(0),0x80);amx::Cell count=0;
    for(auto obj:list) if((flags(memory,obj)&0x28)!=0x28) ++count;
    return count;
  }
  if(name=="moveMonster") {
    auto list=find(text(0),0x200);auto target=find_first(text(1));
    if(target) for(auto obj:list) {
      std::array<std::uint32_t,3> pos;
      for(unsigned i=0;i<3;++i) pos[i]=memory.word(target->addr+0x10+4*i);
      for(unsigned i=0;i<3;++i) memory.word(obj->addr+0x10+4*i,pos[i]);
      store(memory,obj->addr+0x38,2,field(memory,target->addr+0x38,2));scene_call("obj_updateGridCell",{obj->addr});
    }
    return 0;
  }
  bool filtered=name=="increaseEnemyRank" || name=="unRootMonster" || name=="stopFollowingPlayer" || name=="friendly" || name=="unFriendly";
  auto object_name=text(0);
  auto list=filtered ? creatures(object_name) : find(object_name,0x200);
  std::uint32_t total=0;
  for(auto obj:list) {
    auto value=flags(memory,obj);
    if(name=="hideMonster" || name=="showMonster") {
      scene_call(name=="hideMonster" ? "obj_moveOffGrid" : "obj_updateGridCell",{obj->addr});
      value=flags(memory,obj);
      memory.word(obj->addr+0xc,name=="hideMonster" ? value|0x8000 : value&~0x8000u);
      if((value&0x20) || (stub_creature && stub_creature(*obj))) memory.byte(obj->addr+0x231,name=="hideMonster" ? memory.read(obj->addr+0x231)|8 : memory.read(obj->addr+0x231)&0xf7);
    } else if(name=="setOFlag") {auto which=arg(1);auto on=arg(2);apply_oflag(obj,which,on!=0);}
    else if(name=="monstersInTrigger") {if(obj->type_name=="14MonsterTrigger") total+=scene_call("MonsterTrigger_count",{obj->addr});}
    else if(name=="followPath") follow_path(obj);
    else if(name=="stopFollowingPath") stop_path(obj);
    else if(value&0x20) {
      if(name=="increaseEnemyRank") cflags(memory,obj,cflags(memory,obj)|(UINT64_C(1)<<44));
      else if(name=="charmMonster") scene_call("Creature_setCharm",{obj->addr,UINT32_MAX,0u});
      else if(name=="rootMonster") {if(field(memory,obj->addr+0x232,2)!=0x14) {store(memory,obj->addr+0x314,2,0xffff);virtual_call(obj,27,0);}}
      else if(name=="unRootMonster") store(memory,obj->addr+0x314,2,0);
      else if(name=="setMonsterVal" || name=="setCFlag") {auto which=arg(1);auto val=arg(2);if(name=="setCFlag") apply_cflag(obj,which,val!=0);else apply_monster_val(obj,which,val);}
      else if(name=="setWeaponType") {auto weapon=memory.word(obj->addr+0x50);if(weapon) scene_call("virtual",{8u,weapon,0u,std::uint32_t(arg(1))&0xff});}
      else if(name=="setMonsterRange") {auto range=arg(1);if(range>0) virtual_call(obj,34,std::uint32_t(range)&0xffff);}
      else if(name=="killMonster") {if(!(value&8)) {memory.word(obj->addr+0xc,value|0x2009);memory.word(obj->addr+0x228,0);virtual_call(obj,27,0x14);}}
      else if(name=="deleteMonster") virtual_call(obj,1,3);
      else if(name=="followPlayer") {cflags(memory,obj,cflags(memory,obj)|(UINT64_C(1)<<23));scene_call("Creature_setCharm",{obj->addr,UINT32_MAX,0u});}
      else if(name=="stopFollowingPlayer") cflags(memory,obj,cflags(memory,obj)&~(UINT64_C(1)<<23));
      else if(name=="friendly" || name=="unFriendly") {bool on=name=="friendly";memory.word(obj->addr+0xc,on ? value|0x1000000 : value&~0x1000000u);auto cf=cflags(memory,obj);cflags(memory,obj,on ? cf|(UINT64_C(1)<<26) : cf&~(UINT64_C(1)<<26));}
      else if(name=="setCreatureParams") scale(obj,m,1);
      else throw amx::HostFault("unsupported creature handler");
    }
  }
  return amx::s32(total);
}

Variables::InventoryResult Variables::inventory_call(const std::string &name,
    const std::vector<InventoryValue> &args) {
  auto callback=inventory_service;
  if(!callback) throw amx::HostFault("missing inventory service: "+name);
  return callback(name,args);
}
std::uint32_t Variables::inventory_word(const std::string &name,
    const std::vector<InventoryValue> &args) {
  auto result=inventory_call(name,args);
  auto word=std::get_if<std::uint32_t>(&result);
  if(!word) throw amx::HostFault("inventory service requires word result: "+name);
  return *word;
}
objects::GameObject *Variables::player_object(amx::Cell slot) const {
  return obj_at(memory.word(player_record(slot)+0x43d8));
}
std::uint32_t Variables::random_draw() {
  const auto seed=memory.word(0x404464)*0x19660du+0x3c6ef35fu;
  memory.word(0x404464,seed);
  ++draws;
  return seed>>16;
}
void Variables::empty_bag(amx::Cell slot,unsigned bag) {
  const auto rec=player_record(slot), start=rec+0x130+bag*0x1208;
  const auto count=amx::s32(memory.word(start+0x1200));
  for(amx::Cell k=0;k<count;++k)
    if(memory.word(start)!=0) inventory_call("inventory_removeItem",{std::uint32_t(slot),0u,bag,0u});
}
void Variables::drop_at(std::function<objects::GameObject *()> pos_of) {
  const auto block=inventory_word("pool_new",{400u});
  if(block) {
    auto obj=need(pos_of(),"ground item position");
    inventory_call("ItemProp_ctor2",{block,position(memory,obj),0u});
  }
}
bool Variables::give(amx::Cell slot,const std::string &text,amx::Cell variant,
    amx::Cell equip,bool strict,bool reset_idle) {
  const auto rec=player_record(slot);
  const auto definition=inventory_word("item_parseDescriptor",{text,ItemBlank{},UINT32_MAX,std::uint32_t(variant)});
  const auto weight=inventory_word("player_itemWeight",{std::uint32_t(slot)});
  Fpu fpu;
  const bool fits=Fpu::compare(fpu.add(memory.word(rec+0x120),weight),memory.word(rec+0x124))<0;
  if(!fits && (strict || Fpu::compare(weight,0)!=0)) return false;
  const auto index=amx::s32(inventory_word("inventory_giveItem",{std::uint32_t(slot),1u}));
  inventory_call("player_recalcLoad",{std::uint32_t(slot)});
  if(equip) {
    const auto bag=signed_byte(memory.read(definition+0x1d));
    const auto entry=rec+0x130+std::uint32_t(bag)*0x1208u+std::uint32_t(index)*0x48u;
    inventory_call("inventory_equipItem",{std::uint32_t(slot),entry,0u});
    if(reset_idle) inventory_call("player_resetIdleAnim",{std::uint32_t(slot)});
  }
  return true;
}
void Variables::give_or_drop(amx::Cell slot) {
  const auto rec=player_record(slot);
  const auto weight=inventory_word("player_itemWeight",{std::uint32_t(slot)});
  Fpu fpu;
  if(Fpu::compare(fpu.add(memory.word(rec+0x120),weight),memory.word(rec+0x124))<0) {
    inventory_call("inventory_giveItem",{std::uint32_t(slot),1u});
    inventory_call("player_recalcLoad",{std::uint32_t(slot)});
  } else drop_at([&] {return need(player_object(slot),"givePlayerRandom: player without an object");});
}
amx::Cell Variables::inventory_dispatch(const std::string &name,amx::Machine &m) {
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto text=[&](unsigned k) {
    if(name=="onePlayerHasItem" || name=="countQuestItems") return string_argument(m,4+4*k,string_slot);
    if(name=="playerHasItem" || name=="removePlayerItem") return string_argument(m,4+4*k,quest_string_slot);
    if(name=="givePlayerItem") return string_argument(m,4+4*k,cutscene_string_slot);
    unsigned key=name=="giveOnePlayerItem" ? 0xb0 : name=="dropItem" ? 0x60 : 0x40;
    auto address=arg(k);
    if(m.get_addr(address)==amx::ERR_NONE) menu_string_slots[key]=address;
    else if(!menu_string_slots.count(key)) throw StackGarbage("unknown inventory string slot");
    auto result=m.get_string(menu_string_slots.at(key));
    if(result.size()>=160) throw amx::HostFault("inventory string overruns handler frame");
    return result;
  };
  if(name=="playerHasItem") {
    const auto item=text(0);std::uint32_t total=0;
    for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot)
      total+=inventory_word("inventory_countItemByName",{item,std::uint32_t(slot)});
    return amx::s32(total);
  }
  if(name=="onePlayerHasItem") {
    const auto slot=arg(0);
    if(slot<amx::s32(memory.word(NUM_PLAYERS))) {
      // This branch reads only the player's object flag word. In the shared
      // combat World the player is an Actor registered by WorldAuthority,
      // rather than a GameObject in the script World's typed object map.
      const auto player=memory.word(player_record(slot)+0x43d8);
      if(player) {
        if (authority_) {
          if (!authority_->find(player)) throw amx::HostFault("unknown object identity");
        } else {
          (void)obj_at(player); // preserve the standalone registry check
        }
        if (!(memory.word(player+0xc)&0x8)) {
          auto item=text(1);return amx::s32(inventory_word("inventory_countItemByName",{item,std::uint32_t(slot)}));
        }
      }
    }
    return 0;
  }
  if(name=="countQuestItems") {auto item=text(0);return amx::s32(inventory_word("inventory_countItemByName",{item,0u}));}
  if(name=="removePlayerItem") {
    const auto item=text(1);const auto player=arg(0);
    auto remove=[&](amx::Cell slot) {
      auto result=inventory_call("inventory_findItemBySubstring",{std::uint32_t(slot),item});
      auto pair=std::get_if<std::array<std::uint32_t,2>>(&result);
      if(!pair) throw amx::HostFault("findItemBySubstring requires index and bag");
      if(amx::s32((*pair)[0])!=-1) inventory_call("inventory_removeItem",{std::uint32_t(slot),(*pair)[0],(*pair)[1],0u});
    };
    if(player==-1) for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot) remove(slot);
    else if(player<amx::s32(memory.word(NUM_PLAYERS))) remove(player);
  } else if(name=="removeAllItems" || name=="removeAllWeapons") {
    const auto slot=memory.read(0x4f7148);
    for(unsigned bag=0;bag<(name=="removeAllItems" ? 3u : 1u);++bag) empty_bag(slot,bag);
    if(name=="removeAllWeapons") for(unsigned k=0;k<3;++k) memory.word(player_record(slot)+0x4434+4*k,0);
  } else if(name=="givePlayerItem") {
    const auto item=text(0);const auto variant=arg(2);
    for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot) {
      if(memory.word(0x4f713c)==0 || slot==memory.read(0x4f7148)) {
        if(!give(slot,item,variant,arg(1),false,true)) {
          if(memory.word(player_record(slot)+0x43d8)==0)
            drop_at([&] {return need(player_object(alive(0)),"givePlayerItem");});
          else drop_at([&,slot] {return player_object(slot);});
        }
      }
    }
  } else if(name=="giveOnePlayerItem") {
    const auto slot=arg(0);
    if(slot<amx::s32(memory.word(NUM_PLAYERS))) {
      const auto variant=arg(3);auto player=player_object(slot);
      if(player && !(flags(memory,player)&0x8)) {
        const auto item=text(1);
        if(!give(slot,item,variant,arg(2),true,false)) drop_at([&] {return player_object(slot);});
      }
    }
  } else if(name=="givePlayerRandom") {
    Fpu fpu;
    for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot) {
      const auto draw=random_draw();
      const auto scale=inventory_word("level_monsterLevelTimesDifficulty");
      const auto budget=Fpu::to_int(fpu.mul(fpu.mul(fpu.from_int(draw),scale),0x3c75c28f));
      const auto fragment=memory.cstr(0x5312a8);
      if(memory.cstr(0x41b721).find(fragment)!=std::string::npos) {
        const auto difficulty=amx::s32(memory.word(DIFFICULTY));
        if(difficulty>=1 && difficulty<=3) {
          const auto fixed=memory.cstr(0x5312b8+0x50*std::uint32_t(difficulty-1));
          inventory_call("item_parseDescriptor",{fixed,ItemBlank{},0u,0u});
        }
        give_or_drop(slot);
      }
      const auto level=std::uint32_t(Fpu::to_int(memory.word(0x4f714c)));
      for(unsigned k=0;k<10;++k) {
        const auto parity=random_draw()&1;
        if(inventory_word("item_generateWithAffixes",{std::uint32_t(budget),parity,UINT32_MAX,level})!=0) break;
      }
      give_or_drop(slot);
    }
  } else if(name=="dropItem") {
    const auto object=text(0);auto obj=find_first(object);const auto item=text(1);
    if(obj) inventory_call("loot_spawnNamedItem",{position(memory,obj),item});
  } else if(name=="selectDropItem") {
    const auto item=text(0);auto obj=obj_at(memory.word(0x4039f8));
    if(obj) inventory_call("loot_spawnNamedItem",{position(memory,obj),item});
  } else throw amx::HostFault("unsupported inventory handler");
  return 0;
}

std::uint32_t Variables::particle_definition(amx::Cell slot) {
  constexpr std::array<std::uint32_t,3> definitions={0x5d3188,0x5d31d8,0x5d3228};
  if(slot<0 || slot>=3) throw amx::HostFault("particle slot outside documented pointer table");
  return definitions[slot];
}
amx::Cell Variables::effects_dispatch(const std::string &name,amx::Machine &m) {
  const Fpu fpu;
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto raw=[&](unsigned k) {return std::uint32_t(arg(k));};
  auto real=[&](unsigned k) {return fpu.from_int(raw(k));};
  auto address=[&](unsigned k) {
    const auto value=arg(k);
    unsigned key=0x40;
    if(name=="trigger") key=0x1d0;
    else if(name=="autoPropSetState" || name=="autoPropGetState") key=0x1c0;
    else if(name=="deleteObjects") key=0xa0;
    else if(name=="objInPlayerTrig") key=0x110;
    else if(name=="setContainerProps") key=0x840;
    else if(name=="addParticleOnObject") key=0x50;
    else if(name=="particleMorph") key=k==0 ? 0x10f0 : 0x840;
    if(m.get_addr(value)==amx::ERR_NONE) menu_string_slots[key]=value;
    else if(!menu_string_slots.count(key)) throw StackGarbage("unknown effects argument slot");
    return menu_string_slots.at(key);
  };
  auto text=[&](unsigned k) {
    if(name=="stopPropAnim" || name=="propAnimAtEnd" || name=="setNoCollide")
      return string_argument(m,4+4*k,string_slot);
    if(name=="findDistance") return string_argument(m,4+4*k,quest_string_slot);
    auto value=m.get_string(address(k));
    if(value.size()>=160) throw amx::HostFault("effects string overruns handler frame");
    return value;
  };
  if(name=="trigger" || name=="autoPropSetState" || name=="deleteObjects") {
    const auto found=find(text(0),name=="deleteObjects" ? 0x19 : 100);
    for(auto obj:found) {
      if(name=="autoPropSetState" && !(flags(memory,obj)&0x10000000)) continue;
      scene_call("virtual",{name=="trigger" ? 3u : name=="autoPropSetState" ? 18u : 1u,
                           obj->addr,name=="deleteObjects" ? 3u : raw(1)});
    }
  } else if(name=="autoPropGetState") {
    const auto found=find(text(0),100);
    auto obj=need(found.empty() ? nullptr : found.front(),name);
    return flags(memory,obj)&0x10000000 ? amx::s32(memory.word(obj->addr+0x178)) : 0;
  } else if(name=="startPropAnim") {
    auto obj=find_first(text(0));
    const auto anim=text(1);
    obj=need(obj,name);
    const auto mode=arg(2);
    start_anim(obj->addr+0x54,[&]() {
      auto key=memory.cstr(0x531278);
      const auto it=object_properties.find(obj->addr);
      const auto props=it==object_properties.end() ? nullptr : &it->second;
      // Python bytes.lower accepts all byte values, lowering ASCII letters only.
      for(char &ch:key) if(ch>='A' && ch<='Z') ch=char(ch+32);
      if(props && *props) memory.write(0x531278,std::vector<std::uint8_t>(key.begin(),key.end()));
      std::optional<std::string> lump;
      if(props && *props) for(const auto &entry:**props)
        if(entry.size()>key.size() && entry.compare(0,key.size(),key)==0 && entry[key.size()]=='=') {
          lump=entry.substr(key.size()+1);break;
        }
      if(!lump) throw amx::HostFault("startPropAnim: no lump property");
      auto callback=find_resource;
      if(!callback) throw amx::HostFault("res_find callback required");
      return callback(*lump,anim);
    },mode);
  } else if(name=="stopPropAnim" || name=="propAnimAtEnd" || name=="setNoCollide") {
    auto obj=need(find_first(text(0)),name);
    if(name=="stopPropAnim") memory.byte(obj->addr+0x58,0);
    else if(name=="propAnimAtEnd") return (field(memory,obj->addr+0x5a,2)>>7)&1;
    else {
      const auto bits=flags(memory,obj);
      memory.word(obj->addr+0xc,arg(1)==0 ? bits&~1u : bits|1u);
    }
  } else if(name=="objInPlayerTrig") {
    auto obj=find_first(text(0));
    if(obj) {
      auto query=object_query;
      if(!query) throw amx::HostFault("class query callback required");
      const auto triggers=query("world_findObjectsByClass",{std::string("7Trigger"),0x32u});
      for(auto trigger:triggers) {
        need(trigger,"objInPlayerTrig: null trigger");
        if(amx::s32(memory.word(trigger->addr+0x68))!=-1 && scene_call("virtual",{17u,trigger->addr,obj->addr})!=0) return 1;
      }
    }
  } else if(name=="findDistance") {
    const auto first_name=text(0), second_name=text(1);
    auto first=find_first(first_name), second=find_first(second_name);
    if(!first || !second) return 0;
    const auto dy=fpu.sub(memory.word(first->addr+0x14),memory.word(second->addr+0x14));
    const auto dx=fpu.sub(memory.word(first->addr+0x10),memory.word(second->addr+0x10));
    return Fpu::to_int(fpu.sqrt(fpu.add(fpu.mul(dx,dx),fpu.mul(dy,dy))));
  } else if(name=="setContainerProps") {
    const auto found=find(text(0),0x200);
    for(auto obj:found) if(obj->type_name=="13AutoContainer" && arg(1)==1) {
      const auto bits=memory.word(obj->addr+0x124);
      memory.word(obj->addr+0x124,arg(2)==0 ? bits|8u : bits&~8u);
    }
  } else if(name=="lightEffect") {
    const auto ptr=address(0);
    std::array<std::uint32_t,3> pos;
    for(unsigned k=0;k<3;++k) pos[k]=fpu.from_int(std::uint32_t(m.cell(amx::Address(ptr)+4*k)));
    const auto block=scene_call("pool_new",{0xbcu});
    if(block) {
      std::vector<ServiceValue> values={block,pos,0u,0u};
      for(unsigned k=2;k<10;++k) values.push_back(real(k));
      values.push_back(real(10));values.push_back(real(11));values.push_back(ONE);
      values.push_back(fpu.mul(real(1),0x3c23d70a));
      scene_call("LightEffect_ctor",values);
    }
  } else if(name=="addExpandingPool") {
    auto obj=need(find_first(text(0)),name);
    scene_call("decal_add",{position(memory,obj),0u,10u,2u,real(2)});
  } else if(name=="particleMorph") {
    const auto first=find(text(0),0x200), second=find(text(1),0x200);
    auto source=first.empty() || !(flags(memory,first.front())&0x20) ? nullptr : first.front();
    auto target=second.empty() || !(flags(memory,second.front())&0x20) ? nullptr : second.front();
    if(source && target) {
      const auto block=scene_call("pool_new",{0x5cu});
      if(block) scene_call("ParticleMorphEffect_ctor",{block,source->addr,target->addr});
    }
  } else if(name=="setParticle") {
    std::vector<ServiceValue> values={particle_definition(arg(0))};
    for(unsigned k=1;k<8;++k) values.push_back(raw(k));
    for(unsigned k=9;k<12;++k) values.push_back(real(k));
    for(unsigned k:{8u,12u,13u,14u}) values.push_back(raw(k));
    values.push_back(0u);scene_call("particle_define",values);
  } else if(name=="addParticleOnObject") {
    std::array<std::uint32_t,3> velocity,offset,pos;
    for(unsigned k=0;k<3;++k) velocity[k]=fpu.mul(real(5+k),0x3a83126e);
    for(unsigned k=0;k<3;++k) offset[k]=real(2+k);
    auto obj=need(find_first(text(1)),name);
    for(unsigned k=0;k<3;++k) pos[k]=fpu.add(offset[k],memory.word(obj->addr+0x10+4*k));
    const auto slot=arg(0);
    scene_call("particle_spawn",{particle_definition(slot==-1 ? 0 : slot),pos,velocity,0u});
  } else if(name=="shakeCamera") {
    if(memory.word(0x4f7114)==0) scene_call("camera_shake",{0u,real(0)});
  } else if(name=="setFarClip") scene_call("camera_setFarClip",{real(0)});
  else if(name=="setWaveDetail") memory.word(0x4f7130,arg(0)!=0);
  else throw amx::HostFault("unsupported effects handler");
  return 0;
}

std::uint32_t Variables::vehicle_of(amx::Cell slot) const {
  auto player=player_object(slot);
  return !player || (flags(memory,player)&8) ? 0 : memory.word(player->addr+0x540);
}
amx::Cell Variables::vehicle_dispatch(const std::string &name,amx::Machine &m) {
  const Fpu fpu;
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto text=[&](unsigned k) {
    if(name=="putNPCInVehicle") return string_argument(m,4+4*k,quest_string_slot);
    if(name!="putPlayersInVehicle" && name!="entityRideVehicle") return string_argument(m,4+4*k,string_slot);
    auto key=name=="entityRideVehicle" ? 0x70u : 0x40u;
    auto value=arg(k);
    if(m.get_addr(value)==amx::ERR_NONE) menu_string_slots[key]=value;
    else if(!menu_string_slots.count(key)) throw StackGarbage("unknown vehicle string slot");
    auto result=m.get_string(menu_string_slots.at(key));
    if(result.size()>=160) throw amx::HostFault("vehicle string overruns handler frame");
    return result;
  };
  if(name=="getPlayerVPosition") {
    std::uint32_t best=0;
    for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot) {
      auto vehicle=vehicle_of(slot);
      if(vehicle && Fpu::compare(best,memory.word(vehicle+0x90c))<0) best=memory.word(vehicle+0x34c);
    }
    return Fpu::to_int(fpu.mul(best,0x43000000));
  }
  if(name=="getPlayerVDir") {
    for(amx::Cell index=0;index<amx::s32(memory.word(0x4f8420));++index) {
      auto vehicle=vehicle_of(alive(index));
      if(vehicle && Fpu::compare(TWO,memory.word(vehicle+0x348))<0) {
        auto path=memory.word(vehicle+0x910);
        std::array<std::uint32_t,3> terms{};
        for(unsigned k=0;k<3;++k) terms[k]=fpu.mul(memory.word(vehicle+0x468+4*k)^SIGN,memory.word(path+0xc+4*k));
        auto dot=fpu.add(fpu.add(terms[0],terms[1]),terms[2]);
        if(Fpu::compare(0,dot)<0) return 1;
        if(Fpu::compare(dot,0x3f34fdf3)<0) return -1;
      }
    }
    return 0;
  }
  if(name=="entityRideVehicle") {
    const auto entity_name=text(0);
    auto vehicle=need(find_first(text(1)),name)->addr;
    auto entity=need(find_first(entity_name),name)->addr;
    const std::array<std::uint32_t,3> offset={fpu.from_int(std::uint32_t(arg(2))),fpu.from_int(std::uint32_t(arg(3))),fpu.from_int(std::uint32_t(arg(4)))};
    const auto callback=vehicle_seat;
    if(!callback) throw amx::HostFault("vehicle seat service unavailable");
    const auto seat=callback(vehicle,offset);
    for(unsigned k=0;k<3;++k) memory.word(entity+0x10+4*k,seat.position[k]);
    store(memory,entity+0x38,2,seat.facing+std::uint32_t(arg(5)));
    scene_call("obj_updateGridCell",{entity});
    return 0;
  }
  const auto first_name=text(0);
  // Both NPC strings are converted before testing the vehicle.
  const auto npc_name=name=="putNPCInVehicle" ? text(1) : std::string{};
  auto obj=find_first(first_name);
  if(!obj) return 0;
  const auto vehicle=obj->addr;
  if(name=="putNPCInVehicle") {
    auto npc=find_first(npc_name);
    scene_call("Vehicle_attachNpcDriver",{vehicle,npc ? npc->addr : 0u});
  } else if(name=="putPlayersInVehicle") {
    auto active=amx::s32(memory.word(0x4d8ae4));
    if(active!=-1) scene_call("virtual",{14u,vehicle,std::uint32_t(active)});
    for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot) {
      if(slot==amx::s32(memory.word(0x4d8ae4))) continue;
      auto player=need(player_object(slot),name);
      if(!(flags(memory,player)&8)) scene_call("virtual",{14u,vehicle,std::uint32_t(slot)});
    }
  } else if(name=="controlVehicle") {
    memory.word(vehicle+0x410,std::uint32_t(arg(1)));
    memory.word(vehicle+0x2f0,fpu.from_int(std::uint32_t(arg(2))));
  } else if(name=="holdVehicle") scene_call("Vehicle_setHold",{vehicle,std::uint32_t(arg(1))});
  else if(name=="getVPosition") return Fpu::to_int(fpu.mul(memory.word(vehicle+0x90c),0x43000000));
  else throw amx::HostFault("unsupported vehicle handler");
  return 0;
}

namespace {
std::vector<std::uint8_t> read_bytes(const Memory &m,std::uint32_t address,unsigned size) {
  std::vector<std::uint8_t> out;
  out.reserve(size);
  for(unsigned k=0;k<size;++k) out.push_back(m.read(address+k));
  return out;
}
}
void Variables::save_player_record() {
  const auto slot=memory.read(0x4f7148);
  const auto copy=scene_call("save_playerCopy",{std::uint32_t(slot)});
  memory.write(copy,read_bytes(memory,player_record(slot),0x4c30));
  memory.word(0x4f85c0,1);
}
void Variables::restore_player_record() {
  const auto slot=memory.read(0x4f7148);
  const auto rec=player_record(slot);
  const std::array<std::pair<unsigned,unsigned>,5> fields={{{0x4c0d,8},{0x4c15,4},{0x4c19,1},{0x78,24},{0x43d8,4}}};
  std::vector<std::vector<std::uint8_t>> kept;
  for(auto f:fields) kept.push_back(read_bytes(memory,rec+f.first,f.second));
  const auto copy=scene_call("save_playerCopy",{std::uint32_t(slot)});
  memory.write(rec,read_bytes(memory,copy,0x4c30));
  store(memory,0x4f7194,2,240);
  for(unsigned k=0;k<fields.size();++k) memory.write(rec+fields[k].first,kept[k]);
  for(int k=3;k>=0;--k) store(memory,0x4f84a8+2*k,2,240);
  scene_call("game_rebuildAlivePlayers");
  memory.word(0x4f85c0,0);
}
std::array<std::uint32_t,3> Variables::distance3(const std::array<std::uint32_t,3> &a,const std::array<std::uint32_t,3> &b) {
  Fpu f;
  const auto dx=f.sub(a[0],b[0]),dy=f.sub(a[1],b[1]),dz=f.sub(a[2],b[2]);
  return {f.sqrt(f.add(f.add(f.mul(dx,dx),f.mul(dy,dy)),f.mul(dz,dz))),dx,dy};
}
amx::Cell Variables::spell_effect(const std::string &ctor,std::uint32_t size,bool heal) {
  for(amx::Cell slot=0;slot<amx::s32(memory.word(NUM_PLAYERS));++slot) {
    const auto rec=player_record(slot);
    if(heal && Fpu::compare(0,memory.word(rec+0xac))<0) memory.word(rec+0xac,memory.word(rec+0xb0));
    if(memory.word(rec+0x43d8)) {
      const auto block=scene_call("pool_new",{size});
      if(block) scene_call(ctor,{block,1u,memory.word(rec+0x43d8),memory.word(0x41b1e4)});
      scene_call("creature_addSpell",{memory.word(rec+0x43d8),block});
    }
  }
  return 0;
}
amx::Cell Variables::interaction_dispatch(const std::string &name,amx::Machine &m) {
  Fpu f;
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto real=[&](unsigned k) {return f.from_int(std::uint32_t(arg(k)));};
  if(name=="setBestActivePlayer") {
    const auto first=player_object(0);
    const auto slot=(!first || (flags(memory,first)&8)) && amx::s32(memory.word(NUM_PLAYERS))==2 ? 1u : 0u;
    memory.word(0x4d8ae4,slot);return slot;
  }
  if(name=="savePlayer") {save_player_record();return 0;}
  if(name=="restorePlayer") {restore_player_record();return 0;}
  if(name=="hornOmen") return spell_effect("SpellHornOmenEffect_ctor",0x20,false);
  if(name=="planarSteel") return spell_effect("SpellPlanarSteelEffect_ctor",0x6c,true);
  if(name=="explodePotionsOnly") {memory.word(0x4f7128,1);return 0;}
  if(name=="playGotItemSound") {
    auto slot=amx::s32(memory.word(0x4d8ae4));
    if(slot<0) memory.word(0x4d8ae4,0);
    else if(slot>=2) memory.word(0x4d8ae4,1);
    if(amx::s32(memory.word(NUM_PLAYERS))>=2 && Fpu::compare(memory.word(player_record(slot)+0xac),0)<=0)
      slot=memory.word(0x4d8ae4)!=1 ? 1 : 0;
    scene_call("player_voice",{std::uint32_t(slot),9u});return 0;
  }
  std::string text;
  if(name=="closestPlayerDist") text=string_argument(m,4,string_slot);
  else if(name=="hitPlayers") {
    auto &slot=menu_string_slots;
    const auto address=arg(0);
    if(m.get_addr(address)==0) slot[0x60]=address;
    else if(!slot.count(0x60)) throw StackGarbage("unknown hitPlayers string slot");
    text=m.get_string(slot.at(0x60));
    if(text.size()>=160) throw amx::HostFault("hitPlayers string overruns frame");
  } else throw amx::HostFault("unsupported player interaction handler");
  auto centre=find_first(text);
  std::uint32_t best=0x47c35000;
  for(amx::Cell index=0;index<amx::s32(memory.word(0x4f8420));) {
    auto player=player_object(alive(index++));
    if(name=="closestPlayerDist" && !player) continue;
    player=need(player,name);
    need(centre,name);
    const auto d=distance3(position(memory,player),position(memory,centre));
    if(name=="closestPlayerDist") {if(Fpu::compare(d[0],best)<0) best=d[0];}
    else if(Fpu::compare(d[0],real(1))<0) {
      scene_call("virtual",{2u,player->addr,0u,0u,real(2)});
      auto again=need(player_object(alive(index-1)),name);
      memory.word(again->addr+0x2e8,real(3));
      const auto angle=atan2_16(d[2],d[1]);
      again=need(player_object(alive(index-1)),name);
      store(memory,again->addr+0x2e0,2,std::uint32_t(angle));
    }
  }
  return name=="closestPlayerDist" ? Fpu::to_int(best) : 0;
}

amx::Cell Variables::utility_dispatch(const std::string &name, amx::Machine &m) {
  const Fpu f;
  auto arg=[&](unsigned k) {return argument(m,4+4*k);};
  auto str=[&](unsigned k, std::optional<amx::Cell> &slot) {return string_argument(m,4+4*k,slot);};
  if(name=="print") {str(0,string_slot);return 0;}
  if(name=="printVal" || name=="demoRunning") return 0;
  if(name=="getRand") return random_draw();
  if(name=="getScaledTime") return Fpu::to_int(f.mul(f.mul(f.from_int(std::uint32_t(arg(0))),0x42700000),0x3c888888));
  if(name=="sin1000" || name=="cos1000") {
    auto angle=std::uint32_t(arg(0));
    return Fpu::to_int(f.mul(name=="sin1000" ? sin16(angle) : cos16(angle),0x447a0000));
  }
  if(name=="gamePaused") return memory.word(0x41b4f4)!=0 || memory.word(0x500ae8)!=0 || memory.read(0x44fcd0)!=0;
  if(name=="networkMode") return signed_byte(memory.read(0x402791));
  if(name=="levelLoading") return amx::s32(memory.word(0x4f7198));
  if(name=="getLevelTimer") {
    auto ticks=memory.word(0x4f85cc);
    auto value=f.from_int(ticks&0x80000000 ? (ticks>>1)|(ticks&1) : ticks);
    if(ticks&0x80000000) value=f.add(value,value);
    return Fpu::to_int(f.mul(value,0x3c888888));
  }
  if(name=="startMusic") {auto text=str(0,string_slot);scene_call("music_playLevel",{text,3u});return 0;}
  if(name=="startAudio") {auto text=str(0,string_slot);scene_call("stream_playLevel",{1u,text,2u});return 0;}
  if(name=="endMusic") {scene_call("stream_queueStop",{2u});return 0;}
  const auto lump=str(0,quest_string_slot);
  const auto entry=str(1,quest_string_slot);
  auto callback=find_resource;
  if(!callback) throw amx::HostFault("resource callback required");
  const auto resource=callback(lump,entry);
  if(name=="sound") {
    if(resource) scene_call("sound_playSample",{resource,0u,0x3f800000u,0x3f800000u,0u});
  } else {
    const auto handle=scene_call("sound_playOnObject",{resource,0u,0x3f800000u});
    scene_call("sound_setPlaysWhileFrozen",{handle});
  }
  return 0;
}

void Variables::set_script(std::shared_ptr<amx::Machine> machine) {
  script_machine=std::move(machine);
  if(script_machine) script_machine->register_natives(natives());
}
amx::Address Variables::run_script(const std::optional<std::string> &name) {
  auto machine=script_machine;
  if(!name || !machine) return -1;
  const auto found=machine->find_public(*name);
  if(found.first) {
    if(amx::s32(memory.word(MISSING_STAMP))<amx::s32(memory.word(DRAW_FRAME)))
      memory.word(MISSING_STAMP,memory.word(DRAW_FRAME)+20u);
    return -1;
  }
  auto outer20=string_slot, outer30=quest_string_slot,
       outer80=dialog_string_slot, outerd0=cutscene_string_slot;
  auto outer_menu=std::move(menu_string_slots);
  forget_string_slot();
  auto restore=[&]() {
    string_slot=outer20;quest_string_slot=outer30;
    dialog_string_slot=outer80;cutscene_string_slot=outerd0;
    menu_string_slots=std::move(outer_menu);
  };
  try {
    const auto result=machine->execute(found.second,{},entry_pri);
    restore();
    return result.second.value_or(0);
  } catch(...) { restore(); throw; }
}
void Variables::queue_deferred(const std::string &name) {
  const auto queued=amx::s32(memory.word(DEFER_QUEUED));
  if(queued<64) {
    memory.word(DEFER_QUEUED,std::uint32_t(queued)+1u);
    const auto half=(memory.word(DEFER_FLIP)&1)^1;
    memory.string(DEFER_NAMES+half*64*0x50+std::uint32_t(queued)*0x50,name);
  }
}
void Variables::run_deferred() {
  std::int64_t index=0;
  while(index<amx::s32(memory.word(DEFER_COUNT))) {
    const auto half=memory.word(DEFER_FLIP)&1;
    auto name=memory.cstr(DEFER_NAMES+half*64*0x50+std::uint32_t(index)*0x50);
    ++index;
    run_script(name);
  }
  memory.word(DEFER_COUNT,memory.word(DEFER_QUEUED));
  memory.word(DEFER_QUEUED,0);
  memory.word(DEFER_FLIP,memory.word(DEFER_FLIP)+1u);
}
void Variables::clear_deferred() {
  memory.word(DEFER_COUNT,0);memory.word(DEFER_QUEUED,0);memory.word(DEFER_FLIP,0);
}
void Variables::reset_script_state() { clear_deferred();memory.byte(POST_ACTION,0); }
void Variables::set_post_action(const std::string &name) { memory.string(POST_ACTION,name); }
void Variables::run_post_action() {
  if(memory.read(POST_ACTION)) {
    auto name=memory.cstr(POST_ACTION);
    memory.byte(POST_ACTION,0);memory.byte(IN_POST_ACTION,1);
    run_script(name);
    // Deliberately not exception cleanup: a host exception retains flag=1.
    memory.byte(IN_POST_ACTION,0);
  }
}

amx::NativeMap Variables::natives() {
  amx::NativeMap result;
  for(const auto &name:script_names)
    result.emplace(name,bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for(const auto &name:utility_names)
    result.emplace(name,bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const std::string name : {"getv", "setv", "addv", "getpv", "setpv"})
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : interaction_names)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : vehicle_names)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : effects_names)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : inventory_names)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : creature_names)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : menus)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : scene)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : players)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  for (const auto &name : campaign)
    result.emplace(name, bind(name, [this,name](amx::Machine &m, Args &) { return dispatch(name,m); }));
  return result;
}
} // namespace champions::simulation::rta::builtins
