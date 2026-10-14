#include "champions/ui_art_shape.hpp"
#include <algorithm>
namespace champions {
Texture32 shapeRtaUiArt(const Texture32 &p, std::uint32_t rows, std::optional<UiArtCrop> crop) {
    if (!p.width || !p.height || p.width>1024 || p.height>1024 || rows>p.height)
        throw FormatError("Shape requires dimensions 1..1024 and rows 0..height.");
    const auto stride = std::size_t(p.width)*4;
    if (p.rgba.size()!=stride*p.height) throw FormatError("Shape requires exact RGBA8 length.");
    const auto h = rows ? rows*2 : p.height;
    const UiArtCrop r = crop.value_or(UiArtCrop{0,0,p.width,h});
    if (!r.width || !r.height || r.x>p.width || r.y>h || r.width>p.width-r.x || r.height>h-r.y)
        throw FormatError("Shape crop must be nonempty and contained in the resulting page.");
    Texture32 out{r.width,r.height,p.flags,{}};
    out.rgba.resize(std::size_t(r.width)*r.height*4);
    for (std::uint32_t y=0; y<r.height; ++y) {
        const auto dest_y=r.y+y;
        const auto k=dest_y/2;
        const auto src_y=rows ? (dest_y%2 ? std::min(rows+k,p.height-1) : k) : dest_y;
        const auto begin=p.rgba.begin()+std::size_t(src_y)*stride+std::size_t(r.x)*4;
        std::copy_n(begin,std::size_t(r.width)*4,out.rgba.begin()+std::size_t(y)*r.width*4);
    }
    return out;
}
}
