#include "engine_lib.hpp"
#include <sstream>

namespace engine {
    GameObject::GameObject(const math::Vector3D& pos, std::unique_ptr<graphics::Shape> shape, 
                          const graphics::Color& color)
        : position(pos), velocity(0, 0, 0), shape(std::move(shape)), color(color) {
        base::Logger::debug("Created game object");
    }
    
    void GameObject::update(double deltaTime) {
        position = math::VectorMath::add(position, 
            math::Vector3D(velocity.x * deltaTime, velocity.y * deltaTime, velocity.z * deltaTime));
    }
    
    void GameObject::set_velocity(const math::Vector3D& vel) {
        velocity = vel;
    }
    
    math::Vector3D GameObject::get_position() const {
        return position;
    }
    
    const graphics::Shape& GameObject::get_shape() const {
        return *shape;
    }
    
    graphics::Color GameObject::get_color() const {
        return color;
    }
    
    std::string GameObject::describe() const {
        std::ostringstream oss;
        oss << "GameObject at (" << position.x << ", " << position.y << ", " << position.z << ")";
        return oss.str();
    }
    
    void Scene::add_object(std::unique_ptr<GameObject> obj) {
        base::Logger::debug("Added object to scene");
        objects.push_back(std::move(obj));
    }
    
    void Scene::update_all(double deltaTime) {
        for (auto& obj : objects) {
            obj->update(deltaTime);
        }
    }
    
    void Scene::render_all() {
        for (const auto& obj : objects) {
            graphics::Renderer::draw_shape(obj->get_shape(), obj->get_color());
        }
    }
    
    size_t Scene::object_count() const {
        return objects.size();
    }
    
    std::string Scene::get_scene_stats() const {
        std::ostringstream oss;
        oss << "Scene contains " << objects.size() << " objects";
        return oss.str();
    }
    
    Engine::Engine() : elapsed_time(0) {
        base::Logger::log("Engine initialized");
    }
    
    void Engine::add_object(std::unique_ptr<GameObject> obj) {
        scene.add_object(std::move(obj));
    }
    
    void Engine::simulate_frame(double deltaTime) {
        elapsed_time += deltaTime;
        scene.update_all(deltaTime);
        scene.render_all();
    }
    
    std::string Engine::get_engine_stats() const {
        std::ostringstream oss;
        oss << "Engine running for " << elapsed_time << "s, " << scene.get_scene_stats();
        return oss.str();
    }
}