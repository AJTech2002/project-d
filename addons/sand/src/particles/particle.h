#pragma once

#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3i.hpp>

namespace godot {

	static int PARTICLE_ID_COUNTER = 0;
	static float RESTING_VELOCITY = 0.1f;

    class SandEngine; // 👈 forward declaration

    class Particle {
    protected:
        SandEngine* engine;

    public:
        Vector2i cell;
        Vector2 position;
        Vector2 velocity;
        uint32_t type;
        int id;
        bool active = true;
        Vector3i debugColor = Vector3i(-1, -1, -1);

        int32_t get_cell_index() const;

        Particle(
            SandEngine* engine,
            const Vector2i& cell,
            const Vector2& position,
            const Vector2& velocity,
            uint32_t type
        );

        virtual ~Particle() = default; // IMPORTANT

        virtual void set_cell (const int x, const int y, bool clear_old_cell = true);
        virtual void swap (Particle* other);
        virtual void update_debug();
        virtual void set_active (const bool active);
        virtual void update(double delta) {
            update_debug();
        };
    };

}
