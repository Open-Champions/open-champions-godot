#include "champions/inventory_icon_assoc.hpp"
#include "champions/inventory_icons.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace icons = champions::inventory_icons;
namespace {

int checks = 0;
void expect(bool yes, const char *why) {
    ++checks;
    if (!yes) throw std::runtime_error(why);
}
template <class Action> void rejects(Action action, const char *why) {
    bool caught = false;
    try { action(); } catch (const std::invalid_argument &) { caught = true; }
    expect(caught, why);
}
icons::PictureTables tables() {
    icons::PictureTables t;
    t.model_textures = {"mdl0.tex", "axe1.tex"};
    t.prefix_colours = {{{3, 2, 9}, {0, 5, 0}, {12, -1, 4}}};
    for (unsigned i = 0; i < 12; ++i) t.body_codes[i] = "b" + std::to_string(i);
    return t;
}
icons::PictureItem item(std::string name = "sword", int category = 0, int slot = 0,
                        int subtype = 0, int damage_min = 4, int damage_max = 6,
                        std::optional<std::string> picture = "pic_sword1", std::size_t row = 0,
                        unsigned variant = 0, std::uint8_t prefix = icons::NO_PREFIX,
                        std::uint64_t flags = 0) {
    return {{std::move(name), category, slot, subtype, damage_min, damage_max,
             std::move(picture), row}, flags | (std::uint64_t(variant) << 11), prefix};
}
std::string name(const icons::PictureItem &i, int body = 0,
                 std::optional<int> value = std::nullopt) {
    return icons::picture_filename(i, body, tables(), value);
}
void test_names() {
    // Every synthetic filename and body-substitution vector in test_ref_icon_assoc.py.
    expect(name(item()) == "pic_sword11.tex", "weapon default");
    expect(name(item("sword",0,0,0,4,6,"pic_sword1",0,2,0)) == "pic_sword33.tex", "weapon variant colour");
    expect(name(item("sword",0,0,0,4,6,std::nullopt,1)) == "axe11.tex", "model fallback");
    expect(name(item("sword",0,0,4,4,6,"pic_sword1",0,0,0)) == "pic_sword11.tex", "bow");
    expect(name(item("sword",0,0,0,4,6,"pic_sword1",0,0,0,icons::ALT_COLOUR)) == "pic_sword19.tex", "alternate");
    expect(name(item("sword",0,0,4,4,6,"pic_sword1",0,0,2,icons::ALT_COLOUR)) == "pic_sword14.tex", "bow alternate precedence");
    expect(name(item("sword",0,0,0,4,6,"pic_sword1",0,0,1)) == "pic_sword11.tex", "zero colour becomes one");
    expect(name(item("sword+x",0,0,0,20,13)) == "pic_sword115.tex", "preset weapon remainder");
    expect(name(item("sword+x",0,0,0,9,9)) == "pic_sword11.tex", "preset zero remainder");
    expect(name(item("sword",0,0,0,4,6,"pic_sword1",0,31)) == "pic_sword01.tex", "wrapped variant");
    expect(name(item("sword",0,0,0,4,6,"pic_sword1",0,3,0,icons::PLAIN_WEAPON)) == "pic_sword4.tex", "plain weapon");
    const auto alias_a = name(item("sword",0,0,0,4,6,"pic_sword1",0,0,2));
    const auto alias_b = name(item("sword+",0,0,0,1,1,"pic_sword1",0,10));
    expect(alias_a == alias_b && alias_a == "pic_sword112.tex", "concatenation alias");
    const int cloth[] = {6,7,6,7,6,7,6,7,6,7,6,6};
    const int plate[] = {0,1,2,3,4,5,0,1,8,9,10,11};
    for (int b = 0; b < 12; ++b) {
        expect(icons::armour_body(b, true) == cloth[b], "cloth body map");
        expect(icons::armour_body(b, false) == plate[b], "plate body map");
    }
    auto armour = item("plate",1,4,0,4,6,"chest");
    expect(name(armour,3) == "b3_chest1.tex", "armour body");
    armour.prefix = 1;
    expect(name(armour,6) == "b0_chest5.tex", "armour mapped prefix");
    armour.prefix = 2;
    expect(name(armour,2) == "b2_chest-1.tex", "negative armour colour");
    armour.prefix = icons::NO_PREFIX; armour.flags = icons::CLOTH;
    expect(name(armour,9) == "b7_chest1.tex", "cloth armour");
    armour.definition.name = "plate+y"; armour.flags = 0; armour.prefix = 0;
    expect(name(armour,5,-7) == "b5_chest-3.tex", "negative preset remainder");
    armour.prefix = icons::NO_PREFIX;
    expect(name(armour,5,7) == "b5_chest1.tex", "no-prefix preset override");
    armour.prefix = 0;
    rejects([&] { name(armour,5); }, "missing item value refusal");
    armour.definition.slot = 3;
    expect(name(armour,7) == "chest.tex", "plain slot 3");
    expect(name(item("circlet24",1,1,0,4,6,"circ"),7) == "b7_cloth2helmet641.tex", "circlet 2");
    expect(name(item("circlet35",1,1,0,4,6,"circ",0,0,0),6) == "b6_cloth3helmet652.tex", "circlet 3");
    expect(name(item("circlet12",1,1,0,4,6,"circ",0,0,icons::NO_PREFIX,icons::CLOTH),2) == "b2_circ1.tex", "circlet noncloth body bypass");
    rejects([&] { name(item("gold circlet1",1,1,0,4,6,"circ"),6); }, "unreadable circlet prefix");
    rejects([&] { name(item("circlet3",1,1,0,4,6,"circ"),6); }, "missing circlet number");
    expect(name(item("sword",2,0,0,4,6,"gem1",0,4)) == "gem5.tex", "other variant");
    expect(name(item("sword",2,0,0,4,6,"gem",0,4)) == "gem.tex", "other plain");
    expect(name(item("sword",2,0,0,4,6,"ab1.texq",0,1)) == "ab1.tex2.tex", "substring and last-five splice");
    rejects([&] { name(item(),12); }, "invalid viewing body");
    rejects([&] { name(item("sword",0,0,0,4,6,std::nullopt,9)); }, "invalid model row");
}
using Bytes = std::vector<std::uint8_t>;
void append16(Bytes &b, std::uint16_t x) { b.push_back(std::uint8_t(x)); b.push_back(std::uint8_t(x >> 8)); }
void append32(Bytes &b, std::uint32_t x) { append16(b,std::uint16_t(x)); append16(b,std::uint16_t(x >> 16)); }
void write32(Bytes &b, std::size_t at, std::uint32_t x) {
    for (unsigned i = 0; i < 4; ++i) b[at+i] = std::uint8_t(x >> (i*8));
}
Bytes index_bytes(const icons::PictureIndex &groups) {
    Bytes b(4 + groups.size()*12);
    write32(b,0,std::uint32_t(groups.size()));
    for (std::size_t g = 0; g < groups.size(); ++g) {
        const auto stem = std::uint32_t(b.size());
        b.insert(b.end(),groups[g].stem.begin(),groups[g].stem.end()); b.push_back(0);
        const auto entries = std::uint32_t(b.size());
        for (const auto &e : groups[g].entries) {
            append16(b,std::uint16_t(e.number)); append16(b,e.first_sector); append16(b,e.sectors);
            append16(b,std::uint16_t(e.width)); append16(b,std::uint16_t(e.height));
        }
        write32(b,4+g*12,entries); write32(b,8+g*12,stem);
        write32(b,12+g*12,std::uint32_t(groups[g].entries.size()));
    }
    return b;
}
icons::PictureIndex parsed(const icons::PictureIndex &groups) {
    const auto b = index_bytes(groups);
    return icons::parse_index(b.data(),b.size());
}
void test_index() {
    const auto groups = parsed({{"pic_sword",{{11,0,3,40,80},{112,3,3,41,82}}},
                                {"gem",{{-1,6,2,20,24}}},
                                {"pic_sword",{{12,8,3,42,84}}},
                                {"a",{{7,11,2,8,8}}}});
    expect(icons::split_picture_name("pic_sword112.tex") == std::make_pair(std::string("pic_sword"),112), "split numeric suffix");
    expect(icons::split_picture_name("gem.tex") == std::make_pair(std::string("gem"),-1), "split no digits");
    expect(icons::split_picture_name("a.b.tex") == std::make_pair(std::string("a.b"),-1), "split last dot");
    expect(icons::split_picture_name("7.tex") == std::make_pair(std::string("7"),-1), "retain first digit");
    expect(icons::split_picture_name("x2y07") == std::make_pair(std::string("x2y"),7), "split leading zeros");
    expect(!icons::split_picture_name(".tex"), "empty stem");
    expect(icons::find_entry(groups,"pic_sword12.tex")->first_sector == 8, "continue to later group");
    expect(icons::find_entry(groups,"pic_sword112.tex")->width == 41, "numeric lookup");
    expect(icons::find_entry(groups,"gem.tex")->number == -1, "negative-one entry");
    expect(icons::find_entry(groups,"a7.tex")->height == 8, "single-letter stem");
    for (const char *miss : {"PIC_SWORD11.tex","pic_sword13.tex","gem1.tex",".tex","a.tex"}) {
        expect(icons::find_entry(groups,miss) == nullptr, "missing index name");
    }
    auto data = index_bytes({{"s",{{1,0,1,2,2}}}});
    data.pop_back();
    rejects([&] { icons::parse_index(data.data(),data.size()); }, "short entry");
    data = {9,0,0,0};
    rejects([&] { icons::parse_index(data.data(),data.size()); }, "short group table");
    rejects([&] { icons::split_picture_name("x999999999999999999999999999999999"); }, "suffix overflow");
    const auto range = icons::payload_range({1,3,2,8,8},10240);
    expect(range.offset == 6144 && range.size == 4096, "sector range");
    rejects([&] { icons::payload_range({1,3,2,8,8},8192); }, "sector bounds");
}
void test_streaming() {
    icons::PictureIndex src{{"p",{}}};
    for (int i = 0; i < 80; ++i) src[0].entries.push_back({std::int16_t(i),std::uint16_t(i),1,8,8});
    auto groups = parsed(src);
    icons::PictureCache c(groups);
    expect(!c.request("p1.tex",0), "miss queues");
    const auto placeholder = icons::picture_or_placeholder(c.request("p1.tex",0));
    expect(placeholder.placeholder && placeholder.archive == "equipmnt.lmp" &&
           placeholder.lump == "pic_dagger1.tex", "fixed placeholder");
    c.service(0,false);
    expect(c.reading() && c.slots()[0].state == icons::CacheState::Reading, "start first read");
    expect(!c.request("p1.tex",1), "reading still placeholder");
    c.service(1,false); c.service(2,true);
    const auto loaded = c.request("p1.tex",3);
    expect(loaded && loaded->number == 1, "loaded request");
    expect(!icons::picture_or_placeholder(loaded).placeholder, "loaded source");

    icons::PictureCache queue(groups);
    for (const char *n : {"p2.tex","q.tex","p3.tex"}) queue.request(n,0);
    queue.service(0,false);
    expect(queue.slots()[0].state == icons::CacheState::Reading &&
           queue.slots()[1].state == icons::CacheState::Pending &&
           queue.slots()[2].state == icons::CacheState::Pending, "one read per frame");
    queue.service(1,true);
    expect(queue.slots()[0].state == icons::CacheState::Loaded &&
           queue.slots()[1].state == icons::CacheState::Missing &&
           queue.slots()[2].state == icons::CacheState::Reading, "completion then one read");
    expect(!queue.request("q.tex",2), "missing stays placeholder");

    icons::PictureCache full(groups);
    for (int i = 0; i < 64; ++i) full.request("p"+std::to_string(i)+".tex",i%3);
    expect(full.slots().size() == 64, "capacity 64");
    full.request("p70.tex",10);
    expect(full.slots().size() == 64 &&
           std::none_of(full.slots().begin(),full.slots().end(),[](const auto &s){return s.name=="p0.tex";}),
           "oldest eligible eviction");
    // The reference vector sets all last-use values to five; use a fresh
    // cache to test the strict eight-frame cutoff through public operations.
    icons::PictureCache recent(groups);
    for (int i = 0; i < 64; ++i) recent.request("p"+std::to_string(i)+".tex",5);
    recent.request("p71.tex",13);
    expect(std::none_of(recent.slots().begin(),recent.slots().end(),[](const auto &s){return s.name=="p71.tex";}),
           "strict eviction cutoff");
}
void test_grid() {
    const auto one = icons::grid_placement(32,34,100,2,200.0f);
    expect(one.columns == 2 && one.rows == 1 && one.footprint_width == 64 &&
           one.footprint_height == 34 && one.x == 195 && one.y == 176,
           "32 by 34 grid placement");
    const auto tall = icons::grid_placement(40,80,100,2,200.0f);
    expect(tall.columns == 2 && tall.rows == 3 && tall.x == 191 && tall.y == 187,
           "multirow placement");
    const auto fractional = icons::grid_placement(31,35,-50,0,201.75f);
    expect(fractional.columns == 1 && fractional.rows == 2 && fractional.x == -34 &&
           fractional.y == 193, "fractional origin truncation");
    rejects([&] { icons::grid_placement(0,35,0,0,0); }, "invalid dimensions");
}
} // namespace
int main() {
    try {
        test_names(); test_index(); test_streaming(); test_grid();
        std::cout << "PASS inventory-icon-assoc " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL inventory-icon-assoc after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
