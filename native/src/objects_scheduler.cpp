// Authored from the pinned RTA ref_objects.py contract; see
// objects_lifetime.cpp.
#include "objects_internal.hpp"
namespace champions::simulation::rta::objects {
using namespace detail;
std::optional<std::size_t> TaskList::add(std::uint32_t fn,
                                         std::int64_t priority,
                                         const std::string &name,
                                         std::int64_t flags) {
  bounded(name.size());
  for (const auto &t : tasks)
    if (t.fn == fn)
      return {};
  if (tasks.size() >= MAX_TASKS)
    throw ObjectError("task table full");
  auto index = tasks.size();
  while (index && priority < tasks[index - 1].priority)
    --index;
  if (cursor >= std::int64_t(index))
    cursor = step(cursor, 1);
  tasks.insert(tasks.begin() + std::ptrdiff_t(index),
               Task{fn, priority, name, flags});
  return index;
}
std::optional<std::size_t> TaskList::remove(std::uint32_t fn) {
  for (std::size_t i = 0; i < tasks.size(); ++i)
    if (tasks[i].fn == fn) {
      if (cursor >= std::int64_t(i))
        cursor = step(cursor, -1);
      tasks.erase(tasks.begin() + std::ptrdiff_t(i));
      return i;
    }
  return {};
}
bool TaskList::is_running(const std::string &name) const {
  return std::any_of(tasks.begin(), tasks.end(),
                     [&](const Task &t) { return t.name == name; });
}
void TaskList::call(std::int64_t exclude, std::int64_t include,
                    const std::function<void(std::uint32_t)> &invoke) {
  cursor = 0;
  if (tasks.empty())
    return;
  std::size_t work = 0;
  for (;;) {
    budget(work);
    bounded(tasks.size(), MAX_TASKS);
    auto index = cursor < 0 ? std::int64_t(tasks.size()) + cursor : cursor;
    if (index < 0 || std::uint64_t(index) >= tasks.size())
      throw ObjectError("task cursor out of range");
    auto task = tasks[std::size_t(index)];
    if (!(task.flags & exclude) && (include == -1 || (task.flags & include))) {
      if (!invoke)
        throw ObjectError("missing task handler");
      auto fn = invoke;
      fn(task.fn);
    }
    cursor = step(cursor, 1);
    if (cursor >= std::int64_t(tasks.size()))
      return;
  }
}
int World::is_active(const GameObject &o, std::int64_t radius) const {
  if (o.flags & FLAG_FAR_ACTIVE)
    radius = 44000;
  auto r = std::uint32_t(radius);
  auto limit = from_int(r * r);
  auto test = [&](const Vec3 &c) {
    auto x = sub(o.pos[0], c[0]), y = sub(o.pos[1], c[1]),
         z = sub(o.pos[2], c[2]);
    return lt(add(add(mul(x, x), mul(y, y)), mul(z, z)), limit);
  };
  if (cull_by_camera)
    return test(camera_focus) ? 1 : 0;
  bounded(centres.size(), MAX_RETAINED);
  for (const auto &c : centres)
    if (test(c))
      return 1;
  return 0;
}
int World::update_active(GameObject &o, std::int64_t was, std::int64_t radius) {
  auto active = is_active(o, radius);
  if (active != was) {
    if (active)
      run_every_frame(o);
    else
      run_every_60(o);
  }
  return active;
}
std::array<std::size_t, 16> World::reap_cells() const {
  auto value = (std::uint32_t(frame) << 4) & 0x0fffffff;
  auto col = value % 120, row = (value / 120) % 120;
  std::array<std::size_t, 16> out{};
  for (auto &n : out) {
    if (++col >= 120) {
      col = 0;
      if (++row >= 120)
        row = 0;
    }
    n = row * 120 + col;
  }
  return out;
}
std::vector<GameObject *> World::reap() {
  std::vector<GameObject *> victims;
  std::size_t work = 0;
  for (auto index : reap_cells()) {
    auto *n = grid_b[index].next;
    while (n) {
      budget(work);
      auto &o = checked_object(n->obj);
      if (o.flags & FLAG_EXPENDABLE) {
        bool spared = false;
        if (!network_mode && pool_free_count >= 200) {
          auto dx = sub(o.pos[0], camera_focus[0]),
               dy = sub(o.pos[1], camera_focus[1]),
               dz = sub(o.pos[2], camera_focus[2]);
          spared =
              !lt(0x48610000, add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
        }
        if (!spared) {
          victims.push_back(&o);
          call_destroy(o, 3);
          break;
        }
      }
      n = n->next;
    }
  }
  return victims;
}
void World::run_objects() {
  flush_delete_list();
  if (!(std::uint64_t(frame) & 63))
    pool_free_count = std::int64_t(pool.count_free());
  if (pool_free_count < 300)
    reap();
  if (frozen())
    return;
  auto run = [this](GameObject &o) {
    if (!(o.flags & FLAG_NOT_RUN))
      call_run(o);
  };
  walk_list(run_active, exclude_mask, run);
  walk_list(run15[mod(frame, 15)], exclude_mask, run);
  walk_list(run60[mod(frame, 60)], exclude_mask, run);
}
void World::for_each_grid_object(const std::function<void(GameObject &)> &fn) {
  auto call = [&](GameObject &o) {
    if (!fn)
      throw ObjectError("missing grid walker handler");
    auto copy = fn;
    copy(o);
  };
  if (off_grid.next)
    walk_list(off_grid, exclude_mask, call);
  for (auto &c : grid)
    if (c.next)
      walk_list(c, exclude_mask, call);
  for (auto &c : grid_b)
    if (c.next)
      walk_list(c, exclude_mask, call);
}
void World::for_each_run_object(const std::function<void(GameObject &)> &fn) {
  auto call = [&](GameObject &o) {
    if (!fn)
      throw ObjectError("missing run walker handler");
    auto copy = fn;
    copy(o);
  };
  walk_list(run_active, exclude_mask, call);
  for (auto &c : run15)
    walk_list(c, exclude_mask, call);
  for (auto &c : run60)
    walk_list(c, exclude_mask, call);
}
void World::delete_all_objects() {
  pool_free_count = 100000;
  cursor60 = cursor15 = 0;
  tearing_down = 1;
  flush_delete_list();
  auto destroy = [this](GameObject &o) { call_destroy(o, 3); };
  for (auto &c : grid)
    if (c.next)
      walk_list(c, 0, destroy);
  for (auto &c : grid_b)
    if (c.next)
      walk_list(c, 0, destroy);
  if (off_grid.next)
    walk_list(off_grid, 0, destroy);
  names.clear();
  tearing_down = 0;
}
void World::tasks_run(std::int64_t count,
                      const std::function<void(std::uint32_t)> &invoke) {
  if (network_mode)
    menu_lockout = 0;
  std::array<std::pair<std::int64_t, std::int64_t>, 4> saved{};
  for (std::size_t i = 0; i < 4; ++i)
    saved[i] = {pads[i].pressed, pads[i].edge2};
  auto n = s32(std::uint32_t(count));
  if (n > std::int64_t(MAX_WORK))
    throw ResourceLimit("task tick budget exceeded");
  for (std::int32_t i = 0; i < n; ++i) {
    tasks.call(1, -1, invoke);
    if (!network_mode && !frozen() && !warmup_frames)
      frame = s32(std::uint32_t(frame) + 1);
    for (auto &p : pads)
      p.pressed = p.edge2 = 0;
    finish_tick();
  }
  if (s32(std::uint32_t(menu_lockout)) > 0 && !menu_lockout_hold) {
    bool held = false;
    for (const auto &p : pads)
      if (std::uint64_t(p.held) & 0xff0fffff)
        held = true;
    menu_lockout = held ? step(menu_lockout, -1) : 0;
  }
  for (std::size_t i = 0; i < 4; ++i) {
    pads[i].pressed = saved[i].first;
    pads[i].edge2 = saved[i].second;
  }
}
void World::set_object_vis(GameObject *dialog, std::int64_t camera_mode,
                           bool cutscene,
                           const std::vector<GameObject *> &players) {
  if (dialog) {
    cull_by_camera = 0;
    centres = {dialog->pos};
    return;
  }
  cull_by_camera = camera_mode != 1 ? 1 : 0;
  if (network_mode && !cutscene)
    cull_by_camera = 0;
  bounded(players.size(), MAX_RETAINED);
  centres.clear();
  for (auto *o : players)
    if (o)
      centres.push_back(o->pos);
}
int World::is_near_view(const Vec3 &p, Raw reach,
                        const std::vector<GameObject *> &players) const {
  auto test = [&](const Vec3 &c) {
    return le(p[0], add(c[0], reach)) && le(sub(c[0], reach), p[0]) &&
           le(p[1], add(c[1], reach)) && le(sub(c[1], reach), p[1]);
  };
  if (cull_by_camera)
    return test(camera_focus) ? 1 : 0;
  bounded(players.size(), MAX_RETAINED);
  for (auto *o : players)
    if (o && test(o->pos))
      return 1;
  return 0;
}
std::tuple<std::vector<std::size_t>, std::vector<std::size_t>, std::int64_t>
World::alive_players(const std::vector<GameObject *> &players,
                     const std::function<bool(std::size_t)> &is_local) {
  bounded(players.size(), MAX_RETAINED);
  std::vector<std::size_t> alive, local;
  for (std::size_t i = 0; i < players.size(); ++i)
    if (players[i] && !(players[i]->flags & FLAG_DEAD)) {
      alive.push_back(i);
      if (!is_local)
        throw ObjectError("missing local slot handler");
      auto fn = is_local;
      if (fn(i))
        local.push_back(i);
    }
  auto first = local.empty() ? -1 : std::int64_t(local.front());
  return {std::move(alive), std::move(local), first};
}
} // namespace champions::simulation::rta::objects
