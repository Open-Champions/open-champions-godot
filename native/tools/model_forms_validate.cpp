// Private-file structural differential transport. Emits no input path or name.
#include "champions/model.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <type_traits>
namespace {
using namespace champions;
constexpr std::size_t MODEL_INPUT_LIMIT = 16u * 1024u * 1024u;
template <class T> void number(T value) {
  if constexpr (std::is_signed<T>::value)
    std::cout << static_cast<long long>(value);
  else
    std::cout << static_cast<unsigned long long>(value);
}
template <class C> void numbers(const C &values) {
  std::cout << '[';
  bool first = true;
  for (auto v : values) {
    if (!first)
      std::cout << ',';
    first = false;
    number(v);
  }
  std::cout << ']';
}
template <class C> void arrays(const C &values) {
  std::cout << '[';
  bool first = true;
  for (const auto &v : values) {
    if (!first)
      std::cout << ',';
    first = false;
    numbers(v);
  }
  std::cout << ']';
}
template <class C> void floats(const C &values) {
  std::cout << '[';
  bool first = true;
  for (float v : values) {
    std::uint32_t bits;
    static_assert(sizeof(v) == sizeof(bits));
    std::memcpy(&bits, &v, sizeof(bits));
    if (!first)
      std::cout << ',';
    first = false;
    number(bits);
  }
  std::cout << ']';
}
void model(const RtaModel &m) {
  std::cout << "{\"schema\":1,\"status\":\"decoded\",\"flags\":";
  number(m.flags);
  std::cout << ",\"hidden_mask\":";
  number(m.hidden_mask);
  std::cout << ",\"part_mask\":";
  number(m.part_mask);
  std::cout << ",\"attachment_slot\":";
  number(m.attachment_slot);
  std::cout << ",\"has_variants\":" << (m.has_variants ? "true" : "false")
            << ",\"alpha1\":";
  number(m.alpha1);
  std::cout << ",\"fix2\":";
  number(m.fix2);
  std::cout << ",\"variants\":[";
  for (std::size_t i = 0; i < m.variants.size(); ++i) {
    if (i)
      std::cout << ',';
    numbers(
        std::array<std::uint64_t, 2>{m.variants[i].show, m.variants[i].hide});
  }
  std::cout << ']';
  std::cout << ",\"attach_points\":[";
  for (std::size_t i = 0; i < m.attach_points.size(); ++i) {
    if (i)
      std::cout << ',';
    std::cout << '[';
    number(m.attach_points[i].joint);
    std::cout << ',';
    numbers(m.attach_points[i].matrix_raw);
    std::cout << ']';
  }
  std::cout << ']';
  std::cout << ",\"facing_points\":[";
  for (std::size_t i = 0; i < m.facing_points.size(); ++i) {
    if (i)
      std::cout << ',';
    const auto &p = m.facing_points[i];
    std::cout << '[';
    numbers(p.raw);
    std::cout << ',';
    numbers(p.position_raw);
    std::cout << ',';
    numbers(p.normal_raw);
    std::cout << ',';
    number(p.joint);
    std::cout << ']';
  }
  std::cout << ']';
  std::cout << ",\"facing_terminator_raw\":";
  if (m.facing_terminator_raw)
    numbers(*m.facing_terminator_raw);
  else
    std::cout << "null";
  std::cout << ",\"effect_segment_raw\":";
  if (m.effect_segment_raw)
    numbers(*m.effect_segment_raw);
  else
    std::cout << "null";
  std::cout << ",\"sample_points\":[";
  for (std::size_t i = 0; i < m.sample_points.size(); ++i) {
    if (i)
      std::cout << ',';
    std::cout << '[';
    numbers(m.sample_points[i].position_raw);
    std::cout << ',';
    number(m.sample_points[i].joint_size_raw);
    std::cout << ']';
  }
  std::cout << "],\"sample_block_raw\":";
  numbers(m.sample_block_raw);
  std::cout << ",\"meshes\":[";
  for (std::size_t i = 0; i < m.meshes.size(); ++i) {
    if (i)
      std::cout << ',';
    const auto &mesh = m.meshes[i];
    std::cout << "{\"index\":";
    number(mesh.index);
    std::cout << ",\"visible\":" << (mesh.visible_by_default ? "true" : "false")
              << ",\"joint\":";
    number(mesh.joint);
    std::cout << ",\"bounds_raw\":";
    numbers(mesh.bounds_raw);
    std::cout << ",\"declared_triangles\":";
    number(mesh.declared_triangles);
    std::cout << ",\"hull_points\":";
    arrays(mesh.hull_points);
    std::cout << ",\"packets\":[";
    for (std::size_t j = 0; j < mesh.packets.size(); ++j) {
      if (j)
        std::cout << ',';
      const auto &p = mesh.packets[j];
      std::cout << '[';
      number(p.program);
      std::cout << ',';
      number(p.stride);
      std::cout << ',';
      number(p.vertex_count);
      std::cout << ',';
      number(p.strip_count);
      std::cout << ',';
      number(p.first_corner);
      std::cout << ',';
      number(p.corner_count);
      std::cout << ',';
      arrays(p.gif_tags);
      std::cout << ']';
    }
    std::cout << "],\"corners\":[";
    for (std::size_t j = 0; j < mesh.corners.size(); ++j) {
      if (j)
        std::cout << ',';
      const auto &c = mesh.corners[j];
      std::cout << '[';
      floats(c.position);
      std::cout << ',';
      floats(c.normal);
      std::cout << ',';
      floats(c.uv_texels);
      std::cout << ',';
      numbers(c.skin.joints);
      std::cout << ',';
      numbers(c.skin.weights);
      std::cout << ',';
      number(c.skin.count);
      std::cout << ',';
      numbers(c.position_raw);
      std::cout << ',';
      numbers(c.normal_raw);
      std::cout << ',';
      numbers(c.uv_raw);
      std::cout << ',';
      if (c.shell_normal_raw)
        numbers(*c.shell_normal_raw);
      else
        std::cout << "null";
      std::cout << ']';
    }
    std::cout << "]}";
  }
  std::cout << "]}\n";
}
int run(const std::filesystem::path &path) {
  try {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
      std::cout << "{\"schema\":1,\"status\":\"transport_error\",\"reason\":"
                   "\"input\"}\n";
      return 3;
    }
    auto length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length) > MODEL_INPUT_LIMIT) {
      std::cout
          << "{\"schema\":1,\"status\":\"refused\",\"reason\":\"size\"}\n";
      return 2;
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!data.empty())
      input.read(reinterpret_cast<char *>(data.data()),
                 std::streamsize(data.size()));
    if (!input) {
      std::cout << "{\"schema\":1,\"status\":\"transport_error\",\"reason\":"
                   "\"read\"}\n";
      return 3;
    }
    const auto decoded = decodeRtaModel({data.data(), data.size()});
    model(decoded);
    if (!std::cout)
      return 3;
    return 0;
  } catch (const FormatError &) {
    std::cout
        << "{\"schema\":1,\"status\":\"refused\",\"reason\":\"format\"}\n";
    return 2;
  } catch (const std::exception &) {
    std::cout << "{\"schema\":1,\"status\":\"transport_error\",\"reason\":"
                 "\"exception\"}\n";
    return 3;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
  if (argc != 2) {
    std::cout << "{\"schema\":1,\"status\":\"transport_error\",\"reason\":"
                 "\"arguments\"}\n";
    return 3;
  }
  return run(std::filesystem::path(argv[1]));
}
#else
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << "{\"schema\":1,\"status\":\"transport_error\",\"reason\":"
                 "\"arguments\"}\n";
    return 3;
  }
  return run(std::filesystem::path(argv[1]));
}
#endif
