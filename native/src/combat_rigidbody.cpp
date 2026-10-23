// Executed RTA rigid-body constructor chain: research-combat-5 f33d1a3,
// docs/spec/rigidbody-init.md and tools/ref_rigidbody.py. Raw float operations
// retain the reference order. The sole draw uses the cosmetic seed.
#include "champions/combat_ctors.hpp"
#include <array>

namespace champions::simulation::rta::combat_ctors {
namespace {
Raw read(A::Model &m, Address a) { return Raw(m.mem.load(a, 4)); }
void write(A::Model &m, Address a, Raw v) { m.store(a, 4, v); }
template <std::size_t N> std::array<Raw, N> words(A::Model &m, Address a) {
  std::array<Raw, N> v{};
  for (std::size_t i = 0; i < N; ++i) v[i] = read(m, a + 4 * i);
  return v;
}
template <std::size_t N> void put(A::Model &m, Address a, const std::array<Raw, N> &v) {
  for (std::size_t i = 0; i < N; ++i) write(m, a + 4 * i, v[i]);
}
}

void rigidbody_quat_normalize(A::Model &m, Address q) {
  put(m, q, quat_normalize(words<4>(m, q)));
}
void rigidbody_quat_from_mat3(A::Model &m, Address out, Address matrix) {
  put(m, out, quat_from_mat3(words<9>(m, matrix)));
}
void rigidbody_update_derived(A::Model &m, Address body) {
  rigidbody_quat_normalize(m, body + 0x1c);
  const auto r = mat3_transpose(mat3_from_quat(words<4>(m, body + 0x1c)));
  put(m, body + 0x44, r);
  Mat3 scaled{};
  for (unsigned row = 0; row < 3; ++row)
    for (unsigned col = 0; col < 3; ++col)
      scaled[3 * row + col] = mul(r[3 * row + col], read(m, body + 4 + 4 * col));
  const auto iw = mat3_mul(scaled, mat3_transpose(r));
  put(m, body + 0x68, iw);
  put(m, body + 0x8c, mat3_mul_vec_transposed(iw, words<3>(m, body + 0x38)));
}
void rigidbody_init(A::Model &m, Address body, Raw mass, Raw factor,
                    Raw sx, Raw sy, Raw sz) {
  const auto x2 = mul(sx, sx), y2 = mul(sy, sy), z2 = mul(sz, sz);
  for (unsigned off = 0; off < 0x98; off += 4) write(m, body + off, 0);
  const auto weighted_mass = mul(factor, mass);
  const auto about_x = mul(weighted_mass, add(y2, z2));
  const auto about_z = mul(weighted_mass, add(x2, y2));
  const auto about_y = mul(weighted_mass, add(x2, z2));
  write(m, body + 0x24, ONE);
  const auto drawn = m.cosmetic_draw();
  const auto u = mul(from_int(drawn), 0x37800000);
  write(m, body, div(ONE, mass));
  write(m, body + 4, div(0x41400000, about_x));
  write(m, body + 0xc, div(0x41400000, about_z));
  write(m, body + 8, div(0x41400000, about_y));
  write(m, body + 0x28, u);
  rigidbody_quat_normalize(m, body + 0x1c);
  rigidbody_quat_normalize(m, body + 0x1c);
  put(m, body + 0x44, mat3_transpose(mat3_from_quat(words<4>(m, body + 0x1c))));
  put(m, body + 0x8c,
      mat3_mul_vec_transposed(words<9>(m, body + 0x68), words<3>(m, body + 0x38)));
}
} // namespace champions::simulation::rta::combat_ctors
