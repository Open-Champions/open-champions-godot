extends RefCounted

# Texture for the explicitly chosen creature preview model. Association and
# rules: research t3/research-textures 1528708, docs/formats/model-texture.md
# and docs/spec/render-pipeline.md 4.7, 6 and 9.6. Header +0x18 is ALPHA_1.
# The documented equations are 1, 2, 9, 0x2a, 0x44, 0x48, and 0x64/0x68 plus
# a FIX byte up to 0x80. TEST_1 passes alpha < 0x7f. Each packet's PRIM says
# whether blending is on. Flag 0x10 adds the context-2 environment pass, drawn
# before the base pass (ALPHA_2 = 0x64 + header +0x20), then the base
# equation over it. A register the ALPHA table does not list, or a FIX above
# 0x80, stays opaque and labelled.
var texture: ImageTexture
var environment: ImageTexture
var alpha1 := 0
var equation := 0
var alpha_fix := 0
var fix2 := 0
var two_pass := false # header flag 0x10
var meshes: Array = [] # bound meshes in model order
var materials: Dictionary = {} # blend state -> ShaderMaterial
var applied := 0
var gated := false
var _instance: MeshInstance3D

static func _row_bytes(archive: String, row: Dictionary) -> PackedByteArray:
	var size := int(row.get("size", 0))
	if size <= 0 or size > 16 * 1024 * 1024: return PackedByteArray()
	var file := FileAccess.open(archive, FileAccess.READ)
	if file == null: return PackedByteArray()
	file.seek(int(row.get("offset", 0)))
	var bytes := file.get_buffer(size)
	return bytes if bytes.size() == size else PackedByteArray()

# rows: the preview's archive rows; model: the chosen .vif row index.
func bind(archive: String, rows: Array, model: int) -> Dictionary:
	if not ClassDB.class_exists("ChampionsRealWorld"): return {"ok": false, "error": "adapter unavailable"}
	var vif: Dictionary = rows[model]
	var want := str(ClassDB.class_call_static("ChampionsRealWorld", "companion_texture_name", str(vif.get("name", "")))).to_lower()
	return bind_named(archive, rows, model, want)

# The player constructor selects a variant texture independently of bodyv1.vif.
func bind_named(archive: String, rows: Array, model: int, texture_name: String,
		reader: RefCounted = null, fingerprint := "") -> Dictionary:
	if not ClassDB.class_exists("ChampionsRealWorld"): return {"ok": false, "error": "adapter unavailable"}
	var vif: Dictionary = rows[model]
	var want := texture_name.to_lower()
	var tex := {}
	var tex_index := -1
	for i in range(rows.size()):
		var row = rows[i]
		if typeof(row) == TYPE_DICTIONARY and str(row.get("name", "")).to_lower() == want and str(row.get("parent", "")) == str(vif.get("parent", "")):
			tex = row
			tex_index = i
			break
	if tex.is_empty(): return {"ok": false, "error": "no same-base texture in the model's lump"}
	var model_bytes := PackedByteArray()
	var texture_bytes := PackedByteArray()
	if reader != null:
		var model_read: Dictionary = reader.read_player_resource(archive, model, fingerprint, str(vif.get("name", "")))
		var texture_read: Dictionary = reader.read_player_resource(archive, tex_index, fingerprint, str(tex.get("name", "")))
		if not model_read.get("ok", false) or not texture_read.get("ok", false):
			return {"ok": false, "error": "player model or texture archive changed during binding"}
		model_bytes = model_read.bytes
		texture_bytes = texture_read.bytes
	else:
		model_bytes = _row_bytes(archive, vif)
		texture_bytes = _row_bytes(archive, tex)
	var result: Dictionary = ClassDB.class_call_static("ChampionsRealWorld", "model_texture", model_bytes, texture_bytes)
	if not result.get("ok", false): return result
	var image := Image.create_from_data(int(result.width), int(result.height), false, Image.FORMAT_RGBA8, result.rgba)
	if image == null: return {"ok": false, "error": "texture image could not be created"}
	texture = ImageTexture.create_from_image(image)
	alpha1 = int(result.alpha1)
	equation = int(result.get("equation", 0))
	alpha_fix = int(result.get("alpha_fix", 0))
	fix2 = int(result.get("fix2", 0))
	two_pass = bool(result.get("two_pass", false))
	meshes = result.meshes
	# render-pipeline.md 6 gives F = FIX / 128 and nothing for F > 1, so a FIX
	# above 0x80 is gated like an unlisted equation (native refuses it too).
	gated = equation == 0 or alpha_fix > 128
	materials.clear()
	return {"ok": true, "alpha1_documented": not gated, "two_pass": two_pass}

