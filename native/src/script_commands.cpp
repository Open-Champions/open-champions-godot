#include "champions/script_commands.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/objects.hpp"
#include "champions/session.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace champions::simulation::rta::script_commands {
namespace {
const Row kRows[COMMANDS] = {
#include "script_commands_table.inc"
};
// Binder deferred words (script_builtins.cpp). A name queued on this frame
// runs on the next run_deferred, which is the next ready tick.
constexpr std::uint32_t DEFER_COUNT = 0x86da78, DEFER_QUEUED = 0x86da7c,
                         DEFER_FLIP = 0x4f8690;
constexpr std::uint32_t SEED_WORD = 0x404464, ENTRANCE_WORD = 0x4f8468;
constexpr std::uint32_t CAMERA_OBJECT = 0x4e1000;
constexpr std::uint32_t STACK_WIDE = 0x6f0000;
constexpr std::uint32_t STREAM2_COUNT = 0x877158;

std::uint32_t word_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *word = std::get_if<std::uint32_t>(&value))
    return *word;
  throw amx::HostFault("scene value is not a word");
}
const std::string &string_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *text = std::get_if<std::string>(&value))
    return *text;
  throw amx::HostFault("scene value is not a string");
}
std::array<std::uint32_t, 3> vec_of(const builtins::Variables::ServiceValue &value) {
  if (const auto *vec = std::get_if<std::array<std::uint32_t, 3>>(&value))
    return *vec;
  throw amx::HostFault("scene value is not a vector");
}

// Commands whose callee body research executed (52984db), plus setCFlag,
// whose name selector and stop-then-queue are executed and whose queue is
// Playback::play_boss_music. Everything else partial or unknown stays refused.
bool routed_command(const char *name) {
  static const char *names[] = {
      "setMonsterVal",     "selectSetMonsterVal", "setMonsterRange",
      "setCreatureParams", "initScriptSetParams", "setInfo",
      "addHelpMessage",    "addHelpMessageF",     "showBust",
      "showCompass",       "hudResetFadeScreen",  "hudFadeScreen",
      "charmMonster",      "followPlayer",        "startMusic",
      "endMusic",          "setCFlag",            "selectSetCFlag",
  };
  for (const char *item : names)
    if (std::strcmp(item, name) == 0)
      return true;
  return false;
}

// wstr_format 0x19ea78 was not entered. This replaces the first %d, %i or %u
// with the decimal of the first float truncated toward zero, and turns %% into
// one percent. Any other percent sequence is left as text.
std::u16string decimal_of(std::int32_t value) {
  const bool neg = value < 0;
  std::uint32_t mag = value == static_cast<std::int32_t>(0x80000000u)
                          ? 0x80000000u
                          : static_cast<std::uint32_t>(neg ? -value : value);
  std::u16string digits;
  do {
    digits.push_back(static_cast<char16_t>(u'0' + (mag % 10u)));
    mag /= 10u;
  } while (mag);
  std::u16string out;
  if (neg)
    out.push_back(u'-');
  for (auto it = digits.rbegin(); it != digits.rend(); ++it)
    out.push_back(*it);
  return out;
}
std::u16string format_first_integer(const std::u16string &format, std::int32_t value) {
  std::u16string out;
  bool used = false;
  for (std::size_t i = 0; i < format.size(); ++i) {
    if (!used && format[i] == u'%' && i + 1 < format.size()) {
      const auto spec = format[i + 1];
      if (spec == u'd' || spec == u'i' || spec == u'u') {
        const auto number = decimal_of(value);
        out.insert(out.end(), number.begin(), number.end());
        ++i;
        used = true;
        continue;
      }
      if (spec == u'%') {
        out.push_back(u'%');
        ++i;
        continue;
      }
    }
    out.push_back(format[i]);
  }
  return out;
}
std::u16string read_wide(builtins::Memory &memory, std::uint32_t address) {
  std::u16string out;
  for (std::uint32_t i = 0; i < 4096u; ++i) {
    const auto unit = memory.u16(address + i * 2u);
    if (unit == 0)
      return out;
    out.push_back(static_cast<char16_t>(unit));
  }
  throw amx::HostFault("wide string has no terminator");
}
void write_wide(builtins::Memory &memory, std::uint32_t address, const std::u16string &text) {
  if (text.size() >= 4096u)
    throw amx::HostFault("formatted wide string exceeds 4096 units");
  for (std::size_t i = 0; i < text.size(); ++i)
    memory.w16(address + static_cast<std::uint32_t>(i) * 2u, text[i]);
  memory.w16(address + static_cast<std::uint32_t>(text.size()) * 2u, 0);
}

