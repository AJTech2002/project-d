#include "engine.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/shape2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <algorithm>
#include <random>

// rand
#include <cstdlib>

// Particles
#include "particles/sand.h"
#include "particles/water.h"

using namespace godot;

void SandEngine::_bind_methods()
{
  ClassDB::bind_method(D_METHOD("get_ssbo_rid"), &SandEngine::get_ssbo_rid);
  ClassDB::bind_method(D_METHOD("get_grid_width"), &SandEngine::get_grid_width);
  ClassDB::bind_method(D_METHOD("get_grid_height"), &SandEngine::get_grid_height);
  ClassDB::bind_method(D_METHOD("place_particle", "cell", "type"), &SandEngine::spawn_particle);
  ClassDB::bind_method(D_METHOD("set_debug_mode", "mode"), &SandEngine::set_debug_mode);
  ClassDB::bind_method(D_METHOD("get_debug_mode"), &SandEngine::get_debug_mode);
  ClassDB::bind_method(D_METHOD("register_rbody"), &SandEngine::register_rigid_body);
}

void SandEngine::register_rigid_body(RigidBody2D *rBody)
{
  rigidBodies.push_back(rBody);
}

SandEngine::SandEngine()
{
  set_process(true);
  set_notify_transform(true);
}

void SandEngine::_ready()
{

  // Check if in game or editor
  if (!Engine::get_singleton()->is_editor_hint())
  {
    UtilityFunctions::print("Running in game");
  }
  else
  {
    UtilityFunctions::print("Running in editor");
    return;
  }

  cells.resize(width * height);
  cellData.resize(width * height);
  rigidyBodyOccupancy.resize(width * height);

  // initialize cells
  for (int i = 0; i < width * height; i++)
  {
    cells[i].type = 0;      // empty
    cells[i].debug[0] = -1; // red
    cells[i].debug[1] = -1; // green
    cells[i].debug[2] = -1; // blue
  }

  // add 600 random sand particles
  for (int i = 0; i < 0; i++)
  {
    int x = rand() % width;
    int y = rand() % height;
    if (cells[y * width + x].type != 0)
      continue; // skip occupied cells

    spawn_particle(Vector2i(x, y), 1); // type 1 = sand
  }

  create_ssbo();

  update_ssbo();
}

void SandEngine::_draw()
{
  Color color = Color(1, 0, 0, 1); // red
  Vector2 size = Vector2(width, height);

  draw_rect(Rect2(Vector2(0, 0), size), color, false, 1.0);
}

SandEngine::~SandEngine()
{
  // destroy particles
  for (auto p : particle_map)
  {
    delete p.second;
  }
  particle_map.clear();
  active_particles.clear();
  // destroy cells
  cells.clear();
  cellData.clear();
}

void SandEngine::create_ssbo()
{
  size_t byte_size = cells.size() * sizeof(Cell);
  PackedByteArray init;
  init.resize(byte_size);
  std::memset(init.ptrw(), 0, byte_size);

  if (RenderingServer::get_singleton() == nullptr)
  {
    UtilityFunctions::print("RenderingServer singleton is null!");
    return;
  }

  if (RenderingServer::get_singleton()->get_rendering_device() == nullptr)
  {
    UtilityFunctions::print("RenderingDevice is null!");
    return;
  }

  RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
  ssbo_rid = rd->storage_buffer_create(init.size(), init);
}

void SandEngine::update_ssbo()
{
  size_t byte_size = cells.size() * sizeof(Cell);
  PackedByteArray data;
  data.resize(byte_size);
  std::memcpy(data.ptrw(), cells.data(), byte_size);

  if (RenderingServer::get_singleton() == nullptr)
  {
    UtilityFunctions::print("RenderingServer singleton is null!");
    return;
  }

  if (RenderingServer::get_singleton()->get_rendering_device() == nullptr)
  {
    UtilityFunctions::print("RenderingDevice is null!");
    return;
  }

  RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
  rd->buffer_update(ssbo_rid, 0, data.size(), data);
}

RID SandEngine::get_ssbo_rid() const
{
  return ssbo_rid;
}

