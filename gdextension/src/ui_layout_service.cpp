#include "champions/ui_font_page.hpp"
#include "champions/ui_layout.hpp"
#include "champions/ui_text_layout.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {
using namespace godot;
namespace layout = champions::ui_layout;
Dictionary bounds(layout::Clip c) {
    Dictionary d; d["left"]=c.left;d["top"]=c.top;d["right"]=c.right;d["bottom"]=c.bottom;return d;
}
Dictionary rect(layout::Rect r) {
    Dictionary d;d["x"]=r.x;d["y"]=r.y;d["width"]=r.width;d["height"]=r.height;return d;
}
Dictionary draw(const layout::Draw &v) {
    Dictionary d;d["resource"]=String::utf8(v.resource.c_str());d["x"]=v.x;d["y"]=v.y;
    d["clipped"]=v.clipped;if(v.clipped)d["clip"]=bounds(v.clip);return d;
}
Dictionary resultError(const std::exception &e) {
    Dictionary d;d["ok"]=false;d["error"]=String::utf8(e.what());return d;
}
class ChampionsUiLayoutService : public RefCounted {
    GDCLASS(ChampionsUiLayoutService, RefCounted)
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("menu_frame","x","y","width","height","style","no_center","no_border"),&ChampionsUiLayoutService::menu_frame);
        ClassDB::bind_method(D_METHOD("frame_source_rect","resource"),&ChampionsUiLayoutService::frame_source_rect);
        ClassDB::bind_method(D_METHOD("hud_source_rect","resource"),&ChampionsUiLayoutService::hud_source_rect);
        ClassDB::bind_method(D_METHOD("hud_origins","network","local_slots","player_count"),&ChampionsUiLayoutService::hud_origins);
        ClassDB::bind_method(D_METHOD("bar_clip_right","origin","current","maximum"),&ChampionsUiLayoutService::bar_clip_right);
        ClassDB::bind_method(D_METHOD("hud_pending_rects","x","slot","health","health_max","pending_health","mana","mana_max","pending_mana"),&ChampionsUiLayoutService::hud_pending_rects);
        ClassDB::bind_method(D_METHOD("hud_base_layers","x","tick","health","health_max","mana","mana_max","experience","experience_base","experience_next","acknowledged_level","local","cooldown","health_flash"),&ChampionsUiLayoutService::hud_base_layers);
        ClassDB::bind_method(D_METHOD("font_metrics","fnt","ascii","interlaced","count","glyph_index","pen_x","text_y"),&ChampionsUiLayoutService::font_metrics);
        ClassDB::bind_method(D_METHOD("decode_font_page","page","weave"),&ChampionsUiLayoutService::decode_font_page);
        ClassDB::bind_method(D_METHOD("decode_hud_font","fnt","weave"),&ChampionsUiLayoutService::decode_hud_font);
        ClassDB::bind_method(D_METHOD("hud_text_glyphs","fnt","ascii","anchor_x","text_y","tint"),&ChampionsUiLayoutService::hud_text_glyphs);
        ClassDB::bind_method(D_METHOD("font_header_line_height"),&ChampionsUiLayoutService::font_header_line_height);
        ClassDB::bind_method(D_METHOD("hud_art_round4"),&ChampionsUiLayoutService::hud_art_round4);
    }
