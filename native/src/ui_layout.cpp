#include "champions/ui_layout.hpp"
#include "champions/ui_font.hpp"
#include "champions/ui_font_page.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace champions::ui_layout {
namespace {
int checked(long long value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw FormatError("UI layout coordinate overflow");
    return static_cast<int>(value);
}
Clip clip(int l, int t, int r, int b) { return {l,t,r,b}; }
std::uint32_t raw(float f) { std::uint32_t u; std::memcpy(&u,&f,sizeof u); return u; }
struct Bits { int sign{}, exponent{}; std::uint64_t mantissa{}; };
Bits unpack(std::uint32_t u) {
    const int e = (u >> 23) & 255;
    return {int(u >> 31), e, e ? std::uint64_t((u & 0x7fffff) | 0x800000) : 0};
}
int bitWidth(std::uint64_t v) { int n=0; while (v) { ++n; v >>= 1; } return n; }
std::uint32_t pack(int sign, std::uint64_t mag, int e2) {
    if (!mag) return std::uint32_t(sign) << 31;
    const int shift = bitWidth(mag)-24;
    const std::uint64_t m = shift > 0 ? mag >> shift : mag << -shift;
    const int e = e2 + shift + 150;
    if (e > 255) return (std::uint32_t(sign)<<31) | 0x7fffffff;
    if (e < 1) return std::uint32_t(sign)<<31;
    return (std::uint32_t(sign)<<31) | (std::uint32_t(e)<<23) | (std::uint32_t(m)&0x7fffff);
}
std::uint32_t ps2Divide(std::uint32_t a, std::uint32_t b) {
    const auto x=unpack(a), y=unpack(b); const int sign=x.sign^y.sign;
    if (!y.mantissa) return (std::uint32_t(sign)<<31)|0x7fffffff;
    if (!x.mantissa) return std::uint32_t(sign)<<31;
    const std::uint64_t numerator=x.mantissa<<40;
    const auto q=numerator/y.mantissa, rem=numerator%y.mantissa;
    return pack(sign,(q<<1)|(rem?1:0),x.exponent-y.exponent-41);
}
std::uint32_t ps2Multiply(std::uint32_t a, std::uint32_t b) {
    const auto x=unpack(a), y=unpack(b);
    return pack(x.sign^y.sign,x.mantissa*y.mantissa,x.exponent+y.exponent-300);
}
std::uint32_t ps2Add(std::uint32_t a, std::uint32_t b) {
    auto x=unpack(a), y=unpack(b);
    if (!x.mantissa) return b; if (!y.mantissa) return a;
    if (x.exponent < y.exponent) { std::swap(x,y); std::swap(a,b); }
    const int diff=x.exponent-y.exponent;
    if (diff>=25) return a;
    const std::int64_t big=x.sign ? -std::int64_t(x.mantissa) : std::int64_t(x.mantissa);
    const std::int64_t small=y.sign ? -std::int64_t(y.mantissa) : std::int64_t(y.mantissa);
    const auto divisor=std::int64_t(1)<<diff;
    const auto shifted=small>=0 ? (small*64)/divisor : -(((-small*64)+divisor-1)/divisor);
    const auto total=(big*64)+shifted;
    if (!total) return 0;
    return pack(total<0,static_cast<std::uint64_t>(total<0?-total:total),x.exponent-156);
}
int ps2Int(std::uint32_t u) {
    const auto b=unpack(u); if (!b.mantissa) return 0;
    if (b.exponent>=158) return b.sign ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    const int shift=b.exponent-150;
    const auto mag=shift>=0 ? b.mantissa<<shift : shift<=-64 ? 0 : b.mantissa>>-shift;
    return b.sign ? -static_cast<int>(mag) : static_cast<int>(mag);
}
std::string prefix(int style) { return style==2 ? "tile" : style==1 ? "btile" : "ctile"; }
int signed16(Bytes b, std::size_t o) {
    const auto u=unsigned(b.data[o])|(unsigned(b.data[o+1])<<8);
    return u<32768 ? int(u) : int(u)-65536;
}
}

