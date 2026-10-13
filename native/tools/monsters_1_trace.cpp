// Input-only replay. Expected fixture columns are never read by this runner.
#include "../tests/monsters_1_transport.hpp"
#include <fstream>
#include <iostream>
namespace X = champions::test::monsters_1_transport;
namespace M = champions::simulation::rta::monsters_1;
namespace R = champions::simulation::rta;
using X::n; using X::sequence; using X::text;
namespace {
const std::vector<std::string> names = {"handle_clear","anim_start","decal_add","anim_jumpTopToEnd","Creature.v27","Creature.v28","terrain_heightBelow","model_buildHideMask","pool_new","GutChunk_ctorFromProp","obj_getPropStr","script_runByName","loot_spawnNamedItem","obj_delete","arena_addKill","creature_stopBossMusic"};
std::string call(const M::Call &c) {
  std::vector<std::string> args;
  for (const auto &a:c.args) {
    if (auto i=std::get_if<std::uint32_t>(&a)) args.push_back(n(*i));
    else args.push_back(sequence({text("tuple"),X::integers(std::get<M::Vec3>(a))}));
  }
  return sequence({text(c.name),sequence(args)});
}
std::string write(const M::Write &w) { return sequence({n(w.address),n(w.size),n(w.value)}); }
std::string snapshot(const M::Model &m,const std::vector<unsigned> &counts) {
  std::vector<std::string> bytes,stores,writes,calls,events;
  for (auto [a,v]:m.mem.bytes()) bytes.push_back(sequence({n(a),n(v)}));
  for (auto w:m.mem.log()) stores.push_back(write(w));
  for (auto w:m.writes) writes.push_back(write(w));
  for (const auto &c:m.calls) calls.push_back(call(c));
  for (const auto &e:m.events) {
    if (e.is_call) {
      auto c=call(e.call);
      events.push_back("["+text("call")+","+c.substr(1));
    } else events.push_back(sequence({text("write"),n(e.write.address),n(e.write.size),n(e.write.value)}));
  }
  return sequence({sequence(bytes),sequence(stores),sequence(writes),sequence(calls),sequence(events),n(m.draws),X::integers(counts)});
}
void poke(M::Memory &mem,const X::Value &v,bool logged=false) {
  const auto &a=v.array(3);
  auto addr=a[0].integer<std::uint32_t>();auto size=a[1].integer<unsigned>();auto value=a[2].integer<std::uint64_t>();
  if (size>8 || static_cast<std::uint64_t>(addr)+size>UINT64_C(0x100000000)) throw std::runtime_error("invalid poke span");
  if (logged) mem.store(addr,size,value); else mem.poke(addr,size,value);
}
std::string execute(unsigned config,const std::string &input) {
  if (config>=16) throw std::runtime_error("invalid FPU configuration");
  R::set_fpu(static_cast<R::Fpu::Model>(config/4),(config&2)!=0,(config&1)!=0);
  auto parsed=X::Parser(input).parse();const auto &fields=parsed.array(4);
  M::Memory memory;M::Model model(memory);std::vector<unsigned> counts(names.size());
  for (const auto &spec:fields[1].bounded_array(16)) {
    const auto &s=spec.array(4);auto id=s[0].integer<unsigned>();
    if (id>=names.size() || model.callbacks.count(names[id])) throw std::runtime_error("invalid callback id");
    std::vector<std::uint64_t> returns;
    for (const auto &r:s[1].bounded_array(128)) returns.push_back(r.integer<std::uint64_t>());
    if (returns.empty()) throw std::runtime_error("empty callback returns");
    auto mutations=s[2].bounded_array(128);auto fail=s[3].integer<std::int32_t>();
    model.callbacks[names[id]]=[&,id,returns,mutations,fail](M::Model &m,const auto &) {
      auto index=counts[id]++;
      for (const auto &row:mutations) poke(m.mem,row,true);
      if (static_cast<std::int64_t>(index)==fail) throw M::Error("authored callback failure");
      return returns[index<returns.size()?index:returns.size()-1];
    };
  }
  for (const auto &v:fields[2].bounded_array(2)) {
    auto slot=v.integer<unsigned>();if (slot!=27 && slot!=28) throw std::runtime_error("invalid external slot");
    model.external.insert("Creature.v"+n(slot));
  }
  for (const auto &v:fields[0].bounded_array(512)) poke(memory,v);
  auto initial=snapshot(model,counts);std::vector<std::string> steps;
  for (const auto &v:fields[3].bounded_array(128)) {
    const auto &op=v.bounded_array(4);if (op.empty()) throw std::runtime_error("empty opcode");
    auto code=op[0].integer<unsigned>();std::string status="ok",result="_";
    try {
      if (code<5) {
        v.array(3);auto a=op[1].integer<std::uint32_t>();auto b=op[2].integer<std::uint32_t>();
        switch(code) {
        case 0:model.smallspider_set_state(a,b);break;
        case 1:model.smallspider_on_touch(a,b);break;
        case 2:model.mosquito_lose_part(a,b);break;
        case 3:model.scorpion_sever_part(a,b);break;
        case 4:result=n(model.str_prefix_differs(a,b));break;
        }
      } else if (code==5) {
        v.array(4);X::Value row;row.list=true;row.items={op[1],op[2],op[3]};poke(memory,row);
      } else throw std::runtime_error("unknown opcode");
    } catch (const M::Error &) { status="ValueError"; }
    steps.push_back(sequence({text(status),result,snapshot(model,counts)}));
  }
  return sequence({initial,sequence(steps)});
}
}
int main(int argc,char **argv) {
  try {
    if (argc!=2) throw std::runtime_error("usage: monsters-1-trace INPUT.tsv");
    std::ifstream input(argv[1]);if (!input) throw std::runtime_error("cannot open input");
    std::string line;
    while (std::getline(input,line)) {
      if (line.size()>8*1024*1024) throw std::runtime_error("row budget");
      const auto first=line.find('\t'),second=line.find('\t',first+1);
      if (first==line.npos || second==line.npos) throw std::runtime_error("invalid row");
      const auto config=X::Parser(line.substr(0,first)).parse().integer<unsigned>();
      std::cout << execute(config,line.substr(first+1,second-first-1)) << '\n';
    }
    if (!input.eof() || !std::cout) throw std::runtime_error("transport failure");
  } catch(const std::exception &e) {std::cerr << e.what() << '\n';return 1;}
}