// Boss rows compare the unprefixed class. Synthetic objects in this repo store
// a Pawn length prefix ("10GameObject") whose integer is the remainder length.
std::string bare_class(const std::string &cls) {
  std::size_t i = 0;
  unsigned value = 0;
  while (i < cls.size() && cls[i] >= '0' && cls[i] <= '9') {
    if (value > 99999u)
      return cls;
    value = value * 10u + static_cast<unsigned>(cls[i] - '0');
    ++i;
  }
  if (i > 0 && value == cls.size() - i)
    return cls.substr(i);
  return cls;
}
} // namespace

const Row &row(std::size_t index) {
  if (index >= COMMANDS)
    throw std::out_of_range("script command index");
  return kRows[index];
}

Host::Host()
    : variables_(std::make_unique<builtins::Variables>()),
      effects_(std::make_unique<champions::rta::fx::sim::EffectWorld>()),
      audio_(std::make_unique<champions::audio::Playback>()),
      script_fx_(std::make_unique<script_effects::Effects>(variables_->memory)) {
  install();
}
Host::~Host() = default;

void Host::install() {
  const auto known = variables_->natives();
  for (std::size_t i = 0; i < COMMANDS; ++i) {
    if (kRows[i].index != static_cast<int>(i) || !kRows[i].name)
      throw std::logic_error("script command table is not in binder order");
    if (known.find(kRows[i].name) == known.end())
      throw std::logic_error("script command is missing from the binder");
  }
  variables_->local_player_slot = []() { return amx::Cell(0); };
  variables_->run_reward_script = [this](const std::string &name) {
    variables_->run_script(name);
  };
  // Follow camera, mode 1. A null camera faults showBust and showCompass.
  variables_->memory.word(CAMERA_OBJECT + 0x34, 1);
  variables_->memory.word(script_effects::G_CAMERA, CAMERA_OBJECT);
  variables_->scene_service = [this](const std::string &name,
                                     const std::vector<builtins::Variables::ServiceValue> &args) -> std::uint32_t {
    if (name == "obj_moveOffGrid" || name == "obj_updateGridCell") {
      if (args.empty())
        throw amx::HostFault("grid call needs an object");
      auto *obj = variables_->obj_at(word_of(args[0]));
      if (!obj)
        throw amx::HostFault("grid call has no object");
      if (name == "obj_moveOffGrid")
        variables_->world->move_off_grid(*obj);
      else
        variables_->world->update_grid_cell(*obj);
      return 0;
    }
    if (name == "particle_spawn") {
      if (args.size() < 4)
        throw amx::HostFault("particle_spawn argument count");
      Spawn spawn;
      spawn.definition = word_of(args[0]);
      spawn.pos = vec_of(args[1]);
      spawn.vel = vec_of(args[2]);
      spawn.backdate = word_of(args[3]);
      spawns_.push_back(spawn);
      effects_->particle_spawn(spawn.definition, spawn.pos, spawn.vel, spawn.backdate);
      return 0;
    }
    return scene(name, args);
  };
}

amx::NativeMap Host::command_map() {
  const auto inner = variables_->natives();
  amx::NativeMap out;
  for (std::size_t i = 0; i < COMMANDS; ++i) {
    if (kRows[i].effect != Effect::Documented && !routed_command(kRows[i].name)) {
      out.emplace(kRows[i].name, [this, i](amx::Machine &, const std::vector<amx::Cell> &)
                                     -> std::optional<amx::Address> {
        counts_.refused[i] += 1;
        return 0;
      });
      continue;
    }
    const auto found = inner.find(kRows[i].name);
    const auto function = found->second;
    const bool sync_seed = std::strcmp(kRows[i].name, "getRand") == 0;
    const bool entrance = std::strcmp(kRows[i].name, "setEntrance") == 0;
    out.emplace(kRows[i].name, [this, i, function, sync_seed, entrance](
                                   amx::Machine &machine, const std::vector<amx::Cell> &params)
                                   -> std::optional<amx::Address> {
      counts_.executed[i] += 1;
      if (sync_seed)
        sync_seed_in();
      auto result = function(machine, params);
      if (sync_seed)
        sync_seed_out();
      if (entrance)
        apply_entrance();
      return result;
    });
  }
  return out;
}

