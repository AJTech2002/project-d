#pragma once

#include <vector>
#include <map>
#include <set>
#include "particles/particle.h"
#include <godot_cpp/classes/node2d.hpp>
#include <functional>
#include <memory>
#include <godot_cpp/classes/rigid_body2d.hpp>
namespace godot
{

  // Static Opts
  static int MAX_PARTICLES = 100000;

  static int frame = 0;

  enum ParticleDebugMode
  {
    NONE = 0,
    VELOCITY = 1,
    ACTIVE = 2,
  };

  struct Cell
  {
    uint32_t type;
    // debug color
    uint32_t debug[3];
  };

  struct CellInfo
  {
    Particle *particle;
  };

  static_assert(sizeof(Cell) == 16, "Cell struct must be 16 bytes in size");

  class SandEngine : public Node2D
  {
    GDCLASS(SandEngine, Node2D)

  private:
    RID ssbo_rid;
    int height = 300;
    int width = 800;

    ParticleDebugMode debugMode = ParticleDebugMode::VELOCITY;

    std::vector<Cell> cells;
    std::vector<CellInfo> cellData;
    std::vector<RigidBody2D *> rigidBodies;
    std::vector<int> rigidyBodyOccupancy;
    std::map<uint32_t, Particle *> particle_map; // map from particle id -> particle
    std::set<uint32_t> active_particles;
    void create_ssbo();
    void update_ssbo();

  protected:
    static void _bind_methods();

  public:
    SandEngine();
    ~SandEngine();

    Vector2i find_last_available_cell(const Vector2i &from, const Vector2i &to) const;
    void for_each_along_line(const Vector2i &from, const Vector2i &to, const std::function<bool(const int &, const Vector2i &)> &callback) const;

    void _physics_process(double delta) override;
    void _draw() override;
    void _ready() override;
    RID get_ssbo_rid() const;

    int get_grid_width() const { return width; }
    int get_grid_height() const { return height; }

    int get_debug_mode()
    {
      return debugMode;
    }

    void set_debug_mode(int mode)
    {
      debugMode = static_cast<ParticleDebugMode>(mode);
    }

    void spawn_particle(const Vector2i &cell, uint32_t type);

    void register_rigid_body(RigidBody2D *body);

    int gridIndex(const int x, const int y)
    {
      return y * width + x;
    }

    CellInfo *get_cell_info(const int x, const int y)
    {
      if (x < 0 || y < 0 || x >= width || y >= height)
        return nullptr;
      return &cellData[gridIndex(x, y)];
    }

    Cell *get_cell(const int x, const int y)
    {
      if (x < 0 || y < 0 || x >= width || y >= height)
        return nullptr;
      return &cells[gridIndex(x, y)];
    }

    Particle *get_particle(const int x, const int y)
    {
      if (x < 0 || y < 0 || x >= width || y >= height)
        return nullptr;
      return get_cell_info(x, y)->particle;
    }

    void clear_cell(const int x, const int y)
    {
      Cell *oldCell = get_cell(x, y);

      if (oldCell != nullptr)
      {
        CellInfo *oldCellInfo = get_cell_info(x, y);
        oldCell->debug[0] = -1;
        oldCell->debug[1] = -1;
        oldCell->debug[2] = -1;
        oldCell->type = 0;
        oldCellInfo->particle = nullptr;
      }
    }

    void set_cell(const int x, const int y, Particle *particle)
    {
      Cell *newCell = get_cell(x, y);

      if (newCell != nullptr)
      {
        CellInfo *newCellInfo = get_cell_info(x, y);

        newCell->debug[0] = -1;
        newCell->debug[1] = -1;
        newCell->debug[2] = -1;

        newCell->type = particle->type;
        newCellInfo->particle = particle;
      }
    }

    void add_particle(const int x, const int y, Particle *particle)
    {
      particle_map[particle->id] = particle;
      active_particles.insert(particle->id);
      set_cell(x, y, particle);
    }

    void delete_particle(Particle *particle)
    {
      particle_map.erase(particle->id);
      active_particles.erase(particle->id);
      clear_cell(particle->cell.x, particle->cell.y);
      delete particle;
    }

    void set_active(const bool active, Particle *particle)
    {
      if (active)
        active_particles.insert(particle->id);
      else
        active_particles.erase(particle->id);
    }
  };

} // namespace godot