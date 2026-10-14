#include "champions/ui_art_shape.hpp"
#include "ui_font_shape_test_helpers.hpp"
int main() {
    auto f=vectors();std::string line;int parity=0;
    while(std::getline(f,line)) {
        auto p=split(line,'|');if(p[0]!="S")continue;
        auto dims=split(p[1],',');
        champions::Texture32 page{static_cast<std::uint32_t>(std::stoul(dims[0])),static_cast<std::uint32_t>(std::stoul(dims[1])),0,unhex(p[3])};
        std::optional<champions::UiArtCrop> crop;
        if(p[2]!="-") {auto c=split(p[2],',');crop=champions::UiArtCrop{static_cast<std::uint32_t>(std::stoul(c[0])),static_cast<std::uint32_t>(std::stoul(c[1])),static_cast<std::uint32_t>(std::stoul(c[2])),static_cast<std::uint32_t>(std::stoul(c[3]))};}
        auto out=champions::shapeRtaUiArt(page,static_cast<std::uint32_t>(std::stoul(dims[2])),crop);
        check(std::to_string(out.width)+","+std::to_string(out.height)==p[4],"shape reference dimensions");
        check(hex(out.rgba)==p[5],"shape reference bytes");++parity;
    }
    check(parity==52,"shape parity vector count");
    // Safety refusals outside supported valid-input reference domain.
    champions::Texture32 page{2,3,0,std::vector<std::uint8_t>(24,17)};
    for(auto size:{0U,1025U,0xffffffffU}){auto p=page;p.width=size;refuses([&]{champions::shapeRtaUiArt(p,0);},"invalid width");p=page;p.height=size;refuses([&]{champions::shapeRtaUiArt(p,0);},"invalid height");}
    for(auto rows:{4U,0xffffffffU})refuses([&]{champions::shapeRtaUiArt(page,rows);},"invalid rows");
    for(std::size_t n=0;n<24;++n){auto p=page;p.rgba.resize(n);refuses([&]{champions::shapeRtaUiArt(p,0);},"truncated pixels");}
    auto p=page;p.rgba.push_back(17);refuses([&]{champions::shapeRtaUiArt(p,0);},"oversized pixels");
    for(auto crop:{champions::UiArtCrop{0,0,0,1},champions::UiArtCrop{0,0,1,0},champions::UiArtCrop{2,0,1,1},champions::UiArtCrop{0,6,1,1},champions::UiArtCrop{0xffffffffU,0,2,1},champions::UiArtCrop{0,0,0xffffffffU,1}})
        refuses([&]{champions::shapeRtaUiArt(page,3,crop);},"invalid crop");
    std::cout<<"shape parity="<<parity<<" checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
