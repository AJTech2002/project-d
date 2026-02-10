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
	ClassDB::bind_method(D_METHOD("place_particle", "cell", "type"), &SandEngine::place_particle);
}

SandEngine::SandEngine() {
	set_process(true);
	set_notify_transform(true);
}

static int particle_id_counter = 0;
static int MAX_PARTICLES = 100000;

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
	cellData.resize(width * height);

	// initialize cells
	for (int i = 0; i < width * height; i++) {
		cells[i].type = 0; // empty
		cells[i].debug[0] = -1; // red
		cells[i].debug[1] = -1; // green
		cells[i].debug[2] = -1; // blue
	}

	// add 600 random sand particles
	for (int i = 0; i < 6500; i++) {
		int x = rand() % width;
		int y = rand() % height;
		if (cells[y * width + x].type != 0) continue; // skip occupied cells

		place_particle(Vector2i(x, y), 1); // type 1 = sand
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
	for (auto p : particles) {
		delete p;
	}
	particles.clear();
	particle_map.clear();
	active_particles.clear();
	// destroy cells
	cells.clear();
	cellData.clear();
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


void SandEngine::place_particle(const Vector2i &cell, uint32_t type) {
	if (cell.x < 0 || cell.y < 0 || cell.x >= width || cell.y >= height)
		return;

	if (cells[cell.y * width + cell.x].type != 0)
		return;

	Particle* p = new Particle{ cell, Vector2(cell.x, cell.y), Vector2(0, 0), type, particle_id_counter++ };
	particles.push_back(p);
	particle_map[p->id] = p; // add to map for O(1) access
	active_particles.insert(p->id); // add to active set
	cells[cell.y * width + cell.x].type = type;
	cellData[cell.y * width + cell.x].particle = p;
}


Vector2 maxVelocity(3, 9);
float restingVelocity = 0.1f;
int frame = 0;

void SandEngine::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;
	frame++;

	// shuffle active particles
	std::vector<uint32_t> shuffled(active_particles.begin(), active_particles.end());
	std::shuffle(shuffled.begin(), shuffled.end(), std::default_random_engine(frame)); // shuffle

	for (auto &pi : shuffled) {
		Particle *p = particle_map[pi];
		// cells are authoritative
		p->position = Vector2(p->cell.x, p->cell.y);

		// gravity (pixel/cell units per second^2)
		p->velocity += Vector2(0,5.81f) * (float)delta;

		// clamp per-component (better than length clamp for grid sim)
		p->velocity.x = CLAMP(p->velocity.x, -maxVelocity.x, maxVelocity.x);
		p->velocity.y = CLAMP(p->velocity.y, -maxVelocity.y, maxVelocity.y);

		Vector2i from = p->cell;

		// predict destination in grid coordinates (rounded)
		Vector2 pred = Vector2(from.x, from.y) + p->velocity; // NOTE: velocity already integrated with dt above
		Vector2i to = Vector2i((int)Math::round(pred.x), (int)Math::round(pred.y));

		// clamp target to bounds before tracing
		to.x = CLAMP(to.x, 0, width - 1);
		to.y = CLAMP(to.y, 0, height - 1);

		Vector2i new_cell = find_last_available_cell(from, to);

		// if blocked, try diagonal down-left/down-right
		if (new_cell == from) {
			int dir = p->id % 2 == 0 ? -1 : 1; // alternate left/right for different particles to reduce clumping
			dir *= (frame / 10) % 2 == 0 ? -1 : 1; // alternate direction every 10 frames to reduce clumping
			// int dir = rand() % 2 == 0 ? -1 : 1; // alternate left/right for different particles to reduce clumping
			// bool moved = false;
			Vector2i d1 = from + Vector2i(dir, 1);
			Vector2i d2 = from + Vector2i(-dir, 1);

			Vector2 oldVelocity = p->velocity;

			if (d1.x >= 0 && d1.x < width && d1.y >= 0 && d1.y < height &&
			    cells[d1.y * width + d1.x].type == 0) {
				// new_cell = d1;
				p->velocity.x = dir; // add more lateral velocity when we move diagonally to reduce clumping
				// moved = true;
				p->velocity.y = 0.5f; // damp vertical velocity when we move diagonally to reduce clumping
			} else if (d2.x >= 0 && d2.x < width && d2.y >= 0 && d2.y < height &&
				cells[d2.y * width + d2.x].type == 0) {
				// new_cell = d2;
				p->velocity.x = -dir; // add more lateral velocity when we move diagonally to reduce clumping
				p->velocity.y = 0.5f; // damp vertical velocity when we move diagonally to reduce clumping
				// moved = true;
			}
			else {
				p->velocity.x *= 0.2f; // damp velocity if stuck to reduce "correction" on next frame
				p->velocity.y *= 0.2f;
			}

			// cells[p->cell.y * width + p->cell.x].debug[0] = (uint32_t)(CLAMP(std::abs(p->velocity.x)/maxVelocity.x * 255, 0, 255)); // red for horizontal velocity
			// cells[p->cell.y * width + p->cell.x].debug[1] = (uint32_t)(CLAMP(std::abs(p->velocity.y)/maxVelocity.y * 255, 0, 255)); // green for vertical velocity
			// cells[p->cell.y * width + p->cell.x].debug[2] = 0; // blue unused

		}
		else {
			p->velocity.x *= 0.4f;
		}

		if (new_cell != from) {

			// wake up neighboring particles if we are moving
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					int nx = p->cell.x + dx;
					int ny = p->cell.y + dy;
					if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
						Particle* neighbor = cellData[ny * width + nx].particle;
						if (neighbor != nullptr) {
							active_particles.insert(neighbor->id);
						}
					}
				}
			 }
			// move to new cell
			cells[from.y * width + from.x].type = 0;
			cells[from.y * width + from.x].debug[0] = -1;
			cellData[from.y * width + from.x].particle = nullptr;

			p->cell = new_cell;
			p->position = Vector2(p->cell.x, p->cell.y);
			cells[p->cell.y * width + p->cell.x].type = p->type;
			cellData[p->cell.y * width + p->cell.x].particle = p;
			p->velocity.x *= 0.95f;
		}

		// set debug color based on velocity
		// cells[p->cell.y * width + p->cell.x].debug[0] = (uint32_t)(CLAMP(std::abs(p->velocity.x)/maxVelocity.x * 255, 0, 255)); // red for horizontal velocity
		// cells[p->cell.y * width + p->cell.x].debug[1] = (uint32_t)(CLAMP(std::abs(p->velocity.y)/maxVelocity.y * 255, 0, 255)); // green for vertical velocity
		// cells[p->cell.y * width + p->cell.x].debug[2] = 0; // blue unused
		
		

		// check velocity below threshold to consider "resting"
		if (p->velocity.length() < restingVelocity) {
			p->velocity = Vector2(0, 0);
			cells[p->cell.y * width + p->cell.x].debug[0] = 255; // red for horizontal velocity
			cells[p->cell.y * width + p->cell.x].debug[1] = 0; // green for vertical velocity
			cells[p->cell.y * width + p->cell.x].debug[2] = 0; // blue unused
			active_particles.erase(pi); // remove from active set
		}
		else {
			cells[p->cell.y * width + p->cell.x].debug[0] = 0; // red for horizontal velocity
			cells[p->cell.y * width + p->cell.x].debug[1] = 255; // green for vertical velocity
			cells[p->cell.y * width + p->cell.x].debug[2] = 0; // blue unused

			
		}
	}

	update_ssbo();
}
