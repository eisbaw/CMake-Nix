#ifndef ENGINE_LIB_HPP
#define ENGINE_LIB_HPP

#include "graphics_lib.hpp"
#include "math_lib.hpp"
#include <memory>
#include <vector>

namespace engine {
    class GameObject {
    private:
        math::Vector3D position;
        math::Vector3D velocity;
        std::unique_ptr<graphics::Shape> shape;
        graphics::Color color;
        
    public:
        GameObject(const math::Vector3D& pos, std::unique_ptr<graphics::Shape> shape, 
                   const graphics::Color& color);
        
        void update(double deltaTime);
        void set_velocity(const math::Vector3D& vel);
        math::Vector3D get_position() const;
        const graphics::Shape& get_shape() const;
        graphics::Color get_color() const;
        std::string describe() const;
    };
    
    class Scene {
    private:
        std::vector<std::unique_ptr<GameObject>> objects;
        
    public:
        void add_object(std::unique_ptr<GameObject> obj);
        void update_all(double deltaTime);
        void render_all();
        size_t object_count() const;
        std::string get_scene_stats() const;
    };
    
    class Engine {
    private:
        Scene scene;
        double elapsed_time;
        
    public:
        Engine();
        void add_object(std::unique_ptr<GameObject> obj);
        void simulate_frame(double deltaTime);
        std::string get_engine_stats() const;
    };
}

#endif // ENGINE_LIB_HPP