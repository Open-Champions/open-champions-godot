// Numeric facts from 5694ae3c63e7ddff4b2177adf2ae783b23dd6a9b. Regenerate with tools/generate_creature_dispatch.py.
static DispatchTable make_dispatch_0() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{12,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{11,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{6,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{10,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,732,262144},{{{"blend",{9,1036831948,1036831948,16}}},18}});
c.conditions.push_back({{false,732,524288},{{{"blend",{13,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{12,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{7,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{7,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},23};
}
t.events[3]={{{1058642329,1},{1066192076,1},{0,0},{0,0}}};
t.events[4]={{{1053609164,4},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,2},{1053609164,1},{1066192076,1},{0,0}}};
t.events[7]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitTargetInArc"};
t.consumers[6][2]={"Creature_hitTargetInArc"};
t.consumers[6][4]={"Creature_hitTargetInArc"};
t.consumers[7][1]={"Creature_hitTargetInArc"};
t.consumers[7][2]={"Creature_hitTargetInArc"};
t.consumers[7][4]={"Creature_hitTargetInArc"};
t.consumers[8][1]={"Creature_hitTargetInArc"};
t.consumers[8][2]={"Creature_hitTargetInArc"};
t.consumers[8][4]={"Creature_hitTargetInArc"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_1() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{12,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{11,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{14,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{9,1036831948,1036831948,2064}}},18});
c.residues.push_back({{{"blend",{10,1036831948,1036831948,2064}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{12,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{7,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{7,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{6,1036831948,3}}},23};
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1060320051,1},{1067030937,2},{0,0},{0,0}}};
t.events[7]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[14]={{{1055286886,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitTargetInArc"};
t.consumers[6][2]={"Creature_hitTargetInArc"};
t.consumers[7][1]={"Creature_hitTargetInArc"};
t.consumers[7][2]={"Creature_hitTargetInArc"};
t.consumers[8][1]={"Creature_hitTargetInArc"};
t.consumers[8][2]={"Creature_hitTargetInArc"};
t.consumers[9][1]={"math_cos16","math_sin16","slot16","model_getAttachPoint","GameObject_rand"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_2() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1050253721,3}},{"sync",{}}},1};
c.conditions.push_back({{false,976,2},{{{"start",{12,1036831948,3}}},5}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{13,1050253721,3}},{"sync",{}}},3};
c.conditions.push_back({{false,976,2},{{{"start",{12,1036831948,3}}},5}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{12,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"start",{12,1036831948,3}},{"blend",{4,1036831948,1036831948,8}}},6};
c.conditions.push_back({{false,976,1},{{{"start",{12,1036831948,3}},{"blend",{4,1036831948,1036831948,8}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{14,1036831948,1036831948,8}}},7};
c.conditions.push_back({{false,976,2},{{{"start",{12,1036831948,3}}},5}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{13,1050253721,3}},{"sync",{}}},16};
c.conditions.push_back({{false,976,2},{{{"start",{12,1036831948,3}}},5}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1050253721,3}}},19};
c.conditions.push_back({{false,976,2},{{{"start",{12,1036831948,3}}},5}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{6,1050253721,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{6,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{7,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1050253721,6}},{"vslot",{28}}},20});
}
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1060320051,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[7]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[14]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{10,1}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[7]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_3() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{21,1050253721,3}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"push_w",{21,515,0,1051372203,1065353216,0}},{"push_w",{22,513,0,0,1065353216,0}},{"push_w",{23,513,0,0,1065353216,0}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{},7};
c.conditions.push_back({{false,564,1},{{{"blend",{4,1036831948,1036831948,8}}},7}});
}
{ auto &c=t.states[8]; c.baseline={{{"push_w",{7,515,0,1051372203,1065353216,0}},{"push_w",{8,513,0,0,1065353216,0}},{"push_w",{9,513,0,0,1065353216,0}},{"blend",{20,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{19,1036831948,1036831948,8}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{19,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{10,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{15,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{24,1053609164,22}},{"blend",{20,1036831948,1036831948,8}},{"vslot",{28}}},20};
c.conditions.push_back({{true,0,1},{{{"start",{24,1053609164,22}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"push_w",{7,515,0,1051372203,1065353216,0}},{"push_w",{8,513,0,0,1065353216,0}},{"push_w",{9,513,0,0,1065353216,0}},{"blend",{20,1036831948,1036831948,8}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{16,1036831948,3}},{"blend",{18,1036831948,1036831948,8}}},24};
}
{ auto &c=t.states[25]; c.baseline={{{"start",{16,1036831948,3}},{"blend",{17,1036831948,1036831948,8}}},25};
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{15,1}}};
t.consumers[7][1]={"000000","Creature_hitTargetOrPlayerInArc"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[9]={{8},{}};
t.transitions[10]={{23},{}};
t.transitions[18]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_4() { DispatchTable t; t.group="none";
return t; }
static DispatchTable make_dispatch_5() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{17,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{20,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{20,1056964608,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{17,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{7,1056964608,1056964608,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{11,1056964608,1056964608,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{8,1056964608,1056964608,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{5,1056964608,1056964608,8}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{12,1056964608,1056964608,8}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{10,1056964608,1056964608,8}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{6,1056964608,1056964608,8}}},12};
}
{ auto &c=t.states[13]; c.baseline={{{"blend",{9,1056964608,1056964608,2056}}},13};
}
{ auto &c=t.states[14]; c.baseline={{{"blend",{3,1056964608,1056964608,2056}}},14};
}
{ auto &c=t.states[15]; c.baseline={{{"blend",{4,1056964608,1056964608,2056}}},15};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{20,1056964608,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{17,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{13,1050253721,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{18,1036831948,1036831948,16}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{19,1036831948,1036831948,16}}},24};
}
t.events[3]={{{1058642329,16},{0,0},{0,0},{0,0}}};
t.events[4]={{{1058642329,16},{0,0},{0,0},{0,0}}};
t.events[5]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1058642329,17},{0,0},{0,0},{0,0}}};
t.events[10]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[11]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[12]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[13]={{{1073741824,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{17,1}}};
t.consumers[6][1]={"Creature_hitTargetAtReach"};
t.consumers[7][1]={"Creature_hitTargetAtReach"};
t.consumers[8][1]={"Creature_hitTargetOrPlayerInArc"};
t.consumers[9][1]={"Creature_hitTargetOrPlayerInArc"};
t.consumers[10][1]={"Creature_hitTargetOrPlayerInArc"};
t.consumers[11][1]={"Creature_hitTargetOrPlayerInArc"};
t.consumers[12][1]={"Creature_hitTargetAtReach"};
t.consumers[13][1]={"Creature_hitTargetAtReach"};
t.consumers[13][16]={"store +3d0:4=1"};
t.consumers[14][1]={"Creature_hitTargetAtReach"};
t.consumers[14][16]={"store +3d0:4=1"};
t.consumers[15][1]={"Creature_hitTargetAtReach"};
t.consumers[15][16]={"store +3d0:4=1"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[12]={{3},{}};
t.transitions[13]={{3},{}};
t.transitions[14]={{3},{}};
t.transitions[15]={{3},{}};
t.transitions[18]={{0},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_6() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,786,1},{{{"start",{9,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{9,1045220556,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{15,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{10,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1036831948,1036831948,18440}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{7,1036831948,1036831948,18440}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{8,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{15,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{11,1036831948,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{11,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{12,1036831948,6}},{"vslot",{28}}},20});
}
t.events[4]={{{1050253721,1},{1061997772,2},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{1061997772,2},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{1061997772,2},{0,0},{0,0}}};
t.events[7]={{{1050253721,1},{1061997772,2},{0,0},{0,0}}};
t.events[10]={{{1056964608,1},{1063675494,2},{0,0},{0,0}}};
t.events[11]={{{1065353216,132},{0,0},{0,0},{0,0}}};
t.events[12]={{{1065353216,132},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{15,1}}};
t.consumers[6][1]={"GameObject_rand","Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[7][1]={"GameObject_rand","Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[7][2]={"Creature_canSeeTargetIn","setState(0x6)"};
t.consumers[8][1]={"Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[8][2]={"Creature_canSeeTargetIn","setState(0x6)"};
t.consumers[9][1]={"Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[9][2]={"Creature_canSeeTargetIn","setState(0x6)"};
t.consumers[10][1]={"GameObject_rand","Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[10][2]={"Creature_canSeeTargetIn","setState(0x6)"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][4]={"store +c:4=1"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_7() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{19,1050253721,2051}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{19,1050253721,2051}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{17,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1008981770,1008981770,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1008981770,1008981770,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{6,1008981770,1008981770,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{5,1008981770,1008981770,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{18,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{19,1050253721,2051}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{15,1036831948,1036831948,16}}},18});
c.conditions.push_back({{false,732,524288},{{{"blend",{14,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{16,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{8,1050253721,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{8,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{9,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{10,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{11,1050253721,6}},{"vslot",{28}}},20});
c.conditions.push_back({{false,12,2},{{{"start",{8,1050253721,6}},{"jump_end",{}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{7,1036831948,3}}},23};
}
t.events[3]={{{1050253721,2},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,2},{0,0},{0,0},{0,0}}};
t.events[5]={{{1060320051,2},{0,0},{0,0},{0,0}}};
t.events[6]={{{1045220556,2},{0,0},{0,0},{0,0}}};
t.events[8]={{{1050253721,128},{0,0},{0,0},{0,0}}};
t.events[9]={{{1050253721,128},{0,0},{0,0},{0,0}}};
t.events[10]={{{1050253721,128},{0,0},{0,0},{0,0}}};
t.events[11]={{{1050253721,128},{0,0},{0,0},{0,0}}};
t.events[18]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[20]={{{1075000115,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{16,1}},{"randomize",{}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[24][1]={"store +a8:8=8000000000000000"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[24]={{},{0}};
return t; }
static DispatchTable make_dispatch_8() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}}},0};
c.conditions.push_back({{false,1120,1},{{{"start",{27,1036831948,3}}},0}});
c.conditions.push_back({{false,1120,2},{{{"start",{20,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,1120,1},{{{"start",{29,1036831948,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,1120,2},{{{"start",{23,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,1120,1},{{{"start",{29,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,1120,2},{{{"start",{23,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{9,1036831948,3}},{"sync",{}}},5};
c.conditions.push_back({{false,1120,1},{{{"start",{26,1036831948,3}},{"sync",{}}},5}});
c.conditions.push_back({{false,1120,2},{{{"start",{19,1036831948,3}},{"sync",{}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{7,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{22,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{24,1036831948,1036831948,8}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{24,1036831948,1036831948,8}}},12};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{false,1120,1},{{{"start",{28,1036831948,2051}},{"sync",{}}},16}});
c.conditions.push_back({{false,1120,2},{{{"start",{21,1036831948,2051}},{"sync",{}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{{"start",{13,1036831948,20}}},18};
c.residues.push_back({{{"start",{13,1036831948,20}}},18});
c.residues.push_back({{{"blend",{12,1036831948,1036831948,2064}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1050253721,3}}},19};
c.conditions.push_back({{false,1120,1},{{{"start",{27,1050253721,3}}},19}});
c.conditions.push_back({{false,1120,2},{{{"start",{20,1036831948,3}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"start",{8,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"holder",{11}},{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"start",{14,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"start",{13,1045220556,2054}},{"vslot",{28}}},20});
}
t.events[4]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{1061997772,1},{1068708659,1},{0,0}}};
t.events[6]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1072902963,128},{0,0},{0,0},{0,0}}};
t.events[13]={{{1061997772,128},{0,0},{0,0},{0,0}}};
t.events[14]={{{1061997772,128},{0,0},{0,0},{0,0}}};
t.events[22]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[24]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[25]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[7][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[8][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[9][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[10][1]={"math_cos16","math_sin16","Creature_leadTargetPoint","slot16","model_getAttachPoint","math_atan2_16","GameObject_rand","pool_new","DiseaseProjectile_ctor"};
t.consumers[11][1]={"creature_findCorpseOfClass","creature_findWoundedAlly","slot35","Creature_angleToTarget","000000","store +464:4=b4","other.slot32"};
t.consumers[12][1]={"creature_findCorpseOfClass","creature_findWoundedAlly","slot35","000000","creature_addSpell","store +464:4=b4"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][128]={"pool_new","creature_addSpell","slot35","SpellDiseaseEffect_ctor"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[12]={{5},{}};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_9() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{7,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{4,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{4,1045220556,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{7,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{4,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{5,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{7,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{6,1036831948,6}},{"vslot",{28}}},20};
}
t.events[3]={{{1050253721,1},{1061997772,2},{0,0},{0,0}}};
t.events[6]={{{1065353216,132},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{7,1}}};
t.consumers[6][1]={"GameObject_rand","Creature_hitTargetInArc","store +3c8:4=1"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][4]={"store +c:4=1"};
t.transitions[6]={{5},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_10() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{17,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{21,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{21,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{17,1045220556,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{5,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{6,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{18,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{19,1036831948,1036831948,18440}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{20,1036831948,1036831948,18440}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{7,1036831948,1036831948,18440}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{8,1036831948,1036831948,18440}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{9,1036831948,1036831948,18440}}},12};
}
{ auto &c=t.states[13]; c.baseline={{{"blend",{10,1036831948,1036831948,18440}}},13};
}
{ auto &c=t.states[14]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},14};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},14});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},14});
}
{ auto &c=t.states[15]; c.baseline={{{"blend",{22,1036831948,1036831948,2056}}},15};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{21,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{17,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{11,1045220556,6}},{"vslot",{28}}},20};
c.conditions.push_back({{false,969,1},{{{"start",{12,1045220556,6}},{"vslot",{28}}},20}});
}
t.events[3]={{{1056964608,1},{1060320051,2},{0,0},{0,0}}};
t.events[4]={{{1056964608,1},{1060320051,2},{0,0},{0,0}}};
t.events[5]={{{1056964608,1},{1060320051,2},{0,0},{0,0}}};
t.events[6]={{{1056964608,1},{1060320051,2},{0,0},{0,0}}};
t.events[7]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1069966950,1},{1071225241,2},{1075419545,4},{1077936128,8}}};
t.events[10]={{{1074580684,1},{0,0},{0,0},{0,0}}};
t.events[11]={{{1065353216,1},{1077936128,2},{1087687884,4},{0,0}}};
t.events[18]={{{1068708659,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[20]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitAllEnemiesInArc","store +3c8:1=1"};
t.consumers[7][1]={"math_cos16","math_sin16","pool_new","Creature_hitAllEnemiesInArc","store +3c8:1=0","slot35","GroundPoundEffect_ctor"};
t.consumers[8][1]={"Creature_hitAllEnemiesInArc","store +3c8:1=1"};
t.consumers[9][1]={"Creature_hitAllEnemiesInArc","store +3c8:1=1"};
t.consumers[10][1]={"creature_findEnemyInCone","slot16","model_getAttachPoint","math_cos16","math_sin16","Cazic_castShockOfFrost","store +3c8:1=1","Cazic_castShockOfFire"};
t.consumers[11][1]={"creature_findEnemyInCone","slot16","model_getAttachPoint","math_cos16","math_sin16","Cazic_castShockOfFrost","store +3c8:1=1","Cazic_castShockOfFire"};
t.consumers[12][1]={"creature_findEnemyInCone","slot16","model_getAttachPoint","math_cos16","math_sin16","Cazic_castShockOfFrost","store +3c8:1=0","Cazic_castShockOfFire"};
t.consumers[12][2]={"creature_findEnemyInCone","slot16","model_getAttachPoint","math_cos16","math_sin16","Cazic_castShockOfFrost","store +3c8:1=0","Cazic_castShockOfFire"};
t.consumers[12][4]={"creature_findEnemyInCone","slot16","model_getAttachPoint","math_cos16","math_sin16","Cazic_castShockOfFrost","store +3c8:1=0","Cazic_castShockOfFire"};
t.consumers[12][8]={"creature_findEnemyInCone","slot16","model_getAttachPoint","math_cos16","math_sin16","Cazic_castShockOfFrost","slot35","000000","store +3c8:1=0","Cazic_castShockOfFire"};
t.consumers[13][1]={"000000","SpellFrostArmor_typeinfo","creature_findSpellByType","creature_addSpell","store +3c8:1=0","slot35","math_cos16","math_sin16"};
t.consumers[14][1]={"Creature_hitAllEnemiesInArc","store +3c8:1=1"};
t.consumers[14][2]={"Creature_canSeeTargetIn","anim_findTrack","GameObject_rand","anim_startBlendInOut"};
t.consumers[20][-1]={"Cazic_deathEffects","Creature_deadUpdate"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[12]={{5},{}};
t.transitions[13]={{5},{}};
t.transitions[14]={{5},{}};
t.transitions[15]={{3},{}};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_11() { DispatchTable t; t.group="none";
return t; }
static DispatchTable make_dispatch_12() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}}},0};
c.conditions.push_back({{false,968,1},{{{"start",{20,1036831948,3}}},0}});
c.conditions.push_back({{false,968,2},{{{"start",{30,1036831948,3}}},0}});
c.conditions.push_back({{false,968,3},{{{"start",{40,1036831948,3}}},0}});
c.conditions.push_back({{false,968,4},{{{"start",{50,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,968,1},{{{"start",{21,1036831948,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,968,2},{{{"start",{31,1036831948,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,968,3},{{{"start",{41,1036831948,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,968,4},{{{"start",{51,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,968,1},{{{"start",{21,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,968,2},{{{"start",{31,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,968,3},{{{"start",{41,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,968,4},{{{"start",{51,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{10,1036831948,3}},{"sync",{}}},5};
c.conditions.push_back({{false,968,1},{{{"start",{20,1036831948,3}},{"sync",{}}},5}});
c.conditions.push_back({{false,968,2},{{{"start",{30,1036831948,3}},{"sync",{}}},5}});
c.conditions.push_back({{false,968,3},{{{"start",{40,1036831948,3}},{"sync",{}}},5}});
c.conditions.push_back({{false,968,4},{{{"start",{50,1036831948,3}},{"sync",{}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
c.conditions.push_back({{false,968,1},{{{"blend",{13,1036831948,1036831948,8}}},6}});
c.conditions.push_back({{false,968,2},{{{"blend",{23,1036831948,1036831948,8}}},6}});
c.conditions.push_back({{false,968,3},{{{"blend",{33,1036831948,1036831948,8}}},6}});
c.conditions.push_back({{false,968,4},{{{"blend",{43,1036831948,1036831948,8}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},7};
c.conditions.push_back({{false,968,1},{{{"blend",{14,1036831948,1036831948,2056}}},7}});
c.conditions.push_back({{false,968,2},{{{"blend",{24,1036831948,1036831948,2056}}},7}});
c.conditions.push_back({{false,968,3},{{{"blend",{34,1036831948,1036831948,2056}}},7}});
c.conditions.push_back({{false,968,4},{{{"blend",{44,1036831948,1036831948,2056}}},7}});
}
{ auto &c=t.states[8]; c.baseline={{{"start",{5,1036831948,3}}},8};
c.conditions.push_back({{false,968,1},{{{"start",{15,1036831948,3}}},8}});
c.conditions.push_back({{false,968,2},{{{"start",{25,1036831948,3}}},8}});
c.conditions.push_back({{false,968,3},{{{"start",{35,1036831948,3}}},8}});
c.conditions.push_back({{false,968,4},{{{"start",{45,1036831948,3}}},8}});
}
{ auto &c=t.states[9]; c.baseline={{{"start",{12,1036831948,2051}}},9};
c.conditions.push_back({{false,968,1},{{{"start",{22,1036831948,2051}}},9}});
c.conditions.push_back({{false,968,2},{{{"start",{32,1036831948,2051}}},9}});
c.conditions.push_back({{false,968,3},{{{"start",{42,1036831948,2051}}},9}});
c.conditions.push_back({{false,968,4},{{{"start",{52,1036831948,2051}}},9}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{false,968,1},{{{"start",{21,1036831948,2051}},{"sync",{}}},16}});
c.conditions.push_back({{false,968,2},{{{"start",{31,1036831948,2051}},{"sync",{}}},16}});
c.conditions.push_back({{false,968,3},{{{"start",{41,1036831948,2051}},{"sync",{}}},16}});
c.conditions.push_back({{false,968,4},{{{"start",{51,1036831948,2051}},{"sync",{}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{9,1036831948,1036831948,2064}}},18});
c.residues.push_back({{},18});
c.conditions.push_back({{false,968,1},{{{"blend",{19,1036831948,1036831948,2064}}},18}});
c.conditions.push_back({{false,968,2},{{{"blend",{29,1036831948,1036831948,2064}}},18}});
c.conditions.push_back({{false,968,3},{{{"blend",{39,1036831948,1036831948,2064}}},18}});
c.conditions.push_back({{false,968,4},{{{"blend",{49,1036831948,1036831948,2064}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1036831948,3}}},19};
c.conditions.push_back({{false,968,1},{{{"start",{20,1036831948,3}}},19}});
c.conditions.push_back({{false,968,2},{{{"start",{30,1036831948,3}}},19}});
c.conditions.push_back({{false,968,3},{{{"start",{40,1036831948,3}}},19}});
c.conditions.push_back({{false,968,4},{{{"start",{50,1036831948,3}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{6,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{6,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{7,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
c.conditions.push_back({{false,12,2},{{{"start",{6,1045220556,2054}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,968,1},{{{"start",{16,1045220556,2054}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,968,2},{{{"start",{26,1045220556,2054}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,968,3},{{{"start",{36,1045220556,2054}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,968,4},{{{"start",{46,1045220556,2054}},{"vslot",{28}}},20}});
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{1053609164,1},{0,0},{0,0}}};
t.events[6]={{{1076887552,2},{0,0},{0,0},{0,0}}};
t.events[7]={{{1074790400,2},{0,0},{0,0},{0,0}}};
t.events[8]={{{1075838976,2},{0,0},{0,0},{0,0}}};
t.events[12]={{{1045220556,1},{1053609164,1},{0,0},{0,0}}};
t.events[13]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[15]={{{1045220556,1},{1053609164,1},{0,0},{0,0}}};
t.events[16]={{{1076677836,2},{0,0},{0,0},{0,0}}};
t.events[17]={{{1074580684,2},{0,0},{0,0},{0,0}}};
t.events[18]={{{1075838976,4},{0,0},{0,0},{0,0}}};
t.events[22]={{{1045220556,1},{1053609164,1},{1058642329,1},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitTargetInArc"};
t.consumers[7][1]={"Creature_hitTargetInArc"};
t.consumers[20][2]={"Clockwork_shedParts","store +c:4=4003"};
t.consumers[20][4]={"fx_ringBurst","res_find","sound_playOnObject","world_damageInRadius","GameObject_rand","decal_add","pool_new","Clockwork_shedParts","store +c:4=4003","LightEffect_ctor"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_13() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"holder",{14}},{"start",{21,1050253721,2051}},{"sync",{}}},1};
c.conditions.push_back({{true,0,1},{{{"holder",{14}},{"start",{9,1050253721,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"holder",{14}},{"start",{21,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{true,0,1},{{{"holder",{14}},{"start",{9,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{17,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{12,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{12,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{5,1036831948,3}},{"blend",{4,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{19,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"holder",{14}},{"blend",{20,1036831948,1036831948,2056}}},9};
c.conditions.push_back({{true,0,1},{{{"holder",{14}},{"blend",{7,1036831948,1036831948,2056}}},9}});
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{18,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},11};
}
{ auto &c=t.states[16]; c.baseline={{{"holder",{14}},{"start",{21,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{true,0,1},{{{"holder",{14}},{"start",{9,1036831948,2051}},{"sync",{}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{14,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{14,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{15,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{10,1050253721,6}},{"holder",{1}},{"holder",{11}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{10,1050253721,6}},{"holder",{1}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{11,1050253721,6}},{"holder",{1}},{"holder",{11}},{"vslot",{28}}},20});
c.conditions.push_back({{false,12,2},{{{"start",{10,1050253721,6}},{"holder",{1}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},23};
}
t.events[4]={{{1067869798,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1065353216,1},{0,0},{0,0},{0,0}}};
t.events[10]={{{1073741824,128},{1076258406,2},{0,0},{0,0}}};
t.events[11]={{{1073741824,128},{1076258406,2},{0,0},{0,0}}};
t.events[12]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[13]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[18]={{{1051931443,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1060320051,1},{0,0},{0,0},{0,0}}};
t.events[20]={{{1067030937,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{16,1}}};
t.consumers[7][1]={"sound_playOnObject","model_getAttachPoint","GameObject_rand","store +534:4=0","store +3cc:1=1","store +534:4=1080000"};
t.consumers[8][1]={"camera_shake","world_collectObjectsInBox"};
t.consumers[11][1]={"terrain_height","math_cos16","math_sin16","pool_new","holder.slot1","store +3d4:4=f0","store +530:4=0","BoltGoRound_ctor"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][2]={"store +c:4=1"};
t.consumers[23][1]={"holder.slot9","holder.slot1","sound_playOnObject"};
t.transitions[6]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[18]={{0},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_14() { DispatchTable t; t.group="none";
return t; }
static DispatchTable make_dispatch_15() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{17,1050253721,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1050253721,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{7,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{9,1036831948,3}},{"sync",{}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{14,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{14,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{13,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{16,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{11,1045220556,2054}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"start",{15,1036831948,3}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{4,1036831948,2056}}},24};
c.residues.push_back({{{"start",{4,1036831948,2056}}},24});
c.residues.push_back({{{"start",{3,1036831948,2056}}},24});
}
{ auto &c=t.states[25]; c.baseline={{{"blend",{12,1036831948,1036831948,2064}}},25};
}
{ auto &c=t.states[26]; c.baseline={{{"start",{17,1050253721,2051}},{"sync",{}}},26};
}
{ auto &c=t.states[27]; c.baseline={{{"start",{18,1050253721,2051}},{"sync",{}}},27};
}
t.events[5]={{{1045220556,1},{1053609164,128},{0,0},{0,0}}};
t.events[6]={{{1045220556,2},{1053609164,128},{0,0},{0,0}}};
t.events[7]={{{1060320051,4},{1067869798,4},{1071225241,8},{0,0}}};
t.events[8]={{{1036831948,1},{1050253721,2},{0,0},{0,0}}};
t.events[11]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{23}}};
t.consumers[6][128]={"GameObject_rand","setState(0x7)"};
t.consumers[7][128]={"GameObject_rand","setState(0x9)"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[24]={{3},{}};
t.transitions[25]={{17},{}};
return t; }
static DispatchTable make_dispatch_16() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{-1,1050253721,3}}},0};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1052,1},{{{"start",{-1,1050253721,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{-1,1036831948,2051}}},1};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1064,1},{{{"start",{-1,1036831948,2051}}},1}});
}
{ auto &c=t.states[16]; c.baseline={{},0};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1072,1},{{{"start",{-1,1008981770,2051}}},16}});
}
{ auto &c=t.states[22]; c.baseline={{{"start",{-1,1008981770,2051}}},22};
c.conditions.push_back({{false,1064,1},{{{"start",{-1,1008981770,2051}}},22}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{-1,1050253721,3}}},23};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1076,1},{{{"start",{-1,1050253721,3}}},23}});
}
{ auto &c=t.states[24]; c.baseline={{{"start",{-1,1008981770,2051}}},24};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1080,1},{{{"start",{-1,1008981770,2051}}},24}});
}
{ auto &c=t.states[25]; c.baseline={{{"start",{-1,1050253721,3}}},25};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1076,1},{{{"start",{-1,1050253721,3}}},25}});
}
{ auto &c=t.states[26]; c.baseline={{{"start",{-1,1008981770,2051}}},26};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1008981770,2051}}},22}});
c.conditions.push_back({{false,1080,1},{{{"start",{-1,1008981770,2051}}},26}});
}
t.constructor={{"clear",{}},{"set_state",{23}}};
return t; }
static DispatchTable make_dispatch_17() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{13,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{16,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[2]; c.baseline={{{"start",{16,1050253721,2051}},{"sync",{}}},2};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{16,1050253721,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{12,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{8,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{9,1036831948,1036831948,8}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{4,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{16,1050253721,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{11,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{13,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{18,1050253721,6}},{"vslot",{28}}},20};
}
t.events[3]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1056964608,1},{1069547520,2},{1075838976,4},{0,0}}};
t.events[5]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1045220556,4},{1061997772,8},{0,0},{0,0}}};
t.events[18]={{{1080033280,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{13,1}}};
t.consumers[0][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[0][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[1][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[1][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[2][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[2][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[3][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[3][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[5][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[5][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[16][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[16][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[18][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[18][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[19][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[19][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[20][2]={"store +c:4=1"};
t.consumers[20][4]={"model_getAttachPoint","Cyclops_spawnDust"};
t.consumers[20][8]={"model_getAttachPoint","Cyclops_spawnDust"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_18() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{8,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{12,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{10,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{15,1036831948,1036831948,8}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{12,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{6,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{7,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{11,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{10,1050253721,6}},{"vslot",{28}}},20};
c.conditions.push_back({{false,1120,2},{{{"start",{14,1050253721,6}},{"vslot",{28}}},20}});
}
t.events[3]={{{1050253721,1},{1056964608,2},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{1056964608,2},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[15]={{{1065353216,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{8,1}}};
t.consumers[9][1]={"000000"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_19() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{6,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{8,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{5,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{4,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{6,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{9,1045220556,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{9,1045220556,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{10,1045220556,6}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{3,1045220556,6}},{"vslot",{28}}},23};
}
t.events[3]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[9]={{{1056964608,64},{0,0},{0,0},{0,0}}};
t.events[10]={{{1056964608,64},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[20][64]={"pool_new","obj_delete","store +c:4=2","PhProp_ctor2"};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_20() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{16,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{true,0,1},{{{"start",{16,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{14,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{8,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{9,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{5,1036831948,1036831948,18440}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{6,1036831948,1036831948,18440}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{4,1036831948,1036831948,8}}},11};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{16,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{15,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"start",{12,1045220556,2054}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"start",{10,1036831948,3}}},23};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[12]={{{1080033280,128},{0,0},{0,0},{0,0}}};
t.events[16]={{{1045220556,4},{1061997772,8},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[0][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[0][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[1][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[1][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[2][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[2][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[3][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[3][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[4][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[4][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[5][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[5][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[6][1]={"GameObject_rand","Creature_hitTargetInArc","store +3cc:4=0"};
t.consumers[6][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[6][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[7][1]={"math_cos16","math_sin16","pool_new","sound_playOnObject","dustCloud_ctor"};
t.consumers[7][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[7][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[8][1]={"math_cos16","math_sin16","slot16","model_getAttachPoint","GameObject_rand"};
t.consumers[8][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[8][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[9][1]={"Creature_hitTargetInArc"};
t.consumers[9][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[9][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[10][1]={"Creature_hitTargetInArc"};
t.consumers[10][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[10][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[11][1]={"slot16","model_getAttachPoint","math_cos16","math_sin16","pool_new","sound_playOnObject","poisonGasSpell_ctor"};
t.consumers[11][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[11][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[12][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[12][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[13][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[13][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[14][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[14][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[15][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[15][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[16][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[16][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[17][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[17][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[18][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[18][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[19][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[19][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[20][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[20][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[21][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[21][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[22][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[22][8]={"model_getAttachPoint","fx_spawn"};
t.consumers[23][4]={"model_getAttachPoint","fx_spawn"};
t.consumers[23][8]={"model_getAttachPoint","fx_spawn"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_21() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{26,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{28,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{27,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,968,1},{{{"start",{28,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{22,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{18,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{18,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{19,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{20,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{21,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{13,1036831948,1036831948,2056}}},7};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2056}}},7});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,2056}}},7});
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{13,1036831948,1036831948,2056}}},8};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2056}}},8});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,2056}}},8});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{22,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{25,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,732,524288},{{{"start",{4,1036831948,20}},{"push_chained",{5,36}},{"push_chained",{26,2}}},18}});
c.conditions.push_back({{false,968,1},{{{"blend",{15,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{26,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{4,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{4,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{6,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{10,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
}
t.events[4]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[6]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[10]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[13]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[18]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[20]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[21]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{26,1}}};
t.consumers[6][1]={"holder.slot17","Creature_hitTargetInArc"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_22() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1056964608,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1036831948,3}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{15,1036831948,3}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{12,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"start",{12,1036831948,3}},{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"start",{12,1036831948,3}},{"blend",{3,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{15,1036831948,3}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1056964608,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{4,1036831948,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{4,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{5,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{6,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{7,1036831948,6}},{"vslot",{28}}},20});
c.conditions.push_back({{false,12,2},{{{"start",{4,1036831948,6}},{"jump_end",{}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{8,1073741824,3}},{"blend",{14,1008981770,1036831948,8}}},23};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[5]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[6]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{10,1}},{"randomize",{}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_23() { DispatchTable t; t.group="none";
return t; }
static DispatchTable make_dispatch_24() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{7,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"holder",{15}},{"start",{15,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"holder",{15}},{"start",{9,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{12,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"start",{9,1036831948,3}},{"blend",{3,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"start",{10,1036831948,3}},{"blend",{13,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{6,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{7,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{4,1050253721,6}},{"holder",{11}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{4,1050253721,6}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{5,1050253721,6}},{"holder",{11}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{15,1036831948,2051}},{"blend",{11,1036831948,1036831948,2064}}},23};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[5]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[12]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[13]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{7,1}}};
t.transitions[6]={{5},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_25() { DispatchTable t; t.group="none";
return t; }
static DispatchTable make_dispatch_26() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{22,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{6,1050253721,3}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{6,1050253721,3}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{7,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{3,1036831948,1036831948,8}}},6};
c.residues.push_back({{{"start",{7,1036831948,3}},{"blend",{3,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"start",{7,1036831948,3}},{"blend",{4,1036831948,1036831948,8}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{5,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{6,1050253721,3}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"start",{27,1065353216,3}},{"blend",{15,1036831948,1036831948,20}}},25};
c.residues.push_back({{{"start",{27,1065353216,3}},{"blend",{15,1036831948,1036831948,20}}},25});
c.residues.push_back({{{"blend",{16,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{16,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{22,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{15,1050253721,14}},{"vslot",{28}}},20};
c.conditions.push_back({{false,12,2},{{{"start",{8,1050253721,6}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{6,1050253721,3}},{"sync",{}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{24,1036831948,3}}},24};
}
{ auto &c=t.states[26]; c.baseline={{{"start",{6,1036831948,3}},{"blend",{18,1008981770,1036831948,8}}},26};
}
t.events[3]={{{1045220556,2},{0,0},{0,0},{0,0}}};
t.events[4]={{{1045220556,2},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,2},{1053609164,2},{0,0},{0,0}}};
t.events[17]={{{1036831948,1},{1053609164,2},{1054448025,4},{0,0}}};
t.events[18]={{{1036831948,1},{1053609164,2},{1054448025,4},{0,0}}};
t.events[19]={{{1036831948,1},{1053609164,2},{1054448025,4},{0,0}}};
t.events[20]={{{1036831948,1},{1053609164,2},{1054448025,4},{0,0}}};
t.constructor={{"clear",{}},{"push",{21,1}},{"randomize",{}}};
t.consumers[26][1]={"math_cos16","math_sin16","Creature_moveBy","store +3d0:4=1"};
t.consumers[26][2]={"store +3d0:4=0"};
t.consumers[26][4]={"setState(0x3)"};
t.transitions[6]={{26},{}};
t.transitions[18]={{3},{}};
t.transitions[26]={{3},{}};
return t; }
static DispatchTable make_dispatch_27() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{3,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{7,1036831948,2051}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{3,1036831948,3}},{"sync",{}}},5};
c.residues.push_back({{{"start",{3,1036831948,3}},{"sync",{}}},5});
c.residues.push_back({{{"start",{4,1036831948,3}},{"sync",{}}},5});
c.residues.push_back({{{"start",{5,1036831948,3}},{"sync",{}}},5});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{5,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{3,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{3,1036831948,2051}},{"vslot",{28}}},20};
c.conditions.push_back({{true,0,1},{{{"start",{3,1036831948,2051}},{"vslot",{28}}},20}});
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitTargetInArc"};
t.transitions[6]={{5},{}};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_28() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}},{"sync",{}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{10,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{13,1050253721,1050253721,8}}},6};
c.conditions.push_back({{true,0,17301504},{{{"blend",{14,1050253721,1050253721,8}}},6}});
c.conditions.push_back({{false,232,1},{{{"blend",{12,1050253721,1050253721,8}}},6}});
c.conditions.push_back({{false,232,2},{{{"blend",{13,1050253721,1050253721,8}}},6}});
c.conditions.push_back({{false,232,3},{{{"blend",{11,1050253721,1050253721,8}}},6}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{8,1036831948,1036831948,8}}},18};
c.residues.push_back({{{"blend",{8,1036831948,1036831948,8}}},18});
c.residues.push_back({{{"blend",{9,1036831948,1036831948,8}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1036831948,3}},{"sync",{}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{7,1036831948,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{4,1050253721,1050253721,2056}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{5,1050253721,1050253721,2056}}},24};
}
{ auto &c=t.states[25]; c.baseline={{{"blend",{6,1050253721,1050253721,2056}}},25};
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{1056964608,2},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{1056964608,2},{0,0},{0,0}}};
t.events[6]={{{1045220556,1},{1056964608,2},{0,0},{0,0}}};
t.events[7]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[11]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[12]={{{1050253721,2},{0,0},{0,0},{0,0}}};
t.events[13]={{{1045220556,4},{0,0},{0,0},{0,0}}};
t.events[14]={{{1045220556,4},{1050253721,2},{1058642329,1},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"slot16","model_getAttachPoint","math_atan2_16","math_cos16","math_sin16","pool_new","GutChunk_ctor","fx_spawn","world_createObject","GnawSpawn_addChild"};
t.consumers[6][2]={"slot16","model_getAttachPoint","math_atan2_16","math_cos16","math_sin16","pool_new","GutChunk_ctor","fx_spawn","world_createObject","GnawSpawn_addChild"};
t.consumers[6][4]={"slot16","model_getAttachPoint","math_atan2_16","math_cos16","math_sin16","pool_new","GutChunk_ctor","fx_spawn","world_createObject","GnawSpawn_addChild"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[23][1]={"slot16","model_getAttachPoint","pool_new","fx_spawn","GutChunk_ctor"};
t.consumers[23][2]={"slot16","model_buildHideMask","store +e8:4=1","store +220:8=0","store +220:8=1080000"};
t.consumers[24][1]={"slot16","model_getAttachPoint","pool_new","fx_spawn","GutChunk_ctor"};
t.consumers[24][2]={"slot16","model_buildHideMask","store +e8:4=2","store +220:8=0","store +220:8=1080000"};
t.consumers[25][1]={"slot16","model_getAttachPoint","pool_new","fx_spawn","GutChunk_ctor"};
t.consumers[25][2]={"slot16","model_buildHideMask","store +e8:4=4","store +220:8=0","store +220:8=1080000"};
t.transitions[6]={{3},{}};
t.transitions[18]={{5},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
t.transitions[25]={{3},{}};
return t; }
static DispatchTable make_dispatch_29() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{3,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{true,0,1},{{{"start",{19,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{12,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{7,1036831948,1036831948,8}}},6};
c.residues.push_back({{{"blend",{7,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"blend",{8,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"blend",{9,1036831948,1036831948,8}}},6});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{11,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{3,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{20,1036831948,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{20,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{21,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{22,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{23,1036831948,6}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{15,1036831948,3}}},23};
}
t.constructor={{"clear",{}},{"push",{3,1}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_30() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{3,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{},0};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{3,1036831948,6}}},5};
}
{ auto &c=t.states[6]; c.baseline={{},0};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{3,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{3,1036831948,6}},{"vslot",{28}}},20};
}
t.constructor={{"clear",{}},{"push",{3,1}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
return t; }
static DispatchTable make_dispatch_31() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{13,1050253721,3}}},0};
c.conditions.push_back({{false,980,4},{{{"start",{7,1050253721,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{17,1050253721,3}},{"sync",{}}},1};
}
{ auto &c=t.states[2]; c.baseline={{{"start",{14,1050253721,3}},{"sync",{}}},2};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{14,1050253721,3}},{"sync",{}}},3};
c.conditions.push_back({{true,0,1},{{{"start",{17,1050253721,3}},{"sync",{}}},3}});
}
{ auto &c=t.states[4]; c.baseline={{{"start",{14,1050253721,3}},{"sync",{}}},4};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{12,1036831948,3}}},5};
c.conditions.push_back({{false,980,4},{{{"start",{7,1036831948,3}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
c.conditions.push_back({{false,980,1},{{{"blend",{3,1036831948,1036831948,8}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{6,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{16,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{14,1050253721,3}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{18,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{9,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{10,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{11,1036831948,1036831948,16}}},18});
c.conditions.push_back({{false,732,262144},{{},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{13,1050253721,3}}},19};
c.conditions.push_back({{false,980,4},{{{"start",{7,1050253721,3}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"start",{8,1050253721,6}},{"vslot",{28}}},20};
c.conditions.push_back({{true,0,1},{{{"holder",{11}},{"holder",{11}},{"start",{8,1050253721,6}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,12,2},{{{"start",{8,1050253721,6}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,980,4},{{{"holder",{11}},{"holder",{11}},{"start",{8,1050253721,6}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{12,1036831948,3}}},23};
c.conditions.push_back({{false,980,4},{{{"start",{7,1036831948,3}}},23}});
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,2},{1058642329,2},{1065353216,1},{0,0}}};
t.events[6]={{{1058642329,1},{1066192076,2},{0,0},{0,0}}};
t.events[8]={{{1065353216,128},{1065353216,2},{0,0},{0,0}}};
t.events[16]={{{1069547520,1},{0,0},{0,0},{0,0}}};
t.event_variants["26a624/v1"][3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.event_variants["26a624/v1"][4]={{{1050253721,2},{1058642329,2},{1065353216,1},{0,0}}};
t.event_variants["26a624/v1"][6]={{{1063675494,1},{1070386380,2},{0,0},{0,0}}};
t.event_variants["26a624/v1"][16]={{{1069547520,1},{0,0},{0,0},{0,0}}};
t.event_variants["26a624/v1"][8]={{{1065353216,128},{1065353216,2},{0,0},{0,0}}};
t.event_variants["26a624/v2"][3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.event_variants["26a624/v2"][4]={{{1050253721,2},{1058642329,2},{1065353216,1},{0,0}}};
t.event_variants["26a624/v2"][6]={{{1063675494,1},{1070386380,2},{0,0},{0,0}}};
t.event_variants["26a624/v2"][16]={{{1069547520,1},{0,0},{0,0},{0,0}}};
t.event_variants["26a624/v2"][8]={{{1065353216,128},{1065353216,2},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{13,1}}};
t.consumers[20][2]={"store +c:4=1"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{23},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_32() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{6,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{6,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{6,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{6,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"start",{3,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"holder",{11}},{"start",{3,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"start",{4,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"start",{5,1045220556,2054}},{"vslot",{28}}},20});
}
t.events[3]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[4]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[5]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1056964608,1},{1061997772,2},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"holder.slot1","math_cos16","math_sin16","Creature_leadTargetPoint","slot16","model_getAttachPoint","GameObject_rand"};
t.consumers[6][2]={"holder.slot1"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{5},{}};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_33() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{14,1036831948,3}}},0};
c.conditions.push_back({{true,0,1},{{{"start",{23,1036831948,35}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{25,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{true,0,1},{{{"start",{20,1036831948,2083}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{25,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{true,0,1},{{{"start",{20,1036831948,2083}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{15,1036831948,3}}},5};
c.conditions.push_back({{true,0,1},{{{"start",{23,1036831948,35}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},6};
c.conditions.push_back({{true,0,1},{{{"blend",{6,1036831948,1036831948,2056}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{12,1036831948,3}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{11,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{21,1036831948,1036831948,2088}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{24,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{25,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{true,0,1},{{{"start",{20,1036831948,2083}},{"sync",{}}},16}});
}
{ auto &c=t.states[17]; c.baseline={{{"start",{18,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,2064}}},18};
c.conditions.push_back({{true,0,1},{{},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{14,1050253721,3}}},19};
c.conditions.push_back({{true,0,1},{{{"start",{23,1050253721,35}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{7,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{7,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{9,1045220556,2054}},{"vslot",{28}}},20});
c.conditions.push_back({{true,0,1},{{{"blend",{19,1036831948,0,32}},{"push_chained",{7,2054}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{22,1036831948,0,40}},{"push_chained",{23,35}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{19,1036831948,0,32}},{"push_chained",{15,3}}},24};
}
{ auto &c=t.states[25]; c.baseline={{{"blend",{10,1036831948,0,32776}},{"push_chained",{15,32771}}},25};
}
{ auto &c=t.states[26]; c.baseline={{{"start",{14,1036831948,3}}},26};
}
t.events[6]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[9]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[11]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[12]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[21]={{{1050253721,1},{1067030937,2},{0,0},{0,0}}};
t.events[24]={{{1050253721,1},{1067030937,2},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Hag_isGrownForm","Creature_hitTargetInArc"};
t.consumers[7][1]={"SpellConeOfShock_typeinfo","creature_findSpellByType","slot35","000000","creature_addSpell","store +3d0:4=12c"};
t.consumers[8][1]={"slot35","000000","store +3d0:4=12c"};
t.consumers[10][1]={"pool_new","LightningBolt_ctor"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{5,16},{}};
t.transitions[8]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[18]={{5},{}};
t.transitions[23]={{3,16},{}};
t.transitions[24]={{5},{}};
t.transitions[25]={{5},{}};
t.transitions[26]={{0},{}};
return t; }
static DispatchTable make_dispatch_34() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}}},0};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[2]; c.baseline={{{"vslot",{27,3}}},2};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[4]; c.baseline={{{"vslot",{27,3}}},4};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{7,1036831948,3}}},5};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},7};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{14,1036831948,1036831948,2056}}},8};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[9]; c.baseline={{{"vslot",{27,3}}},9};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},10};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},11};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[12]; c.baseline={{{"vslot",{27,3}}},12};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[13]; c.baseline={{{"vslot",{27,3}}},13};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[14]; c.baseline={{{"vslot",{27,3}}},14};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[15]; c.baseline={{{"vslot",{27,3}}},15};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[17]; c.baseline={{{"start",{7,1036831948,3}}},17};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1050253721,3}}},19};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"start",{15,1036831948,6}},{"vslot",{28}}},20};
c.conditions.push_back({{false,968,1},{{},0}});
c.conditions.push_back({{false,1136,3},{{{"holder",{11}},{"holder",{11}},{"start",{15,1036831948,6}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[21]; c.baseline={{{"vslot",{27,3}}},21};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[22]; c.baseline={{{"vslot",{27,3}}},22};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},23};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{12,1036831948,1036831948,2056}}},24};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[25]; c.baseline={{{"start",{20,1036831948,3}}},25};
}
{ auto &c=t.states[26]; c.baseline={{{"start",{20,1036831948,3}}},26};
}
{ auto &c=t.states[27]; c.baseline={{{"start",{20,1036831948,3}}},27};
}
{ auto &c=t.states[28]; c.baseline={{{"start",{26,1036831948,3}}},28};
}
{ auto &c=t.states[29]; c.baseline={{{"start",{25,1036831948,3}}},29};
}
{ auto &c=t.states[30]; c.baseline={{{"start",{23,1036831948,3}}},30};
}
{ auto &c=t.states[31]; c.baseline={{{"start",{24,1036831948,3}}},31};
}
{ auto &c=t.states[32]; c.baseline={{{"blend",{17,1036831948,1036831948,8}}},32};
}
{ auto &c=t.states[33]; c.baseline={{{"blend",{18,1036831948,1036831948,8}}},33};
}
{ auto &c=t.states[34]; c.baseline={{{"blend",{19,1036831948,1036831948,8}}},34};
}
{ auto &c=t.states[35]; c.baseline={{{"start",{22,1036831948,2060}}},35};
c.conditions.push_back({{false,968,1},{{{"start",{22,1036831948,2060}}},35}});
}
t.events[3]={{{1045220556,1},{1058642329,1},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{1058642329,1},{1065353216,1},{0,0}}};
t.events[5]={{{1050253721,2},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{1061997772,1},{1070386380,1},{0,0}}};
t.events[8]={{{1073741824,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[15]={{{1036831948,128},{0,0},{0,0},{0,0}}};
t.events[17]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[18]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[24]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{10,1}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[17]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
t.transitions[35]={{0},{}};
return t; }
static DispatchTable make_dispatch_35() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1036831948,3}}},0};
c.conditions.push_back({{false,968,1},{{{"start",{14,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{19,1050253721,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,968,1},{{{"start",{20,1050253721,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{19,1036831948,2051}}},3};
c.conditions.push_back({{false,968,1},{{{"start",{20,1036831948,2051}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{15,1036831948,3}}},5};
c.conditions.push_back({{false,968,1},{{{"start",{14,1036831948,3}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"start",{14,1036831948,3}},{"blend",{10,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[8]; c.baseline={{{"start",{6,1036831948,3}}},8};
c.residues.push_back({{{"start",{6,1036831948,3}}},8});
c.residues.push_back({{{"start",{7,1036831948,3}}},8});
c.residues.push_back({{{"start",{8,1036831948,3}}},8});
}
{ auto &c=t.states[9]; c.baseline={{{"start",{15,1036831948,3}},{"blend",{16,1036831948,1036831948,8}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"start",{14,1036831948,3}},{"blend",{3,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"start",{14,1036831948,3}},{"blend",{9,1036831948,1036831948,16392}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"start",{14,1036831948,3}},{"blend",{17,1036831948,1036831948,2056}}},12};
}
{ auto &c=t.states[13]; c.baseline={{{"start",{14,1036831948,3}},{"blend",{12,1036831948,1036831948,8}}},13};
}
{ auto &c=t.states[14]; c.baseline={{{"start",{14,1036831948,3}},{"blend",{11,1036831948,1036831948,2056}}},14};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{19,1036831948,2051}}},16};
c.conditions.push_back({{false,968,1},{{{"start",{20,1036831948,2051}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{},0};
c.conditions.push_back({{false,968,1},{{{"blend",{5,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{4,1050253721,7}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"start",{15,1036831948,3}},{"blend",{12,1036831948,1036831948,8}}},23};
}
t.events[3]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1064514355,1},{1061997772,2},{0,0},{0,0}}};
t.events[10]={{{1050253721,1},{1060320051,1},{0,0},{0,0}}};
t.events[11]={{{1065772646,1},{1061997772,2},{0,0},{0,0}}};
t.events[12]={{{1060320051,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1065353216,1},{0,0},{0,0},{0,0}}};
t.events[17]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{15,1}}};
t.consumers[11][1]={"camera_shake","math_cos16","math_sin16","pool_new","GameObject_rand","particle_spawn","LightEffect_ctor"};
t.consumers[11][2]={"store +3d8:4=1","store +490:2=73","store +492:2=28","store +496:2=64","store +498:2=40","store +49e:2=ffc0","store +43c:4=43340000","store +494:2=0","store +49a:2=ffc0","store +49c:2=ffc0"};
t.consumers[12][1]={"sound_playOnObject","math_cos16","math_sin16","pool_new","GameObject_rand","particle_spawn","camera_shake","slot16","fx_modelParticlesNearCamera_1f61c0","store +3d0:4=1","LightEffect_ctor"};
t.consumers[23][1]={"store +3c8:4=1"};
t.transitions[6]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[12]={{3},{}};
t.transitions[13]={{3},{}};
t.transitions[14]={{3},{}};
t.transitions[18]={{0},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_36() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{11,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{12,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,8}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{5,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{6,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{13,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{7,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{9,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{10,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{11,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{12,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{11,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{8,1045220556,6}},{"vslot",{28}}},20};
}
t.events[3]={{{1063675494,1},{1070386380,1},{0,0},{0,0}}};
t.events[4]={{{1060320051,1},{1067869798,1},{1075000115,1},{0,0}}};
t.events[5]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1065353216,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[13]={{{1045220556,1},{1069547520,1},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[7][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[8][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[9][1]={"math_cos16","math_sin16","slot16","model_getAttachPoint","GameObject_rand","store +3c8:4=1"};
t.consumers[10][1]={"math_cos16","math_sin16","slot16","model_getAttachPoint","GameObject_rand"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{3},{}};
t.transitions[18]={{5},{}};
return t; }
static DispatchTable make_dispatch_37() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{18,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{20,1036831948,2051}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{20,1053609164,2051}}},3};
c.conditions.push_back({{false,972,1},{{{"start",{20,1053609164,2051}}},3}});
c.conditions.push_back({{false,972,2},{{{"start",{22,1036831948,2051}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{18,1036831948,3}},{"sync",{}}},5};
c.conditions.push_back({{false,972,2},{{{"start",{19,1036831948,3}},{"sync",{}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{5,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{6,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{7,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{21,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{21,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{20,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{15,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{15,1036831948,1036831948,2064}}},18});
c.residues.push_back({{{"blend",{16,1036831948,1036831948,2064}}},18});
c.conditions.push_back({{false,732,262144},{{{"blend",{17,1036831948,1036831948,2064}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{18,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{8,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{9,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{10,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{11,1045220556,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{12,1036831948,1036831948,2064}}},23};
c.residues.push_back({{{"blend",{12,1036831948,1036831948,2064}}},23});
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2064}}},23});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,2064}}},23});
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1045220556,1},{1056964608,2},{1061997772,2},{1067030937,2}}};
t.events[8]={{{1058642329,128},{0,0},{0,0},{0,0}}};
t.events[9]={{{1058642329,128},{0,0},{0,0},{0,0}}};
t.events[10]={{{1058642329,128},{0,0},{0,0},{0,0}}};
t.events[11]={{{1058642329,128},{0,0},{0,0},{0,0}}};
t.events[17]={{{1069547520,8},{0,0},{0,0},{0,0}}};
t.events[21]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{18,1}},{"set_state",{0}}};
t.consumers[8][1]={"creature_findCorpseOfClass","creature_findWoundedAlly","other.slot32"};
t.consumers[9][1]={"creature_findCorpseOfClass","creature_findWoundedAlly","slot35","000000","creature_addSpell","other.slot32"};
t.consumers[18][8]={"store +2e8:4=0"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_38() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{8,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{24,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{24,1045220556,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{8,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{17,1045220556,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{17,1045220556,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{18,1045220556,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{19,1045220556,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{15,1045220556,1036831948,2056}}},7};
c.residues.push_back({{{"blend",{15,1045220556,1036831948,2056}}},7});
c.residues.push_back({{{"blend",{16,1045220556,1036831948,2056}}},7});
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{22,1045220556,1036831948,2056}}},8};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{24,1045220556,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{5,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{8,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{3,1036831948,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{3,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{4,1036831948,6}},{"vslot",{28}}},20});
}
t.events[3]={{{1066192076,4},{0,0},{0,0},{0,0}}};
t.events[4]={{{1056964608,4},{0,0},{0,0},{0,0}}};
t.events[15]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[17]={{{1056964608,1},{1065353216,1},{0,0},{0,0}}};
t.events[18]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[21]={{{1063675494,4},{1065353216,1},{1069547520,4},{1077936128,2}}};
t.events[22]={{{1063675494,4},{1065353216,1},{1069547520,4},{1078355558,2}}};
t.constructor={{"clear",{}},{"push",{23,1}}};
t.consumers[8][1]={"store +3c8:4=1"};
t.consumers[8][2]={"math_cos16","math_sin16","Creature_leadTargetPoint","slot16","model_getAttachPoint","math_atan2_16","pool_new","sound_playOnObject","store +3c8:4=0","MissileWeapon_ctor"};
t.consumers[20][-1]={"Creature_deadUpdate","obj_stopRunning"};
t.transitions[0]={{3},{}};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_39() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{36,1036831948,3}}},0};
c.conditions.push_back({{false,968,1},{{{"start",{31,1036831948,3}}},0}});
c.conditions.push_back({{false,968,2},{{{"start",{24,1036831948,3}}},0}});
c.conditions.push_back({{false,968,3},{{{"start",{18,1036831948,3}}},0}});
c.conditions.push_back({{false,968,4},{{{"start",{9,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{37,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,968,1},{{{"start",{33,1036831948,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,968,2},{{{"start",{27,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{37,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,968,1},{{{"start",{32,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,968,2},{{{"start",{26,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,968,4},{{{"start",{10,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{},5};
c.conditions.push_back({{false,968,1},{{{"start",{31,1036831948,3}}},5}});
c.conditions.push_back({{false,968,2},{{{"start",{22,1036831948,3}}},5}});
c.conditions.push_back({{false,968,3},{{{"start",{16,1036831948,3}}},5}});
c.conditions.push_back({{false,968,4},{{{"start",{5,1036831948,3}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{},6};
c.conditions.push_back({{false,968,1},{{{"start",{31,1036831948,3}},{"blend",{28,1036831948,1036831948,2056}}},6}});
c.conditions.push_back({{false,968,2},{{{"start",{22,1036831948,3}},{"blend",{19,1036831948,1036831948,2056}}},6}});
c.conditions.push_back({{false,968,3},{{{"start",{16,1036831948,3}},{"blend",{13,1036831948,1036831948,2056}}},6}});
c.conditions.push_back({{false,968,4},{{{"start",{5,1036831948,3}},{"blend",{3,1036831948,1036831948,2056}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{9,1036831948,3}},{"blend",{12,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"start",{9,1036831948,3}},{"blend",{11,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"start",{36,1036831948,3}},{"blend",{34,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{26,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{},17};
c.conditions.push_back({{false,968,1},{{{"start",{31,1036831948,3}}},17}});
c.conditions.push_back({{false,968,2},{{{"start",{22,1036831948,3}}},17}});
c.conditions.push_back({{false,968,3},{{{"start",{16,1036831948,3}}},17}});
c.conditions.push_back({{false,968,4},{{{"start",{5,1036831948,3}}},17}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{35,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,968,1},{{{"blend",{30,1036831948,1036831948,16}}},18}});
c.conditions.push_back({{false,968,2},{{{"blend",{25,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{36,1036831948,3}}},19};
c.conditions.push_back({{false,968,1},{{{"start",{31,1036831948,3}}},19}});
c.conditions.push_back({{false,968,2},{{{"start",{24,1036831948,3}}},19}});
c.conditions.push_back({{false,968,3},{{{"start",{18,1036831948,3}}},19}});
c.conditions.push_back({{false,968,4},{{{"start",{9,1036831948,3}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{38,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{38,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{40,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{42,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{44,1036831948,6}},{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20});
c.conditions.push_back({{false,12,2},{{{"start",{38,1036831948,6}},{"vslot",{28}}},20}});
}
t.events[3]={{{1036831948,1},{1045220556,1},{1053609164,1},{0,0}}};
t.events[4]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[11]={{{1056964608,2},{1058642329,4},{1065353216,1},{0,0}}};
t.events[12]={{{1045220556,2},{1058642329,1},{0,0},{0,0}}};
t.events[13]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[15]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[20]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[21]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[28]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[29]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[34]={{{1045220556,1},{1066192076,2},{0,0},{0,0}}};
t.events[38]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[40]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[42]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[44]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.transitions[7]={{16},{}};
t.transitions[8]={{16},{}};
t.transitions[9]={{16},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_40() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{4,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,1120,1},{{{"start",{15,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{3,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{10,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{11,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{12,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{9,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{13,1036831948,1036831948,2056}}},11};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{14,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{4,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{16,1045220556,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"start",{6,1036831948,3}},{"vslot",{35}}},23};
c.conditions.push_back({{true,0,1},{{{"start",{6,1036831948,3}}},23}});
c.conditions.push_back({{false,7224064,1048576},{{{"start",{6,1036831948,3}},{"vslot",{35}}},23}});
c.conditions.push_back({{false,7224064,2097152},{{{"start",{6,1036831948,3}},{"vslot",{35}}},23}});
c.conditions.push_back({{false,7224064,3670016},{{{"start",{6,1036831948,3}},{"vslot",{35}}},23}});
}
{ auto &c=t.states[24]; c.baseline={{{"start",{7,1036831948,3}},{"vslot",{35}}},24};
c.conditions.push_back({{true,0,1},{{{"start",{7,1036831948,3}}},24}});
c.conditions.push_back({{false,7224064,1048576},{{{"start",{7,1036831948,3}},{"vslot",{35}}},24}});
c.conditions.push_back({{false,7224064,2097152},{{{"start",{7,1036831948,3}},{"vslot",{35}}},24}});
c.conditions.push_back({{false,7224064,3670016},{{{"start",{7,1036831948,3}},{"vslot",{35}}},24}});
}
t.events[5]={{{1069547520,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[10]={{{1045220556,1},{1060320051,1},{1067869798,1},{1072064102,1}}};
t.events[11]={{{1045220556,1},{1061997772,1},{0,0},{0,0}}};
t.events[12]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[13]={{{1065353216,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1075125810,64},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"holder.slot17","Creature_hitAllEnemiesInArc","store +460:1=2"};
t.consumers[7][1]={"holder.slot17","Creature_hitAllEnemiesInArc","store +460:1=2"};
t.consumers[8][1]={"holder.slot17","Creature_hitAllEnemiesInArc","store +460:1=2"};
t.consumers[9][1]={"slot35","000000","creature_addSpell","store +460:1=0"};
t.consumers[10][1]={"holder.slot17","Creature_hitAllEnemiesInArc","store +460:1=2"};
t.consumers[11][1]={"holder.slot9","holder.slot17","Creature_hitAllEnemiesInArc","store +460:1=0"};
t.consumers[20][-1]={"Creature_deadUpdate","Marr_deathEffects"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_41() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{11,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{10,1050253721,2051}}},1};
}
{ auto &c=t.states[5]; c.baseline={{{"blend",{11,1036831948,1036831948,16}}},5};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{11,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{11,1050253721,6}}},20};
}
t.constructor={{"clear",{}},{"push",{11,1}}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_42() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{13,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{20,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{20,1050253721,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{13,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{5,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{16,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{17,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{7,1036831948,1036831948,2056}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},12};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{20,1050253721,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{13,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{18,1050253721,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{15,1036831948,1036831948,2064}}},23};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1073741824,1},{1075838976,2},{0,0},{0,0}}};
t.events[15]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1060320051,1},{0,0},{0,0},{0,0}}};
t.events[17]={{{1070805811,1},{1008981770,64},{1070805811,128},{0,0}}};
t.events[19]={{{1008981770,16},{1082130432,32},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{13,1}}};
t.consumers[6][1]={"Creature_hitTargetOrPlayerInArc","store +3d8:4=1"};
t.consumers[7][1]={"Creature_hitTargetOrPlayerInArc","handle_clear","store +3d8:4=1","store +3c8:4=0"};
t.consumers[8][1]={"handle_clear","store +3d8:4=1","store +3c8:4=0"};
t.consumers[9][1]={"000000","math_cos16","math_sin16","pool_new","RainOfFireSpell_ctor"};
t.consumers[9][2]={"store +3d8:4=1"};
t.consumers[10][1]={"Creature_hitTargetOrPlayerInArc","store +3d8:4=1"};
t.consumers[11][1]={"Creature_hitTargetOrPlayerInArc","store +3d8:4=1"};
t.consumers[12][1]={"Creature_hitTargetOrPlayerInArc","store +3d8:4=1"};
t.consumers[23][1]={"model_getAttachPoint","pool_new","Soul_ctor","handle_link","other.slot27"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[12]={{3},{}};
t.transitions[18]={{0},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_43() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{20,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{7,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{8,1036831948,1036831948,16392}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{9,1036831948,1036831948,16392}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{4,1036831948,1036831948,18440}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"start",{17,1036831948,2059}},{"blend",{19,1036831948,1036831948,8}}},12};
c.conditions.push_back({{false,1132,1},{{{"start",{17,1036831948,2059}}},12}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{17,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{14,1036831948,1036831948,2064}}},18};
c.conditions.push_back({{false,552,3212836864},{{{"holder",{11}},{"holder",{11}},{"blend",{14,1036831948,1036831948,2064}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{15,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"start",{11,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{11,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{12,1045220556,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{10,1036831948,3}}},23};
}
t.events[3]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1058642329,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1045220556,1},{1058642329,1},{0,0},{0,0}}};
t.events[7]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1045220556,1},{1052266987,4},{0,0},{0,0}}};
t.events[9]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[13]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitTargetInArc"};
t.consumers[7][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[8][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[9][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[10][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[11][1]={"holder.slot17","Creature_hitTargetInArc"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[12]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_44() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{12,1056964608,2051}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{12,1036831948,2051}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{12,1036831948,2051}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2064}}},18});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,2064}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{16,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{5,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{5,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{6,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{7,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1045220556,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{9,1036831948,1036831948,2064}}},23};
c.residues.push_back({{{"blend",{9,1036831948,1036831948,2064}}},23});
c.residues.push_back({{{"blend",{10,1036831948,1036831948,2064}}},23});
c.residues.push_back({{{"blend",{11,1036831948,1036831948,2064}}},23});
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1065353216,128},{0,0},{0,0},{0,0}}};
t.events[6]={{{1069547520,128},{0,0},{0,0},{0,0}}};
t.events[7]={{{1069547520,128},{0,0},{0,0},{0,0}}};
t.events[8]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_45() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}}},0};
c.conditions.push_back({{false,968,1},{{{"start",{40,1036831948,3}}},0}});
c.conditions.push_back({{false,968,4},{{{"start",{25,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,968,1},{{{"start",{42,1036831948,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,968,4},{{{"start",{31,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{12,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,968,1},{{{"start",{42,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,968,4},{{{"start",{30,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{10,1036831948,3}},{"sync",{}}},5};
c.conditions.push_back({{false,968,1},{{{"start",{40,1036831948,3}},{"sync",{}}},5}});
c.conditions.push_back({{false,968,4},{{{"start",{20,1036831948,3}},{"sync",{}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
c.conditions.push_back({{false,968,1},{{{"blend",{14,1036831948,1036831948,2056}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{10,1036831948,3}},{"blend",{32,1036831948,1036831948,2056}}},7};
c.residues.push_back({{{"start",{10,1036831948,3}},{"blend",{32,1036831948,1036831948,2056}}},7});
c.residues.push_back({{{"start",{10,1036831948,3}},{"blend",{33,1036831948,1036831948,2056}}},7});
c.residues.push_back({{{"start",{10,1036831948,3}},{"blend",{34,1036831948,1036831948,2056}}},7});
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{17,1036831948,1036831948,2056}}},8};
c.residues.push_back({{{"blend",{17,1036831948,1036831948,2056}}},8});
c.residues.push_back({{{"blend",{18,1036831948,1036831948,2056}}},8});
c.residues.push_back({{{"blend",{19,1036831948,1036831948,2056}}},8});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{false,968,1},{{{"start",{41,1036831948,2051}},{"sync",{}}},16}});
c.conditions.push_back({{false,968,4},{{{"start",{29,1036831948,2051}},{"sync",{}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,968,1},{{{"blend",{39,1036831948,1036831948,16}}},18}});
c.conditions.push_back({{false,968,4},{{{"blend",{24,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{7,1050253721,2054}},{"holder",{11}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{7,1050253721,2054}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1050253721,2054}},{"holder",{11}},{"vslot",{28}}},20});
c.conditions.push_back({{false,968,1},{{{"start",{37,1050253721,2054}},{"holder",{11}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,968,4},{{{"start",{22,1050253721,2054}},{"holder",{11}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{10,1036831948,3}},{"blend",{43,1036831948,1036831948,2056}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},24};
c.conditions.push_back({{false,968,1},{{{"start",{41,1036831948,2051}},{"sync",{}}},24}});
c.conditions.push_back({{false,968,4},{{{"start",{29,1036831948,2051}},{"sync",{}}},24}});
}
t.events[3]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1041865113,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[15]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1036831948,1},{1056964608,1},{0,0},{0,0}}};
t.events[17]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[18]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[19]={{{1036831948,1},{1056964608,1},{0,0},{0,0}}};
t.events[32]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[33]={{{1069547520,1},{0,0},{0,0},{0,0}}};
t.events[34]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[43]={{{1045220556,1},{1061997772,2},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[23][1]={"pool_new","DustDevil_ctor"};
t.consumers[23][2]={"sound_playOnObject","Creature_relocateAlongClearRay","pool_new","store +a8:8=8000020000000800","store +c:4=3","DustDevil_ctor"};
t.transitions[6]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_46() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{8,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{8,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{10,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"start",{12,1036831948,3}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{7,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{8,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{5,1036831948,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"start",{11,1036831948,3}}},23};
c.conditions.push_back({{true,0,1},{{{"start",{11,1036831948,3}}},23}});
}
{ auto &c=t.states[24]; c.baseline={{{"start",{11,1036831948,3}}},24};
}
t.events[3]={{{1050253721,1},{1063675494,1},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.events[10]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{8,1}}};
t.consumers[8][1]={"math_cos16","math_sin16"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_47() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{},0};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1036831948,3}}},0}});
c.conditions.push_back({{false,1144,1},{{},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{-1,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{true,0,1},{{{"start",{-1,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[23]; c.baseline={{},23};
}
t.constructor={{"clear",{}},{"push",{-1,1}},{"randomize",{}}};
return t; }
static DispatchTable make_dispatch_48() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{3,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{},0};
}
{ auto &c=t.states[3]; c.baseline={{},0};
}
{ auto &c=t.states[5]; c.baseline={{},0};
}
{ auto &c=t.states[6]; c.baseline={{},0};
}
{ auto &c=t.states[16]; c.baseline={{},0};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{},0};
}
{ auto &c=t.states[20]; c.baseline={{},0};
}
t.constructor={{"clear",{}},{"push",{3,1}}};
return t; }
static DispatchTable make_dispatch_49() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{3,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{7,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{3,1050253721,6}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{8,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{8,1036831948,0,36}},{"push_chained",{5,2059}}},7};
}
{ auto &c=t.states[8]; c.baseline={{},8};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{7,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{3,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"vslot",{28}},{"start",{5,1050253721,2054}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{8,1036831948,1036831948,8}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{8,1036831948,1036831948,8}}},24};
}
{ auto &c=t.states[25]; c.baseline={{},25};
}
{ auto &c=t.states[26]; c.baseline={{{"start",{3,1036831948,3}}},26};
}
t.events[4]={{{1056964608,1},{1061997772,2},{1067030937,2},{1069547520,4}}};
t.events[8]={{{1056964608,1},{1061997772,2},{1067030937,2},{1069547520,4}}};
t.constructor={{"clear",{}},{"push",{3,1}},{"set_state",{0}}};
t.consumers[6][1]={"Creature_hitAllEnemiesInArc","Creature_turnToTarget","store +3d4:4=1"};
t.consumers[6][2]={"Creature_hitAllEnemiesInArc"};
t.consumers[6][4]={"Creature_hitAllEnemiesInArc","store +3d4:4=0"};
t.consumers[23][1]={"Creature_hitAllEnemiesInArc","Creature_turnToTarget","store +3d4:4=1"};
t.consumers[23][2]={"Creature_hitAllEnemiesInArc"};
t.consumers[23][4]={"Creature_hitAllEnemiesInArc","store +3d4:4=0"};
t.consumers[24][1]={"Creature_hitAllEnemiesInArc","Creature_turnToTarget","store +3d4:4=1"};
t.consumers[24][2]={"Creature_hitAllEnemiesInArc"};
t.consumers[24][4]={"Creature_hitAllEnemiesInArc","store +3d4:4=0"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_50() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{11,1053609164,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{10,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{8,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{8,1036831948,1036831948,2064}}},18});
c.residues.push_back({{{"blend",{9,1036831948,1036831948,2064}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"start",{6,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"holder",{11}},{"start",{6,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"start",{7,1045220556,2054}},{"vslot",{28}}},20});
}
t.events[3]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{1060320051,1},{1067869798,4},{0,0}}};
t.events[6]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{10,1}},{"set_state",{0}}};
t.consumers[6][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[7][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[8][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[8][4]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[9][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[9][4]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_51() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{31,1050253721,3}}},0};
c.residues.push_back({{{"start",{31,1050253721,3}}},0});
c.residues.push_back({{{"start",{33,1050253721,3}}},0});
c.residues.push_back({{{"start",{35,1050253721,3}}},0});
c.conditions.push_back({{false,1144,4},{{{"start",{39,1050253721,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{43,1050253721,3}},{"sync",{}}},1};
c.residues.push_back({{{"start",{43,1050253721,3}},{"sync",{}}},1});
c.residues.push_back({{{"start",{44,1050253721,3}},{"sync",{}}},1});
c.conditions.push_back({{false,1144,2},{{{"start",{41,1050253721,3}},{"sync",{}}},1}});
}
{ auto &c=t.states[2]; c.baseline={{{"start",{17,1050253721,3}},{"sync",{}}},2};
c.residues.push_back({{{"start",{17,1050253721,3}},{"sync",{}}},2});
c.residues.push_back({{{"start",{18,1050253721,3}},{"sync",{}}},2});
c.conditions.push_back({{false,1144,2},{{{"start",{41,1050253721,3}},{"sync",{}}},2}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{17,1050253721,3}},{"sync",{}}},3};
c.residues.push_back({{{"start",{17,1050253721,3}},{"sync",{}}},3});
c.residues.push_back({{{"start",{18,1050253721,3}},{"sync",{}}},3});
c.conditions.push_back({{false,1144,1},{{{"start",{43,1050253721,3}},{"sync",{}}},3}});
c.conditions.push_back({{false,1144,2},{{{"start",{41,1050253721,3}},{"sync",{}}},3}});
}
{ auto &c=t.states[4]; c.baseline={{{"start",{17,1050253721,3}},{"sync",{}}},4};
c.residues.push_back({{{"start",{17,1050253721,3}},{"sync",{}}},4});
c.residues.push_back({{{"start",{18,1050253721,3}},{"sync",{}}},4});
c.conditions.push_back({{false,1144,2},{{{"start",{15,1050253721,3}},{"sync",{}}},4}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{30,1036831948,3}}},5};
c.residues.push_back({{{"start",{30,1036831948,3}}},5});
c.residues.push_back({{{"start",{32,1036831948,3}}},5});
c.residues.push_back({{{"start",{34,1036831948,3}}},5});
c.conditions.push_back({{true,0,1},{{{"start",{28,1036831948,3}}},5}});
c.conditions.push_back({{false,1144,4},{{{"start",{38,1036831948,3}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"holder",{15}},{"holder",{15}},{"holder",{15}},{"blend",{19,1036831948,1036831948,8}}},6};
c.residues.push_back({{{"holder",{15}},{"holder",{15}},{"holder",{15}},{"blend",{19,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"holder",{15}},{"holder",{15}},{"holder",{15}},{"blend",{20,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"holder",{15}},{"holder",{15}},{"holder",{15}},{"blend",{21,1036831948,1036831948,8}}},6});
c.conditions.push_back({{true,0,1},{{{"holder",{15}},{"blend",{19,1036831948,1036831948,8}}},6}});
c.conditions.push_back({{false,1144,1},{{{"holder",{15}},{"holder",{15}},{"holder",{15}},{"blend",{24,1036831948,1036831948,8}}},6}});
c.conditions.push_back({{false,1144,4},{{{"blend",{22,1036831948,1036831948,8}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{19,1036831948,1036831948,8}}},7};
c.residues.push_back({{{"blend",{19,1036831948,1036831948,8}}},7});
c.residues.push_back({{{"blend",{20,1036831948,1036831948,8}}},7});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{17,1050253721,3}},{"sync",{}}},16};
c.residues.push_back({{{"start",{17,1050253721,3}},{"sync",{}}},16});
c.residues.push_back({{{"start",{18,1050253721,3}},{"sync",{}}},16});
}
{ auto &c=t.states[17]; c.baseline={{{"start",{28,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{11,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{11,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{12,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{13,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{31,1050253721,3}}},19};
c.residues.push_back({{{"start",{31,1050253721,3}}},19});
c.residues.push_back({{{"start",{33,1050253721,3}}},19});
c.residues.push_back({{{"start",{35,1050253721,3}}},19});
c.conditions.push_back({{false,1144,4},{{{"start",{39,1050253721,3}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"start",{4,1050253721,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{4,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{5,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{6,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{7,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{8,1050253721,6}},{"vslot",{28}}},20});
c.conditions.push_back({{false,1144,2},{{{"holder",{11}},{"holder",{11}},{"start",{3,1050253721,6}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,1144,4},{{{"holder",{11}},{"holder",{11}},{"start",{4,1050253721,6}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{31,1036831948,3}}},23};
c.residues.push_back({{{"start",{31,1036831948,3}}},23});
c.residues.push_back({{{"start",{33,1036831948,3}}},23});
c.residues.push_back({{{"start",{35,1036831948,3}}},23});
}
t.events[3]={{{1077936128,128},{1069547520,2},{0,0},{0,0}}};
t.events[4]={{{1065353216,128},{1069547520,2},{0,0},{0,0}}};
t.events[5]={{{1065353216,128},{1069547520,2},{0,0},{0,0}}};
t.events[6]={{{1053609164,128},{1069547520,2},{0,0},{0,0}}};
t.events[7]={{{1065353216,128},{1069547520,2},{0,0},{0,0}}};
t.events[8]={{{1065353216,128},{1069547520,2},{0,0},{0,0}}};
t.events[19]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[20]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[21]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[22]={{{1065353216,1},{1073741824,2},{0,0},{0,0}}};
t.events[23]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[24]={{{1050253721,2},{1058642329,1},{0,0},{0,0}}};
t.events[25]={{{1045220556,2},{1056964608,2},{1065353216,1},{0,0}}};
t.events[26]={{{1045220556,2},{1056964608,2},{1065353216,1},{0,0}}};
t.constructor={{"clear",{}},{"push",{31,1}}};
t.consumers[7][1]={"holder.slot17","Creature_hitTargetInArc"};
t.transitions[7]={{23},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_52() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{14,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{24,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{19,1050253721,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{14,1050253721,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{5,1045220556,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{6,1045220556,1036831948,18440}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{7,1045220556,1036831948,18440}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{20,1045220556,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{21,1045220556,1036831948,18440}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{9,1045220556,1036831948,8}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{11,1045220556,1036831948,8}}},12};
}
{ auto &c=t.states[13]; c.baseline={{{"blend",{10,1045220556,1036831948,8}}},13};
}
{ auto &c=t.states[14]; c.baseline={{{"blend",{12,1045220556,1036831948,8}}},14};
}
{ auto &c=t.states[15]; c.baseline={{{"blend",{18,1045220556,1036831948,16}}},15};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{24,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1045220556,1045220556,2064}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{14,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{8,1045220556,2054}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{22,1050253721,1036831948,16416}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{23,1050253721,1036831948,16416}}},24};
}
{ auto &c=t.states[25]; c.baseline={{{"blend",{16,1036831948,1036831948,2064}}},25};
}
{ auto &c=t.states[26]; c.baseline={{{"blend",{17,1036831948,1036831948,2056}}},26};
}
{ auto &c=t.states[27]; c.baseline={{{"blend",{15,1045220556,1036831948,8}}},27};
}
{ auto &c=t.states[28]; c.baseline={{{"blend",{3,1045220556,1036831948,2056}}},28};
}
{ auto &c=t.states[29]; c.baseline={{{"blend",{4,1045220556,1036831948,2056}}},29};
}
t.events[3]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1036831948,1},{1056964608,2},{1071225241,1},{1072902963,2}}};
t.events[10]={{{1036831948,1},{1056964608,2},{1071225241,1},{1072902963,2}}};
t.events[11]={{{1050253721,1},{1060320051,2},{1071225241,1},{1072902963,2}}};
t.events[12]={{{1050253721,1},{1060320051,2},{1071225241,1},{1072902963,2}}};
t.events[18]={{{1065353216,1},{1071225241,2},{1069547520,1},{0,0}}};
t.events[19]={{{1041999197,1},{1055118912,2},{1054951207,4},{1040254232,8}}};
t.events[20]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.events[21]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[3][1]={"model_getAttachPoint","math_cos16","math_sin16","fx_spawn"};
t.consumers[3][2]={"model_getAttachPoint","math_cos16","math_sin16","fx_spawn"};
t.consumers[3][4]={"model_getAttachPoint","math_cos16","math_sin16","fx_spawn"};
t.consumers[3][8]={"model_getAttachPoint","math_cos16","math_sin16","fx_spawn"};
t.consumers[11][1]={"store +3d4:4=1"};
t.consumers[11][2]={"Creature_hitAllEnemiesInArc","store +3d8:4=1"};
t.consumers[12][1]={"store +3d4:4=1"};
t.consumers[12][2]={"Creature_hitAllEnemiesInArc","store +3d8:4=1"};
t.consumers[13][1]={"store +3d4:4=1"};
t.consumers[13][2]={"Creature_hitAllEnemiesInArc","store +3d8:4=1"};
t.consumers[14][1]={"store +3d4:4=1"};
t.consumers[14][2]={"Creature_hitAllEnemiesInArc","store +3d8:4=1"};
t.consumers[15][1]={"model_getAttachPoint","math_cos16","math_sin16","RedDragon_breathFx","store +3d4:4=1"};
t.consumers[15][2]={"players_findFarthestWithin"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[12]={{5},{}};
t.transitions[13]={{5},{}};
t.transitions[14]={{5},{}};
t.transitions[15]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{5},{}};
t.transitions[24]={{5},{}};
t.transitions[25]={{3},{}};
t.transitions[27]={{11,15},{}};
t.transitions[28]={{5},{}};
t.transitions[29]={{5},{}};
return t; }
static DispatchTable make_dispatch_53() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{5,1036831948,3}}},0};
c.conditions.push_back({{false,972,1},{{{"start",{6,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{6,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[16]; c.baseline={{},0};
c.conditions.push_back({{true,0,1},{{{"start",{9,1036831948,2051}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{4,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{6,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"vslot",{28}},{"start",{5,1036831948,6}}},20};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{5,1}}};
t.transitions[6]={{0},{}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_54() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1050253721,2051}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{19,1050253721,2051}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{17,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"start",{17,1036831948,3}},{"blend",{8,1036831948,1036831948,8}}},7};
c.residues.push_back({{{"start",{17,1036831948,3}},{"blend",{8,1036831948,1036831948,8}}},7});
c.residues.push_back({{{"start",{17,1036831948,3}},{"blend",{6,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"start",{17,1036831948,3}},{"blend",{7,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"start",{17,1036831948,3}},{"blend",{5,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"start",{17,1036831948,3}},{"blend",{6,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"start",{17,1036831948,3}},{"blend",{7,1036831948,1036831948,8}}},6});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{19,1050253721,2051}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{15,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{16,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{11,1050253721,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{11,1050253721,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{12,1050253721,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{17,1036831948,3}},{"blend",{13,1036831948,1036831948,2056}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{17,1036831948,3}},{"blend",{14,1036831948,1036831948,2056}}},24};
}
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[11]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[12]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{16,1}}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_55() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{7,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{15,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{8,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{5,1056964608,1056964608,8}}},6};
c.residues.push_back({{{"blend",{5,1056964608,1056964608,8}}},6});
c.residues.push_back({{{"blend",{6,1056964608,1056964608,8}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{5,1056964608,1056964608,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{6,1056964608,1056964608,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{4,1056964608,1056964608,8}}},9};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{11,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{10,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{7,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{9,1050253721,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{13,1036831948,1036831948,2056}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{14,1036831948,1036831948,2056}}},24};
}
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1048576000,1},{1051931443,2},{0,0},{0,0}}};
t.events[6]={{{1048576000,1},{1051931443,2},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{7,1}}};
t.consumers[7][2]={"GameObject_rand","Creature_canSeeTargetIn","setState(0x8)"};
t.consumers[8][2]={"GameObject_rand","Creature_canSeeTargetIn","setState(0x7)"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[18]={{5},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_56() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{-1,1036831948,3}}},0};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{-1,1036831948,2051}},{"sync",{}}},1};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,2051}},{"sync",{}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{-1,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{-1,1036831948,3}}},5};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"start",{-1,1036831948,3}},{"blend",{-1,1036831948,1036831948,2056}}},6};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}},{"blend",{-1,1036831948,1036831948,2056}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{-1,1036831948,3}},{"blend",{-1,1036831948,1036831948,2056}}},7};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}},{"blend",{-1,1036831948,1036831948,2056}}},7}});
}
{ auto &c=t.states[8]; c.baseline={{{"start",{-1,1036831948,3}},{"blend",{-1,1036831948,1036831948,2056}}},8};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}},{"blend",{-1,1036831948,1036831948,2056}}},8}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{-1,1036831948,2051}},{"sync",{}}},16};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,2051}},{"sync",{}}},16}});
}
{ auto &c=t.states[17]; c.baseline={{{"start",{-1,1036831948,3}}},17};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}}},17}});
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{-1,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,976,4294967295},{{{"blend",{-1,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{-1,1036831948,3}}},19};
c.conditions.push_back({{false,976,4294967295},{{{"start",{-1,1036831948,3}}},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"push",{-1,16}},{"push_chained",{-1,3}}},23};
c.conditions.push_back({{false,976,4294967295},{{{"push",{-1,16}},{"push_chained",{-1,3}}},23}});
}
t.constructor={{"clear",{}},{"push",{-1,1}},{"set_state",{0}}};
t.transitions[18]={{3},{}};
t.transitions[23]={{0},{}};
return t; }
static DispatchTable make_dispatch_57() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{6,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{8,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{8,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{6,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{8,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{6,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"vslot",{16}}},20};
c.conditions.push_back({{true,0,1},{{{"vslot",{16}},{"vslot",{16}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{},23};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{1065353216,1},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{7,12}},{"set_state",{23}}};
t.transitions[23]={{},{0}};
return t; }
static DispatchTable make_dispatch_58() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{7,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{9,1050253721,3}}},1};
}
{ auto &c=t.states[5]; c.baseline={{},0};
}
{ auto &c=t.states[16]; c.baseline={{},0};
}
{ auto &c=t.states[18]; c.baseline={{},0};
}
{ auto &c=t.states[19]; c.baseline={{},0};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{3,1050253721,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{3,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{4,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{5,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{6,1050253721,6}},{"vslot",{28}}},20});
c.conditions.push_back({{false,12,2},{{{"start",{3,1050253721,6}},{"jump_end",{}},{"vslot",{28}}},20}});
}
t.constructor={{"push",{7,1}},{"randomize",{}}};
return t; }
static DispatchTable make_dispatch_59() { DispatchTable t; t.group="special";
{ auto &c=t.states[0]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{7,1050253721,3}}},0}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{9,1050253721,2051}},{"sync",{}}},1}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{6,1050253721,3}}},5}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{},0}});
}
{ auto &c=t.states[18]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"vslot",{27,24}}},18}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{},0}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{18,1036831948,6}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{6,1050253721,3}}},23}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[24]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{8,1050253721,3}}},24}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[25]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{},0}});
}
{ auto &c=t.states[26]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"blend",{10,1036831948,1036831948,8}}},26}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[27]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{},27}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[28]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"blend",{14,1036831948,1036831948,12}}},28}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[29]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"blend",{11,1056964608,1056964608,8}}},29}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[30]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{13,1036831948,3}}},30}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[31]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"start",{15,1050253721,3}}},31}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[32]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{true,0,1},{{{"blend",{16,1036831948,1036831948,8}}},32}});
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[33]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[34]; c.baseline={{{"start",{18,1036831948,6}}},34};
c.conditions.push_back({{false,968,1},{{},0}});
}
t.events[11]={{{1060320051,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{7,1}}};
t.consumers[29][1]={"slot16","model_getSamplePointCount","obj_delete","store +c:4=2","model_getSamplePoint","GameObject_rand"};
t.transitions[18]={{24},{}};
t.transitions[25]={{24},{}};
return t; }
static DispatchTable make_dispatch_60() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{11,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1050253721,3}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{18,1050253721,3}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{11,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{4,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{6,1008981770,1036831948,8}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"start",{20,1036831948,3}},{"blend",{21,1036831948,1036831948,8}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"start",{20,1036831948,3}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{13,1036831948,1036831948,8}}},10};
}
{ auto &c=t.states[11]; c.baseline={{},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{16,1008981770,1036831948,8}}},12};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{18,1050253721,3}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{7,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{11,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{5,1050253721,6}},{"vslot",{28}}},20};
c.conditions.push_back({{false,12,2},{{{"start",{5,1050253721,6}},{"jump_end",{}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{11,1036831948,3}},{"blend",{17,1036831948,1036831948,8}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"blend",{12,1036831948,1036831948,8}}},24};
}
t.events[4]={{{1045220556,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1086324736,128},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{1067869798,2},{1073741824,4},{0,0}}};
t.events[12]={{{1045220556,1},{1063675494,2},{1066192076,1},{0,0}}};
t.events[13]={{{1056964608,1},{1069547520,2},{0,0},{0,0}}};
t.events[14]={{{1056964608,1},{1069547520,2},{0,0},{0,0}}};
t.events[15]={{{1056964608,1},{1069547520,2},{0,0},{0,0}}};
t.events[16]={{{1050253721,1},{1056964608,1},{1060320051,1},{0,0}}};
t.events[17]={{{1067869798,1},{1075838976,1},{1076258406,1},{0,0}}};
t.events[21]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{11,1}},{"randomize",{}}};
t.consumers[10][2]={"Creature_canSeeTargetIn","000000","GameObject_rand","Creature_hitTargetAtReach"};
t.consumers[11][2]={"Creature_canSeeTargetIn","000000","GameObject_rand","Creature_hitTargetAtReach"};
t.consumers[12][1]={"math_cos16","math_sin16","Creature_leadTargetPoint","model_getAttachPoint","math_atan2_16","GameObject_rand","pool_new","PoisonProjectile_ctor"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][8]={"camera_shake"};
t.consumers[23][1]={"camera_shake"};
t.consumers[24][1]={"store +3c8:4=1"};
t.consumers[24][2]={"camera_shake"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[12]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_61() { DispatchTable t; t.group="none";
return t; }
static DispatchTable make_dispatch_62() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{18,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{23,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{23,1050253721,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{18,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{9,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{10,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{19,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{11,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{13,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{12,1036831948,1036831948,2056}}},12};
}
{ auto &c=t.states[13]; c.baseline={{{"blend",{21,1036831948,1036831948,2056}}},13};
}
{ auto &c=t.states[14]; c.baseline={{{"blend",{22,1036831948,1036831948,2056}}},14};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{23,1050253721,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{16,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{16,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{17,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{18,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{14,1050253721,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{14,1050253721,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{15,1050253721,6}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{20,1050253721,2051}},{"sync",{}}},23};
}
t.events[8]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1050253721,1},{1056964608,4},{0,0},{0,0}}};
t.events[10]={{{1050253721,1},{1056964608,4},{0,0},{0,0}}};
t.events[11]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[12]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[13]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[15]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.events[19]={{{1067030937,1},{1075838976,2},{0,0},{0,0}}};
t.events[21]={{{1036831948,1},{1050253721,2},{0,0},{0,0}}};
t.events[22]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{18,1}}};
t.consumers[6][1]={"Creature_hitTargetInArc"};
t.consumers[6][4]={"Creature_canSeeTargetIn","setState(0x7)"};
t.consumers[7][1]={"Creature_hitTargetInArc"};
t.consumers[7][4]={"Creature_canSeeTargetIn","setState(0x7)"};
t.consumers[8][1]={"slot35","000000","creature_addSpell"};
t.consumers[8][2]={"SpellConeOfFrost_typeinfo","creature_removeSpellByType"};
t.consumers[9][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[9][4]={"Creature_canSeeTargetIn","setState(0x7)"};
t.consumers[10][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[10][4]={"Creature_canSeeTargetIn","setState(0x7)"};
t.consumers[11][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[11][4]={"Creature_canSeeTargetIn","setState(0x7)"};
t.consumers[12][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[12][4]={"Creature_canSeeTargetIn","setState(0x7)"};
t.consumers[13][1]={"Creature_hitTargetInArc"};
t.consumers[13][2]={"Creature_canSeeTargetIn","setState(0x3)"};
t.consumers[14][1]={"creature_findEnemyInCone","math_cos16","math_sin16","pool_new","slot35","math_atan2_16","ShockOfFrost_ctor"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[12]={{5},{}};
t.transitions[13]={{3},{}};
t.transitions[14]={{3},{}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_63() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{14,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{17,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{16,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{15,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{5,1036831948,1036831948,8}}},6};
c.residues.push_back({{{"blend",{5,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"blend",{6,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"blend",{7,1036831948,1036831948,8}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{15,1036831948,3}},{"blend",{18,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{16,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{10,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{10,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{11,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{12,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{13,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{14,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{8,1050253721,6}},{"holder",{11}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{8,1050253721,6}},{"holder",{11}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{9,1050253721,6}},{"holder",{11}},{"vslot",{28}}},20});
}
t.events[5]={{{1045220556,1},{1053609164,2},{0,0},{0,0}}};
t.events[6]={{{1045220556,1},{1053609164,2},{0,0},{0,0}}};
t.events[7]={{{1045220556,1},{1053609164,2},{0,0},{0,0}}};
t.events[8]={{{1053609164,128},{0,0},{0,0},{0,0}}};
t.events[9]={{{1053609164,128},{0,0},{0,0},{0,0}}};
t.events[18]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{14,1}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{3},{}};
t.transitions[18]={{0},{}};
return t; }
static DispatchTable make_dispatch_64() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{10,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{7,1036831948,3}}},5};
}
{ auto &c=t.states[7]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{4,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{14,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{3,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{5,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{6,1036831948,1036831948,2056}}},11};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{13,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{7,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{9,1036831948,1036831948,16}}},18};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{10,1050253721,3}}},19};
c.conditions.push_back({{false,968,1},{{},0}});
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"start",{15,1036831948,6}},{"vslot",{28}}},20};
c.conditions.push_back({{false,972,1048576},{{{"holder",{11}},{"holder",{11}},{"start",{15,1036831948,6}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{10,1036831948,3}},{"blend",{8,1036831948,1036831948,2056}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{7,1036831948,3}},{"blend",{12,1036831948,1036831948,2056}}},24};
}
t.events[3]={{{1045220556,1},{1058642329,1},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{1058642329,2},{1065353216,2},{0,0}}};
t.events[5]={{{1050253721,4},{0,0},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{1061997772,1},{1070386380,4},{0,0}}};
t.events[8]={{{1073741824,1},{0,0},{0,0},{0,0}}};
t.events[14]={{{1056964608,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{10,1}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[8]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_65() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{6,1050253721,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"start",{16,1036831948,3}},{"blend",{5,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"start",{15,1036831948,3}},{"blend",{3,1036831948,1036831948,2056}}},7};
c.residues.push_back({{{"start",{15,1036831948,3}},{"blend",{3,1036831948,1036831948,2056}}},7});
c.residues.push_back({{{"start",{15,1036831948,3}},{"blend",{4,1036831948,1036831948,2056}}},7});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{6,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2064}}},18});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,2064}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"blend",{13,1036831948,1036831948,2064}}},18};
c.residues.push_back({{{"blend",{13,1036831948,1036831948,2064}}},18});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,2064}}},18});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{7,1050253721,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{7,1050253721,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{8,1050253721,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{9,1050253721,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{6,1050253721,2051}},{"sync",{}}},23};
}
{ auto &c=t.states[24]; c.baseline={{{"start",{18,1036831948,3}}},24};
}
{ auto &c=t.states[25]; c.baseline={{{"start",{16,1036831948,3}},{"blend",{12,1008981770,1036831948,2056}}},25};
}
t.events[3]={{{1051931443,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1051931443,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1036831948,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{15,1}},{"randomize",{}}};
t.transitions[6]={{25},{}};
t.transitions[18]={{3},{}};
t.transitions[25]={{3},{}};
return t; }
static DispatchTable make_dispatch_66() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{6,1036831948,3}}},0};
c.conditions.push_back({{false,972,1},{{{"start",{6,1036831948,3}}},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{19,1036831948,2051}}},1};
c.conditions.push_back({{false,972,1},{{{"start",{19,1036831948,2051}}},1}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{19,1036831948,2051}},{"sync",{}}},3};
c.conditions.push_back({{false,968,1},{{{"start",{16,1036831948,2051}},{"sync",{}}},3}});
c.conditions.push_back({{false,972,1},{{{"start",{19,1036831948,2051}},{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{3,1036831948,3}},{"sync",{}}},5};
c.conditions.push_back({{false,968,1},{{{"start",{7,1036831948,3}},{"sync",{}}},5}});
c.conditions.push_back({{false,972,1},{{{"start",{3,1036831948,3}},{"sync",{}}},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"start",{3,1036831948,3}}},6};
c.conditions.push_back({{false,972,1},{{{"start",{3,1036831948,3}}},6}});
}
{ auto &c=t.states[7]; c.baseline={{{"start",{6,1036831948,3}},{"blend",{8,1036831948,1036831948,2056}}},7};
c.conditions.push_back({{false,972,1},{{{"start",{6,1036831948,3}},{"blend",{8,1036831948,1036831948,2056}}},7}});
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{13,1045220556,1045220556,2056}}},8};
c.conditions.push_back({{false,972,1},{{{"blend",{13,1045220556,1045220556,2056}}},8}});
c.conditions.push_back({{false,984,1},{{{"blend",{13,1045220556,1045220556,2056}}},8}});
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{14,1045220556,1045220556,2056}}},9};
c.conditions.push_back({{false,972,1},{{{"blend",{14,1045220556,1045220556,2056}}},9}});
c.conditions.push_back({{false,984,1},{{{"blend",{14,1045220556,1045220556,2056}}},9}});
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{15,1045220556,1045220556,2056}}},10};
c.conditions.push_back({{false,972,1},{{{"blend",{15,1045220556,1045220556,2056}}},10}});
c.conditions.push_back({{false,984,1},{{{"blend",{15,1045220556,1045220556,2056}}},10}});
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{12,1045220556,1045220556,2056}}},11};
c.conditions.push_back({{false,972,1},{{{"blend",{12,1045220556,1045220556,2056}}},11}});
c.conditions.push_back({{false,984,1},{{{"blend",{12,1045220556,1045220556,2056}}},11}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{18,1036831948,6}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{3,1050253721,3}}},17};
c.conditions.push_back({{false,968,1},{{{"start",{11,1050253721,3}}},17}});
c.conditions.push_back({{false,972,1},{{{"start",{3,1050253721,3}}},17}});
}
{ auto &c=t.states[18]; c.baseline={{},0};
c.conditions.push_back({{false,972,1},{{},0}});
}
{ auto &c=t.states[19]; c.baseline={{},0};
c.conditions.push_back({{false,972,1},{{},0}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{18,1036831948,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"start",{4,1036831948,6}}},23};
c.conditions.push_back({{false,972,1},{{{"start",{4,1036831948,6}}},23}});
}
{ auto &c=t.states[24]; c.baseline={{{"start",{6,1056964608,4}},{"blend",{9,1036831948,1036831948,32}}},24};
c.conditions.push_back({{false,972,1},{{{"start",{6,1056964608,4}},{"blend",{9,1036831948,1036831948,32}}},24}});
}
{ auto &c=t.states[25]; c.baseline={{{"start",{18,1053609164,6}}},25};
c.conditions.push_back({{false,972,1},{{{"start",{18,1053609164,6}}},25}});
}
t.events[4]={{{1060320051,1},{1065353216,2},{0,0},{0,0}}};
t.events[8]={{{1067030937,1},{1072064102,2},{0,0},{0,0}}};
t.events[9]={{{1056964608,1},{1073741824,2},{1050253721,4},{1053609164,8}}};
t.events[12]={{{1063675494,1},{1067030937,2},{0,0},{0,0}}};
t.events[13]={{{1050253721,1},{1056964574,2},{0,0},{0,0}}};
t.events[14]={{{1045220556,1},{1058642346,2},{0,0},{0,0}}};
t.events[15]={{{1060320051,1},{1067030803,2},{0,0},{0,0}}};
t.events[18]={{{1050253721,1},{1069547520,2},{0,0},{0,0}}};
t.events[20]={{{1073741824,64},{1074161254,128},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{5,1}},{"randomize",{}}};
t.consumers[11][1]={"Creature_hitTargetInArc"};
t.consumers[16][1]={"store +3d0:4=1"};
t.consumers[16][2]={"VampireLord_enterBatForm","GameObject_rand","store +3d0:4=0"};
t.consumers[20][1]={"store +3d0:4=1"};
t.consumers[20][2]={"VampireLord_enterBatForm","GameObject_rand","store +3c8:4=0","store +3d0:4=0"};
t.consumers[25][1]={"store +3d0:4=1"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[11]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[24]={{3},{}};
return t; }
static DispatchTable make_dispatch_67() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{16,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{9,1045220556,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1045220556,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{4,1050253721,1050253721,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{10,1050253721,1050253721,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1050253721,1050253721,16392}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{7,1050253721,1050253721,16392}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{6,1050253721,1050253721,2056}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{8,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{13,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{16,1036831948,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{11,1036831948,6}},{"vslot",{28}}},20};
c.residues.push_back({{{"start",{11,1036831948,6}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{12,1036831948,6}},{"vslot",{28}}},20});
}
t.events[4]={{{1050253721,1},{1060320051,2},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{1056964608,2},{0,0},{0,0}}};
t.events[6]={{{1050253721,1},{1060320051,2},{0,0},{0,0}}};
t.events[7]={{{1050253721,1},{1056964608,2},{0,0},{0,0}}};
t.events[10]={{{1056964608,1},{1063675494,2},{0,0},{0,0}}};
t.events[11]={{{1065353216,132},{0,0},{0,0},{0,0}}};
t.events[12]={{{1065353216,132},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{16,1}}};
t.consumers[6][1]={"Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[6][2]={"Creature_canSeeTargetIn","creature_findEnemyInCone","setState(0x5)"};
t.consumers[7][1]={"GameObject_rand","Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[7][2]={"Creature_canSeeTargetIn","creature_findEnemyInCone","setState(0x5)"};
t.consumers[8][1]={"Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[8][2]={"Creature_canSeeTargetIn","creature_findEnemyInCone","setState(0x5)"};
t.consumers[9][1]={"Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[9][2]={"Creature_canSeeTargetIn","creature_findEnemyInCone","setState(0x5)"};
t.consumers[10][1]={"GameObject_rand","Creature_hitAllEnemiesInArc","store +3c8:4=1"};
t.consumers[10][2]={"Creature_canSeeTargetIn","creature_findEnemyInCone","setState(0x5)"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][4]={"store +c:4=1"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_68() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"holder",{15}},{"holder",{15}},{"start",{12,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{9,1050253721,3}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{9,1050253721,3}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"holder",{15}},{"holder",{15}},{"start",{12,1050253721,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"holder",{15}},{"holder",{15}},{"blend",{4,1036831948,1036831948,8}}},6};
c.residues.push_back({{{"holder",{15}},{"holder",{15}},{"blend",{4,1036831948,1036831948,8}}},6});
c.residues.push_back({{{"holder",{15}},{"holder",{15}},{"blend",{5,1036831948,1036831948,8}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{7,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{9,1050253721,3}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"holder",{15}},{"holder",{15}},{"start",{12,1050253721,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{10,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"holder",{15}},{"holder",{15}},{"start",{12,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"start",{6,1050253721,6}},{"vslot",{28}}},20};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{12,1}}};
t.consumers[20][0]={"GameObject_rand"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_69() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{20,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{18,1053609164,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{16,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{4,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{19,1036831948,1036831948,2056}}},10};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{14,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{15,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"holder",{11}},{"holder",{11}},{"start",{9,1045220556,2054}},{"vslot",{28}}},20};
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{9,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{10,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"holder",{11}},{"holder",{11}},{"start",{11,1045220556,2054}},{"vslot",{28}}},20});
c.residues.push_back({{{"start",{12,1045220556,2054}},{"vslot",{28}}},20});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{7,1036831948,3}}},23};
}
t.events[3]={{{1045220556,1},{1050253721,2},{0,0},{0,0}}};
t.events[4]={{{1045220556,1},{1050253721,2},{0,0},{0,0}}};
t.events[5]={{{1045220556,1},{1050253721,2},{0,0},{0,0}}};
t.events[6]={{{1045220556,1},{1056964608,2},{0,0},{0,0}}};
t.events[9]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[10]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[11]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.events[12]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[6][2]={"Creature_canSeeTargetIn"};
t.consumers[7][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[7][2]={"Creature_canSeeTargetIn","setState(0x6)"};
t.consumers[8][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[8][2]={"Creature_canSeeTargetIn"};
t.consumers[9][1]={"holder.slot17","Creature_hitTargetInArc"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[8]={{3},{}};
t.transitions[9]={{3},{}};
t.transitions[10]={{3},{}};
t.transitions[18]={{3},{}};
return t; }
static DispatchTable make_dispatch_70() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{18,1050253721,3}}},0};
c.conditions.push_back({{false,972,1},{{{"start",{21,1050253721,3}}},0}});
c.conditions.push_back({{false,972,2},{{{"start",{30,1050253721,3}}},0}});
c.conditions.push_back({{false,972,3},{{{"start",{31,1050253721,3}}},0}});
c.conditions.push_back({{false,972,4},{{},0}});
}
{ auto &c=t.states[1]; c.baseline={{{"start",{20,1050253721,3}},{"sync",{}}},1};
c.conditions.push_back({{false,972,1},{{{"start",{26,1050253721,3}},{"sync",{}}},1}});
c.conditions.push_back({{false,972,3},{{{"start",{33,1050253721,3}},{"sync",{}}},1}});
c.conditions.push_back({{false,972,4},{{{"sync",{}}},1}});
}
{ auto &c=t.states[2]; c.baseline={{{"start",{20,1050253721,3}},{"sync",{}}},2};
c.conditions.push_back({{false,972,1},{{{"start",{26,1050253721,3}},{"sync",{}}},2}});
c.conditions.push_back({{false,972,3},{{{"start",{37,1050253721,3}},{"sync",{}}},2}});
c.conditions.push_back({{false,972,4},{{{"sync",{}}},2}});
}
{ auto &c=t.states[3]; c.baseline={{{"start",{19,1050253721,3}},{"sync",{}}},3};
c.conditions.push_back({{false,972,1},{{{"start",{26,1050253721,3}},{"sync",{}}},3}});
c.conditions.push_back({{false,972,3},{{{"start",{33,1050253721,3}},{"sync",{}}},3}});
c.conditions.push_back({{false,972,4},{{{"sync",{}}},3}});
}
{ auto &c=t.states[5]; c.baseline={{{"start",{18,1036831948,3}}},5};
c.conditions.push_back({{false,972,1},{{{"start",{21,1036831948,3}}},5}});
c.conditions.push_back({{false,972,2},{{{"start",{30,1036831948,3}}},5}});
c.conditions.push_back({{false,972,3},{{{"start",{32,1036831948,3}}},5}});
c.conditions.push_back({{false,972,4},{{},5}});
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{17,1036831948,1036831948,8}}},6};
c.conditions.push_back({{false,972,1},{{},6}});
c.conditions.push_back({{false,972,2},{{{"blend",{27,1036831948,1036831948,8}}},6}});
c.conditions.push_back({{false,972,3},{{{"blend",{35,1036831948,1036831948,8}}},6}});
}
{ auto &c=t.states[16]; c.baseline={{{"start",{19,1050253721,3}},{"sync",{}}},16};
c.conditions.push_back({{false,972,1},{{{"start",{23,1050253721,3}},{"sync",{}}},16}});
c.conditions.push_back({{false,972,3},{{{"start",{33,1050253721,3}},{"sync",{}}},16}});
c.conditions.push_back({{false,972,4},{{{"sync",{}}},16}});
}
{ auto &c=t.states[18]; c.baseline={{},0};
c.conditions.push_back({{false,972,2},{{{"blend",{28,1036831948,1036831948,16}}},18}});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{18,1050253721,3}}},19};
c.conditions.push_back({{false,972,1},{{{"start",{21,1050253721,3}}},19}});
c.conditions.push_back({{false,972,2},{{{"start",{30,1050253721,3}}},19}});
c.conditions.push_back({{false,972,3},{{{"start",{31,1050253721,3}}},19}});
c.conditions.push_back({{false,972,4},{{},19}});
}
{ auto &c=t.states[20]; c.baseline={{{"start",{38,1050253721,6}},{"vslot",{28}}},20};
c.conditions.push_back({{true,0,1},{{{"start",{38,1050253721,6}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,12,2},{{{"start",{38,1050253721,6}},{"vslot",{28}}},20}});
c.conditions.push_back({{false,786,1},{{{"start",{38,1050253721,6}},{"vslot",{28}}},20}});
}
{ auto &c=t.states[23]; c.baseline={{{"start",{29,1050253721,3}}},23};
}
{ auto &c=t.states[24]; c.baseline={{},24};
c.conditions.push_back({{false,972,1},{{{"blend",{25,1036831948,1036831948,8}}},24}});
}
{ auto &c=t.states[25]; c.baseline={{},25};
c.conditions.push_back({{false,972,1},{{{"blend",{24,1036831948,1036831948,8}}},25}});
}
t.events[17]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[24]={{{1056293519,1},{1066192076,2},{0,0},{0,0}}};
t.events[25]={{{1078565273,1},{1081711001,2},{0,0},{0,0}}};
t.events[27]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[34]={{{1050253721,1},{1069547520,2},{0,0},{0,0}}};
t.events[35]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[38]={{{1056964608,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.consumers[20][2]={"store +c:4=1"};
t.consumers[24][1]={"model_buildHideMask","GameObject_rand","libc_sprintf","res_find","sound_playOnObject","store +e8:4=0","store +220:8=0","store +3e4:4=1","store +3d8:8=0","store +3d0:8=0","store +3dc:4=42680000","store +3e0:2=0","store +220:8=1080000"};
t.consumers[24][2]={"model_buildHideMask","store +e8:4=400","store +220:8=0","store +220:8=1080000"};
t.transitions[6]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[24]={{0},{}};
t.transitions[25]={{3},{}};
return t; }
static DispatchTable make_dispatch_71() { DispatchTable t; t.group="steered";
{ auto &c=t.states[0]; c.baseline={{{"start",{9,1050253721,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{7,1050253721,3}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{7,1050253721,3}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{10,1036831948,3}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,8}}},6};
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{5,1036831948,1036831948,8}}},7};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{7,1050253721,3}},{"sync",{}}},16};
}
{ auto &c=t.states[17]; c.baseline={{{"start",{10,1036831948,3}}},17};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{8,1036831948,1036831948,16}}},18};
}
{ auto &c=t.states[19]; c.baseline={{{"start",{9,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{4,1050253721,6}},{"vslot",{28}}},20};
}
{ auto &c=t.states[23]; c.baseline={{{"blend",{5,1036831948,1036831948,8}}},23};
}
t.events[3]={{{1050253721,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1053609164,1},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"push",{9,1}}};
t.consumers[23][1]={"math_cos16","math_sin16","terrain_height","pool_new","Skeleton_ctor","handle_link","res_find","sound_playOnObject"};
t.transitions[6]={{3},{}};
t.transitions[7]={{3},{}};
t.transitions[18]={{3},{}};
t.transitions[23]={{3},{}};
return t; }
static DispatchTable make_dispatch_72() { DispatchTable t; t.group="root";
{ auto &c=t.states[0]; c.baseline={{{"start",{15,1036831948,3}}},0};
}
{ auto &c=t.states[1]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},1};
}
{ auto &c=t.states[3]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},3};
}
{ auto &c=t.states[5]; c.baseline={{{"start",{15,1036831948,3}},{"sync",{}}},5};
}
{ auto &c=t.states[6]; c.baseline={{{"blend",{3,1036831948,1036831948,2056}}},6};
c.residues.push_back({{{"blend",{3,1036831948,1036831948,2056}}},6});
c.residues.push_back({{{"blend",{4,1036831948,1036831948,2056}}},6});
}
{ auto &c=t.states[7]; c.baseline={{{"blend",{10,1036831948,1036831948,2056}}},7};
}
{ auto &c=t.states[8]; c.baseline={{{"blend",{17,1036831948,1036831948,2056}}},8};
}
{ auto &c=t.states[9]; c.baseline={{{"blend",{7,1036831948,1036831948,18440}}},9};
}
{ auto &c=t.states[10]; c.baseline={{{"blend",{9,1036831948,1036831948,18440}}},10};
}
{ auto &c=t.states[11]; c.baseline={{{"blend",{6,1036831948,1036831948,2056}}},11};
}
{ auto &c=t.states[12]; c.baseline={{{"blend",{8,1036831948,1036831948,2056}}},12};
}
{ auto &c=t.states[13]; c.baseline={{{"blend",{16,1036831948,1036831948,2056}}},13};
}
{ auto &c=t.states[14]; c.baseline={{{"blend",{5,1036831948,1036831948,2056}}},14};
}
{ auto &c=t.states[16]; c.baseline={{{"start",{18,1036831948,2051}},{"sync",{}}},16};
}
{ auto &c=t.states[18]; c.baseline={{{"blend",{11,1036831948,1036831948,16}}},18};
c.residues.push_back({{{"blend",{11,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{12,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{13,1036831948,1036831948,16}}},18});
c.residues.push_back({{{"blend",{14,1036831948,1036831948,16}}},18});
}
{ auto &c=t.states[19]; c.baseline={{{"start",{15,1050253721,3}}},19};
}
{ auto &c=t.states[20]; c.baseline={{{"start",{19,1045220556,2054}},{"vslot",{28}}},20};
c.conditions.push_back({{true,0,1},{{{"holder",{11}},{"start",{20,1045220556,2054}},{"vslot",{28}}},20}});
}
t.events[3]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[4]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[5]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[6]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[7]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[8]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[9]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[10]={{{1061997772,1},{0,0},{0,0},{0,0}}};
t.events[16]={{{1072064102,1},{0,0},{0,0},{0,0}}};
t.events[17]={{{1050253721,1},{1060320051,2},{0,0},{0,0}}};
t.events[19]={{{1045220556,128},{0,0},{0,0},{0,0}}};
t.constructor={{"clear",{}},{"set_state",{0}}};
t.consumers[6][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[7][1]={"holder.slot17","Creature_hitTargetInArc"};
t.consumers[8][1]={"Creature_hitAllEnemiesInArc"};
t.consumers[8][2]={"Creature_hitAllEnemiesInArc"};
t.consumers[9][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[10][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[11][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[12][1]={"holder.slot17","Creature_hitAllEnemiesInArc"};
t.consumers[13][1]={"math_cos16","math_sin16","pool_new","store +478:4=258","slot35","GroundPoundEffect_ctor"};
t.consumers[14][1]={"creature_findEnemyInCone","math_cos16","math_sin16","terrain_height","3aef58","3af9b8","fx_spawn","store +464:4=1","store +470:8=0","store +468:8=0","store +474:4=0","store +468:2=0","store +478:4=258","store +470:4=0","store +46c:4=0","store +474:4=3f800000","handle_link","fx_ringBurst","res_find","sound_playOnObject","combat_applyAreaDamage","GameObject_rand","decal_add","pool_new","LightEffect_ctor","handle_clear","store +470:8=3f80000000000000","store +464:4=0","store +460:4=0"};
t.consumers[20][-1]={"Creature_deadUpdate"};
t.transitions[6]={{5},{}};
t.transitions[7]={{5},{}};
t.transitions[8]={{5},{}};
t.transitions[9]={{5},{}};
t.transitions[10]={{5},{}};
t.transitions[11]={{5},{}};
t.transitions[12]={{5},{}};
t.transitions[13]={{5},{}};
t.transitions[14]={{5},{}};
t.transitions[18]={{5},{}};
return t; }
const std::map<std::string, DispatchTable> &dispatch_tables() {
static const auto tables=[] { std::map<std::string, DispatchTable> out;
out.emplace("Agony",make_dispatch_0());
out.emplace("Amphian",make_dispatch_1());
out.emplace("Ant",make_dispatch_2());
out.emplace("AntQueen",make_dispatch_3());
out.emplace("AntWave",make_dispatch_4());
out.emplace("Arenabeast",make_dispatch_5());
out.emplace("Badger",make_dispatch_6());
out.emplace("BlackWidow",make_dispatch_7());
out.emplace("Bubo",make_dispatch_8());
out.emplace("CastBadger",make_dispatch_9());
out.emplace("Cazic",make_dispatch_10());
out.emplace("CdEyeball",make_dispatch_11());
out.emplace("Clockwork",make_dispatch_12());
out.emplace("CloudGiant",make_dispatch_13());
out.emplace("CreatureContainer",make_dispatch_14());
out.emplace("Creep",make_dispatch_15());
out.emplace("Critter",make_dispatch_16());
out.emplace("Cyclops",make_dispatch_17());
out.emplace("Demon",make_dispatch_18());
out.emplace("Droid",make_dispatch_19());
out.emplace("FatCyclops",make_dispatch_20());
out.emplace("FemaleDarkElf",make_dispatch_21());
out.emplace("FireBeetle",make_dispatch_22());
out.emplace("FireFly",make_dispatch_23());
out.emplace("Froglock",make_dispatch_24());
out.emplace("Generator",make_dispatch_25());
out.emplace("Ghoul",make_dispatch_26());
out.emplace("Gnaw",make_dispatch_27());
out.emplace("GnawSpawn",make_dispatch_28());
out.emplace("Gnome",make_dispatch_29());
out.emplace("GnomeNavigator",make_dispatch_30());
out.emplace("Goblin",make_dispatch_31());
out.emplace("Gremlin",make_dispatch_32());
out.emplace("Hag",make_dispatch_33());
out.emplace("Horsemen",make_dispatch_34());
out.emplace("Innoruuk",make_dispatch_35());
out.emplace("IronGuardian",make_dispatch_36());
out.emplace("Kobold",make_dispatch_37());
out.emplace("LavaMonster",make_dispatch_38());
out.emplace("MaleDarkElf",make_dispatch_39());
out.emplace("Marr",make_dispatch_40());
out.emplace("Mermaid",make_dispatch_41());
out.emplace("Minion",make_dispatch_42());
out.emplace("Minotaur",make_dispatch_43());
out.emplace("Mosquito",make_dispatch_44());
out.emplace("Mummy",make_dispatch_45());
out.emplace("MummyKing",make_dispatch_46());
out.emplace("NPC",make_dispatch_47());
out.emplace("Natasla",make_dispatch_48());
out.emplace("Nightmare",make_dispatch_49());
out.emplace("Ogre",make_dispatch_50());
out.emplace("Orc",make_dispatch_51());
out.emplace("RedDragon",make_dispatch_52());
out.emplace("Rondo",make_dispatch_53());
out.emplace("Scorpion",make_dispatch_54());
out.emplace("SeaMonster",make_dispatch_55());
out.emplace("Skeleton",make_dispatch_56());
out.emplace("Skelight",make_dispatch_57());
out.emplace("SmallSpider",make_dispatch_58());
out.emplace("Soul",make_dispatch_59());
out.emplace("SpiderQueen",make_dispatch_60());
out.emplace("Teleporter",make_dispatch_61());
out.emplace("Tormentor",make_dispatch_62());
out.emplace("Ulthork",make_dispatch_63());
out.emplace("UndeadKnight",make_dispatch_64());
out.emplace("Vampire",make_dispatch_65());
out.emplace("VampireLord",make_dispatch_66());
out.emplace("WarBoar",make_dispatch_67());
out.emplace("WarWraith",make_dispatch_68());
out.emplace("Warrior",make_dispatch_69());
out.emplace("WoodElfSoldier",make_dispatch_70());
out.emplace("Wraith",make_dispatch_71());
out.emplace("Zek",make_dispatch_72());
return out; }(); return tables; }