func set_environment(image: ImageTexture) -> void:
	if environment == image: return
	environment = image
	materials.clear()
	if _instance != null and is_instance_valid(_instance): apply(_instance)

func _equation_rgb(dst: String) -> String:
	match equation:
		1: return "src * (1.0 - as_w) + %s * as_w" % dst
		2: return "src * (1.0 - as_w)"
		9: return "src + %s * as_w" % dst
		0x2a: return "src"
		0x44: return "src * as_w + %s * (1.0 - as_w)" % dst
		0x48: return "%s + src * as_w" % dst
		0x64: return "src * fix_w + %s * (1.0 - fix_w)" % dst
		0x68: return "%s + src * fix_w" % dst
		_: return "src"

# base_blend / env_blend: 1 on, 0 off, -1 mixed or unstated.
# env_mesh: native `environment`, flag 0x10 and every packet of this mesh
# has the context-2 tag. ALPHA_2 is written only under flag 0x10
# (model-texture.md), so the composite also needs two_pass here, and a
# header +0x20 of at most 0x80.
func _material(base_blend: int, env_blend: int, env_mesh: bool) -> ShaderMaterial:
	var composite := env_mesh and two_pass and fix2 <= 128 and environment != null and equation != 0 and base_blend != -1 and env_blend != -1
	var key := "%d|%d|%d|%d|%d|%s" % [equation, alpha_fix, fix2, base_blend, env_blend, "env" if composite else "fb"]
	if materials.has(key): return materials[key]
	var sample_screen := composite and env_blend == 1 and fix2 < 128
	var render := "render_mode unshaded, cull_disabled, depth_draw_opaque;\n"
	var alpha := ""
	var albedo := " ALBEDO = src;\n"
	if not composite and base_blend == 1 and equation != 0:
		if equation == 1 or equation == 0x44 or equation == 0x64:
			render = "render_mode unshaded, cull_disabled, blend_mix, depth_draw_always;\n"
		elif equation == 9:
			render = "render_mode unshaded, cull_disabled, blend_premul_alpha, depth_draw_always;\n"
		elif equation == 0x48 or equation == 0x68:
			render = "render_mode unshaded, cull_disabled, blend_add, depth_draw_always;\n"
		if equation == 1 or equation == 9:
			alpha = " ALPHA = clamp(1.0 - as_w, 0.0, 1.0);\n"
		elif equation == 0x44 or equation == 0x48:
			alpha = " ALPHA = as_w;\n"
		elif equation == 0x64 or equation == 0x68:
			alpha = " ALPHA = fix_w;\n"
		elif equation == 2:
			albedo = " ALBEDO = src * (1.0 - as_w);\n"
	var code := "shader_type spatial;\n" + render
	code += "uniform sampler2D base_texture : source_color, filter_linear, repeat_disable;\n"
	code += "uniform float fix_w = 0.0;\n"
	if composite:
		code += "uniform sampler2D env_texture : source_color, filter_linear, repeat_enable;\n"
		code += "uniform vec2 env_size = vec2(128.0);\n"
		code += "uniform float env_w = 1.0;\n"
		if sample_screen:
			code += "uniform sampler2D screen_texture : hint_screen_texture, filter_linear, repeat_disable;\n"
		# render-pipeline.md 4.7 (V): program 0x44 writes the second UV per
		# vertex, (192 + 64 c.x, 192 + 64 c.y) texels with c = the camera-space
		# unit normal x 0.99, and the GS interpolates it. Godot's view y is up
		# and the game camera's y is down, so y is negated. Computed here, not
		# per pixel, and before Godot flips back-face normals. A zero normal
		# stays at texel 192.
		code += "varying vec2 env_uv;\n"
		code += "void vertex() {\n vec3 n = MODELVIEW_NORMAL_MATRIX * NORMAL;\n float len2 = dot(n, n);\n"
		code += " vec2 c = len2 > 0.0 ? vec2(n.x, -n.y) * inversesqrt(len2) * 0.99 : vec2(0.0);\n"
		code += " env_uv = (vec2(192.0) + 64.0 * c) / env_size;\n}\n"
	code += "void fragment() {\n vec4 texel = texture(base_texture, UV);\n"
	# TEST_1 0x507f5: pass while raw alpha is below 0x7f. The decoder stores
	# min(255, raw * 255 / 128), so raw 0x7f is byte 253.
	code += " bool keep = texel.a * 255.0 < 252.5;\n"
	code += " vec3 src = texel.rgb * COLOR.rgb;\n float as_w = clamp(texel.a, 0.0, 1.0);\n"
	if composite:
		# Assumption (I, render-pipeline.md 9.6 "draw the environment texture
		# first"): the environment pass covers every pixel of the mesh and the
		# composite writes depth once, so a texel that fails TEST_1 shows the
		# environment sample. Model setup writes TEST_1 only; the TEST_2 and
		# ZBUF of the original's context-2 pass are not documented.
		code += " vec3 env_src = texture(env_texture, env_uv).rgb * COLOR.rgb;\n vec3 pass1 = env_src;\n"
		if env_blend == 1:
			if sample_screen:
				code += " if (env_w < 1.0) {\n  vec3 fb = texture(screen_texture, SCREEN_UV).rgb;\n"
				code += "  pass1 = env_src * env_w + fb * (1.0 - env_w);\n }\n"
			# env_w == 1 replaces the destination with the environment sample.
		code += " vec3 rgb = pass1;\n if (keep) {\n"
		if base_blend == 0:
			code += "  rgb = src;\n"
		else:
			code += "  rgb = %s;\n" % _equation_rgb("pass1")
		code += " }\n ALBEDO = rgb;\n}\n"
	else:
		code += " if (!keep) discard;\n"
		code += albedo
		code += alpha + "}\n"
	var shader := Shader.new()
	shader.code = code
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_shader_parameter("base_texture", texture)
	material.set_shader_parameter("fix_w", float(alpha_fix) / 128.0)
	if composite:
		material.set_shader_parameter("env_texture", environment)
		material.set_shader_parameter("env_size", Vector2(environment.get_size()))
		material.set_shader_parameter("env_w", float(fix2) / 128.0)
	material.set_meta("native_fx_vertex_colors", true) # ALBEDO uses COLOR
	materials[key] = material
	return material

