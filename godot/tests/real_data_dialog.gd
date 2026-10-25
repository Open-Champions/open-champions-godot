extends SceneTree

# Opt-in run of the NPC dialog on the user's own game folder through the
# real-data play scene (docs/DIALOG.md). For every level with a talker it
# attaches the level script with the dialog, starts play, presses the use
# button where the player stands, then talks to each talker through the
# controller's labelled talk-to diagnostic, confirming every eighth tick, and
# draws the box each frame. Prints and writes aggregate counts only; never
# part of CI. Arguments:
#   --game-folder=DIR --cache=DIR [--summary=FILE] [--capture-dir=DIR]
#   [--from=I] [--to=J] [--last-choice] [--voice-gate-probe]
var args := {}
var scene: Control
var counts := {}
var failures := 0

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
		elif argument.begins_with("--"):
			args[argument.substr(2)] = true
	run.call_deferred()

func add(key: String, n := 1) -> void:
	counts[key] = int(counts.get(key, 0)) + n

func check(ok: bool, what: String) -> bool:
	if ok: add("checks.passed")
	else:
		failures += 1
		add("checks.failed")
		printerr("FAILED: " + what)
	return ok

func write_summary() -> Dictionary:
	var summary := {"counts": counts, "failures": failures}
	if args.has("summary"):
		var file := FileAccess.open(str(args["summary"]), FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(summary, "  ", true) + "\n")
	return summary

func frame_counts() -> void:
	var frame: Dictionary = scene.npc_dialog.frame
	if not frame.get("ok", false) or not frame.get("open", false): return
	add("draw.frames_open")
	if not frame.get("sprites", []).is_empty(): add("draw.frames_with_box")
	if not frame.get("glyphs", []).is_empty(): add("draw.frames_with_text")
	if int(frame.get("speaker_glyph_quads", 0)) > 0: add("draw.frames_with_speaker")
	add("draw.glyphs", frame.get("glyphs", []).size())
	for why in frame.get("refusals", []): add("draw.refused " + str(why))
	# The cursor and "more" glyph (docs/spec/dialog-gaps.md 5): quads made from the byte at 0x41b978.
	add("draw.cursor_glyph_quads", int(frame.get("cursor_glyph_quads", 0)))
	add("draw.speaker_glyph_quads", int(frame.get("speaker_glyph_quads", 0)))
	check(scene.controller.text_hud().get("suppressed", false), "open dialog suppresses the HUD")
	check(not scene.play_hud.visible and not scene.hud_round3.visible, "open dialog hides the status and auxiliary HUD layers")

# Voice stream 1 as the native mixer reports it (counts only).
# The conversation camera (docs/spec/dialog-gaps.md 2): frames with a published pose, frames
# with the swoop still moving, and the frontend gate that follows dialog_isOpen.
func camera_counts() -> void:
	var cam: Dictionary = scene.controller.dialog_camera()
	if cam.get("pose", false): add("camera.pose_frames")
	if cam.get("moving", false): add("camera.moving_frames")
	var gate: Dictionary = scene.controller.dialog_gate("prop_draw", {})
	if gate.get("ok", false) and gate.get("outcome", "") == "skip": add("gate.props_hidden_frames")

func voice_counts() -> void:
	var audio = scene.get("audio")
	if audio == null or audio.get("bridge") == null: return
	var streams: Array = audio.bridge.get_observation().get("streams", [])
	if streams.size() < 2: return
	var s: Dictionary = streams[1]
	if s.get("active", false): add("voice.frames_active")
	if int(s.get("entry_position", 0)) > 0: add("voice.frames_position_positive")
	if int(s.get("queued", 0)) > 0: add("voice.frames_queued")
	if int(s.get("status", 0)) & 1: add("voice.frames_playing")

