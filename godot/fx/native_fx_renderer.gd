extends Node3D
class_name NativeFxRenderer

# Presentation only: this node owns no game state. The simulation drives the
# bridge (ChampionsFxBridge): advance_tick() once per simulated tick, present()
# once per rendered frame with the game camera. The bridge computes what the
# original effect drawers send to the GS (particles and groups, kind-2 lines,
# glows, trails, beams; docs/NATIVE_FX.md) and returns draw runs in GS order.
# This script draws each run in clip space, depth-tested against the 3D scene.

# Godot units per game unit. Used only to place the depth of each vertex.
@export var world_scale: float = 1.0
# The original frame is 1280 x 448 displayed at 4:3. With keep_aspect the runs
# are scaled so a viewport of another aspect shows them at the original shape.
@export var keep_aspect: bool = true

var bridge: Object
var textures: Dictionary = {}
var _runs: Array[MeshInstance3D] = []
var _materials: Dictionary = {}
var _original_meshes: Dictionary = {}
var _lit_meshes: Dictionary = {}
var _mesh_owners: Dictionary = {}
# Existing standalone callers retain Godot coordinates. The play adapter opts
# into the native XY floor / Z-up convention for particles and lighting.
var native_z_up := false

func to_render(value: Vector3) -> Vector3:
    return Vector3(value.x, value.z, -value.y) if native_z_up else value

func to_native(value: Vector3) -> Vector3:
    return Vector3(value.x, -value.z, value.y) if native_z_up else value

func clear_vertex_lighting() -> void:
    for key in _mesh_owners:
        var owner = _mesh_owners[key].get_ref()
        if owner != null and owner.mesh == _lit_meshes.get(key):
            owner.mesh = _original_meshes[key]
            owner.material_override = owner.get_meta("fx_original_override") if owner.has_meta("fx_original_override") else null
    _original_meshes.clear()
    _lit_meshes.clear()
    _mesh_owners.clear()


# GS ALPHA_1 values the effect drawers use, and the matching Godot blends:
# 0x44 (Cs - Cd) * As + Cd, 0x48 Cs * As + Cd, 0x09 Cs + Cd * As.
const BLENDS := {0x44: "blend_mix", 0x48: "blend_add", 0x09: "blend_premul_alpha"}

const RUN_SHADER := """
shader_type spatial;
render_mode unshaded, BLEND_MODE, depth_draw_never, cull_disabled, shadows_disabled;
uniform sampler2D tex : source_color, FILTER;
uniform bool textured = true;
uniform bool inverse_alpha = false;
uniform float world_scale = 1.0;
uniform float x_scale = 1.0;
void vertex() {
    // VERTEX = (x, y) in the original's normalised screen, z = view depth.
    vec4 c = PROJECTION_MATRIX * vec4(0.0, 0.0, -VERTEX.z * world_scale, 1.0);
    // +y is up in the GS runs. The projection carries the backend's clip-space
    // Y correction (negative [1][1] where clip Y points down); apply the same.
    float y_sign = sign(PROJECTION_MATRIX[1][1]);
    POSITION = vec4(VERTEX.x * x_scale * c.w, VERTEX.y * y_sign * c.w, c.z, c.w);
}
void fragment() {
    // Untextured (kind-2 lines): the vertex colour itself, alpha A / 128.
    vec4 texel = textured ? texture(tex, UV) : vec4(0.5, 0.5, 0.5, 1.0);
    // GS modulate: 0x80 in either the texel or the vertex is 1.0.
    vec3 rgb = min(texel.rgb * COLOR.rgb * 2.0, vec3(1.0));
    float a = clamp(texel.a * COLOR.a * 2.0, 0.0, 1.0);
    ALBEDO = rgb;
    ALPHA = inverse_alpha ? 1.0 - a : a;
}
"""

func _ready() -> void:
    bridge = ClassDB.instantiate("ChampionsFxBridge")

