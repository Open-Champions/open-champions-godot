#include "champions/messages.hpp"
#include <iostream>
using namespace champions::messages::rta;
int main() {
    unsigned checks=0;
    auto check=[&](bool b) { ++checks; if(!b) throw std::runtime_error("messages API check failed"); };
    auto invalid=[&](auto f) { try { f(); } catch(const std::invalid_argument&) { return; } throw std::runtime_error("expected invalid_argument"); };
    try {
        Memory memory; Machine defaults(memory); defaults.register_all();
        defaults.deliver(0,{1,2}); check(defaults.calls.back().payload==Bytes({1,2}));
        defaults.send(4,{9}); check(defaults.calls.size()==1);
        memory.poke(network_mode_address,1,1); defaults.send(255,{3});
        check(defaults.calls.back().name=="net_getLocalPlayerSlot");
        check(memory.load(peers_address+ring_offset,1)==255);
        for(auto channel:{3u,5u,7u}) {
            auto before=memory.bytes; auto writes=memory.writes.size(); auto calls=defaults.calls.size();
            bool refused=false;
            try { defaults.deliver(channel,{0}); } catch(const Unsupported&) { refused=true; }
            check(refused && before==memory.bytes && writes==memory.writes.size() && calls==defaults.calls.size());
        }
        invalid([&] { defaults.deliver(15,{}); });
        invalid([&] { defaults.deliver(0xffffffff,{}); });
        invalid([&] { encode(static_cast<Kind>(27)); });
        invalid([&] { encode(Kind::equip,{1,2}); });
        invalid([&] { encode(Kind::recall,{1},{1}); });
        invalid([&] { memory.store(0,3,0); });
        memory.poke(0xffffffff,8,0x1122334455667788ull);
        check(memory.bytes.at(0x100000006ull)==0x11);
        check(memory.load(0xffffffff,8)==0x1122334455667788ull);
        // Reentrant external callbacks retain the ordered call history.
        Memory nested_memory; Machine* active=nullptr;
        Machine nested(nested_memory,{{},[&](const std::string& name,const Bytes&,std::uint64_t) {
            if(name=="cutscene_onMsg") active->deliver(1,{42});
        }});
        active=&nested; nested.register_all(); nested.deliver(0,{1});
        check(nested.calls.size()==2 && nested.calls[1].name=="voice_onMsg");
        check(encode(Kind::load_level).payload==Bytes({11,0,0,0}));
        std::cout<<checks<<" messages API checks passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
