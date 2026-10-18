extends Node3D
class_name NativeFxRenderer

# This presentation node owns no authoritative game state. Call advance_tick()
# from the simulation clock, then present() for each rendered frame.
@export var particle_sheet: Texture2D
@export var particle_sheet_height: int = 0
@export var floor_sprite_sheet: Texture2D

var bridge: Object
var _blend: MultiMeshInstance3D
var _add: MultiMeshInstance3D
var _decals: MultiMeshInstance3D
var _original_meshes: Dictionary = {}

const SPRITE_SHADER := """
shader_type spatial;
render_mode unshaded, BLEND_MODE, depth_draw_never, cull_disabled;
uniform sampler2D sheet : source_color, filter_nearest;
uniform float sheet_height = 0.0;
uniform vec2 buffer_size = vec2(1280.0, 448.0);
varying vec2 atlas_uv;
void vertex() {
    vec4 view = MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 clip = PROJECTION_MATRIX * view;
    float depth = max(-view.z, 0.001);
    clip.xy += VERTEX.xy * vec2(4.0, 2.0) * INSTANCE_CUSTOM.r
        * clip.w / (depth * buffer_size);
    POSITION = clip;
    atlas_uv = (INSTANCE_CUSTOM.gb / 16.0 + UV * 30.0) /
        vec2(512.0, sheet_height);
}
void fragment() {
    vec4 texel = texture(sheet, atlas_uv);
    ALBEDO = texel.rgb * COLOR.rgb * 2.0;
    ALPHA = clamp(texel.a * COLOR.a * 2.0, 0.0, 1.0);
}
"""

const DECAL_SHADER := """
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;
uniform sampler2D sheet : source_color, filter_linear;
void vertex() {
    UV = (INSTANCE_CUSTOM.xy + vec2(0.5) +
        UV * (INSTANCE_CUSTOM.zw - INSTANCE_CUSTOM.xy - vec2(1.0))) /
        vec2(256.0, 128.0);
}
void fragment() {
    vec4 texel = texture(sheet, UV);
    ALBEDO = texel.rgb * COLOR.rgb;
    ALPHA = texel.a * COLOR.a;
}
"""

const DECAL_RECTS := [
    Vector4(0, 0, 64, 64), Vector4(64, 64, 96, 96),
    Vector4(64, 96, 96, 128), Vector4(64, 0, 128, 64),
    Vector4(96, 64, 128, 96), Vector4(96, 96, 128, 128),
    Vector4(128, 0, 192, 64), Vector4(128, 64, 160, 96),
    Vector4(128, 96, 160, 128), Vector4(0, 64, 64, 128),
    Vector4(192, 0, 256, 64)
]

func _ready() -> void:
    bridge = ClassDB.instantiate("ChampionsFxBridge")
    _blend = _make_instances("blend_mix")
    _add = _make_instances("blend_add")
    _decals = _make_decal_instances()
    add_child(_blend)
    add_child(_add)
    add_child(_decals)

func _make_instances(blend_mode: String) -> MultiMeshInstance3D:
    var node := MultiMeshInstance3D.new()
    var quad := QuadMesh.new()
    quad.size = Vector2(2, 2)
    var mat := ShaderMaterial.new()
    var shader := Shader.new()
    shader.code = SPRITE_SHADER.replace("BLEND_MODE", blend_mode)
    mat.shader = shader
    quad.material = mat
    var mm := MultiMesh.new()
    mm.transform_format = MultiMesh.TRANSFORM_3D
    mm.use_colors = true
    mm.use_custom_data = true
    mm.mesh = quad
    node.multimesh = mm
    return node

func _make_decal_instances() -> MultiMeshInstance3D:
    var node := MultiMeshInstance3D.new()
    var quad := PlaneMesh.new()
    quad.size = Vector2(2, 2)
    var mat := ShaderMaterial.new()
    var shader := Shader.new()
    shader.code = DECAL_SHADER
    mat.shader = shader
    quad.material = mat
    var mm := MultiMesh.new()
    mm.transform_format = MultiMesh.TRANSFORM_3D
    mm.use_colors = true
    mm.use_custom_data = true
    mm.mesh = quad
    node.multimesh = mm
    return node

