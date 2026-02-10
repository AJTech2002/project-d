#pragma once

#include <godot_cpp/classes/sprite2d.hpp>
#include <vector>
#include <map>
#include <set>

namespace godot {

struct Particle; // Forward declaration

struct Cell {
	uint32_t type;
	// debug color
	uint32_t debug[3];
};

struct CellInfo {
	Particle* particle;
};

struct Particle {
	Vector2i cell;
	Vector2 position;
	Vector2 velocity;
	uint32_t type;

	int id;
};

static_assert(sizeof(Cell) == 16, "Cell struct must be 16 bytes in size");

class SandEngine : public Node2D {
	GDCLASS(SandEngine, Node2D)

private:
	RID ssbo_rid;
	int height = 300;
	int width = 300;
	std::vector<Cell> cells;
	std::vector<CellInfo> cellData;
	std::vector<Particle*> particles;
	std::map<uint32_t, Particle*> particle_map; // map from cell index to particle pointer for O(1) access
	std::set<uint32_t> active_particles;
	void create_ssbo();
	void update_ssbo();

protected:
	static void _bind_methods();

public:
	SandEngine();
	~SandEngine();

	Vector2i find_last_available_cell (const Vector2i &from, const Vector2i &to) const;
	Vector2i next_available_cell_along_velocity(const Vector2i &from, const Vector2 &velocity, float dt) const;
	
	void _physics_process(double delta) override;
	// void _process(double delta) override;
	void _draw() override;
	void _ready() override;
	RID get_ssbo_rid() const;

	int get_grid_width() const { return width; }
	int get_grid_height() const { return height; }

	void place_particle(const Vector2i &cell, uint32_t type);
};

} // namespace godot