// Real assertion-based tests for the ECS / game-engine foundation
// (17-graphics/ecs/) — including real integration with ADR 0025's
// software rasterizer (renderWorld actually calls graphics::renderScene
// unmodified). See docs/ADR/0026-ecs-game-engine-foundation.md.

#include "../ecs/components.hpp"
#include "../ecs/systems.hpp"
#include "../ecs/world.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

bool nearlyEqual(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) < tolerance;
}

}  // namespace

void testEntityCreationAndUniqueness() {
    ecs::World world;
    ecs::Entity a = world.createEntity();
    ecs::Entity b = world.createEntity();

    check(world.isAlive(a) && world.isAlive(b), "ecs: freshly-created entities are alive");
    check(!(a == b), "ecs: two freshly-created entities are distinct handles");
}

void testDestroyedEntityGenerationPreventsStaleHandleReuse() {
    ecs::World world;
    ecs::Entity a = world.createEntity();
    world.destroyEntity(a);
    check(!world.isAlive(a), "ecs: a destroyed entity's handle reports not-alive");

    ecs::Entity reused = world.createEntity();
    check(reused.id == a.id, "ecs test setup: the freed id was actually recycled by the next createEntity() call");
    check(reused.generation != a.generation,
          "ecs: a recycled id gets a bumped generation, so the old and new handles are distinguishable");
    check(!world.isAlive(a), "ecs: the OLD (stale) handle for a recycled id still correctly reports not-alive");
    check(world.isAlive(reused), "ecs: the NEW handle for the recycled id correctly reports alive");
}

void testDestroyingAnEntityRemovesItsComponents() {
    ecs::World world;
    ecs::Entity e = world.createEntity();
    world.addComponent<ecs::Transform>(e, ecs::Transform{});
    check(world.hasComponent<ecs::Transform>(e), "ecs test setup: the component was actually added");

    world.destroyEntity(e);
    check(!world.hasComponent<ecs::Transform>(e), "ecs: destroying an entity removes its components");
    check(world.getComponent<ecs::Transform>(e) == nullptr, "ecs: getComponent on a destroyed entity's handle returns nullptr, not stale data");
}

void testComponentAddGetHasRemoveAreIndependentPerType() {
    ecs::World world;
    ecs::Entity e = world.createEntity();

    world.addComponent<ecs::Transform>(e, ecs::Transform{sci::Vec3(1, 2, 3), sci::Vec3(1, 1, 1), 0.0});
    world.addComponent<ecs::Velocity>(e, ecs::Velocity{sci::Vec3(5, 0, 0)});

    check(world.hasComponent<ecs::Transform>(e) && world.hasComponent<ecs::Velocity>(e),
          "ecs: an entity can hold two different component types simultaneously");

    ecs::Transform* t = world.getComponent<ecs::Transform>(e);
    check(t != nullptr && t->position.x == 1, "ecs: getComponent returns the exact component data that was added");

    world.removeComponent<ecs::Velocity>(e);
    check(!world.hasComponent<ecs::Velocity>(e) && world.hasComponent<ecs::Transform>(e),
          "ecs: removing one component type does not affect a different component type on the same entity");
}

void testTransformToMatrixHandComputed() {
    ecs::Transform transform;
    transform.position = sci::Vec3(10, 0, 0);
    transform.rotationZRadians = M_PI / 2.0;
    transform.scale = sci::Vec3(2, 2, 2);

    // Composition order is translate * rotateZ * scale: (1,0,0) scaled
    // by 2 -> (2,0,0); rotated 90deg around Z -> (0,2,0); translated by
    // (10,0,0) -> (10,2,0).
    graphics::Mat4 m = transform.toMatrix();
    double x, y, z, w;
    m.transformPoint(sci::Vec3(1, 0, 0), x, y, z, w);
    check(nearlyEqual(x, 10, 1e-9) && nearlyEqual(y, 2, 1e-9),
          "ecs: Transform::toMatrix() composes translate*rotateZ*scale matching the hand-computed result");
}