func load_definition_file(path: String) -> Dictionary:
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return {"ok": false, "error": FileAccess.get_open_error()}
    return bridge.load_definitions_json(file.get_as_text())

# Reads the effect textures by name from the user's own archives (a .GOB that
# holds fx.lmp and engine.lmp, or the two .LMP files). Nothing is bundled.
func load_textures(archive_paths: PackedStringArray) -> Dictionary:
    var result: Dictionary = bridge.load_effect_textures(archive_paths)
    for id in result.get("textures", {}):
        var row: Dictionary = result["textures"][id]
        var image := Image.create_from_data(row["width"], row["height"], false,
            Image.FORMAT_RGBA8, row["rgba"])
        textures[id] = ImageTexture.create_from_image(image)
    return result

func advance_tick(sim_frame: int, cutscene: bool = false) -> void:
    bridge.tick(sim_frame, cutscene)

# camera: the game camera as the bridge expects it (see ChampionsFxBridge.draw).
func present(camera: Dictionary, frozen: bool = false) -> Dictionary:
    var frame: Dictionary = bridge.draw(camera, frozen)
    if not frame.get("ok", false):
        return frame
    var runs: Array = frame["runs"]
    while _runs.size() < runs.size():
        var node := MeshInstance3D.new()
        node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
        add_child(node)
        _runs.append(node)
    var viewport_size := Vector2(get_viewport().size)
    var x_scale := 1.0
    if keep_aspect and viewport_size.y > 0.0:
        x_scale = (4.0 / 3.0) / (viewport_size.x / viewport_size.y)
    var missing: Array = []
    for i in _runs.size():
        var node := _runs[i]
        if i >= runs.size():
            node.visible = false
            continue
        var run: Dictionary = runs[i]
        # A run whose primitives were all culled has no vertices; Godot refuses
        # an empty surface, so there is nothing to draw.
        if run["vertices"].is_empty():
            node.visible = false
            continue
        var texture_id: String = run["texture"]
        var texture: Texture2D = textures.get(texture_id)
        if texture_id != "" and texture == null:
            if not missing.has(texture_id):
                missing.append(texture_id)
            node.visible = false
            continue
        node.visible = true
        node.mesh = _run_mesh(run, _material(run, texture, x_scale, i))
    var refusals: Array = frame.get("refusals", [])
    for id in missing:
        refusals.append("texture %s not loaded from the user's files" % id)
    return {"ok": true, "runs": runs.size(), "refusals": refusals}

func _material(run: Dictionary, texture: Texture2D, x_scale: float, order: int) -> ShaderMaterial:
    var blend: int = run["blend"]
    var filter: String = "filter_linear" if run["filter"] == "linear" else "filter_nearest"
    var key := "%d/%s" % [blend, filter]
    var shader: Shader = _materials.get(key)
    if shader == null:
        shader = Shader.new()
        shader.code = RUN_SHADER.replace("BLEND_MODE", BLENDS.get(blend, "blend_mix")) \
            .replace("FILTER", filter)
        _materials[key] = shader
    var mat := ShaderMaterial.new()
    mat.shader = shader
    # GS order: later runs are drawn over earlier ones.
    mat.render_priority = clampi(order - 128, -128, 127)
    mat.set_shader_parameter("tex", texture)
    mat.set_shader_parameter("textured", texture != null)
    mat.set_shader_parameter("inverse_alpha", blend == 0x09)
    mat.set_shader_parameter("world_scale", world_scale)
    mat.set_shader_parameter("x_scale", x_scale)
    return mat

func _run_mesh(run: Dictionary, mat: ShaderMaterial) -> ArrayMesh:
    var arrays: Array = []
    arrays.resize(Mesh.ARRAY_MAX)
    arrays[Mesh.ARRAY_VERTEX] = run["vertices"]
    arrays[Mesh.ARRAY_COLOR] = run["colors"]
    if run["texture"] != "":
        arrays[Mesh.ARRAY_TEX_UV] = run["uvs"]
    var primitive := Mesh.PRIMITIVE_LINES if run["prim"] == "lines" else Mesh.PRIMITIVE_TRIANGLES
    var mesh := ArrayMesh.new()
    mesh.add_surface_from_arrays(primitive, arrays)
    mesh.surface_set_material(0, mat)
    # Vertices are placed in clip space by the shader; never frustum-cull them.
    mesh.custom_aabb = AABB(Vector3(-1e6, -1e6, -1e6), Vector3(2e6, 2e6, 2e6))
    return mesh

