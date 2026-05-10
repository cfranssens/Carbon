#include "api/core.hpp"

#include <emscripten/emscripten.h>
#include <chrono>

template <size_t N>
struct alignas(16) Blob {
    std::byte data[N];
};

static double g_last_ms = 0.0;

class Sys : public api::System {
public:

    void update() override {

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < 500000; ++i) {
            createEntity<0>(Blob<16>{});
        }

        auto end = std::chrono::high_resolution_clock::now();

        g_last_ms =
        std::chrono::duration<double, std::milli>(end - start)
        .count();
    }
};

extern "C" {

    EMSCRIPTEN_KEEPALIVE
    double benchmark_64() {

        api::Core core;

        core.registerSystem<Sys>();

        return g_last_ms;
    }

}
