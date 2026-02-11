#include "particle.h"
#include "../engine.h"

namespace godot {

    

    int32_t Particle::get_cell_index() const {
        return cell.y * engine->get_grid_width() + cell.x;
    }

    Particle::Particle(
        SandEngine* engine,
        const Vector2i& cell,
        const Vector2& position,
        const Vector2& velocity,
        uint32_t type
    ) : engine(engine), cell(cell), position(position), velocity(velocity), type(type) {
        id = PARTICLE_ID_COUNTER++;
    }

    void Particle::set_cell(const int x, const int y, bool clear_old_cell) {
        
        if (clear_old_cell) {
            engine->clear_cell(cell.x, cell.y); // clear old cell
        }
        
        int width = engine->get_grid_width();
        int height = engine->get_grid_height();
        
        // Wake up neighbours before moving
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = cell.x + dx;
                int ny = cell.y + dy;
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    Particle* neighbor = engine->get_particle(nx, ny);
                    if (neighbor != nullptr && !neighbor->active) {
                        neighbor->set_active(true);
                    }
                }
            }
        }

        cell.x = x;
        cell.y = y;
        position.x = x;
        position.y = y;

        engine->set_cell(x, y, this);
        
    }

    void Particle::set_active(const bool active) {
        this->active = active;
        engine->set_active(active, this);
    }

    void Particle::swap(Particle* other) {
        Vector2i tempCell = cell;
        Vector2 tempPosition = position;

        // Do not clear old cell since we will be swapping into it
        set_cell(other->cell.x, other->cell.y, false);
        other->set_cell(tempCell.x, tempCell.y, false);
    }

    void Particle::update_debug() {
        switch (engine->get_debug_mode()) {
            case 1: // velocity debug
                debugColor.x = (int)(CLAMP(std::abs(velocity.x)/10.0 * 255, 0, 255)); // red for horizontal velocity
                debugColor.y = (int)(CLAMP(std::abs(velocity.y)/10.0 * 255, 0, 255)); // green for vertical velocity
                debugColor.z = 0; // blue unused
                break;
            case 2:
                debugColor.x = active ? 0 : 255; // red for in active state
                debugColor.y = active ? 255 : 0; // green for active state
                debugColor.z = 0;
                break;
            default:
                debugColor = Vector3i(-1, -1, -1);
                break;
        }

        if (engine->get_cell(cell.x, cell.y) != nullptr) {
            engine->get_cell(cell.x, cell.y)->debug[0] = debugColor.x;
            engine->get_cell(cell.x, cell.y)->debug[1] = debugColor.y;
            engine->get_cell(cell.x, cell.y)->debug[2] = debugColor.z;
        }
    }
}