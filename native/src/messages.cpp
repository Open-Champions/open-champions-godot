#include "champions/messages.hpp"
#include <array>
#include <algorithm>
#include "champions/core_arithmetic.hpp"
#include <utility>

namespace champions::messages::rta {
namespace {
struct Registration { std::uint32_t handler, destination; };
constexpr std::array<Registration, 8> registration{{
    {0x198f64,0x83d1c8}, {0x31f354,0x505b14}, {0x22ee2c,0x8e2e94},
    {0x1d8ca0,0x86ac40}, {0x1e01b0,0x86aa60}, {0x1bfdb0,0x866d18},
    {0x31d82c,0x8e8ed8}, {0x1c5d28,0x86a6a0}
}};
void check_size(unsigned size) {
    if (size != 1 && size != 2 && size != 4 && size != 8)
        throw std::invalid_argument("memory scalar size must be 1, 2, 4 or 8");
}
}
std::uint64_t Memory::load(std::uint32_t address, unsigned size) const {
    check_size(size);
    std::uint64_t value = 0;
    for (unsigned i=0; i<size; ++i) {
        auto it = bytes.find(std::uint64_t(address)+i);
        if (it != bytes.end()) value |= std::uint64_t(it->second) << (8*i);
    }
    return value;
}
void Memory::poke(std::uint32_t address, unsigned size, std::uint64_t value) {
    check_size(size);
    for (unsigned i=0; i<size; ++i)
        bytes[std::uint64_t(address)+i] = std::uint8_t(value >> (8*i));
}
void Memory::store(std::uint32_t address, unsigned size, std::uint64_t value) {
    check_size(size);
    if (size<8) value &= (std::uint64_t(1) << (8*size))-1;
    writes.push_back({address,size,value});
    poke(address,size,value);
}
Machine::Machine(Memory& memory, Callbacks callbacks)
    : memory_(memory), callbacks_(std::move(callbacks)) {}
std::uint32_t Machine::register_handler(std::uint32_t handler) {
    for (std::uint32_t i=0; i<16; ++i)
        if (memory_.load(handlers_address+4*i,4)==handler) return i;
    std::uint32_t i=0;
    while (i<16 && memory_.load(handlers_address+4*i,4)!=0) ++i;
    // The reference writes the seventeenth word when the table is full.
    memory_.store(handlers_address+4*i,4,handler);
    return i;
}
std::vector<std::uint32_t> Machine::register_all() {
    std::vector<std::uint32_t> ids;
    for (auto entry: registration) {
        auto id=register_handler(entry.handler);
        ids.push_back(id);
        memory_.store(entry.destination,4,id);
    }
    return ids;
}
void Machine::send(std::uint32_t channel, const Bytes& data) {
    if (memory_.load(network_mode_address,1)==0) {
        auto buffer=data;
        buffer.insert(buffer.end(),16,0); // Python send's explicit synthetic tail.
        deliver(channel,buffer);
        return;
    }
    calls.push_back({"net_getLocalPlayerSlot",{},0,{}});
    auto slot=callbacks_.local_player_slot ? callbacks_.local_player_slot(0) : 0;
    auto peer=peers_address+slot*peer_size;
    auto ring=peer+ring_offset;
    auto position=std::uint32_t(memory_.load(peer+write_position_offset,4));
    memory_.store(ring+(position&ring_mask),1,channel);
    memory_.store(ring+((position+1)&ring_mask),1,data.size());
    for (std::size_t i=0; i<data.size(); ++i)
        memory_.store(ring+((position+std::uint32_t(i)+2)&ring_mask),1,data[i]);
    memory_.store(peer+write_position_offset,4,(position+std::uint32_t(data.size())+2)&ring_mask);
}
void Machine::deliver(std::uint32_t channel, const Bytes& payload) {
    // Match the actual Python reference: raw32 address arithmetic, no id guard.
    auto handler=memory_.load(handlers_address+4*channel,4);
    if (handler==0x1e01b0) return;
    if (handler==0x1d8ca0) { hud_on_msg(payload); return; }
    if (handler==0x1bfdb0 || handler==0x1c5d28)
        throw Unsupported("inventory/shop execution is not ported");
    std::string name;
    switch (handler) {
    case 0x198f64: name="cutscene_onMsg"; break;
    case 0x31f354: name="voice_onMsg"; break;
    case 0x22ee2c: name="dialog_onMsg"; break;
    case 0x31d82c: name="text_onMsg"; break;
    default: throw std::invalid_argument("no handler for channel");
    }
    calls.push_back({name,payload,payload.size(),{}});
    if (callbacks_.external_handler) callbacks_.external_handler(name,payload,payload.size());
}
namespace {
constexpr std::uint32_t players=0x4d8af0, abilities=0x4440, ability_size=0x68;
constexpr std::uint32_t skill_table=0x41b170, class_skills=0x4ef538;
constexpr std::uint32_t old_abilities=0x4f5cc8, marker=0x53b358, hud_marker=0x53c7d0;
constexpr std::uint32_t table_end=0x53b368;
std::int32_t signed32(std::uint64_t x) {
    auto v=std::uint32_t(x);
    return v<0x80000000u ? std::int32_t(v) : std::int32_t(std::int64_t(v)-0x100000000ll);
}
std::int32_t signed16(std::uint64_t x) {
    auto v=std::uint16_t(x);
    return v<0x8000 ? v : std::int32_t(v)-0x10000;
}
bool contains(const Bytes& haystack,const Bytes& needle) {
    return std::search(haystack.begin(),haystack.end(),needle.begin(),needle.end())!=haystack.end()
        || needle.empty();
}
}
std::uint32_t Machine::rec(std::int64_t slot) const {
    return players+std::uint32_t(slot)*0x4c30u;
}
std::int32_t Machine::signed_load(std::uint32_t address,unsigned size) const {
    auto v=memory_.load(address,size);
    if(size==1) return v<0x80 ? std::int32_t(v) : std::int32_t(v)-0x100;
    if(size==2) return signed16(v);
    return signed32(v);
}
Bytes Machine::cstr(std::uint32_t address,std::int64_t limit) const {
    Bytes out;
    while(limit>0 && out.size()<std::uint64_t(limit)) {
        auto b=std::uint8_t(memory_.load(address+std::uint32_t(out.size()),1));
        if(!b) break;
        out.push_back(b);
    }
    return out;
}
std::uint64_t Machine::call(const std::string& name,std::vector<std::uint64_t> arguments) {
    if(name=="net_getLocalPlayerSlot") {
        calls.push_back({name,{},arguments.at(0),{}});
        return callbacks_.local_player_slot ? callbacks_.local_player_slot(std::uint32_t(arguments.at(0))) : 0;
    }
    calls.push_back({name,{},0,arguments,{},true});
    if(callbacks_.typed_service) {
        std::vector<Argument> typed;
        for(auto a:arguments) typed.emplace_back(a);
        return callbacks_.typed_service(name,typed);
    }
    return callbacks_.service ? callbacks_.service(name,arguments) : 0;
}
std::uint64_t Machine::typed_call(const std::string& name,std::vector<Argument> arguments) {
    calls.push_back({name,{},0,{},arguments,true});
    return callbacks_.typed_service ? callbacks_.typed_service(name,arguments) : 0;
}
Bytes Machine::wstr(std::uint32_t address,std::int64_t limit) const {
    Bytes out;
    while(limit>0 && out.size()<std::uint64_t(limit)) {
        auto c=memory_.load(address+std::uint32_t(out.size()),2);
        if(!c) break;
        out.push_back(std::uint8_t(c)); out.push_back(std::uint8_t(c>>8));
    }
    return out;
}
Position Machine::vec(std::uint32_t address) const {
    return {std::uint32_t(memory_.load(address,4)),std::uint32_t(memory_.load(address+4,4)),
        std::uint32_t(memory_.load(address+8,4))};
}
void Machine::copy(std::uint32_t dst,std::uint32_t src,std::int64_t size) {
    for(std::int64_t i=0;i<size;) {
        memory_.store(dst+std::uint32_t(i),8,memory_.load(src+std::uint32_t(i),8));
        if(size-i<=8) break;
        i+=8;
    }
}
void Machine::sound(std::uint32_t lump,std::uint32_t entry) {
    auto resource=call("res_find",{lump,entry});
    call("sound_playOnObject",{resource,0,0x3f800000});
}
std::int32_t Machine::skill_rank(std::int64_t slot,std::int64_t skill_id) {
    auto a=rec(slot)+0x4b08;
    while(signed_load(a,2)!=-1) {
        if(signed_load(a,2)==signed16(skill_id)) return signed_load(a+2,2);
        a+=4;
    }
    return 0;
}
void Machine::set_skill_rank(std::int64_t slot,std::int64_t skill_id,std::int64_t rank) {
    auto a=rec(slot)+0x4b08;
    while(signed_load(a,2)!=-1) {
        if(signed_load(a,2)==signed32(skill_id)) { memory_.store(a+2,2,rank); return; }
        a+=4;
    }
    memory_.store(a,2,skill_id); memory_.store(a+2,2,rank);
}
std::int32_t Machine::passive_level(std::uint64_t slot,std::uint64_t mask) {
    if(slot>4) return 0;
    auto r=rec(std::int64_t(slot));
    auto table=class_skills+std::uint32_t(memory_.load(r+0x50,4))*0xa88u;
    for(unsigned n=0;n<48;++n) if(memory_.load(table+n*0x38,8)==mask) {
        return std::max(0,signed_load(r+0xef+n,1));
    }
    return 0;
}
void Machine::raise_passive_level(std::int64_t slot,std::uint64_t mask,std::int64_t level) {
    auto r=rec(slot), table=class_skills+std::uint32_t(memory_.load(r+0x50,4))*0xa88u;
    for(unsigned n=0;n<48;++n) if(memory_.load(table+n*0x38,8)==mask) {
        if(signed_load(r+0xef+n,1)<signed32(level)) memory_.store(r+0xef+n,1,level);
        return;
    }
}
void Machine::rebuild_abilities(std::uint64_t mask,std::int64_t slot,std::int64_t requested) {
    namespace C=champions::simulation::rta;
    auto r=rec(slot); auto index=signed32(requested);
    if(index==-1) {
        auto owned=memory_.load(r+0x128,8)&mask;
        for(unsigned i=0;i<15;++i) if(!owned && signed_load(r+abilities+i*ability_size+0x20,2)==-1) {
            index=std::int32_t(i); break;
        }
    }
    if(index==-1) return;
    auto target=r+abilities+std::uint32_t(index)*ability_size;
    auto end=cstr(table_end);
    for(auto row=old_abilities;cstr(row)!=end;row+=ability_size) {
        if(mask!=memory_.load(row+0x48,8)) continue;
        if(signed_load(row+0x20,2)!=0x1d) {
            auto name=cstr(row); name.push_back(0);
            for(std::size_t i=0;i<name.size();++i) memory_.store(target+std::uint32_t(i),1,name[i]);
            for(auto offset:{0x20u,0x22u}) memory_.store(target+offset,2,memory_.load(row+offset,2));
            for(auto offset:{0x24u,0x25u}) memory_.store(target+offset,1,memory_.load(row+offset,1));
            for(auto offset:{0x26u,0x28u}) memory_.store(target+offset,2,memory_.load(row+offset,2));
            memory_.store(target+0x2c,4,memory_.load(row+0x2c,4));
            for(unsigned k=0;k<8;++k) memory_.store(target+0x30+2*k,2,memory_.load(row+0x30+2*k,2));
            memory_.store(target+0x40,4,memory_.load(row+0x40,4));
            memory_.store(target+0x48,8,memory_.load(row+0x48,8));
            for(unsigned k=0;k<5;++k) memory_.store(target+0x50+4*k,4,memory_.load(row+0x50+4*k,4));
        }
        auto u=[&](unsigned o){return std::uint32_t(memory_.load(r+o,4));};
        if(mask==0x40) memory_.store(r+0x124,4,C::add(u(0x124),0x41700000));
        else if(mask==0x80) {
            auto hp=C::add(u(0xac),0x40400000), top=C::add(u(0xb0),0x40400000);
            memory_.store(r+0xb0,4,top); memory_.store(r+0xac,4,hp);
            if(C::lt(top,hp)) memory_.store(r+0xac,4,top);
        } else if(mask==0x100) {
            memory_.store(r+0xc4,4,C::add(u(0xc4),0x40a00000));
            memory_.store(r+0xc0,4,C::add(u(0xc0),0x40a00000));
            if(C::lt(u(0xc4),u(0xc0))) memory_.store(r+0xc0,4,u(0xc4));
        }
        memory_.store(r+0x128,8,memory_.load(r+0x128,8)|mask);
        return;
    }
}
void Machine::add_skill_ability(std::int64_t requested,std::int64_t slot) {
    auto skill_id=signed32(requested); auto r=rec(slot);
    auto skill=std::uint32_t(memory_.load(skill_table+std::uint32_t(skill_id)*4,4));
    if(memory_.load(skill+8,4)==3) return;
    int free=-1;
    for(unsigned i=0;i<15;++i) if(signed_load(r+abilities+i*ability_size+0x20,2)==-1) {free=int(i);break;}
    auto name=cstr(marker);
    for(unsigned i=0;i<15;++i) {
        auto a=r+abilities+i*ability_size;
        if(signed_load(a+0x20,2)==skill_id && cstr(a)==name) free=-1;
    }
    if(free==-1) return;
    auto target=r+abilities+std::uint32_t(free)*ability_size;
    for(unsigned i=0;i<ability_size;++i) memory_.store(target+i,1,0);
    memory_.store(target+8,1,memory_.load(marker+8,1));
    memory_.store(target,8,memory_.load(marker,8));
    memory_.store(target+0x20,2,skill_id); memory_.store(target+0x24,1,1);
    memory_.store(target+0x25,1,memory_.load(skill+8,1));
    auto rank=skill_rank(slot,skill_id);
    set_skill_rank(slot,skill_id,std::max(rank,1));
}
void Machine::on_quick_slot(std::int64_t slot,std::int64_t button,std::int64_t index) {
    auto r=rec(slot);
    if(call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x53c3b8,0x53c858);
    auto mine=button==0 ? 0x4a84u : 0x4a85u, other=button==0 ? 0x4a85u : 0x4a84u;
    if(signed_load(r+other,1)==index) memory_.store(r+other,1,memory_.load(r+mine,1));
    memory_.store(r+mine,1,index);
}
void Machine::on_assign_spell(std::int64_t slot,std::int64_t index,std::int64_t kind,std::uint64_t value) {
    auto r=rec(slot), target=r+abilities+std::uint32_t(index)*ability_size;
    int found=-1;
    for(unsigned i=0;i<8;++i) {
        auto a=r+abilities+i*ability_size;
        if(kind==1 ? std::uint64_t(std::int64_t(signed_load(a+0x20,2)))==value : memory_.load(a+0x48,8)==value)
            found=int(i);
    }
    if(found!=-1) {
        auto old=r+abilities+std::uint32_t(found)*ability_size;
        auto held=memory_.load(target+0x20,2);
        memory_.store(old+0x48,8,UINT64_MAX); memory_.store(old+0x20,2,held);
        if(signed16(held)!=-1) {
            if(contains(cstr(target),cstr(hud_marker))) {
                memory_.store(old,8,memory_.load(hud_marker,8)); memory_.store(old+8,1,memory_.load(hud_marker+8,1));
                auto skill=std::uint32_t(memory_.load(skill_table+std::uint32_t(signed_load(target+0x20,2))*4,4));
                memory_.store(old+0x25,1,memory_.load(skill+8,1));
                memory_.store(old+0x24,1,skill_rank(slot,signed_load(target+0x20,2)));
            } else rebuild_abilities(0x20000,slot,found);
        }
    }
    if(kind==1) {
        memory_.store(target+8,1,memory_.load(hud_marker+8,1)); memory_.store(target,8,memory_.load(hud_marker,8));
        for(unsigned i=0;i<8;++i) {
            auto a=r+abilities+i*ability_size+0x20;
            if(std::uint64_t(std::int64_t(signed_load(a,2)))==value) memory_.store(a,2,0);
        }
        auto skill_id=signed32(value);
        auto skill=std::uint32_t(memory_.load(skill_table+std::uint32_t(skill_id)*4,4));
        memory_.store(target+0x20,2,value); memory_.store(target+0x25,1,memory_.load(skill+8,1));
        memory_.store(target+0x24,1,skill_rank(slot,skill_id));
    } else rebuild_abilities(value,slot,index);
    if(call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x53c3b8,0x53c858);
}
void Machine::mask_local_buttons(std::int64_t slot,std::int64_t frames,std::uint32_t mask) {
    auto e=0x5d0bc8u+std::uint32_t(slot)*12;
    auto old=signed_load(e,4);
    memory_.store(e+8,4,0); memory_.store(e,4,std::max(std::int64_t(old),frames)); memory_.store(e+4,4,mask);
}
void Machine::on_menu_state(std::int64_t slot,std::int64_t state) {
    auto r=rec(slot), player=std::uint32_t(memory_.load(r+0x43d8,4));
    if(!player) return;
    if(state!=0) {
        memory_.store(player+0xc,4,memory_.load(player+0xc,4)|0x400000); memory_.store(r+0x4bc8,1,0);
        if(state>=2) {
            player=std::uint32_t(memory_.load(r+0x43d8,4));
            memory_.store(player+0xc,4,memory_.load(player+0xc,4)|0x800000);
        }
    } else {
        mask_local_buttons(slot,0x3c,0xf0); memory_.store(r+0x4bc8,1,10);
        player=std::uint32_t(memory_.load(r+0x43d8,4));
        memory_.store(player+0xc,4,memory_.load(player+0xc,4)&0xff7fffff);
    }
}
void Machine::on_quick_cast(std::int64_t slot,std::int64_t index) {
    auto r=rec(slot), target=r+abilities+std::uint32_t(index)*ability_size;
    if(!target) return;
    auto player=std::uint32_t(memory_.load(r+0x43d8,4));
    if(memory_.load(player+0x488,4)) return;
    if(memory_.load(target+0x25,1)==1) {
        auto text=call("text_lookup",{0x53c870});
        call("hud_showMessage",{text,0x5a,std::uint32_t(slot),0});
    } else if(memory_.load(player+0x58c,4)==2) memory_.store(player+0x488,4,target);
}
bool Machine::same_identity(std::uint32_t a,std::uint32_t b) const {
    return memory_.load(b,4)!=0 && memory_.load(a+8,8)==memory_.load(b+8,8)
        && memory_.load(a,4)==memory_.load(b,4) && memory_.load(a+0x20,4)==memory_.load(b+0x20,4);
}
std::uint32_t Machine::grid(std::int64_t panel,std::int64_t bag) const {
    return std::uint32_t(panel)*0x23988u+std::uint32_t(bag)*0xbdd8u+std::uint32_t(memory_.load(0x4ebbb0,4));
}
void Machine::remove_item(std::int64_t slot,std::int64_t requested,std::int64_t bag,std::int64_t panel) {
    auto r=rec(slot), base=r+0x130u+std::uint32_t(bag)*0x1208u;
    auto count_at=r+0x1330u+std::uint32_t(bag)*0x1208u;
    std::int64_t index=signed32(requested);
    auto item=base+std::uint32_t(index)*0x48u;
    auto es=signed_load(std::uint32_t(memory_.load(item,4))+0x1c,1);
    auto equipment=r+0x43f0u;
    auto clear=[&](unsigned first,unsigned second) {
        if(memory_.load(equipment+first*4,4)==item) memory_.store(equipment+first*4,4,0);
        else if(memory_.load(equipment+second*4,4)==item) memory_.store(equipment+second*4,4,0);
    };
    if(std::uint32_t(es-5)<2) clear(5,6);
    else if(std::uint32_t(es-14)<2) clear(14,15);
    else if(memory_.load(equipment+std::uint32_t(es)*4,4)==item)
        memory_.store(equipment+std::uint32_t(es)*4,4,0);
    if(memory_.load(r+0x4434u+std::uint32_t(signed_load(r+0x4a7c,1))*4,4)==item) {
        memory_.store(r+0x4a86,1,5); call("player_resetIdleAnim",{std::uint32_t(slot)});
    }
    if(memory_.load(r+0x4434,4)==item) memory_.store(r+0x4434,4,0);
    else if(memory_.load(r+0x4438,4)==item) memory_.store(r+0x4438,4,0);
    if(index<std::int64_t(signed_load(count_at,4))-1) {
        std::uint64_t dst=item; auto i=index;
        do {
            auto src=dst+0x48;
            copy(std::uint32_t(dst),std::uint32_t(src),0x48);
            for(unsigned k=0;k<17;++k) if(memory_.load(equipment+4*k,4)==src)
                memory_.store(equipment+4*k,4,dst);
            for(unsigned k=0;k<2;++k) {
                if(memory_.load(r+0x4434+4*k,4)==src) memory_.store(r+0x4434+4*k,4,dst);
                if(memory_.load(r+0x443c,4)==src) memory_.store(r+0x443c,4,dst);
            }
            ++i; memory_.store(std::uint32_t(src),4,0); dst=src;
            if(!(i<std::int64_t(signed_load(count_at,4))-1)) break;
        } while(true);
    }
    if(call("net_isLocalSlot",{std::uint32_t(slot)}) && signed_load(0x41b69d,1)!=0 && memory_.load(0x4ebbb0,4)!=0) {
        auto g=grid(panel,bag);
        for(unsigned row=0;row<75;++row) for(unsigned col=0;col<9;++col) {
            auto cell=g+col*0x1518u+row*0x48u;
            if(memory_.load(cell,4)!=0) for(unsigned k=0;k<64;++k)
                if(same_identity(cell,base+k*0x48u)) memory_.store(cell+0x22,2,k);
        }
    }
    auto count=std::int64_t(signed_load(count_at,4)), last=count-1;
    if(index<last) {
        for(auto i=index;i<last;++i) {
            auto a=base+std::uint32_t(i)*0x48u+0x22;
            memory_.store(a,2,memory_.load(a,2)-1);
        }
        index=last;
    }
    for(auto i=index;i<64;++i) memory_.store(base+std::uint32_t(i)*0x48u,4,0);
    memory_.store(count_at,4,std::uint64_t(count-1)); call("player_recalcStats",{});
}
void Machine::stop_stream() {
    if(call("audio_streamStatus",{2})&0x40) call("stream_queueStop",{2});
}
void Machine::set_pending_level(const Bytes& name,bool clear_load_flag) {
    auto end=std::find(name.begin(),name.end(),0);
    std::uint32_t i=0;
    for(auto it=name.begin();it!=end;++it,++i) memory_.store(0x41b6d0u+i,1,*it);
    memory_.store(0x41b6d0u+i,1,0); memory_.store(0x41b720,1,0xff);
    call("pauseMenu_close",{});
    memory_.store(0x41b4f4,4,0); memory_.store(0x41b4fc,4,0x3c); memory_.store(0x44fa80,1,0);
    if(clear_load_flag) memory_.store(0x44f208,1,0);
    stop_stream();
}
void Machine::recall(std::int64_t slot) {
    namespace C=champions::simulation::rta;
    call("pauseMenu_close",{}); auto r=rec(slot);
    std::uint32_t error=0; bool in_hub=false;
    if(call("game_inNoSaveZone",{})) error=0x534300;
    else if(C::le(std::uint32_t(memory_.load(r+0xac,4)),0)) error=0x534318;
    else {
        in_hub=contains(cstr(0x41b721),cstr(0x534330));
        if(in_hub && cstr(r+0x24)==cstr(0x534338)) error=0x534340;
    }
    if(error) {
        auto text=call("text_lookup",{error}); call("hud_showMessage",{text,0xb4,UINT32_MAX,0}); return;
    }
    if(!in_hub) {
        bool used=false;
        if(memory_.load(r+0x43d8,4)!=0 && C::lt(0,std::uint32_t(memory_.load(r+0xac,4)))) {
            std::int64_t i=0;
            while(i<signed_load(r+0x1330+2*0x1208,4)) {
                auto it=r+0x130u+2*0x1208u+std::uint32_t(i)*0x48u;
                auto def=std::uint32_t(memory_.load(it,4));
                if(cstr(std::uint32_t(memory_.load(def,4)))==cstr(0x534358)) {
                    auto left=std::uint16_t(memory_.load(it+4,2)-1); used=true;
                    memory_.store(it+4,2,left); if(left==0) remove_item(slot,i,2,0); break;
                }
                ++i;
            }
        }
        if(!used) {
            auto text=call("text_lookup",{0x534388});
            call("hud_showMessage",{text,0xb4,std::uint32_t(slot),0}); return;
        }
    }
    memory_.store(0x41b51d,1,0x3c);
    std::int64_t i=0;
    while(i<signed_load(0x4f8420,4)) {
        auto block=call("pool_new",{0x60});
        if(block) {
            auto player=std::uint32_t(memory_.load(rec(signed_load(0x4f8440u+std::uint32_t(i)*4,4))+0x43d8,4));
            typed_call("RecallEffect_ctor",{block,vec(player+0x10)});
        }
        ++i;
    }
    sound(0x534368,0x534370); stop_stream();
    memory_.store(0x41b4fc,4,0x3c); memory_.store(0x41b4f4,4,0); memory_.store(0x44fa80,1,0);
}
void Machine::on_player_left(std::int64_t slot) {
    auto r=rec(slot); auto name=cstr(r), space=cstr(0x53c290); name.insert(name.end(),space.begin(),space.end());
    Bytes text; for(auto c:name) { text.push_back(c); text.push_back(0); }
    auto translated=wstr(std::uint32_t(call("text_lookup",{0x53c298})));
    text.insert(text.end(),translated.begin(),translated.end());
    typed_call("hud_showMessage",{text,std::uint64_t(300),std::uint64_t(UINT32_MAX),std::uint64_t(0)});
    auto player=std::uint32_t(memory_.load(r+0x43d8,4)); if(!player) return;
    auto entry=std::uint32_t(memory_.load(player+0x48,4))+0x10u;
    memory_.store(player+0xc,4,memory_.load(player+0xc,4)&0xffbfffff);
    call("Player_takeDamage_virtual",{player+std::uint32_t(signed_load(entry,2)),0,0x20000040,0x49742400});
    call("Player_processDamage",{memory_.load(r+0x43d8,4)});
}
void Machine::on_disconnect_request(std::int64_t slot) {
    if(call("net_isLocalSlot",{std::uint32_t(slot)})) return;
    if(signed_load(0x4f8420,4)==1 && signed_load(0x4f8440,4)==slot) {
        std::int64_t i=0;
        while(i<signed_load(0x4d8ae0,4)) {
            if(call("net_isSlotActive",{std::uint64_t(i)}) && i!=slot) {
                auto player=std::uint32_t(memory_.load(rec(slot)+0x43d8,4));
                typed_call("player_respawn",{std::uint64_t(i),vec(player+0x10),
                    std::uint64_t(std::uint32_t(signed_load(player+0x38,2))),std::uint64_t(2),std::uint64_t(8)});
                break;
            }
            ++i;
        }
    }
    call("net_disconnectPlayer",{std::uint32_t(slot),1});
}
void Machine::on_checksum(std::int64_t slot,std::uint64_t live,std::uint64_t saved) {
    auto e=0x86ab58u+std::uint32_t(slot)*8;
    memory_.store(e,4,live); auto count=std::uint8_t(memory_.load(0x86aaa8,1)+1);
    memory_.store(0x86aaa8,1,count); memory_.store(e+4,4,saved);
    auto signed_count=count<128 ? std::int32_t(count) : std::int32_t(count)-256;
    if(signed_count!=signed_load(0x4d8ae0,4) || signed_count<=0) return;
    std::int64_t i=0;
    while(i<signed_load(0x4d8ae0,4)) {
        for(unsigned j=0;j<2;++j) {
            auto at=std::uint32_t(i)*8+j*4;
            if(memory_.load(0x86ab58u+at,4)!=memory_.load(0x86ab38u+at,4)) {
                memory_.store(0x4f7144,4,1);
                if(i!=signed32(call("net_getLocalPlayerSlot",{0}))) call("net_disconnectPlayer",{std::uint64_t(i),1});
            }
        }
        ++i;
    }
}
void Machine::hud_on_msg(const Bytes& payload) {
    auto p=payload; if(p.size()<16) p.resize(16,0);
    auto b=[&](unsigned i)->std::int32_t { return p[i]<128 ? p[i] : std::int32_t(p[i])-256; };
    auto w=[&](unsigned i)->std::uint32_t {
        return std::uint32_t(p[i])|(std::uint32_t(p[i+1])<<8)|(std::uint32_t(p[i+2])<<16)|(std::uint32_t(p[i+3])<<24);
    };
    switch(b(0)) {
    case 0: on_quick_slot(b(1),b(2),b(3)); break;
    case 1: on_assign_spell(b(1),b(2),signed32(w(4)),std::uint64_t(w(8))|(std::uint64_t(w(12))<<32)); break;
    case 2: on_menu_state(b(1),b(2)); break;
    case 3:
        if(call("dialog_isOpen",{})==0 && call("task_isRunning",{0x53c280})==0) {
            recall(b(1)); if(signed_load(0x41b51d,1)!=0) call("net_closeLocalMenus",{0});
        }
        break;
    case 4: on_player_left(b(1)); break;
    case 5: call("Savepoint_startSavedEffect",{w(4)}); break;
    case 6: on_disconnect_request(b(1)); break;
    case 7: set_pending_level(Bytes(p.begin()+1,p.end()),false); break;
    case 8: memory_.store(0x4f694a,2,signed_load(network_mode_address,1)!=0 ? 0xf0 : 1); break;
    case 9: memory_.store(0x86aaa8,1,0); memory_.store(0x4f7138,4,1); break;
    case 10: on_checksum(b(1),w(4),w(8)); break;
    case 11: set_pending_level(Bytes(p.begin()+4,p.end()),true); break;
    case 12: memory_.store(0x4f7140,4,1); break;
    case 13: on_quick_cast(b(1),b(2)); break;
    default: break;
    }
}

Message encode(Kind kind, const std::vector<std::uint64_t>& a, const Bytes& name) {
    static constexpr std::array<unsigned,27> counts{{7,3,2,3,2,2,3,4,2,5,3,4,3,1,1,1,1,0,0,0,3,0,0,2,2,7,1}};
    auto index=static_cast<unsigned>(kind);
    if (index>=counts.size() || a.size()!=counts[index] || (kind!=Kind::new_world && !name.empty()))
        throw std::invalid_argument("message encoder argument mismatch");
    Message out{index<10 ? "inventory" : index<24 ? "game" : index<26 ? "shop" : "null",{}};
    auto put=[&](std::uint64_t value,unsigned width=1) {
        for (unsigned i=0;i<width;++i) out.payload.push_back(std::uint8_t(value>>(8*i)));
    };
    switch (kind) {
    case Kind::drop: case Kind::sell:
        put(kind==Kind::drop ? 0 : 1); put(a[0]); put(a[3],2); put(a[1],4);
        put(a[4],2); put(a[2]); put(a[5]); put(a[6]); break;
    case Kind::equip: put(1); put(a[2]); put(a[0],2); put(a[1],4); break;
    case Kind::unequip: put(2); put(0); put(a[0],2); put(a[1],4); break;
    case Kind::join_panel: put(4); put(a[0]); put(a[1]); put(a[2]); break;
    case Kind::weapon_class: put(5); put(a[0]); put(a[1]); break;
    case Kind::stat_point: put(6); put(a[0]); put(a[1],2); break;
    case Kind::skill_point: put(7); put(a[0]); put(a[1]); put(0); put(a[2],4); break;
    case Kind::combine: put(8); put(a[0]); put(0,2); put(a[1],4); put(a[2],4); put(a[3],4); break;
    case Kind::level_ack: put(9); put(a[0]); put(a[1]); break;
    case Kind::use_potion: put(10); put(a[0]); for (unsigned i=1;i<5;++i) put(a[i],2); break;
    case Kind::quick_slot: put(0); put(a[0]); put(a[1]); put(a[2]); break;
    case Kind::assign_spell: put(1); put(a[0]); put(a[1]); put(0); put(a[2],4); put(a[3],8); break;
    case Kind::menu_state: put(2); put(a[0]); put(a[1]); put(a[2]); break;
    case Kind::recall: put(3); put(a[0]); break;
    case Kind::player_left: put(4); put(a[0]); break;
    case Kind::savepoint: put(0x05050505,4); put(a[0],4); break;
    case Kind::disconnect_request: put(6); put(a[0]); break;
    case Kind::new_world: put(7); out.payload.insert(out.payload.end(),name.begin(),name.end()); put(0); break;
    case Kind::exit_countdown: put(8); break;
    case Kind::checksum_start: put(9); break;
    case Kind::checksum: put(10); put(a[0]); put(0,2); put(a[1],4); put(a[2],4); break;
    case Kind::load_level: put(11); put(0,3); break;
    case Kind::medal_ack: put(12); break;
    case Kind::quick_cast: put(13); put(a[0]); put(a[1]); break;
    case Kind::buy: put(0); put(a[0]); put(0,2); put(a[1],4); break;
    case Kind::shop_closed: put(1); put(a[0]); break;
    }
    return out;
}
} // namespace champions::messages::rta