void testMovementSystemAdvancesPositionDeterministically() {
    ecs::World world;
    ecs::Entity e = world.createEntity();
    world.addComponent<ecs::Transform>(e, ecs::Transform{sci::Vec3(0, 0, 0), sci::Vec3(1, 1, 1), 0.0});
    world.addComponent<ecs::Velocity>(e, ecs::Velocity{sci::Vec3(2, 0, 0)});

    ecs::stepMovementSystem(world, 1.0);
    ecs::Transform* t1 = world.getComponent<ecs::Transform>(e);
    check(nearlyEqual(t1->position.x, 2.0), "ecs: stepMovementSystem advances position by velocity*dt after one step");

    ecs::stepMovementSystem(world, 0.5);
    ecs::Transform* t2 = world.getComponent<ecs::Transform>(e);
    check(nearlyEqual(t2->position.x, 3.0), "ecs: stepMovementSystem correctly accumulates across multiple steps with different dt");
}

void testRenderWorldWithNoCameraReturnsFalse() {
    ecs::World world;
    ecs::Entity mesh = world.createEntity();
    world.addComponent<ecs::Transform>(mesh, ecs::Transform{});
    world.addComponent<ecs::MeshComponent>(mesh, ecs::MeshComponent{});

    graphics::FrameBuffer fb(10, 10);
    graphics::Color background{0, 0, 0};
    fb.clear(background);

    bool rendered = ecs::renderWorld(world, fb);
    check(!rendered, "ecs: renderWorld with no camera entity returns false");
    check(fb.getPixel(5, 5) == background, "ecs: renderWorld with no camera entity touches no pixels");
}

void testRenderWorldIntegratesWithGraphicsRasterizer() {
    ecs::World world;

    ecs::Entity camera = world.createEntity();
    world.addComponent<ecs::Transform>(camera, ecs::Transform{sci::Vec3(0, 0, 5), sci::Vec3(1, 1, 1), 0.0});
    world.addComponent<ecs::CameraComponent>(camera, ecs::CameraComponent{sci::Vec3(0, 0, 0), 1.0472});

    ecs::Entity meshEntity = world.createEntity();
    ecs::MeshComponent mesh;
    mesh.triangles.push_back(ecs::MeshTriangle{
        sci::Vec3(-1, -1, 0), sci::Vec3(1, -1, 0), sci::Vec3(0, 1, 0), graphics::Color{255, 0, 0}
    });
    world.addComponent<ecs::MeshComponent>(meshEntity, mesh);
    world.addComponent<ecs::Transform>(meshEntity, ecs::Transform{sci::Vec3(0, 0, 0), sci::Vec3(1, 1, 1), 0.0});

    graphics::FrameBuffer fb(20, 20);
    fb.clear(graphics::Color{0, 0, 0});
    bool rendered = ecs::renderWorld(world, fb);

    check(rendered, "ecs: renderWorld with a real camera entity returns true");
    check(fb.getPixel(10, 10) == graphics::Color{255, 0, 0},
          "ecs: a mesh entity centered in front of the camera renders visibly near screen center, via the REAL graphics::renderScene");
}

void testMovingMeshEntityChangesRenderedOutput() {
    ecs::World world;
    ecs::Entity camera = world.createEntity();
    world.addComponent<ecs::Transform>(camera, ecs::Transform{sci::Vec3(0, 0, 5), sci::Vec3(1, 1, 1), 0.0});
    world.addComponent<ecs::CameraComponent>(camera, ecs::CameraComponent{sci::Vec3(0, 0, 0), 1.0472});

    ecs::Entity meshEntity = world.createEntity();
    ecs::MeshComponent mesh;
    mesh.triangles.push_back(ecs::MeshTriangle{
        sci::Vec3(-1, -1, 0), sci::Vec3(1, -1, 0), sci::Vec3(0, 1, 0), graphics::Color{255, 0, 0}
    });
    world.addComponent<ecs::MeshComponent>(meshEntity, mesh);
    world.addComponent<ecs::Transform>(meshEntity, ecs::Transform{sci::Vec3(0, 0, 0), sci::Vec3(1, 1, 1), 0.0});

    graphics::FrameBuffer fbBefore(20, 20);
    fbBefore.clear(graphics::Color{0, 0, 0});
    ecs::renderWorld(world, fbBefore);
    check(fbBefore.getPixel(10, 10) == graphics::Color{255, 0, 0}, "ecs test setup: the triangle starts visible at screen center");

    // Move the mesh far off to the side via its Transform.
    ecs::Transform* meshTransform = world.getComponent<ecs::Transform>(meshEntity);
    meshTransform->position = sci::Vec3(20, 0, 0);

    graphics::FrameBuffer fbAfter(20, 20);
    fbAfter.clear(graphics::Color{0, 0, 0});
    ecs::renderWorld(world, fbAfter);
    check(fbAfter.getPixel(10, 10) == graphics::Color{0, 0, 0},
          "ecs: moving an entity's Transform between renders genuinely changes where it renders (live ECS state drives the scene, not a cached one)");
}

