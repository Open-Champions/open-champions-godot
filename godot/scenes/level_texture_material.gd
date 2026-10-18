extends RefCounted

# The decoded alpha byte uses the GS 0..128 scale. ALPHA_1=1 reverses the
# usual source/destination weights; the shader passes 1-A/128 to Godot.
static func create(source: Texture2D, surface: Dictionary, native_lighting := false) -> ShaderMaterial:
	var alpha1 := int(surface.alpha1)
	var cutout := int(surface.test1) == 0x582f5
	var blend := bool(surface.blend_enabled)
	var mode := "opaque"
	if blend and alpha1 == 1: mode = "mix"
	elif blend and alpha1 == 0x48: mode = "add"
	# Context-2 environment colour/coordinates are separate inputs. The base
	# texture pass stays visible until they are supplied by an archive loader.
	var shader := Shader.new()
	var render := "render_mode unshaded, cull_disabled, depth_draw_opaque;\n"
	if mode == "mix": render = "render_mode unshaded, cull_disabled, blend_mix, depth_draw_always;\n"
	elif mode == "add": render = "render_mode unshaded, cull_disabled, blend_add, depth_draw_always;\n"
	var code := "shader_type spatial;\n" + render
	code += "uniform sampler2D base_texture : source_color, filter_linear, repeat_disable;\n"
	code += "void fragment() {\n vec4 texel = texture(base_texture, UV);\n"
	if cutout: code += " if (texel.a * 255.0 >= 47.0) discard;\n"
	code += " ALBEDO = texel.rgb * COLOR.rgb;\n" if native_lighting else " ALBEDO = texel.rgb;\n"
	if mode == "mix": code += " ALPHA = clamp(1.0 - texel.a * (255.0 / 128.0), 0.0, 1.0);\n"
	elif mode == "add": code += " ALPHA = clamp(texel.a * (255.0 / 128.0), 0.0, 1.0);\n"
	code += "}\n"
	shader.code = code
	var material := ShaderMaterial.new()
	material.shader = shader
	if native_lighting: material.set_meta("native_fx_vertex_colors", true)
	material.render_priority = int(surface.get("sort_layer", 0))
	material.set_shader_parameter("base_texture", source)
	return material