SharedRandom &Host::seed() {
  return external_seed_ ? *external_seed_ : variables_->world->shared;
}
void Host::sync_seed_in() { variables_->memory.word(SEED_WORD, seed().state); }
void Host::sync_seed_out() { seed().state = variables_->memory.word(SEED_WORD); }
void Host::apply_entrance() { flow_.entrance = variables_->memory.cstr(ENTRANCE_WORD); }
void Host::bind_seed(SharedRandom *next) { external_seed_ = next; }

std::uint32_t Host::intern_bytes(const std::string &text) {
  if (text.size() >= 160)
    throw amx::HostFault("interned text exceeds 160 bytes");
  const auto at = scratch_;
  scratch_ += 512;
  if (scratch_ > 0x00f00000u)
    throw amx::HostFault("script string scratch exhausted");
  variables_->memory.string(at, text);
  return at;
}
std::uint32_t Host::intern_wide(const std::u16string &text) {
  if (text.size() >= 4096)
    throw amx::HostFault("interned wide text exceeds 4096 units");
  const auto at = scratch_;
  scratch_ += 512 * ((static_cast<std::uint32_t>(text.size()) * 2u + 512u) / 512u);
  if (scratch_ > 0x00f00000u)
    throw amx::HostFault("script string scratch exhausted");
  write_wide(variables_->memory, at, text);
  return at;
}
void Host::note_type(builtins::Obj &obj, const std::string &type) {
  const auto found = type_at_.find(type);
  const auto at = found == type_at_.end() ? type_at_.emplace(type, intern_bytes(type)).first->second
                                          : found->second;
  variables_->memory.word(obj.addr + 0x34, at);
}

