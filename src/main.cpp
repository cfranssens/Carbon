#include "ecs/archetype.hpp"
#include "ecs/system.hpp"
#include "thread_pool.hpp"
#include <cstdlib>
#include <iostream>

int main() {
  Archetype *t;
  ThreadPool pool;

  System sys;
  sys.test<int, float>();

  std::cout << "Hello world!" << std::endl;
  return 0;
}
