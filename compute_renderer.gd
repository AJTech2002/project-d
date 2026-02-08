extends Node

@export var overlay_rect_path: NodePath

var rd: RenderingDevice

var tex_rid: RID
var tex_view_rid: RID

var shader_rid: RID
var pipeline_rid: RID
var uniform_set_rid: RID


var out_tex: Texture2DRD

var W := 124
var H := 124
var pixelDensity := 1

func _ready():
	rd = RenderingServer.get_rendering_device()
	
	
	# get screen size and adjust W/H if needed
	var screen_size = get_viewport().get_visible_rect().size
	W = screen_size.x * pixelDensity
	H = screen_size.y * pixelDensity

	# --- Create storage+sampled texture ---
	var fmt := RDTextureFormat.new()
	fmt.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
	fmt.width = W
	fmt.height = H
	fmt.usage_bits = (
		RenderingDevice.TEXTURE_USAGE_STORAGE_BIT |
		RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT |
		RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT
	)


	var view := RDTextureView.new()
	tex_rid = rd.texture_create(fmt, view)

	# --- Compute shader + pipeline ---
	var shader_file: RDShaderFile = load("res://overlay_compute.glsl")
	var spirv := shader_file.get_spirv()
	shader_rid = rd.shader_create_from_spirv(spirv)
	pipeline_rid = rd.compute_pipeline_create(shader_rid)

	

	# --- Uniform set binding the storage image ---
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	u.binding = 0
	u.add_id(tex_rid)

	uniform_set_rid = rd.uniform_set_create([u], shader_rid, 0)

	# --- Wrap RID for UI display ---
	out_tex = Texture2DRD.new()
	out_tex.texture_rd_rid = tex_rid

	var tr := get_node(overlay_rect_path) as TextureRect
	tr.texture = out_tex

func _process(_dt):
	# Dispatch compute every frame
	var cl := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(cl, pipeline_rid)
	rd.compute_list_bind_uniform_set(cl, uniform_set_rid, 0)

	# Match these to your shader's local size
	var gx := int(ceil(float(W) / 8.0))
	var gy := int(ceil(float(H) / 8.0))
	rd.compute_list_dispatch(cl, gx, gy, 1)
	rd.compute_list_end()

	# rd.submit()
