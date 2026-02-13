#include "water.h"
#include "../engine.h"
#include <godot_cpp/variant/vector2.hpp>
#include "sand.h"

static const godot::Vector2 MAX_VELOCITY(9, 9);
float FLOW_VISCOSITY = 15.0f;

void godot::Water::update(double delta)
{
    // Gravity
    // this->velocity += Vector2(0, 5.81f) * (float)delta;
    Vector2i from = this->cell;

    int width = engine->get_grid_width();
    int height = engine->get_grid_height();

    RigidBody2D *withinRigidbody = engine->get_rigid_body_at(from.x, from.y);

    Vector2i new_cell = from;

    if (withinRigidbody != nullptr)
    {

        Vector2i searchTo = from + Vector2i(0, -50);
        engine->for_each_along_line(from, searchTo, [&](const int &i, const Vector2i &cell)
                                    {
                                        if (i == 0)
                                            return false; // skip the starting cell
                                        if (i > 100)
                                        {
                                            print_line("Warning: find_last_available_cell for rigidbody exceeded 100 iterations, possible infinite loop. Returning last available cell found.");
                                            return true; // limit to 100 iterations to prevent infinite loops
                                        }

                                        if (engine->get_rigid_body_at(cell.x, cell.y) == nullptr)
                                        {
                                            new_cell = cell;
                                            return true; // stop iterating once we find a non-rigidbody cell
                                        }
                                        return false; // continue iterating
                                    });

        velocity.y = -2.0f; // give an initial upward velocity to help escape the rigidbody
        // x away from center of rigidbody to help escape horizontally as well
        Vector2 body_center = withinRigidbody->get_global_transform().get_origin();
        if (from.x < body_center.x)
            velocity.x = -2.0f;
        else
            velocity.x = 2.0f;
        
        // apply force to rbody from this position -> body center
        Vector2 force_dir = (body_center - Vector2(from.x, from.y)).normalized();
        Vector2 relPos = withinRigidbody->get_global_transform().xform_inv(Vector2(from.x, from.y));
        withinRigidbody->apply_force(force_dir * 20.0f, relPos);
        // ensure max vel
        withinRigidbody->set_linear_velocity(withinRigidbody->get_linear_velocity().clamp(Vector2(-10, -10), Vector2(10, 10)));
        withinRigidbody->set_angular_velocity(CLAMP(withinRigidbody->get_angular_velocity(), -2.0f, 2.0f));
    }
    else
    {
        // check down cell
        Vector2i down = from + Vector2i(0, 1);
        if (down.x >= 0 && down.x < width && down.y >= 0 && down.y < height &&
            engine->get_cell(down.x, down.y)->type == 0)
        {
            this->velocity.y = Math::lerp(this->velocity.y, 5.0f, float(delta) * 3.0f);
        }
        // if blocked, try diagonal down-left/down-right
        else
        {
            int dir = this->id % 2 == 0 ? -1 : 1;
            // dir *= (frame / 10) % 2 == 0 ? -1 : 1; // alternate direction every 10 frames to reduce clumping

            Vector2i d1 = from + Vector2i(dir, 1);
            Vector2i d2 = from + Vector2i(-dir, 1);

            Vector2 oldVelocity = this->velocity;

            if (d1.x >= 0 && d1.x < width && d1.y >= 0 && d1.y < height &&
                engine->get_cell(d1.x, d1.y)->type == 0)
            {
                this->velocity.x = Math::lerp(this->velocity.x, dir, float(delta) * FLOW_VISCOSITY);
                this->velocity.y = Math::lerp(this->velocity.y, 0.5f, float(delta) * 3.0f);
                // this->velocity.y *= 0.95;
                // this->velocity.y = CLAMP(this->velocity.y, 0.5, MAX_VELOCITY.y); // prevent water from flowing upwards too much
            }
            else if (d2.x >= 0 && d2.x < width && d2.y >= 0 && d2.y < height &&
                     engine->get_cell(d2.x, d2.y)->type == 0)
            {
                this->velocity.x = Math::lerp(this->velocity.x, -dir, float(delta) * FLOW_VISCOSITY);
                this->velocity.y = Math::lerp(this->velocity.y, 0.5f, float(delta) * 3.0f);

                // this->velocity.y *= 0.95;
                // this->velocity.y = CLAMP(this->velocity.y, 0.5, MAX_VELOCITY.y); // prevent water from flowing upwards too much
            }
            else
            {
                // Check horizontal movement to allow water to flow sideways when blocked
                Vector2i h1 = from + Vector2i(dir, 0);
                Vector2i h2 = from + Vector2i(-dir, 0);

                if (h1.x >= 0 && h1.x < width && h1.y >= 0 && h1.y < height &&
                    engine->get_cell(h1.x, h1.y)->type == 0)
                {
                    this->velocity.x = Math::lerp(this->velocity.x, dir * 2.0f, float(delta) * FLOW_VISCOSITY);
                    this->velocity.y = Math::lerp(this->velocity.y, 0.2f, float(delta) * 20.0f);

                    // this->velocity.y *= 0.8f;
                    // this->velocity.y = CLAMP(this->velocity.y, 0.5, MAX_VELOCITY.y); // prevent water from flowing upwards too much
                }
                else if (h2.x >= 0 && h2.x < width && h2.y >= 0 && h2.y < height &&
                         engine->get_cell(h2.x, h2.y)->type == 0)
                {
                    this->velocity.x = Math::lerp(this->velocity.x, -dir * 2.0f, float(delta) * FLOW_VISCOSITY);
                    this->velocity.y = Math::lerp(this->velocity.y, 0.2f, float(delta) * 20.0f);

                    // this->velocity.y *= 0.8f;
                    // this->velocity.y = CLAMP(this->velocity.y, 0.5, MAX_VELOCITY.y); // prevent water from flowing upwards too much
                }
                else
                {
                    this->velocity.x *= 0.99f;
                    this->velocity.y *= 0.65f;
                }
            }
        }

        this->velocity.x = CLAMP(this->velocity.x, -MAX_VELOCITY.x, MAX_VELOCITY.x);
        this->velocity.y = CLAMP(this->velocity.y, -MAX_VELOCITY.y, MAX_VELOCITY.y);

        Vector2 pred = Vector2(from.x, from.y) + this->velocity;
        Vector2i to = Vector2i((int)Math::round(pred.x), (int)Math::round(pred.y));

        to.x = CLAMP(to.x, 0, width - 1);
        to.y = CLAMP(to.y, 0, height - 1);

        new_cell = engine->find_last_available_cell(from, to);
    }

    // else
    // {
    //     this->velocity.x *= 0.4f;
    // }

    if (new_cell != from)
    {
        this->set_cell(new_cell.x, new_cell.y);
        // this->velocity.x *= 0.95f;
    }

    if (this->velocity.length() < RESTING_VELOCITY)
    {
        this->velocity = Vector2(0, 0);
        // this->set_active(false);
    }
}