# Explicit test stimulus, with no ready tick between the two draw calls.
# Verifies that the bridge passes stream status to the text-off draw gate;
# the live mixer observation is restored before the conversation advances.
func probe_voice_gate() -> void:
	if not args.has("voice-gate-probe") or counts.get("probe.voice_gate", 0) != 0: return
	var status: Dictionary = scene.controller.dialog_status()
	if not status.get("prompt_shown", false) or status.get("closing", false): return
	var bridge = scene.audio.bridge
	var streams: Array = bridge.get_observation().get("streams", [])
	if streams.size() < 2: return
	var live: Dictionary = streams[1]
	scene.controller.dialog_voice_state({"queue_count": 0, "status": 1, "position": 0})
	var playing: Dictionary = scene.controller.dialog_frame(0)
	scene.controller.dialog_voice_state({"queue_count": 0, "status": 0, "position": 0})
	var silent: Dictionary = scene.controller.dialog_frame(0)
	scene.controller.dialog_voice_state({"queue_count": int(live.get("queued", 0)), "status": int(live.get("status", 0)), "position": int(live.get("entry_position", 0))})
	check(playing.get("ok", false) and int(playing.get("cursor_glyph_quads", 0)) == 0, "text-off prompt is hidden while voice status is playing")
	check(silent.get("ok", false) and int(silent.get("cursor_glyph_quads", 0)) > 0, "text-off prompt appears after voice status clears")
	add("probe.voice_gate")

# A level script can open an offline help box before or during a talk. The
# draw task owns confirmation for its 90-frame delay; advance no dialog input
# until it has closed and released the session pause.
func confirm_help() -> void:
	if not scene.controller.text_hud().get("help_open", false): return
	add("help.boxes_confirmed_by_harness")
	for frame in range(200):
		if scene.controller.help_frame(true).get("closed", false): return
	check(false, "the help box closes within the documented delay")