std::uint32_t Host::scene(const std::string &name,
                          const std::vector<builtins::Variables::ServiceValue> &args) {
  auto &fx = *script_fx_;
  auto &memory = variables_->memory;
  if (name == "text_lookup") {
    if (args.empty())
      throw amx::HostFault("text_lookup needs a key");
    // The string table was not entered. The widened key is the text the
    // setter copies. It is not retail copy.
    const auto &key = string_of(args[0]);
    std::u16string wide;
    wide.reserve(key.size());
    for (unsigned char byte : key)
      wide.push_back(static_cast<char16_t>(byte));
    return intern_wide(wide);
  }
  if (name == "wstr_format") {
    if (args.size() < 3 || string_of(args[0]) != "stack")
      throw amx::HostFault("wstr_format destination is not the stack buffer");
    const auto formatted = format_first_integer(read_wide(memory, word_of(args[1])),
                                                 Fpu::to_int(word_of(args[2])));
    write_wide(memory, STACK_WIDE, formatted);
    return 0;
  }
  if (name == "wstr_copy") {
    if (args.size() < 2)
      throw amx::HostFault("wstr_copy argument count");
    const auto source = std::holds_alternative<std::string>(args[1]) && string_of(args[1]) == "stack"
                            ? STACK_WIDE
                            : word_of(args[1]);
    fx.wstr_copy(word_of(args[0]), source);
    return 0;
  }
  if (name == "hud_showMessage") {
    if (args.size() < 4)
      throw amx::HostFault("hud_showMessage argument count");
    fx.show_message(word_of(args[0]), word_of(args[1]), word_of(args[2]), word_of(args[3]));
    return 0;
  }
  if (name == "hud_showHelpMessage") {
    if (args.size() < 3)
      throw amx::HostFault("hud_showHelpMessage argument count");
    fx.show_help(word_of(args[0]), word_of(args[1]), word_of(args[2]));
    return 0;
  }
  if (name == "hud_setCompassTarget") {
    if (args.size() < 2)
      throw amx::HostFault("hud_setCompassTarget argument count");
    const auto pos = vec_of(args[0]);
    fx.set_compass(pos.data(), word_of(args[1]));
    return 0;
  }
  if (name == "hud_setBust") {
    if (args.size() < 4)
      throw amx::HostFault("hud_setBust argument count");
    const auto texture = intern_bytes(string_of(args[0]));
    fx.set_bust(texture, word_of(args[1]), word_of(args[2]), word_of(args[3]));
    return 0;
  }
  if (name == "hud_setFade") {
    if (args.empty())
      throw amx::HostFault("hud_setFade argument count");
    fx.set_fade(word_of(args[0]));
    return 0;
  }
  if (name == "hud_stepFade") {
    if (args.empty())
      throw amx::HostFault("hud_stepFade argument count");
    return fx.step_fade(word_of(args[0]));
  }
  if (name == "Creature_scaleStats") {
    if (args.size() < 6)
      throw amx::HostFault("Creature_scaleStats argument count");
    std::uint32_t scales[5];
    for (int i = 0; i < 5; ++i)
      scales[i] = word_of(args[static_cast<std::size_t>(i + 1)]);
    return fx.scale_stats(word_of(args[0]), scales);
  }
  if (name == "Creature_setCharm") {
    if (args.size() < 3)
      throw amx::HostFault("Creature_setCharm argument count");
    fx.set_charm(word_of(args[0]), word_of(args[1]), word_of(args[2]));
    return 0;
  }
  if (name == "virtual") {
    if (args.size() < 3)
      throw amx::HostFault("virtual argument count");
    if (word_of(args[0]) != 34u)
      throw amx::HostFault("undocumented virtual slot");
    fx.slot34_set_home_range(word_of(args[1]), word_of(args[2]));
    return 0;
  }
  if (name == "music_playLevel") {
    if (args.size() < 2)
      throw amx::HostFault("music_playLevel argument count");
    const auto &title = string_of(args[0]);
    const auto flags = word_of(args[1]);
    const auto result = fx.music_play_level(intern_bytes(title), flags);
    if (fx.channel_volume) {
      music_channel_ = *fx.channel_volume;
      audio_->set_music_volume(memory.u32(script_effects::G_MUSIC_VOLUME));
      audio_->music_play_level(title, flags);
    }
    return result;
  }
  if (name == "stream_queueStop") {
    if (args.empty())
      throw amx::HostFault("stream_queueStop argument count");
    const auto stream = word_of(args[0]);
    const auto before = stream == 2 ? memory.u32(STREAM2_COUNT) : 0u;
    const auto result = fx.queue_stop(stream);
    if (stream == 2 && before < 8)
      audio_->queue_stop(2);
    return result;
  }
  if (name == "creature_startBossMusic") {
    if (args.empty())
      throw amx::HostFault("creature_startBossMusic argument count");
    const auto addr = word_of(args[0]);
    const auto type_at = memory.u32(addr + 0x34);
    if (type_at == 0)
      return 0;
    std::string object_name;
    if (auto *obj = variables_->obj_at(addr))
      if (auto found = objects::props_find(obj->props, "name"))
        object_name = *found;
    audio_->set_music_volume(memory.u32(script_effects::G_MUSIC_VOLUME));
    audio_->play_boss_music(bare_class(memory.cstr(type_at)), object_name);
    return 0;
  }
  throw amx::HostFault("undocumented scene service");
}

bool Host::load(const amx::ByteVector &image) {
  machine_.reset();
  variables_->script_machine.reset();
  level_started_ = false;
  waiting_ = false;
  faulted_ = false;
  counted_wait_ = false;
  counted_complete_ = false;
  last_value_ = 0;
  if (image.empty() || image.size() > session::MAX_SCRIPT_BYTES)
    return false;
  try {
    auto machine = std::make_shared<amx::Machine>(image, command_map());
    if (machine->load() != amx::ERR_NONE)
      return false;
    for (const auto &binding : machine->natives()) {
      if (binding.kind != amx::BindingKind::Bound || !binding.function)
        return false;
    }
    machine_ = std::move(machine);
    variables_->script_machine = machine_;
    return true;
  } catch (const std::exception &) {
    machine_.reset();
    variables_->script_machine.reset();
    return false;
  }
}

bool Host::adopt_objects(const std::uint8_t *data, std::size_t size) {
  if (!data || !size) {
    counts_.objects_refused += 1;
    return false;
  }
  try {
    const auto records = level_flow::parse_objects(champions::Bytes{data, size});
    for (const auto &record : records) {
      auto props = std::make_shared<std::vector<std::string>>(record.props);
      const std::string type = record.cls.empty() ? "10GameObject" : record.cls;
      auto obj = variables_->new_object({}, 0, record.position, record.angle, type, props);
      note_type(*obj, type);
      counts_.objects_registered += 1;
    }
    return true;
  } catch (const std::exception &) {
    counts_.objects_refused += 1;
    return false;
  }
}

