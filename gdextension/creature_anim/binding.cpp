#include "champions/creature_anim/dispatch.hpp"
#include "champions/model.hpp"
#include <cmath>
#include <cstring>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <memory>
using namespace godot;
namespace R = champions::simulation::rta;
namespace A = R::creature_anim;
namespace {
double number(R::Raw raw) {
  const auto bits = R::f2d(raw);
  double out;
  std::memcpy(&out, &bits, sizeof(out));
  if (!std::isfinite(out))
    throw A::ValueError("nonfinite presentation component");
  return out;
}
std::vector<std::uint8_t> bytes(const PackedByteArray &a) {
  if (a.is_empty())
    return {};
  return {a.ptr(), a.ptr() + a.size()};
}
Variant structured(const R::monsters_1::Structured &v) {
  using K = R::creature_movement::ValueKind;
  if (v.kind == K::None)
    return Variant();
  if (v.kind == K::Integer)
    return v.scalar;
  if (v.kind == K::Buffer) {
    PackedByteArray b;
    b.resize(v.bytes.size());
    std::memcpy(b.ptrw(), v.bytes.data(), v.bytes.size());
    return b;
  }
  Array a;
  if (v.kind == K::Vector) {
    for (auto x : v.vec)
      a.push_back(int64_t(x));
  } else
    for (const auto &x : v.items)
      a.push_back(structured(x));
  return a;
}
Variant argument(const R::monsters_1::Argument &v) {
  return std::visit(
      [](const auto &x) -> Variant {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::uint32_t>)
          return int64_t(x);
        else if constexpr (std::is_same_v<T, std::string>)
          return String(x.c_str());
        else if constexpr (std::is_same_v<T, R::monsters_1::Structured>)
          return structured(x);
        else {
          Array a;
          if constexpr (std::is_same_v<T, R::monsters_1::TaggedTuple>) {
            a.push_back(String(x.tag.c_str()));
            for (const auto &v : x.fields)
              a.push_back(structured(v));
          } else if constexpr (std::is_same_v<T, R::monsters_1::Descriptor>) {
            for (auto v : x.effect)
              a.push_back(int64_t(v));
          } else
            for (auto v : x)
              a.push_back(int64_t(v));
          return a;
        }
      },
      v);
}
class ChampionsCreatureAnimation : public RefCounted {
  GDCLASS(ChampionsCreatureAnimation, RefCounted)
  std::unique_ptr<A::Creature> actor;
  double units = 1;
  bool busy = false;
  struct Busy {
    bool &b;
    explicit Busy(bool &flag) : b(flag) {
      if (b)
        throw A::ValueError("reentrant creature operation");
      b = true;
    }
    ~Busy() { b = false; }
  };
  A::Creature &get() {
    if (!actor)
      throw A::ValueError("configure first");
    return *actor;
  }
  template <class Fn> Dictionary attempt(Fn fn) {
    Dictionary d;
    try {
      Busy guard(busy);
      fn(d);
      d["ok"] = true;
    } catch (const std::exception &e) {
      d["ok"] = false;
      d["error"] = String(e.what());
    }
    return d;
  }
  Dictionary convert(const A::Frame &f) {
    Dictionary d;
    PackedVector3Array positions, normals;
    PackedInt64Array rows;
    for (auto v : f.positions)
      positions.push_back(
          Vector3(double(v[0]) / A::POSITION_DENOMINATOR * units,
                  double(v[2]) / A::POSITION_DENOMINATOR * units,
                  -double(v[1]) / A::POSITION_DENOMINATOR * units));
    for (auto v : f.normals)
      normals.push_back(Vector3(double(v[0]) / A::NORMAL_DENOMINATOR,
                                double(v[2]) / A::NORMAL_DENOMINATOR,
                                -double(v[1]) / A::NORMAL_DENOMINATOR));
    for (int j = 0; j < f.pose.joint_count; ++j)
      for (auto row : f.pose.mats[j])
        for (auto v : row)
          rows.push_back(v);
    // CPU mat34 basis rows are the column-vector transform's columns.
    const auto &m = f.object;
    auto B = [](double x, double y, double z) { return Vector3(x, z, -y); };
    Basis basis;
    basis.set_column(0, B(number(m[0]), number(m[1]), number(m[2])));
    basis.set_column(1, B(number(m[6]), number(m[7]), number(m[8])));
    basis.set_column(2, B(-number(m[3]), -number(m[4]), -number(m[5])));
    d["positions"] = positions;
    d["normals"] = normals;
    d["pose_rows"] = rows;
    d["joint_count"] = f.pose.joint_count;
    d["object_transform"] =
        Transform3D(basis, B(number(m[9]) * units, number(m[10]) * units,
                             number(m[11]) * units));
    d["root_motion"] =
        B(number(f.motion.x) * units, number(f.motion.y) * units, 0);
    d["events"] = f.motion.events;
    d["root_yaw"] = f.motion.yaw;
    d["model_resource"] = int64_t(f.model);
    d["texture_resource"] = int64_t(f.texture);
    return d;
  }

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "config"),
                         &ChampionsCreatureAnimation::configure);
    ClassDB::bind_method(D_METHOD("clear_history"),
                         &ChampionsCreatureAnimation::clear_history);
    ClassDB::bind_method(D_METHOD("write_memory", "address", "width", "value"),
                         &ChampionsCreatureAnimation::write_memory);
    ClassDB::bind_method(D_METHOD("read_memory", "address", "width"),
                         &ChampionsCreatureAnimation::read_memory);
    ClassDB::bind_method(D_METHOD("set_service", "name", "callback"),
                         &ChampionsCreatureAnimation::set_service);
    ClassDB::bind_method(D_METHOD("construct", "type_name", "position_raw",
                                  "facing", "properties"),
                         &ChampionsCreatureAnimation::construct);
    ClassDB::bind_method(D_METHOD("change_state", "state"),
                         &ChampionsCreatureAnimation::change_state);
    ClassDB::bind_method(D_METHOD("touch", "other"),
                         &ChampionsCreatureAnimation::touch);
    ClassDB::bind_method(D_METHOD("tick", "simulation_frame", "draw_frame"),
                         &ChampionsCreatureAnimation::tick);
    ClassDB::bind_method(D_METHOD("draw", "draw_frame"),
                         &ChampionsCreatureAnimation::draw);
  }

