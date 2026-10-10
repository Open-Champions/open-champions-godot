#include "champions/core_state.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace champions::simulation::rta;
namespace {
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> out;
  std::istringstream in(s);
  std::string x;
  while (std::getline(in, x, separator))
    out.push_back(x);
  return out;
}
std::uint64_t number(const std::string &s) {
  std::size_t end = 0;
  auto value = std::stoull(s, &end);
  if (end != s.size())
    throw std::runtime_error("invalid number");
  return value;
}
std::string hex(std::uint64_t v) {
  std::ostringstream out;
  out << std::hex << v;
  return out.str();
}
template <class T> std::string numbers(const T &values) {
  std::string out;
  for (auto v : values) {
    if (!out.empty())
      out += ",";
    out += std::to_string(v);
  }
  return out.empty() ? "-" : out;
}
struct Runner {
  std::map<std::string, Lcg> rng;
  LibcRand libc;
  bool has_libc = false;
  Pool pool;
  std::unique_ptr<HandlePool> handles;
  std::array<Cell, 4> cells;
  std::array<Object, 8> objects;
  std::map<unsigned, std::unique_ptr<Iterator>> iterators;
  std::string rng_state() {
    std::map<std::string, std::uint64_t> all;
    for (const auto &pair : rng)
      all[pair.first] = pair.second.state;
    if (has_libc)
      all["libc"] = libc.state;
    std::string out;
    for (auto pair : all) {
      if (!out.empty())
        out += ';';
      out += pair.first + '=' + hex(pair.second);
    }
    return out.empty() ? "-" : out;
  }
  std::string pool_state() {
    std::string out = "base=" + numbers(pool.base) +
                      ";head=" + numbers(pool.head) +
                      ";last=" + std::to_string(pool.last) +
                      ";end=" + std::to_string(pool.end) + ";links=";
    bool first = true;
    for (auto pair : pool.link) {
      if (!first)
        out += ',';
      first = false;
      out += std::to_string(pair.first) + ':' + std::to_string(pair.second);
    }
    return out;
  }
  std::string token(const Link *link) {
    if (!link)
      return "-";
    if (link->kind == LinkKind::Node)
      return "n" + std::to_string(static_cast<const Node *>(link)->index);
    for (unsigned i = 0; i < cells.size(); ++i)
      if (link == &cells[i])
        return "c" + std::to_string(i);
    for (const auto &pair : iterators)
      if (link == pair.second.get())
        return "i" + std::to_string(pair.first);
    throw std::runtime_error("unknown link");
  }
  static std::string token(const Object *obj) {
    return obj ? "o" + std::to_string(obj->id) : "-";
  }
  Link *link(const std::string &s) {
    if (s == "-")
      return nullptr;
    auto i = number(s.substr(1));
    switch (s[0]) {
    case 'n':
      return handles->nodes().at(i);
    case 'c':
      return &cells.at(i);
    case 'i':
      return iterators.at(unsigned(i)).get();
    default:
      throw std::runtime_error("invalid link token");
    }
  }
  Cell &cell(const std::string &s) {
    if (s.at(0) != 'c')
      throw std::runtime_error("expected cell");
    return cells.at(number(s.substr(1)));
  }
  Object *object(const std::string &s) {
    if (s == "-")
      return nullptr;
    if (s.at(0) != 'o')
      throw std::runtime_error("expected object");
    return &objects.at(number(s.substr(1)));
  }
  std::string fields(const Link *n) {
    return token(n->next) + ',' + token(n->prev) + ',' + token(n->partner) +
           ',' + token(n->obj);
  }
  std::string handle_state() {
    std::string out = "free=" + token(handles->free);
    for (unsigned i = 0; i < cells.size(); ++i)
      out += ";c" + std::to_string(i) + '=' + token(cells[i].next);
    for (const auto *n : handles->nodes())
      out += ';' + token(n) + '=' + fields(n) + ',' + std::to_string(n->tag);
    for (const auto &pair : iterators)
      out += ";i" + std::to_string(pair.first) + '=' +
             fields(pair.second.get()) + ',' +
             std::to_string(pair.second->mask);
    for (const auto &o : objects)
      out += ";o" + std::to_string(o.id) + '=' + std::to_string(o.flags);
    return out;
  }
  std::string objlist(const std::vector<Object *> &list) {
    std::string out;
    for (auto *obj : list) {
      if (!out.empty())
        out += ',';
      out += token(obj);
    }
    return out.empty() ? "-" : out;
  }
  std::string run_rng(const std::string &op,
                      const std::vector<std::string> &a) {
    if (op == "config") {
      const std::map<std::string, Fpu::Model> modes{
          {"ps2", Fpu::Model::Ps2},
          {"ps2g1", Fpu::Model::Ps2g1},
          {"chop", Fpu::Model::Chop},
          {"nearest", Fpu::Model::Nearest}};
      set_fpu(modes.at(a.at(0)), number(a.at(1)) != 0, number(a.at(2)) != 0);
      rng.clear();
      has_libc = false;
      return "-";
    }
    if (op == "new") {
      const auto &kind = a.at(0);
      const auto &name = a.at(1);
      auto seed = number(a.at(2));
      if (kind == "LibcRand") {
        libc = LibcRand(seed);
        has_libc = true;
      } else if (kind == "ObjectRandom" || kind == "SpellRandom") {
        SharedRandom shared(rng.at("shared").state);
        if (kind == "ObjectRandom")
          rng.insert_or_assign(name, ObjectRandom(shared));
        else
          rng.insert_or_assign(name, SpellRandom(shared));
        rng.at("shared").state = shared.state;
      } else if (kind == "SharedRandom")
        rng.insert_or_assign(name, SharedRandom(std::uint32_t(seed)));
      else if (kind == "CosmeticRandom")
        rng.insert_or_assign(name, CosmeticRandom(std::uint32_t(seed)));
      else if (kind == "EffectRandom")
        rng.insert_or_assign(name, EffectRandom(std::uint32_t(seed)));
      else if (kind == "Lcg")
        rng.insert_or_assign(name, Lcg(std::uint32_t(seed)));
      else
        throw std::runtime_error("unknown RNG type");
      return "-";
    }
    if (op == "seed") {
      libc.seed(std::uint32_t(number(a.at(1))));
      return "-";
    }
    if (op == "rand")
      return std::to_string(libc.rand());
    auto &r = rng.at(a.at(0));
    if (op == "draw")
      return std::to_string(r.draw());
    if (op == "unit")
      return std::to_string(r.unit());
    if (op == "signed_unit")
      return std::to_string(r.signed_unit());
    if (op == "in_unit_sphere")
      return numbers(r.in_unit_sphere());
    throw std::runtime_error("unknown RNG op");
  }
  std::string run_pool(const std::string &op,
                       const std::vector<std::string> &a) {
    auto arg = [&](unsigned i) { return std::uint32_t(number(a.at(i))); };
    try {
      if (op == "reset")
        pool.init();
      else if (op == "new")
        return std::to_string(pool.allocate(arg(0)));
      else if (op == "batch") {
        std::vector<std::uint32_t> out;
        for (unsigned i = 0; i < arg(1); ++i)
          out.push_back(pool.allocate(arg(0)));
        return numbers(out);
      } else if (op == "free")
        pool.free(arg(0));
      else if (op == "class")
        return std::to_string(pool.class_of(arg(0)));
      else if (op == "list")
        return numbers(pool.free_list(arg(0)));
      else if (op == "count")
        return std::to_string(pool.count_free());
      else if (op == "erase")
        pool.link.erase(arg(0));
      else
        throw std::runtime_error("unknown pool op");
      return "-";
    } catch (const PoolError &) {
      return "!PoolError";
    } catch (const PoolMissingLink &) {
      return "!KeyError";
    } catch (const std::out_of_range &) {
      return "!IndexError";
    }
  }
  std::string run_handle(const std::string &op,
                         const std::vector<std::string> &a) {
    auto arg = [&](unsigned i) { return std::uint32_t(number(a.at(i))); };
    try {
      if (op == "reset") {
        iterators.clear();
        handles = std::make_unique<HandlePool>(arg(0));
        for (auto &c : cells)
          c.next = nullptr;
        for (unsigned i = 0; i < objects.size(); ++i)
          objects[i] = {i, 0};
      } else if (op == "init")
        handles->init();
      else if (op == "add")
        return token(handles->add(cell(a.at(0)), object(a.at(1))));
      else if (op == "insert")
        return token(
            handles->insert(cell(a.at(0)), link(a.at(1)), object(a.at(2))));
      else if (op == "link")
        return token(
            handles->link(cell(a.at(0)), cell(a.at(1)), object(a.at(2))));
      else if (op == "both")
        handles->link_both(cell(a.at(0)), object(a.at(1)), cell(a.at(2)),
                           object(a.at(3)));
      else if (op == "remove")
        return token(handles->remove(link(a.at(0))));
      else if (op == "clear")
        handles->clear(cell(a.at(0)));
      else if (op == "take")
        return token(handles->take());
      else if (op == "release")
        handles->release(link(a.at(0)));
      else if (op == "push")
        handles->push(*link(a.at(0)), link(a.at(1)));
      else if (op == "unlink")
        handles->unlink(link(a.at(0)));
      else if (op == "tag")
        link(a.at(0))->tag = arg(1);
      else if (op == "flags")
        object(a.at(0))->flags = arg(1);
      else if (op == "count")
        return std::to_string(handles->count_free());
      else if (op == "items")
        return objlist(handles->items(cell(a.at(0))));
      else if (op == "walk")
        return objlist(walk(cell(a.at(0)), arg(1)));
      else if (op == "begin")
        iterators.insert_or_assign(
            arg(0), std::make_unique<Iterator>(cell(a.at(1)), arg(2)));
      else if (op == "current")
        return token(iterators.at(arg(0))->current());
      else if (op == "done")
        return iterators.at(arg(0))->done() ? "1" : "0";
      else if (op == "advance")
        iterators.at(arg(0))->advance();
      else if (op == "end")
        iterators.at(arg(0))->end();
      else
        throw std::runtime_error("unknown handle op");
      return "-";
    } catch (const HandleError &) {
      return "!HandleError";
    }
  }
};
} // namespace
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: core_state_tests VECTOR_FILE [ACTUAL_TRACE]\n";
    return 2;
  }
  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "cannot open vector file\n";
    return 2;
  }
  std::ofstream trace;
  if (argc == 3) {
    trace.open(argv[2], std::ios::binary);
    if (!trace) {
      std::cerr << "cannot open trace output\n";
      return 2;
    }
  }
  std::uint64_t digest = UINT64_C(14695981039346656037);
  Runner runner;
  std::string line;
  std::size_t lines = 0, checks = 0;
  std::map<std::string, std::size_t> domains;
  try {
    while (std::getline(input, line)) {
      ++lines;
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      auto cols = split(line, '\t');
      if (cols.size() != 5)
        throw std::runtime_error("expected five columns");
      auto args = split(cols[2], ',');
      std::string result, state;
      if (cols[0] == "rng") {
        result = runner.run_rng(cols[1], args);
        state = runner.rng_state();
      } else if (cols[0] == "pool") {
        result = runner.run_pool(cols[1], args);
        state = runner.pool_state();
      } else if (cols[0] == "handle") {
        result = runner.run_handle(cols[1], args);
        state = runner.handle_state();
      } else
        throw std::runtime_error("unknown domain");
      if (result != cols[3])
        throw std::runtime_error("result mismatch for " + cols[0] + "/" +
                                 cols[1] + ": got " + result + ", expected " +
                                 cols[3]);
      if (state != cols[4]) {
        std::size_t offset = 0;
        while (offset < state.size() && offset < cols[4].size() &&
               state[offset] == cols[4][offset])
          ++offset;
        throw std::runtime_error(
            "state mismatch for " + cols[0] + "/" + cols[1] + " at byte " +
            std::to_string(offset) + ": got " + state.substr(offset, 80) +
            ", expected " + cols[4].substr(offset, 80));
      }
      const auto actual = cols[0] + "\t" + cols[1] + "\t" + cols[2] + "\t" +
                          result + "\t" + state + "\n";
      for (unsigned char byte : actual) {
        digest ^= byte;
        digest *= UINT64_C(1099511628211);
      }
      if (trace.is_open()) {
        trace << actual;
        if (!trace)
          throw std::runtime_error("trace output failed");
      }
      ++checks;
      ++domains[cols[0]];
    }
    if (input.bad())
      throw std::runtime_error("vector read failed");
    if (domains["rng"] < 100 || domains["pool"] < 30 || domains["handle"] < 100)
      throw std::runtime_error("missing or incomplete state vectors");
    if (trace.is_open()) {
      trace.flush();
      if (!trace)
        throw std::runtime_error("trace output flush failed");
    }
    std::cout << checks
              << " complete state traces passed; fnv1a64=" << hex(digest)
              << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "line " << lines << ": " << e.what() << '\n';
    return 1;
  }
}
