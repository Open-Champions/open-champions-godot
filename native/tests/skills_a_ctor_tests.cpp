#include "champions/skills_a.hpp"
#include <iostream>
namespace S = champions::simulation::rta::skills_a;
using champions::simulation::rta::Raw;
constexpr Raw OBJ=0x1000, PROPS=0x2000, OLD=0x2100, STR=0x3000;
constexpr Raw BACK=0x4000, NODE=0x4100, NEXT=0x4200;
constexpr Raw GRID=0x5db4f0, OFF=0x5f76f0, SEED=S::G_RAND_SEED;
constexpr Raw NAMES=0x5f76f8, OBJECTS=0x5f7ef8, COUNT=0x5f86f8;
void check(bool ok,int line) {
  if (!ok) throw std::runtime_error("constructor contract check at line "+std::to_string(line));
}
#define CHECK(x) check((x),__LINE__)
template<class F> void refuses(F f) {
  bool caught=false;
  try { f(); } catch (const S::Unsupported &) { caught=true; }
  CHECK(caught);
}
struct Fixture {
  S::Model m;
  std::vector<Raw> cells;
  Fixture() {
    text(0x5321a8,"hide"); text(0x5321b0,"name");
    m.named_callbacks["handle_link"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      CHECK(a.size()==3 && a[1]==OBJ+8 && a[2]==OBJ);
      const Raw cell=a[0], old=raw(cell); cells.push_back(cell);
      for (const auto &[p,v]:std::initializer_list<std::pair<Raw,Raw>>{
          {OBJ+8,BACK},{BACK,0},{BACK+4,OBJ+8},{BACK+8,NODE},{BACK+12,OBJ},
          {NODE,old},{NODE+4,cell},{NODE+8,BACK},{NODE+12,OBJ},{cell,NODE}})
        model.mem.poke(p,4,v);
      if (old) model.mem.poke(old+4,4,NODE);
      return {};
    };
  }
  Raw raw(Raw p) const { return Raw(m.mem.load(p,4)); }
  void text(Raw p,const std::string &s) {
    for (std::size_t i=0;i<s.size();++i) m.mem.poke(p+Raw(i),1,static_cast<unsigned char>(s[i]));
    m.mem.poke(p+Raw(s.size()),1,0);
  }
  std::string text(Raw p) const {
    std::string s;
    for (unsigned i=0;i<512;++i) {
      const auto c=Raw(m.mem.load(p+i,1));
      if (!c) return s;
      s.push_back(char(c));
    }
    throw std::runtime_error("unterminated fixture");
  }
  void properties(Raw p,const std::vector<std::string> &values,Raw base=STR) {
    for (std::size_t i=0;i<values.size();++i) {
      text(base+Raw(128*i),values[i]);
      m.mem.poke(p+Raw(4*i),4,base+Raw(128*i));
    }
    m.mem.poke(p+Raw(4*values.size()),4,0);
  }
  void ctor(Raw props=0,Raw x=0,Raw definition=0) {
    m.gameobject_ctor(OBJ,{x,0,0},definition,props);
  }
};
void write_at(const S::Model &m,std::size_t i,Raw p,std::size_t size,Raw value) {
  CHECK(i<m.writes.size());
  const auto &w=m.writes[i];
  CHECK(w.address==p && w.size==size && w.value==value);
}
int main() {
  try {
    // Prefix and one shared draw, with the input vector copied before writes.
    {
      Fixture f; f.m.mem.poke(SEED,4,0x12345678);
      f.ctor();
      const std::pair<Raw,Raw> first[]={{OBJ,0},{OBJ+4,0},{OBJ+0x48,0x5122a8},
        {OBJ+8,0},{OBJ+0x1c,0x41c00000},{OBJ+0x20,0x42900000},{OBJ+0xc,0}};
      for (unsigned i=0;i<7;++i) write_at(f.m,i,first[i].first,4,first[i].second);
      const Raw seed=S::lcg(0x12345678);
      const auto n=f.m.writes.size();
      write_at(f.m,n-3,OBJ+0x2c,4,0);
      write_at(f.m,n-2,SEED,4,seed);
      write_at(f.m,n-1,OBJ+0x44,4,seed>>16);
      CHECK(f.m.draws==1 && f.raw(OBJ+0x2c)==0);
    }
    // Incoming hide is published only after the stale-property query.
    {
      Fixture f; f.properties(PROPS,{"hide=1"}); f.ctor(PROPS);
      CHECK(f.raw(OFF)==0 && f.raw(OBJ+0xc)==0 && f.raw(OBJ+0x2c)==PROPS);
    }
    // Reused object's stale hide moves the peer from a nonempty grid cell.
    {
      Fixture f; f.properties(OLD,{"hide=-2"},0x6000);
      f.m.mem.poke(OBJ+0x2c,4,OLD);
      const Raw cell=GRID+4*(60*120+60);
      f.m.mem.poke(cell,4,NEXT); f.m.mem.poke(OFF,4,0x4300);
      f.ctor();
      std::vector<std::pair<Raw,Raw>> move;
      for (const auto &w:f.m.writes) if (w.address==cell || w.address==NEXT+4 ||
          w.address==NODE || w.address==NODE+4 || w.address==OFF || w.address==0x4304)
        move.push_back({w.address,Raw(w.value)});
      CHECK((move==std::vector<std::pair<Raw,Raw>>({{cell,NEXT},{NEXT+4,cell},
        {NODE+4,OFF},{NODE,0x4300},{OFF,NODE},{0x4304,NODE}})));
      CHECK(f.raw(OBJ+8)==BACK && f.raw(NODE+8)==BACK && f.cells.size()==1);
    }
    // Relocating a node already linked in the off-grid list also unlinks first.
    {
      Fixture f; f.properties(OLD,{"hide=1"},0x6000);
      f.m.mem.poke(OBJ+0x2c,4,OLD); f.m.mem.poke(OFF,4,NEXT);
      f.ctor(0,0x467a0000);
      CHECK(f.raw(OFF)==NODE && f.raw(NODE)==NEXT && f.raw(NEXT+4)==NODE &&
        f.raw(NODE+4)==OFF);
    }
    // Signed negative edge conversion from the reference's four probes.
    for (const auto &[x,col]:std::initializer_list<std::pair<Raw,Raw>>{
         {0xbf000000,60},{0xbf800000,59},{0xc3807333,59},{0xc3808000,58}}) {
      Fixture f; f.ctor(0,x);
      CHECK(f.cells.back()==GRID+4*(60*120+col));
    }
    // Quoted names, duplicate case-folded hashes, ordinal searches and key writes.
    {
      Fixture f; f.properties(PROPS,{"name=\"a\"tail","Name=ignored","name=A","name="});
      f.ctor(PROPS);
      CHECK(f.raw(COUNT)==3 && f.text(STR)=="name=a");
      CHECK(f.raw(NAMES+4*Raw(0))==STR+384+5);
      CHECK(f.raw(STR+7)!=0); // bytes after the first inserted NUL are retained
      CHECK(f.raw(NAMES+4*66)==STR+5 && f.raw(NAMES+4*67)==STR+256+5);
      unsigned key_writes=0;
      for (const auto &w:f.m.writes) if (w.address==0x5321b0) ++key_writes;
      CHECK(key_writes==7);
    }
    // A stale mixed-case property does not match; the canonical key is edited.
    {
      Fixture f; f.properties(OLD,{"Hide=9","hide= -2tail"},0x6000);
      f.m.mem.poke(OBJ+0x2c,4,OLD); f.text(0x5321a8,"HIDE");
      f.ctor();
      CHECK(f.text(0x5321a8)=="hide" && f.raw(OFF)==NODE);
    }
    // A tombstone is reused only by its own name, without replacing live keys.
    {
      Fixture f; f.properties(PROPS,{"name=alpha"}); f.ctor(PROPS);
      const Raw slot=Raw(426); // name_hash("alpha") from the pinned reference
      CHECK(f.raw(NAMES+4*slot)==STR+5);
      f.m.mem.poke(OBJECTS+4*slot,4,0);
      f.properties(PROPS,{"name=ALPHA"},STR+128);
      f.ctor(PROPS);
      CHECK(f.raw(NAMES+4*slot)==STR+128+5);
      f.m.mem.poke(OBJECTS+4*slot,4,0);
      f.properties(PROPS,{"name=cu"},STR+256); // different name, same hash
      f.ctor(PROPS);
      CHECK(f.raw(NAMES+4*((slot+1)&511))==STR+256+5);
      f.properties(PROPS,{"name=alpha"},STR+384);
      f.ctor(PROPS);
      CHECK(f.raw(NAMES+4*slot)==STR+384+5 &&
        f.raw(NAMES+4*((slot+1)&511))==STR+256+5);
    }
    // A full table refuses after quote editing and the count increment.
    {
      Fixture f; f.properties(PROPS,{"name=\"new\"suffix"});
      f.text(0x5000,"used");
      for (Raw i=0;i<512;++i) {
        f.m.mem.poke(NAMES+4*i,4,0x5000);
        f.m.mem.poke(OBJECTS+4*i,4,OBJ);
      }
      refuses([&] { f.ctor(PROPS); });
      CHECK(f.text(STR)=="name=new" && f.raw(COUNT)==1 &&
        f.raw(NAMES)==0x5000 && f.m.draws==1);
    }
    // Class lookup re-reads definition after callback and observes changed grid flags.
    {
      Fixture f; const Raw d=0x7000,replacement=0x7400;
      f.m.mem.poke(d,4,0x7100); f.m.mem.poke(d+8,4,0x7200);
      f.m.mem.poke(0x41bf78,4,2);
      f.m.named_callbacks["res_find"]=[&](S::Model &m,const S::Values &a) -> S::Values {
        CHECK(a==S::Values({0x7100,0x7200}));
        m.mem.poke(OBJ+0x24,4,replacement); m.mem.poke(OBJ+0xc,4,0x800);
        return {0x7654};
      };
      f.ctor(0,0,d);
      CHECK(f.raw(d+12)==0 && f.raw(replacement+12)==0x7654 &&
        f.raw(replacement+4)==2 && f.cells.back()==0x5e95f0+4*(60*120+60));
    }
    // Required callback refusal retains the completed prefix, without a draw.
    {
      Fixture f; f.m.named_callbacks.erase("handle_link");
      refuses([&] { f.ctor(); });
      CHECK(f.raw(OBJ+0x48)==0x5122a8 && f.m.draws==0);
    }
    std::cout << "Skills-A GameObject constructor contract PASS\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}
