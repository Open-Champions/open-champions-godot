// Included in binding.cpp's adapter namespace. Simulation stays in native/.
class ChampionsAnimationDispatch : public RefCounted {
  GDCLASS(ChampionsAnimationDispatch, RefCounted)
  struct Owner {
    std::shared_ptr<A::Environment> env;
    A::StateId state = 0;
    A::ClipId base = 0;
    std::map<R::Raw, R::Raw> entries;
    std::map<std::string, R::Raw> names;
    A::Mesh mesh;
    std::string cls;
    R::Raw current = 0, model = 0, texture = 0;
    double units = 1;
    R::Raw blend = 0, previous = 0;
    bool fidget = false;
    std::optional<R::Raw> tick;
    ~Owner() {
      if (env && state)
        try {
          env->runtime.anim_state_dtor(state);
        } catch (...) {
        }
    }
    A::ClipId resource(R::Raw r) const {
      auto i = env->resources.find(r);
      if (i == env->resources.end())
        throw A::Unsupported("unregistered dispatch resource");
      if (base) {
        const auto &expected = env->runtime.clip(base);
        const auto &actual = env->runtime.clip(i->second);
        if (actual.depth != expected.depth || actual.bind != expected.bind ||
            actual.joint_count != expected.joint_count)
          throw A::Unsupported("resource is outside the paired skeleton");
      }
      return i->second;
    }
    A::ClipId entry(R::Raw n) const {
      auto i = entries.find(n);
      if (i == entries.end())
        throw A::Unsupported("missing resolved definition entry");
      return i->second ? resource(i->second) : 0;
    }
  };
  std::unique_ptr<Owner> owner;
  Callable virtual_handler;
  ~ChampionsAnimationDispatch() { virtual_handler = Callable(); }
  bool busy = false;
  unsigned virtual_depth = 0;
  Owner &get() {
    if (!owner)
      throw A::ValueError("configure dispatch first");
    return *owner;
  }
  template <class Fn> Dictionary attempt(Fn fn) {
    Dictionary d;
    if (busy) {
      d["ok"] = false;
      d["error"] = "reentrant dispatch operation";
      return d;
    }
    busy = true;
    try {
      fn(d);
      d["ok"] = true;
    } catch (const std::exception &e) {
      d["ok"] = false;
      d["error"] = String(e.what());
    } catch (...) {
      d["ok"] = false;
      d["error"] = "unknown native exception";
    }
    busy = false;
    return d;
  }
  static R::Raw raw(Variant v) {
    if (v.get_type() != Variant::INT)
      throw A::ValueError("raw32 integer required");
    int64_t n = v;
    if (n < 0 || n > UINT32_MAX)
      throw A::ValueError("outside raw32");
    return n;
  }
  static A::Choice dispatch_choice(Dictionary d) {
    A::Choice c;
    c.random = raw(d.get("random", 0));
    if (d.has("field") && d.has("return"))
      throw A::Unsupported("combined conditions are unknown");
    if (d.has("field"))
      c.condition = A::Condition{false, raw(d["field"]), raw(d["value"])};
    if (d.has("return"))
      c.condition = A::Condition{true, 0, raw(d["return"])};
    return c;
  }
  A::DispatchContext context() {
    auto &o = get();
    A::DispatchContext c{
        o.env->runtime, o.state, [&o](R::Raw n) { return o.entry(n); }, {}};
    // Review finding 8: a handler whose object was freed is dropped and refused
    // by name, never left to look like "no handler installed".
    if (!virtual_handler.is_null() && !virtual_handler.is_valid()) {
      virtual_handler = Callable();
      throw A::HostFault("virtual handler object was freed; the handler was removed");
    }
    if (virtual_handler.is_valid())
      c.external = [this](const A::AnimOp &op) {
        if (!virtual_handler.is_valid())
          throw A::HostFault("virtual handler object was freed during the dispatch");
        Array args;
        for (auto a : op.args)
          args.push_back(a);
        Variant v = virtual_handler.call(String(op.kind.c_str()), args);
        if (op.kind == "vslot" && op.args.at(0) == 27) {
          if (v.get_type() != Variant::DICTIONARY)
            throw A::Unsupported(
                "slot27 requires a filtered_state/choice decision");
          Dictionary decision = v;
          auto selection = A::select_dispatch(
              get().cls, raw(decision["filtered_state"]),
              dispatch_choice(decision.get("choice", Dictionary())));
          if (virtual_depth >= 16)
            throw A::ResourceLimit("nested dispatch depth");
          ++virtual_depth;
          try {
            auto nested = context();
            A::apply_dispatch(nested, selection);
            get().current = selection.stored;
          } catch (...) {
            --virtual_depth;
            throw;
          }
          --virtual_depth;
        } else if (v.get_type() != Variant::BOOL || !bool(v))
          throw A::Unsupported(
              "virtual/holder service did not acknowledge execution");
      };
    return c;
  }
  A::NamedClip names() {
    return [this](const std::string &s) -> A::ClipId {
      auto &o = get();
      auto it = o.names.find(s);
      return it == o.names.end() ? 0 : o.resource(it->second);
    };
  }
  static A::PlayerInput input(Dictionary d) {
    A::PlayerInput p;
    p.weapon_class = raw(d.get("weapon_class", 0));
    p.model = raw(d.get("model", 0));
    p.variant = raw(d.get("variant", 1));
    p.draw = raw(d.get("draw", 0));
    p.object_flags = raw(d.get("object_flags", 0));
    p.no_combat_level = d.get("no_combat_level", false);
    p.display_only = d.get("display_only", false);
    const Variant hit_value = d.get("relative_hit", 0);
    if (hit_value.get_type() != Variant::INT)
      throw A::ValueError("relative hit must be an integer");
    int64_t hit = hit_value;
    if (hit < INT16_MIN || hit > INT16_MAX)
      throw A::ValueError("relative hit must be signed16");
    p.relative_hit = hit;
    return p;
  }
  void consume_tick(R::Raw frame) {
    auto &o = get();
    if (o.tick && (frame - *o.tick == 0 || frame - *o.tick >= 0x80000000u))
      throw A::ValueError("duplicate or stale dispatch tick");
    o.tick = frame;
    o.env->runtime.set_sim_frame(frame);
    o.env->runtime.set_draw_frame(frame);
  }
  static Array events(const std::vector<A::DispatchEvent> &events) {
    Array out;
    for (const auto &e : events) {
      Dictionary d;
      d["kind"] = String(e.kind.c_str());
      d["consumer"] = String(e.consumer.c_str());
      d["state"] = int64_t(e.state);
      d["mask"] = e.mask;
      d["word"] = int64_t(e.word);
      out.push_back(d);
    }
    return out;
  }
  void pose(Dictionary &d, const A::Motion &motion, bool copy = false,
            bool scripted = false) {
    auto &o = get();
    auto &r = o.env->runtime;
    const auto &p = r.draw_pose(o.state);
    PackedVector3Array positions, normals;
    PackedInt64Array rows;
    for (const auto &v : o.mesh.vertices) {
      auto x = A::skin_point(p, v);
      auto n = A::skin_normal(p, v);
      positions.push_back(
          Vector3(double(x[0]) / A::POSITION_DENOMINATOR * o.units,
                  double(x[2]) / A::POSITION_DENOMINATOR * o.units,
                  -double(x[1]) / A::POSITION_DENOMINATOR * o.units));
      normals.push_back(Vector3(double(n[0]) / A::NORMAL_DENOMINATOR,
                                double(n[2]) / A::NORMAL_DENOMINATOR,
                                -double(n[1]) / A::NORMAL_DENOMINATOR));
    }
    for (int j = 0; j < p.joint_count; ++j)
      for (auto row : p.mats[j])
        for (auto v : row)
          rows.push_back(v);
    d["positions"] = positions;
    d["normals"] = normals;
    d["pose_rows"] = rows;
    d["joint_count"] = p.joint_count;
    d["events"] = motion.events;
    d["flags"] = int64_t(r.state(o.state).flags);
    d["state"] = int64_t(o.current);
    d["root_x_raw"] = int64_t(motion.x);
    d["root_y_raw"] = int64_t(motion.y);
    d["root_yaw"] = motion.yaw;
    d["root_space"] = "animation_local";
    d["model_resource"] = int64_t(o.model);
    d["texture_resource"] = int64_t(o.texture);
    d["emissions"] =
        events(o.cls == "Player"
                   ? A::player_events(o.current, motion.events, copy, scripted)
                   : A::dispatch_events(o.cls, o.current, motion.events));
  }

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "config"),
                         &ChampionsAnimationDispatch::configure);
    ClassDB::bind_method(D_METHOD("set_virtual_handler", "handler"),
                         &ChampionsAnimationDispatch::set_virtual_handler);
    ClassDB::bind_method(D_METHOD("select_state", "state", "choice", "frame"),
                         &ChampionsAnimationDispatch::select_state);
    ClassDB::bind_method(D_METHOD("install_events", "variant"),
                         &ChampionsAnimationDispatch::install_events);
    ClassDB::bind_method(D_METHOD("player_action", "action", "input"),
                         &ChampionsAnimationDispatch::player_action);
    ClassDB::bind_method(D_METHOD("player_reset", "input"),
                         &ChampionsAnimationDispatch::player_reset);
    ClassDB::bind_method(
        D_METHOD("player_fidget", "stick_raw", "weapon_class", "shared_draw"),
        &ChampionsAnimationDispatch::player_fidget);
    ClassDB::bind_method(D_METHOD("player_combo", "model", "weapon_class",
                                  "old_variant", "frame", "shared_draw"),
                         &ChampionsAnimationDispatch::player_combo);
    ClassDB::bind_method(
        D_METHOD("tick", "frame", "speed_raw", "copy", "scripted_walk"),
        &ChampionsAnimationDispatch::tick);
    ClassDB::bind_method(D_METHOD("player_move", "frame", "speed_raw", "mode"),
                         &ChampionsAnimationDispatch::player_move);
    ClassDB::bind_method(D_METHOD("transition_probe", "flags"),
                         &ChampionsAnimationDispatch::transition_probe);
  }

