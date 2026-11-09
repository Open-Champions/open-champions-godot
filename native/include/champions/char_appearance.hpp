#pragma once
// Created-character appearance, attachment, equipment-visual, shell and pose
// contracts for Return to Arms US v2.00. Godot-free and deterministic.
//
// Every rule below is Observed/Executed in research-char-appearance @ 153d1c3
// (audits research-char-appearance-r2 and -r3-fix PASS). "CCA:n" cites
//   docs/spec/created-character-appearance.md line n at that commit,
// "REF:n" cites tools/ref_char_attachment.py line n at that commit, and
// "SE4:n" cites research-loading-readiness @ 366b65a
//   docs/starting-equipment-r4.md line n (reused, not re-derived; CCA:253-259).
//
// Refused here, with the reason kept in docs/CHAR_APPEARANCE.md:
//  - animated joint matrices (CCA:236-238, 433-434, 460-461): the attachment
//    record's matrix is copied, never evaluated against a pose;
//  - who advances the creation preview (CCA:351-364, 482-485): no step is
//    added to the creation clip;
//  - pixel parity / the 0x174e78 packet image and VU/GS output (CCA:236-238,
//    458-460): only the parameters and the gate are modelled.
// Resource names, archive bytes and format templates are caller input read
// from the owner's files at run time; nothing retail is embedded.
#include "champions/animation_clock.hpp"
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::char_appearance {

struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---- record choices and constructor handles (CCA:25-32, 138-170) -----------
// Player record +0x20..+0x23 signed bytes: hair, shared colour, beard, skin.
struct Choices {
  std::int8_t hair = -1, colour = 0, beard = -1, skin = 0;
};

enum class Layer : std::uint8_t { Hair, Beard, Body };
enum class Kind : std::uint8_t { Mesh, Texture };

// Format-template addresses resolved at run time (CCA:152-156). Templates are
// printf strings in the owner's executable; only the addresses live here.
inline constexpr std::uint32_t HAIR_MESH_FORMAT = 0x535b78,
                               HAIR_TEXTURE_FORMAT = 0x535b88,
                               BEARD_MESH_FORMAT = 0x535b98,
                               BEARD_TEXTURE_FORMAT = 0x535ba8,
                               BODY_MESH_FORMAT = 0x535bc0,
                               BODY_TEXTURE_FORMAT = 0x535bd0;
// Model +0x54 indexes the prefix pointer table (CCA:148-149).
inline constexpr std::uint32_t MODEL_PREFIX_TABLE = 0x4ebbb8;
// Archive selection 0x1a1eac-0x1a1ef0 (CCA:146-149).
inline constexpr std::uint32_t FIRST_ARCHIVE = 0x5353e0, SECOND_ARCHIVE = 0x535b68,
                               ARCHIVE_OVERRIDE_BYTE = 0x506040;
std::uint32_t selection_archive(std::int32_t players, bool override_byte);

struct Templates {
  std::string hair_mesh, hair_texture, beard_mesh, beard_texture, body_mesh,
      body_texture;
};

struct Request {
  Layer layer;
  Kind kind;
  std::uint32_t object_offset; // destination handle field (CCA:154-156)
  std::uint32_t format_address;
  std::string name;
};

// printf subset used by the six constructor templates: %s %d %c and %%.
// Arguments are consumed in order prefix, number, letter (REF of the
// appearance reference, tools/ref_char_appearance.py:59-74 @ 153d1c3).
// Any other conversion, or a conversion/argument count mismatch, refuses.
std::string format_resource(const std::string &format, const std::string &prefix,
                            std::optional<std::int32_t> number = {},
                            std::optional<std::int32_t> letter = {});

// Ordered requests: hair pair, beard pair, then body pair (CCA:158-161).
// Hair/beard use style+1 and colour+97; style -1 omits both requests of that
// layer. Body mesh is fixed; body texture uses skin+1.
std::vector<Request> constructor_requests(const std::string &prefix,
                                          const Templates &templates,
                                          Choices choices);

struct Pair {
  std::uint32_t mesh = 0, texture = 0; // 0 is a null handle
  bool both() const { return mesh != 0 && texture != 0; }
};
struct Handles {
  Pair hair, beard, body;
};
// Lookup returns 0 when a resource is absent. Null results stay null; a
// skipped layer (style -1) clears both handles even when `prior` holds
// poisoned nonzero values (CCA:158-164). No other style is substituted
// for a missing resource (CCA:166-170).
using Lookup = std::function<std::uint32_t(const Request &)>;
Handles constructor_handles(const std::string &prefix, const Templates &templates,
                            Choices choices, const Lookup &lookup,
                            Handles prior = {});

// ---- hair/beard gates and the cover flag (CCA:182-199) ---------------------
// Five substring predicates in pinned-address order: name contains token A
// (0x535d70), picture base contains B (0x535d78), picture base contains C
// (0x535d80), name contains D (0x535d90), nonnull picture base contains E
// (0x535d98). Tokens are caller input from the executable.
struct CoverHits {
  bool name_a = false, picture_b = false, picture_c = false, name_d = false,
       picture_e = false;
};
// Sticky: only an eligible equipped part with both handles can set it.
bool cover_flag(std::int32_t model, std::int32_t definition_slot, bool prior,
                bool part_has_both_handles, CoverHits hits);
struct Gates {
  bool hair = false, beard = false;
};
// head_exception: the present head item's definition name contains token
// 0x535da8 (CCA:195-196).
Gates appearance_gates(std::int32_t model, std::int32_t skin, bool covered,
                       bool head_present, bool head_exception);
// Each optional layer independently needs its gate plus both handles.
struct Draws {
  bool hair = false, beard = false;
};
Draws appearance_draws(std::int32_t model, std::int32_t skin, bool covered,
                       bool head_present, bool head_exception,
                       const Handles &handles);

// ---- body show mask (CCA:201-208, 410-419) ---------------------------------
inline constexpr std::uint64_t ALL_SHOWN = ~std::uint64_t(0);
inline constexpr std::uint32_t BODY_MASK_COUNTER = 0x41b514;
inline constexpr std::int32_t BODY_MASK_COUNTER_LIMIT = 0x111;
struct VariantRecord {
  std::uint64_t show = 0, hide = 0;
};
// 0x179550: complement of the default-hidden word, then selected records in
// ascending bit order: add show bits, then remove hide bits.
std::uint64_t variant_show_mask(std::uint64_t default_hidden, bool table_enabled,
                                std::uint32_t selection,
                                const std::array<VariantRecord, 32> &records);
// Start of an equipment pass: all ones, or 0x179550 with selection bit one
// when the signed counter at 0x41b514 is >= 0x111.
std::uint64_t initial_body_mask(std::uint32_t counter, std::uint64_t default_hidden,
                                bool table_enabled,
                                const std::array<VariantRecord, 32> &records);

// One Player_draw frame's ordering (CCA:403-419): the body draw reads object
// +0x220 *before* Player_drawEquipped rebuilds it, so the body always uses
// the previous pass's mask. Only actually drawn parts clear their hide bits.
class BodyMask {
public:
  explicit BodyMask(std::uint64_t stored) : stored_(stored) {}
  std::uint64_t stored() const { return stored_; }
  // model_draw show-mask argument of Creature_draw / the inventory body draw.
  std::uint64_t body_draw_argument() const { return stored_; }
  void begin_equipment_pass(std::uint64_t initial) { pass_ = initial; open_ = true; }
  void part_drawn(std::uint64_t part_hide);
  void end_equipment_pass();

private:
  std::uint64_t stored_;
  std::uint64_t pass_ = 0;
  bool open_ = false;
};

// mesh_runs 0x177df4-0x177eb0 / 0x178cbc-0x178d74 (CCA:229-232, 414-416):
// (start, length) of coalesced shown runs; empty spans emit nothing. The
// 64-mesh EE shift edge is preserved, not corrected (REF:96-122).
std::vector<std::pair<std::uint32_t, std::uint32_t>>
mesh_runs(const std::vector<std::uint32_t> &boundaries, std::uint64_t show_mask);

// ---- attachment packets (CCA:210-227) --------------------------------------
// 0x1760ec: header +0x13 of -1 disables selection even with a nonnegative
// override; otherwise the override replaces the header unless it is -1.
// Indices outside the 16-record table refuse (the original has no clamp).
std::optional<std::uint32_t> attachment_index(std::int32_t header_0x13,
                                              std::int32_t override_index);
// parent + parent.byte[0x22]*16 + index*0x34.
std::uint32_t attachment_record_address(std::uint32_t parent,
                                        std::uint8_t parent_byte_0x22,
                                        std::uint32_t index);
// Joint index times four, the packet word at 0x17614c.
std::uint32_t attachment_joint_word(std::int32_t joint);
// Nine row-major rotation words then the origin -> the packet's 3x4 matrix,
// column by column. Mirroring flips the sign of the first row (REF:83-87).
std::array<Raw, 12> attachment_matrix(const std::array<Raw, 12> &record_values,
                                      bool mirrored);

// ---- equipment visuals by body (CCA:246-298; SE4:44-56) --------------------
struct StartingVisuals {
  std::int32_t class_index;
  std::array<std::uint32_t, 4> pointer_slots; // weapon, then armor 4, 10, 12
};
StartingVisuals starting_visual_slots(std::int32_t body);
// Shield (definition slot 3) bit by body model, 0x1a3cfc-0x1a3d84 jump table
// 0x535e00; models failing unsigned < 12 use bit 0 (CCA:262-271).
std::uint32_t shield_variant_bit(std::uint32_t body_model);
std::uint32_t shield_selection(std::uint32_t body_model);
// Second equipment pointer forces attach override 1; others stay -1.
std::int32_t equipped_override(std::uint32_t pointer_slot);

struct PartHeader {
  std::int8_t attach_0x13 = -1;
  std::uint64_t hide = 0;            // part qword cleared from the body mask
  std::uint64_t default_hidden = 0;  // header word used by 0x179550
  bool table_enabled = false;
  std::array<VariantRecord, 32> records{};
  std::uint16_t flags = 0;           // part flag 0x20 gates the shell pass
};
struct PartDraw {
  bool drawn = false;                  // both resource handles present
  std::uint64_t show_mask = ALL_SHOWN; // part draw show-mask argument
  bool variant_called = false;         // 0x179550 on the part model
  std::int32_t attach_override = -1;
  std::optional<std::uint32_t> attach_index; // from header +0x13 / override
  bool mirrored = false;               // model 2 0x179860 before the draw
  bool shell_reset = false;            // 0x179978 when override != -1
};
// One 0x1a3b90-0x1a3df8 iteration (CCA:283-298). Does not switch on the
// record class word (+0x50 is not read, CCA:280-281, 473-475).
PartDraw equipped_part_draw(std::uint32_t pointer_slot, std::uint32_t definition_slot,
                            std::uint32_t body_model, const PartHeader &part,
                            bool both_handles);

// ---- shell parameters and the 0x174e78 gate (CCA:300-325, 421-434) ---------
inline constexpr std::uint32_t SHELL_BLOCK = 0x3e9cd0, SHELL_POINTER = 0x41c160,
                               SHELL_COUNT = 0x41c164, SHELL_RESET_POINTER = 0x41c150;
inline constexpr Raw INVENTORY_DRAW_ARGUMENT = 0x3fc00000; // 0x1c0c64, 1.5
struct ShellBlock {
  std::uint8_t byte3 = 0, byte11 = 0;
  Raw float4 = 0, float12 = 0;
};
struct ShellState {
  std::uint32_t pointer = 0;
  std::int32_t count = 0;
  std::optional<ShellBlock> block; // nullopt: block bytes not written
};
// 0x1068d8: item flag bit 33 through 0x1046c4 (7 or 0). Clear: count 0, no
// block write. Set: count 2, scale 7*0.025+0.6 clamped to 1 only above 1.
ShellState shell_fields(bool item_flag_33, Raw draw_argument);
// f12 for the 0x1a4d08 caller from the (Unknown-source) projection in f0.
Raw equipped_draw_argument(Raw projection);
// 0x179978: pointer 0x41c150, count 0.
void reset_shell(ShellState &state);
// model_draw 0x177da4 / model_drawPart 0x178c88: flag 0x20 and count > 0.
bool shell_pass_gate(std::uint16_t part_flags, std::int32_t shell_count);

// ---- poses (CCA:326-401) ---------------------------------------------------
namespace anim_clock = animation_clock;
inline constexpr std::uint32_t CREATION_FORMAT = 0x53ff68,
                               CREATION_FORMAT_MARKED = 0x53ff58,
                               CREATION_IMPORT_LITERAL = 0x53fd38,
                               CREATION_RACE_BYTE = 0x8e2c91;
inline constexpr Raw CREATION_BLEND = 0x3e4ccccd;   // 0.2 at 0x225584
inline constexpr std::uint32_t CREATION_FLAGS = 3;  // addiu 3 at 0x225580
// Thirteen race slots, twelve distinct addresses (REF:326-329).
inline constexpr std::array<std::uint32_t, 13> CREATION_RACE_LITERALS{{
    0x53fdc0, 0x53fdc0, 0x53fdc8, 0x53fde0, 0x53fe10, 0x53fe18, 0x53fe40,
    0x53fe48, 0x53fe78, 0x53fe80, 0x53fea0, 0x53fd98, 0x53fd70}};
struct CreationStart {
  std::uint32_t format_address;
  // nullopt: the descriptor name is used. Otherwise the race literal address.
  std::optional<std::uint32_t> race_literal;
};
// 0x2254bc-0x2255b4. nullopt: no restart (gate failed, or the import literal
// with a race byte outside 0..12 after +1).
std::optional<CreationStart> creation_restart(std::uint16_t object_0x11a,
                                              std::uint8_t object_0x118,
                                              bool descriptor_is_import_literal,
                                              std::int8_t race_byte);
// Both formats are a leading %s plus a fixed suffix (REF:353-357).
std::string creation_clip_name(const std::string &name, const std::string &format);
// anim_start(0.2, state, clip, 3): time 0, weight 0; rate 1 on an empty state,
// 0x3daaaaab when one cell is present. No anim_advance is added (CCA:482-485).
anim_clock::Track *install_creation_clip(anim_clock::AnimState &state, anim_clock::ClipPtr clip);

// inventory_selectIdleAnim 0x1c3f08 (CCA:375-388): lump by model, clip by
// weapon class, flags 0x43, 0.1 blend (forced off for cell 0).
inline constexpr Raw INVENTORY_BLEND = 0x3dcccccd;
inline constexpr std::uint32_t INVENTORY_FLAGS = 0x43;
inline constexpr std::array<std::uint32_t, 12> INVENTORY_LUMPS{{
    0x538488, 0x538478, 0x538508, 0x5384d8, 0x5384f8, 0x5384c8,
    0x5384a8, 0x5384e8, 0x538498, 0x5384b8, 0x538518, 0x538528}};
std::uint32_t inventory_lump_address(std::int32_t model);
std::uint32_t inventory_idle_address(std::int32_t weapon_class);
// Already the current track: nothing is written (returns nullptr).
anim_clock::Track *inventory_select_idle(anim_clock::AnimState &state, anim_clock::ClipPtr clip);
// The inventory draw's one unit step from 0x1c0a10, before model_draw. From a
// fresh cell this gives time 0x3f7fffff and weight 1.
std::int32_t inventory_draw_step(anim_clock::AnimSystem &system, anim_clock::AnimState &state);

// player_resetIdleAnim 0x1b2470 (CCA:389-401; research-player-anim cda95dc
// docs/spec/player-animation-selection.md:58-66 for cells 1..3 weight/life 0).
inline constexpr std::uint32_t IDLE_ROW_SIZE = 0x68, IDLE_FLAGS = 0x1205;
inline constexpr std::array<std::uint32_t, 4> IDLE_CELL_OFFSETS{{0x44, 0x4c, 0x54, 0x5c}};
inline constexpr std::uint32_t DISPLAY_IDLE_DEFAULT = 0x537628;
std::uint32_t display_idle_address(std::int32_t weapon_class); // REF:339-340
std::array<std::uint32_t, 4> idle_cell_offsets(std::int32_t weapon_class);
struct IdleClips {
  std::array<anim_clock::ClipPtr, 4> table{};
  anim_clock::ClipPtr display{}; // used for cell 0 only when object +0x462 != 0
};
// Returns false (state untouched) when no player or hit points are not > 0.
// Writes clip, flags, weight and life of cells 0..3 and count 4. Time and
// rate words are never written: earlier values survive.
bool reset_idle_anim(anim_clock::AnimState &state, bool player_present, Raw hit_points,
                     bool display_only, const IdleClips &clips);

} // namespace champions::simulation::rta::char_appearance
