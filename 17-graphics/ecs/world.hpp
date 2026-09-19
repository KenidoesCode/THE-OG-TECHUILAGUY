#pragma once

#include "entity.hpp"

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

// A small, real ECS World — per-type component pools, generation-
// checked entity handles. Header-only: every component-storage method
// is a template, and there is no benefit splitting a .cpp out for the
// non-template entity-lifecycle bits given how small they are. See
// docs/ADR/0026-ecs-game-engine-foundation.md.

namespace ecs {

// Only `remove` needs to be virtual — World's destroyEntity() cleans
// up every component type an entity might hold without needing to
// enumerate concrete component types itself.
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void remove(uint32_t entityId) = 0;
};

template <typename T>
class ComponentPool : public IComponentPool {
public:
    T& add(uint32_t entityId, T value) {
        return data[entityId] = std::move(value);
    }

    T* get(uint32_t entityId) {
        auto it = data.find(entityId);
        return it == data.end() ? nullptr : &it->second;
    }

    bool has(uint32_t entityId) const {
        return data.count(entityId) > 0;
    }

    void remove(uint32_t entityId) override {
        data.erase(entityId);
    }

    std::vector<uint32_t> entityIds() const {
        std::vector<uint32_t> ids;
        ids.reserve(data.size());
        for (const auto& [id, unused] : data) ids.push_back(id);
        return ids;
    }

private:
    std::unordered_map<uint32_t, T> data;
};

class World {
public:
    Entity createEntity() {
        uint32_t id;
        if (!freeIds.empty()) {
            id = freeIds.back();
            freeIds.pop_back();
            alive[id] = 1;
        } else {
            id = static_cast<uint32_t>(generations.size());
            generations.push_back(0);
            alive.push_back(1);
        }
        return Entity{id, generations[id]};
    }

    bool isAlive(Entity e) const {
        return e.id < generations.size() && generations[e.id] == e.generation && alive[e.id] != 0;
    }

    void destroyEntity(Entity e) {
        if (!isAlive(e)) return;
        for (auto& [type, pool] : pools) {
            pool->remove(e.id);
        }
        generations[e.id] += 1;
        alive[e.id] = 0;
        freeIds.push_back(e.id);
    }

    template <typename T>
    T& addComponent(Entity e, T component) {
        return poolFor<T>().add(e.id, std::move(component));
    }

    template <typename T>
    T* getComponent(Entity e) {
        if (!isAlive(e)) return nullptr;
        return poolFor<T>().get(e.id);
    }

    template <typename T>
    bool hasComponent(Entity e) const {
        auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return false;
        return static_cast<ComponentPool<T>*>(it->second.get())->has(e.id);
    }

    template <typename T>
    void removeComponent(Entity e) {
        poolFor<T>().remove(e.id);
    }

    // Every currently-alive entity id holding a component of type T.
    template <typename T>
    std::vector<Entity> view() {
        std::vector<Entity> result;
        for (uint32_t id : poolFor<T>().entityIds()) {
            Entity e{id, generations[id]};
            if (isAlive(e)) result.push_back(e);
        }
        return result;
    }

private:
    template <typename T>
    ComponentPool<T>& poolFor() {
        std::type_index key(typeid(T));
        auto it = pools.find(key);
        if (it == pools.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            ComponentPool<T>& ref = *pool;
            pools[key] = std::move(pool);
            return ref;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    std::vector<uint32_t> generations;
    std::vector<uint8_t> alive;
    std::vector<uint32_t> freeIds;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools;
};

}  // namespace ecs
