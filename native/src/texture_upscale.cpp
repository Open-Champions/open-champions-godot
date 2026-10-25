#include "champions/texture_upscale.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
namespace champions {
namespace {
using Pixel = std::array<std::uint8_t, 4>;
Pixel sample(const Texture32 &s, int x, int y) {
    x = std::clamp(x, 0, int(s.width)-1);
    y = std::clamp(y, 0, int(s.height)-1);
    Pixel p{};
    std::copy_n(s.rgba.data() + (std::size_t(y)*s.width+x)*4, 4, p.begin());
    return p;
}
void store(Texture32 &s, unsigned x, unsigned y, Pixel p) {
    std::copy(p.begin(), p.end(), s.rgba.begin()+(std::size_t(y)*s.width+x)*4);
}
Texture32 allocate(const Texture32 &s, unsigned f) {
    const std::uint64_t w = std::uint64_t(s.width)*f, h = std::uint64_t(s.height)*f;
    if (!w || !h || w>4096 || h>4096 || w*h>16777216)
        throw FormatError("Upscaled texture exceeds 4096 pixels per axis or 64 MiB.");
    return {unsigned(w), unsigned(h), s.flags, std::vector<std::uint8_t>(std::size_t(w*h*4))};
}
// Authored from the published Scale2x/Scale3x neighborhood specification,
// https://www.scale2x.it/algorithm . No upstream implementation is included.
Texture32 edge(const Texture32 &s, unsigned f) {
    auto out = allocate(s,f);
    for (unsigned y=0;y<s.height;++y) for(unsigned x=0;x<s.width;++x) {
        std::array<Pixel,9> n{};
        for(int j=0;j<3;++j) for(int i=0;i<3;++i) n[j*3+i]=sample(s,int(x)+i-1,int(y)+j-1);
        const auto &a=n[0], &b=n[1], &c=n[2], &d=n[3], &e=n[4], &r=n[5], &g=n[6], &h=n[7], &i=n[8];
        std::array<Pixel,9> block; block.fill(e);
        if(b!=h && d!=r) {
            const bool tl=d==b, tr=b==r, bl=d==h, br=h==r;
            block[0]=tl?d:e;
            block[f-1]=tr?r:e;
            block[(f-1)*f]=bl?d:e;
            block[f*f-1]=br?r:e;
            if(f==3) {
                if((tl && e!=c)||(tr && e!=a)) block[1]=b;
                if((tl && e!=g)||(bl && e!=a)) block[3]=d;
                if((tr && e!=i)||(br && e!=c)) block[5]=r;
                if((bl && e!=i)||(br && e!=g)) block[7]=h;
            }
        }
        for(unsigned j=0;j<f;++j) for(unsigned k=0;k<f;++k) store(out,x*f+k,y*f+j,block[j*f+k]);
    }
    return out;
}
struct Axis { int base; std::array<std::int64_t,4> weights; };
Axis axis(unsigned u, unsigned f) {
    const int den=int(2*f), numerator=int(2*u+1)-int(f);
    const int base=numerator>=0?numerator/den:-((-numerator+den-1)/den);
    const std::int64_t t=numerator-base*den, d=den, t2=t*t, t3=t2*t;
    return {base, {-t*d*d+2*t2*d-t3, 2*d*d*d-5*t2*d+3*t3,
                   t*d*d+4*t2*d-3*t3, -t2*d+t3}};
}
std::uint8_t rounded(std::int64_t n,std::int64_t d) {
    return std::uint8_t(std::clamp<std::int64_t>((n+d/2)/d,0,255));
}
}
static Texture32 upscaleUnconstrained(const Texture32 &s,unsigned f,TextureFilter filter) {
    if(f<2 || f>4) throw FormatError("Upscaling factor must be 2, 3 or 4.");
    if(!s.width || !s.height || s.width>1024 || s.height>1024 ||
       s.rgba.size()!=std::uint64_t(s.width)*s.height*4) throw FormatError("Invalid bounded RGBA8 input.");
    if(filter==TextureFilter::PixelArt) return f==4?edge(edge(s,2),2):edge(s,f);
    if(filter!=TextureFilter::Nearest && filter!=TextureFilter::Bicubic) throw FormatError("Unknown texture filter.");
    auto out=allocate(s,f);
    const std::int64_t d=2*f, denominator=2*d*d*d, norm=denominator*denominator;
    for(unsigned y=0;y<out.height;++y) for(unsigned x=0;x<out.width;++x) {
        if(filter==TextureFilter::Nearest) {store(out,x,y,sample(s,x/f,y/f));continue;}
        const auto ax=axis(x,f), ay=axis(y,f);
        std::int64_t alpha=0; std::array<std::int64_t,3> color{};
        for(int j=0;j<4;++j) for(int k=0;k<4;++k) {
            const auto p=sample(s,ax.base+k-1,ay.base+j-1);
            const auto weight=ax.weights[k]*ay.weights[j];
            alpha+=weight*p[3];
            for(int ch=0;ch<3;++ch) color[ch]+=weight*p[3]*p[ch];
        }
        Pixel p{};
        p[3]=rounded(std::clamp<std::int64_t>(alpha,0,255*norm),norm);
        if(alpha>0 && p[3]) for(int ch=0;ch<3;++ch)
            p[ch]=rounded(std::clamp<std::int64_t>(color[ch],0,255*alpha),alpha);
        store(out,x,y,p);
    }
    return out;
}
namespace {
bool flat(const Texture32 &s, unsigned x, unsigned y) {
    const auto p = sample(s,x,y);
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
        if(sample(s,int(x)+dx,int(y)+dy)!=p) return false;
    return true;
}
void validateSource(const Texture32 &s) {
    if(!s.width || !s.height || s.width>1024 || s.height>1024 ||
       s.rgba.size()!=std::uint64_t(s.width)*s.height*4)
        throw FormatError("Invalid bounded fidelity source.");
}
}
Texture32 upscaleTexture(const Texture32 &s,unsigned f,TextureFilter filter) {
    auto out = upscaleUnconstrained(s,f,filter);
    // Original uniform colors (including hidden transparent RGB) and hard alpha
    // masks take precedence over interpolation. No invented material semantics.
    for(unsigned y=0;y<s.height;++y) for(unsigned x=0;x<s.width;++x) {
        const auto p=sample(s,x,y);
        const bool uniform=flat(s,x,y);
        for(unsigned j=0;j<f;++j) for(unsigned i=0;i<f;++i) {
            if(uniform || p[3]==0) store(out,x*f+i,y*f+j,p);
            else if(p[3]==255) out.rgba[((std::size_t(y*f+j)*out.width)+x*f+i)*4+3]=255;
        }
    }
    return out;
}
TextureFidelity measureTextureFidelity(const Texture32 &s,const Texture32 &o,unsigned f) {
    validateSource(s);
    if(f<2 || f>4) throw FormatError("Fidelity factor must be 2, 3 or 4.");
    TextureFidelity m;
    m.error_denominator=f*f;
    if(o.width!=std::uint64_t(s.width)*f || o.height!=std::uint64_t(s.height)*f ||
       o.rgba.size()!=std::uint64_t(o.width)*o.height*4) return m;
    m.aspect_preserved=true;
    std::set<Pixel> palette;
    for(unsigned y=0;y<s.height;++y) for(unsigned x=0;x<s.width;++x) palette.insert(sample(s,x,y));
    for(unsigned y=0;y<s.height;++y) for(unsigned x=0;x<s.width;++x) {
        const auto p=sample(s,x,y);
        const bool uniform=flat(s,x,y);
        std::array<std::uint64_t,4> sum{};
        for(unsigned j=0;j<f;++j) for(unsigned i=0;i<f;++i) {
            const auto q=sample(o,x*f+i,y*f+j);
            m.new_palette_pixels+=palette.count(q)==0;
            m.flat_region_changes+=uniform && q!=p;
            m.hard_alpha_changes+=(p[3]==0 || p[3]==255) && q[3]!=p[3];
            for(unsigned ch=0;ch<4;++ch) sum[ch]+=q[ch];
        }
        for(unsigned ch=0;ch<4;++ch) {
            const auto expected=std::uint64_t(p[ch])*f*f;
            const auto error=sum[ch]>expected?sum[ch]-expected:expected-sum[ch];
            m.error_numerator+=error;
            m.max_error_numerator=std::max(m.max_error_numerator,error);
        }
    }
    return m;
}

}