void testDestroyingMeshEntityRemovesItFromRender() {
    ecs::World world;
    ecs::Entity camera = world.createEntity();
    world.addComponent<ecs::Transform>(camera, ecs::Transform{sci::Vec3(0, 0, 5), sci::Vec3(1, 1, 1), 0.0});
    world.addComponent<ecs::CameraComponent>(camera, ecs::CameraComponent{sci::Vec3(0, 0, 0), 1.0472});

    ecs::Entity meshEntity = world.createEntity();
    ecs::MeshComponent mesh;
    mesh.triangles.push_back(ecs::MeshTriangle{
        sci::Vec3(-1, -1, 0), sci::Vec3(1, -1, 0), sci::Vec3(0, 1, 0), graphics::Color{255, 0, 0}
    });
    world.addComponent<ecs::MeshComponent>(meshEntity, mesh);
    world.addComponent<ecs::Transform>(meshEntity, ecs::Transform{sci::Vec3(0, 0, 0), sci::Vec3(1, 1, 1), 0.0});

    world.destroyEntity(meshEntity);

    graphics::FrameBuffer fb(20, 20);
    fb.clear(graphics::Color{0, 0, 0});
    ecs::renderWorld(world, fb);
    check(fb.getPixel(10, 10) == graphics::Color{0, 0, 0},
          "ecs: destroying a mesh entity removes it from subsequent renders entirely");
}

void testRenderWorldIsDeterministic() {
    ecs::World world;
    ecs::Entity camera = world.createEntity();
    world.addComponent<ecs::Transform>(camera, ecs::Transform{sci::Vec3(0, 0, 5), sci::Vec3(1, 1, 1), 0.0});
    world.addComponent<ecs::CameraComponent>(camera, ecs::CameraComponent{sci::Vec3(0, 0, 0), 1.0472});

    ecs::Entity meshEntity = world.createEntity();
    ecs::MeshComponent mesh;
    mesh.triangles.push_back(ecs::MeshTriangle{
        sci::Vec3(-1, -1, 0), sci::Vec3(1, -1, 0), sci::Vec3(0, 1, 0), graphics::Color{0, 255, 0}
    });
    world.addComponent<ecs::MeshComponent>(meshEntity, mesh);
    world.addComponent<ecs::Transform>(meshEntity, ecs::Transform{sci::Vec3(0, 0, 0), sci::Vec3(1, 1, 1), 0.0});

    graphics::FrameBuffer fb1(20, 20);
    fb1.clear(graphics::Color{0, 0, 0});
    ecs::renderWorld(world, fb1);

    graphics::FrameBuffer fb2(20, 20);
    fb2.clear(graphics::Color{0, 0, 0});
    ecs::renderWorld(world, fb2);

    bool identical = true;
    for (size_t y = 0; y < 20 && identical; ++y) {
        for (size_t x = 0; x < 20; ++x) {
            if (!(fb1.getPixel(x, y) == fb2.getPixel(x, y))) {
                identical = false;
                break;
            }
        }
    }
    check(identical, "ecs: rendering the identical World state twice produces byte-for-byte identical framebuffers");
}

int main() {
    testEntityCreationAndUniqueness();
    testDestroyedEntityGenerationPreventsStaleHandleReuse();
    testDestroyingAnEntityRemovesItsComponents();
    testComponentAddGetHasRemoveAreIndependentPerType();
    testTransformToMatrixHandComputed();
    testMovementSystemAdvancesPositionDeterministically();
    testRenderWorldWithNoCameraReturnsFalse();
    testRenderWorldIntegratesWithGraphicsRasterizer();
    testMovingMeshEntityChangesRenderedOutput();
    testDestroyingMeshEntityRemovesItFromRender();
    testRenderWorldIsDeterministic();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
