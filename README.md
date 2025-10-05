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

## Core 
As of now, The Core class is responsible for managing all runtime entity data by collecting flushed StorageBuffers into a flat vector [m_storage](https://github.com/cfranssens/Carbon/blob/master/include/ecs/core.hpp#L75). This vector holds N archetype ranges that are stored contigiuosly in memory. For example indices 0 through 4 correspond with archetype 1. These ranges are stored in [m_archetypeRanges](https://github.com/cfranssens/Carbon/blob/master/include/ecs/core.hpp#L79). On insertion the to be inserted buffer gets repeatedly swapped with the first element of each range to preserve the order. For now this suffices although I'm planning to keep an amount of free space per range to immediately insert to and later reclaim that space. 

## Systems 
A system is derived from the [`System`](https://github.com/cfranssens/Carbon/blob/master/include/ecs/system.hpp#L17-L55) base class. This class includes [`m_recipes`](https://github.com/cfranssens/Carbon/blob/master/include/ecs/system.hpp#L54), 
a ska::flat_hash_map that maps a calculated hash from a createEntity call to a defined archetype with a unique order. This is what I've found to be the best way to do so, 
as it removes the need to calculate this order upfront for every createEntity call and only a requires a cheap order-dependent hash.
This hash corresponds to a fixed size StorageBuffer where component data gets stored for batched insertion into the Core.  


## Planned
- Layered system scheduling (Lazily sorted).
- Command buffers for insertion/removal of components from existing entities.
- Querying based on inclusion/exclusion filters.
- Evalutation per system what archetypes it queries beforehand.
- An API that is usable. 

## Dependencies
This project depends on the following libraries:

- **[SIMDe](https://github.com/simd-everywhere/simde)**  
  Portable SIMD (vectorized) operations. Used for component mask operations in `ArchetypeSignature`.

- **[ska::flat_hash_map](https://github.com/skarupke/flat_hash_map)**  
  High-performance hash map used for storing recipes and other ECS data structures.

- **[ankerl::unordered_dense](https://github.com/martinus/unordered_dense)**
  High-performance hash map used because it retains insertion order for internal storage. 
