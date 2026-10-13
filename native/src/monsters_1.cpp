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
std::uint64_t Model::call(const std::string &name, std::vector<Argument> args) {
  if (calls.size() >= history_limit || events.size() >= history_limit)
    throw Unsupported("monster history limit");
  Call c{name, std::move(args)};
  calls.push_back(c); events.push_back({true, {}, c});
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
    auto d = random(c); write(c+0xf4,4,u32(c+0x300)+d%300+120); start(c,0x3c,3);
  } else if (state == 1) { clear_path(c); wander(c); start(c,0x4c,3); }
  else if (state == 5 || state == 16 || state == 18 || state == 19) state = s16(static_cast<Raw>(mem.load(c+0x232,2)));
  else if (state == 20) {
    write(c+0xc,4,u32(c+0xc)|9);
    auto angle = random(c); auto size = random(c)%10+10;
    call("decal_add",{vec(c+0x10),static_cast<Raw>(s16(angle)),6u,1u,from_int(size)});
    start(c,0x1c+8*(random(c)&3),6);
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
  else if (state == 19) start(c,0x1c,3);
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
  if (state == 0) { auto clock=u32(c+0x300); auto d=random(c); write(c+0xf4,4,clock+d%60+120); start(c,0x84,3); }
  else if (state == 1) { clear_path(c); wander(c); start(c,0x94,0x803); }
  else if (state == 3 || state == 16) { clear_path(c); start(c,0x9c,0x803); }
  else if (state == 5) start(c,0x8c,3,0x3dcccccc);
  else if (state == 6) {
    start(c,0x8c,3,0x3dcccccc);
    if (random(c)%6==0) { state=7; swing(c,0x44,8); }
    else swing(c,0x2c+8*(random(c)%3),8);
    write(c+0xf4,4,random(c)%20);
  } else if (state == 18) {
    auto current=s16(static_cast<Raw>(mem.load(c+0x232,2)));
    if (current==19 || (current==6 && (mem.load(c+0x11a,2)&8))) state=current;
    else swing(c,0x7c,0x10);
  } else if (state == 19) start(c,0x84,3);
  else if (state == 20) {
    if (random(c)%3==0) scorpion_sever_part(c,random(c)%6);
    drop_loot(c,0x54b648,0x3fcccccc,0x3f199999,0x3fb33333);
    start(c,0x5c+8*(random(c)&1),0x806); virtual_call(c,28,0);
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
    if (s16(static_cast<Raw>(mem.load(c+0x232,2)))==6 && (mem.load(c+0x11a,2)&8)) state=6;
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
} // namespace champions::simulation::rta::monsters_1
