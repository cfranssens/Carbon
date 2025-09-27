# Carbon 
This is my work-in-progress attempt at creating an ECS engine focused on performance.
Originally with the intent of learning how ECS engines function for use with my OpenGL engine.
I've worked on this project for a few months now and think it's in a state where I'm comfortable enough to share progress. 

## Features
- Archetype based entity registration. (Packed component types)
- SIMD accelerated archetype querying. 
- Batched entity creation via SpawnCommandBuffers.
- Runtime registration of archetype variants (type order) and lookup within systems.

## The Archetype
An archetype is defined as a set of components that belong to an entity. (See: [`Archetype::fromComponents<Components...>`](https://github.com/cfranssens/Carbon/blob/master/include/ecs/archetype.hpp#L20-L48)).
From the unique type registry indices a bitset is constructed using a simde__m256i, this limits the number of unique component types to 256, but allows for fast querying performance. 
This archetype also stores an order in which component types are passed in relation to the order in which components have been initially registered. 
This order is used in SpawnCommandBuffers to define in which order to store components. 

## Systems 
A system is derived from the [`System`](https://github.com/cfranssens/Carbon/blob/master/include/ecs/system.hpp#L17-L55) base class. This class includes [`m_recipes`](https://github.com/cfranssens/Carbon/blob/master/include/ecs/system.hpp#L54), 
a ska::flat_hash_map that maps a calculated hash from a createEntity call to a defined archetype with a unique order. This is what I've found to be the best way to do so, 
as it removes the need to calculate this order upfront for every createEntity call and only a requires a cheap order-dependent hash.  

## Dependencies
This project depends on the following libraries:

- **[SIMDe](https://github.com/simd-everywhere/simde)**  
  Portable SIMD (vectorized) operations. Used for component mask operations in `ArchetypeSignature`.

- **[ska::flat_hash_map](https://github.com/skarupke/flat_hash_map)**  
  High-performance hash map used for storing recipes and other ECS data structures.
