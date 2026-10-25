// Synthetic input driver: expected outputs are supplied only by the Python runner.
#include "champions/messages.hpp"
#include <iostream>
#include <sstream>
using namespace champions::messages::rta;
namespace {
std::string hex(std::uint64_t n) { std::ostringstream s; s<<std::hex<<n; return s.str(); }
std::string hex(const Integer& n) { return n.hex(); }
std::string bytes(const Bytes& b) {
    if(b.empty()) return "-";
    const char* digits="0123456789abcdef";
    std::string s; for(auto x:b) { s+=digits[x>>4]; s+=digits[x&15]; } return s;
}
Bytes unbytes(const std::string& s) {
    if(s=="-") return {};
    if(s.size()%2) throw std::invalid_argument("odd byte string");
    Bytes b;
    for(std::size_t i=0;i<s.size();i+=2) {
        auto pair=s.substr(i,2); std::size_t used=0;
        auto x=std::stoul(pair,&used,16);
        if(used!=2) throw std::invalid_argument("invalid bytes");
        b.push_back(std::uint8_t(x));
    }
    return b;
}
std::uint64_t number(const std::string& s) {
    std::size_t used=0; auto n=std::stoull(s,&used,16);
    if(used!=s.size()) throw std::invalid_argument("invalid number");
    return n;
}
std::any general_value(const std::string& token) {
    auto value=token.substr(2);
    if(token.substr(0,2)=="i=") return Integer::from_hex(value);
    if(token.substr(0,2)=="b=") return unbytes(value);
    if(token.substr(0,2)=="n=") return std::any{};
    if(token.substr(0,2)=="t=") return value;
    throw std::invalid_argument("general value");
}
std::string general_text(const std::any& value) {
    if(!value.has_value()) return "n=-";
    if(auto v=std::any_cast<Integer>(&value)) return "i="+v->hex();
    if(auto v=std::any_cast<std::uint64_t>(&value)) return "i="+hex(*v);
    if(auto v=std::any_cast<Bytes>(&value)) return "b="+bytes(*v);
    return "t="+std::any_cast<std::string>(value);
}
std::string execute(const std::string& script) {
    Memory memory; Items::Tables tables;
    tables.circlet_stem_equip="authored-circlet"; tables.thrown_stem="authored-thrown";
    tables.weapon_multipliers.assign(256,0x3f800000);
    Position spot{}; bool vector_return=false;
    std::uint32_t slot=0, mutation_address=0; unsigned mutation_size=0;
    std::uint64_t mutation_value=0;
    bool local_throw=false, external_throw=false, local_mutation=false;
    std::map<std::string,std::uint64_t> services;
    std::string service_throw;
    std::vector<std::pair<Integer,std::int64_t>> backing_calls; bool backing_on=false;
    std::map<std::string,Callbacks::General> general;
    Machine machine(memory,{
        [&](std::uint32_t)->std::uint32_t {
            if(local_mutation && mutation_size) memory.poke(mutation_address,mutation_size,mutation_value);
            if(local_throw) throw std::runtime_error("callback-error");
            return slot;
        },
        [&](const std::string&,const Bytes&,std::uint64_t) {
            if(mutation_size) memory.poke(mutation_address,mutation_size,mutation_value);
            if(external_throw) throw std::runtime_error("callback-error");
        },
        [&](const std::string& name,const std::vector<std::uint64_t>&)->std::uint64_t {
            if(mutation_size) memory.poke(mutation_address,mutation_size,mutation_value);
            if(name==service_throw) throw std::runtime_error("callback-error");
            return services[name];
        },
        [&](const std::string& name,const std::vector<Argument>&)->std::uint64_t {
            if(mutation_size) memory.poke(mutation_address,mutation_size,mutation_value);
            if(name==service_throw) throw std::runtime_error("callback-error");
            return services[name];
        },
        [&](const std::string& name,const std::vector<Argument>&)->std::variant<std::uint64_t,Position> {
            if(mutation_size) memory.poke(mutation_address,mutation_size,mutation_value);
            if(name==service_throw) throw std::runtime_error("callback-error");
            if(vector_return) return spot;
            return services[name];
        },
        [&](const std::string& name) { return general[name]; }
    }, &tables);
    std::string results,command; std::istringstream commands(script);
    while(std::getline(commands,command,';')) {
        std::istringstream tokens(command); std::vector<std::string> t; std::string word;
        while(tokens>>word) t.push_back(word);
        if(t.empty()) throw std::invalid_argument("empty action");
        auto n=[&](std::size_t i) { return number(t.at(i)); };
        auto arity=[&](std::size_t size) { if(t.size()!=size) throw std::invalid_argument("action arity"); };
        std::string result;
        try {
            if(t[0]=="backing") {
                arity(3); backing_on=true; auto value=Integer::from_hex(t[1]); auto mode=t[2];
                memory.under=[&,value,mode](const Integer& a,std::int64_t w) {
                    backing_calls.emplace_back(a,w);
                    if(mode=="throw") throw std::runtime_error("callback-error");
                    if(mode=="mutate") memory.poke(a.next(),1,Integer(0xee));
                    return value;
                };
            }
            else if(t[0]=="gpoke" || t[0]=="gstore") {
                arity(4); auto a=Integer::from_hex(t[1]); auto w=s64(Integer::from_hex(t[2]).low64()); auto v=Integer::from_hex(t[3]);
                if(t[0]=="gpoke") memory.poke(a,w,v); else memory.store(a,w,v);
            }
            else if(t[0]=="gload") { arity(3); result="integer="+memory.load(Integer::from_hex(t[1]),s64(Integer::from_hex(t[2]).low64())).hex(); }
            else if(t[0]=="gcall") {
                if(t.size()<3) throw std::invalid_argument("general call arity");
                auto mode=t[2]; general[t[1]]=mode=="missing"?Callbacks::General{}:Callbacks::General([mode](const std::vector<std::any>& args)->std::any {
                    if(mode=="throw") throw std::runtime_error("callback-error");
                    return args.empty()?std::any{}:args.back();
                });
                std::vector<std::any> args; for(std::size_t i=3;i<t.size();++i) args.push_back(general_value(t[i]));
                result="general="+general_text(machine.call(t[1],args));
            }
            else if(t[0]=="definition") {
                arity(9); auto d=std::make_shared<Items::Definition>();
                d->addr=std::uint32_t(n(1)); d->category=Items::s32(std::uint32_t(n(2)));
                d->slot=Items::s32(std::uint32_t(n(3))); d->subtype=Items::s32(std::uint32_t(n(4)));
                d->price=Items::s32(std::uint32_t(n(5))); d->damage_bits=std::uint32_t(n(6));
                d->flags=n(7); auto name=unbytes(t[8]); d->name=std::string(name.begin(),name.end());
                bool replaced=false; for(auto& old:tables.defs) if(old->addr==d->addr) { old=d; replaced=true; break; }
                if(!replaced) tables.defs.push_back(d);
            }
            else if(t[0]=="spot") { arity(4); spot={std::uint32_t(n(1)),std::uint32_t(n(2)),std::uint32_t(n(3))}; vector_return=true; }
            else if(t[0]=="shop") { arity(2); machine.shop_on_msg(unbytes(t[1])); result="ok"; }
            else if(t[0]=="inv") { arity(2); machine.inv_dispatch(unbytes(t[1])); result="ok"; }
            else if(t[0]=="sound") {
                arity(6); SoundTable table{}; for(unsigned i=0;i<13;++i) table[i]=0x1000+17*i;
                result=hex(sound_name(table,n(1),std::int64_t(n(2)),n(3),n(4)!=0));
                if(n(5)!=0) throw std::invalid_argument("sound reserved field");
            }
            else if(t[0]=="signed64") { arity(2); result="signed="+std::to_string(s64(n(1))); }
            else if(t[0]=="poke") { arity(4); memory.poke(std::uint32_t(n(1)),unsigned(n(2)),n(3)); }
            else if(t[0]=="local") { arity(2); slot=std::uint32_t(n(1)); }
            else if(t[0]=="throwlocal") { arity(1); local_throw=true; }
            else if(t[0]=="mutatelocal") { arity(1); local_mutation=true; }
            else if(t[0]=="throwexternal") { arity(1); external_throw=true; }
            else if(t[0]=="mutate") { arity(4); mutation_address=std::uint32_t(n(1)); mutation_size=unsigned(n(2)); mutation_value=n(3); }
            else if(t[0]=="service") { arity(3); services[t[1]]=n(2); }
            else if(t[0]=="throwservice") { arity(2); service_throw=t[1]; }
            else if(t[0]=="invoke") {
                if(t.size()<2) throw std::invalid_argument("invoke arity");
                auto v=[&](std::size_t i)->std::int64_t {
                    auto x=n(i); return x<=INT64_MAX ? std::int64_t(x) : -1-std::int64_t(UINT64_MAX-x);
                };
                auto name=t[1]; result="ok";
                if(name=="u8") { arity(3); result=hex(std::uint64_t(machine.u8(n(2)))); }
                else if(name=="s8") { arity(3); result=hex(std::uint64_t(machine.s8(n(2)))); }
                else if(name=="u16") { arity(3); result=hex(std::uint64_t(machine.u16(n(2)))); }
                else if(name=="s16") { arity(3); result=hex(std::uint64_t(machine.s16(n(2)))); }
                else if(name=="u32") { arity(3); result=hex(std::uint64_t(machine.u32(n(2)))); }
                else if(name=="s32") { arity(3); result=hex(std::uint64_t(machine.s32(n(2)))); }
                else if(name=="u64") { arity(3); result=hex(std::uint64_t(machine.u64(n(2)))); }
                else if(name=="w8") { arity(4); machine.w8(n(2),n(3)); }
                else if(name=="w16") { arity(4); machine.w16(n(2),n(3)); }
                else if(name=="w32") { arity(4); machine.w32(n(2),n(3)); }
                else if(name=="w64") { arity(4); machine.w64(n(2),n(3)); }
                else if(name=="shop_buy") { arity(4); machine.shop_buy(v(2),std::uint32_t(n(3))); }
                else if(name=="shop_sell") { arity(9); machine.shop_sell(v(2),v(3),v(4),v(5),v(6),v(7),v(8)); }
                else if(name=="send_character_to_lobby") { arity(3); machine.send_character_to_lobby(v(2)); }
                else if(name=="item_object") { arity(3); auto packed=machine.item_object(std::uint32_t(n(2))).pack(); result="bytes="+bytes(Bytes(packed.begin(),packed.end())); }
                else if(name=="check_equip_allowed") { arity(4); result=hex(machine.check_equip_allowed(v(2),std::uint32_t(n(3)))); }
                else if(name=="item_value") { arity(3); result=hex(std::uint64_t(std::int64_t(machine.item_value(std::uint32_t(n(2)))))); }
                else if(name=="give_item") { arity(5); result=hex(std::uint64_t(std::int64_t(machine.give_item(v(2),std::uint32_t(n(3)),v(4))))); }
                else if(name=="first_recall_hint") { arity(4); machine.first_recall_hint(v(2),std::uint32_t(n(3))); }
                else if(name=="unref_item") { arity(4); machine.unref_item(v(2),n(3)); }
                else if(name=="drop_item") { arity(9); machine.drop_item(v(2),v(3),v(4),v(5),v(6),v(7),v(8)); }
                else if(name=="equip_item") { arity(5); machine.equip_item(v(2),n(3),v(4)); }
                else if(name=="_equip_item") { arity(5); machine._equip_item(v(2),n(3),v(4)); }
                else if(name=="unequip_item") { arity(4); machine.unequip_item(v(2),n(3)); }
                else if(name=="on_join_panel") { arity(5); machine.on_join_panel(v(2),v(3),v(4)); }
                else if(name=="on_stat_point") { arity(4); machine.on_stat_point(v(2),v(3)); }
                else if(name=="on_skill_point") { arity(5); machine.on_skill_point(v(2),v(3),v(4)); }
                else if(name=="skill_rank") { arity(4); result=hex(std::uint64_t(std::int64_t(machine.skill_rank(v(2),v(3))))); }
                else if(name=="set_skill_rank") { arity(5); machine.set_skill_rank(v(2),v(3),v(4)); }
                else if(name=="passive_level") { arity(4); result=hex(machine.passive_level(n(2),n(3))); }
                else if(name=="raise_passive_level") { arity(5); machine.raise_passive_level(v(2),n(3),v(4)); }
                else if(name=="rebuild_abilities") { arity(5); machine.rebuild_abilities(n(2),v(3),v(4)); }
                else if(name=="add_skill_ability") { arity(4); machine.add_skill_ability(v(2),v(3)); }
                else if(name=="on_quick_slot") { arity(5); machine.on_quick_slot(v(2),v(3),v(4)); }
                else if(name=="on_assign_spell") { arity(6); machine.on_assign_spell(v(2),v(3),v(4),n(5)); }
                else if(name=="mask_local_buttons") { arity(5); machine.mask_local_buttons(v(2),v(3),std::uint32_t(n(4))); }
                else if(name=="on_menu_state") { arity(4); machine.on_menu_state(v(2),v(3)); }
                else if(name=="on_quick_cast") { arity(4); machine.on_quick_cast(v(2),v(3)); }
                else if(name=="recall") { arity(3); machine.recall(v(2)); }
                else if(name=="stop_stream") { arity(2); machine.stop_stream(); }
                else if(name=="on_player_left") { arity(3); machine.on_player_left(v(2)); }
                else if(name=="on_disconnect_request") { arity(3); machine.on_disconnect_request(v(2)); }
                else if(name=="on_checksum") { arity(5); machine.on_checksum(v(2),n(3),n(4)); }
                else if(name=="remove_item") { arity(6); machine.remove_item(v(2),v(3),v(4),v(5)); }
                else if(name=="grid") { arity(4); result=hex(machine.grid(v(2),v(3))); }
                else if(name=="same_identity") { arity(4); result=hex(machine.same_identity(std::uint32_t(n(2)),std::uint32_t(n(3)))); }
                else if(name=="copy") { arity(5); machine.copy(std::uint32_t(n(2)),std::uint32_t(n(3)),v(4)); }
                else if(name=="cstr") { arity(4); result="bytes="+bytes(machine.cstr(std::uint32_t(n(2)),v(3))); }
                else if(name=="wstr") { arity(4); result="bytes="+bytes(machine.wstr(std::uint32_t(n(2)),v(3))); }
                else if(name=="vec") { arity(3); auto p=machine.vec(std::uint32_t(n(2))); result="vec="+hex(p[0])+","+hex(p[1])+","+hex(p[2]); }
                else throw std::invalid_argument("unknown method");
            }
            else if(t[0]=="level") { arity(3); machine.set_pending_level(unbytes(t[1]),n(2)!=0); result="ok"; }
            else if(t[0]=="hud") { arity(2); machine.hud_on_msg(unbytes(t[1])); result="ok"; }
            else if(t[0]=="register") { arity(2); result=hex(machine.register_handler(std::uint32_t(n(1)))); }
            else if(t[0]=="all") {
                arity(1); for(auto id:machine.register_all()) { if(!result.empty()) result+=','; result+=hex(id); }
            }
            else if(t[0]=="send" || t[0]=="deliver") {
                arity(3); auto payload=unbytes(t[2]);
                if(t[0]=="send") {
                    try { machine.send(std::uint32_t(n(1)),payload); }
                    catch(const std::invalid_argument&) { result="error"; }
                }
                else {
                    try { machine.deliver(std::uint32_t(n(1)),payload); }
                    catch(const std::invalid_argument&) { result="error"; }
                }
                if(result.empty()) result="ok";
            }
            else if(t[0]=="encode") {
                if(t.size()<3) throw std::invalid_argument("encode arity");
                std::vector<std::uint64_t> args;
                for(std::size_t i=3;i<t.size();++i) args.push_back(n(i));
                auto msg=encode(static_cast<Kind>(n(1)),args,unbytes(t[2]));
                result=msg.family+":"+bytes(msg.payload);
            }
            else throw std::invalid_argument("unknown action");
        } catch(const Unsupported&) { result="unsupported"; }
          catch(const std::invalid_argument&) {
              if(t[0]=="invoke" && t.size()>1 && t[1]=="send_character_to_lobby") result="error";
              else throw;
          }
          catch(const std::runtime_error& e) {
              if(std::string(e.what())!="callback-error") throw;
              result="callback-error";
          }
        if(!result.empty()) { if(!results.empty()) results+='|'; results+=result; }
    }
    std::string out="M";
    for(auto entry:memory.bytes) out+=";"+hex(entry.first)+":"+hex(entry.second);
    out+="/W";
    for(auto w:memory.writes) out+=";"+hex(w.address)+":"+hex(w.size)+":"+hex(w.value);
    out+="/C";
    for(auto c:machine.calls) {
        out+=";"+c.name+":";
        if(c.name.rfind("generic_",0)==0) { out+="general"; for(const auto& a:c.general_arguments) out+=":"+general_text(a); }
        else if(!c.service_call) out+=bytes(c.payload)+":"+hex(c.argument);
        else {
            out+="args";
            for(auto a:c.arguments) out+=":"+hex(a);
            for(const auto& a:c.typed_arguments) {
                out+=':';
                if(auto scalar=std::get_if<std::uint64_t>(&a)) out+=hex(*scalar);
                else if(auto text=std::get_if<Bytes>(&a)) out+="bytes="+bytes(*text);
                else {
                    out+="vec="; const auto& v=std::get<Position>(a);
                    out+=hex(v[0])+","+hex(v[1])+","+hex(v[2]);
                }
            }
        }
    }
    if(backing_on) { out+="/B"; for(const auto& p:backing_calls) out+=";"+p.first.hex()+":"+(p.second<0?Integer::from_hex("-"+hex(std::uint64_t(-(p.second+1))+1)).hex():hex(p.second)); }
    return out+"/R;"+results;
}
}
int main() {
    std::string line;
    try {
        while(std::getline(std::cin,line)) {
            if(line.empty()) throw std::invalid_argument("empty input");
            std::cout<<execute(line)<<'\n';
        }
        if(!std::cin.eof()) throw std::runtime_error("input failure");
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
