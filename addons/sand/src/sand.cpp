#include "sand.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <algorithm>
#include <random>

// rand
#include <cstdlib>

using namespace godot;

void SandEngine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_ssbo_rid"), &SandEngine::get_ssbo_rid);
	ClassDB::bind_method(D_METHOD("get_grid_width"), &SandEngine::get_grid_width);
	ClassDB::bind_method(D_METHOD("get_grid_height"), &SandEngine::get_grid_height);
}

SandEngine::SandEngine() {
	set_process(true);
	set_notify_transform(true);
}

void SandEngine::_ready() {

	// Check if in game or editor
	if (!Engine::get_singleton()->is_editor_hint()) {
		UtilityFunctions::print("Running in game");
	}
	else {
		UtilityFunctions::print("Running in editor");
		return;
	}

	cells.resize(width * height);
	// add 600 random sand particles
	for (int i = 0; i < 1500; i++) {
		int x = rand() % width;
		int y = rand() % height;
		particles.push_back(Particle{ Vector2i(x, y), Vector2(x, y), Vector2(0, 0), 1, i });
		cells[y * width + x].type = particles.back().type;
	}

	create_ssbo();

	update_ssbo();
}

void SandEngine::_draw() {
	Color color = Color(1, 0, 0, 1); // red
	Vector2 size = Vector2(width, height);
	
	draw_rect(Rect2(Vector2(0,0), size), color, false, 1.0);
}

SandEngine::~SandEngine() {
	// destroy particles
	particles.clear();
	// destroy cells
	cells.clear();
}


void SandEngine::create_ssbo() {
	size_t byte_size = cells.size() * sizeof(Cell);
	PackedByteArray init;
	init.resize(byte_size);
	std::memset(init.ptrw(), 0, byte_size);

	if (RenderingServer::get_singleton() == nullptr) {
		UtilityFunctions::print("RenderingServer singleton is null!");
		return;
	}

	if (RenderingServer::get_singleton()->get_rendering_device() == nullptr) {
		UtilityFunctions::print("RenderingDevice is null!");
		return;
	}

	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	ssbo_rid = rd->storage_buffer_create(init.size(), init);
}

void SandEngine::update_ssbo() {
	size_t byte_size = cells.size() * sizeof(Cell);
	PackedByteArray data;
	data.resize(byte_size);
	std::memcpy(data.ptrw(), cells.data(), byte_size);
	
	if (RenderingServer::get_singleton() == nullptr) {
		UtilityFunctions::print("RenderingServer singleton is null!");
		return;
	}

	if (RenderingServer::get_singleton()->get_rendering_device() == nullptr) {
		UtilityFunctions::print("RenderingDevice is null!");
		return;
	}

	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	rd->buffer_update(ssbo_rid, 0, data.size(), data);
}

RID SandEngine::get_ssbo_rid() const {
	return ssbo_rid;
}

Vector2i SandEngine::find_last_available_cell(
	const Vector2i &from,
	const Vector2i &to
) const {
	int x0 = from.x;
	int y0 = from.y;
	int x1 = to.x;
	int y1 = to.y;

	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;

	int dy = -abs(y1 - y0);
	int sy = y0 < y1 ? 1 : -1;

	int err = dx + dy;

	Vector2i last = from;
	bool first = true;

	while (true) {
		// skip starting cell (occupied by ourselves)
		if (!first) {
			if (x0 < 0 || y0 < 0 || x0 >= width || y0 >= height)
				break;

			if (cells[y0 * width + x0].type != 0)
				break;

			last = Vector2i(x0, y0);
		}

		first = false;

		if (x0 == x1 && y0 == y1)
			break;

		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}

	return last;
}


Vector2 maxVelocity(100, 100);
void SandEngine::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;

	std::shuffle(particles.begin(), particles.end(), std::mt19937(std::random_device()()));

	for (auto &p : particles) {
		// cells are authoritative
		p.position = Vector2(p.cell.x, p.cell.y);

		// gravity (pixel/cell units per second^2)
		p.velocity += Vector2(0, 9.81f) * (float)delta;

		// clamp per-component (better than length clamp for grid sim)
		p.velocity.x = CLAMP(p.velocity.x, -maxVelocity.x, maxVelocity.x);
		p.velocity.y = CLAMP(p.velocity.y, -maxVelocity.y, maxVelocity.y);

		Vector2i from = p.cell;

		// predict destination in grid coordinates (rounded)
		Vector2 pred = Vector2(from.x, from.y) + p.velocity; // NOTE: velocity already integrated with dt above
		Vector2i to = Vector2i((int)Math::round(pred.x), (int)Math::round(pred.y));

		// clamp target to bounds before tracing
		to.x = CLAMP(to.x, 0, width - 1);
		to.y = CLAMP(to.y, 0, height - 1);

		Vector2i new_cell = find_last_available_cell(from, to);

		// if blocked, try diagonal down-left/down-right
		if (new_cell == from) {
			int dir = (rand() & 1) ? 1 : -1;

			Vector2i d1(from.x + dir, from.y + 1);
			Vector2i d2(from.x - dir, from.y + 1);

			bool moved = false;

			if (d1.x >= 0 && d1.x < width && d1.y >= 0 && d1.y < height &&
			    cells[d1.y * width + d1.x].type == 0) {
				new_cell = d1;
				moved = true;
			} else if (d2.x >= 0 && d2.x < width && d2.y >= 0 && d2.y < height &&
				cells[d2.y * width + d2.x].type == 0) {
				new_cell = d2;
				moved = true;
			}

			if (!moved) {
				// truly stuck: damp velocity hard so it doesn't "correct" next frame
				p.velocity *= 0.2f;
				continue;
			}
		}

		// move to new cell
		cells[from.y * width + from.x].type = 0;
		p.cell = new_cell;
		p.position = Vector2(p.cell.x, p.cell.y);
		cells[p.cell.y * width + p.cell.x].type = p.type;

		// reduce lateral drift over time
		p.velocity.x *= 0.95f;
	}

	update_ssbo();
}
