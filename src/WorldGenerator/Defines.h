// --
//  Defines.h
//  WorldGenerator
//
//  File for constants and simple classes accessed throughout the project

#ifndef Defines_h
#define Defines_h

#include <chrono>
#include <stdexcept>

namespace WorldBuilder {
    
    // define custom float type for easy presision switching
    typedef double wb_float;
    
    // epsilon that we consider for close enough to equal numbers
    static const wb_float float_epsilon = 0.00001;
    
    // age before which new cells that collide are simply removed (as the probably shouldn't have been created in the first place
    static const wb_float min_interaction_age = 10.0;
    
/*************** World Attributes ***************/
    struct WorldAttributes {
        wb_float mantleDensity;
        wb_float radius;
        wb_float totalSeaDepth;
        wb_float sealevel;
        wb_float waterDensity;
    };
    
/*************** World Step time tracking ***************/
    // used for coarse grained performance tracking
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
