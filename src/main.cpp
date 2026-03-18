#include "api/core.hpp"
#include "archetype.hpp"
#include "platform/pageblock.hpp"
#include "world/allocator.hpp"
#include <chrono>

struct Position {};
struct Health {};

class Sys : public api::System {
  void update() override {
    constexpr size_t COUNT = 10000000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < COUNT; i ++) {
      createEntity<0>(Position{});
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1e3f;
    
    std::cout << "Took " << duration << " ms (" << COUNT / (duration / 1e3f) / 1e6f << "M allocs / sec)" << std::endl;
  }
};

int main() {
  api::Core core;
  core.registerSystem<Sys>();
  return 0;
}
