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
}
Integer::Integer(std::uint64_t v) { while(v) { magnitude_.push_back(std::uint8_t(v)); v >>= 8; } }
void Integer::normalize() { while(!magnitude_.empty() && magnitude_.back()==0) magnitude_.pop_back(); if(magnitude_.empty()) negative_=false; }
Integer Integer::from_hex(const std::string& text) {
    Integer out; std::size_t start=(!text.empty() && text[0]=='-')?1:0;
    if(start==text.size()) throw std::invalid_argument("empty integer");
    for(std::size_t end=text.size();end>start;) {
        auto begin=end-start>1?end-2:start; unsigned v=0;
        for(auto i=begin;i<end;++i) { char c=text[i]; unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:16;
            if(d==16) throw std::invalid_argument("invalid integer"); v=v*16+d; }
        out.magnitude_.push_back(std::uint8_t(v)); end=begin;
    }
    out.negative_=start!=0; out.normalize(); return out;
}
std::string Integer::hex() const {
    if(magnitude_.empty()) return "0";
    const char* d="0123456789abcdef"; std::string s=negative_?"-":"";
    auto top=magnitude_.back(); if(top>>4) s+=d[top>>4]; s+=d[top&15];
    for(std::size_t i=magnitude_.size()-1;i>0;--i) { auto b=magnitude_[i-1]; s+=d[b>>4]; s+=d[b&15]; } return s;
}
std::uint8_t Integer::byte(std::size_t i) const {
    auto b=i<magnitude_.size()?magnitude_[i]:0;
    if(!negative_) return std::uint8_t(b);
    bool carry=true; for(std::size_t j=0;j<i && j<magnitude_.size();++j) if(magnitude_[j]) { carry=false; break; }
    return std::uint8_t((b^255)+(carry?1:0));
}
std::uint64_t Integer::low64() const { std::uint64_t v=0; for(unsigned i=0;i<8;++i) v|=std::uint64_t(byte(i))<<(8*i); return v; }
Integer Integer::from_twos(Bytes b,bool negative) {
    Integer out; out.negative_=negative;
    if(negative) { unsigned carry=1; for(auto& v:b) { auto x=(v^255)+carry; v=std::uint8_t(x); carry=x>>8; } }
    out.magnitude_=std::move(b); out.normalize(); return out;
}
Integer Integer::next() const {
    Integer out=*this;
    if(negative_) { for(auto& b:out.magnitude_) { if(b) { --b; break; } b=255; } }
    else { unsigned carry=1; for(auto& b:out.magnitude_) { auto v=b+carry; b=std::uint8_t(v); carry=v>>8; if(!carry) break; } if(carry) out.magnitude_.push_back(1); }
    out.normalize(); return out;
}
Integer Integer::truncated(std::size_t size) const { Bytes b; for(std::size_t i=0;i<size;++i) b.push_back(byte(i)); return from_twos(std::move(b),false); }
Integer Integer::or_shift(const Integer& other,std::size_t shift) const {
    auto size=std::max(magnitude_.size(),other.magnitude_.size()+shift)+1; Bytes b(size);
    for(std::size_t i=0;i<size;++i) b[i]=byte(i)|(i>=shift?other.byte(i-shift):0);
    return from_twos(std::move(b),negative_||other.negative_);
}
bool Integer::operator==(const Integer& b) const { return negative_==b.negative_ && magnitude_==b.magnitude_; }
bool Integer::operator<(const Integer& b) const {
    if(negative_!=b.negative_) return negative_;
    bool less=magnitude_.size()<b.magnitude_.size();
    if(magnitude_.size()==b.magnitude_.size()) less=std::lexicographical_compare(magnitude_.rbegin(),magnitude_.rend(),b.magnitude_.rbegin(),b.magnitude_.rend());
    return negative_?(!less && !(*this==b)):less;
}
Integer Memory::load(const Integer& address,std::int64_t size) const {
    // Snapshot all overlay bytes before any backing callback, as Python does.
    std::vector<std::pair<Integer,int>> parts; auto a=address; bool absent=true;
    for(std::int64_t i=0;i<size;++i,a=a.next()) { auto it=bytes.find(a); int b=it==bytes.end()?-1:it->second; parts.emplace_back(a,b); if(b>=0) absent=false; }
    // Keep the selected callable alive if it replaces under during invocation.
    auto backing=[&](const Integer& a,std::int64_t n) {
        auto f=under; return f?f(a,n):Integer();
    };
    if(absent && size>=0) return backing(address,size);
    Integer value; std::size_t i=0;
    for(const auto& part:parts) { auto b=part.second<0?backing(part.first,1):Integer(part.second); value=value.or_shift(b,i++); }
    return value;
}
void Memory::poke(const Integer& address,std::int64_t size,const Integer& value) {
    auto a=address; for(std::int64_t i=0;i<size;++i,a=a.next()) bytes[a]=value.byte(std::size_t(i));
}
void Memory::store(const Integer& address,std::int64_t size,const Integer& value) {
    if(size<0) throw std::invalid_argument("negative shift count");
    auto v=value.truncated(std::size_t(size)); writes.push_back({address,size,v}); poke(address,size,v);
}
std::uint64_t Memory::load(std::uint32_t a,unsigned s) const {
    if(s!=1 && s!=2 && s!=4 && s!=8) throw std::invalid_argument("scalar width");
    if(under) return load(Integer(a),s).low64();
    std::uint64_t v=0; for(unsigned i=0;i<s;++i) { auto it=bytes.find(Integer(std::uint64_t(a)+i)); if(it!=bytes.end()) v|=std::uint64_t(it->second)<<(8*i); } return v;
}
void Memory::poke(std::uint32_t a,unsigned s,std::uint64_t v) {
    if(s!=1 && s!=2 && s!=4 && s!=8) throw std::invalid_argument("scalar width");
    poke(Integer(a),s,Integer(v));
}
void Memory::store(std::uint32_t a,unsigned s,std::uint64_t v) {
    if(s!=1 && s!=2 && s!=4 && s!=8) throw std::invalid_argument("scalar width");
    store(Integer(a),s,Integer(v));
}
Machine::Machine(Memory& memory, Callbacks callbacks, const Items::Tables* tables)
    : tables_(tables), memory_(memory), callbacks_(std::move(callbacks)) {}
