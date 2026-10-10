#include "champions/core_arithmetic.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace rta = champions::simulation::rta;
namespace {
std::uint64_t evaluate(const std::string &op, std::uint64_t a,
                       std::uint64_t b) {
  const auto x = static_cast<rta::Raw>(a), y = static_cast<rta::Raw>(b);
#define BINARY(name)                                                           \
  if (op == #name)                                                             \
  return rta::name(x, y)
#define UNARY(name)                                                            \
  if (op == #name)                                                             \
  return rta::name(x)
#define SIGNED(name)                                                           \
  if (op == #name)                                                             \
  return static_cast<std::uint32_t>(rta::name(x))
#define DOUBLE_BINARY(name)                                                    \
  if (op == #name)                                                             \
  return rta::name(a, b)
  BINARY(add);
  BINARY(sub);
  BINARY(mul);
  BINARY(div);
  BINARY(rsqrt);
  BINARY(fmax);
  BINARY(fmin);
  BINARY(lt);
  BINARY(le);
  BINARY(eq);
  UNARY(sqrt);
  UNARY(from_int);
  UNARY(to_uint);
  UNARY(neg);
  UNARY(fabs);
  SIGNED(to_int);
  SIGNED(round_half_away);
  SIGNED(s16);
  SIGNED(s32);
  UNARY(i2d);
  UNARY(f2d);
  DOUBLE_BINARY(dadd);
  DOUBLE_BINARY(dsub);
  DOUBLE_BINARY(dmul);
  DOUBLE_BINARY(ddiv);
  DOUBLE_BINARY(fmod);
  if (op == "dcmp")
    return static_cast<std::uint32_t>(rta::dcmp(a, b));
  if (op == "d2i")
    return static_cast<std::uint32_t>(rta::d2i(a));
  if (op == "d2f")
    return rta::d2f(a);
  throw std::runtime_error("unknown vector operation: " + op);
#undef BINARY
#undef UNARY
#undef SIGNED
#undef DOUBLE_BINARY
}
rta::Fpu::Model model(const std::string &name) {
  if (name == "ps2")
    return rta::Fpu::Model::Ps2;
  if (name == "ps2g1")
    return rta::Fpu::Model::Ps2g1;
  if (name == "chop")
    return rta::Fpu::Model::Chop;
  if (name == "nearest")
    return rta::Fpu::Model::Nearest;
  throw std::runtime_error("unknown FPU model: " + name);
}
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error(
          "usage: core-arithmetic-tests VECTORS.tsv [TRACE.tsv]");
    std::ifstream input(argv[1]);
    if (!input)
      throw std::runtime_error("cannot open vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2]);
      if (!trace)
        throw std::runtime_error("cannot create trace");
    }
    std::string line;
    std::uint64_t digest = UINT64_C(14695981039346656037);
    std::size_t count = 0, failures = 0;
    while (std::getline(input, line)) {
      if (line.empty() || line[0] == '#')
        continue;
      std::istringstream row(line);
      std::string setting, op, extra;
      int bias = 0, nearest = 0;
      std::uint64_t a = 0, b = 0, expected = 0;
      if (!(row >> setting >> bias >> nearest >> op >> std::hex >> a >> b >>
            expected) ||
          (row >> extra) || bias < 0 || bias > 1 || nearest < 0 || nearest > 1)
        throw std::runtime_error("malformed vector line");
      rta::set_fpu(model(setting), bias != 0, nearest != 0);
      const auto actual = evaluate(op, a, b);
      ++count;
      if (actual != expected) {
        if (failures < 20)
          std::cerr << "mismatch " << count << ' ' << setting << ' ' << bias
                    << ' ' << nearest << ' ' << op << ' ' << std::hex << a
                    << ' ' << b << " expected=" << expected
                    << " actual=" << actual << std::dec << '\n';
        ++failures;
      }
      for (unsigned shift = 0; shift < 64; shift += 8) {
        digest ^= (actual >> shift) & 255u;
        digest *= UINT64_C(1099511628211);
      }
      if (trace)
        trace << count << '\t' << op << '\t' << std::hex << std::setw(16)
              << std::setfill('0') << actual << std::dec << '\n';
    }
    require(input.eof(), "error reading vectors");
    require(count > 0, "empty vector file");
    if (failures)
      throw std::runtime_error(std::to_string(failures) +
                               " exact-bit mismatches");
    // Independent, legible contract anchors; vectors also test every instance
    // operation through wrappers.
    rta::Fpu ps2, nearest(rta::Fpu::Model::Nearest);
    require(ps2.div(rta::ONE, 0x40400000u) == 0x3eaaaaaau, "truncating third");
    require(nearest.div(rta::ONE, 0x40400000u) == 0x3eaaaaabu, "nearest third");
    rta::set_fpu(rta::Fpu::Model::Nearest, true, true);
    require(ps2.from_int(16777217) == 0x4b800000u,
            "explicit Fpu independent of context");
    require(rta::f2d(0x7f800000u) == UINT64_C(0x47f0000000000000),
            "single exponent255 is finite");
    require(rta::dadd(rta::D_SIGN, rta::D_SIGN) == rta::D_SIGN,
            "double negative zero");
    require(rta::fmod(3, 2) == 1, "fmod retains subnormals");
    bool worker_default = false, worker_changed = false;
    std::thread worker([&] {
      worker_default = rta::div(rta::ONE, 0x40400000u) == 0x3eaaaaaau;
      rta::set_fpu(rta::Fpu::Model::Chop);
      worker_changed = rta::from_int(16777217) == 0x4b800000u;
    });
    worker.join();
    require(worker_default && worker_changed &&
                rta::div(rta::ONE, 0x40400000u) == 0x3eaaaaabu,
            "thread contexts must remain independent");
    bool rejected = false;
    try {
      rta::Fpu bad(static_cast<rta::Fpu::Model>(99));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "invalid model must reject");
    {
      rta::ScopedFpu fixed(ps2);
      require(rta::div(rta::ONE, 0x40400000u) == 0x3eaaaaaau,
              "compound operations select their explicit contract");
      {
        rta::ScopedFpu nested(nearest);
        require(rta::div(rta::ONE, 0x40400000u) == 0x3eaaaaabu,
                "nested scope selects its own contract");
      }
      require(rta::div(rta::ONE, 0x40400000u) == 0x3eaaaaaau,
              "nested scope restores the previous contract");
    }
    try {
      rta::ScopedFpu fixed(ps2);
      throw std::runtime_error("authored failure inside scoped operation");
    } catch (const std::runtime_error &) {
    }
    require(rta::div(rta::ONE, 0x40400000u) == 0x3eaaaaabu,
            "scope restores the caller after exceptions");
    std::cout
        << count
        << " exact-bit vectors and 12 independent checks passed; trace_fnv1a64="
        << std::hex << std::setw(16) << std::setfill('0') << digest << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
