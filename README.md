<img src="https://github.com/cfranssens/Carbon/blob/master/carbon.png" alt="logo" width="256"/>

# Carbon ECS

## Introduction
This is my attempt at building a competitive ECS engine. It is primarily a passion project, but also a way to better understand how ECS systems work internally.

## Overview
Carbon is an archetype-based ECS, where archetypes are treated as canonicalized, compiled types.
Storage is implemented as a per-thread, isolated, chunked arena allocator, statically indexed by archetype. These arenas contain arrays of page-aligned blocks that store metadata, entity data, and layout definitions.

A **PageBlock** is a fixed-size region (64 KiB in this repo) consisting of:
- Two bitsets: active state and snapshot state
- A contiguous entity data region

Block capacity is derived from the layout descriptor by taking the largest component in the archetype as the sizing baseline, then fitting as many aligned component arrays as possible into the remaining block space after reserving room for both bitsets and metadata. This keeps the layout predictable while still making efficient use of the available space.
Entity insertion is resolved using a block pointer into the active bitset to determine availability. The same mechanism applies to other structural operations.

## Planned (partially implemented in earlier revisions)
Queries are globally cached and track which parts of the archetype registry have already been indexed. Since the registry is mostly append-only, this can be validated using a simple index + length check, avoiding full rebuilds.

A **Query**:
- Holds a lambda operating on components present in the snapshot bitset
- Manages a set of **Jobs**, where each job processes a single PageBlock

Jobs are also cached incrementally, so only new storage regions contribute additional jobs.

## Concurrency Model
The system allows multiple parallel writers for structural changes within isolated regions, such as appends and same-thread migrations.

Cross-thread structural changes, such as deletions, require additional checks to avoid conflicts. This is handled with bit operations between the active and snapshot bitsets. Data itself can be copied freely, but the active and snapshot bitsets control how and when that happens.

The snapshot state acts as a global sync point, representing the active state from the previous frame.

As a result, cross-thread structural changes are always delayed by at least one frame. A possible future improvement here is explicit sync points per Query instead of a single global sync at end of frame.

## Non-Structural Changes
Writes performed inside Jobs are deferred until the end-of-frame sync point via write buffers for non-const queried components.
An important semantic here is that if access is limited across the frame (only ever one Job writes to the PageBlock a d it is not read afterwards), those writes can be applied immediately instead of being deferred.

Even when data writes are immediate, state updates are still deferred until the next sync point to avoid inconsistencies. This avoids traditional ECS synchronization issues and allows for more flexible execution. The goal is to structure workloads so threads are never idle.
Systems are loosely ordered by definition order unless explicitly overridden with a priority.
