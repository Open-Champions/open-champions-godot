#include "champions/texture_upscale.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace champions;
#define check(v) do { if(!(v)) throw std::runtime_error("pixel expectation failed at line " + std::to_string(__LINE__)); } while(false)
int main() {
 try {
    for(unsigned f: {2u,3u,4u}) for(auto mode:{TextureFilter::Nearest,TextureFilter::PixelArt,TextureFilter::Bicubic}) {
        Texture32 s{1,1,7,{31,85,173,127}}; auto o=upscaleTexture(s,f,mode);
        check(o.width==f && o.height==f && o.flags==7);
        for(std::size_t i=0;i<o.rgba.size();++i) check(o.rgba[i]==s.rgba[i%4]);
    }
    Texture32 s{3,3,0,{}};
    // B and D red, all others black. Literal expected center neighborhoods.
    for(unsigned i=0;i<9;++i) s.rgba.insert(s.rgba.end(),{std::uint8_t(i==1||i==3?255:0),0,0,255});
    for(unsigned f:{2u,3u}) {
        auto o=upscaleTexture(s,f,TextureFilter::PixelArt);
        for(unsigned y=0;y<f;++y) for(unsigned x=0;x<f;++x)
            check(o.rgba[((y+f)*o.width+x+f)*4]==(x==0&&y==0?255:0));
    }
    Texture32 pattern{3,3,0,{}};
    const unsigned colors[]{0,1,3,1,2,4,3,4,0};
    for(auto color:colors) pattern.rgba.insert(pattern.rgba.end(), {std::uint8_t(color*50),0,0,255});
    const unsigned center2[]{1,2,2,4}, center3[]{1,1,2,1,2,4,2,4,4};
    for(unsigned f:{2u,3u}) {
        auto o=upscaleTexture(pattern,f,TextureFilter::PixelArt);
        for(unsigned y=0;y<f;++y) for(unsigned x=0;x<f;++x) {
            const auto offset=((y+f)*o.width+x+f)*4;
            check(o.rgba[offset]==50*(f==2?center2[y*f+x]:center3[y*f+x]));
            check(o.rgba[offset+1]==0 && o.rgba[offset+2]==0 && o.rgba[offset+3]==255);
        }
    }
    auto twice=upscaleTexture(upscaleTexture(s,2,TextureFilter::PixelArt),2,TextureFilter::PixelArt);
    check(twice.rgba==upscaleTexture(s,4,TextureFilter::PixelArt).rgba);
    for(unsigned f:{2u,3u,4u}) {
        auto o=upscaleTexture(s,f,TextureFilter::Nearest);
        for(unsigned y=0;y<o.height;++y) for(unsigned x=0;x<o.width;++x) for(unsigned ch=0;ch<4;++ch)
            check(o.rgba[(y*o.width+x)*4+ch]==s.rgba[((y/f)*3+x/f)*4+ch]);
    }
    Texture32 alpha{2,1,0,{255,0,0,255,0,0,255,0}};
    const std::vector<std::vector<unsigned>> expected{{255,203,52,0},{255,255,179,76,0,0},{255,255,234,167,88,21,0,0}};
    for(unsigned f:{2u,3u,4u}) {
        auto o=upscaleTexture(alpha,f,TextureFilter::Bicubic);
        for(unsigned y=0;y<o.height;++y) for(unsigned x=0;x<o.width;++x) {
            auto i=(y*o.width+x)*4; auto a=expected[f-2][x];
            check(o.rgba[i+3]==a); check(o.rgba[i]==(a?255:0));check(o.rgba[i+1]==0 && o.rgba[i+2]==0);
        }
        alpha.rgba={0,0,255,0,255,0,0,255};
        auto reversed=upscaleTexture(alpha,f,TextureFilter::Bicubic);
        for(unsigned x=0;x<o.width;++x) for(unsigned ch=0;ch<4;++ch)
            check(reversed.rgba[x*4+ch]==o.rgba[(o.width-1-x)*4+ch]);
        alpha.rgba={255,0,0,255,0,0,255,0};
    }
    for(auto invalid:{Texture32{0,1,0,{}},Texture32{1,1,0,{}},Texture32{1025,1,0,{}},Texture32{std::numeric_limits<unsigned>::max(),1,0,{}}}) {
        bool refused=false;try{upscaleTexture(invalid,4,TextureFilter::Nearest);}catch(const FormatError&){refused=true;}check(refused);
    }
    for(unsigned f:{0u,1u,5u,std::numeric_limits<unsigned>::max()}) {
        bool refused=false;try{upscaleTexture(alpha,f,TextureFilter::Nearest);}catch(const FormatError&){refused=true;}check(refused);
    }
    bool refused=false;try{upscaleTexture(alpha,2,static_cast<TextureFilter>(99));}catch(const FormatError&){refused=true;}check(refused);
    Texture32 transparent{1,1,0,{99,88,77,0}};
    check(upscaleTexture(transparent,2,TextureFilter::Bicubic).rgba==std::vector<std::uint8_t>(16,0));
    std::cout<<"texture upscale exact pixels PASS\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
