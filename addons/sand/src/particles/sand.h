#pragma once
#include "particle.h"

namespace godot
{

  class Sand : public Particle
  {

  public:
    const static int TYPE = 1;

    Sand(
        SandEngine *engine,
        const Vector2i &cell,
        const Vector2 &position,
        const Vector2 &velocity) : Particle(engine, cell, position, velocity, TYPE) {}
    void update(double delta) override;
  };

}
