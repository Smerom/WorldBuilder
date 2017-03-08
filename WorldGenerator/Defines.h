// --
//  Defines.h
//  WorldGenerator
//

#ifndef Defines_h
#define Defines_h

#include <chrono>

namespace WorldBuilder {
    static const float float_epsilon = 0.00001;
    
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
        TaskTracker move;
        TaskTracker cast;
        TaskTracker homeostasis;
        TaskTracker collision;
        TaskTracker superCycle;
        
        TaskTracker erode;
        TaskTracker plateRemoval;
        
        float timestepUsed;
    };
}

#endif /* Defines_h */