std::int64_t s64(std::uint64_t value) {
    return value <= INT64_MAX ? std::int64_t(value) : -1 - std::int64_t(UINT64_MAX - value);
}
std::uint64_t Machine::u8(std::uint64_t address) const { return memory_.load(std::uint32_t(address),1); }
std::uint64_t Machine::u16(std::uint64_t address) const { return memory_.load(std::uint32_t(address),2); }
std::uint64_t Machine::u32(std::uint64_t address) const { return memory_.load(std::uint32_t(address),4); }
std::uint64_t Machine::u64(std::uint64_t address) const { return memory_.load(std::uint32_t(address),8); }
std::int32_t Machine::s8(std::uint64_t address) const { return signed_load(std::uint32_t(address),1); }
std::int32_t Machine::s16(std::uint64_t address) const { return signed_load(std::uint32_t(address),2); }
std::int32_t Machine::s32(std::uint64_t address) const { return signed_load(std::uint32_t(address),4); }
void Machine::w8(std::uint64_t address,std::uint64_t value) { memory_.store(std::uint32_t(address),1,value); }
void Machine::w16(std::uint64_t address,std::uint64_t value) { memory_.store(std::uint32_t(address),2,value); }
void Machine::w32(std::uint64_t address,std::uint64_t value) { memory_.store(std::uint32_t(address),4,value); }
void Machine::w64(std::uint64_t address,std::uint64_t value) { memory_.store(std::uint32_t(address),8,value); }
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
    if (handler==0x1bfdb0) { inv_dispatch(payload); return; }
    if (handler==0x1c5d28) { shop_on_msg(payload); return; }
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
std::any Machine::call(const std::string& name,std::vector<std::any> arguments) {
    Call c{name,{},0,{},{},true}; c.general_arguments=arguments; calls.push_back(std::move(c));
    auto f=callbacks_.lookup?callbacks_.lookup(name):Callbacks::General{};
    return f?f(arguments):std::any(std::uint64_t(0));
}
std::uint64_t Machine::scalar_call(const std::string& name,std::vector<std::uint64_t> arguments) {
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
    auto resource=scalar_call("res_find",{lump,entry});
    scalar_call("sound_playOnObject",{resource,0,0x3f800000});
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
    if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x53c3b8,0x53c858);
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
    if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x53c3b8,0x53c858);
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
        auto text=scalar_call("text_lookup",{0x53c870});
        scalar_call("hud_showMessage",{text,0x5a,std::uint32_t(slot),0});
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
        memory_.store(r+0x4a86,1,5); scalar_call("player_resetIdleAnim",{std::uint32_t(slot)});
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
    if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)}) && signed_load(0x41b69d,1)!=0 && memory_.load(0x4ebbb0,4)!=0) {
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
    memory_.store(count_at,4,std::uint64_t(count-1)); scalar_call("player_recalcStats",{});
}
void Machine::stop_stream() {
    if(scalar_call("audio_streamStatus",{2})&0x40) scalar_call("stream_queueStop",{2});
}
void Machine::set_pending_level(const Bytes& name,bool clear_load_flag) {
    auto end=std::find(name.begin(),name.end(),0);
    std::uint32_t i=0;
    for(auto it=name.begin();it!=end;++it,++i) memory_.store(0x41b6d0u+i,1,*it);
    memory_.store(0x41b6d0u+i,1,0); memory_.store(0x41b720,1,0xff);
    scalar_call("pauseMenu_close",{});
    memory_.store(0x41b4f4,4,0); memory_.store(0x41b4fc,4,0x3c); memory_.store(0x44fa80,1,0);
    if(clear_load_flag) memory_.store(0x44f208,1,0);
    stop_stream();
}
void Machine::recall(std::int64_t slot) {
    namespace C=champions::simulation::rta;
    scalar_call("pauseMenu_close",{}); auto r=rec(slot);
    std::uint32_t error=0; bool in_hub=false;
    if(scalar_call("game_inNoSaveZone",{})) error=0x534300;
    else if(C::le(std::uint32_t(memory_.load(r+0xac,4)),0)) error=0x534318;
    else {
        in_hub=contains(cstr(0x41b721),cstr(0x534330));
        if(in_hub && cstr(r+0x24)==cstr(0x534338)) error=0x534340;
    }
    if(error) {
        auto text=scalar_call("text_lookup",{error}); scalar_call("hud_showMessage",{text,0xb4,UINT32_MAX,0}); return;
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
            auto text=scalar_call("text_lookup",{0x534388});
            scalar_call("hud_showMessage",{text,0xb4,std::uint32_t(slot),0}); return;
        }
    }
    memory_.store(0x41b51d,1,0x3c);
    std::int64_t i=0;
    while(i<signed_load(0x4f8420,4)) {
        auto block=scalar_call("pool_new",{0x60});
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
    auto translated=wstr(std::uint32_t(scalar_call("text_lookup",{0x53c298})));
    text.insert(text.end(),translated.begin(),translated.end());
    typed_call("hud_showMessage",{text,std::uint64_t(300),std::uint64_t(UINT32_MAX),std::uint64_t(0)});
    auto player=std::uint32_t(memory_.load(r+0x43d8,4)); if(!player) return;
    auto entry=std::uint32_t(memory_.load(player+0x48,4))+0x10u;
    memory_.store(player+0xc,4,memory_.load(player+0xc,4)&0xffbfffff);
    scalar_call("Player_takeDamage_virtual",{player+std::uint32_t(signed_load(entry,2)),0,0x20000040,0x49742400});
    scalar_call("Player_processDamage",{memory_.load(r+0x43d8,4)});
}
void Machine::on_disconnect_request(std::int64_t slot) {
    if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) return;
    if(signed_load(0x4f8420,4)==1 && signed_load(0x4f8440,4)==slot) {
        std::int64_t i=0;
        while(i<signed_load(0x4d8ae0,4)) {
            if(scalar_call("net_isSlotActive",{std::uint64_t(i)}) && i!=slot) {
                auto player=std::uint32_t(memory_.load(rec(slot)+0x43d8,4));
                typed_call("player_respawn",{std::uint64_t(i),vec(player+0x10),
                    std::uint64_t(std::uint32_t(signed_load(player+0x38,2))),std::uint64_t(2),std::uint64_t(8)});
                break;
            }
            ++i;
        }
    }
    scalar_call("net_disconnectPlayer",{std::uint32_t(slot),1});
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
                if(i!=signed32(scalar_call("net_getLocalPlayerSlot",{0}))) scalar_call("net_disconnectPlayer",{std::uint64_t(i),1});
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
        if(scalar_call("dialog_isOpen",{})==0 && scalar_call("task_isRunning",{0x53c280})==0) {
            recall(b(1)); if(signed_load(0x41b51d,1)!=0) scalar_call("net_closeLocalMenus",{0});
        }
        break;
    case 4: on_player_left(b(1)); break;
    case 5: scalar_call("Savepoint_startSavedEffect",{w(4)}); break;
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