void Host::add_object(const std::string &name, std::uint32_t flags,
                      const std::array<std::uint32_t, 3> &pos, std::uint32_t angle) {
  auto props = std::make_shared<std::vector<std::string>>();
  props->push_back("name=" + name);
  auto obj = variables_->new_object({}, flags, pos, angle, "10GameObject", props);
  note_type(*obj, "10GameObject");
  counts_.objects_registered += 1;
}

void Host::note_fault() {
  counts_.faults += 1;
  faulted_ = true;
  waiting_ = false;
}
void Host::remember(const std::exception &error) {
  if (last_fault_.empty())
    last_fault_ = error.what();
  note_fault();
}
void Host::note_queue() {
  const auto state = queue();
  if (state.count == 0 && state.queued == 0) {
    waiting_ = false;
    if (level_started_ && !faulted_ && !counted_complete_) {
      counts_.reached_complete += 1;
      counted_complete_ = true;
    }
    return;
  }
  waiting_ = true;
  if (!counted_wait_) {
    counts_.reached_wait += 1;
    counted_wait_ = true;
  }
}
Stop Host::classify() {
  if (faulted_)
    return Stop::Fault;
  note_queue();
  if (waiting_)
    return Stop::Wait;
  if (level_started_ && counted_complete_)
    return Stop::Complete;
  return Stop::Missing;
}

Stop Host::enter(const std::string &name, bool optional) {
  if (!machine_ || faulted_) {
    if (!faulted_)
      note_fault();
    return Stop::Fault;
  }
  const auto found = machine_->find_public(name);
  if (found.first != amx::ERR_NONE) {
    if (!optional)
      counts_.missing_entries += 1;
    return Stop::Missing;
  }
  machine_->budget = session::SCRIPT_BUDGET;
  try {
    const auto result = machine_->execute(found.second, {}, 0);
    last_value_ = static_cast<amx::Cell>(result.second.value_or(0));
    if (result.first != amx::ERR_NONE) {
      note_fault();
      return Stop::Fault;
    }
  } catch (const std::exception &error) {
    remember(error);
    return Stop::Fault;
  }
  return Stop::Complete;
}

Stop Host::begin_level() {
  if (!machine_) {
    note_fault();
    return Stop::Fault;
  }
  bool entered = false;
  for (const char *name : {"preStart", "start"}) {
    const auto found = machine_->find_public(name);
    if (found.first != amx::ERR_NONE)
      continue;
    if (!entered) {
      counts_.started += 1;
      level_started_ = true;
      entered = true;
    }
    if (enter(name, false) == Stop::Fault)
      return Stop::Fault;
  }
  if (!entered)
    return Stop::Missing;
  machine_->budget = session::SCRIPT_BUDGET;
  try {
    variables_->run_post_action();
  } catch (const std::exception &error) {
    remember(error);
    return Stop::Fault;
  }
  return classify();
}

void Host::on_ready_tick() {
  if (!waiting_ || faulted_ || !machine_)
    return;
  machine_->budget = session::SCRIPT_BUDGET;
  try {
    variables_->run_deferred();
  } catch (const std::exception &error) {
    remember(error);
    return;
  }
  classify();
}

Stop Host::run(const std::string &entry) {
  const auto stop = enter(entry, false);
  if (stop == Stop::Fault || stop == Stop::Missing)
    return stop;
  note_queue();
  if (faulted_)
    return Stop::Fault;
  return waiting_ ? Stop::Wait : Stop::Complete;
}

Queue Host::queue() const {
  Queue state;
  if (!variables_)
    return state;
  state.count = variables_->memory.word(DEFER_COUNT);
  state.queued = variables_->memory.word(DEFER_QUEUED);
  state.flip = variables_->memory.word(DEFER_FLIP);
  return state;
}
std::string Host::queue_key() const {
  constexpr std::uint32_t names = 0x86b278;
  constexpr std::size_t bytes = 2u * 64u * 0x50u;
  auto raw = variables_->memory.read(names, bytes);
  const auto state = queue();
  for (const auto word : {state.count, state.queued}) {
    for (int shift = 0; shift < 32; shift += 8)
      raw.push_back(static_cast<std::uint8_t>(word >> shift));
  }
  return std::string(raw.begin(), raw.end());
}
std::uint32_t Host::public_count() const {
  return machine_ ? static_cast<std::uint32_t>(machine_->num_publics) : 0;
}
} // namespace champions::simulation::rta::script_commands