public:
  Dictionary clear_history() {
    return attempt([&](Dictionary &d) {
      auto &c = get();
      d["memory_writes_discarded"] = int64_t(c.memory.log().size());
      d["model_calls_discarded"] = int64_t(c.model.calls.size());
      d["sound_ranges_discarded"] = int64_t(c.runtime.sound_log.size());
      c.memory.clear_log();
      c.model.calls.clear();
      c.model.writes.clear();
      c.model.events.clear();
      c.runtime.sound_log.clear();
    });
  }
  Dictionary configure(Dictionary config) {
    return attempt([&](Dictionary &out) {
      if (String(config.get("profile", "")) != "rta-us-v2.00" ||
          String(config.get("class", "")) != "SmallSpider")
        throw A::Unsupported(
            "only documented RTA US v2.00 SmallSpider dispatch is admitted");
      double scale = config.get("units", 1.0);
      if (!std::isfinite(scale) || scale <= 0)
        throw A::ValueError("units must be positive finite");
      std::unique_ptr<A::Creature> next;
      if (config.has("shared_with")) {
        Object *other = config["shared_with"];
        auto *shared = Object::cast_to<ChampionsCreatureAnimation>(other);
        if (!shared || shared == this || shared->busy)
          throw A::ValueError(
              "shared_with must be another idle configured creature binding");
        next = std::make_unique<A::Creature>(
            shared->get().environment(),
            int64_t(config.get("address", 0x1000)));
      } else
        next = std::make_unique<A::Creature>(
            int64_t(config.get("address", 0x1000)));
      Array memory = config.get("memory", Array());
      for (int64_t i = 0; i < memory.size(); ++i) {
        Array w = memory[i];
        if (w.size() != 3)
          throw A::ValueError("memory record must have address,width,value");
        next->memory.poke(int64_t(w[0]), int64_t(w[1]), int64_t(w[2]));
      }
      Dictionary clips = config["clips"];
      Array keys = clips.keys();
      A::Pairing pairing;
      pairing.definition = int64_t(config["definition"]);
      pairing.model = int64_t(config["model_resource"]);
      pairing.texture = int64_t(config.get("texture_resource", 0));
      for (int64_t i = 0; i < keys.size(); ++i) {
        int64_t resource = keys[i];
        if (resource <= 0 || resource > UINT32_MAX)
          throw A::ValueError("invalid clip resource");
        pairing.clips[resource] =
            next->environment()->register_clip(resource, bytes(clips[keys[i]]));
      }
      pairing.base = pairing.clips.at(int64_t(config["base_resource"]));
      auto model_bytes = bytes(config["model_bytes"]);
      auto model =
          champions::decodeRtaModel({model_bytes.data(), model_bytes.size()});
      A::Mesh mesh;
      auto visible = champions::modelVisibilityMask(
          model, int64_t(config.get("variant", -1)));
      for (const auto &m : model.meshes)
        if ((visible >> m.index) & 1)
          for (const auto &corner : m.corners) {
            A::Vertex v;
            v.position = corner.position_raw;
            v.normal = corner.normal_raw;
            for (unsigned j = 0; j < corner.skin.count; ++j)
              v.influences.push_back(
                  {corner.skin.joints[j], corner.skin.weights[j]});
            mesh.vertices.push_back(std::move(v));
          }
      next->bind(std::move(pairing), std::move(mesh));
      actor = std::move(next);
      units = scale;
      out["configured"] = true;
    });
  }
  // Callbacks may read/write engine memory, but cannot recursively
  // tick/reconfigure.
  Dictionary write_memory(int64_t a, int64_t width, int64_t value) {
    Dictionary d;
    try {
      get().memory.store(a, width, value);
      d["ok"] = true;
    } catch (const std::exception &e) {
      d["ok"] = false;
      d["error"] = String(e.what());
    }
    return d;
  }
  Dictionary read_memory(int64_t a, int64_t width) {
    Dictionary d;
    try {
      d["value"] = int64_t(get().memory.load(a, width));
      d["ok"] = true;
    } catch (const std::exception &e) {
      d["ok"] = false;
      d["error"] = String(e.what());
    }
    return d;
  }
  Dictionary set_service(String name, Callable callback) {
    return attempt([&](Dictionary &) {
      auto key = std::string(name.utf8().get_data());
      if (key.rfind("anim_", 0) == 0)
        throw A::ValueError("animation callbacks are native-owned");
      if (!callback.is_valid())
        throw A::ValueError("service must be callable");
      get().model.callbacks[key] =
          [callback](auto &, const auto &args) -> std::uint64_t {
        Array a;
        for (const auto &arg : args)
          a.push_back(argument(arg));
        Variant result = callback.call(a);
        if (result.get_type() != Variant::INT)
          throw A::HostFault(
              "service must return an integer; no silent missing callback");
        return int64_t(result);
      };
    });
  }
  Dictionary construct(int64_t name, PackedInt64Array pos, int64_t facing,
                       int64_t props) {
    return attempt([&](Dictionary &) {
      if (pos.size() != 3)
        throw A::ValueError("three raw32 position lanes required");
      get().construct(name, {R::Raw(pos[0]), R::Raw(pos[1]), R::Raw(pos[2])},
                      facing, props);
    });
  }
  Dictionary change_state(int64_t s) {
    return attempt([&](Dictionary &) { get().change_state(s); });
  }
  Dictionary touch(int64_t other) {
    return attempt([&](Dictionary &) { get().touch(other); });
  }
  Dictionary tick(int64_t sim, int64_t frame) {
    return attempt([&](Dictionary &d) {
      if (sim < 0 || sim > UINT32_MAX || frame < 0 || frame > UINT32_MAX)
        throw A::ValueError("frame outside raw32");
      d = convert(get().tick(sim, frame));
    });
  }
  Dictionary draw(int64_t frame) {
    return attempt([&](Dictionary &d) {
      if (frame < 0 || frame > UINT32_MAX)
        throw A::ValueError("frame outside raw32");
      d = convert(get().draw(frame));
    });
  }
};
#include "dispatch_binding.inc"
void initialize(ModuleInitializationLevel level) {
  if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
    GDREGISTER_CLASS(ChampionsCreatureAnimation);
    GDREGISTER_CLASS(ChampionsAnimationDispatch);
  }
}
void terminate(ModuleInitializationLevel) {}
} // namespace
extern "C" GDExtensionBool GDE_EXPORT champions_creature_animation_init(
    GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr lib,
    GDExtensionInitialization *init) {
  GDExtensionBinding::InitObject o(proc, lib, init);
  o.register_initializer(initialize);
  o.register_terminator(terminate);
  o.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
  return o.init();
}
