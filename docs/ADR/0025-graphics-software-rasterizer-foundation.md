# ADR 0025: Graphics — deterministic software rasterizer foundation

**Status:** Accepted. This is the FOUNDATION vertical slice of PRD
Layer 17 (Graphics & simulation). It builds a real CPU software
rasterizer — framebuffer, 4×4 transform matrices, perspective
projection, depth-tested triangle rasterization, and a minimal scene
renderer — with deterministic, pixel-exact output. No GPU/window
system is used or required; this is an explicit, reasoned choice, not
a fallback taken because something else failed.

## Context

This project's test environment (a hosted compiler, run via CI-style
`bash tools/verify_all.sh` and inside a WSL2/Linux shell — see
`docs/VERIFICATION.md`) has no guaranteed GPU, display server, or
windowing toolkit. Introducing a real GPU API (Vulkan/OpenGL/DirectX)
or a windowing dependency (GLFW/SDL) at this stage would make the
graphics foundation itself untestable in this project's own hosted
verification runner — exactly the "do not blindly introduce a massive
graphics dependency" trap this ADR was scoped to avoid. A deterministic
CPU software rasterizer is instead the smallest real implementation
that establishes the actual architecture (transforms → projection →
rasterization → framebuffer) a future GPU backend or windowed
interactive viewer could sit behind, without any of this foundation's
correctness depending on hardware or a display being present.

## Architecture

```
Scene (triangles in world space)
        │
   Model/View/Projection (Mat4)
        │
   Screen-space vertices (x, y, depth)
        │
   Rasterizer (barycentric, per-pixel depth test)
        │
   FrameBuffer (RGB pixels + depth buffer)
```

- **`graphics::Mat4`** (`17-graphics/math/mat4.hpp/cpp`): a real 4×4
  matrix — identity, translation, scale, a Z-axis rotation, matrix
  multiplication, and a perspective-projection matrix (the standard
  field-of-view/aspect/near/far form). Operates on
  `18-scientific-computing/linalg/vec3.hpp`'s existing `Vec3` for
  points — a genuine cross-layer integration (scientific computing →
  graphics), not a parallel vector type.
- **`graphics::FrameBuffer`** (`17-graphics/render/framebuffer.hpp/cpp`):
  a real 2D RGB pixel buffer plus a parallel depth buffer, both
  indexable and clearable.
- **`graphics::rasterizeTriangle`** (`17-graphics/render/rasterizer.hpp/cpp`):
  real barycentric-coordinate triangle rasterization directly against
  a `FrameBuffer` — for every pixel in the triangle's bounding box,
  computes barycentric weights, rejects the pixel if any weight is
  negative (outside the triangle), otherwise interpolates depth and
  writes the pixel only if it passes the depth test (nearer than
  whatever is already in the depth buffer at that pixel) — real
  occlusion between overlapping triangles, not draw-order-dependent
  painter's-algorithm layering.
- **`graphics::renderScene`** (`17-graphics/scene/scene.hpp/cpp`): the
  actual end-to-end path — takes a `Scene` (a flat list of world-space
  triangles, each with a solid color) and a camera (position, target,
  field of view), builds the view and perspective matrices, projects
  every triangle's three vertices to screen space, and rasterizes each
  one into a `FrameBuffer`.

## Determinism

Every stage here is pure arithmetic over `double`s with no randomness
and no timing dependency — rendering the identical `Scene` from the
identical camera always produces the identical `FrameBuffer` pixel-
for-pixel, which is exactly what the test suite checks (not just "some
pixels changed," but specific, hand-reasoned-about pixel positions and
colors).

## What this does not support

- **No GPU execution, no windowing, no interactive input.** Everything
  renders to an in-memory `FrameBuffer`; there is no way to display it
  on screen yet, and no event loop.
- **No shaders/programmable pipeline.** Every triangle is rendered
  with a single flat, interpolation-free color (no vertex colors
  blended across a triangle, no textures, no lighting model).
- **No anti-aliasing.** Pixel coverage is a hard inside/outside test.
- **No scene graph, no hierarchical transforms, no asset loading
  (meshes/textures/materials from files).** A `Scene` is a flat list
  of triangles built directly in code.
- **No clipping against the near/far/side planes** — a triangle
  partially or fully behind the camera can rasterize incorrectly
  (screen coordinates computed from a negative or near-zero view-space
  depth are not well-defined for a perspective divide). Scenes used by
  this slice's tests keep all geometry safely in front of the camera;
  real clipping is explicit future work.
- **No optimization** — this is a naive, unoptimized rasterizer (no
  tile-based rendering, no SIMD, no multithreading).

## Integration points prepared for

This is deliberately structured so later systems can consume it
without rework: `FrameBuffer` is a plain, self-contained pixel/depth
buffer (any future consumer — a terminal/image-file writer, a real
window blit, or a headless test harness — just reads its pixel array);
`Scene`/`renderScene` take world-space triangles and a camera
independent of where the geometry came from, so NetLab's future visual
topology editor, the Space simulator's future visualization, a
robotics simulator, or a game engine can each build a `Scene` from
their own domain data and call the identical `renderScene` — none of
which exist yet.

## Tested invariants

`17-graphics/tests/graphics_test.cpp`: `FrameBuffer` clear/set/get
pixel correctness; `Mat4` identity, translation, scale, and rotation
each verified against hand-computed expected transformed points;
matrix multiplication verified against a hand-computed small example;
perspective projection of a known world-space point produces the
expected screen-space position (a point directly in front of the
camera projects near the screen center); `rasterizeTriangle` correctly
colors a pixel provably inside a triangle and leaves a pixel provably
outside it at the background color, verified at explicit, reasoned-
about pixel coordinates, not merely "some pixels changed"; the depth
test correctly keeps the nearer of two overlapping triangles' colors
regardless of draw order (rendering them in both orders is asserted to
produce the identical result); and a full `renderScene` call on a
small, fixed scene produces the exact same `FrameBuffer` byte-for-byte
across repeated calls, proving determinism end to end, not just at
each individual stage.

## Consequences

THE OG TECHUILAGUY now has a real, tested, deterministic software
rendering pipeline capable of actually turning a 3D scene description
into pixels with correct depth occlusion — the "scene → render" loop
this domain's demo-first target calls for, entirely CPU-side and
hardware-independent. The next real gaps, in roughly increasing order:
near-plane clipping (needed before this can safely render arbitrary
camera-relative geometry), a way to actually view the output (a PPM/
image-file writer would be the smallest next step, a real window the
larger one), then the integration points named above (NetLab
visualization, Space Systems visualization, a robotics simulator, a
game engine) — none of which exist yet.
