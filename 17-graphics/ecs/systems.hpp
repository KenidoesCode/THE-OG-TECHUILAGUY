#pragma once

#include "../render/framebuffer.hpp"
#include "world.hpp"

// The two systems this slice provides — see
// docs/ADR/0026-ecs-game-engine-foundation.md.

namespace ecs {

// Advances transform.position by velocity.linear * dt for every
// entity with both a Transform and a Velocity component.
void stepMovementSystem(World& world, double dt);

// Finds the first entity with both Transform and CameraComponent,
// builds a graphics::Scene from every entity with both Transform and
// MeshComponent (transforming each local-space triangle vertex into
// world space), and renders it via graphics::renderScene (ADR 0025,
// unmodified). Returns false (touching no pixels) if no camera entity
// exists — a world with no camera is a normal, valid state.
bool renderWorld(World& world, graphics::FrameBuffer& fb);

}  // namespace ecs
