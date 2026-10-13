#include "champions/messages.hpp"
#include <array>
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
    calls.push_back({"net_getLocalPlayerSlot",{},0});
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
    if (handler==0x1bfdb0 || handler==0x1d8ca0 || handler==0x1c5d28)
        throw Unsupported("inventory/game/shop execution is not ported");
    std::string name;
    switch (handler) {
    case 0x198f64: name="cutscene_onMsg"; break;
    case 0x31f354: name="voice_onMsg"; break;
    case 0x22ee2c: name="dialog_onMsg"; break;
    case 0x31d82c: name="text_onMsg"; break;
    default: throw std::invalid_argument("no handler for channel");
    }
    calls.push_back({name,payload,payload.size()});
    if (callbacks_.external_handler) callbacks_.external_handler(name,payload,payload.size());
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