# Call after model movement or light changes; call once for static WORLD chunks.
# Source arrays are cached, so repeated calls do not multiply previous tints.
func apply_vertex_lighting(instance: MeshInstance3D, model: bool) -> Dictionary:
    if instance.mesh == null or not instance.mesh is ArrayMesh:
        return {"ok": false, "refusal": "ArrayMesh required"}
    var key := instance.get_instance_id()
    for old in _mesh_owners.keys():
        if _mesh_owners[old].get_ref() == null:
            _mesh_owners.erase(old)
            _original_meshes.erase(old)
            _lit_meshes.erase(old)
    if not _original_meshes.has(key) or instance.mesh != _lit_meshes.get(key):
        _original_meshes[key] = instance.mesh
        instance.set_meta("fx_original_override", instance.material_override)
    _mesh_owners[key] = weakref(instance)
    var source: ArrayMesh = _original_meshes[key]
    var lit := ArrayMesh.new()
    for surface in source.get_surface_count():
        var arrays: Array = source.surface_get_arrays(surface)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
        if normals.size() != vertices.size():
            return {"ok": false, "refusal": "vertex normals required"}
        var old_colors := PackedColorArray()
        if arrays[Mesh.ARRAY_COLOR] is PackedColorArray:
            old_colors = arrays[Mesh.ARRAY_COLOR]
        var colors := PackedColorArray()
        colors.resize(vertices.size())
        for i in vertices.size():
            var world_pos: Vector3 = to_native(instance.global_transform * vertices[i])
            var world_normal: Vector3 = to_native((instance.global_transform.basis.inverse().transposed() * normals[i]).normalized())
            var value: Vector3
            if model:
                value = bridge.shade_model(world_pos, world_normal)
            else:
                value = bridge.shade_world(world_pos, world_normal)
            var original := Color.WHITE
            if old_colors.size() == vertices.size():
                original = old_colors[i]
            colors[i] = original * Color(value.x / 255.0, value.y / 255.0, value.z / 255.0, 1.0)
        arrays[Mesh.ARRAY_COLOR] = colors
        lit.add_surface_from_arrays(source.surface_get_primitive_type(surface), arrays)
        var material := source.surface_get_material(surface)
        if material is StandardMaterial3D:
            var copy: StandardMaterial3D = material.duplicate()
            copy.vertex_color_use_as_albedo = true
            copy.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
            lit.surface_set_material(surface, copy)
        elif material == null:
            var default_material := StandardMaterial3D.new()
            default_material.vertex_color_use_as_albedo = true
            default_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
            lit.surface_set_material(surface, default_material)
        else:
            return {"ok": false, "refusal": "material must consume vertex colors"}
    var override: Material = instance.get_meta("fx_original_override") if instance.has_meta("fx_original_override") else null
    if override is StandardMaterial3D:
        var copy: StandardMaterial3D = override.duplicate()
        copy.vertex_color_use_as_albedo = true
        copy.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
        instance.material_override = copy
    elif override is ShaderMaterial and override.get_meta("native_fx_vertex_colors", false):
        var copy: ShaderMaterial = override.duplicate()
        if override.has_meta("native_fx_unshaded_code"):
            var shader := Shader.new()
            shader.code = override.get_meta("native_fx_unshaded_code")
            copy.shader = shader
        instance.material_override = copy
    elif override != null:
        return {"ok": false, "refusal": "override material must consume vertex colors"}
    instance.mesh = lit
    _lit_meshes[key] = lit
    return {"ok": true}
