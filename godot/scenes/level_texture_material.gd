extends RefCounted

static var _shaders: Dictionary = {}

# GS alpha is 0..128. Blend equations are the geometry-chunk header values:
# 2 replaces the destination with src × (1 − As), 1 is src × (1 − As) + dst × As,
# 9 is src + dst × As, 0x48 is dst + src × As, and 0x2a (context 2) is src.
# Flag 0x400 discards a fragment whose alpha byte is at or above 0x2f.
# World chunks keep the frame depth test and depth write. Automap tiles do not.
const PLAY_LIGHT_BASE := 128

static func create(source: Texture2D, surface: Dictionary, native_lighting := false) -> ShaderMaterial:
	var alpha1 := int(surface.alpha1)
	var cutout := int(surface.test1) == 0x582f5
	var blend := bool(surface.blend_enabled)
	var env := bool(surface.get("environment", false)) and bool(surface.get("env_ready", false))
	var mode := "replace"
	if env:
		mode = "env"
	elif blend and alpha1 == 1:
		mode = "mix"
	elif blend and alpha1 == 0x48:
		mode = "add"
	elif blend and alpha1 == 9:
		mode = "premul"
	elif blend and alpha1 == 2:
		mode = "darken"
	elif blend and alpha1 == 0x44:
		mode = "usual"
	var depth := "depth_draw_opaque"
	if mode == "usual":
		depth = "depth_draw_never, depth_test_disabled"
	elif mode == "mix" or mode == "add" or mode == "premul":
		depth = "depth_draw_always"
	var render := "render_mode unshaded, cull_disabled, " + depth + ";\n"
	if mode == "mix" or mode == "usual":
		render = "render_mode unshaded, cull_disabled, blend_mix, " + depth + ";\n"
	elif mode == "add":
		render = "render_mode unshaded, cull_disabled, blend_add, " + depth + ";\n"
	elif mode == "premul":
		render = "render_mode unshaded, cull_disabled, blend_premul_alpha, " + depth + ";\n"
	var filter := "filter_nearest" if mode == "usual" else "filter_linear"
	var code := "shader_type spatial;\n" + render
	code += "uniform sampler2D base_texture : source_color, " + filter + ", repeat_disable;\n"
	code += "uniform bool cutout = false;\n"
	code += "uniform int alpha_mode = 2;\n"
	code += "uniform bool use_env = false;\n"
	code += "uniform sampler2D env_texture : source_color, filter_linear, repeat_enable;\n"
	code += "uniform vec4 env_row4 = vec4(0.0);\n"
	code += "uniform vec4 env_row5 = vec4(192.0, 192.0, 192.0, 192.0);\n"
	if mode == "env":
		code += "varying vec3 stored_normal;\n"
		code += "void vertex() {\n stored_normal = CUSTOM0.xyz;\n}\n"
	code += "void fragment() {\n vec4 texel = texture(base_texture, UV);\n"
	code += " float gs = clamp(texel.a * (255.0 / 128.0), 0.0, 1.0);\n"
	code += " bool keep = !(cutout && texel.a * 255.0 >= 47.0);\n"
	code += " vec3 lit = texel.rgb * COLOR.rgb;\n" if native_lighting else " vec3 lit = texel.rgb;\n"
	code += " vec3 rgb = lit;\n"
	if mode == "env":
		code += " vec3 n = stored_normal / 16.0;\n"
		code += " vec2 centre = VIEWPORT_SIZE * 0.5;\n"
		code += " vec2 screen = vec2(FRAGCOORD.x - centre.x, centre.y - FRAGCOORD.y);\n"
		code += " float u = env_row5.z + n.x * env_row4.x + n.y * env_row4.z + n.z * env_row5.x + 0.07 * screen.x;\n"
		code += " float v = env_row5.w + n.x * env_row4.y + n.y * env_row4.w + n.z * env_row5.y + 0.07 * screen.y;\n"
		code += " vec3 env_lit = texture(env_texture, vec2(u, v) / vec2(textureSize(env_texture, 0))).rgb;\n"
		code += " if (use_env) {\n"
		code += "  vec3 src = lit;\n  vec3 dest = env_lit;\n"
		code += "  if (alpha_mode == 2) rgb = src * (1.0 - gs);\n"
		code += "  else if (alpha_mode == 1) rgb = src * (1.0 - gs) + dest * gs;\n"
		code += "  else if (alpha_mode == 9) rgb = src + dest * gs;\n"
		code += "  else if (alpha_mode == 72) rgb = dest + src * gs;\n"
		code += "  else rgb = src * gs + dest * (1.0 - gs);\n"
		code += "  if (!keep) rgb = dest;\n"
		code += " } else if (!keep) discard;\n"
	elif mode == "darken":
		code += " if (!keep) discard;\n rgb = lit * (1.0 - gs);\n"
	elif mode == "mix":
		code += " if (!keep) discard;\n"
	elif mode == "add" or mode == "premul" or mode == "usual":
		code += " if (!keep) discard;\n"
	else:
		code += " if (!keep) discard;\n"
	code += " ALBEDO = rgb;\n"
	if mode == "mix":
		code += " ALPHA = clamp(1.0 - gs, 0.0, 1.0);\n"
	elif mode == "add":
		code += " ALPHA = gs;\n"
	elif mode == "premul":
		code += " ALPHA = clamp(1.0 - gs, 0.0, 1.0);\n"
	elif mode == "usual":
		code += " ALPHA = clamp(COLOR.a * (255.0 / 128.0), 0.0, 1.0);\n"
	code += "}\n"
	# One Shader per distinct program: a large level has thousands of surfaces
	# but only a handful of material modes.
	var shader: Shader = _shaders.get(code)
	if shader == null:
		shader = Shader.new()
		shader.code = code
		_shaders[code] = shader
	var material := ShaderMaterial.new()
	material.shader = shader
	if native_lighting:
		material.set_meta("native_fx_vertex_colors", true)
	material.render_priority = int(surface.get("sort_layer", 0))
	material.set_shader_parameter("base_texture", source)
	material.set_shader_parameter("cutout", cutout)
	material.set_shader_parameter("alpha_mode", alpha1)
	material.set_shader_parameter("use_env", env)
	return material

static func automap_material(atlas: Texture2D) -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix, depth_draw_never, depth_test_disabled;
uniform sampler2D atlas : source_color, filter_nearest, repeat_disable;
void fragment() {
	vec4 texel = texture(atlas, UV);
	ALBEDO = texel.rgb;
	ALPHA = clamp(COLOR.a * (255.0 / 128.0), 0.0, 1.0);
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	material.render_priority = 5
	material.set_shader_parameter("atlas", atlas)
	return material
