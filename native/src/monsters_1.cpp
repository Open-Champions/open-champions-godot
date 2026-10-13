// Authored translation of the pinned MIT research contracts; see MONSTER_SYSTEMS.md.
#include "champions/monsters_1.hpp"
#include <utility>
namespace champions::simulation::rta::monsters_1 {
namespace {
constexpr std::size_t history_limit = 65536;
void creature_span(Address c) {
  if (c > UINT32_MAX - 0x3c7u) throw Unsupported("creature raw32 span");
}
Raw pointer_result(std::uint64_t value) {
  if (value > UINT32_MAX) throw Unsupported("callback pointer outside raw32");
  return static_cast<Raw>(value);
}
struct Depth {
  std::size_t &value;
  explicit Depth(std::size_t &v) : value(v) {
    if (v == 64) throw Unsupported("monster recursion limit");
    ++value;
  }
  ~Depth() { --value; }
};
std::int32_t signed8(std::uint64_t v) {
  return (v & 128) ? static_cast<std::int32_t>(v & 255) - 256 : static_cast<std::int32_t>(v & 255);
}
}
Model::Model(Memory &m, CallbackPolicy p) : mem(m), policy(p) {}
Raw Model::anim_flags(Address c, Raw mask) const {
  if (c>UINT32_MAX-0x11bu) throw Unsupported("animation flag raw32 span");
  return static_cast<Raw>(mem.load(c+0x11a,2)) & mask;
}
Raw Model::u32(Address a) const { return static_cast<Raw>(mem.load(a, 4)); }
Vec3 Model::vec(Address a) const { return {u32(a), u32(a+4), u32(a+8)}; }
void Model::write(Address a, std::size_t size, std::uint64_t v) {
  if (writes.size() >= history_limit || events.size() >= history_limit)
    throw Unsupported("monster history limit");
  if (a > UINT32_MAX) throw Unsupported("non-raw32 write address");
  if (size < 8) v &= (UINT64_C(1) << (size*8)) - 1;
  Write w{static_cast<Raw>(a), size, v};
  writes.push_back(w); events.push_back({false, w, {}});
  mem.store(a, size, v);
}
void Model::wvec(Address a, Vec3 v) {
  write(a, 8, v[0] | (static_cast<std::uint64_t>(v[1]) << 32));
  write(a+8, 4, v[2]);
}
void Model::record_call(const Call &c) {
  if (calls.size() >= history_limit || events.size() >= history_limit)
    throw Unsupported("monster history limit");
  calls.push_back(c); events.push_back({true, {}, c});
}
std::uint64_t Model::call(const std::string &name, std::vector<Argument> args) {
  Call c{name, std::move(args)};
  record_call(c);
  auto it = callbacks.find(name);
  if (it != callbacks.end()) { auto fn = it->second; return fn(*this, c.args); }
  if (policy == CallbackPolicy::RequireHandlers)
    throw creature_hp::MissingHandler("missing monster callback: " + name);
  return 0; // Explicit opt-in pinned reference zero policy.
}
std::uint32_t Model::random(Address c) {
  const auto seed = u32(c+0x44)*0x19660du + 0x3c6ef35fu;
  write(c+0x44,4,seed); return seed >> 16;
}
std::pair<bool, std::int32_t> Model::filter(Address c, std::uint32_t requested) {
  auto state = s32(requested);
  if (u32(0x4f7120) && !u32(0x4f711c) && u32(0x503890) && !u32(0x503894)) return {true,state};
  if (signed8(mem.load(c+0x231,1)) && state == 1) return {true,state};
  if (state == 20) for (auto off : {0x31c,0x318,0x314,0x31a}) write(c+off,2,0);
  if (mem.load(c+0x314,2) && state != 0 && state != 5 && !(state >= 6 && state <= 15))
    state = u32(c+0x234) ? 5 : 0;
  else if (mem.load(c+0x31c,2) && state != 19) state = 19;
  else {
    if (!u32(c+0x234)) write(c+0x318,2,0);
    if (mem.load(c+0x318,2) && state != 16) state = 16;
  }
  if (state == 0) { call("handle_clear", {static_cast<Raw>(c+0x234)}); write(c+0x234,4,0); }
  return {false,state};
}
void Model::clear_path(Address c) {
  for (auto off : {0x244,0xf8,0xfc,0x240,0x23c,0x248})
    write(c+off,4,(off == 0xf8 || off == 0xfc) ? MINUS_ONE : UINT32_MAX);
  write(c+0x2bc,4,0); write(c+0x2c4,4,0);
  wvec(c+0x2c8,vec(c+0x10)); write(c+0x2d4,4,u32(c+0x300)); wvec(c+0x2ec,{0,0,0});
}
void Model::wander(Address c) {
  const auto cf = mem.load(c+0xa8,8);
  if (cf & 0x8000000) return;
  auto home = vec(c+0x3b4);
  const auto distance = [&] {
    auto p = vec(c+0x10);
    auto x = sub(p[0],home[0]), y = sub(p[1],home[1]), z = sub(p[2],home[2]);
    return add(add(mul(x,x),mul(y,y)),mul(z,z));
  };
  if ((cf & 0xc00000) && lt(0x46610000,distance())) return;
  std::int32_t reach = (cf & 0xc00000) ? 120 : 300;
  if (lt(u32(c+0x3c0),from_int(static_cast<Raw>(reach)))) reach = to_int(u32(c+0x3c0));
  auto d2 = distance();
  const auto square = static_cast<Raw>(reach)*static_cast<Raw>(reach);
  if (le(from_int(square),d2) || ((mem.load(c+0xa8,8)&0x400000) && lt(0x46610000,d2))) {
    wvec(c+0x104,home); return;
  }
  Vec3 point = home;
  for (unsigned i=0;i<10;++i) {
    auto x = add(u32(c+0x3b4),from_int((random(c)&(static_cast<Raw>(reach)*2))-static_cast<Raw>(reach)));
    auto y = add(u32(c+0x3b8),from_int((random(c)&(static_cast<Raw>(reach)*2))-static_cast<Raw>(reach)));
    point = {x,y,u32(c+0x3bc)};
    auto ground = static_cast<Raw>(call("terrain_heightBelow",{u32(0x44fdc8),point,u32(c+0x1c)}));
    auto z = u32(c+0x18);
    if (lt(sub(z,0x41400000),ground) && lt(ground,add(z,0x41400000))) break;
  }
  wvec(c+0x104,point);
}
void Model::start(Address c, std::uint32_t offset, std::uint32_t flags, Raw blend) {
  call("anim_start",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+offset),flags,blend});
}
void Model::swing(Address c, std::uint32_t offset, std::uint32_t flags) {
  call("anim_startBlendInOut",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+offset),flags,0x3dccccccu,0x3dccccccu});
}
void Model::drop_loot(Address c, Raw item, Raw a, Raw b, Raw d) {
  call("Creature_dropLoot",{static_cast<Raw>(c),vec(c+0x10),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))),item,0x42100000u,TWO,a,b,d,0x3e800000u});
}
void Model::on_death(Address c, std::uint32_t quiet) {
  write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0xfffffffff73fffff));
  auto script = pointer_result(call("obj_getPropStr",{static_cast<Raw>(c),0x52fbf8u,0u}));
  if (script) { write(0x4039f8,4,c); call("script_runByName",{script}); write(0x4039f8,4,0); }
  if (!quiet) {
    auto item = pointer_result(call("obj_getPropStr",{static_cast<Raw>(c),0x52fc08u,0u}));
    if (item) call("loot_spawnNamedItem",{vec(c+0x10),item});
  }
  auto flags = u32(c+0xc);
  if (flags&2) {
    write(c+0xc,4,flags|0x4000); call("anim_jumpTopToEnd",{static_cast<Raw>(c+0x114)}); call("obj_delete",{static_cast<Raw>(c)});
  }
  if (u32(0x4f7120)) {
    auto node = u32(c+0x2e4);
    call("arena_addKill",{node ? u32(static_cast<Address>(node)+0xc) : 0u,0u});
  }
  call("creature_stopBossMusic",{static_cast<Raw>(c)});
}
void Model::virtual_call(Address c, unsigned slot, std::uint32_t arg) {
  const auto table = u32(c+0x48);
  const auto fn = u32(static_cast<Address>(table)+8*slot+4);
  const auto self = static_cast<Raw>(c + s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*slot,2))));
  const auto name = "Creature.v"+std::to_string(slot);
  if (!external.count(name)) {
    if (slot == 27 && fn == 0x2ab704) { smallspider_set_state(self,arg); return; }
    if (slot == 27 && fn == 0x267a0c) { gnaw_set_state(self,arg); return; }
    if (slot == 27 && fn == 0x297f54) { mosquito_set_state(self,arg); return; }
    if (slot == 27 && fn == 0x2a55ec) { scorpion_set_state(self,arg); return; }
    if (slot == 27 && fn == 0x244aa4) { badger_set_state(self,arg); return; }
    if (slot == 27 && fn == 0x3c2688) { write(static_cast<Address>(self)+0x232,2,arg); return; }
    if (slot == 28 && fn == 0x10beb8) { on_death(self,arg); return; }
    // Both derived Python dispatch tables recognize body pointers even in an
    // unexpected slot. Do not turn such unported bodies into guessed callbacks.
    constexpr Raw modelled[] = {
      0x2ab5d8,0x2ab704,0x2ab8e0,0x267654,0x267a0c,0x297b30,
      0x297f54,0x2a5068,0x2a55ec,0x244380,0x244aa4,0x244a9c,
      0x27236c,0x272f4c,0x2a8b84,0x2a91b8,0x2c2440,0x2c382c,
      0x2c34e4,0x2c3c54,0x3c2738,0x10e658,0x10beb8,0x3c2708};
    for (auto body : modelled)
      if (fn == body) throw Unsupported("unported or mismatched modelled virtual body");
  }
  call(name,{self,arg});
}
void Model::spider_state_body(Address c, std::uint32_t requested) {
  auto [refused,state] = filter(c,requested);
  if (refused) return;
  if (state == 0) {
    auto d = random(c); write(c+0xf4,4,u32(c+0x300)+d%300+120); start(c,0x3c,3,0x3e999999);
  } else if (state == 1) { clear_path(c); wander(c); start(c,0x4c,3,0x3e999999); }
  else if (state == 5 || state == 16 || state == 18 || state == 19) state = s16(static_cast<Raw>(mem.load(c+0x232,2)));
  else if (state == 20) {
    write(c+0xc,4,u32(c+0xc)|9);
    auto angle = random(c); auto size = random(c)%10+10;
    call("decal_add",{vec(c+0x10),static_cast<Raw>(s16(angle)),6u,1u,from_int(size)});
    start(c,0x1c+8*(random(c)&3),6,0x3e999999);
    if (u32(c+0xc)&2) call("anim_jumpTopToEnd",{static_cast<Raw>(c+0x114)});
    virtual_call(c,28,0);
  }
  write(c+0x232,2,static_cast<Raw>(state));
}
void Model::smallspider_set_state(Address c, std::uint32_t state) {
  Depth guard(depth_); creature_span(c); spider_state_body(c,state);
}
void Model::gnaw_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_); creature_span(c);
  auto [refused,state] = filter(c,requested); if (refused) return;
  const auto anim = static_cast<Raw>(c+0x114);
  if (state == 0) start(c,0x1c,3,0x3dcccccc);
  else if (state == 1) { clear_path(c); wander(c); start(c,0x3c,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 3) { clear_path(c); start(c,0x3c,0x803,0x3dcccccc); }
  else if (state == 5) { start(c,0x1c+8*(random(c)%3),3,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 6) swing(c,0x1c+8*(random(c)%3),0x808);
  else if (state == 16) { write(c+0x318,2,60); clear_path(c); start(c,0x34,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 18) state = s16(static_cast<Raw>(mem.load(c+0x232,2)));
  else if (state == 19) start(c,0x1c,3,0x3e999999);
  else if (state == 20) {
    for (unsigned i=0;i<6;++i) {
      auto spot=vec(c+0x10); spot[2]=add(spot[2],0x41c00000);
      if (i<2) {
        auto block=pointer_result(call("pool_new",{0x268u}));
        if (block) { auto variant=random(c)%3; call("GutChunk_ctor",{block,spot,variant,2u,ONE}); call("PhProp_initPhysics",{block,0x12u}); }
      }
      call("fx_spawn",{9u,spot});
    }
    start(c,0x1c,0x803,0x3dcccccc); virtual_call(c,28,0);
  } else state=0;
  write(c+0x232,2,static_cast<Raw>(state));
}
void Model::mosquito_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_); creature_span(c);
  auto [refused,state]=filter(c,requested); if (refused) return;
  if (state == 0 || state == 5) {
    start(c,0x84,3,0x3dcccccc);
    // The clock is read before the RNG write, as in the reference expression.
    auto clock=u32(c+0x300); auto d=random(c);
    write(c+0xf4,4,clock+d%120+(state==5 ? 60 : 0));
  } else if (state == 1) { clear_path(c); wander(c); start(c,0x64,0x803,HALF); }
  else if (state == 3 || state == 16) {
    if (state==16) write(c+0x318,2,60);
    clear_path(c); start(c,0x64,0x803,0x3dcccccc);
  } else if (state == 6) swing(c,0x1c+8*(random(c)&1),0x808);
  else if (state == 18) { swing(c,0x6c+8*(random(c)&1),0x810); mosquito_lose_part(c,random(c)%3+1); }
  else if (state == 19) start(c,0x84,3,0x3dcccccc);
  else if (state == 20) {
    drop_loot(c,0,0x3fcccccc,0x3f199999,0x3fb33333);
    start(c,0x2c+8*(random(c)&3),0x806,0x3e4ccccc);
    mosquito_lose_part(c,(random(c)&3)+4); virtual_call(c,28,0);
  } else if (state == 23) swing(c,0x4c+8*(random(c)%3),0x810);
  else state=0;
  write(c+0x232,2,static_cast<Raw>(state));
  auto flags=mem.load(c+0xa8,8);
  write(c+0xa8,8,(s16(static_cast<Raw>(state))==0 || s16(static_cast<Raw>(state))==20) ? flags&~UINT64_C(0x100000) : flags|UINT64_C(0x100000));
}
void Model::scorpion_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_); creature_span(c);
  auto [refused,state]=filter(c,requested); if (refused) return;
  if (state == 0) { auto clock=u32(c+0x300); auto d=random(c); write(c+0xf4,4,clock+d%60+120); start(c,0x84,3,0x3e999999); }
  else if (state == 1) { clear_path(c); wander(c); start(c,0x94,0x803,0x3e999999); }
  else if (state == 3 || state == 16) { clear_path(c); start(c,0x9c,0x803,0x3e999999); }
  else if (state == 5) start(c,0x8c,3,0x3dcccccc);
  else if (state == 6) {
    start(c,0x8c,3,0x3dcccccc);
    if (random(c)%6==0) { state=7; swing(c,0x44,8); }
    else swing(c,0x2c+8*(random(c)%3),8);
    write(c+0xf4,4,random(c)%20);
  } else if (state == 18) {
    auto current=s16(static_cast<Raw>(mem.load(c+0x232,2)));
    if (current==19 || (current==6 && anim_flags(c,8))) state=current;
    else swing(c,0x7c,0x10);
  } else if (state == 19) start(c,0x84,3,0x3e999999);
  else if (state == 20) {
    if (random(c)%3==0) scorpion_sever_part(c,random(c)%6);
    drop_loot(c,0x54b648,0x3fcccccc,0x3f199999,0x3fb33333);
    start(c,0x5c+8*(random(c)&1),0x806,0x3e999999); virtual_call(c,28,0);
  } else if (state == 23 || state == 24) { start(c,0x8c,3,0x3dcccccc); swing(c,state==23 ? 0x6c : 0x74,0x808); }
  write(c+0x232,2,static_cast<Raw>(state));
}
void Model::badger_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_);
  // Badger state transitions also write the extension word at +0x3c8.
  if (c>UINT32_MAX-0x3cfu) throw Unsupported("badger raw32 span");
  auto [refused,state]=filter(c,requested); if (refused) return;
  const auto anim=static_cast<Raw>(c+0x114);
  if (state == 0) { start(c,0x7c,3,0x3dcccccc); write(c+0xf4,4,u32(c+0x300)+180); }
  else if (state == 1) {
    clear_path(c); wander(c); start(c,mem.load(c+0x312,2)==0 ? 0x8c : 0x4c,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim});
  } else if (state == 3) {
    clear_path(c); auto current=s16(static_cast<Raw>(mem.load(c+0x232,2)));
    if (current!=3 && !(current>=6 && current<=10)) start(c,0x4c,0x803,0x3e4ccccc);
    call("anim_syncToSimFrame",{anim});
  } else if (state == 5 || state == 19) start(c,0x7c,3,0x3dcccccc);
  else if (state>=6 && state<=10) {
    constexpr Raw offsets[]={0x24,0x54,0x2c,0x3c,0x34};
    swing(c,offsets[state-6],(state==8 || state==9) ? 0x4808 : 0x808); write(c+0x3c8,4,0);
  } else if (state == 16) { clear_path(c); start(c,0x44,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 18) {
    if (s16(static_cast<Raw>(mem.load(c+0x232,2)))==6 && anim_flags(c,8)) state=6;
    else swing(c,0x6c+8*(random(c)&1),0x10);
  } else if (state == 20) {
    auto flags=u32(c+0xc); if (!(flags&2)) write(c+0xc,4,flags&~1u);
    drop_loot(c,0,0x40333333,0x40400000,0x40400000);
    start(c,0x5c+8*(random(c)&1),6,0x3dcccccc); virtual_call(c,28,0);
  }
  write(c+0x232,2,static_cast<Raw>(state));
}

