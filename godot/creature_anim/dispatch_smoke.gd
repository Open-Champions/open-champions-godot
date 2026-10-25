extends "res://creature_anim/smoke.gd"

var virtual_calls: Array = []
var rng_calls := 0
func synthetic_virtual(kind: String, args: Array) -> bool:
	virtual_calls.append([kind,args])
	return true
func synthetic_redispatch(_kind: String, args: Array) -> Dictionary:
	return {"filtered_state":args[1],"choice":{}}
func recursive_redispatch(_kind: String, _args: Array) -> Dictionary:
	return {"filtered_state":2,"choice":{}}
func synthetic_rng() -> int:
	rng_calls += 1
	return 0
func config_for(cls: String) -> Dictionary:
	var fixture = load("res://tests/skinning_fixture.gd")
	var clips := {}
	var entries := {}
	for i in range(112):
		var bytes: PackedByteArray = fixture.animation(64 + i)
		bytes.encode_u32(4,120)
		clips[0x10000+256*i] = bytes
		entries[i] = 0x10000+256*i
	if cls != "Player":
		entries[1] = 0x9000
		entries[2] = 0xa000
	var names := {}
	for name in ["dha_quake.anm","nw_seated.anm","nw_idlea.anm","ohw_idled.anm","ohw_atakseq2.anm","knockdown1_c.anm","cyclone1.anm","spel_bw_cyclone.anm","bash.anm","spel_bw_ancestral.anm","ohw_warcry.anm","ohw_roar.anm","spel_vb_roar.anm","ohw_battlecry.anm","spel_vb_battlecry.anm","ohw_bloodlust.anm","spel_de_shadowstep.anm"]:
		names[name] = 0x16400
	return {"profile":"rta-us-v2.00", "class":cls, "clips":clips,
		"entries":entries, "named_clips":names,"base_resource":0x10000,
		"model_resource":0x9000,"texture_resource":0xa000,"model_bytes":fixture.model()}
