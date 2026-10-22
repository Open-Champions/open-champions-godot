// Ground-item refusal until resource and bounds services are available.
// The 400 authored cases cover stale records, alignment and valid records.
#include "champions/script_builtins.hpp"
#include "champions/script_items.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace si = champions::simulation::rta::script_items;
namespace b = champions::simulation::rta::builtins;
namespace {
unsigned checks = 0, failed = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failed;
    if (failed < 20)
      std::cerr << "FAIL " << what << '\n';
  }
}
std::vector<std::uint8_t> bytes_of(const std::string &hex) {
  std::vector<std::uint8_t> out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  return out;
}
void load_record(si::Service &s, const std::vector<std::uint8_t> &item) {
  for (std::uint32_t k = 0; k < item.size(); ++k)
    s.call("stack_record_store", {k, std::uint32_t(item[k])});
}
bool refuses(si::Service &s, const std::string &name, const std::vector<b::Variables::InventoryValue> &args) {
  try {
    s.call(name, args);
  } catch (const si::Refused &) {
    return true;
  }
  return false;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: item-drops-tests VECTORS\n";
    return 2;
  }
  std::ifstream in(argv[1]);
  check(static_cast<bool>(in), "vectors open");
  std::string line;
  unsigned grounds = 0, faults = 0;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string kind, hex;
    words >> kind;
    if (kind == "groundfault") {
      std::string seed, block, item;
      words >> seed >> block >> item;
      auto memory = std::make_shared<b::Memory>();
      b::Variables v(memory);
      si::Service s(v);
      memory->word(si::G_RAND_SEED, std::uint32_t(std::stoul(seed, nullptr, 16)));
      load_record(s, bytes_of(item));
      const auto at = std::uint32_t(std::stoul(block, nullptr, 16));
      check(refuses(s, "ItemProp_ctor2", {at, std::array<std::uint32_t, 3>{0, 0, 0}}),
            "a stale record or a misaligned block refuses: " + line.substr(0, 40));
      check(memory->word(si::G_RAND_SEED) == std::uint32_t(std::stoul(seed, nullptr, 16)), "a refusal draws nothing");
      ++faults;
      continue;
    }
    check(kind == "ground", "vector kind");
    std::string seed, block, p0, p1, p2, item, definition, bar, after, image;
    words >> seed >> block >> p0 >> p1 >> p2 >> item >> definition >> bar >> after >> bar >> image;
    auto memory = std::make_shared<b::Memory>();
    b::Variables v(memory);
    si::Service s(v);
    const auto start = std::uint32_t(std::stoul(seed, nullptr, 16));
    memory->word(si::G_RAND_SEED, start);
    load_record(s, bytes_of(item));
    const auto at = std::uint32_t(std::stoul(block, nullptr, 16));
    const std::array<std::uint32_t, 3> pos{std::uint32_t(std::stoul(p0, nullptr, 16)),
                                           std::uint32_t(std::stoul(p1, nullptr, 16)),
                                           std::uint32_t(std::stoul(p2, nullptr, 16))};
    check(refuses(s, "ItemProp_ctor2", {at, pos}), "valid record refuses without resource and bounds services");
    check(memory->word(si::G_RAND_SEED) == start && v.draws == 0,
          "refused ground item draws no shared seed");
    check(v.world->objects.find(at) == v.world->objects.end() && s.counts.ground_items == 0,
          "refused ground item publishes no object");
    ++grounds;
  }
  check(grounds == 264 && faults == 136, "vector counts");

  // The pool model remains traced, but the item service refuses before
  // allocation because a complete ItemProp cannot be built.
  {
    auto memory = std::make_shared<b::Memory>();
    b::Variables v(memory);
    si::Service s(v);
    check(refuses(s, "pool_new", {400u}), "ground item refuses before pool allocation");
    std::uint32_t first = 0, last = 0;
    unsigned got = 0;
    for (;; ++got) {
      const auto block = v.world->pool.allocate(400u);
      if (!block)
        break;
      if (!first)
        first = block;
      last = block;
      if (got > 10000)
        break;
    }
    // The world's pool model (ref_core.Pool): the 0x500 class holds 512 blocks and a
    // full class spills into the 3000-byte class of 32, then returns 0.
    check(first != 0 && got == 512 + 32 && last > first, "pool_new(0x190) takes 512 + 32 blocks, then 0");
    check(first % 16 == 0, "blocks are 16-aligned");
    // After a refusal nothing was allocated and the seed stays.
    memory->word(si::G_RAND_SEED, 7);
    check(refuses(s, "ItemProp_ctor2", {first, std::array<std::uint32_t, 3>{0, 0, 0}}), "an empty record refuses");
    check(memory->word(si::G_RAND_SEED) == 7 && s.counts.ground_items == 0, "and draws nothing");
  }
  // Even a synthetically valid record cannot publish an incomplete ItemProp.
  {
    auto memory = std::make_shared<b::Memory>();
    b::Variables v(memory);
    si::Service s(v);
    std::vector<std::uint8_t> item(0x48, 0);
    item[0] = 0x00, item[1] = 0x10, item[2] = 0x10;
    load_record(s, item);
    const auto block = v.world->pool.allocate(400u);
    check(refuses(s, "ItemProp_ctor2", {block, std::array<std::uint32_t, 3>{0, 0, 0}}),
          "ground item refuses with unbound resources and bounds");
    check(s.counts.ground_items == 0 && v.world->objects.find(block) == v.world->objects.end(),
          "no incomplete ItemProp is registered");
  }
  if (failed) {
    std::cerr << failed << " of " << checks << " checks failed\n";
    return 1;
  }
  std::cout << "item-drops ok (" << checks << " checks)\n";
  return 0;
}