int frameBorder(int style) { return style==2 || style==1 ? 32 : 16; }
Rect frameSourceRect(const std::string &resource) {
    for (const auto &family : {std::string("btile"),std::string("ctile"),std::string("tile")}) {
        if (resource.size()<family.size()+5 || resource.compare(0,family.size(),family)!=0 ||
            resource.compare(resource.size()-4,4,".tex")!=0) continue;
        const auto part=resource.substr(family.size(),resource.size()-family.size()-4);
        if (part=="center") return {0,0,family=="tile"?320:family=="ctile"?128:64,family=="tile"?224:64};
        for (const char *p : {"top","bot","left","right","topleft","topright","botleft","botright"})
            if (part==p) { const int n=family=="ctile"?16:32; return {0,0,n,n}; }
    }
    throw FormatError("not a documented frame resource");
}
Rect hudSourceRect(const std::string &resource) {
    if (resource=="hud_multi.tex") return {0,0,158,46};
    if (resource=="hud_statusbar00.tex" || resource=="hud_statusbar00glow.tex") return {0,0,160,46};
    for (const auto &color : {std::string("red"),std::string("green"),std::string("blue")}) {
        const std::string stem="bar_"+color;
        if (resource.size()!=stem.size()+6 || resource.compare(0,stem.size(),stem)!=0 ||
            resource.compare(resource.size()-4,4,".tex")!=0) continue;
        const auto a=resource[stem.size()],b=resource[stem.size()+1];
        if (a<'0'||a>'9'||b<'0'||b>'9') break;
        const int frame=(a-'0')*10+(b-'0');
        if (frame<=10) return {0,0,100,color=="green"?6:8};
    }
    throw FormatError("not a documented HUD resource");
}
std::vector<Draw> menuFrame(int x,int y,int width,int height,int style,bool no_center,bool no_border) {
    // Bounds keep the exact loops finite without clamping small original frames.
    if (width<0 || height<0 || width>4096 || height>4096) throw FormatError("frame dimensions outside supported domain");
    const int n=frameBorder(style), right=checked(static_cast<long long>(x)+width),bottom=checked(static_cast<long long>(y)+height);
    const auto family=prefix(style); const int cw=style==2?320:style==1?64:128,ch=style==2?224:64;
    std::vector<Draw> out;
    auto emit=[&](const char *part,int dx,int dy,bool clipped=false,Clip c={}) {
        out.push_back({family+part+".tex",dx,dy,clipped,c});
    };
    if (!no_border) {
        emit("botleft",x,checked(static_cast<long long>(bottom)-n-1));
        emit("botright",checked(static_cast<long long>(right)-n),checked(static_cast<long long>(bottom)-n-1));
        emit("topleft",x,checked(static_cast<long long>(y)-1));
        emit("topright",checked(static_cast<long long>(right)-n),checked(static_cast<long long>(y)-1));
        for (int d=0;d<width-2*n;d+=n) {
            emit("top",checked(static_cast<long long>(x)+n+d),y,true,clip(x,y,checked(static_cast<long long>(right)-n),checked(static_cast<long long>(bottom)-n)));
            emit("bot",checked(static_cast<long long>(x)+n+d),checked(static_cast<long long>(bottom)-n),true,
                 clip(x,y,checked(static_cast<long long>(right)-n+(style==2?4:0)),bottom));
        }
        for (int d=0;d<height-2*n;d+=n) {
            auto c=clip(x,y,right,checked(static_cast<long long>(bottom)-n));
            emit("left",x,checked(static_cast<long long>(y)+n+d),true,c);
            emit("right",checked(static_cast<long long>(right)-n),checked(static_cast<long long>(y)+n+d),true,c);
        }
    }
    if (!no_center) {
        const int hlim=height-2*n+(style==2?51:0),wlim=width-2*n+(style==2?52:0);
        for (int dy=0;dy<hlim;dy+=ch) for (int dx=0;dx<wlim;dx+=cw) {
            const int offset=style==2?26:0;
            emit("center",checked(static_cast<long long>(x)+n+(style==2?-25:0)+dx),
                 checked(static_cast<long long>(y)+n+(style==2?-26:0)+dy),true,
                 clip(x,y,checked(static_cast<long long>(right)-n+offset),checked(static_cast<long long>(bottom)-n+offset)));
        }
    }
    return out;
}
std::array<int,4> hudOrigins(bool network,const std::array<bool,4>&local_slots,int player_count) {
    if (player_count<0 || player_count>4) throw FormatError("HUD player count outside supported slots");
    std::array<int,4> values{12,478,167,323}; int previous=0;
    if (network) {
        for (int slot=0;slot<player_count;++slot) if (slot && local_slots[slot]) {
            previous=values[slot]; values[slot]=values[0];
        }
        if (!local_slots[0]) values[0]=previous;
    }
    return values;
}
int barClipRight(int origin,float current,float maximum) {
    if (origin<0 || !std::isfinite(current) || !std::isfinite(maximum) || current<0 || maximum<=0)
        throw FormatError("outside supported bar domain");
    const auto fraction=ps2Divide(raw(current),raw(maximum));
    const auto scaled=ps2Multiply(fraction,raw(99.f));
    return ps2Int(ps2Add(raw(static_cast<float>(origin)),scaled));
}
std::vector<HudLayer> hudBaseLayers(int x,std::uint32_t tick,float health,float health_max,float mana,float mana_max,
                                    int experience,int experience_base,int experience_next,int acknowledged_level,
                                    bool local,int cooldown,bool health_flash) {
    const int frame=(tick>>2)%11,start=checked(static_cast<long long>(x)+15);
    std::vector<HudLayer> out;
    auto bar=[&](const char *color,int y,float value,float maximum,std::uint32_t tint) {
        const auto number=frame<10?"0"+std::to_string(frame):std::to_string(frame);
        out.push_back({{std::string("bar_")+color+number+".tex",start,y,true,
                       clip(start,24,barClipRight(start,value,maximum),124)},tint,6});
    };
    bar("red",46,health,health_max,health_flash?0x80ffffff:0x80808080);
    if (local) {
        if (acknowledged_level>0) bar("green",56,float(checked(static_cast<long long>(experience)-experience_base)),
                                       float(checked(static_cast<long long>(experience_next)-experience_base)),0x80808080);
        bar("blue",63,mana,mana_max,cooldown>0?0x2aacacac:0x80acacac);
    }
    out.push_back({{local?"hud_statusbar00.tex":"hud_multi.tex",checked(static_cast<long long>(x)-4),36,true,clip(0,0,1000,1000)},0x80808080,6});
    return out;
}
// a = trunc((C/M)*99); t = trunc(C+pending), lowered to trunc(M) when
// float(t) > M; b = trunc((float(t)/M)*99). The integer conversion comes
// before the ratio, unlike the bar fill. Returns min(S+b, cap); S+a is the left.
int pendingRight(int origin,int cap,float current,float pending,float maximum) {
    if (origin<0 || !std::isfinite(current) || !std::isfinite(pending) || !std::isfinite(maximum) ||
        current<0 || pending<=0 || maximum<=0 || current+pending>=16777216.f)
        throw FormatError("outside supported pending-overlay domain");
    int total=ps2Int(ps2Add(raw(current),raw(pending)));
    if (static_cast<float>(total)>maximum) total=ps2Int(raw(maximum));
    const int b=ps2Int(ps2Multiply(ps2Divide(raw(static_cast<float>(total)),raw(maximum)),raw(99.f)));
    return std::min(checked(static_cast<long long>(origin)+b),cap);
}
std::vector<HudRect> hudPendingRects(int x,int slot,float health,float health_max,float pending_health,
                                     float mana,float mana_max,float pending_mana) {
    if (slot<0 || slot>3) throw FormatError("HUD slot outside supported slots");
    static constexpr std::array<int,4> caps{180,605,333,447}; // by original slot, 0x1da050-0x1da0ac
    const int start=checked(static_cast<long long>(x)+15);
    std::vector<HudRect> out;
    auto rect=[&](float current,float maximum,float pending,int top,int bottom,std::uint32_t colour) {
        if (!(pending>0)) return;
        if (!std::isfinite(current) || !std::isfinite(maximum) || current<0 || maximum<=0)
            throw FormatError("outside supported pending-overlay domain");
        const int a=ps2Int(ps2Multiply(ps2Divide(raw(current),raw(maximum)),raw(99.f)));
        out.push_back({checked(static_cast<long long>(start)+a),top,
                       pendingRight(start,caps[std::size_t(slot)],current,pending,maximum),bottom,colour,6});
    };
    rect(health,health_max,pending_health,47,51,0x80000060);
    rect(mana,mana_max,pending_mana,66,70,0x805a4632);
    return out;
}
FontMetrics fontMetrics(Bytes b) {
    if (!b.data || b.size<0x214) throw FormatError("FNT layout requires 0x214-byte prefix");
    const auto existing=readRtaUiFont(b);
    const auto n=existing.glyphs.size();
    const auto nk=unsigned(b.data[4])|(unsigned(b.data[5])<<8)|(unsigned(b.data[6])<<16)|(unsigned(b.data[7])<<24);
    if (!n || n>=0x8000 || existing.glyph_offset<0x214 ||
        existing.glyph_offset>b.size || n>(b.size-existing.glyph_offset)/16 ||
        existing.kerning_offset<existing.glyph_offset+n*16 || existing.kerning_offset>b.size ||
        nk>(b.size-existing.kerning_offset)/6 || existing.texture_offset<std::size_t(existing.kerning_offset)+std::size_t(nk)*6 ||
        existing.texture_offset>b.size || b.size-existing.texture_offset<0x40)
        throw FormatError("invalid FNT layout table spans");
    FontMetrics out;
    for (std::size_t i=0;i<256;++i) {
        out.charmap[i]=signed16(b,0x14+i*2);
        if (out.charmap[i]>=static_cast<int>(n)) throw FormatError("invalid FNT character map");
    }
    out.glyphs.reserve(n);
    for (std::size_t i=0;i<n;++i) {
        const auto o=std::size_t(existing.glyph_offset)+i*16;
        const auto &g=existing.glyphs[i]; const int start=signed16(b,o+14);
        if (start>=static_cast<int>(nk)) throw FormatError("invalid FNT kerning start");
        out.glyphs.push_back({std::uint16_t(g[0]),signed16(b,o+2),signed16(b,o+4),signed16(b,o+6),
                              signed16(b,o+8),signed16(b,o+10),signed16(b,o+12),start});
    }
    for (std::size_t i=0;i<nk;++i) {
        const auto o=std::size_t(existing.kerning_offset)+i*6;
        const auto u16=[&](std::size_t at){return std::uint16_t(unsigned(b.data[at])|(unsigned(b.data[at+1])<<8));};
        out.kerns.push_back({u16(o),u16(o+2),signed16(b,o+4)});
    }
    return out;
}
DecodedFontPage decodeFontFile(Bytes bytes, bool weave) {
    const auto font = fontMetrics(bytes);
    (void)font;
    const auto suffix = readRtaUiFont(bytes);
    return decodeFontPage({suffix.texture.data(), suffix.texture.size()}, weave);
}
std::array<HudArt, 34> hudArtRound4() {
    std::array<HudArt, 34> arts{};
    static char bar_names[22][16];
    int n = 0;
    for (const char *color : {"red", "blue"}) {
        for (int frame = 0; frame < 11; ++frame) {
            std::snprintf(bar_names[n], sizeof bar_names[n], "bar_%s%02d.tex", color, frame);
            arts[n] = {"HUD.LMP", bar_names[n], 100, 8, 104, 8, 4};
            ++n;
        }
    }
    arts[n++] = {"HUD.LMP", "hud_statusbar00.tex", 160, 46, 80, 24, 23};
    arts[n++] = {"HUD.LMP", "hud_statusbar00glow.tex", 160, 46, 80, 24, 23};
    arts[n++] = {"INTERFAC.LMP", "button.tex", 83, 38, 88, 40, 19};
    for (const char *name : {"tilebot.tex", "tilebotleft.tex", "tilebotright.tex", "tileleft.tex",
                             "tileright.tex", "tiletop.tex", "tiletopleft.tex", "tiletopright.tex"})
        arts[n++] = {"INTERFAC.LMP", name, 32, 32, 32, 32, 16};
    arts[n++] = {"INTERFAC.LMP", "tilecenter.tex", 320, 224, 320, 224, 112};
    return arts;
}
Rect hudArtSource(const HudArt &art) { return {0, 0, art.used_width, art.used_height}; }
std::pair<Rect,Rect> FontMetrics::glyphRects(std::size_t index,int pen_x,int text_y) const {
    if (index>=glyphs.size()) throw FormatError("glyph index outside font");
    const auto &g=glyphs[index]; const int w=g.u1-g.u0,h=g.v1-g.v0;
    return {{g.u0,g.v0,w,h},{pen_x,checked(static_cast<long long>(text_y)+g.y_offset),w,h}};
}
int FontMetrics::widthAscii(const std::vector<std::uint8_t>&bytes,bool interlaced,std::size_t count) const {
    if (glyphs.empty()) throw FormatError("font has no glyph zero");
    long long pen=0; int previous=-1;
    for (std::size_t i=0;i<std::min(count,bytes.size());++i) {
        if (!bytes[i]) break;
        const int index=charmap[bytes[i]];
        if (index>=static_cast<int>(glyphs.size())) throw FormatError("invalid font map index");
        if (index<=0) pen+=static_cast<long long>(glyphs[0].advance)*16;
        else {
            const auto &g=glyphs[index];
            for (int k=g.kern_start;k>=0 && k<static_cast<int>(kerns.size()) && kerns[k].current==index;++k)
                if (kerns[k].previous==previous) { pen+=static_cast<long long>(kerns[k].adjustment)*(interlaced?16:8); break; }
            pen+=static_cast<long long>(g.advance)*16;
        }
        previous=index;
        if (pen<std::numeric_limits<int>::min()*16LL || pen>std::numeric_limits<int>::max()*16LL)
            throw FormatError("font width overflow");
    }
    // Arithmetic right shift, including negative kerning results.
    return checked(pen>=0 ? pen/16 : -((15-pen)/16));
}
}
