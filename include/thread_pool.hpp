#include "log.hpp"
#include <string>
#include <thread>

class ThreadPool {
public:
  ThreadPool(size_t n = std::thread::hardware_concurrency()) {
    LOG_INFO("Hardware thread capacity: " + std::to_string(n));
  }
};