public:
    Dictionary menu_frame(int x,int y,int width,int height,int style,bool no_center,bool no_border) const {
        try { Array a;for(const auto &v:layout::menuFrame(x,y,width,height,style,no_center,no_border))a.push_back(draw(v));
            Dictionary d;d["ok"]=true;d["draws"]=a;d["border"]=layout::frameBorder(style);return d;
        } catch(const std::exception&e){return resultError(e);}
    }
    Dictionary frame_source_rect(const String &resource) const {
        try {auto s=resource.utf8();Dictionary d;d["ok"]=true;d["rect"]=rect(layout::frameSourceRect(s.get_data()));return d;}
        catch(const std::exception&e){return resultError(e);}
    }
    Dictionary hud_source_rect(const String &resource) const {
        try {auto s=resource.utf8();Dictionary d;d["ok"]=true;d["rect"]=rect(layout::hudSourceRect(s.get_data()));return d;}
        catch(const std::exception&e){return resultError(e);}
    }
    Dictionary hud_origins(bool network,const PackedInt32Array &local_slots,int player_count) const {
        try {std::array<bool,4> local{};for(int i=0;i<local_slots.size();++i) {
                if(local_slots[i]<0||local_slots[i]>3)throw champions::FormatError("invalid local HUD slot");
                local[local_slots[i]]=true;
            }
            auto v=layout::hudOrigins(network,local,player_count);PackedInt32Array a;for(int x:v)a.push_back(x);
            Dictionary d;d["ok"]=true;d["origins"]=a;return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary bar_clip_right(int origin,double current,double maximum) const {
        try {Dictionary d;d["ok"]=true;d["right"]=layout::barClipRight(origin,float(current),float(maximum));return d;}
        catch(const std::exception&e){return resultError(e);}
    }
    Dictionary hud_base_layers(int x,int64_t tick,double health,double health_max,double mana,double mana_max,
                               int experience,int experience_base,int experience_next,int acknowledged_level,
                               bool local,int cooldown,bool health_flash) const {
        try {if(tick<0||tick>UINT32_MAX)throw champions::FormatError("HUD tick outside supported range");
            Array a;for(const auto &v:layout::hudBaseLayers(x,std::uint32_t(tick),float(health),float(health_max),float(mana),float(mana_max),experience,experience_base,experience_next,acknowledged_level,local,cooldown,health_flash)) {
                auto d=draw(v.draw);d["tint"]=int64_t(v.tint);d["layer"]=v.layer;a.push_back(d);
            }Dictionary d;d["ok"]=true;d["layers"]=a;return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary hud_pending_rects(int x,int slot,double health,double health_max,double pending_health,
                                 double mana,double mana_max,double pending_mana) const {
        try {Array a;for(const auto &r:layout::hudPendingRects(x,slot,float(health),float(health_max),float(pending_health),float(mana),float(mana_max),float(pending_mana))) {
                Dictionary d;d["left"]=r.left;d["top"]=r.top;d["right"]=r.right;d["bottom"]=r.bottom;
                d["colour"]=int64_t(r.colour);d["layer"]=r.layer;a.push_back(d);
            }Dictionary d;d["ok"]=true;d["rects"]=a;return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary font_metrics(const PackedByteArray &fnt,const PackedByteArray &ascii,bool interlaced,int count,int glyph_index,int pen_x,int text_y) const {
        try {if(count<0)throw champions::FormatError("negative ASCII count");
            auto font=layout::fontMetrics({fnt.ptr(),std::size_t(fnt.size())});
            std::vector<std::uint8_t> text;
            if(ascii.size())text.assign(ascii.ptr(),ascii.ptr()+ascii.size());
            Dictionary d;d["ok"]=true;d["glyph_count"]=int(font.glyphs.size());d["kern_count"]=int(font.kerns.size());
            d["ascii_width"]=font.widthAscii(text,interlaced,std::size_t(count));
            if(glyph_index>=0){auto pair=font.glyphRects(std::size_t(glyph_index),pen_x,text_y);
                d["source"]=rect(pair.first);d["destination"]=rect(pair.second);}
            return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary decode_font_page(const PackedByteArray &page,bool weave) const {
        try {
            const auto decoded=champions::decodeFontPage({page.ptr(),std::size_t(page.size())},weave);
            PackedByteArray rgba;
            if(rgba.resize(int64_t(decoded.image.rgba.size()))!=OK) throw std::bad_alloc();
            std::copy(decoded.image.rgba.begin(),decoded.image.rgba.end(),rgba.ptrw());
            Dictionary d;d["ok"]=true;d["width"]=int(decoded.image.width);d["height"]=int(decoded.image.height);
            d["flags"]=int(decoded.image.flags);d["rgba"]=rgba;d["uncovered"]=int64_t(decoded.uncovered);
            d["packet_offset"]=int64_t(decoded.packet_offset);return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary decode_hud_font(const PackedByteArray &fnt,bool weave) const {
        try {
            const auto decoded=layout::decodeFontFile({fnt.ptr(),std::size_t(fnt.size())},weave);
            PackedByteArray rgba;
            if(rgba.resize(int64_t(decoded.image.rgba.size()))!=OK) throw std::bad_alloc();
            std::copy(decoded.image.rgba.begin(),decoded.image.rgba.end(),rgba.ptrw());
            Dictionary d;d["ok"]=true;d["width"]=int(decoded.image.width);d["height"]=int(decoded.image.height);
            d["flags"]=int(decoded.image.flags);d["rgba"]=rgba;d["uncovered"]=int64_t(decoded.uncovered);
            d["packet_offset"]=int64_t(decoded.packet_offset);return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary hud_text_glyphs(const PackedByteArray &fnt,const PackedByteArray &ascii,int anchor_x,int text_y,int64_t tint) const {
        try {
            if(tint<0||tint>UINT32_MAX) throw champions::FormatError("HUD text tint is outside a 32-bit word");
            std::vector<std::uint8_t> text;
            if(ascii.size()) text.assign(ascii.ptr(),ascii.ptr()+ascii.size());
            const auto glyphs=champions::ui_text_layout::hudTextGlyphs({fnt.ptr(),std::size_t(fnt.size())},text,anchor_x,text_y,std::uint32_t(tint));
            Array rows;
            for(const auto &g:glyphs){
                Dictionary item;item["x_half"]=g.x_half;item["y"]=g.y;item["source_x"]=g.source.x;item["source_y"]=g.source.y;
                item["source_width"]=g.source.width;item["source_height"]=g.source.height;item["tint"]=int64_t(g.tint);rows.push_back(item);
            }
            Dictionary d;d["ok"]=true;d["glyphs"]=rows;return d;
        }catch(const std::exception&e){return resultError(e);}
    }
    Dictionary font_header_line_height() const {
        try { champions::refuseFntHeaderLineHeight(); }
        catch(const std::exception&e){return resultError(e);}
    }
    Dictionary hud_art_round4() const {
        try {
            Array rows;
            for(const auto &art:layout::hudArtRound4()){
                Dictionary item;item["lump"]=String::utf8(art.lump);item["entry"]=String::utf8(art.entry);
                item["used_width"]=art.used_width;item["used_height"]=art.used_height;
                item["upload_width"]=art.upload_width;item["upload_height"]=art.upload_height;item["rows"]=art.rows;
                item["source_x"]=0;item["source_y"]=0;item["source_width"]=art.used_width;item["source_height"]=art.used_height;
                rows.push_back(item);
            }
            Dictionary d;d["ok"]=true;d["entries"]=rows;return d;
        }catch(const std::exception&e){return resultError(e);}
    }
};
}
void register_ui_layout_dynamic_service();
void register_ui_layout_service() { GDREGISTER_CLASS(ChampionsUiLayoutService); register_ui_layout_dynamic_service(); }
