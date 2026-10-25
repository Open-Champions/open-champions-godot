// Compatibility measurement only: compare against the immutable integration
// base's actual writers. New animation expectations come from Python instead.
#include "champions/native_save.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace N = R::native_save;
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    if (argc != 2)
      throw std::runtime_error("output required");
    std::ofstream out(std::filesystem::path(argv[1]), std::ios::binary);
    for (unsigned policy = 0; policy < 4; ++policy)
      for (unsigned f = 0; f < 16; ++f) {
        C::Config c;
        c.model = R::Fpu::Model(f / 4);
        c.mul_bias = f & 2;
        c.div_nearest = f & 1;
        if (policy) {
          c.movement = C::MovementPolicy::DryCreature;
          c.max_ready_ticks = 512;
        }
        if (policy == 2) {
          c.recovery.emplace();
          c.max_ready_ticks = 256;
        }
        if (policy == 3) {
          c.facing.emplace();
          c.facing->initial.rate = R::ONE;
          c.max_ready_ticks = 256;
        }
        N::Scene scene(c);
        scene.advance(21);
        scene.advance(5);
        scene.advance(16);
        scene.set_paused(true);
        for (const auto &bytes : {scene.save(), scene.save_snapshot(),
                                  scene.save_indexed_snapshot()}) {
          auto n = std::uint32_t(bytes.size());
          for (unsigned k = 0; k < 4; ++k)
            out.put(char(n >> (8 * k)));
          out.write(reinterpret_cast<const char *>(bytes.data()),
                    std::streamsize(bytes.size()));
        }
      }
    if (!out)
      throw std::runtime_error("legacy bytes output");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