namespace {
constexpr SoundTable equip_sounds{{0x538060,0x538078,0x538090,0x5380a8,0x5380c0,0x5380d8,0x5380f0,0x538108,0x538120,0x538138,0x538150,0x538168,0x538180}};
constexpr SoundTable drop_sounds{{0x537e18,0x537e30,0x537e48,0x538198,0x5381b0,0x5381c8,0x537e78,0x537e90,0x537ea8,0x537ec0,0x537ed8,0x537ef0,0x537f08}};
constexpr std::uint32_t bags=0x130, bag_size=0x1208, counts=0x1330, item_size=0x48;
constexpr std::uint32_t equipment=0x43f0, weapons=0x4434, second_weapon=0x443c;
constexpr std::uint32_t active_set=0x4a7c, weapon_class=0x4a86, object=0x43d8;
std::uint32_t panel_anim(std::int64_t slot) { return 0x866850+std::uint32_t(slot)*0x108u; }
}
std::uint32_t sound_name(const SoundTable& t,std::uint64_t flags,std::int64_t slot,std::uint64_t subtype,bool short_form) {
    auto sub=std::uint32_t(std::uint8_t(subtype))-1;
    if(flags&0x800000) return t[short_form?2:sub<4?0:flags&0x20000000?1:2];
    if(flags&0x200000) return t[3];
    if(slot==3) return t[4];
    if(flags&0x400000) return t[5];
    if(flags&0x40000) return t[short_form?8:sub<3?6:flags&0x20000000?7:8];
    if(flags&0x100000) return t[9];
    if(flags&0x80000) return t[10];
    if(flags&0x1000000) return t[11];
    return t[12];
}
Items::Item Machine::item_object(std::uint32_t item) const {
    if(!tables_) throw Unsupported("item tables must be supplied");
    Bytes data;
    for(unsigned i=0;i<item_size;++i) data.push_back(std::uint8_t(memory_.load(item+i,1)));
    return Items::Item::unpack(*tables_,data);
}
std::int32_t Machine::check_equip_allowed(std::int64_t slot,std::uint32_t item) const {
    auto r=rec(slot); Items::Player p;
    for(auto a=r+0x4b08;signed_load(a,2)!=-1;a+=4)
        p.skills.emplace(signed_load(a,2),signed_load(a+2,2));
    p.class_id=signed_load(r+0x50,4); p.level=signed_load(r+0x58,1);
    p.strength=signed_load(r+0x90,4); p.intelligence=signed_load(r+0x94,4);
    p.dexterity=signed_load(r+0x98,4); p.stamina=signed_load(r+0x9c,4);
    auto value=item_object(item);
    return Items::equip_restriction(*tables_,p,value);
}
std::int32_t Machine::item_value(std::uint32_t item) const {
    auto value=item_object(item); return Items::item_value(*tables_,value);
}
void Machine::send_character_to_lobby(std::int64_t slot) {
    if(!scalar_call("net_allPlayersState2",{})) return;
    if(memory_.load(0x4f7120,4)||memory_.load(0x4f713c,4)) return;
    std::vector<std::uint32_t> active;
    for(std::int64_t i=0;i<signed_load(0x4d8ae0,4);++i)
        if(scalar_call("net_isSlotActive",{std::uint64_t(i)})) active.push_back(std::uint32_t(i));
    if(active.empty()) return;
    if(slot==-1) {
        auto local=scalar_call("net_getLocalPlayerSlot",{0});
        auto pick=std::uint32_t(memory_.load(0x404464,4)+local)%active.size();
        scalar_call("lobby_sendCharacter",{active[pick],0}); return;
    }
    auto frame=signed_load(0x44fdcc,4);
    if(frame<0) throw std::invalid_argument("negative frame counter");
    auto chosen=active[std::size_t(frame)%active.size()];
    if(signed32(chosen)==signed32(scalar_call("net_getLocalPlayerSlot",{0})))
        scalar_call("lobby_sendCharacter",{std::uint32_t(slot),0});
}
void Machine::first_recall_hint(std::int64_t slot,std::uint32_t r) {
    memory_.store(r+0x4c25,1,1); scalar_call("vars_set",{0x537dc8,1});
    auto title=scalar_call("text_lookup",{0x537de0}), body=scalar_call("text_lookup",{0x537df0});
    scalar_call("hud_showHelpMessage",{std::uint32_t(slot),title,body});
}
std::int32_t Machine::give_item(std::int64_t slot,std::uint32_t src,std::int64_t announce) {
    auto r=rec(slot), definition=std::uint32_t(memory_.load(src,4));
    auto bag=signed_load(definition+0x1d,1);
    auto base=r+bags+std::uint32_t(bag)*bag_size, count_at=r+counts+std::uint32_t(bag)*bag_size;
    auto net=signed_load(network_mode_address,1); bool had=false;
    if(!net) {
        // range() snapshots the player count before this loop.
        auto count=signed_load(0x4d8ae0,4);
        for(std::int64_t i=0;i<count;++i) if(signed_load(rec(i)+0x4c25,1)==1) had=true;
    } else had=signed_load(r+0x4c25,1)==1;
    auto flags=memory_.load(src+8,8); auto name=cstr(std::uint32_t(memory_.load(definition,4)));
    if(flags&0x100) for(unsigned i=0;i<64;++i) {
        auto it=base+i*item_size;
        if(memory_.load(it,4)==definition && memory_.load(it+8,8)==flags &&
            memory_.load(it+0x20,1)==memory_.load(src+0x20,1) && memory_.load(it+0x21,1)==memory_.load(src+0x21,1)) {
            if(signed_load(0x4f719c,4)==1 && !had && signed32(announce)!=0 && name==cstr(0x537db8) &&
                scalar_call("vars_get",{0x537dc8})==0 && net==0) first_recall_hint(slot,r);
            memory_.store(it+4,2,memory_.load(it+4,2)+memory_.load(src+4,2));
            scalar_call("player_recalcLoad",{std::uint32_t(slot)});
            scalar_call("player_countPotions",{std::uint32_t(slot),0}); scalar_call("player_countPotions",{std::uint32_t(slot),1});
            scalar_call("player_recalcStats",{}); return std::int32_t(i);
        }
    }
    if(name==cstr(0x537e08)||name==cstr(0x537e10)) {
        for(std::int64_t i=0;i<signed_load(count_at,4);++i) {
            auto other=std::uint32_t(memory_.load(base+std::uint32_t(i)*item_size,4));
            if(other && cstr(std::uint32_t(memory_.load(other,4)))==name) return -1;
        }
    }
    auto count=signed_load(count_at,4); auto dst=base+std::uint32_t(count)*item_size;
    if(memory_.load(dst,4)) return -1;
    copy(dst,src,item_size);
    if(signed32(announce)!=0 && name==cstr(0x537db8) && scalar_call("vars_get",{0x537dc8})==0 && net==0 && !had)
        first_recall_hint(slot,r);
    scalar_call("player_countPotions",{std::uint32_t(slot),0}); scalar_call("player_countPotions",{std::uint32_t(slot),1});
    count=signed_load(count_at,4); memory_.store(base+std::uint32_t(count)*item_size+0x22,2,count);
    scalar_call("player_recalcStats",{}); count=signed_load(count_at,4); memory_.store(count_at,4,std::int64_t(count)+1);
    return count;
}
void Machine::unref_item(std::int64_t slot,std::uint64_t item) {
    auto r=rec(slot);
    if(item==memory_.load(r+weapons,4)) {
        auto second=memory_.load(r+second_weapon,4);
        memory_.store(r+weapons,4,0); memory_.store(r+equipment,4,0);
        send(std::uint32_t(memory_.load(0x866d18,4)),encode(Kind::equip,{std::uint64_t(slot),second,0}).payload);
        memory_.store(r+equipment+4,4,0); memory_.store(r+second_weapon,4,0);
    } else if(item==memory_.load(r+second_weapon,4)) {
        memory_.store(r+weapon_class,1,0); memory_.store(r+second_weapon,4,0); memory_.store(r+equipment+4,4,0);
        scalar_call("player_resetIdleAnim",{std::uint32_t(slot)});
        scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),panel_anim(slot),0});
    }
}
void Machine::drop_item(std::int64_t slot,std::int64_t index,std::int64_t bag,std::int64_t column,
                       std::int64_t row,std::int64_t panel,std::int64_t mode) {
    auto r=rec(slot), item=r+bags+std::uint32_t(index)*item_size+std::uint32_t(bag)*bag_size;
    if(!memory_.load(item,4)) return;
    auto player=std::uint32_t(memory_.load(r+object,4)); memory_.store(player+0x47a,1,0);
    auto definition=std::uint32_t(memory_.load(item,4));
    if(mode<2) {
        auto entry=sound_name(drop_sounds,memory_.load(item+8,8),signed_load(definition+0x1c,1),memory_.load(definition+0x1e,1));
        if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x537d08,entry);
    }
    if(signed_load(r+weapon_class,1)==7) unref_item(slot,item);
    if(memory_.load(r+weapons+4*std::uint32_t(signed_load(r+active_set,1)),4)==item) {
        memory_.store(r+weapon_class,1,5); scalar_call("player_resetIdleAnim",{std::uint32_t(slot)});
    }
    definition=std::uint32_t(memory_.load(item,4)); auto count=signed_load(item+4,2);
    auto taken=(signed_load(definition+0x1c,1)==8 || contains(cstr(std::uint32_t(memory_.load(definition,4))),cstr(0x5385e0)) ||
        (memory_.load(definition+0x28,8)&0x1c0000000000ull)) ? std::min(count,20) : 1;
    memory_.store(item+4,2,count-taken);
    if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)}) && memory_.load(0x4ebbb0,4)) {
        auto g=grid(panel,signed_load(std::uint32_t(memory_.load(item,4))+0x1d,1));
        for(unsigned row_i=0;row_i<75;++row_i) for(unsigned c=0;c<9;++c) {
            auto cell=g+c*0x1518+row_i*item_size;
            if(same_identity(cell,item)) memory_.store(cell+4,2,memory_.load(cell+4,2)-taken);
        }
    }
    if(mode==0) {
        std::vector<Argument> args{vec(player+0x10),std::uint64_t(0x30),std::uint64_t(0xc),std::uint64_t(0),std::uint64_t(0)};
        std::variant<std::uint64_t,Position> spot;
        if(callbacks_.result_service) {
            calls.push_back({"world_findFreeSpot",{},0,{},args,true});
            spot=callbacks_.result_service("world_findFreeSpot",args);
        } else spot=typed_call("world_findFreeSpot",args);
        auto block=scalar_call("pool_new",{400});
        if(block) {
            if(!std::holds_alternative<Position>(spot)) throw Unsupported("world_findFreeSpot must return a position");
            typed_call("ItemProp_ctor2",{block,std::get<Position>(spot),std::uint64_t(item),std::uint64_t(0)});
            memory_.store(std::uint32_t(block)+0x14c,2,taken);
        }
    }
    if(signed_load(item+4,2)<1) {
        auto category=signed_load(std::uint32_t(memory_.load(item,4))+0x1d,1);
        if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)}) && memory_.load(0x4ebbb0,4)) {
            auto g=grid(panel,category);
            for(unsigned row_i=0;row_i<75;++row_i) for(unsigned c=0;c<9;++c) {
                if(std::int64_t(c)==signed32(column)&&std::int64_t(row_i)==signed32(row)) continue;
                auto cell=g+c*0x1518+row_i*item_size;
                if(same_identity(cell,item)) memory_.store(cell,4,0);
            }
            memory_.store(g+std::uint32_t(row)*item_size+std::uint32_t(column)*0x1518,4,0);
        }
        remove_item(slot,index,category,panel);
        scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),panel_anim(slot),0});
    }
    scalar_call("player_recalcLoad",{std::uint32_t(slot)}); scalar_call("player_recalcStats",{});
}
void Machine::equip_item(std::int64_t slot,std::uint64_t item,std::int64_t quiet) {
    ++depth;
    try { _equip_item(slot,std::uint32_t(item),quiet); }
    catch(...) { --depth; throw; }
    --depth;
}
void Machine::_equip_item(std::int64_t slot,std::uint64_t item,std::int64_t quiet) {
    auto r=rec(slot); if(!memory_.load(item,4)) return;
    auto reason=check_equip_allowed(slot,item);
    if(reason) {
        if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x537d08,0x537f38);
        unsigned panel=0;
        if(signed_load(0x4f4078,4)!=signed32(slot)) panel=signed_load(0x4f40ec,4)==signed32(slot);
        auto title=scalar_call("text_lookup",{0x538018});
        auto body=scalar_call("text_lookup",{memory_.load(0x537ff0+4*std::uint32_t(reason),4)});
        scalar_call("hud_showPlayerNotice",{std::uint32_t(slot),title,body,panel}); return;
    }
    auto definition=std::uint32_t(memory_.load(item,4)); auto es=signed_load(definition+0x1c,1);
    if(es==16) return;
    int new_set=-1;
    if(signed_load(definition+0x1d,1)==0&&es!=8) {
        auto subtype=signed_load(definition+0x1e,1);
        if(subtype==4||subtype==6) { new_set=1; scalar_call("script_runByName",{0x538028}); scalar_call("vars_set",{0x538038,1}); }
        else { new_set=0; scalar_call("script_runByName",{0x538048}); }
        es=signed_load(std::uint32_t(memory_.load(item,4))+0x1c,1);
    }
    if(es==3 && signed_load(r+active_set,1)==1 && memory_.load(r+weapons+4,4) && signed_load(r+weapon_class,1)!=6) {
        new_set=0;
        if(memory_.load(r+weapons,4)) equip_item(slot,memory_.load(r+weapons,4),0);
    }
    bool second=false;
    if(new_set!=-1) {
        memory_.store(r+active_set,1,new_set); definition=std::uint32_t(memory_.load(item,4));
        if(signed_load(definition+0x1d,1)==0) {
            for(unsigned k=0;k<2;++k) if(memory_.load(r+weapons+4*k,4)==item) memory_.store(r+weapons+4*k,4,0);
            if(signed_load(definition+0x1c,1)==0 && passive_level(std::uint64_t(slot),0x40000)>0) {
                auto first=std::uint32_t(memory_.load(r+weapons,4));
                if(first && signed_load(std::uint32_t(memory_.load(first,4))+0x1c,1)==0) {
                    if(memory_.load(r+equipment,4)!=first) equip_item(slot,first,0);
                    memory_.store(r+second_weapon,4,item); second=true;
                }
            }
            if(!second) memory_.store(r+weapons+4*std::uint32_t(new_set),4,item);
        }
    }
    definition=std::uint32_t(memory_.load(item,4)); es=signed_load(definition+0x1c,1);
    auto where=std::uint32_t(es)*4;
    if(memory_.load(r+equipment+where,4)!=item) {
        auto category=signed_load(definition+0x1d,1);
        if(category==0) {
            if(es!=8) { memory_.store(r+weapon_class,1,memory_.load(definition+0x1e,1)); definition=std::uint32_t(memory_.load(item,4)); }
            auto kind=signed_load(definition+0x1c,1);
            if(kind==2) {
                memory_.store(r+equipment+8,4,0); memory_.store(r+equipment,4,0); memory_.store(r+equipment+4,4,0);
                auto subtype=signed_load(definition+0x1e,1);
                if(subtype!=4&&subtype!=6) { memory_.store(r+equipment+12,4,0); memory_.store(r+second_weapon,4,0); }
            } else if(kind==0) {
                auto two=std::uint32_t(memory_.load(r+equipment+8,4));
                if(two && signed_load(std::uint32_t(memory_.load(two,4))+0x1d,1)==0) {
                    memory_.store(r+equipment+8,4,0); memory_.store(r+equipment,4,0);
                }
                if(passive_level(std::uint64_t(slot),0x40000)>0&&memory_.load(r+equipment,4)) {
                    memory_.store(r+equipment+12,4,0); memory_.store(r+weapon_class,1,7); where=4;
                }
                definition=std::uint32_t(memory_.load(item,4));
            }
            if(signed_load(std::uint32_t(memory_.load(item,4))+0x1c,1)!=8) {
                scalar_call("player_resetIdleAnim",{std::uint32_t(slot)}); scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),panel_anim(slot),0});
            }
        } else if(category==1) {
            if(es==3) {
                auto two=std::uint32_t(memory_.load(r+equipment+8,4)); bool changed=false;
                if(two && signed_load(std::uint32_t(memory_.load(two,4))+0x1d,1)==0 && signed_load(r+weapon_class,1)!=6) {
                    memory_.store(r+equipment+8,4,0); memory_.store(r+weapons+4*std::uint32_t(signed_load(r+active_set,1)),4,0);
                    memory_.store(r+weapon_class,1,5); changed=true;
                } else if(memory_.load(r+second_weapon,4)) {
                    memory_.store(r+second_weapon,4,0); memory_.store(r+equipment+4,4,0); memory_.store(r+weapon_class,1,0); changed=true;
                }
                if(changed) { scalar_call("player_resetIdleAnim",{std::uint32_t(slot)}); scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),panel_anim(slot),0}); }
            }
        } else if(category==2) {
            if(es==5&&memory_.load(r+equipment+20,4)&&!memory_.load(r+equipment+24,4)) es=6;
            where=std::uint32_t(es)*4;
            if(es==14&&memory_.load(r+0x4c26,1)) {
                where=0x38;
                if(memory_.load(r+equipment+56,4)) { if(!memory_.load(r+equipment+60,4)) es=15; where=std::uint32_t(es)*4; }
            }
        }
        memory_.store(r+equipment+where,4,item);
    }
    if(signed32(quiet)==0) {
        definition=std::uint32_t(memory_.load(item,4));
        auto entry=sound_name(equip_sounds,memory_.load(item+8,8),signed_load(definition+0x1c,1),memory_.load(definition+0x1e,1));
        if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x537d08,entry);
    }
    scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),panel_anim(slot),0}); scalar_call("player_recalcStats",{});
}
void Machine::unequip_item(std::int64_t slot,std::uint64_t raw_item) {
    auto r=rec(slot), item=std::uint32_t(raw_item), definition=std::uint32_t(memory_.load(item,4));
    auto category=signed_load(definition+0x1d,1), es=signed_load(definition+0x1c,1); auto anim=panel_anim(slot);
    if(category==0) {
        if(memory_.load(r+second_weapon,4)&&es==0) unref_item(slot,item);
        else {
            bool found=false; std::uint64_t swapped=0;
            if(es==8) found=true;
            else if(memory_.load(r+weapons+4*std::uint32_t(signed_load(r+active_set,1)),4)==item) {
                found=true; swapped=scalar_call("Player_weaponSwapInput",{memory_.load(r+object,4),0x41bfd8+std::uint32_t(slot)*0x58u,1});
                if(!swapped) memory_.store(r+weapon_class,1,5);
                scalar_call("player_resetIdleAnim",{std::uint32_t(slot)}); scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),anim,0});
            }
            if(found&&!swapped) memory_.store(r+equipment+std::uint32_t(es)*4,4,0);
            for(unsigned k=0;k<2;++k) if(memory_.load(r+weapons+4*k,4)==item) memory_.store(r+weapons+4*k,4,0);
        }
    } else if(category==1) memory_.store(r+equipment+std::uint32_t(es)*4,4,0);
    else if(category==2) {
        bool found=false;
        for(auto k:{6u,5u,14u,15u}) if(memory_.load(r+equipment+4*k,4)==item) { memory_.store(r+equipment+4*k,4,0); found=true; break; }
        if(!found&&es==7) memory_.store(r+equipment+28,4,0);
    }
    auto entry=sound_name(drop_sounds,memory_.load(item+8,8),signed_load(std::uint32_t(memory_.load(item,4))+0x1c,1),0,true);
    if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x537d08,entry);
    scalar_call("inventory_selectIdleAnim",{std::uint32_t(slot),anim,0}); scalar_call("player_recalcStats",{});
}
void Machine::on_join_panel(std::int64_t panel,std::int64_t slot,std::int64_t level_up) {
    auto p=0x4f4040+std::uint32_t(panel)*0x74u;
    for(auto off:{0x14u,0x24u,0x28u,0x18u,0x1cu,0x3cu,0x40u}) memory_.store(p+off,4,0);
    auto rows=memory_.load(p+0x70,4); memory_.store(p+0x18,4,rows); memory_.store(p+0x4c,4,0);
    scalar_call("inventory_refreshPanelModel",{std::uint32_t(slot),std::uint32_t(panel),0});
    memory_.store(p+0x24,4,rows*memory_.load(0x4f4240,4));
    if(signed32(scalar_call("net_getLocalPlayerSlot",{0}))==signed32(slot)) memory_.store(0x44fd7c,1,0);
    if(level_up==0) return;
    auto owner=signed_load(p+0x34,4);
    if(scalar_call("net_isLocalSlot",{std::uint32_t(owner)})) sound(0x537d08,0x537d10);
    auto r=rec(owner);
    send(std::uint32_t(memory_.load(0x866d18,4)),encode(Kind::level_ack,{std::uint64_t(owner),memory_.load(r+0x59,1)}).payload);
    memory_.store(r+0xed,1,owner); memory_.store(p+0x50,4,5); memory_.store(r+0xee,1,0);
    memory_.store(0x4f65c8+(signed_load(network_mode_address,1)!=0?0:std::uint32_t(panel)*0x74u),1,1);
}
void Machine::on_stat_point(std::int64_t slot,std::int64_t raw_stat) {
    auto r=rec(slot); auto stat=signed32(raw_stat);
    if(stat<4) {
        if(memory_.load(r+0xec,1)) {
            auto a=r+0x78+std::uint32_t(stat)*4; memory_.store(a,4,memory_.load(a,4)+1);
            memory_.store(r+0xec,1,memory_.load(r+0xec,1)+255);
        }
        if(scalar_call("net_isLocalSlot",{std::uint32_t(slot)})) sound(0x537d08,0x537f20);
        if(!memory_.load(r+0xec,1)) { memory_.store(r+0xec,1,0); memory_.store(r+0xee,1,1); }
    } else {
        if(std::uint32_t(stat)-4<4) { auto a=r+0x68+std::uint32_t(stat)*4; memory_.store(a,4,memory_.load(a,4)+1); }
        if(stat==8) memory_.store(r+0xe8,4,memory_.load(r+0xe8,4)+1);
    }
    scalar_call("player_recalcStats",{});
}
void Machine::on_skill_point(std::int64_t slot,std::int64_t is_skill,std::int64_t value) {
    auto r=rec(slot); if(signed_load(r+0xe8,4)<=0) return;
    if(is_skill==0) {
        auto mask=std::uint64_t(std::int64_t(signed32(value)));
        if(passive_level(std::uint64_t(slot),mask)>=20) return;
        memory_.store(r+0xe8,4,memory_.load(r+0xe8,4)-1); rebuild_abilities(mask,slot,-1);
        raise_passive_level(slot,mask,passive_level(std::uint64_t(slot),mask)+1); return;
    }
    auto id=signed32(value); if(skill_rank(slot,id)>=20) return;
    memory_.store(r+0xe8,4,memory_.load(r+0xe8,4)-1);
    if(skill_rank(slot,id)>0) set_skill_rank(slot,id,skill_rank(slot,id)+1);
    else if(memory_.load(std::uint32_t(memory_.load(skill_table+std::uint32_t(id)*4,4))+8,4)==3) set_skill_rank(slot,id,1);
    else add_skill_ability(id,slot);
}
void Machine::inv_dispatch(const Bytes& payload) {
    auto p=payload; p.resize(std::max<std::size_t>(16,p.size()),0);
    auto b=[&](unsigned i){return p[i]<128?int(p[i]):int(p[i])-256;};
    auto h=[&](unsigned i){return signed16(p[i]|std::uint32_t(p[i+1])<<8);};
    auto w=[&](unsigned i){return std::uint32_t(p[i])|std::uint32_t(p[i+1])<<8|std::uint32_t(p[i+2])<<16|std::uint32_t(p[i+3])<<24;};
    switch(b(0)) {
    case 0: drop_item(b(1),w(4),b(10),h(2),h(8),b(11),b(12)); send_character_to_lobby(b(1)); break;
    case 1: equip_item(h(2),w(4),b(1)); break;
    case 2: unequip_item(h(2),w(4)); break;
    case 4: on_join_panel(b(1),b(2),b(3)); break;
    case 5: memory_.store(rec(b(1))+weapon_class,1,p[2]); scalar_call("player_resetIdleAnim",{std::uint32_t(b(1))}); break;
    case 6: on_stat_point(b(1),h(2)); break;
    case 7: on_skill_point(b(1),b(2),w(4)); break;
    case 8: scalar_call("inventory_combineGems",{std::uint32_t(b(1)),w(4),w(8),w(12)}); break;
    case 9: memory_.store(rec(b(1))+0x58,1,p[2]); break;
    case 10: scalar_call("Player_usePotion",{memory_.load(rec(b(1))+object,4),std::uint32_t(h(2)),std::uint32_t(h(4)),std::uint32_t(h(6)),std::uint32_t(h(8))}); break;
    default: break;
    }
}

