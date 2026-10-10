#include "champions/model.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
namespace fs = std::filesystem;
using Buffer = std::vector<std::uint8_t>;
using Row = std::array<int,4>;
unsigned checks = 0;
void check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
void put(Buffer& b, std::size_t at, std::uint64_t value, unsigned size) {
    for (unsigned i=0;i<size;++i) b.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
void word(Buffer& b, std::uint32_t value) { const auto at=b.size(); b.resize(at+4); put(b,at,value,4); }
std::size_t unpack(Buffer& b,unsigned cmd,unsigned addr,bool uns,const std::vector<Row>& rows) {
    const auto at=b.size(); word(b,(cmd<<24)|(unsigned(rows.size())<<16)|0x8000|(uns?0x4000:0)|addr);
    const unsigned width=4U>>(cmd&3),components=((cmd>>2)&3)+1;
    for(const auto& row:rows) for(unsigned c=0;c<components;++c) {
        const auto out=b.size();b.resize(out+width);put(b,out,static_cast<std::uint32_t>(row[c]),width);
    }
    while(b.size()%4)b.push_back(0);
    return at;
}
struct Fixture { Buffer bytes; std::size_t pos,norm,slots,weights,copies,tag,mscal,uv; };
Fixture make(unsigned strip=3,bool copied=false,
             const std::vector<Row>& weight_rows={{0,0,255,3}}) {
    Fixture f{};auto& b=f.bytes;b.resize(0x80,0);
    put(b,0,1,8);put(b,0x12,1,1);put(b,0x13,255,1);put(b,0x25,4,1);put(b,0x26,1,1);put(b,0x28,0x80,4);
    word(b,0x04000003);
    f.pos=unpack(b,0x69,0,false,{{0,0,0,0},{160,0,0,0},{0,160,80,0}});
    f.norm=unpack(b,0x6a,3,false,{{0,0,127,0},{0,0,127,0},{0,0,127,0}});
    f.slots=unpack(b,0x69,6,true,{{3,0,3,0},{3,0,3,0},{0,0,0,0},{3,0,0,0},{6,0,0,0}});
    f.weights=unpack(b,0x6e,11,true,weight_rows);
    std::vector<Row> copies{{0,0,0,0}};
    if(copied) { copies[0][0]=1;copies.push_back({0,9,3,12}); }
    if(strip==83) {
        copies={{40,0,0,0}};
        for(unsigned i=3;i<83;i+=2) copies.push_back({int((i%3)*3),int(i*3),int(((i+1)%3)*3),int((i+1)*3)});
    }
    const auto copy_address=11+unsigned(weight_rows.size());
    f.copies=unpack(b,0x6d,copy_address,true,copies);
    const std::uint64_t low=strip|0x8000ULL|(1ULL<<46)|(0x15cULL<<47)|(3ULL<<60);
    f.tag=unpack(b,0x6c,copy_address+unsigned(copies.size()),false,{{int(low&0xffffffffU),int(low>>32),0x413,0}});
    f.mscal=b.size();word(b,0x14000042);word(b,0x01000103);
    std::vector<Row> uv;
    for(unsigned i=0;i<strip;++i) uv.push_back({int(i*16),int(i*32),0,0});
    f.uv=unpack(b,0x65,0x81,true,uv);word(b,0x01000101);
    while(b.size()%16)b.push_back(0);
    put(b,0x2c,b.size(),4);return f;
}
champions::RtaModel decode(const Buffer& b) { return champions::decodeRtaModel({b.data(),b.size()}); }
void reject(const Buffer& b,const char* why) {
    bool failed=false;try{(void)decode(b);}catch(const champions::FormatError&){failed=true;}check(failed,why);
}
void mutation(std::size_t at,std::uint64_t value,unsigned size,const char* why) {
    auto f=make();put(f.bytes,at,value,size);reject(f.bytes,why);
}
Buffer withVariants(Buffer b, std::size_t table_start = 0x40) {
    const auto count = b.at(0x12);
    b.insert(b.begin()+table_start,512,0);
    put(b,0x10,2,2);put(b,0x21,table_start/16,1);
    put(b,0x25,b.at(0x25)+32,1);
    for (unsigned i=0;i<=count;++i) {
        const auto at=0x28+i*4;
        std::uint32_t value=0;
        for(unsigned j=0;j<4;++j)value|=std::uint32_t(b[at+j])<<(8*j);
        put(b,at,value+512,4);
    }
    return b;
}
void variantTests() {
    auto b=withVariants(make().bytes);
    put(b,8,1,8); // Own mesh0 hidden by default; part_mask is already1.
    put(b,0x40,1,8); // Variant0 reveals it.
    put(b,0x50,~std::uint64_t(0),8);put(b,0x58,~std::uint64_t(0),8); // Hide wins every conflict.
    put(b,0x40+31*16,std::uint64_t(1)<<63,8);put(b,0x48+31*16,1,8);
    auto m=decode(b);
    check(m.has_variants&&m.variants[0].show==1&&m.variants[31].show==(std::uint64_t(1)<<63)&&m.variants[31].hide==1,"all32 variant records retain unsigned64 values");
    check(champions::modelVisibilityMask(m)==~std::uint64_t(1)&&!m.meshes[0].visible_by_default,"Default preserves hidden mask independently of variants");
    check(champions::modelVisibilityMask(m,0)==~std::uint64_t(0),"variant can reveal own mesh despite parent part mask");
    check(champions::modelVisibilityMask(m,1)==0,"hide wins show/hide conflict and permits all-hidden selection");
    check(champions::modelVisibilityMask(m,2)==champions::modelVisibilityMask(m),"unused record leaves Default unchanged");
    m.hidden_mask=std::uint64_t(1)<<63;
    check((champions::modelVisibilityMask(m,31)&(std::uint64_t(1)<<63))!=0&&
          (champions::modelVisibilityMask(m,31)&1)==0,"variant31 reveals mesh63 and hides mesh0 without signed shifts");
    m.hidden_mask=~std::uint64_t(0);m.part_mask=~std::uint64_t(0);
    check(champions::modelVisibilityMask(m)==0&&champions::modelVisibilityMask(m,0)==1,"all-hidden Default can reveal one mesh; part mask excluded");
    for(int selection:{-2,32,63}) {
        bool failed=false;try{(void)champions::modelVisibilityMask(m,selection);}catch(const champions::FormatError&){failed=true;}
        check(failed,"variant selection must be Default or0..31");
    }
    const auto absent=decode(make().bytes);
    check(!absent.has_variants&&champions::modelVisibilityMask(absent)==~std::uint64_t(0),"absent table still supports Default");
    for(int selection:{0,31}) {
        bool failed=false;try{(void)champions::modelVisibilityMask(absent,selection);}catch(const champions::FormatError&){failed=true;}
        check(failed,"variant selection without a table is rejected");
    }
    b=withVariants(make().bytes);put(b,0x25,0x23,1);reject(b,"variant table must contain all32 records before next table");
    b=withVariants(make().bytes);put(b,0x21,0,1);reject(b,"variant flag cannot name an absent table");
    b=withVariants(make().bytes);put(b,0x10,0,2);reject(b,"variant table cannot omit its flag");
    b=withVariants(make().bytes);b.resize(0x230);reject(b,"truncated variant record region is rejected");
    Buffer many(0x9b0,0);put(many,0x12,64,1);put(many,0x13,255,1);put(many,0x26,1,1);put(many,0x25,0x3b,1);
    for(unsigned i=0;i<=64;++i)put(many,0x28+i*4,many.size(),4);
    many=withVariants(many,0x3b0);put(many,8,std::uint64_t(1)<<63,8);
    put(many,0x3b0+31*16,std::uint64_t(1)<<63,8);
    const auto sixtyfour=decode(many);
    check(sixtyfour.meshes.size()==64&&!sixtyfour.meshes[63].visible_by_default&&
          ((champions::modelVisibilityMask(sixtyfour,31)>>63)&1)==1,"decoded64-mesh table supports variant31 affecting mesh63");
}
void skinTests() {
    using Skin = champions::ModelSkinInfluences;
    auto same = [](const Skin& a, const Skin& b) {
        return a.count==b.count && a.joints==b.joints && a.weights==b.weights;
    };
    const Skin rigid{{63,0,0,0},{255,0,0,0},1};
    for (unsigned run : {3U,255U}) {
        auto f=make(3,false,{{252,19,255,int(run)},{0,255,255,255}});
        const auto m=decode(f.bytes);
        check(m.meshes[0].corners.size()==3,"rigid run emits only packet vertices, including overshoot");
        for (const auto& corner:m.meshes[0].corners)
            check(same(corner.skin,rigid),"rigid run ignores w1, maps joint63, and ignores terminal padding");
    }
    const std::vector<Row> blend{{0,64,252,191},
                                 {4,100,8,100},{12,55,255,207},
                                 {16,0,20,55},{24,100,28,100}};
    const std::array<Skin,3> expected{{
        {{0,63,0,0},{64,191,0,0},2},
        {{1,2,3,0},{100,100,55,0},3},
        {{4,5,6,7},{0,55,100,100},4}
    }};
    auto f=make(3,false,blend);
    const auto original=decode(f.bytes).meshes[0].corners;
    for(unsigned i=0;i<3;++i)
        check(same(original[i].skin,expected[i]),"two/three/four joint blends preserve order, byte weights, and zero-weight slots");
    std::fill(f.bytes.begin(),f.bytes.end(),0);
    check(same(original[2].skin,expected[2]),"decoded influences own their storage after source mutation");
    f=make(3,false,blend);
    for(unsigned axis=0;axis<3;++axis)put(f.bytes,f.pos+4+6+axis*2,0,2);
    const auto coincident=decode(f.bytes).meshes[0].corners;
    check(coincident.size()==3&&coincident[0].position==coincident[1].position&&
          same(coincident[0].skin,expected[0])&&same(coincident[1].skin,expected[1]),
          "coincident positions retain distinct source vertex skin influences");
    const auto runs=decode(make(3,false,{{0,87,255,1},{252,0,255,2}}).bytes).meshes[0].corners;
    check(runs[0].skin.joints[0]==0&&same(runs[1].skin,rigid)&&same(runs[2].skin,rigid),
          "consecutive rigid runs advance exactly across source vertices");
    f=make(3,false,blend);
    for(unsigned i=0;i<3;++i)put(f.bytes,f.norm+4+i*3+2,129,1);
    const auto reversed=decode(f.bytes).meshes[0].corners;
    check(reversed[0].position==original[1].position&&same(reversed[0].skin,expected[1])&&
          same(reversed[1].skin,expected[0]),"winding reversal carries each source vertex's influences");
    f=make(5,true,blend);
    const auto copied=decode(f.bytes).meshes[0].corners;
    check(copied.size()==9&&copied[5].uv_texels!=original[0].uv_texels&&
          same(copied[5].skin,expected[0]),"copied strip UV seam retains original vertex influences");
    for(const auto& corner:copied) {
        bool matched=false;
        for(unsigned i=0;i<3;++i) if(corner.position==original[i].position) matched=same(corner.skin,expected[i]);
        check(matched,"all copied strip corners follow their source vertex influences");
    }
    f=make(4,true,blend);put(f.bytes,f.copies+4+8+6,0,2);
    const auto alias=decode(f.bytes).meshes[0].corners;
    check(alias.size()==3&&same(alias.back().skin,expected[0]),"overlapping strip copy preserves snapshotted source influences");
    reject(make(3,false,{{0,0,255,2}}).bytes,"weight run must cover all source vertices");
    reject(make(3,false,{{0,0,255,0}}).bytes,"zero-length rigid run is rejected");
    reject(make(3,false,{{0,100,4,100}}).bytes,"missing continuation is rejected");
    reject(make(3,false,{{0,100,4,100},{8,54,255,0},{0,0,255,2}}).bytes,"incorrect blended total is rejected");
    reject(make(3,false,{{0,200,4,100},{8,0,255,0},{0,0,255,2}}).bytes,"oversummed blended weights are rejected");
    reject(make(3,false,{{0,100,4,100},{9,55,255,0},{0,0,255,2}}).bytes,"unaligned continuation joint is rejected");
    reject(make(3,false,{{0,100,4,100},{8,25,253,30},{0,0,255,2}}).bytes,"out-of-range fourth joint is rejected");
    reject(make(3,false,{{0,0,255,3},{0,254,255,255}}).bytes,"only the documented terminal padding is accepted");
    reject(make(3,false,{{0,0,255,3},{0,255,255,255},{0,255,255,255}}).bytes,"extra terminal padding entries are rejected");
}
void tests() {
    auto f=make();auto model=decode(f.bytes);
    check(model.meshes.size()==1&&model.meshes[0].visible_by_default&&model.part_mask==1,"parent part mask does not hide own mesh");
    const auto original=model.meshes[0].corners;
    check(original.size()==3&&original[1].position==std::array<float,3>{10,0,0}&&original[2].position==std::array<float,3>{0,10,5},"triangle positions and scale");
    check(original[0].normal==std::array<float,3>{0,0,1}&&original[2].uv_texels==std::array<float,2>{2,4},"normal and corner UV scale");
    std::fill(f.bytes.begin(),f.bytes.end(),0);check(model.meshes[0].corners[1].position[0]==10,"owned geometry survives input mutation");
    f=make();put(f.bytes,f.tag,0xc000+13,2);check(decode(f.bytes).meshes[0].corners.size()==3,"GIF words accept unsigned V4-32 without numeric reinterpretation");
    f=make();put(f.bytes,8,1,8);check(!decode(f.bytes).meshes[0].visible_by_default,"hidden-by-default mask");
    put(f.bytes,0x38,65535,2);check(decode(f.bytes).meshes[0].corners.size()==3,"record triangle hint is not authoritative");
    f=make();put(f.bytes,f.pos+4,0x8000,2);put(f.bytes,f.pos+6,0x7fff,2);
    auto signed_model=decode(f.bytes);bool extrema=false;for(const auto& c:signed_model.meshes[0].corners)extrema|=c.position[0]==-2048.0f&&c.position[1]==2047.9375f;
    check(extrema,"signed16 conversion is defined at extrema");
    f=make();for(unsigned i=0;i<3;++i)put(f.bytes,f.norm+4+i*3+2,129,1);
    const auto reversed=decode(f.bytes).meshes[0].corners;
    check(reversed[0].position==original[1].position&&reversed[0].uv_texels==original[1].uv_texels,"normal-directed winding swaps matching UV corners");
    f=make();put(f.bytes,f.slots+4+4*6,0x8006,2);check(decode(f.bytes).meshes[0].corners.empty(),"terminal nodraw suppresses triangle");
    f=make(5,true);auto copies=decode(f.bytes).meshes[0].corners;
    check(copies.size()==9&&copies[5].position==original[0].position&&copies[5].uv_texels==std::array<float,2>{3,6},"copy rows preserve per-strip UV seams");
    // Both sources are snapshotted before d2 then d1. Here d2 overwrites s1;
    // d1 must still receive the old vertex0 rather than vertex1.
    put(f.bytes,f.copies+4+8,0,2);put(f.bytes,f.copies+4+8+2,9,2);
    put(f.bytes,f.copies+4+8+4,3,2);put(f.bytes,f.copies+4+8+6,0,2);
    // Strip4 would be unfilled now: shorten to4 with a fresh synthetic packet.
    auto alias=make(4,true);put(alias.bytes,alias.copies+4+8+6,0,2);
    const auto alias_corners=decode(alias.bytes).meshes[0].corners;
    check(alias_corners.size()==3&&alias_corners.back().position==original[0].position,"copy source snapshot survives overlapping destination");
    f=make(4,true);put(f.bytes,f.slots+4+2*6,0x8000,2);
    put(f.bytes,f.slots+4+3*6+4,0x8000,2);put(f.bytes,f.slots+4+4*6+4,0x8000,2);
    put(f.bytes,f.copies+12,1,2);put(f.bytes,f.copies+14,10,2);
    check(decode(f.bytes).meshes[0].corners.size()==3,"XYZ copied through lane1 retains its nodraw bit");
    f=make(5,true);put(f.bytes,f.copies+4+8+2,0x8009,2);check(decode(f.bytes).meshes[0].corners.size()==6,"copy destination owns nodraw flag");
    f=make();for(std::size_t n=0;n<f.bytes.size();++n)reject(Buffer(f.bytes.begin(),f.bytes.begin()+n),"every truncated model prefix rejected");
    bool null_rejected=false;try{(void)champions::decodeRtaModel({nullptr,128});}catch(const champions::FormatError&){null_rejected=true;}check(null_rejected,"null input");
    reject(Buffer(16U*1024U*1024U+16,0),"input budget");
    for(unsigned count:{0U,65U,255U})mutation(0x12,count,1,"signed mesh count bound");
    mutation(0x26,0,1,"zero mesh tables unsupported");mutation(0x26,2,1,"multiple mesh tables unsupported");
    for(unsigned flag:{0x10U,0x20U,0x40U,0x80U})mutation(0x10,flag,2,"unsupported model passes/flags");
    mutation(0x14,1,4,"runtime model field");mutation(0x27,4,1,"effect segment unsupported");mutation(0x13,16,1,"attachment bound");
    mutation(0x28,0x81,4,"unaligned mesh start");mutation(0x28,0x20,4,"mesh overlaps header");mutation(0x2c,0xfffffff0,4,"mesh outside file");mutation(0x2c,0x70,4,"descending offsets");
    mutation(0x25,0,1,"missing hull");mutation(0x25,7,1,"hull exceeds its table extent");mutation(0x25,2,1,"hull overlaps mesh records");
    mutation(0x10,2,2,"variant flag mismatch");mutation(0x21,4,1,"variant offset mismatch");mutation(0x24,4,1,"aliased ancillary tables");
    mutation(0x36,64,1,"mesh joint bound");mutation(0x37,1,1,"mesh reserved byte");
    mutation(0x80,0x04000000,4,"zero vertices");mutation(0x80,0x0400002a,4,"vertex count exceeds41");mutation(0x80,0x05000001,4,"STMOD row transform unsupported");
    mutation(f.pos+3,0xe9,1,"IRQ unpack");mutation(f.pos+3,0x79,1,"masked unpack");mutation(f.pos,0,2,"absolute unpack");mutation(f.pos,0x8400,2,"reserved unpack address bits");
    mutation(f.pos+2,0,1,"num0 is256 and exceeds mesh payload");mutation(f.pos+2,4,1,"position count mismatch");mutation(f.norm,0x8004,2,"normal address mismatch");
    mutation(f.weights+4,1,1,"unaligned joint byte");mutation(f.weights+7,0,1,"empty weight run");mutation(f.weights+6,4,1,"incomplete blended weight");
    mutation(f.copies+4,1,2,"copy count mismatch");mutation(f.copies+6,1,2,"copy header reserved field");
    mutation(f.slots+4+4*6,254,2,"slot address bound");mutation(f.slots+4+4*6,0,2,"unfilled final strip slot");
    mutation(f.tag+4,0,2,"zero strip count");mutation(f.tag+4,0x8054,2,"strip exceeds83");mutation(f.tag+12,0x412,8,"GIF register list");
    mutation(f.mscal,0x14000044,4,"reflection program unsupported");mutation(f.mscal,0x14000046,4,"world program unsupported");
    mutation(f.mscal+4,0x01000104,4,"UV stride mismatch");mutation(f.uv,0xc080,2,"UV address mismatch");mutation(f.uv+2,2,1,"UV count mismatch");
    mutation(f.uv+16,0,4,"missing cycle reset");
    // A valid second mesh starts immediately after the first; a changed UNPACK
    // count may not borrow bytes from that next mesh.
    f=make();const auto mesh_end=f.bytes.size();f.bytes.resize(mesh_end+256,0);put(f.bytes,f.pos+2,80,1);reject(f.bytes,"UNPACK bounded by mesh end, not whole file");
    //64 meshes exercise visibility bit63 without shifting by64. Empty aligned
    // mesh streams are permitted and consume no output budget.
    Buffer many(0x9b0,0);put(many,0x12,64,1);put(many,0x13,255,1);put(many,0x26,1,1);put(many,0x25,0x3b,1);put(many,8,1ULL<<63,8);
    for(unsigned i=0;i<=64;++i)put(many,0x28+i*4,many.size(),4);
    auto sixtyfour=decode(many);check(sixtyfour.meshes.size()==64&&!sixtyfour.meshes[63].visible_by_default&&sixtyfour.meshes[62].visible_by_default,"64th visibility bit");
    f=make(83);const Buffer packet(f.bytes.begin()+0x80,f.bytes.end());Buffer large(f.bytes.begin(),f.bytes.begin()+0x80);
    for(unsigned i=0;i<2470;++i)large.insert(large.end(),packet.begin(),packet.end());put(large,0x2c,large.size(),4);reject(large,"global triangle output budget");
    f=make(1);const Buffer tiny(f.bytes.begin()+0x80,f.bytes.end());large.assign(f.bytes.begin(),f.bytes.begin()+0x80);
    for(unsigned i=0;i<32769;++i)large.insert(large.end(),tiny.begin(),tiny.end());put(large,0x2c,large.size(),4);reject(large,"global packet budget");
}
void write(const fs::path& path,const Buffer& b) {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!f)throw std::runtime_error("cannot write authored model fixture");}
int run(const std::vector<fs::path>& args) {
    try {
        if(args.size()==2&&args[0]=="--write-fixtures") {fs::create_directories(args[1]);write(args[1]/"triangle42.vif",make().bytes);write(args[1]/"copied-strip42.vif",make(5,true).bytes);return 0;}
        if(!args.empty())throw std::runtime_error("unexpected test arguments");tests();variantTests();skinTests();std::cout<<checks<<" model checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<fs::path> args;for(int i=1;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#else
int main(int argc,char** argv){std::vector<fs::path> args;for(int i=1;i<argc;++i)args.push_back(fs::u8path(argv[i]));return run(args);}
#endif