void Model::smallspider_on_touch(Address c, std::uint32_t other) {
  Depth guard(depth_); creature_span(c);
  if (other && s16(static_cast<Raw>(mem.load(c+0x232,2))) != 20 && (u32(static_cast<Address>(other)+0xc)&0x40)) virtual_call(c,27,20);
  if (u32(static_cast<Address>(other)+0xc)&0x40) write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x800000000000000));
}
void Model::shed(Address c) {
  auto block = pointer_result(call("pool_new",{0x268u}));
  if (!block) return;
  auto definition = u32(c+0x24);
  call("GutChunk_ctorFromProp",{block,vec(c+0x10),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))),u32(static_cast<Address>(definition)+0xc),static_cast<Raw>(c+0x114),u32(definition),u32(static_cast<Address>(definition)+8),u32(static_cast<Address>(definition)+0x10),ONE});
  write(static_cast<Address>(block)+0x1c4,4,add(u32(static_cast<Address>(block)+0x1c4),TWO));
}
void Model::mosquito_lose_part(Address c, std::uint32_t part) {
  Depth guard(depth_); creature_span(c);
  auto model = [&] { return u32(static_cast<Address>(u32(c+0x24))+0xc); };
  call("model_buildHideMask",{model(),u32(c+0xe8)});
  constexpr Raw bits[] = {0,0x10,0x10,0x40,2,1,4,8};
  if (part >= 1 && part <= 7) write(c+0xe8,4,u32(c+0xe8)|bits[part]);
  auto mask = call("model_buildHideMask",{model(),u32(c+0xe8)}); write(c+0x220,8,mask);
  shed(c);
}
void Model::scorpion_sever_part(Address c, std::uint32_t part) {
  Depth guard(depth_); creature_span(c);
  call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+0xc),u32(c+0xe8)});
  auto bits = u32(c+0xe8)|(1u<<(part&31));
  auto model = u32(static_cast<Address>(u32(c+0x24))+0xc);
  write(c+0xe8,4,bits);
  auto mask = call("model_buildHideMask",{model,bits}); write(c+0x220,8,mask);
  shed(c);
}
std::uint32_t Model::str_prefix_differs(Address text, Address prefix) {
  Depth guard(depth_);
  if (text > UINT32_MAX || prefix > UINT32_MAX) throw Unsupported("prefix raw32 address");
  for (unsigned i=0;i<4096;++i) {
    auto want = signed8(mem.load(prefix+i,1));
    if (!want) return 0;
    auto have = signed8(mem.load(text+i,1));
    if (mem.load(static_cast<Raw>(0x5628e1+want),1)&1) want += 32;
    if (mem.load(static_cast<Raw>(0x5628e1+have),1)&1) have += 32;
    if (have != want) return 1;
  }
  throw Unsupported("prefix scan limit");
}
// Base constructor writes are part of each derived constructor's contract.
// GameObject, property lookup, balancing and statistics remain host services.
void Model::creature_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw def, Raw props, Raw hp) {
  call("GameObject_ctor",{static_cast<Raw>(c),pos,def,props});
  write(c+0x50,4,0); write(c+0x48,4,0x50cc58); write(c+0x4c,4,0x50d010); write(c+0x119,1,8);
  for (auto off:{0x2f4,0x2ec,0x2f0,0x234,0x238,0x2e4,0x360,0x364}) write(c+off,4,0);
  wvec(c+0x3b4,pos); write(c+0x38,2,facing); write(c+0x22c,4,hp); write(c+0x220,8,UINT64_MAX);
  write(c+0xc,4,u32(c+0xc)|0x20); write(c+0xc8,4,0); write(c+0x228,4,hp);
  write(c+0x23c,4,UINT32_MAX); write(c+0x240,4,UINT32_MAX);
  for (auto off:{0x2bc,0x2c0,0x2dc,0xf4,0x2c4,0x300}) write(c+off,4,0);
  for (auto off:{0x110,0x31c,0x312,0x316}) write(c+off,2,0);
  write(c+0x231,1,0); write(c+0x2fc,1,0); write(c+0x310,1,255); write(c+0x5a,1,255); write(c+0xa0,1,0);
  write(c+0x3c,4,u32(c+0x18)); write(c+0xdc,4,ONE); write(c+0x58,2,65535); write(c+0x3c4,2,600);
  for (auto off:{0xb8,0x304,0xe0,0x2d8}) write(c+off,4,0);
  for (auto off:{0x314,0x31a,0x318}) write(c+off,2,0);
  auto serial=mem.load(0x4f85c4,1); write(0x4f85c4,1,serial+1); write(c+0x230,1,serial);
  auto prop=[&](Raw key){return call("obj_getPropInt",{static_cast<Raw>(c),key,0u});};
  auto str=[&](Raw key){return call("obj_getPropStr",{static_cast<Raw>(c),key,0u});};
  if (prop(0x52f9f0)) write(c+0x231,1,mem.load(c+0x231,1)|1);
  if (prop(0x52f9f8)) { write(c+0x231,1,mem.load(c+0x231,1)|8); call("obj_moveOffGrid",{static_cast<Raw>(c)}); write(c+0xc,4,u32(c+0xc)|0x8000); }
  if (str(0x52fa00)) { write(c+0x368,4,pointer_result(str(0x52fa00))); write(c+0xc,4,u32(c+0xc)|0x100000); }
  if (prop(0x52fa10)) write(c+0xc8,4,0x447a0000);
  write(c+0x3c0,4,0x447a0000);
  if (prop(0x52fa18)) write(c+0x3c0,4,from_int(static_cast<Raw>(prop(0x52fa18))));
  constexpr Raw keys[]={0x52fa28,0x52fa38,0x52fa48}, offsets[]={0x3ac,0x3ae,0x3b0}, defaults[]={300,500,0x4000};
  for (unsigned i=0;i<3;++i) { write(c+offsets[i],2,defaults[i]); if (prop(keys[i])) write(c+offsets[i],2,prop(keys[i])); }
  if (prop(0x52fa58)) write(c+0xdc,4,mul(from_int(static_cast<Raw>(prop(0x52fa58))),0x3c23d70a));
  if (prop(0x52fa60)) write(c+0xa8,8,mem.load(c+0xa8,8)|1);
  if (prop(0x52fa70)) write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x1000000000000));
  if (str(0x52fa80)||str(0x52fa88)||str(0x52fa90)) write(c+0xa8,8,mem.load(c+0xa8,8)|0x40000000);
  if (name) {
    write(c+0x34,4,name); record_call({"balance_lookup",{name}});
    Balance factors{};
    if (balance_lookup) { auto fn=balance_lookup; factors=fn(*this,name); }
    else if (policy==CallbackPolicy::RequireHandlers) throw creature_hp::MissingHandler("missing monster callback: balance_lookup");
    if (!factors[0]) factors={0,ONE,ONE,ONE,ONE,ONE};
    call("Creature_setStats",{static_cast<Raw>(c),1u,factors[1],factors[2],factors[3],factors[4],factors[5]});
  }
  write(c+0x2fa,2,mem.load(c+0x38,2));
}
void Model::event_params(Address c, Raw entry, std::array<Raw,4> masks, std::array<Raw,4> times) {
  for (unsigned k=0;k<4;++k) {
    auto record=u32(static_cast<Address>(u32(c+0x24))+8*entry+4);
    write(static_cast<Address>(record)+0x18+8*k,1,masks[k]);
    write(static_cast<Address>(record)+0x14+8*k,4,times[k]);
  }
}
void Model::smallspider_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props) {
  Depth guard(depth_); creature_span(c);
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x502818,props,ONE);
  write(c+0xb8,4,0x42480000); write(c+0x1c,4,0x41200000); write(c+0x48,4,0x50fd40); write(c+0x38,2,facing);
  write(c+0xb0,4,0x3f333333); write(c+0x232,2,0); write(c+0xb4,1,0x12);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8004001000)); write(c+0xec,4,u32(0x44fdcc));
  write(c+0x1c,4,0x40a00000); write(c+0xdc,4,0x3e4ccccc); write(c+0x64,4,ONE);
  call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x3c),1u});
  call("anim_randomizeTopTime",{static_cast<Raw>(c+0x114)}); write(c+0x232,2,0);
  call("obj_runEvery60",{static_cast<Raw>(c)}); call("Creature_init",{static_cast<Raw>(c)});
}
void Model::gnaw_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props) {
  Depth guard(depth_); creature_span(c);
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x5000a0,props,0x41200000);
  auto frame=u32(0x44fdcc); write(c+0xb4,1,1); write(c+0xb0,4,0x3f999999); write(c+0xec,4,frame);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x200001000)); write(c+0x48,4,0x515038);
  write(c+0x1c,4,0x40c00000); write(c+0xf4,4,frame+300); write(c+0xb8,4,0); write(c+0x232,2,0);
  auto unit=f2d(mul(from_int(random(c)),0x37800000));
  write(c+0xdc,4,d2f(dadd(dmul(unit,mem.load(0x545ea0,8)),mem.load(0x545ea8,8))));
  call("anim_clear",{static_cast<Raw>(c+0x114)}); gnaw_set_state(c,0); call("obj_runEveryFrame",{static_cast<Raw>(c)});
  for (Raw e:{3u,4u,5u}) event_params(c,e,{1,0,0,0},{0x3e4ccccc,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::mosquito_ctor(Address c, Vec3 pos, Raw facing, Raw props, Raw name) {
  Depth guard(depth_); creature_span(c);
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x5017a0,props,0x41200000);
  auto frame=u32(0x44fdcc); write(c+0x28,1,0x1e); write(c+0xb4,1,0x12); write(c+0x48,4,0x516a08);
  write(c+0xb0,4,0x3f999999); write(c+0x1c,4,0x41200000); write(c+0xec,4,frame); write(c+0xf4,4,frame+300);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000001000)); write(c+0x5c,4,0xbe4ccccc); write(c+0xb8,4,0);
  write(c+0x232,2,0); write(c+0x64,4,ONE); write(c+0x68,4,ONE);
  if (str_prefix_differs(name,0x549af0)) write(c+0xdc,4,0x3f333333);
  call("anim_clear",{static_cast<Raw>(c+0x114)}); mosquito_set_state(c,0); call("obj_runEvery60",{static_cast<Raw>(c)});
  for (Raw e:{3u,4u}) event_params(c,e,{1,0,0,0},{0x3e4ccccc,0,0,0});
  for (Raw e:{5u,6u,7u,8u}) event_params(c,e,{0x80,0,0,0},{e==5?ONE:e==8?HALF:0x3fc00000u,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::scorpion_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props) {
  Depth guard(depth_); if (c>UINT32_MAX-0x3c8u) throw Unsupported("scorpion raw32 span");
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x502238,props,0x41200000);
  write(c+0x1c,4,0x41f00000); write(c+0x48,4,0x50fe88); write(c+0xb0,4,0x3f999999); write(c+0x28,1,0x46);
  write(c+0xb8,4,0); write(c+0x232,2,0); write(c+0xb4,1,0x12); write(c+0xec,4,u32(0x44fdcc)); write(c+0x64,4,0x3ecccccc);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000001000)); write(c+0xe8,4,0);
  auto step=mul(from_int(random(c)%6),0x3d4ccccc); auto scale=d2f(dadd(f2d(step),UINT64_C(0x3fe8000000000000)));
  write(c+0xdc,4,scale); write(c+0x1c,4,mul(u32(c+0x1c),scale)); write(c+0x3c8,1,random(c)&1);
  auto mask=call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+0xc),u32(c+0xe8)}); write(c+0x220,8,mask);
  call("anim_clear",{static_cast<Raw>(c+0x114)}); call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x84),1u});
  call("obj_runEvery60",{static_cast<Raw>(c)});
  for (Raw e:{5u,6u,7u,8u}) event_params(c,e,{1,0,0,0},{0x3e999999,0,0,0});
  for (Raw e:{11u,12u}) event_params(c,e,{0x80,0,0,0},{HALF,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::badger_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props, Raw variant) {
  Depth guard(depth_); if (c>UINT32_MAX-0x3ccu) throw Unsupported("badger constructor raw32 span");
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x4fecf8+0xa0*variant,props,0x41200000);
  write(c+0x48,4,0x510f30); write(c+0x1c,4,0x420c0000); write(c+0x28,1,0x28); write(c+0xb8,4,0); write(c+0xb0,4,ONE);
  write(c+0x232,2,0); write(c+0xb4,1,3); write(c+0xec,4,u32(0x44fdcc)); write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x200000000)); write(c+0x3cc,1,variant);
  if (signed8(variant)==1) { write(c+0x60,4,ONE); write(c+0x5c,4,0xbe999999); write(c+0xdc,4,0x3f4ccccc); write(c+0xe0,4,0xc1200000); write(c+0x1c,4,0x41c80000); }
  else { write(c+0x28,1,0x2d); write(c+0xe0,4,0xc1700000); }
  write(c+0xe8,4,0); auto mask=call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+0xc),0u}); write(c+0x220,8,mask);
  call("anim_clear",{static_cast<Raw>(c+0x114)}); call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x7c),1u});
  call("obj_runEvery60",{static_cast<Raw>(c)});
  for (Raw e:{4u,5u,6u,7u}) event_params(c,e,{1,2,0,0},{0x3e999999,0x3f4ccccc,0,0});
  event_params(c,10,{1,2,0,0},{HALF,0x3f666666,0,0});
  for (Raw e:{11u,12u}) event_params(c,e,{0x84,0,0,0},{ONE,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
// Attack-resolution family. Inherited movement shares memory and forwards each
// observation before host callbacks, retaining exact prefixes on exceptions.
std::uint64_t Model::see(Address c, Raw arc, Raw far, Raw near,
                         std::optional<Raw> facing) {
  creature_span(c);
  const auto face = facing.value_or(static_cast<Raw>(mem.load(c+0x38,2)));
  const auto node=u32(c+0x234);
  if (!node) return 0;
  return call("creature_canSeeObj",{static_cast<Raw>(c),u32(static_cast<Address>(node)+12),
    static_cast<Raw>(s16(face)),static_cast<Raw>(s16(arc)),0u,near,far,0u});
}
void Model::dead_update(Address c, Raw ev, Raw flag, Raw size, Vec3 push) {
  creature_span(c);
  call("Creature_deadUpdate",{static_cast<Raw>(c),ev,push,flag,size});
}
Raw Model::badger_pick(Address c) {
  creature_span(c);
  const auto pick=random(c)%5;
  return pick==0 ? 8 : pick==1 ? 9 : 6;
}
void Model::turn_to_target(Address c) {
  const auto node=u32(c+0x234);
  if (!node) return;
  auto table=u32(c+0x48);
  auto self=static_cast<Raw>(c+s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*26,2))));
  const auto point=vec(static_cast<Address>(u32(static_cast<Address>(node)+12))+0x10);
  if (u32(static_cast<Address>(table)+8*26+4)==0x10bb08 && !external.count("Creature.v26")) {
    // The same bridge executes the independently ported turning body.
    creature_movement::Model inherited(mem,policy);
    try { inherited.turn_toward(self,0,point); }
    catch (...) {
      for (const auto &w:inherited.writes) {
        if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
        writes.push_back(w);events.push_back({false,w,{}});
      }
      throw;
    }
    for (const auto &w:inherited.writes) {
      if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
      writes.push_back(w);events.push_back({false,w,{}});
    }
  } else {
    // Known model bodies in a wrong slot refuse, just as the existing API does.
    const auto fn=u32(static_cast<Address>(table)+8*26+4);
    for (auto body:{0x2ab5d8u,0x2ab704u,0x2ab8e0u,0x267654u,0x267a0cu,0x297b30u,
                   0x297f54u,0x2a5068u,0x2a55ecu,0x244380u,0x244aa4u,0x244a9cu,
                   0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,
                   0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
      if (fn==body && !external.count("Creature.v26")) throw Unsupported("mismatched modelled turn body");
    call("Creature.v26",{self,0u,point});
  }
}
Structured Model::movement_virtual(Address obj,unsigned slot,std::vector<Structured> args) {
  namespace V=creature_movement;
  const auto table=u32(obj+0x48),fn=u32(static_cast<Address>(table)+8*slot+4);
  const auto self=static_cast<Raw>(obj+s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*slot,2))));
  if (fn==0x2ab8e0 && slot==6 && !external.count("Creature.v6")) { smallspider_on_touch(self,static_cast<Raw>(args.at(0).scalar));return {}; }
  if (!external.count("Creature.v"+std::to_string(slot)) &&
      ((slot==6 && fn==0x11a418) || (slot==10 && fn==0x11a3f0) ||
       (slot==11 && (fn==0x11a448 || fn==0x244a9c)))) {
    if (slot==6 && (u32(static_cast<Address>(static_cast<Raw>(args.at(0).scalar))+12)&0x40))
      write(static_cast<Address>(self)+0xa8,8,mem.load(static_cast<Address>(self)+0xa8,8)|UINT64_C(0x800000000000000));
    return {};
  }
  for (auto body:{0x2ab5d8u,0x2ab704u,0x2ab8e0u,0x267654u,0x267a0cu,0x297b30u,
                 0x297f54u,0x2a5068u,0x2a55ecu,0x244380u,0x244aa4u,0x244a9cu,
                 0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,
                 0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
    if (fn==body && !external.count("Creature.v"+std::to_string(slot))) throw Unsupported("unported or mismatched movement virtual body");
  std::vector<Argument> values{self};
  for (const auto &a:args) {
    if (a.kind==V::ValueKind::Integer) values.emplace_back(static_cast<Raw>(a.scalar));
    else if (a.kind==V::ValueKind::Vector) values.emplace_back(a.vec);
    else values.emplace_back(a);
  }
  call("Creature.v"+std::to_string(slot),std::move(values));return {};
}
void Model::movement(Address c,bool settle) {
  namespace V=creature_movement;
  V::Model inherited(mem,policy);
  std::size_t cursor=0;
  auto flush=[&] {
    while (cursor<inherited.events.size()) {
      const auto &e=inherited.events[cursor++];
      if (e.kind==V::EventKind::Write) {
        if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
        writes.push_back(e.write);events.push_back({false,e.write,{}});
      }
      // Calls are recorded by the forwarding handler, after the preceding writes.
    }
  };
  inherited.virtual_dispatch=[&](auto obj,auto slot,auto args) {
    flush();return movement_virtual(obj,slot,std::move(args));
  };
  for (const auto &name:{"water_disturb","water_test","libc_sprintf","res_find","sound_playOnObject",
                        "world_slideMove","obj_updateGridCell","world_collectObjectsInBox","terrain_heightBelow"}) {
    inherited.callbacks[name]=[&,name](auto &,const auto &args) {
      flush();std::vector<Argument> values;
      for (const auto &a:args) {
        if (a.kind==V::ValueKind::Integer) values.emplace_back(static_cast<Raw>(a.scalar));
        else if (a.kind==V::ValueKind::Vector) values.emplace_back(a.vec);
        else values.emplace_back(a);
      }
      auto it=structured_callbacks.find(name);
      if (it!=structured_callbacks.end()) {
        record_call({name,values});auto cb=it->second;return cb(*this,values);
      }
      // Structured services cannot be faked by an integer return.
      if (std::string(name)=="world_collectObjectsInBox" || std::string(name)=="libc_sprintf" ||
          (std::string(name)=="world_slideMove" && std::get<Raw>(values.at(3)))) {
        record_call({name,values});
        if (policy==CallbackPolicy::RequireHandlers) throw creature_hp::MissingHandler("missing structured monster callback: "+std::string(name));
        if (std::string(name)=="world_collectObjectsInBox") return V::Value::list({});
        if (std::string(name)=="libc_sprintf") return V::Value::buffer({});
        return V::Value::tuple({V::Value::integer(0),V::Value::vector({})});
      }
      return V::Value::integer(static_cast<std::int64_t>(call(name,std::move(values))));
    };
  }
  try { if (settle) inherited.settle(c);else inherited.move_by(c,{}); }
  catch (...) { flush();throw; }
  flush();
}
void Model::scorpion_attack(Address c,Raw ev) {
  Depth guard(depth_);creature_span(c);
  bool count_down=true;
  if (u32(c+0x234)) {
    if (ev&1) {
      Raw flags=8;Argument descriptor=0u;
      if (s16(static_cast<Raw>(mem.load(c+0x232,2)))==7) {
        const auto level=u32(0x4f719c);
        if (level==2 || level==3) {
          const auto divisor=level==2 ? 4 : 8;
          for (auto off:{0xd0,0xd2}) {
            const auto value=s16(static_cast<Raw>(mem.load(c+off,2)));
            write(c+off,2,static_cast<Raw>(value/divisor));
          }
        }
        flags=0x10004;
        descriptor=Descriptor{{0x17,static_cast<Raw>(mem.load(c+0xd0,2)),static_cast<Raw>(mem.load(c+0xd2,2)),0}};
      }
      call("Creature_hitTargetAtReach",{static_cast<Raw>(c),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd0,2)))),
        static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd2,2)))),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd4,2)))),flags,descriptor,0x42100000u});
    }
    if (anim_flags(c,8)) { turn_to_target(c);count_down=!anim_flags(c,8); }
  } else count_down=!anim_flags(c,8);
  if (count_down) write(c+0xf4,4,u32(c+0xf4)-1);
  if (s32(u32(c+0xf4))>0) return;
  const auto draw=random(c);virtual_call(c,27,draw&1 ? 23 : draw&2 ? 24 : 3);
}
void Model::badger_attack(Address c,Raw ev) {
  Depth guard(depth_);
  if (c>UINT32_MAX-0x3cfu) throw Unsupported("badger raw32 span");
  movement(c,true);
  auto state=[&]{return s16(static_cast<Raw>(mem.load(c+0x232,2)));};
  if (ev&1) {
    Raw flags=8;const auto st=state();
    if (signed8(mem.load(c+0x3cc,1))!=1 && (st==6 || st==7 || st==10))
      flags=random(c)%3==0 ? 0x80008 : 0x40008;
    call("Creature_hitAllEnemiesInArc",{static_cast<Raw>(c),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))),7000u,
      static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd0,2)))),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd2,2)))),
      static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd4,2)))),flags,0u,0u,add(u32(c+0x1c),0x41c80000)});
    write(c+0x3c8,4,1);
  }
  if (u32(c+0x234)) {
    auto st=state();
    if (st==6 && !u32(c+0x3c8) && anim_flags(c,8)) { if (see(c,7000,0x41200000)) turn_to_target(c);st=state(); }
    if (st==10) { if (anim_flags(c,8) && see(c,7000,0x41200000)) turn_to_target(c);st=state(); }
    if (st==7 && anim_flags(c,8) && see(c,7000,0x41200000)) turn_to_target(c);
  }
  if ((ev&2) && state()!=6 && see(c,7000,0x42700000)) virtual_call(c,27,6);
  if (!anim_flags(c,8)) virtual_call(c,27,3);
}
} // namespace champions::simulation::rta::monsters_1
