class_name ComputeRenderer

extends Node

@export var overlay_rect_path: NodePath
@export var sandEngine: SandEngine
@export var camera: Camera2D
@export var brushType = 1

# header category
@export_group("Engine")
@export var debugOption: int = 0

static var instance: ComputeRenderer

static func get_instance():
	return instance

var rd: RenderingDevice

var tex_rid: RID
var tex_view_rid: RID

var shader_rid: RID
var pipeline_rid: RID
var uniform_set_rid: RID
var out_tex: Texture2DRD

var param_buffer: RID
var bodies: Array[RigidBody2D] = []

var W := 124
var H := 124
var pixelDensity := 1

var initialized := false

func register_rigid_body(body: RigidBody2D):
	bodies.append(body)

func _ready():
	instance = self

	await get_tree().process_frame
	await get_tree().process_frame

	# Register rigidbodies that will affect the simulation
	for b in bodies:
		sandEngine.register_rbody(b)

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
	if spirv.compile_error_compute != "":
		push_error("=== COMPUTE SHADER ERROR ===\n" + spirv.compile_error_compute)
		return

	
	shader_rid = rd.shader_create_from_spirv(spirv)
	shader_rid = rd.shader_create_from_spirv(spirv)
	pipeline_rid = rd.compute_pipeline_create(shader_rid)

	
	# --- Uniform set binding the storage image ---
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	u.binding = 0
	u.add_id(tex_rid)


	# 300 x 300 is grid width, W, H is screen size
	var param_buf := PackedInt32Array([W, H, sandEngine.get_grid_width(), sandEngine.get_grid_height(), int(camera.position.x), int(camera.position.y), debugOption, 0]).to_byte_array()

	var p := rd.uniform_buffer_create(param_buf.size(), param_buf)
	
	var param_ub := RDUniform.new()
	param_ub.uniform_type = RenderingDevice.UNIFORM_TYPE_UNIFORM_BUFFER
	param_ub.binding = 1
	param_ub.add_id(p)

	param_buffer = p

	var storage_buf := RDUniform.new()
	storage_buf.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	storage_buf.binding = 2
	storage_buf.add_id(sandEngine.get_ssbo_rid())

	uniform_set_rid = rd.uniform_set_create([u, param_ub, storage_buf], shader_rid, 0)


	# --- Wrap RID for UI display ---
	out_tex = Texture2DRD.new()
	out_tex.texture_rd_rid = tex_rid

	var tr := get_node(overlay_rect_path) as TextureRect
	tr.texture = out_tex

	initialized = true

func _process(_dt):
	if not initialized:
		return

	sandEngine.set_debug_mode(debugOption)

	# Update uniform buffer with camera position
	var top_left := camera.get_screen_center_position() - Vector2(W, H) * 0.5
	var param_buf := PackedInt32Array([W, H, sandEngine.get_grid_width(), sandEngine.get_grid_height(), int(floor(top_left.x)), int(floor(top_left.y)), debugOption, 0]).to_byte_array()
	rd.buffer_update(param_buffer, 0, param_buf.size(), param_buf)
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

	if Input.is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_MIDDLE):
		brushType += 1
		if brushType > 2:
			brushType = 1
		

	# Check for mouse down, cast to world position, and place particle
	if Input.is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
		var world_pos = camera.get_global_mouse_position()
		var cell_x = int(floor(world_pos.x))
		var cell_y = int(floor(world_pos.y))

		var placement_radius = 4
		for x in range(cell_x - placement_radius, cell_x + placement_radius + 1):
			for y in range(cell_y - placement_radius, cell_y + placement_radius + 1):
				sandEngine.place_particle(Vector2i(x, y), brushType) # type 1 is sand, change if you want to place different particle types
