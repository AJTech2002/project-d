extends CharacterBody2D

@export var speed := 120.0
@export var jump_velocity := -300.0
@export var gravity := 900.0

func _physics_process(delta):
	if not is_on_floor():
		velocity.y += gravity * delta

	var direction := 0
	if Input.is_key_pressed(KEY_A):
		direction -= 1
	if Input.is_key_pressed(KEY_D):
		direction += 1

	velocity.x = direction * speed

	if Input.is_key_pressed(KEY_W) and is_on_floor():
		velocity.y = jump_velocity

	move_and_slide()
