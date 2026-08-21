#include "api/core.hpp"
#include "world/entity.hpp"
#include "world/query.hpp"

#include <iostream>
#include <vector>


struct Position { float x, y, z, w; };
struct Velocity { float x, y, z, w; };
struct Tag {};

const int ENTITY_COUNT = 10'000'000;


class EcsBenchmarkSystem : public api::System {
    std::vector<world::Entity> entities;

public:
    void initialise() override {

            for (int i = 0; i < ENTITY_COUNT; ++i) {

                    createEntity<0>(Position{}, Velocity{});
        }

    }


    void update(float dt) override {

        query<0, Position, Velocity>([](world::Entity e, Position& p, Velocity& v) {
           p.x += v.x;
           p.y += v.y;
        });

    };
};

int main() {


        
    api::Core<0>::registerSystem<EcsBenchmarkSystem>();
    api::Core<0>::tick(0.0f);

    return 0;
}
