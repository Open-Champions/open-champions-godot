// Opt-in count-only measurement. No extraction, names, pixel hashes or writes.
#include "champions/texture_upscale.hpp"
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
namespace fs = std::filesystem;
namespace {
struct Totals {
    std::uint64_t successful=0, accepted=0, error=0, palette=0, flat=0, alpha=0;
};
std::array<Totals,9> totals{};
std::uint64_t textures=0,decoded=0,containers=0,refusedContainers=0;
void inspect(champions::Bytes bytes,const std::string &extension,unsigned depth=0) {
    using namespace champions;
    if(extension==".tex" || extension==".etex") {
        ++textures;
        Texture32 source{};
        try { source=decodeRtaTexture(bytes); } catch(const FormatError &) {return;}
        ++decoded;
        for(unsigned f=2;f<=4;++f) for(unsigned mode=0;mode<3;++mode) {
            auto &t=totals[(f-2)*3+mode];
            try {
                auto result=upscaleTexture(source,f,TextureFilter(mode));
                const auto m=measureTextureFidelity(source,result,f);
                ++t.successful; t.accepted+=m.accepted(); t.error+=m.error_numerator;
                t.palette+=m.new_palette_pixels; t.flat+=m.flat_region_changes; t.alpha+=m.hard_alpha_changes;
            } catch(const FormatError &) {} // Explicit bounded refusal, never silently dropped.
        }
    } else if(extension==".lmp" || extension==".gob") {
        if(depth>=4) {++refusedContainers;return;}
        std::vector<Entry> entries;
        try {entries=extension==".lmp"?readLmp(bytes):readGob(bytes);}
        catch(const FormatError &) {++refusedContainers;return;}
        ++containers;
        for(const auto &entry:entries) {
            std::string ext=fs::path(entry.name).extension().string();
            for(auto &c:ext) c=char(std::tolower(static_cast<unsigned char>(c)));
            inspect(payload(bytes,entry),ext,depth+1);
        }
    }
}
}
int run(const std::vector<fs::path> &args) {
    if(args.size()!=2 || args[0]!="--rta") {
        std::cerr<<"Usage: champions-texture-fidelity --rta PRIVATE_DIRECTORY\n";return 2;
    }
    try {
        const fs::path root(args[1]);
        if(!fs::is_directory(root) || fs::is_symlink(root)) throw std::runtime_error("input");
        for(const auto &entry:fs::recursive_directory_iterator(root)) {
            if(entry.is_symlink() || !entry.is_regular_file()) continue;
            auto ext=entry.path().extension().string();
            for(auto &c:ext) c=char(std::tolower(static_cast<unsigned char>(c)));
            if(ext!=".tex" && ext!=".etex" && ext!=".lmp" && ext!=".gob") continue;
            const auto size=entry.file_size();
            if(size>67108864) {
                if(ext==".tex" || ext==".etex") ++textures; else ++refusedContainers;
                continue;
            }
            std::ifstream stream(entry.path(),std::ios::binary);
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
            if(!stream.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(size))) throw std::runtime_error("read");
            inspect({bytes.data(),bytes.size()},ext);
        }
        std::cout<<"texture_occurrences\t"<<textures<<"\ndecoded\t"<<decoded<<"\ncontainers\t"<<containers<<"\nrefused_containers\t"<<refusedContainers<<'\n';
        std::cout<<"factor\tfilter\tsuccessful\texact_fidelity\terror_numerator\terror_denominator\tnew_palette_pixels\tflat_changes\thard_alpha_changes\n";
        const char *names[]{"nearest","pixel-art","bicubic"};
        for(unsigned f=2;f<=4;++f) for(unsigned mode=0;mode<3;++mode) {
            const auto &t=totals[(f-2)*3+mode];
            std::cout<<f<<'\t'<<names[mode]<<'\t'<<t.successful<<'\t'<<t.accepted<<'\t'<<t.error<<'\t'<<f*f<<'\t'<<t.palette<<'\t'<<t.flat<<'\t'<<t.alpha<<'\n';
        }
        return textures?0:2;
    } catch(...) {std::cerr<<"Count-only validation failed (traversal, read or allocation).\n";return 1;}
}

#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    std::vector<fs::path> args;
    for(int i=1;i<argc;++i) args.emplace_back(argv[i]);
    return run(args);
}
#else
int main(int argc,char **argv) {
    std::vector<fs::path> args;
    for(int i=1;i<argc;++i) args.push_back(fs::u8path(argv[i]));
    return run(args);
}
#endif
