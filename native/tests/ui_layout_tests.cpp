#include "champions/ui_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace champions;
using namespace champions::ui_layout;
namespace {
int checks=0;
void expect(bool yes,const char*label) { ++checks; if (!yes) throw std::runtime_error(label); }
template<class F> void rejects(F f,const char*label) {
    bool threw=false; try { f(); } catch (const std::exception&) { threw=true; } expect(threw,label);
}
bool same(Clip a,Clip b) { return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom; }
void put16(std::vector<std::uint8_t>&b,std::size_t o,int v) { b[o]=v&255; b[o+1]=(v>>8)&255; }
void put32(std::vector<std::uint8_t>&b,std::size_t o,std::uint32_t v) { put16(b,o,v&65535); put16(b,o+2,v>>16); }
float word(const std::string &hex) {
    const auto bits=std::uint32_t(std::stoul(hex,nullptr,16)); float f; std::memcpy(&f,&bits,sizeof f); return f;
}
// tools/generate_hud_pending_vectors.py: research-ui 3e39392 docs/hud-layout.md
// pending overlays through the pinned tools/ee_emu.py float model.
void pendingVectors(const char *path) {
    std::ifstream in(path); expect(bool(in),"hud_pending.tsv opens");
    std::string line; int rows=0;
    while (std::getline(in,line)) {
        if (line.empty()||line[0]=='#') continue;
        std::istringstream f(line); int origin,slot,n; std::string h,hm,ph,m,mm,pm;
        f>>origin>>slot>>h>>hm>>ph>>m>>mm>>pm>>n;
        const auto got=hudPendingRects(origin,slot,word(h),word(hm),word(ph),word(m),word(mm),word(pm));
        expect(int(got.size())==n,"pending overlay count");
        for (const auto &r:got) {
            int l,t,ri,b; std::string colour; f>>l>>t>>ri>>b>>colour;
            expect(r.left==l&&r.top==t&&r.right==ri&&r.bottom==b&&r.colour==std::stoul(colour,nullptr,16)&&r.layer==6,"pending overlay vector");
        }
        ++rows;
    }
    expect(rows==9,"nine pending overlay vectors");
}
std::vector<std::uint8_t> fontFixture() {
    std::vector<std::uint8_t> b(0x290); put16(b,0,3);put16(b,2,123);put32(b,4,2);
    put32(b,8,0x214);put32(b,12,0x244);put32(b,16,0x250);
    for (int i=0;i<256;++i) put16(b,0x14+i*2,-1);
    put16(b,0x14+'A'*2,1);put16(b,0x14+'B'*2,2);
    const int g[3][8]={{0,0,0,0,0,0,3,-1},{65,1,5,2,8,2,7,-1},{66,5,10,4,10,0,8,0}};
    for (int i=0;i<3;++i) for(int j=0;j<8;++j) put16(b,0x214+i*16+j*2,g[i][j]);
    put16(b,0x244,1);put16(b,0x246,2);put16(b,0x248,-3);
    return b;
}
}
int main(int argc,char**argv) {
 try {
    auto a=menuFrame(100,80,105,100,1);
    expect(a.size()==13,"btile draw count");
    expect(a[0].resource=="btilebotleft.tex"&&a[0].x==100&&a[0].y==147,"btile first corner");
    expect(a[1].x==173&&a[1].y==147&&a[2].y==79&&a[3].x==173,"btile corner order");
    expect(a[6].resource=="btiletop.tex"&&a[6].x==164&&same(a[6].clip,{100,80,173,148}),"partial top");
    expect(a.back().x==132&&a.back().y==112&&same(a.back().clip,{100,80,173,148}),"btile center");
    auto b=menuFrame(100,80,105,100,2);
    expect(b.back().resource=="tilecenter.tex"&&b.back().x==107&&b.back().y==86&&same(b.back().clip,{100,80,199,174}),"tile center overlap");
    expect(same(b[5].clip,{100,80,177,180}),"tile bottom clip overlap");
    auto centers=menuFrame(0,0,64,238,2,false,true);
    expect(centers.size()==2&&centers[0].y==6&&centers[1].y==230,"inclusive last row");
    expect(menuFrame(0,0,12,13,2,false,true).empty(),"small center missing");
    expect(menuFrame(0,0,10,10,2,true,true).empty(),"both suppression flags");
    auto small=menuFrame(0,0,10,10,1,true);
    expect(small.size()==4&&small[1].x==-22&&small[1].y==-23,"small frame not clamped");
    auto c=menuFrame(0,0,100,100,9),d=menuFrame(0,0,100,100,0);
    expect(c.size()==d.size()&&c[0].resource==d[0].resource,"style fallback");
    expect(frameBorder(2)==32&&frameBorder(1)==32&&frameBorder(0)==16,"borders");
    auto r=frameSourceRect("tilecenter.tex");expect(r.width==320&&r.height==224,"tile source");
    r=frameSourceRect("ctiletop.tex");expect(r.width==16&&r.height==16,"ctile source");
    r=frameSourceRect("btileleft.tex");expect(r.width==32&&r.height==32,"btile source");
    rejects([]{frameSourceRect("guessed.tex");},"unknown frame rejected");
    r=hudSourceRect("bar_red10.tex");expect(r.width==100&&r.height==8,"red HUD source");
    r=hudSourceRect("bar_green00.tex");expect(r.width==100&&r.height==6,"green HUD source");
    r=hudSourceRect("hud_multi.tex");expect(r.width==158&&r.height==46,"remote HUD source");
    r=hudSourceRect("hud_statusbar00glow.tex");expect(r.width==160&&r.height==46,"glow HUD source");
    rejects([]{hudSourceRect("bar_blue11.tex");},"unknown HUD frame rejected");
    expect((hudOrigins()==std::array<int,4>{12,478,167,323}),"default origins");
    expect((hudOrigins(true,{false,false,true,false})==std::array<int,4>{167,478,12,323}),"local promotion");
    expect((hudOrigins(true,{true,false,true,false})==std::array<int,4>{12,478,12,323}),"slot zero remains");
    auto layers=hudBaseLayers(12,44,1,4,2,4,50,0,100,1,true,1);
    expect(layers.size()==4&&layers[0].draw.resource=="bar_red00.tex"&&layers[1].draw.resource=="bar_green00.tex"&&layers[2].draw.resource=="bar_blue00.tex"&&layers[3].draw.resource=="hud_statusbar00.tex","base HUD order");
    expect(layers[0].draw.x==27&&layers[0].draw.y==46&&layers[1].draw.y==56&&layers[2].draw.y==63&&layers[3].draw.x==8,"base HUD positions");
    expect(same(layers[0].draw.clip,{27,24,51,124})&&layers[2].tint==0x2aacacac,"base HUD clip and tint");
    auto remote=hudBaseLayers(478,43,1,1,0,0,0,0,1,1,false);
    expect(remote.size()==2&&remote[0].draw.resource=="bar_red10.tex"&&remote[1].draw.resource=="hud_multi.tex"&&remote[1].draw.x==474,"remote HUD");
    const int fills[5]={27,51,76,101,126};
    for(int i=0;i<5;++i) expect(barClipRight(27,float(i),4.f)==fills[i],"fill exact vector");
    expect(barClipRight(27,8,4)==225,"fill is unclamped");
    const struct {float value,maximum;int right;} fractional[]={{1,3,59},{2,3,92},{3,7,69},{5,9,81},{17,31,81},{1,8,39},{0.1f,1,36},{0.7f,1,96}};
    for(const auto &v:fractional)expect(barClipRight(27,v.value,v.maximum)==v.right,"fractional PS2 fill vector");
    rejects([]{barClipRight(27,0,0);},"zero maximum rejected");
    rejects([]{barClipRight(27,-1,2);},"negative current rejected");
    rejects([]{barClipRight(27,NAN,1);},"nan rejected");
    rejects([]{barClipRight(27,1,INFINITY);},"infinity rejected");
    expect(argc>1,"hud_pending.tsv path argument");
    pendingVectors(argv[1]);
    rejects([]{hudPendingRects(12,4,1,2,1,1,2,1);},"pending overlay slot outside 0..3");
    rejects([]{hudPendingRects(12,0,1,0,1,1,2,0);},"pending overlay zero maximum");
    rejects([]{pendingRight(27,180,1,NAN,2);},"pending overlay nan");
    expect(hudPendingRects(12,0,1,2,0,1,2,-1).empty(),"no positive pending, no overlay");
    auto fbytes=fontFixture();auto font=fontMetrics({fbytes.data(),fbytes.size()});
    expect(font.glyphs.size()==3&&font.kerns.size()==2&&font.glyphs[2].u0==5&&font.glyphs[2].advance==8,"font fields");
    expect(font.kerns[0].previous==1&&font.kerns[0].current==2&&font.kerns[0].adjustment==-3,"signed kerning");
    auto rects=font.glyphRects(1,100,50);
    expect(rects.first.x==1&&rects.first.y==2&&rects.first.width==4&&rects.first.height==6&&rects.second.x==100&&rects.second.y==52,"glyph rectangles");
    expect(font.widthAscii({'A','B'})==12&&font.widthAscii({'A','B'},false)==13,"ASCII kerning modes");
    expect(font.widthAscii({'B','A'})==15&&font.widthAscii({'A','?','B'})==18,"ASCII order and fallback");
    expect(font.widthAscii({'A','B',0,'B','A'})==12&&font.widthAscii({'A','B'},true,1)==7&&font.widthAscii({'A','B'},true,0)==0,"ASCII NUL and count");
    auto signed_font=fbytes;put16(signed_font,0x214+16+2,-32768);put16(signed_font,0x214+16+4,32767);
    put16(signed_font,0x214+16+6,-7);put16(signed_font,0x214+16+8,9);put16(signed_font,0x214+16+10,-32768);
    auto signed_metrics=fontMetrics({signed_font.data(),signed_font.size()});
    auto signed_rect=signed_metrics.glyphRects(1,11,32767);
    expect(signed_rect.first.x==-32768&&signed_rect.first.width==65535&&signed_rect.first.y==-7&&signed_rect.second.y==-1,"signed glyph extrema");
    put16(signed_font,0x214+16+12,-1);
    signed_metrics=fontMetrics({signed_font.data(),signed_font.size()});
    expect(signed_metrics.widthAscii({'A'})==-1,"signed advance and arithmetic shift");
    rejects([&]{fontMetrics({fbytes.data(),0x213});},"short font rejected");
    rejects([&]{fontMetrics({fbytes.data(),fbytes.size()-1});},"short texture rejected");
    auto bad=fbytes;put32(bad,8,0);rejects([&]{fontMetrics({bad.data(),bad.size()});},"bad glyph offset rejected");
    bad=fbytes;put32(bad,12,0x215);rejects([&]{fontMetrics({bad.data(),bad.size()});},"overlapping kerning offset rejected");
    bad=fbytes;put32(bad,16,0xffffffff);rejects([&]{fontMetrics({bad.data(),bad.size()});},"bad texture offset rejected");
    bad=fbytes;put16(bad,0x14,3);rejects([&]{fontMetrics({bad.data(),bad.size()});},"bad charmap rejected");
    auto changed_word=fbytes;put16(changed_word,2,9);
    expect(fontMetrics({changed_word.data(),changed_word.size()}).widthAscii({'A','B'})==font.widthAscii({'A','B'}),"header +2 is not a width");
    const auto arts=hudArtRound4();
    expect(arts.size()==34,"thirty-four HUD identities");
    std::vector<std::string> names;
    int old_matches=0,bars=0;
    std::vector<std::pair<int,int>> used;
    for (const auto &art:arts) {
        names.emplace_back(art.entry);
        const auto source=hudArtSource(art);
        expect(source.x==0&&source.y==0&&source.width==art.used_width&&source.height==art.used_height,"source is the used header");
        expect(art.rows*2==art.used_height,"one field is half the used height");
        used.emplace_back(art.used_width,art.used_height);
        const std::string entry=art.entry;
        std::pair<int,int> old{32,32};
        if (entry.rfind("bar_",0)==0) old={104,8};
        else if (entry.rfind("hud_status",0)==0) old={160,48};
        else if (entry=="button.tex") old={88,40};
        else if (entry=="tilecenter.tex") old={320,224};
        const bool matched=old.first==art.used_width&&old.second==art.used_height;
        if (matched) ++old_matches;
        const bool tile=entry.rfind("tile",0)==0;
        expect(matched==tile,"only frame tiles already matched the header");
        if (entry.rfind("bar_",0)==0) {
            ++bars;
            expect(art.upload_width==104&&art.upload_height==8&&art.used_width==100&&art.used_height==8,"bar upload stays 104x8");
            auto drawn=hudSourceRect(entry);
            expect(drawn.width==100&&drawn.height==8,"bar source agrees");
        }
        if (entry=="button.tex")
            expect(art.used_width==83&&art.used_height==38&&art.upload_width==88&&art.upload_height==40&&art.rows==19,"button used crop");
        if (entry.rfind("hud_status",0)==0) {
            auto drawn=hudSourceRect(entry);
            expect(drawn.width==160&&drawn.height==46&&art.upload_width==80&&art.upload_height==24,"status source agrees");
        }
        if (tile && entry!="tilecenter.tex") {
            auto drawn=frameSourceRect(entry);
            expect(drawn.width==32&&drawn.height==32,"frame tile source agrees");
        }
    }
    expect(old_matches==9&&bars==22,"nine prior pages and twenty-two bars");
    expect(std::find(names.begin(),names.end(),"button.tex")!=names.end(),"button is one of the thirty-four");
    rejects([]{hudSourceRect("button.tex");},"button is not a bar source");
    auto center=frameSourceRect("tilecenter.tex");
    expect(center.width==320&&center.height==224,"center source stays");
    std::sort(used.begin(),used.end());
    used.erase(std::unique(used.begin(),used.end()),used.end());
    expect(used.size()==5,"five used sizes");
    std::cout<<"ui-layout exact synthetic checks: "<<checks<<" passed\n";
 } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
