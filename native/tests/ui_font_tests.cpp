#include "champions/ui_font.hpp"
#include "ui_font_shape_test_helpers.hpp"
int main() {
    auto f=vectors();std::string line;std::vector<std::uint8_t> sample;int parity=0;
    while(std::getline(f,line)) {
        auto p=split(line,'|');if(p[0]!="F")continue;
        auto b=unhex(p[1]);if(sample.empty())sample=b;
        auto out=champions::readRtaUiFont({b.data(),b.size()});
        check(records(out.glyphs)==p[2],"glyph reference mismatch");
        check(records(out.kerns)==p[3],"kerning reference mismatch");
        check(hex(out.texture)==p[4],"texture suffix reference mismatch");++parity;
    }
    check(parity==6,"font parity vector count");
    // Authored safety tests, never counted as reference parity.
    refuses([]{champions::readRtaUiFont({nullptr,0x214});},"null refused");
    for(std::size_t n=0;n<sample.size()-7+1;++n)
        refuses([&]{champions::readRtaUiFont({sample.data(),n});},"truncation refused");
    for(auto offset: {8U,12U,16U}) {
        for(auto bad: {0xffffffffU,0xfffffff0U,static_cast<unsigned>(sample.size()+1)}) {
            auto b=sample;put32(b,offset,bad);
            refuses([&]{champions::readRtaUiFont({b.data(),b.size()});},"invalid offset refused");
        }
    }
    auto b=sample;put32(b,16,static_cast<unsigned>(b.size()));
    refuses([&]{champions::readRtaUiFont({b.data(),b.size()});},"empty suffix refused");
    b=sample;b[0]=255;b[1]=255;put32(b,8,0xfffffff0U);
    refuses([&]{champions::readRtaUiFont({b.data(),b.size()});},"count/range overflow refused");
    std::cout<<"font parity="<<parity<<" checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
