#include "systems.hpp"

#include "../scene/scene.hpp"
#include "components.hpp"

namespace ecs {

namespace {

sci::Vec3 transformLocalPoint(const graphics::Mat4& m, const sci::Vec3& local) {
    double x, y, z, w;
    m.transformPoint(local, x, y, z, w);
    // Transform::toMatrix() only ever composes translation/rotationZ/
    // scale, all of which are affine (w row is always [0,0,0,1]), so
    // w is always exactly 1 here — no perspective divide needed.
    return sci::Vec3(x, y, z);
}

}  // namespace

void stepMovementSystem(World& world, double dt) {
    for (Entity e : world.view<Velocity>()) {
        Transform* transform = world.getComponent<Transform>(e);
        Velocity* velocity = world.getComponent<Velocity>(e);
        if (transform == nullptr || velocity == nullptr) continue;
        transform->position += velocity->linear * dt;
    }
}

bool renderWorld(World& world, graphics::FrameBuffer& fb) {
    Entity cameraEntity{};
    bool foundCamera = false;
    for (Entity e : world.view<CameraComponent>()) {
        if (world.hasComponent<Transform>(e)) {
            cameraEntity = e;
            foundCamera = true;
            break;
        }
    }
    if (!foundCamera) return false;

    Transform* cameraTransform = world.getComponent<Transform>(cameraEntity);
    CameraComponent* cameraComponent = world.getComponent<CameraComponent>(cameraEntity);

    graphics::Camera camera{cameraTransform->position, cameraComponent->target, cameraComponent->fovYRadians};

    graphics::Scene scene;
    for (Entity e : world.view<MeshComponent>()) {
        Transform* transform = world.getComponent<Transform>(e);
        MeshComponent* mesh = world.getComponent<MeshComponent>(e);
        if (transform == nullptr || mesh == nullptr) continue;

        graphics::Mat4 model = transform->toMatrix();
        for (const auto& tri : mesh->triangles) {
            graphics::Triangle worldTri;
            worldTri.v0 = transformLocalPoint(model, tri.v0);
            worldTri.v1 = transformLocalPoint(model, tri.v1);
            worldTri.v2 = transformLocalPoint(model, tri.v2);
            worldTri.color = tri.color;
            scene.triangles.push_back(worldTri);
        }
    }

    graphics::renderScene(scene, camera, fb);
    return true;
}

}  // namespace ecs
