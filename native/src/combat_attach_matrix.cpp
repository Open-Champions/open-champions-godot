// Held Soul attach matrices, research-combat-ctors-4 6baf4cd,
// docs/spec/attach-matrix.md and tools/ref_attach_matrix.py.
#include "champions/combat_ctors.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
A::Value iv(std::uint64_t v) { return std::int64_t(v&0xffffffffu); }
struct B {
  A::Model &m;
  Raw u8(Address a) const { return Raw(m.mem.load(a,1)); }
  Raw u16(Address a) const { return Raw(m.mem.load(a,2)); }
  Raw u32(Address a) const { return Raw(m.mem.load(a,4)); }
  Vec3 vec(Address a) const { return {u32(a),u32(a+4),u32(a+8)}; }
  Mat34 matrix(Address a) const {
    Mat34 out{};
    for (unsigned k=0;k<12;++k) out[k]=u32(Raw(a+4*k));
    return out;
  }
  Address record(Address model,Raw index) const {
    return Raw(model+16*u8(model+0x22)+0x34*index);
  }
};
}
Mat34 anim_get_joint_matrix(A::Model &m,Address state,Raw joint) {
  B b{m};auto pose=b.u32(state);
  if (!pose) {
    m.external_call("anim_buildPose",{iv(state),iv(0)});
    pose=b.u32(state);
  }
  if (!pose) pose=0x7b8380;
  auto j=s32(joint);
  if (j<0) j=0;
  auto last=s16(b.u16(pose+8))-1;
  if (last<j) j=last;
  auto base=Raw(pose+0x18*j+0x10);
  Mat34 out{};
  for (unsigned r=0;r<3;++r)
    for (unsigned k=0;k<3;++k)
      out[3*r+k]=mul(from_int(Raw(s16(b.u16(Raw(base+8*k+2*r))))),0x39800000);
  for (unsigned k=0;k<3;++k)
    out[9+k]=mul(from_int(Raw(s16(b.u16(Raw(base+8*k+6))))),0x3c800000);
  return out;
}
Mat34 model_get_attach_matrix(A::Model &m,Raw scale,Address model,
                              Address state,Raw index) {
  B b{m};auto rec=b.record(model,index);
  auto joint=state?anim_get_joint_matrix(m,state,b.u32(rec)):mat34_set_identity();
  auto out=mat34_mul(b.matrix(rec+4),joint);
  auto size=mat34_set_identity();
  for (unsigned k=0;k<9;++k) size[k]=mul(size[k],scale);
  return mat34_mul(out,size);
}
Mat34 model_get_attach_world_matrix(A::Model &m,Raw scale,Address model,
                                    const Vec3 &position,Raw facing,
                                    Address state,Raw index) {
  B b{m};auto rec=b.record(model,index);
  auto joint=state?anim_get_joint_matrix(m,state,b.u32(rec)):mat34_set_identity();
  auto out=mat34_mul(b.matrix(rec+4),joint);
  auto inverse=div(ONE,scale);
  for (unsigned k=0;k<9;++k) out[k]=mul(out[k],inverse);
  auto world=mat34_set_pos_rot_z_scale(scale,position,Raw(s16(Raw(s16(facing)+0x4000))));
  return mat34_mul(out,world);
}
Mat34 creature_get_attach_matrix0(A::Model &m,Address creature) {
  B b{m};
  return model_get_attach_world_matrix(m,b.u32(creature+0xdc),
          b.u32(b.u32(creature+0x24)+0xc),b.vec(creature+0x10),
          Raw(s16(b.u16(creature+0x38))),creature+0x114,0);
}
} // namespace champions::simulation::rta::combat_ctors
