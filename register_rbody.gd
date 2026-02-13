extends RigidBody2D

func _ready() -> void:
	var compute_renderer = ComputeRenderer.get_instance()
	compute_renderer.register_rigid_body(self )