void Machine::shop_on_msg(const Bytes& payload) {
    auto p=payload; p.resize(std::max<std::size_t>(16,p.size()),0);
    auto b=[&](unsigned i){return p[i]<128?int(p[i]):int(p[i])-256;};
    auto h=[&](unsigned i){return signed16(p[i]|std::uint32_t(p[i+1])<<8);};
    auto w=[&](unsigned i){return std::uint32_t(p[i])|std::uint32_t(p[i+1])<<8|std::uint32_t(p[i+2])<<16|std::uint32_t(p[i+3])<<24;};
    if(b(0)==0) shop_buy(b(1),w(4));
    else if(b(0)==1) shop_sell(b(1),w(4),b(10),h(2),h(8),b(11),b(12));
}
void Machine::shop_buy(std::int64_t slot,std::uint32_t record) {
    namespace C=champions::simulation::rta;
    auto r=rec(slot), definition=std::uint32_t(memory_.load(record,4));
    auto factor=0x3f199999u;
    if(signed_load(definition+0x1d,1)==1) factor=0x3ee66666;
    if((memory_.load(record+8,8)&0x30000000000ull) || signed_load(definition+0x1c,1)==8) factor=C::ONE;
    auto price=C::to_int(C::mul(C::from_int(std::uint32_t(item_value(record))),factor));
    auto gold=signed_load(r+0xe4,4);
    if(gold<price) return;
    memory_.store(r+0xe4,4,std::uint32_t(gold)-std::uint32_t(price));
    give_item(slot,record,1);
}
void Machine::shop_sell(std::int64_t slot,std::int64_t index,std::int64_t bag,
                        std::int64_t column,std::int64_t row,std::int64_t panel,std::int64_t mode) {
    namespace C=champions::simulation::rta;
    auto r=rec(slot), item=r+0x130u+std::uint32_t(index)*0x48u+std::uint32_t(bag)*bag_size;
    auto value=C::from_int(std::uint32_t(item_value(item)));
    auto definition=std::uint32_t(memory_.load(item,4));
    if(signed_load(definition+0x1c,1)==8 || contains(cstr(std::uint32_t(memory_.load(definition,4))),cstr(0x538660))
       || (memory_.load(definition+0x28,8)&0x1c0000000000ull)) value=C::mul(value,0x41a00000);
    auto paid=C::to_int(C::mul(C::div(value,C::from_int(std::uint32_t(signed_load(item+4,2)))),0x3d4ccccc));
    if(paid<=0) paid=1;
    if(signed_load(definition+0x1c,1)==8 || (memory_.load(definition+0x28,8)&0x400000000000ull)) paid=0;
    auto gold=signed32(std::uint32_t(signed_load(r+0xe4,4))+std::uint32_t(paid));
    memory_.store(r+0xe4,4,std::uint32_t(gold));
    if(gold>99999999) memory_.store(r+0xe4,4,99999999);
    drop_item(slot,index,bag,column,row,panel,mode);
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
