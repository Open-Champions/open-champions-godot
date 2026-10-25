#include "champions/creature_anim/creature.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::creature_anim;
template <class T> void emit(T v) {
  std::cout << static_cast<std::int64_t>(v) << ' ';
}
template <class T> void array(const T &v) {
  for (auto x : v)
    emit(x);
}
void snapshot(Runtime &r) {
  const auto s = r.snapshot();
  emit(s.decoder.sim_frame);
  emit(s.decoder.cosmetic_seed);
  emit(s.draw_frame);
  emit(s.states.size());
  for (auto &v : s.states) {
    emit(v.id);
    emit(v.value.pose);
    emit(v.value.count);
    emit(v.value.capacity);
    emit(v.value.flags);
    array(v.value.tracks);
  }
  emit(s.decoder.tracks.size());
  for (auto &v : s.decoder.tracks) {
    auto &t = v.value;
    emit(t.clip);
    emit(t.slot);
    emit(t.weight);
    emit(t.rate);
    emit(t.life);
    emit(t.life_rate);
    emit(t.time);
    emit(t.flags);
    emit(t.sound_idx);
  }
  for (auto &v : s.decoder.slots) {
    emit(v.index);
    emit(v.clip);
    emit(v.cursor);
    emit(v.tick);
    emit(v.owner);
    emit(v.yaw);
    for (auto &j : v.joints) {
      emit(j.pt);
      emit(j.rt);
      array(j.pos);
      array(j.vel);
      array(j.quat);
      array(j.qvel);
    }
  }
  array(s.decoder.order);
  for (auto &p : s.poses) {
    emit(p.tag);
    emit(p.stamp);
    emit(p.joint_count);
    emit(p.owner);
    for (auto &m : p.mats)
      for (auto &row : m)
        array(row);
  }
  array(s.pose_order);
  for (auto &v : s.vu_mem)
    array(v);
  emit(r.sound_log.size());
  for (auto &v : r.sound_log) {
    emit(v.clip);
    emit(v.first);
    emit(v.end);
  }
  for (auto &v : r.cue_throttle)
    array(v);
  std::cout << '\n';
}
int main() {
  std::unique_ptr<Runtime> owner;
  Runtime *r = nullptr;
  std::unique_ptr<Creature> actor;
  std::string line;
  std::size_t n = 0;
  try {
    while (std::getline(std::cin, line)) {
      ++n;
      std::istringstream in(line);
      std::string op;
      in >> op;
      if (op.empty())
        continue;
      auto u = [&]() {
        std::uint64_t x = 0;
        if (!(in >> x))
          throw std::runtime_error("missing unsigned operand");
        return x;
      };
      auto raw = [&]() { return static_cast<Raw>(u()); };
      if (op == "init") {
        auto mode = u(), bias = u(), divn = u(), slots = u(), poses = u();
        set_fpu(static_cast<Fpu::Model>(mode), bias, divn);
        owner = std::make_unique<Runtime>(slots, Limits{}, poses);
        r = owner.get();
      } else if (op == "clip") {
        std::string hex;
        in >> hex;
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i < hex.size(); i += 2)
          bytes.push_back(static_cast<std::uint8_t>(
              std::stoul(hex.substr(i, 2), nullptr, 16)));
        r->add_clip(bytes);
      } else if (op == "state")
        r->add_state(static_cast<int>(u()));
      else if (op == "frames") {
        auto sim = raw(), draw = raw();
        r->set_sim_frame(sim);
        r->set_draw_frame(draw);
      } else if (op == "seed")
        r->set_cosmetic_seed(raw());
      else if (op == "push") {
        auto s = u(), c = u();
        auto f = raw(), w = raw(), rate = raw(), life = raw(), lr = raw();
        r->anim_push_with_weights(s, c, f, w, rate, life, lr);
      } else if (op == "time") {
        auto id = u();
        r->track(id).time = raw();
      } else if (op == "mask") {
        auto id = u();
        r->clip(id).joint_mask = u();
      } else if (op == "event") {
        auto id = u(), i = u();
        auto sec = raw(), mask = raw();
        r->clip(id).events.at(i) = {sec, static_cast<std::uint8_t>(mask)};
      } else if (op == "sound") {
        auto id = u(), group = u(), limit = u(), count = u();
        auto &c = r->clip(id);
        c.sound_group = group;
        r->group_limits[group] = limit;
        c.sound_times.emplace();
        for (std::size_t i = 0; i < count; ++i)
          c.sound_times->push_back(static_cast<std::int32_t>(raw()));
      } else if (op == "advance") {
        auto id = u();
        auto speed = raw();
        auto sound = u();
        auto m = r->advance(speed, id, sound);
        emit(m.events);
        emit(m.x);
        emit(m.y);
        emit(m.yaw);
        std::cout << '\n';
      } else if (op == "creature") {
        auto id = u();
        auto speed = raw(), f = raw(), scale = raw(), slow = raw(),
             active = raw(), x = raw(), y = raw(), yaw = raw();
        auto m = r->creature_step(speed, id, s16(f), scale, slow, active,
                                  {0, x, y, s16(yaw)});
        emit(m.events);
        emit(m.x);
        emit(m.y);
        emit(m.yaw);
        std::cout << '\n';
      } else if (op == "draw") {
        auto &p = r->draw_pose(u());
        emit(p.joint_count);
        for (int i = 0; i < p.joint_count; ++i)
          for (auto &row : p.mats[i])
            array(row);
        std::cout << '\n';
      } else if (op == "build") {
        auto id = u(), draw = u();
        auto p = r->build_pose(id, draw);
        emit(p ? 1 : 0);
        std::cout << '\n';
      } else if (op == "joint") {
        auto id = u();
        array(r->joint_matrix(id, static_cast<int>(u())));
        std::cout << '\n';
      } else if (op == "copy")
        r->anim_state_copy(u());
      else if (op == "start") {
        auto id = u(), c = u();
        auto flags = raw(), blend = raw();
        r->anim_start(blend, id, c, flags);
      } else if (op == "stop") {
        auto id = u(), c = u();
        auto blend = raw();
        r->anim_stop(blend, id, c);
      } else if (op == "jump")
        r->anim_jump_top_to_end(u());
      else if (op == "snap")
        r->anim_snap_top_to_end(u());
      else if (op == "random")
        r->anim_randomize_top_time(u());
      else if (op == "sync")
        r->anim_sync_to_sim_frame(u());
      else if (op == "below")
        r->anim_sync_top_to_below(u());
      else if (op == "clear")
        r->anim_clear(u());
      else if (op == "release")
        r->anim_release_caches(u());
      else if (op == "dtor")
        r->anim_state_dtor(u());
      else if (op == "reset")
        r->reset_stack();
      else if (op == "compose") {
        Quat q;
        Vec3 p;
        std::array<std::int16_t, 4> b{};
        for (auto &v : q)
          v = raw();
        for (auto &v : p)
          v = raw();
        for (unsigned i = 0; i < 3; ++i)
          b[i] = s16(raw());
        auto depth = raw();
        for (auto &row : r->compose_joint(q, p, b, s32(depth)))
          array(row);
        std::cout << '\n';
      } else if (op == "spider_init") {
        actor = std::make_unique<Creature>(0x1000, 8, 4);
        auto &m = actor->memory;
        Pairing p;
        p.definition = 0x502818;
        p.model = 0x9000;
        p.texture = 0xa000;
        for (const auto &v : r->snapshot().decoder.clips) {
          auto id = actor->runtime.add_clip(v.value);
          p.clips[0x6000 + 0x100 * (id - 1)] = id;
          if (id == 1)
            p.base = id;
        }
        m.poke(0x1024, 4, p.definition);
        m.poke(0x502824, 4, p.model);
        m.poke(0x50282c, 4, p.texture);
        m.poke(0x502854, 4, 0x6000);
        m.poke(0x502864, 4, 0x6100);
        for (unsigned i = 0; i < 4; ++i)
          m.poke(0x502834 + 8 * i, 4, 0x6200 + 0x100 * i);
        m.poke(0x1048, 4, 0x50fd40);
        m.poke(0x50fd40 + 27 * 8 + 4, 4, 0x2ab704);
        m.poke(0x50fd40 + 28 * 8 + 4, 4, 0x10beb8);
        m.poke(0x1300, 4, 100);
        actor->bind(p, {{{{16, 0, 0}, {0, 0, 127}, {{0, 255}}}}});
        actor->model.policy = creature_hp::CallbackPolicy::ReferenceZero;
        r = &actor->runtime;
      } else if (op == "poke") {
        auto a = u(), width = u(), v = u();
        actor->memory.poke(a, width, v);
      } else if (op == "spider_state")
        actor->change_state(raw());
      else if (op == "spider_construct")
        actor->construct(0, {0, 0, 0}, 0, 0);
      else if (op == "spider_touch")
        actor->touch(raw());
      else if (op == "spider_tick") {
        auto sim = raw(), draw = raw();
        actor->tick(sim, draw);
      } else if (op == "spider_snapshot") {
        snapshot(*r);
        const auto &m = actor->memory;
        emit(m.bytes().size());
        for (auto [a, b] : m.bytes()) {
          emit(a);
          emit(b);
        }
        emit(m.log().size());
        for (auto w : m.log()) {
          emit(w.address);
          emit(w.size);
          emit(w.value);
        }
        std::cout << '\n';
      } else if (op == "snapshot")
        snapshot(*r);
      else
        throw std::runtime_error("unknown opcode " + op);
    }
  } catch (const std::exception &e) {
    std::cerr << "line " << n << ": " << e.what() << '\n';
    return 1;
  }
}
