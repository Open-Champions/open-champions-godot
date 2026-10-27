#pragma once
#include "champions/amx.hpp"
#include "champions/dialog.hpp"
#include "champions/script_builtins.hpp"
#include "champions/quests.hpp"
#include <optional>

namespace champions::simulation::rta::hub_offers {
// All authority below: research-mission-unlocks@950b2538
// docs/spec/mission-unlocks.md. These are dialog nodes, NOT mission adjacency.
enum class Kind { Active, Offer, Pending, NormalFalse };
struct Predicate { int offset, mission; Kind kind; };
inline constexpr std::array<Predicate, 31> predicates{{
    {0x3f0,0,Kind::Active}, // :46-55 (calls 0x3524)
    {0x11c,1,Kind::Offer}, {0x11c,1,Kind::Offer}, // :56
    {0x198,1,Kind::Pending}, // :57
    {0x498,0,Kind::NormalFalse}, {0x498,0,Kind::NormalFalse}, // :58
    {0x680,0,Kind::NormalFalse}, // :59; normal mode only, :81-84
    {0x734,2,Kind::Offer}, {0x898,2,Kind::Pending}, // :60-61
    {0x9a0,3,Kind::Offer}, {0x9a0,3,Kind::Offer}, {0xb04,3,Kind::Pending}, // :62-63
    {0xc0c,4,Kind::Offer}, {0xd70,4,Kind::Pending}, // :64-65
    {0xe78,5,Kind::Offer}, {0xfdc,5,Kind::Pending}, // :66-67
    {0x10e4,6,Kind::Offer}, {0x10e4,6,Kind::Offer}, {0x1248,6,Kind::Pending}, // :68-69
    {0x1350,7,Kind::Offer}, {0x14b4,7,Kind::Pending}, // :70-71
    {0x15bc,8,Kind::Offer}, {0x1638,9,Kind::Offer}, // :72-73; no grants!
    {0x16b4,10,Kind::Offer}, {0x16b4,10,Kind::Offer}, {0x1818,10,Kind::Pending}, // :74-75
    {0x1920,11,Kind::Offer}, {0x1a84,11,Kind::Pending}, // :76-77
    {0x1b8c,12,Kind::Offer}, {0x1b8c,12,Kind::Offer}, {0x1cf0,12,Kind::Pending} // :78-79
}};
// Private string-pool offsets, resolved from the supplied script, never names.
// :57,61,63,65,67,69,71-73,75,77,79 respectively; signed values :39-48,120-125.
inline constexpr std::array<int,12> variables{{
    0x6c8,0x6d8,0x6e8,0x6f8,0x708,0x718,0x728,0x738,0x748,0x758,0x76c,0x780}};

struct Binding {
  std::array<std::string,31> conditions;
  std::array<std::string,12> names;
  // Match the entire documented node/public structure, not a file/actor name.
  // This does not establish physical resource binding/reachability (:26-33,129-142).
  static std::optional<Binding> match(amx::Machine &machine, const std::vector<dialog::Node> &nodes) {
    if (nodes.size() != predicates.size()) return std::nullopt;
    const auto first=machine.find_public(nodes[0].condition);
    if (first.first!=amx::ERR_NONE || machine.public_entry(first.second).second!=0x3f0)
      return std::nullopt;
    Binding b;
    for (std::size_t i=0; i<nodes.size(); ++i) {
      const auto entry = machine.find_public(nodes[i].condition);
      if (entry.first != amx::ERR_NONE || machine.public_entry(entry.second).second != predicates[i].offset)
        throw dialog::Refused("hub predicate structure differs from pinned research");
      b.conditions[i] = nodes[i].condition;
    }
    // Both second-offer dispatches, :107-112. Do not accept an offset collision
    // in an unrelated script as authority for a grant.
    if (nodes[7].choices.size()!=2) throw dialog::Refused("hub offer choice structure is unproved");
    for (std::size_t i=0; i<2; ++i) {
      const auto entry=machine.find_public(nodes[7].choices[i].action);
      if (entry.first!=amx::ERR_NONE || machine.public_entry(entry.second).second != (i==0 ? 0x7b0 : 0x2064))
        throw dialog::Refused("hub offer action differs from pinned research");
    }
    for (std::size_t i=0; i<variables.size(); ++i) {
      b.names[i]=machine.get_string(variables[i]);
      if (b.names[i].empty()) throw dialog::Refused("hub offer variable is unbound");
    }
    return b;
  }
  bool evaluate(std::size_t node, builtins::Variables &state) const {
    const auto d=amx::s32(state.memory.word(0x4f719c)); // :46-48; tools/difftest_mission_unlocks.py:114-120
    // Retain the campaign's existing refusal for imported/out-of-domain rows.
    (void)quests::mission_index(d,1);
    const auto a=[&](int m){ return state.mission_available(d,m)!=0; };
    const auto c=[&](int m){ return state.mission_complete(d,m)!=0; };
    const auto v=[&](int m){ return state.quest_get(names.at(std::size_t(m-1))); }; // :84 creates absent zero
    const auto &p=predicates.at(node);
    switch(p.kind) {
    case Kind::NormalFalse: return false;
    case Kind::Offer: return !a(p.mission);
    case Kind::Pending: return a(p.mission) && !c(p.mission) && v(p.mission)==0;
    case Kind::Active:
      for(int m=1;m<=12;++m) if(a(m) && !c(m) && v(m)!=0) return true;
      return false;
    }
    throw dialog::Refused("unknown hub predicate");
  }
  int start(builtins::Variables &state) const {
    // :49-51 node zero first, first nonzero, zero fallback. Never travel order.
    for(std::size_t i=0;i<predicates.size();++i) if(evaluate(i,state)) return int(i);
    return 0;
  }
};
} // namespace champions::simulation::rta::hub_offers