Vector2i SandEngine::find_last_available_cell(
    const Vector2i &from,
    const Vector2i &to) const
{
  Vector2i last_available = from;
  for_each_along_line(from, to, [&](const int &i, const Vector2i &cell)
                      {
                        if (i == 0)
                          return false; // skip the starting cell
                        if (i > 100)
                        {
                          print_line("Warning: find_last_available_cell exceeded 100 iterations, possible infinite loop. Returning last available cell found.");
                          return true; // limit to 100 iterations to prevent infinite loops
                        }
                        if (cells[cell.y * width + cell.x].type != 0)
                        {
                          return true; // stop iterating
                        }
                        last_available = cell;
                        return false; // continue iterating
                      });

  return last_available;
}

void SandEngine::for_each_along_line(
    const Vector2i &from,
    const Vector2i &to,
    const std::function<bool(const int &, const Vector2i &)> &callback) const
{
  int x0 = from.x;
  int y0 = from.y;
  int x1 = to.x;
  int y1 = to.y;

  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;

  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;

  int err = dx + dy;
  int i = 0;

  while (true)
  {
    if (x0 < 0 || y0 < 0 || x0 >= width || y0 >= height)
      break;

    if (callback(i, Vector2i(x0, y0)))
      break;

    if (x0 == x1 && y0 == y1)
      break;

    i++;
    int e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void SandEngine::spawn_particle(const Vector2i &cell, uint32_t type)
{
  if (cell.x < 0 || cell.y < 0 || cell.x >= width || cell.y >= height)
    return;

  if (cells[cell.y * width + cell.x].type != 0)
    return;

  if (type == Sand::TYPE)
  {
    Sand *p = new Sand{this, cell, Vector2(cell.x, cell.y), Vector2(0, 0)};
    add_particle(cell.x, cell.y, p);
  }
  else if (type == Water::TYPE)
  {
    Water *p = new Water{this, cell, Vector2(cell.x, cell.y), Vector2(0, 0)};
    add_particle(cell.x, cell.y, p);
  }
}

void SandEngine::_physics_process(double delta)
{
  if (Engine::get_singleton()->is_editor_hint())
    return;
  frame++;

  // shuffle active particles
  // std::vector<uint32_t> shuffled(active_particles.begin(), active_particles.end());
  // std::shuffle(shuffled.begin(), shuffled.end(), std::default_random_engine(frame)); // shuffle

  // clear rigidbodies
  memset(rigidBodies.data(), 0, rigidBodies.size() * sizeof(int));

  for (int i = 0; i < rigidBodies.size(); i++)
  {
    RigidBody2D *rb = rigidBodies[i];
    CollisionShape2D *shape = static_cast<CollisionShape2D *>(rb->get_child(0));
    Ref<Shape2D> shape2D = shape->get_shape();
    // shape 2d is a rectangle, get bounds
    if (shape2D->get_class() == "RectangleShape2D")
    {
      Vector2 extents = static_cast<RectangleShape2D *>(shape2D.ptr())->get_size();
      int width = static_cast<int>(extents.x);
      int height = static_cast<int>(extents.y);

      Transform2D global_transform = rb->get_global_transform();
      for (int x = -width; x <= width; x++)
      {
        for (int y = -height; y <= height; y++)
        {
          Vector2 point = global_transform.xform(Vector2(x, y));
          int grid_x = static_cast<int>(point.x);
          int grid_y = static_cast<int>(point.y);
          if (grid_x < 0 || grid_y < 0 || grid_x >= this->width || grid_y >= this->height)
            continue;
          rigidyBodyOccupancy[grid_y * this->width + grid_x] = i;
          get_cell(grid_x, grid_y)->debug[0] = 255; // mark rigidbody occupied cells as red for debugging
          get_cell(grid_x, grid_y)->debug[1] = 0;
          get_cell(grid_x, grid_y)->debug[2] = 0;
        }
      }
    }
  }

  std::vector<uint32_t> to_update(
      active_particles.begin(),
      active_particles.end());

  for (uint32_t pi : to_update)
  {
    Particle *p = particle_map[pi];
    p->update(delta);
  }

  if (debugMode != ParticleDebugMode::NONE)
  {
    for (auto &pi : particle_map)
    {
      Particle *p = pi.second;
      p->update_debug();
    }
  }

  update_ssbo();
}
