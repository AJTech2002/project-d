#include "sand.h"
#include "../engine.h"
#include <godot_cpp/variant/vector2.hpp>
#include "water.h"

static const godot::Vector2 MAX_VELOCITY(3, 9);

void godot::Sand::update(double delta)
{
    // Gravity
    this->velocity += Vector2(0, 5.81f) * (float)delta;

    this->velocity.x = CLAMP(this->velocity.x, -MAX_VELOCITY.x, MAX_VELOCITY.x);
    this->velocity.y = CLAMP(this->velocity.y, -MAX_VELOCITY.y, MAX_VELOCITY.y);

    Vector2i from = this->cell;

    Vector2 pred = Vector2(from.x, from.y) + this->velocity;
    Vector2i to = Vector2i((int)Math::round(pred.x), (int)Math::round(pred.y));

    int width = engine->get_grid_width();
    int height = engine->get_grid_height();

    to.x = CLAMP(to.x, 0, width - 1);
    to.y = CLAMP(to.y, 0, height - 1);

    Vector2i new_cell = from;
	engine->for_each_along_line(from, to, [&](const int &i, const Vector2i &cell) {
		if (i == 0) return false; // skip the starting cell
		if (i > 100) {
			print_line("Warning: find_last_available_cell exceeded 100 iterations, possible infinite loop. Returning last available cell found.");
			return true; // limit to 100 iterations to prevent infinite loops
		}
        
        // Support swapping with liquid
		if (engine->get_cell(cell.x, cell.y)->type != 0 && engine->get_cell(cell.x, cell.y)->type != Water::TYPE) {
			return true; // stop iterating
		}
		new_cell = cell;
		return false; // continue iterating
	});

    // if blocked, try diagonal down-left/down-right
    if (new_cell == from)
    {
        int dir = this->id % 2 == 0 ? -1 : 1;
        dir *= (frame / 10) % 2 == 0 ? -1 : 1; // alternate direction every 10 frames to reduce clumping
   
        Vector2i d1 = from + Vector2i(dir, 1);
        Vector2i d2 = from + Vector2i(-dir, 1);

        Vector2 oldVelocity = this->velocity;

        if (d1.x >= 0 && d1.x < width && d1.y >= 0 && d1.y < height &&
            (engine->get_cell(d1.x, d1.y)->type == 0 || engine->get_cell(d1.x, d1.y)->type == Water::TYPE))
        {
            this->velocity.x = dir;
            this->velocity.y = 0.5f;
        }
        else if (d2.x >= 0 && d2.x < width && d2.y >= 0 && d2.y < height &&
                 (engine->get_cell(d2.x, d2.y)->type == 0 || engine->get_cell(d2.x, d2.y)->type == Water::TYPE))
        {
            this->velocity.x = -dir;
            this->velocity.y = 0.5f;
        }
        else
        {
            this->velocity.x *= 0.2f;
            this->velocity.y *= 0.2f;
        }
    }
    else
    {
        this->velocity.x *= 0.4f;
    }

    if (new_cell != from)
    {
        // Swap with liquid if needed
        if (engine->get_cell(new_cell.x, new_cell.y)->type == Water::TYPE) {
            engine->get_particle(new_cell.x, new_cell.y)->set_active(true);
            swap(engine->get_particle(new_cell.x, new_cell.y));
        }

        this->set_cell(new_cell.x, new_cell.y);
        this->velocity.x *= 0.95f;
    }

    if (this->velocity.length() < RESTING_VELOCITY)
    {
        this->velocity = Vector2(0, 0);
        this->set_active(false);
    }
}