func run() -> void:
	check(ClassDB.class_exists("ChampionsAnimationDispatch"),"dispatch binding registered")
	api = ClassDB.instantiate("ChampionsAnimationDispatch")
	for cls in ["Critter","AntQueen","Gnaw","AntWave","missing"]:
		check(not api.configure({"profile":"rta-us-v2.00","class":cls}).ok,"partial class refused: "+cls)
	var invalid_model := config_for("Player")
	invalid_model["model_resource"] = 0
	check(not api.configure(invalid_model).ok,"null selected model refused")
	check(api.configure(config_for("Agony")).ok,"Agony configured")
	check(not api.select_state(0xdead,{},0).ok,"unknown state refused")
	var agony_hit = api.select_state(18,{"field":0x2dc,"value":0x380000},0)
	check(agony_hit.ok and agony_hit.state==18,"agony knockback mask")
	check(not api.select_state(20,{},0).ok,"missing virtual handler refused")
	check(api.install_events("").ok,"Agony events installed")
	check(api.select_state(0,{},0).ok,"idle dispatch")
	check(api.select_state(6,{},0).ok,"attack dispatch")
	var result: Dictionary
	for frame in range(1,38):
		result = api.tick(frame,0x3f800000,false,false)
		check(result.ok,"attack tick "+str(frame)+": "+str(result.get("error","")))
		check(bool(result.events&1)==(frame==37),"hit occurs at tick 37")
	check(result.emissions.size()==1 and result.emissions[0].kind=="hit_test","typed hit emission")
	check(result.positions.size()==3 and result.joint_count==4,"skinned dispatch pose")
	check(not api.tick(37,0x3f800000,false,false).ok,"duplicate dispatch tick refused")
	check(api.transition_probe(0).observed_targets==PackedInt64Array([3]),"attack exit observation")
	check(not api.transition_probe(3).ok,"unobserved transition predicate refused")
	check(api.set_virtual_handler(Callable(self,"synthetic_virtual")).ok,"explicit virtual handler")
	check(api.select_state(20,{},38).ok and virtual_calls==[["vslot",[28]]],"death dispatch calls slot28")
	result = api.tick(38,0x3f800000,false,false)
	check(result.ok and result.emissions.size()==1 and result.emissions[0].kind=="corpse","corpse receives complete event word including zero")
	var shared: RefCounted = ClassDB.instantiate("ChampionsAnimationDispatch")
	var shared_config := config_for("Agony")
	shared_config["shared_with"] = api
	check(shared.configure(shared_config).ok,"second table actor shares event identity")
	check(shared.select_state(6,{},38).ok,"shared actor attack")
	var shared_ok := true
	for frame in range(39,76):
		result=shared.tick(frame,0x3f800000,false,false)
		shared_ok = shared_ok and result.ok
	check(shared_ok and result.events&1==1,"second configuration preserves installed event slots")
	var ant: RefCounted = ClassDB.instantiate("ChampionsAnimationDispatch")
	check(ant.configure(config_for("Ant")).ok,"steered class configured")
	check(ant.select_state(1,{},42).ok,"steered walk resync")
	result=ant.tick(43,0x3f800000,false,false)
	check(result.ok and not result.flags&0x800 and result.root_x_raw==0 and result.root_y_raw==0,"steered walk does not extract root")
	var horse: RefCounted = ClassDB.instantiate("ChampionsAnimationDispatch")
	check(horse.configure(config_for("Horsemen")).ok,"Horsemen configured")
	check(horse.set_virtual_handler(Callable(self,"synthetic_virtual")).ok,"Horsemen handler installed")
	check(not horse.select_state(2,{},5).ok,"slot27 cannot be silently acknowledged")
	check(horse.set_virtual_handler(Callable(self,"recursive_redispatch")).ok,"recursive fixture installed")
	check(not horse.select_state(2,{},5).ok,"recursive slot27 has a bounded refusal")
	check(horse.set_virtual_handler(Callable(self,"synthetic_redispatch")).ok,"filtered redispatch service installed")
	check(horse.select_state(2,{},5).ok,"nested state3 executes synchronously")
	result=horse.tick(6,0x3f800000,false,false)
	check(result.ok and result.state==2 and result.flags&0x800,"nested pose executes before outer state store")
	var player: RefCounted = ClassDB.instantiate("ChampionsAnimationDispatch")
	check(player.configure(config_for("Player")).ok,"player configured")
	check(player.install_events("").ok,"player event slots")
	check(player.player_reset({}).ok,"four player base layers")
	result=player.player_move(1,0x3f400000,0)
	check(result.ok and result.positions.size()==3,"player locomotion pose")
	var render_positions: PackedVector3Array = result.positions
	var render_normals: PackedVector3Array = result.normals
	check(player.player_fidget(0,0,Callable(self,"synthetic_rng")).ok and rng_calls==1,"fidget draws once")
	check(player.player_fidget(0x3f800000,0,Callable(self,"synthetic_rng")).ok and rng_calls==1,"fidget stop preserves RNG order")
	check(player.player_combo(0,0,1,10,Callable(self,"synthetic_rng")).ok and rng_calls==2,"combo consumes shared draw")
	check(not player.player_combo(9,1,1,10,Callable(self,"synthetic_rng")).ok and rng_calls==2,"unrecorded model9 class1 combo refuses before draw")
	check(not player.player_combo(11,3,1,10,Callable(self,"synthetic_rng")).ok and rng_calls==2,"unrecorded model11 class3 combo refuses before draw")
	for action in ["state_6","state_7","state_9","state_14"]:
		check(not player.player_action(action,{}).ok,"unknown player action refused")
	var cyclone = player.player_action("cyclone",{"object_flags":0x40})
	check(cyclone.ok and cyclone.state==6,"cyclone stores 6")
	var ancestral = player.player_action("ancestral_call",{"weapon_class":0,"object_flags":0x40})
	check(ancestral.ok and ancestral.state==6,"ancestral call stores 6")
	check(not player.player_action("roar",{"weapon_class":4}).ok,"weapon class 4 shout refused")
	var charge = player.player_action("charge",{"object_flags":0x40})
	check(charge.ok and charge.state==9,"charge stores 9")
	check(player.player_action("charge",{"object_flags":0x20}).state==0,"charge without 0x40 stores 0")
	var shadow = player.player_action("shadow_step",{"object_flags":0x40})
	check(shadow.ok and shadow.state==14 and shadow.object_flags==0x45,"shadow step push")
	var shadow_end = player.player_action("shadow_step_end",{"object_flags":0x45})
	check(shadow_end.ok and shadow_end.state==2 and shadow_end.object_flags==0x40,"shadow step returns to state 2")
	var gnaw: RefCounted = ClassDB.instantiate("ChampionsAnimationDispatch")
	check(gnaw.configure(config_for("Gnaw")).ok,"Gnaw dispatch admitted")
	check(gnaw.select_state(0,{},0).ok,"Gnaw idle clip")
	check(not player.player_action("warcry",{"weapon_class":4}).ok,"unwritten bow warcry name refused")
	check(player.player_action("ground_pound",{}).ok,"ground pound dispatch")
	var ground_events := 0
	for frame in range(2,94):
		result=player.tick(frame,0x3f800000,false,false)
		check(result.ok,"ground pound pose tick")
		ground_events |= result.events
	check(ground_events&15==15,"all ground pound masks emitted")
	check(player.player_action("knockdown",{"relative_hit":-8192}).ok,"directional damage dispatch")
	check(player.player_action("death",{"weapon_class":2,"model":0}).ok,"player death selection")
	result=player.tick(94,0x3f800000,false,false)
	check(result.ok and result.flags&0x206==0x206,"player death end hold")
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX]=render_positions
	arrays[Mesh.ARRAY_NORMAL]=render_normals
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES,arrays)
	check(mesh.get_surface_count()==1,"dispatch pose builds mesh")
	if "--render" in OS.get_cmdline_user_args():
		await render_pose(mesh,Transform3D.IDENTITY)
	print("CREATURE_DISPATCH_SMOKE: ",checks," checks pass")
	quit(0)