func load_definition_file(path: String) -> Dictionary:
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return {"ok": false, "error": FileAccess.get_open_error()}
    return bridge.load_definitions_json(file.get_as_text())

func advance_tick(cutscene: bool = false) -> void:
    bridge.tick(cutscene)

func present(sim_frame: int, frozen: bool = false) -> Dictionary:
    bridge.draw_step(sim_frame, frozen)
    if particle_sheet == null or particle_sheet_height <= 0:
        return {"ok": false, "refusal": "particle sheet and measured height required"}
    _fill_particles(_blend, 0)
    _fill_particles(_add, 1)
    if floor_sprite_sheet != null:
        _fill_decals()
    return {"ok": true, "line_refusal": "kind-2 second-vertex alpha unresolved"}

func _fill_particles(node: MultiMeshInstance3D, kind: int) -> void:
    var rows: Array = bridge.particle_snapshot()
    var chosen: Array = []
    for row in rows:
        if row["kind"] == kind:
            chosen.append(row)
    var mm := node.multimesh
    mm.instance_count = chosen.size()
    var mat: ShaderMaterial = mm.mesh.material
    mat.set_shader_parameter("sheet", particle_sheet)
    mat.set_shader_parameter("sheet_height", float(particle_sheet_height))
    mat.set_shader_parameter("buffer_size", Vector2(get_viewport().size))
    for i in chosen.size():
        var p: Dictionary = chosen[i]
        mm.set_instance_transform(i, Transform3D(Basis(), p["position"]))
        mm.set_instance_color(i, p["rgba"])
        mm.set_instance_custom_data(i, Color(p["size"], p["u0"], p["v0"], 0.0))

func _fill_decals() -> void:
    var rows: Array = bridge.decal_snapshot()
    var mm := _decals.multimesh
    mm.instance_count = rows.size()
    var mat: ShaderMaterial = mm.mesh.material
    mat.set_shader_parameter("sheet", floor_sprite_sheet)
    for i in rows.size():
        var row: Dictionary = rows[i]
        var angle: float = float(row["angle"]) * TAU / 65536.0
        var size: float = row["size"]
        var basis := Basis(Vector3.UP, angle).scaled(Vector3(size, 1.0, size))
        mm.set_instance_transform(i, Transform3D(basis, row["position"] + Vector3.UP * 3.0))
        var tint: Vector3 = row["tint"]
        mm.set_instance_color(i, Color(tint.x / 255.0, tint.y / 255.0, tint.z / 255.0, 1.0))
        var rect: Vector4 = DECAL_RECTS[row["type"]]
        mm.set_instance_custom_data(i, Color(rect.x, rect.y, rect.z, rect.w))

# Call after model movement or light changes; call once for static WORLD chunks.
# Source arrays are cached, so repeated calls do not multiply previous tints.
func apply_vertex_lighting(instance: MeshInstance3D, model: bool) -> Dictionary:
    if instance.mesh == null or not instance.mesh is ArrayMesh:
        return {"ok": false, "refusal": "ArrayMesh required"}
    var key := instance.get_instance_id()
    if not _original_meshes.has(key):
        _original_meshes[key] = instance.mesh
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
            var world_pos: Vector3 = instance.global_transform * vertices[i]
            var world_normal: Vector3 = (instance.global_transform.basis * normals[i]).normalized()
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
            lit.surface_set_material(surface, copy)
        elif material == null:
            var default_material := StandardMaterial3D.new()
            default_material.vertex_color_use_as_albedo = true
            lit.surface_set_material(surface, default_material)
        else:
            return {"ok": false, "refusal": "material must consume vertex colors"}
    instance.mesh = lit
    return {"ok": true}
