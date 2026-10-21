#include "champions/skinning.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
using Skin=champions::ModelSkinInfluences;
using Matrix=std::array<double,12>;
constexpr Matrix identity{{1,0,0,0,0,1,0,0,0,0,1,0}};
champions::RtaAnimationTransforms transforms(unsigned count=4) {
    champions::RtaAnimationTransforms result{};result.tick=1.5;result.joints.resize(count);
    for(auto& joint:result.joints){joint.valid=true;joint.drives_joint=true;joint.model=identity;joint.skin=identity;}
    return result;
}
champions::RtaModel model(const Skin& skin=Skin{{0,0,0,0},{255,0,0,0},1}) {
    champions::RtaModel result{};
    champions::ModelCorner corner{};corner.position={2,3,4};corner.skin=skin;
    result.meshes.push_back({7,false,0,{corner,corner,corner}});
    return result;
}
void reject(const champions::RtaModel& m,const champions::RtaAnimationTransforms& t,const char* message) {
    bool failed=false;try{(void)champions::skinRtaModelPositions(m,t);}catch(const champions::FormatError&){failed=true;}check(failed,message);
}
void near(const std::array<double,3>& actual,const std::array<double,3>& expected,const char* message) {
    for(unsigned axis=0;axis<3;++axis)check(std::abs(actual[axis]-expected[axis])<1e-12,message);
}
void tests() {
    auto m=model();auto t=transforms();
    t.joints[0].skin={0,-1,0,10,1,0,0,-2,0,0,1,5}; // Z90 + translation -> (7,0,9).
    t.joints[1].skin={1,0,0,-4,0,0,-1,7,0,1,0,2}; // X90 + translation -> (-2,3,5).
    t.joints[2].skin={0,0,1,1,0,1,0,2,-1,0,0,3}; // Y90 + translation -> (5,5,1).
    t.joints[3].skin={1,0,0,-1,0,1,0,-2,0,0,1,-3}; // Translation -> (1,1,1).
    auto output=champions::skinRtaModelPositions(m,t);
    check(output.tick==1.5&&output.meshes.size()==1&&output.meshes[0].index==7&&output.meshes[0].positions.size()==3,"output retains tick mesh index and corner count including hidden mesh");
    near(output.meshes[0].positions[0],{7,0,9},"rigid skin matrix uses column-vector rotation then translation");
    m.meshes[0].corners[0].position={99,99,99};t.joints[0].skin[3]=99;
    near(output.meshes[0].positions[0],{7,0,9},"returned positions own their storage");
    t.joints[0].skin[3]=10;
    const std::array<Skin,3> blends{{
        {{0,1,0,0},{85,170,0,0},2},
        {{0,1,2,0},{85,85,85,0},3},
        {{0,1,2,3},{51,51,51,102},4}
    }};
    const std::array<std::array<double,3>,3> expected{{{{1,2,19.0/3}},{{10.0/3,8.0/3,5}},{{2.4,2,3.4}}}};
    for(unsigned i=0;i<3;++i)near(champions::skinRtaModelPositions(model(blends[i]),t).meshes[0].positions[0],expected[i],"independent two/three/four influence anchors");
    m=model({{0,1,0,0},{0,255,0,0},2});
    near(champions::skinRtaModelPositions(m,t).meshes[0].positions[0],{-2,3,5},"explicit zero-weight influence retains a valid reference and contributes zero");
    m=model();m.meshes[0].corners[1].uv_texels={8,9};m.meshes[0].corners[2].skin={{1,0,0,0},{255,0,0,0},1};
    output=champions::skinRtaModelPositions(m,t);
    check(output.meshes[0].positions[0]==output.meshes[0].positions[1],"UV seam duplicates retain their source position result");
    near(output.meshes[0].positions[2],{-2,3,5},"coincident positions retain distinct corner influences");
    m.meshes.push_back({2,true,0,{}});output=champions::skinRtaModelPositions(m,t);
    check(output.meshes[0].index==7&&output.meshes[1].index==2&&output.meshes[1].positions.empty(),"mesh ordering and empty mesh retained without visibility filtering");
    auto sixtyfour=transforms(64);sixtyfour.joints[63].skin[11]=3;
    near(champions::skinRtaModelPositions(model({{63,0,0,0},{255,0,0,0},1}),sixtyfour).meshes[0].positions[0],{2,3,7},"joint63 supported");
    m=model();auto invalid_unused=t;invalid_unused.joints[3].valid=false;invalid_unused.joints[3].model={};invalid_unused.joints[3].skin={};
    check(champions::skinRtaModelPositions(m,invalid_unused).meshes[0].positions.size()==3,"unused invalid zero transform is permitted");
    champions::RtaModel empty{};auto no_transforms=transforms(0);
    check(champions::skinRtaModelPositions(empty,no_transforms).meshes.empty(),"empty direct model yields empty owned result");
    // The direct operation does not consume or claim to transform other attributes.
    m=model();m.meshes[0].corners[0].normal[0]=std::numeric_limits<float>::quiet_NaN();
    check(champions::skinRtaModelPositions(m,t).meshes[0].positions.size()==3,"nonconsumed normal metadata is left outside position-only contract");
    for(int slot:{0,15,-2,127}){m=model();m.attachment_slot=static_cast<std::int8_t>(slot);reject(m,t,"attachment models cannot use direct skinning path");}
    m=model();m.meshes.resize(65);reject(m,t,"mesh count budget enforced");
    m=model();m.meshes[0].index=64;reject(m,t,"mesh index range enforced");
    m=model();m.meshes.push_back(m.meshes[0]);reject(m,t,"duplicate mesh index rejected");
    m=model();m.meshes[0].corners.pop_back();reject(m,t,"triangle triples required");
    m=model();auto bad=t;bad.joints.resize(65);reject(m,bad,"transform count budget enforced");
    for(double tick:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){bad=t;bad.tick=tick;reject(m,bad,"tick finite and nonnegative");}
    bad=t;bad.joints[3].drives_joint=false;reject(m,bad,"unused masked joint still means incomplete pose");
    bad=t;bad.joints[0].valid=false;bad.joints[0].model={};bad.joints[0].skin={};reject(m,bad,"referenced invalid transform rejected");
    bad=t;bad.joints[3].valid=false;reject(m,bad,"invalid unused transform must use explicit zero matrices");
    for(double value:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bad=t;bad.joints[3].skin[0]=value;reject(m,bad,"all skin matrices must be finite even if unused");
        bad=t;bad.joints[3].model[0]=value;reject(m,bad,"public model matrix must also be finite");
    }
    for(float value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        m=model();m.meshes[0].corners[2].position[1]=value;reject(m,t,"every source position must be finite");
    }
    for(unsigned count:{0U,5U,255U}){m=model();m.meshes[0].corners[2].skin.count=static_cast<std::uint8_t>(count);reject(m,t,"influence count range enforced");}
    m=model();m.meshes[0].corners[0].skin.weights[0]=254;reject(m,t,"weights must sum to255");
    m=model({{0,1,0,0},{255,255,0,0},2});reject(m,t,"oversummed byte weights rejected");
    m=model();m.meshes[0].corners[0].skin.joints[0]=4;reject(m,t,"influence joint must exist");
    m=model({{0,4,0,0},{255,0,0,0},2});reject(m,t,"zero-weight reference outside transform count rejected");
    m=model({{0,3,0,0},{255,0,0,0},2});reject(m,invalid_unused,"zero-weight reference to invalid transform rejected");
    m=model();m.meshes[0].corners[0].skin.joints[3]=1;reject(m,t,"unused joint tail must be zero");
    m=model();m.meshes[0].corners[0].skin.weights[3]=1;reject(m,t,"unused weight tail must be zero");
    m=model();reject(m,no_transforms,"nonempty model requires its joint transforms");
    bad=t;bad.joints[0].skin[0]=std::numeric_limits<double>::max();reject(m,bad,"finite matrix multiplication overflow rejected");
    bad=t;bad.joints[0].skin={std::numeric_limits<double>::max()/4,0,0,std::numeric_limits<double>::max(),0,1,0,0,0,0,1,0};
    reject(m,bad,"finite transformed translation sum overflow rejected");
    m=model({{0,1,0,0},{0,255,0,0},2});reject(m,bad,"even zero-weight referenced transform arithmetic must remain finite");
    m=model();
    m.meshes[0].corners[0].normal={1,0,0};
    m.meshes[0].corners[1].normal={0,1,0};
    m.meshes[0].corners[2].normal={0,0,1};
    auto normals=champions::skinRtaModelNormals(m,t);
    check(normals.meshes.size()==1&&normals.meshes[0].index==7&&normals.meshes[0].normals.size()==3,"normal output keeps mesh index and corner order");
    near(normals.meshes[0].normals[0],{0,1,0},"a stored normal rotates by the skin 3x3 and ignores translation");
    near(normals.meshes[0].normals[2],{0,0,1},"the Z column of that rotation leaves a Z normal");
    m.meshes[0].corners[0].skin={{0,1,0,0},{85,170,0,0},2};
    normals=champions::skinRtaModelNormals(m,t);
    near(normals.meshes[0].normals[0],{170.0/255.0,85.0/255.0,0},"two influences blend the rotated normals by weight/255");
    m=model();m.meshes[0].corners[0].normal[0]=std::numeric_limits<float>::quiet_NaN();
    bool normal_refused=false;
    try{(void)champions::skinRtaModelNormals(m,t);}catch(const champions::FormatError&){normal_refused=true;}
    check(normal_refused,"a nonfinite stored normal is rejected");
    check(champions::skinRtaModelPositions(m,t).meshes[0].positions.size()==3,"position skinning still ignores normal metadata");
    m=model();m.meshes[0].corners.resize(600000,m.meshes[0].corners[0]);
    check(champions::skinRtaModelPositions(m,t).meshes[0].positions.size()==600000,"exact200000 triangle budget accepted");
    m.meshes.push_back({2,true,0,std::vector<champions::ModelCorner>(3,m.meshes[0].corners[0])});
    reject(m,t,"triangle budget shared across meshes");
}
} // namespace
int main(){try{tests();std::cout<<checks<<" skinning checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
