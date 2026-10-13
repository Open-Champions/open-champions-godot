// Synthetic input driver: expected outputs are supplied only by the Python runner.
#include "champions/messages.hpp"
#include <iostream>
#include <sstream>
using namespace champions::messages::rta;
namespace {
std::string hex(std::uint64_t n) { std::ostringstream s; s<<std::hex<<n; return s.str(); }
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
std::string execute(const std::string& script) {
    Memory memory;
    std::uint32_t slot=0, mutation_address=0; unsigned mutation_size=0;
    std::uint64_t mutation_value=0;
    bool local_throw=false, external_throw=false, local_mutation=false;
    std::map<std::string,std::uint64_t> services;
    std::string service_throw;
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
        }
    });
    std::string results,command; std::istringstream commands(script);
    while(std::getline(commands,command,';')) {
        std::istringstream tokens(command); std::vector<std::string> t; std::string word;
        while(tokens>>word) t.push_back(word);
        if(t.empty()) throw std::invalid_argument("empty action");
        auto n=[&](std::size_t i) { return number(t.at(i)); };
        auto arity=[&](std::size_t size) { if(t.size()!=size) throw std::invalid_argument("action arity"); };
        std::string result;
        try {
            if(t[0]=="poke") { arity(4); memory.poke(std::uint32_t(n(1)),unsigned(n(2)),n(3)); }
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
                if(name=="skill_rank") { arity(4); result=hex(std::uint64_t(std::int64_t(machine.skill_rank(v(2),v(3))))); }
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
          catch(const std::invalid_argument&) { throw; }
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
        if(!c.service_call) out+=bytes(c.payload)+":"+hex(c.argument);
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
