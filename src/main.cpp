#include "api/core.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <array>
#include <cstddef>

template <size_t N>
struct alignas(16) Blob {
  std::byte data[N];
};

class Sys : public api::System {
public:
  template <size_t THREAD, typename T>
  void run_thread(size_t count) {
    for (size_t i = 0; i < count; ++i) {
      createEntity<THREAD>(T{});
    }

    //auto q = query<const float, int>();
  }

  template <typename T>
  float run_case(size_t threads, size_t per_thread) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (size_t t = 0; t < threads; ++t) {
      workers.emplace_back([&, t]() {
        switch (t) {
          case 0: run_thread<0, T>(per_thread); break;
          case 1: run_thread<1, T>(per_thread); break;
          case 2: run_thread<2, T>(per_thread); break;
          case 3: run_thread<3, T>(per_thread); break;
          case 4: run_thread<4, T>(per_thread); break;
          case 5: run_thread<5, T>(per_thread); break;
          case 6: run_thread<6, T>(per_thread); break;
          case 7: run_thread<7, T>(per_thread); break;
          case 8: run_thread<8, T>(per_thread); break;
          case 9: run_thread<9, T>(per_thread); break;
          case 10: run_thread<10, T>(per_thread); break;
          case 11: run_thread<11, T>(per_thread); break;
          case 12: run_thread<12, T>(per_thread); break;
          case 13: run_thread<13, T>(per_thread); break;
          case 14: run_thread<14, T>(per_thread); break;
          case 15: run_thread<15, T>(per_thread); break;
        }
      });
    }

    for (auto& t : workers) t.join();

    auto end = std::chrono::high_resolution_clock::now();

    float ms =
    std::chrono::duration_cast<std::chrono::microseconds>(end - start)
    .count() /
    1e3f;

    size_t total = threads * per_thread;
    return total / (ms / 1e3f); // entities/sec
  }

  void update() override {
    constexpr size_t PER_THREAD = 20'000'000;

    // bytes per entity sweep
    using Sizes = std::integer_sequence<size_t,
    16, 32, 64, 128, 256>;

    // thread sweep
    std::array<size_t, 6> thread_counts = {1, 2, 4, 6, 8, 10};

    std::cout << "Entities/sec (Millions)\n";
    std::cout << "Bytes -> ";

    // header
    ([]<size_t... Ns>(std::integer_sequence<size_t, Ns...>) {
      ((std::cout << Ns << "\t"), ...);
    })(Sizes{});
    std::cout << "\n";

    // rows = threads
    for (size_t threads : thread_counts) {
      std::cout << threads << " threads:\t";

      ([]<size_t... Ns>(Sys* self, size_t threads, size_t per_thread,
                        std::integer_sequence<size_t, Ns...>) {

        (([&] {
          float eps = self->run_case<Blob<Ns>>(threads, per_thread);
          std::cout << (eps / 1e6f) << "\t";
        }()), ...);

                        })(this, threads, PER_THREAD, Sizes{});

                        std::cout << "\n";
    }
  }
};

int main() {
  api::Core core;
  core.registerSystem<Sys>();

}
