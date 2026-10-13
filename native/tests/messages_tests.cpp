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
        for(auto channel:{5u,7u}) {
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
        },{},{}});
        active=&nested; nested.register_all(); nested.deliver(0,{1});
        check(nested.calls.size()==2 && nested.calls[1].name=="voice_onMsg");
        check(encode(Kind::load_level).payload==Bytes({11,0,0,0}));
        // Missing named services record calls and return zero.
        Memory controls; Machine ctl(controls);
        constexpr std::uint32_t r=0x4d8af0, a=r+0x4440;
        controls.poke(r+0x4b08,2,0xffff); controls.poke(r+0x4b0c,2,0xffff);
        ctl.set_skill_rank(0,7,-2); check(ctl.skill_rank(0,7)==-2 && ctl.skill_rank(0,8)==0);
        check(controls.load(r+0x4b0c,2)==0xffff); // no invented terminator write
        controls.poke(r+0x4a84,1,2); controls.poke(r+0x4a85,1,3);
        ctl.on_quick_slot(0,0,3);
        check(controls.load(r+0x4a84,1)==3 && controls.load(r+0x4a85,1)==2);
        check(ctl.calls.size()==1 && ctl.calls[0].arguments==std::vector<std::uint64_t>({0}));
        controls.poke(a+0x25,1,1); ctl.on_quick_cast(0,0);
        check(ctl.calls.back().name=="hud_showMessage" && ctl.calls.back().arguments[0]==0);
        controls.poke(a+0x25,1,2); controls.poke(0x58c,4,2); ctl.on_quick_cast(0,0);
        check(controls.load(0x488,4)==a); // null object is still model memory
        // Sound failure retains call prefix and occurs before quick-slot stores.
        Memory failing; Machine fail(failing,{{},{},
            [&](const std::string& name,const std::vector<std::uint64_t>&)->std::uint64_t {
                if(name=="res_find") throw std::runtime_error("sound failed");
                return 1;
            },{}});
        bool threw=false;
        try { fail.on_quick_slot(0,0,1); } catch(const std::runtime_error&) { threw=true; }
        check(threw && failing.writes.empty() && fail.calls.size()==2);
        // Named callbacks can reenter and change memory before the outer body reads it.
        Memory reentrant; Machine* control_active=nullptr;
        Machine control_nested(reentrant,{{},{},
            [&](const std::string& name,const std::vector<std::uint64_t>&)->std::uint64_t {
                if(name=="net_isLocalSlot") control_active->mask_local_buttons(0,70,0xaa);
                return 0;
            },{}});
        control_active=&control_nested; control_nested.on_quick_slot(0,0,5);
        check(reentrant.writes.size()==4 && reentrant.load(0x5d0bc8,4)==70);
        // HUD dispatch uses the caller's stale tail; offline send adds zeros.
        Memory hud_memory; Machine hud(hud_memory); hud.register_all();
        hud_memory.poke(0x44f208,1,7);
        hud.deliver(3,{11,0,0,0,'a','b',0,'x'});
        check(hud_memory.load(0x41b6d0,2)==0x6261 && hud_memory.load(0x44f208,1)==0);
        hud.send(3,{11,0,0,0}); check(hud_memory.load(0x41b6d0,1)==0);
        auto writes=hud_memory.writes.size(); hud.hud_on_msg({255});
        check(hud_memory.writes.size()==writes);
        hud.stop_stream(); check(hud.calls.back().name=="audio_streamStatus");
        // Built text is a byte argument, never a fake resource address.
        Memory text_memory; bool typed_seen=false;
        Machine text(text_memory,{{},{},{},
            [&](const std::string& name,const std::vector<Argument>& args)->std::uint64_t {
                if(name=="hud_showMessage") {
                    typed_seen=std::get<Bytes>(args[0])==Bytes({'A',0,' ',0,0x34,0x12});
                }
                return name=="text_lookup" ? 0x10000 : 0;
            }});
        text_memory.poke(r,1,'A'); text_memory.poke(0x53c290,1,' ');
        text_memory.poke(0x10000,2,0x1234); text.on_player_left(0);
        check(typed_seen && text.calls.back().typed_arguments.size()==4);
        // Two mismatched checksum words disconnect twice; failures keep stores.
        Memory checksum_memory; Machine checksum(checksum_memory,{
            [](std::uint32_t){return 1u;},{},{},{}});
        checksum_memory.poke(0x4d8ae0,4,1); checksum.on_checksum(0,1,2);
        check(checksum.calls.size()==4 && checksum.calls[1].name=="net_disconnectPlayer"
            && checksum.calls[3].name=="net_disconnectPlayer" && checksum_memory.load(0x4f7144,4)==1);
        // Lifecycle services can reenter; subsequent outer stores retain order.
        Memory lifecycle_memory; Machine* lifecycle_active=nullptr;
        Machine lifecycle(lifecycle_memory,{{},{},
            [&](const std::string& name,const std::vector<std::uint64_t>&)->std::uint64_t {
                if(name=="pauseMenu_close") lifecycle_active->hud_on_msg({12});
                return 0;
            },{}});
        lifecycle_active=&lifecycle; lifecycle.set_pending_level({'n'},true);
        check(lifecycle_memory.load(0x4f7140,4)==1 && lifecycle_memory.writes[3].address==0x4f7140);
        // A typed callback exception preserves the text lookup call prefix.
        Memory typed_failure_memory; Machine typed_failure(typed_failure_memory,{{},{},{},
            [](const std::string& name,const std::vector<Argument>&)->std::uint64_t {
                if(name=="hud_showMessage") throw std::runtime_error("text failed");
                return 0;
            }});
        threw=false;
        try { typed_failure.on_player_left(0); } catch(const std::runtime_error&) { threw=true; }
        check(threw && typed_failure.calls.size()==2 && typed_failure_memory.writes.empty());
        std::cout<<checks<<" messages API checks passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
