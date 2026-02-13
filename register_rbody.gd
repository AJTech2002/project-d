extends RigidBody2D

var dragging := false
var drag_offset := Vector2.ZERO

func _ready() -> void:
	var compute_renderer = ComputeRenderer.get_instance()
	compute_renderer.register_rigid_body(self)
	input_pickable = true

func _input_event(viewport, event, shape_idx):
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
		start_drag()

func _input(event):
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT and not event.pressed:
		stop_drag()

func start_drag():
	dragging = true
	drag_offset = global_position - get_global_mouse_position()
	linear_velocity = Vector2.ZERO
	angular_velocity = 0.0
	# claer forces
	set_axis_velocity(Vector2.ZERO)
	freeze = true   # Godot 4 (prevents physics)

func stop_drag():
	if dragging:
		dragging = false
		freeze = false
	
	await get_tree().process_frame
	linear_velocity = Vector2.ZERO
	angular_velocity = 0.0

func _physics_process(delta):
	if dragging:
		linear_velocity = Vector2.ZERO
		angular_velocity = 0.0
		global_position = get_global_mouse_position() + drag_offset
	
