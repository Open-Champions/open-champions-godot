#include "champions/fx_decals.hpp"

#include "champions/core_math.hpp"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace champions::rta::fx::decals {
namespace C = champions::simulation::rta;

Raw f32(double value) noexcept {
    if (std::isfinite(value) && std::fabs(value) >= 3.4028235677973366e38)
        return value < 0 ? 0xFF800000u : 0x7F800000u;
    const float f = static_cast<float>(value);
    Raw r;
    std::memcpy(&r, &f, sizeof r);
    return r;
}

double to_float(Raw raw) noexcept {
    float f;
    std::memcpy(&f, &raw, sizeof f);
    return static_cast<double>(f);
}

bool allowed(std::int64_t kind, std::int64_t detail) noexcept {
    const std::uint64_t k = static_cast<std::uint64_t>(kind);
    if (detail == 0 && k <= 2)
        return false;
    if (detail == 1 && k == 0)
        return false;
    return true;
}

std::optional<std::pair<int, int>> cell_of(double x, double y) noexcept {
    const std::int64_t cx = (static_cast<std::int64_t>(C::to_int(f32(x))) + GRID_ORIGIN) >> 8;
    const std::int64_t cy = (static_cast<std::int64_t>(C::to_int(f32(y))) + GRID_ORIGIN) >> 8;
    if (0 <= cx && cx < GRID && 0 <= cy && cy < GRID)
        return std::make_pair(static_cast<int>(cx), static_cast<int>(cy));
    return std::nullopt;
}

std::array<std::int16_t, 12> Decal::record() const {
    auto h = [](std::int64_t v) { return static_cast<std::int16_t>(C::s16(static_cast<std::uint32_t>(v))); };
    return {h(rgb[0]), h(rgb[1]), h(rgb[2]), 0x80, h(x4), h(y4), h(z4), h(counted),
            h(c4),     h(s4),     h(tex),    0};
}

DecalStore::DecalStore(std::int64_t detail_) : detail(detail_) {
    for (int i = 0; i < BLOCKS; ++i) {
        free_.push_back(i);
        blocks_[static_cast<std::size_t>(i)].index = i;
    }
    grid_.fill(-1);
}

const Decal *DecalStore::add(double size, const std::array<double, 3> &pos, std::int64_t angle,
                             std::int64_t kind, std::int64_t grow,
                             const std::array<std::int64_t, 3> &tint) {
    if (!allowed(kind, detail))
        return nullptr;
    const Raw x = f32(pos[0]), y = f32(pos[1]), z = f32(pos[2]);
    const auto c = cell_of(to_float(x), to_float(y));
    if (!c)
        return nullptr;
    int &slot = grid_[static_cast<std::size_t>(c->second * GRID + c->first)];
    if (slot < 0) {
        if (free_.empty())
            return nullptr;
        slot = free_.front();
        free_.pop_front();
        Block &fresh = blocks_[static_cast<std::size_t>(slot)];
        fresh.count = 0;
        fresh.zmin = fresh.zmax = 0;
        fresh.counted = 0;
    }
    Block &blk = blocks_[static_cast<std::size_t>(slot)];
    if (blk.count >= PER_BLOCK)
        return nullptr;
    const Raw sz = f32(size);
    const Raw four = f32(4.0);
    const std::uint32_t a = static_cast<std::uint32_t>(angle);
    Decal &d = blk.decals[static_cast<std::size_t>(blk.count)];
    d.kind = kind;
    for (std::size_t i = 0; i < 3; ++i)
        d.rgb[i] = static_cast<int>(tint[i] & 0xFF);
    d.x4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(x, four))));
    d.y4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(y, four))));
    d.z4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(C::add(z, f32(3.0)), four))));
    d.counted = static_cast<std::uint64_t>(kind) < 3 ? 1 : 0;
    d.c4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(C::mul(C::cos16(a), sz), four))));
    d.s4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(C::mul(C::sin16(a), sz), four))));
    d.tex = C::s16(static_cast<std::uint32_t>(kind * 2 + TEX_BASE));
    const int index = blk.count;
    blk.count += 1;
    blk.counted += d.counted;
    if (C::lt(z, C::from_int(static_cast<std::uint32_t>(blk.zmin))))
        blk.zmin = C::s16(static_cast<std::uint32_t>(C::to_int(z)));
    if (C::lt(C::from_int(static_cast<std::uint32_t>(blk.zmax)), z))
        blk.zmax = C::s16(static_cast<std::uint32_t>(C::to_int(z)));
    total_ += 1;
    if (grow) {
        for (auto &g : growers_) {
            if (!g) {
                Grower gr;
                gr.mode = static_cast<int>(grow & 0xFF);
                gr.block = blk.index;
                gr.index = index;
                gr.angle = static_cast<std::uint32_t>(angle & 0xFFFF);
                gr.size = 0;
                gr.final_size = sz;
                g = gr;
                break;
            }
        }
    }
    return &d;
}

void DecalStore::grow_step(bool frozen) {
    if (frozen || total_ == 0)
        return;
    const Raw four = f32(4.0);
    for (auto &g : growers_) {
        if (!g)
            continue;
        const Raw step = g->mode == 2 ? STEP_SLOW : STEP_FAST;
        g->size = C::add(g->size, step);
        Decal &d = blocks_[static_cast<std::size_t>(g->block)].decals[static_cast<std::size_t>(g->index)];
        d.c4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(C::mul(C::cos16(g->angle), g->size), four))));
        d.s4 = C::s16(static_cast<std::uint32_t>(C::to_int(C::mul(C::mul(C::sin16(g->angle), g->size), four))));
        if (C::le(g->final_size, g->size))
            g.reset();
    }
}

void DecalStore::reset() {
    free_.clear();
    for (int i = 0; i < BLOCKS; ++i) {
        free_.push_back(i);
        Block &b = blocks_[static_cast<std::size_t>(i)];
        b.count = 0;
        b.zmin = b.zmax = 0;
        b.counted = 0;  // the record memory is kept: growers may still write into it
    }
    grid_.fill(-1);
}

int DecalStore::cell_block(int cx, int cy) const {
    if (cx < 0 || cx >= GRID || cy < 0 || cy >= GRID)
        throw std::out_of_range("decal cell");
    return grid_[static_cast<std::size_t>(cy * GRID + cx)];
}

const Block &DecalStore::block(int index) const { return blocks_.at(static_cast<std::size_t>(index)); }

std::vector<std::pair<std::pair<int, int>, const Decal *>> DecalStore::decals() const {
    std::vector<std::pair<std::pair<int, int>, const Decal *>> out;
    for (int cy = 0; cy < GRID; ++cy)
        for (int cx = 0; cx < GRID; ++cx) {
            const int b = grid_[static_cast<std::size_t>(cy * GRID + cx)];
            if (b < 0)
                continue;
            const Block &blk = blocks_[static_cast<std::size_t>(b)];
            for (int i = 0; i < blk.count; ++i)
                out.push_back({{cx, cy}, &blk.decals[static_cast<std::size_t>(i)]});
        }
    return out;
}

}  // namespace champions::rta::fx::decals
