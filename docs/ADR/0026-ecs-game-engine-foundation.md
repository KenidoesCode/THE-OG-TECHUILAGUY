# ADR 0026: ECS / game-engine foundation, integrated with the Graphics renderer

**Status:** Accepted. This is the FOUNDATION vertical slice of the
game-engine portion of PRD Layer 17 ("Graphics & simulation
(rasterizer, renderer, **engine**)") — deliberately kept under
`17-graphics/` rather than a new top-level layer number, since the PRD
already places "engine" inside this same layer as the rasterizer/
renderer this ADR builds directly on (ADR 0025).

## Context

ADR 0025 gave the project a real, deterministic software renderer that
consumes a flat `Scene` of world-space triangles. Nothing yet
*produces* that `Scene` from a live, mutable world of objects — a real
game/simulation engine needs entities that can be created, destroyed,
and given/removed components at runtime, and systems that operate over
those components each tick. This ADR is that missing piece: a real
Entity-Component-System (ECS) whose render system builds an ADR
0025 `Scene` from live ECS state and calls the existing `renderScene`
unchanged — genuine integration, not a second rendering path.

## Entity model

`Entity { uint32_t id; uint32_t generation; }` (`ecs/entity.hpp`).
`World` recycles freed ids but **increments the generation counter**
each time an id is reused — the classic ECS "stale handle" defense: an
`Entity` handle captured before a `destroyEntity()` call becomes
provably not-alive (`isAlive()` returns `false`) even after its
numeric id is later reused by a brand-new entity, because the
generations no longer match. This is directly tested
(`testDestroyedEntityGenerationPreventsStaleHandleReuse`), not merely
assumed from the design.

## Component storage

Each component type `T` gets its own `ComponentPool<T>` (a flat
`std::unordered_map<uint32_t, T>` keyed by entity id) inside
`World`, created lazily on first use and looked up by `std::type_index`
through a small non-templated `IComponentPool` base (only `remove(id)`
needs to be virtual, so `destroyEntity` can clean up every component
type an entity might have without `World` needing to enumerate
concrete component types itself). `World::addComponent<T>`/
`getComponent<T>`/`hasComponent<T>`/`removeComponent<T>`/`view<T>`
(every entity id currently holding a `T`) are the entire public
component API — no archetype/chunked storage, no query caching (see
"What this does not support").

## Components (this slice)

- **`Transform`**: `position`/`scale` (`sci::Vec3`, real cross-layer
  reuse from `18-scientific-computing`) plus a single `rotationZRadians`
  double — matching exactly the transforms ADR 0025's `Mat4` already
  supports (translation, scale, Z-rotation; no arbitrary 3D rotation
  yet, since `Mat4` itself doesn't have one). `Transform::toMatrix()`
  composes `translation * rotationZ * scale` via `Mat4::multiply`,
  reusing ADR 0025's matrix code directly.
- **`Velocity`**: a single `sci::Vec3 linear` — the entire state a
  minimal movement system needs.
- **`MeshComponent`**: a list of local-space triangles (`MeshTriangle
  { v0, v1, v2 (Vec3, entity-local space); color }`) — deliberately
  not a full mesh-asset format (no shared vertex buffers, no indexed
  triangles, no loading from a file); a component holds its own
  triangle list directly.
- **`CameraComponent`**: `target` (`sci::Vec3`) and `fovYRadians` — an
  entity with both a `Transform` (supplying the camera's world
  position) and a `CameraComponent` is a real, renderable camera.

## Systems (this slice)

- **`stepMovementSystem(world, dt)`** (`ecs/systems.cpp`): for every
  entity with both `Transform` and `Velocity`, advances
  `transform.position += velocity.linear * dt` — real, deterministic
  integration (`position` after `dt`, `2*dt`, ... exactly matches
  `velocity * elapsed` for constant velocity, directly tested).
- **`renderWorld(world, frameBuffer)`**: finds the first entity with
  both `Transform` and `CameraComponent` (returns `false`, rendering
  nothing, if none exists — a world with no camera is a normal, valid
  state, not a crash); builds a `graphics::Camera` from it; for every
  entity with both `Transform` and `MeshComponent`, transforms each
  local-space triangle vertex by that entity's `Transform::toMatrix()`
  into world space and appends it to a `graphics::Scene`; calls ADR
  0025's `renderScene` **unchanged** — this is the actual integration
  point this ADR exists to build, not a parallel renderer.

## Determinism

Every system here is pure arithmetic over already-deterministic
`Mat4`/`Vec3`/`renderScene` primitives with no randomness and no wall-
clock dependency (`dt` is always caller-supplied) — rendering the
identical `World` state through `renderWorld` twice, or stepping
`stepMovementSystem` with the identical `dt` sequence twice, produces
byte-for-byte identical results, directly tested the same way ADR
0025's own determinism test works.

## What this does not support

- **No archetype/chunked/SoA component storage, no query caching.**
  `view<T>()` walks a `std::unordered_map` fresh every call — correct,
  not optimized for large entity counts.
- **No arbitrary 3D rotation** (only Z-axis, inherited directly from
  `Mat4`'s own current limitation — see ADR 0025).
- **No parent/child transform hierarchy.** Every `Transform` is in
  world space directly; there is no scene-graph-style relative
  transform composition.
- **No scripting, no serialization/save-load of a `World`, no asset
  loading (meshes are defined directly in code as triangle lists).**
- **No physics/collision detection, no audio, no input handling, no
  networking.** `stepMovementSystem` is a plain, unconditional
  position integrator — nothing here detects or resolves collisions.
- **No editor, no multi-threaded system scheduling** — systems are
  called directly, in whatever order the caller chooses.
- **No component removal during iteration safety guarantee** — this
  slice's tests never mutate a pool while iterating its own `view<T>()`
  result; that specific hazard is out of scope to characterize here.

## Tested invariants

`17-graphics/tests/ecs_test.cpp`: creating entities produces unique,
initially-alive handles; destroying an entity makes `isAlive()` false
and removes every component it held; a destroyed entity's numeric id
being reused by a new entity does NOT make the old, captured `Entity`
handle alive again (the generation-mismatch stale-handle defense,
directly tested); component add/get/has/remove work correctly and
independently per type on the same entity; `Transform::toMatrix()`
produces the hand-computed expected transformed point for a combined
translate+rotate+scale case; `stepMovementSystem` advances position
by exactly `velocity * dt` per step, deterministically, over multiple
steps; `renderWorld` with no camera entity returns `false` and touches
no pixels; a world with one camera entity and one mesh entity
correctly renders a visible triangle near the expected screen
position (reusing ADR 0025's own rasterizer/scene-render machinery
unmodified); moving the mesh entity via `Transform` between two
`renderWorld` calls changes where the rendered triangle appears
(proving the ECS state genuinely drives the render, not a cached
scene); and destroying the mesh entity then re-rendering removes the
triangle from the output entirely.

## Consequences

THE OG TECHUILAGUY now has a real, tested ECS whose render system is
directly integrated with ADR 0025's software rasterizer — a genuine
"entity → component → system → camera → rendered scene" loop, not a
separate toy renderer bolted alongside the real one. This is
explicitly still a small foundation: no physics, no scripting, no
asset pipeline, no editor. The next real gaps for this specific domain,
in roughly increasing order: a simple collision/physics system (built
on `18-scientific-computing`'s existing numerics, another real
cross-layer integration opportunity), then scene serialization, then
an actual interactive loop (which needs a real window/input system ADR
0025 explicitly deferred). None of those exist yet.
