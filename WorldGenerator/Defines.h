// --
//  Defines.h
//  WorldGenerator
//

#ifndef Defines_h
#define Defines_h

#include <chrono>
#include <stdexcept>

namespace WorldBuilder {
    
    // define custom float type for easy presision switching
    typedef double wb_float;
    
    static const wb_float float_epsilon = 0.00001;
    
    static const wb_float min_interaction_age = 10.0;
    
/*************** World Attributes ***************/
    struct WorldAttributes {
        float mantleDensity;
        float radius;
        float sealevel;
        float waterDensity;
    };
    
/*************** World Step time tracking ***************/
    class TaskTracker {
    public:
        void start(){
            this->startTime = std::chrono::high_resolution_clock::now();
        };
        void end(){
            this->endTime = std::chrono::high_resolution_clock::now();
        };
        std::chrono::duration<double> duration(){
            return this->endTime - this->startTime;
        };
    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
        std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
    };
    
    struct WorldUpdateTask  {
        TaskTracker movement;
        TaskTracker transition;
        TaskTracker modification;
        
        wb_float timestepUsed;
    };
}

#endif /* Defines_h */
