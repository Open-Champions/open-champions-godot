#include "champions/animation.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Buffer = std::vector<std::uint8_t>;
unsigned checks = 0;
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
void put(Buffer& data, std::size_t at, std::uint64_t value, unsigned size) {
    for (unsigned i=0;i<size;++i) data.at(at+i)=static_cast<std::uint8_t>(value>>(i*8));
}
struct Fixture { Buffer bytes; std::size_t bind,keys; };
Fixture make(const std::vector<std::uint8_t>& depths, bool sound_pointers=false, unsigned gap=0) {
    const auto bind=(0x70+depths.size()+3)&~std::size_t(3);
    const auto keys=bind+depths.size()*8+gap;
    Fixture f{Buffer(keys+4+(sound_pointers?8:0),0),bind,keys};
    put(f.bytes,0,depths.size(),4);put(f.bytes,4,120,4);
    put(f.bytes,8,keys,4);put(f.bytes,12,bind,4);put(f.bytes,16,0x70,4);
    std::copy(depths.begin(),depths.end(),f.bytes.begin()+0x70);
    for(std::size_t i=0;i<depths.size();++i) {
        put(f.bytes,bind+i*8,i,2);put(f.bytes,bind+i*8+2,65535-i,2);put(f.bytes,bind+i*8+4,64,2);
    }
    // Opaque authored key bytes: metadata success intentionally does not certify keys.
    put(f.bytes,keys,0xdecafbad,4);
    if(sound_pointers) {put(f.bytes,0x34,keys+4,4);put(f.bytes,0x38,keys+8,4);}
    return f;
}
champions::RtaAnimationSkeleton read(const Buffer& data) {
    return champions::readRtaAnimationSkeleton({data.data(),data.size()});
}
void reject(const Buffer& data,const char* message) {
    bool rejected=false;try{(void)read(data);}catch(const champions::FormatError&){rejected=true;}
    check(rejected,message);
}
void tests() {
    auto f=make({0,1,2,1,2,0,1,0},true);
    put(f.bytes,0x48,0x8000000000000005ULL,8);put(f.bytes,4,0xffffffffU,4);
    auto skeleton=read(f.bytes);
    check(skeleton.depths==std::vector<std::uint8_t>({0,1,2,1,2,0,1,0}),"depths preserve exact source order");
    check(skeleton.parents==std::vector<std::int16_t>({-1,0,1,0,3,-1,5,-1}),"parents follow the nearest ancestor and reset across multiple roots");
    check(skeleton.duration_ticks==0xffffffffU&&skeleton.joint_mask==0x8000000000000005ULL,"full-width duration and mask are preserved without signed narrowing");
    check(skeleton.bind_offsets[3]==std::array<std::int16_t,3>{3,-4,64},"bind offsets preserve exact signed units without float scaling");
    std::fill(f.bytes.begin(),f.bytes.end(),0);
    check(skeleton.depths[4]==2&&skeleton.parents[6]==5&&skeleton.bind_offsets[3][1]==-4,"returned skeleton owns storage after input mutation");
    f=make({0});put(f.bytes,f.bind,0x8000,2);put(f.bytes,f.bind+2,0x7fff,2);put(f.bytes,f.bind+4,0xffff,2);
    check(read(f.bytes).bind_offsets[0]==std::array<std::int16_t,3>{-32768,32767,-1},"signed16 conversion is exact at both extrema and negative one");
    f=make({});auto empty=read(f.bytes);
    check(empty.depths.empty()&&empty.parents.empty()&&empty.bind_offsets.empty(),"zero-joint metadata permits coincident empty spans and key start");
    put(f.bytes,4,0,4);check(read(f.bytes).duration_ticks==0,"zero duration remains metadata, not a playback policy");
    std::vector<std::uint8_t> chain(64);for(unsigned i=0;i<64;++i)chain[i]=static_cast<std::uint8_t>(i);
    skeleton=read(make(chain).bytes);
    check(skeleton.parents.size()==64&&skeleton.parents[63]==62&&skeleton.depths[63]==63,"64-joint chain supports structural depth beyond observed sample maxima");
    skeleton=read(make(std::vector<std::uint8_t>(64,0)).bytes);
    check(std::all_of(skeleton.parents.begin(),skeleton.parents.end(),[](auto p){return p==-1;}),"64 independent roots are supported");
    f=make({0},false,7);check(read(f.bytes).depths.size()==1,"gaps before keys do not imply unknown padding semantics");
    f=make({0});
    for(auto at:{0x14U,0x3cU,0x3dU,0x3eU,0x40U,0x44U,0x50U,0x6fU})f.bytes[at]=255;
    check(read(f.bytes).parents[0]==-1,"unconsumed event sound and unknown header fields are not guessed or parsed");
    f=make({0},true);put(f.bytes,f.keys+4,0xffffffffU,4);put(f.bytes,f.keys+8,0xffffffffU,4);
    check(read(f.bytes).parents[0]==-1,"sound pointer bounds do not claim validation of sound contents");
    // Header pointers are checked independently; partial optional metadata does not
    // imply any sound-table pairing or sound semantic validation in this reader.
    put(f.bytes,0x34,0,4);check(read(f.bytes).parents[0]==-1,"a sole bounded event pointer is metadata only");
    put(f.bytes,0x34,f.keys+4,4);put(f.bytes,0x38,0,4);check(read(f.bytes).parents[0]==-1,"a sole bounded name pointer is metadata only");
    f=make({0,1,1});
    for(std::size_t size=0;size<f.bytes.size();++size)
        reject(Buffer(f.bytes.begin(),f.bytes.begin()+size),"every truncated required metadata or key prefix is rejected");
    bool failed=false;
    try{(void)champions::readRtaAnimationSkeleton({nullptr,0x70});}catch(const champions::FormatError&){failed=true;}
    check(failed,"null source is rejected");
    failed=false;
    try{(void)champions::readRtaAnimationSkeleton({f.bytes.data(),16U*1024U*1024U+1});}catch(const champions::FormatError&){failed=true;}
    check(failed,"oversized input is rejected before accessing its buffer");
    auto bad=f.bytes;put(bad,0,65,4);reject(bad,"more than64 joints is rejected");
    bad=f.bytes;put(bad,0,0xffffffffU,4);reject(bad,"overflow-shaped joint count is rejected");
    bad=f.bytes;put(bad,16,0x71,4);reject(bad,"unsupported displaced depth list is rejected");
    bad=f.bytes;put(bad,16,0x100070,4);reject(bad,"relocated depth pointer is rejected");
    for(auto offset:{0U,0x6fU,0xffffffffU}) {
        bad=f.bytes;put(bad,12,offset,4);reject(bad,"invalid bind pointer is rejected");
        bad=f.bytes;put(bad,8,offset,4);reject(bad,"invalid key pointer is rejected");
    }
    bad=f.bytes;put(bad,12,0x72,4);reject(bad,"bind records cannot overlap depth entries");
    bad=f.bytes;put(bad,12,f.bytes.size()-8,4);reject(bad,"bind records must fit entirely");
    bad=f.bytes;put(bad,8,f.bind+23,4);reject(bad,"key prefix cannot overlap bind records");
    bad=f.bytes;put(bad,8,f.bytes.size()-3,4);reject(bad,"key prefix needs four bounded bytes");
    bad=f.bytes;put(bad,f.bind+6,1,2);reject(bad,"nonzero bind record padding is outside supported subset");
    reject(make({1}).bytes,"first joint must be a root");
    reject(make({0,2}).bytes,"depth cannot skip a parent");
    reject(make({0,1,2,0,2}).bytes,"new root cannot inherit a stale earlier branch parent");
    reject(make({0,255}).bytes,"depth255 is rejected before stack indexing");
    chain.back()=64;reject(make(chain).bytes,"depth64 exceeds joint stack");
    f=make({0},true);
    for(auto field:{0x34U,0x38U})for(auto offset:{0x20U,unsigned(f.bind),unsigned(f.keys),unsigned(f.bytes.size()-3),0xffffffffU}) {
        bad=f.bytes;put(bad,field,offset,4);reject(bad,"optional sound pointer must name four bytes after key prefix");
    }
    bad=f.bytes;put(bad,0x38,f.keys+4,4);reject(bad,"sound event pointer cannot alias name prefix");
    bad=f.bytes;put(bad,0x34,f.keys+8,4);put(bad,0x38,f.keys+4,4);reject(bad,"sound pointers must retain documented order");
}
} // namespace
int main() {
    try{tests();std::cout<<checks<<" animation skeleton checks passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