public:
  Dictionary configure(Dictionary config) {
    return attempt([&](Dictionary &d) {
      if (String(config.get("profile", "")) != "rta-us-v2.00")
        throw A::Unsupported("RTA US v2.00 profile required");
      auto next = std::make_unique<Owner>();
      next->cls = String(config.get("class", "")).utf8().get_data();
      if (next->cls != "Player")
        A::dispatch_table(next->cls);
      next->units = config.get("units", 1.0);
      if (!std::isfinite(next->units) || next->units <= 0)
        throw A::ValueError("units must be positive finite");
      if (config.has("shared_with")) {
        Object *obj = config["shared_with"];
        auto *other = Object::cast_to<ChampionsAnimationDispatch>(obj);
        if (!other || other == this || other->busy)
          throw A::ValueError(
              "shared_with needs another idle dispatch binding");
        next->env = other->get().env;
      } else
        next->env = std::make_shared<A::Environment>();
      next->state = next->env->runtime.add_state();
      auto &r = next->env->runtime;
      Dictionary clips = config["clips"];
      Array keys = clips.keys();
      if (keys.size() > 512)
        throw A::ResourceLimit("dispatch clip budget");
      for (int64_t i = 0; i < keys.size(); ++i)
        next->env->register_clip(raw(keys[i]), bytes(clips[keys[i]]));
      auto base = next->resource(raw(config["base_resource"]));
      next->base = base;
      const auto &skeleton = r.clip(base);
      for (int64_t i = 0; i < keys.size(); ++i) {
        const auto &cl = r.clip(next->resource(raw(keys[i])));
        if (cl.depth != skeleton.depth || cl.bind != skeleton.bind ||
            cl.joint_count != skeleton.joint_count)
          throw A::Unsupported("incompatible paired skeleton");
      }
      int previous = -1;
      for (auto depth : skeleton.depth) {
        if (depth < 0 || depth > previous + 1)
          throw A::Unsupported("non-preorder dispatch skeleton");
        previous = depth;
      }
      Dictionary entries = config["entries"];
      keys = entries.keys();
      for (int64_t i = 0; i < keys.size(); ++i) {
        auto k = raw(keys[i]);
        if (k > 111)
          throw A::Unsupported("definition entry outside documented range");
        next->entries[k] = raw(entries[keys[i]]);
      }
      Dictionary names = config.get("named_clips", Dictionary());
      keys = names.keys();
      for (int64_t i = 0; i < keys.size(); ++i)
        next->names[String(keys[i]).utf8().get_data()] = raw(names[keys[i]]);
      next->model = raw(config["model_resource"]);
      if (!next->model)
        throw A::Unsupported("non-null selected model resource required");
      next->texture = raw(config.get("texture_resource", 0));
      if (next->cls != "Player" && (next->entries.at(1) != next->model ||
                                    next->entries.at(2) != next->texture))
        throw A::Unsupported("model/texture do not match class entries");
      auto b = bytes(config["model_bytes"]);
      auto model = champions::decodeRtaModel({b.data(), b.size()});
      auto visible = champions::modelVisibilityMask(
          model, integer(config.get("variant", -1), "variant"));
      A::Pose admission;
      admission.joint_count = skeleton.joint_count;
      for (const auto &m : model.meshes)
        if ((visible >> m.index) & 1)
          for (const auto &corner : m.corners) {
            A::Vertex v;
            v.position = corner.position_raw;
            v.normal = corner.normal_raw;
            for (unsigned j = 0; j < corner.skin.count; ++j)
              v.influences.push_back(
                  {corner.skin.joints[j], corner.skin.weights[j]});
            A::skin_point(admission, v);
            next->mesh.vertices.push_back(v);
          }
      if (next->mesh.vertices.size() > 600000)
        throw A::ResourceLimit("mesh corner budget");
      owner = std::move(next);
      virtual_handler = Callable();
      d["configured"] = true;
    });
  }
  Dictionary set_virtual_handler(Callable handler) {
    return attempt([&](Dictionary &) {
      get();
      virtual_handler = handler;
    });
  }
  Dictionary select_state(Variant state, Dictionary choice, Variant frame) {
    return attempt([&](Dictionary &d) {
      auto &o = get();
      if (o.cls == "Player")
        throw A::Unsupported("Player uses action dispatch");
      auto c = dispatch_choice(choice);
      auto selected = A::select_dispatch(o.cls, raw(state), c);
      o.env->runtime.set_sim_frame(raw(frame));
      auto ctx = context();
      A::apply_dispatch(ctx, selected);
      o.current = selected.stored;
      d["state"] = int64_t(o.current);
    });
  }
  Dictionary install_events(String variant) {
    return attempt([&](Dictionary &) {
      auto &o = get();
      auto c = context();
      if (o.cls == "Player") {
        if (!variant.is_empty())
          throw A::Unsupported("unknown player event variant");
        A::install_player_events(c);
      } else
        A::install_dispatch_events(c, o.cls, variant.utf8().get_data());
    });
  }
  Dictionary player_action(String action, Dictionary args) {
    return attempt([&](Dictionary &d) {
      auto &o = get();
      if (o.cls != "Player")
        throw A::Unsupported("Player required");
      auto s = A::player_action(action.utf8().get_data(), input(args), names());
      auto c = context();
      if (action == "ground_pound")
        A::install_ground_pound_events(c.runtime, s.ops.at(0).args.at(0));
      A::apply_dispatch(c, s);
      o.current = s.stored;
      d["state"] = int64_t(o.current);
      if (s.object_flags)
        d["object_flags"] = int64_t(*s.object_flags);
    });
  }
  Dictionary player_reset(Dictionary args) {
    return attempt([&](Dictionary &) {
      auto &o = get();
      if (o.cls != "Player")
        throw A::Unsupported("Player required");
      auto c = context();
      A::reset_player_idle(c, input(args), names());
      o.current = 2;
    });
  }
  Dictionary player_fidget(Variant stick, Variant cls, Callable draw) {
    return attempt([&](Dictionary &d) {
      auto &o = get();
      if (o.cls != "Player" || o.current != 2)
        throw A::Unsupported("neutral Player required");
      auto c = context();
      d["started"] =
          A::player_fidget(c, raw(stick), o.fidget, raw(cls), names(), [&]() {
            if (!draw.is_valid())
              throw A::Unsupported("shared draw service required");
            return raw(draw.call());
          });
    });
  }
  Dictionary player_combo(Variant model, Variant cls, Variant old,
                          Variant frame, Callable draw) {
    return attempt([&](Dictionary &d) {
      if (get().cls != "Player")
        throw A::Unsupported("Player required");
      auto v =
          A::player_combo_variant(raw(model), raw(cls), raw(old), raw(frame));
      if (!draw.is_valid())
        throw A::Unsupported("combo requires one shared RNG draw");
      raw(draw.call());
      d["variant"] = int64_t(v);
    });
  }
  Dictionary tick(Variant frame, Variant speed, bool copy, bool scripted) {
    return attempt([&](Dictionary &d) {
      auto &o = get();
      auto f = raw(frame), s = raw(speed);
      if (o.cls == "Player")
        A::admit_player_state(o.current);
      consume_tick(f);
      auto motion = o.env->runtime.advance(s, o.state, false);
      pose(d, motion, copy, scripted);
    });
  }
  Dictionary player_move(Variant frame, Variant speed, Variant mode) {
    return attempt([&](Dictionary &d) {
      auto &o = get();
      if (o.cls != "Player")
        throw A::Unsupported("Player required");
      A::admit_player_state(o.current);
      auto f = raw(frame), s = raw(speed), m = raw(mode);
      consume_tick(f);
      auto c = context();
      auto v = A::player_locomotion(c, s, o.blend, o.previous, m);
      o.blend = v.blend;
      o.previous = v.previous;
      pose(d, v.motion);
      d["rate_raw"] = int64_t(v.speed);
      d["blend_raw"] = int64_t(v.blend);
    });
  }
  Dictionary transition_probe(Variant flags) {
    return attempt([&](Dictionary &d) {
      auto &o = get();
      PackedInt64Array states;
      if (o.cls == "Player") {
        auto s = A::player_marker_exit(o.current, raw(flags));
        if (s)
          states.push_back(*s);
      } else
        for (auto s :
             A::dispatch_transition_probe(o.cls, o.current, raw(flags)))
          states.push_back(s);
      d["observed_targets"] = states;
    });
  }
};