func run() -> void:
	var cache := str(args.get("cache", ""))
	if cache.is_empty() or not args.has("game-folder"):
		printerr("--game-folder and --cache are required")
		quit(2)
		return
	if args.has("capture-dir"): DirAccess.make_dir_recursive_absolute(str(args["capture-dir"]))
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = cache.path_join("level-library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = cache
	check(scene.open_game_folder(str(args["game-folder"])), "game folder opens")
	check(scene.npc_dialog.font_texture != null, "gamefontmed.fnt decodes")
	check(scene.npc_dialog.box_texture != null, "box.tex of dlgbox.lmp decodes")
	if scene.npc_dialog.box_texture != null:
		add("box.width", scene.npc_dialog.box_texture.get_width())
		add("box.height", scene.npc_dialog.box_texture.get_height())
	var first := int(args.get("from", "0"))
	var last := mini(int(args.get("to", str(scene.real.levels.size()))), scene.real.levels.size())
	var captured := false
	var captured_choices := false
	var captured_closed := false
	for index in range(first, last):
		scene.return_to_selection()
		for i in range(3): await process_frame
		var loaded: Dictionary = scene.real.load_level(index)
		if not loaded.ok:
			add("levels.refused")
			continue
		add("levels.loaded")
		scene.level_script.set_pressed_no_signal(true)
		if not scene.attach_level_script():
			add("levels.script_refused")
			continue
		var status: Dictionary = scene.controller.dialog_status()
		if not check(status.get("ok", false), "the dialog attaches with the level script"): continue
		var talkers := int(status.get("talkers", 0))
		if talkers == 0: continue
		add("levels.with_talkers")
		add("talkers", talkers)
		scene.actions.play.pressed.emit()
		if not check(scene.play_mode, "play starts on a level with talkers"): continue
		confirm_help()
		# The use button where the player stands (story start).
		# Wait until a ready tick has taken the press.
		var uses_before := int(scene.controller.dialog_status().get("counts", {}).get("uses", 0))
		scene.controller.dialog_use()
		for i in range(30):
			scene.advance_step()
			confirm_help()
			if int(scene.controller.dialog_status().get("counts", {}).get("uses", 0)) > uses_before: break
		add("use." + str(scene.controller.dialog_status().get("last_use", "")))
		for talker in range(talkers):
			confirm_help()
			for i in range(4000):
				if not scene.controller.dialog_status().get("open", false): break
				# Closing waits for displayed-frame fades. Pump the view here as
				# in the measured conversation loop, and stop confirming once
				# an ending choice was accepted (the original can run it again).
				await process_frame
				if i % 8 == 7 and not scene.controller.dialog_status().get("closing", false): scene.controller.dialog_press(0x40)
				scene.advance_step()
				confirm_help()
			if scene.controller.dialog_status().get("stalled", false): scene.leave_stalled_dialog()
			if not check(not scene.controller.dialog_status().get("open", false), "previous conversation closes before a fresh talk attempt"): continue
			if not scene.controller.dialog_talk_to(talker).get("ok", false):
				add("talk.refused_index")
				continue
			add("talk.tried")
			scene.advance_step()
			confirm_help()
			var opened: bool = scene.controller.dialog_status().get("open", false)
			add("talk.opened" if opened else "talk.not_opened")
			var ticks := 0
			while ticks < 4000 and scene.controller.dialog_status().get("open", false):
				await process_frame # the view draws one dialog frame per displayed frame
				frame_counts()
				voice_counts()
				camera_counts()
				probe_voice_gate()
				if not captured and args.has("capture-dir") and DisplayServer.get_name() != "headless" and scene.npc_dialog.frame.get("glyphs", []).size() > 40:
					await RenderingServer.frame_post_draw
					var image := root.get_texture().get_image()
					if image != null and image.save_png(str(args["capture-dir"]).path_join("dialog-open.png")) == OK:
						add("captures")
						captured = true
				var current: Dictionary = scene.controller.dialog_status()
				if not captured_choices and args.has("capture-dir") and DisplayServer.get_name() != "headless" and current.get("prompt_shown", false) and int(current.get("visible_choices", 0)) > 1:
					await RenderingServer.frame_post_draw
					var image := root.get_texture().get_image()
					if image != null and image.save_png(str(args["capture-dir"]).path_join("dialog-choices.png")) == OK:
						add("captures")
						captured_choices = true
				var pad := 0
				if ticks % 8 == 7: pad = 0x40
				if args.has("last-choice") and int(current.get("visible_choices", 0)) > 1:
					# Walk all the way to the last visible choice. The previous
					# harness only moved down once and could confirm mid-blend.
					var cursor := int(current.get("cursor", 0))
					var target := int(current.get("cursor_target", 0))
					if cursor != target: pad = 0
					elif cursor + 1 < int(current.visible_choices) and current.get("prompt_shown", false): pad = 0x4000
				if current.get("closing", false): pad = 0
				if pad != 0: scene.controller.dialog_press(pad)
				scene.advance_step()
				confirm_help()
				ticks += 1
				if scene.controller.dialog_status().get("stalled", false):
					add("talk.stalled")
					scene.leave_stalled_dialog()
					break
			if opened:
				if not scene.controller.dialog_status().get("open", false):
					add("talk.closed")
					scene._present_npc_dialog()
					check(scene.play_hud.visible and scene.hud_round3.visible, "closed dialog restores the HUD layers")
					if not captured_closed and args.has("capture-dir") and DisplayServer.get_name() != "headless":
						await process_frame
						await RenderingServer.frame_post_draw
						var image := root.get_texture().get_image()
						if image != null and image.save_png(str(args["capture-dir"]).path_join("dialog-closed.png")) == OK:
							add("captures")
							captured_closed = true
				else: check(false, "conversation closes within the tick limit")
		var talk: Dictionary = scene.controller.dialog_status().get("counts", {})
		for key in talk:
			if key == "interact" or key == "triggers" or key == "mouth" or key == "refused_reasons":
				for result in talk[key]: add("dialog.%s.%s" % [key, str(result)], int(talk[key][result]))
			else:
				add("dialog." + str(key), int(talk[key]))
		write_summary()
	add("voice.lines_queued", scene.npc_dialog.voice_calls)
	scene.return_to_selection()
	scene.real.clear_cache()
	counts["complete"] = 1
	print(JSON.stringify(write_summary(), "  ", true))
	quit(0 if failures == 0 else 1)