# The preview builds one surface per default-visible mesh with corners, in
# model order. Rebuild its current mesh with the bound UVs. One blend state
# for every surface becomes the instance override; mixed states use per-surface
# materials. Both paths consume the native FX vertex colours.
func apply(instance: MeshInstance3D) -> bool:
	_instance = instance
	if texture == null or instance == null or instance.mesh == null: return false
	var visible: Array = []
	for m in meshes:
		if m.visible_by_default and PackedVector2Array(m.uv).size() > 0: visible.append(m)
	var source: Mesh = instance.mesh
	if visible.size() != source.get_surface_count() or visible.is_empty(): return false
	var first: Array = source.surface_get_arrays(0)
	var already := first[Mesh.ARRAY_TEX_UV] is PackedVector2Array and PackedVector2Array(first[Mesh.ARRAY_TEX_UV]).size() == PackedVector2Array(visible[0].uv).size()
	var states := {}
	for m in visible:
		var base := -1 if gated else int(m.blend)
		var env_mesh := (not gated) and bool(m.get("environment", false)) and environment != null and int(m.get("environment_blend", -1)) != -1 and base != -1
		states[_key(base, int(m.get("environment_blend", -1)), env_mesh)] = true
	if not already:
		var mesh := ArrayMesh.new()
		for k in range(source.get_surface_count()):
			var arrays: Array = source.surface_get_arrays(k)
			var uv: PackedVector2Array = visible[k].uv
			if uv.size() != PackedVector3Array(arrays[Mesh.ARRAY_VERTEX]).size(): return false
			arrays[Mesh.ARRAY_TEX_UV] = uv
			mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
			if states.size() > 1:
				mesh.surface_set_material(k, _material_for(visible[k]))
		instance.mesh = mesh
	elif states.size() > 1:
		for k in range(source.get_surface_count()):
			source.surface_set_material(k, _material_for(visible[k]))
	instance.material_override = _material_for(visible[0]) if states.size() == 1 else null
	applied += 1
	return true

func _key(base: int, env_blend: int, env_mesh: bool) -> String:
	return "%d|%d|%s" % [base, env_blend, "env" if env_mesh else "fb"]

func _material_for(mesh: Dictionary) -> ShaderMaterial:
	var base := -1 if gated else int(mesh.blend)
	var env_blend := int(mesh.get("environment_blend", -1))
	var env_mesh := (not gated) and bool(mesh.get("environment", false)) and environment != null and env_blend != -1 and base != -1
	return _material(base, env_blend, env_mesh)
