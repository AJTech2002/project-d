#include "particle.h"

namespace godot {

    class Water : public Particle {
        public:
            const static int TYPE = 2;

        Water(
            SandEngine* engine,
            const Vector2i& cell,
            const Vector2& position,
            const Vector2& velocity
        ) : Particle(engine, cell, position, velocity, TYPE) {}
        
        void update(double delta) override;
        
        
    };

}
