extends RefCounted

# Texture for the explicitly chosen creature preview model. Association and
# rules: research docs/formats/model-texture.md. A model has no texture name;
# creature class definitions name `x.vif` then `x.tex`, and every .vif has a
# same-base .tex in its lump. UVs are per corner; header +0x18 value 1 is
# out = src + (dst - src) x As and 9 is out = src + dst x As (As = alpha/128,
# alpha 0 opaque); TEST_1 passes alpha < 0x7f; each packet's PRIM says whether
# blending is on. Other +0x18 values and the context-2 environment pass are not
# documented for display, so those draw the base texture opaque (labelled).
var texture: ImageTexture
var alpha1 := 0
var meshes: Array = [] # bound meshes in model order
var materials: Dictionary = {} # blend state -> ShaderMaterial
var applied := 0
var gated := false

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
	var tex := {}
	for row in rows:
		if typeof(row) == TYPE_DICTIONARY and str(row.get("name", "")).to_lower() == want and str(row.get("parent", "")) == str(vif.get("parent", "")):
			tex = row
			break
	if tex.is_empty(): return {"ok": false, "error": "no same-base texture in the model's lump"}
	var result: Dictionary = ClassDB.class_call_static("ChampionsRealWorld", "model_texture", _row_bytes(archive, vif), _row_bytes(archive, tex))
	if not result.get("ok", false): return result
	var image := Image.create_from_data(int(result.width), int(result.height), false, Image.FORMAT_RGBA8, result.rgba)
	if image == null: return {"ok": false, "error": "texture image could not be created"}
	texture = ImageTexture.create_from_image(image)
	alpha1 = int(result.alpha1)
	meshes = result.meshes
	gated = not (alpha1 == 1 or alpha1 == 9)
	materials.clear()
	return {"ok": true, "alpha1_documented": not gated}

func _material(blend: int) -> ShaderMaterial:
	if materials.has(blend): return materials[blend]
	var render := "render_mode unshaded, cull_disabled, depth_draw_opaque;\n"
	var alpha := ""
	if blend == 1 and alpha1 == 1:
		# src (1 - As) + dst As: Godot mix with alpha 1 - As.
		render = "render_mode unshaded, cull_disabled, blend_mix, depth_draw_always;\n"
		alpha = " ALPHA = clamp(1.0 - texel.a, 0.0, 1.0);\n"
	elif blend == 1 and alpha1 == 9:
		# src + dst As: premultiplied blend with alpha 1 - As.
		render = "render_mode unshaded, cull_disabled, blend_premul_alpha, depth_draw_always;\n"
		alpha = " ALPHA = clamp(1.0 - texel.a, 0.0, 1.0);\n"
	var code := "shader_type spatial;\n" + render
	code += "uniform sampler2D base_texture : source_color, filter_linear, repeat_disable;\n"
	code += "void fragment() {\n vec4 texel = texture(base_texture, UV);\n"
	# TEST_1 0x507f5: a pixel passes while its raw alpha is below 0x7f. The
	# decoder stores min(255, raw * 255 / 128), so raw 0x7f is 253.
	code += " if (texel.a * 255.0 >= 252.5) discard;\n"
	code += " ALBEDO = texel.rgb * COLOR.rgb;\n" + alpha + "}\n"
	var shader := Shader.new()
	shader.code = code
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_shader_parameter("base_texture", texture)
	material.set_meta("native_fx_vertex_colors", true) # ALBEDO uses COLOR
	materials[blend] = material
	return material

# The preview builds one surface per default-visible mesh with corners, in
# model order. Rebuild its current mesh with the bound UVs. One blend state
# for every surface becomes the instance override, which native FX lighting
# accepts; mixed states use per-surface materials (lighting then refuses).
func apply(instance: MeshInstance3D) -> bool:
	if texture == null or instance == null or instance.mesh == null: return false
	var visible: Array = []
	for m in meshes:
		if m.visible_by_default and PackedVector2Array(m.uv).size() > 0: visible.append(m)
	var source: Mesh = instance.mesh
	if visible.size() != source.get_surface_count() or visible.is_empty(): return false
	var first: Array = source.surface_get_arrays(0)
	if first[Mesh.ARRAY_TEX_UV] is PackedVector2Array and PackedVector2Array(first[Mesh.ARRAY_TEX_UV]).size() == PackedVector2Array(visible[0].uv).size():
		return true # already textured (or a lighting copy of it)
	var states := {}
	for m in visible: states[-1 if gated else int(m.blend)] = true
	var mesh := ArrayMesh.new()
	for k in range(source.get_surface_count()):
		var arrays: Array = source.surface_get_arrays(k)
		var uv: PackedVector2Array = visible[k].uv
		if uv.size() != PackedVector3Array(arrays[Mesh.ARRAY_VERTEX]).size(): return false
		arrays[Mesh.ARRAY_TEX_UV] = uv
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		if states.size() > 1: mesh.surface_set_material(k, _material(-1 if gated else int(visible[k].blend)))
	instance.mesh = mesh
	instance.material_override = _material(states.keys()[0]) if states.size() == 1 else null
	applied += 1
	return true
