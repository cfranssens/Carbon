#include "ecs/archetype.hpp"
#include "ecs/command_buffer.hpp"
#include "ecs/core.hpp"
#include "ecs/system.hpp"
#include "thread_pool.hpp"
#include <cstdlib>
#include <iostream>

Core core;

struct Velocity {
  float x, y, z;
};

class MovementSystem : public System {
public:
  void update() override {
    EntityID first = createEntity(2.f, 1);
    EntityID second = createEntity(2.f, 1, 1ULL);
    EntityID third = createEntity('p', 1);

    EntityID fourth = createEntity(Velocity{0.0f, 0.0f, 0.0f}, 0);

    flushAllStorageBuffers(&core);
  }
};

int main() {
  Archetype *t;

  MovementSystem sys;
  SystemScheduler scheduler;
  scheduler.registerSystem<MovementSystem>();

  scheduler.update();
  scheduler.update();
  core.debugRanges();

  return 0;
}
