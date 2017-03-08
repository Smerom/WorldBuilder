// --
//  SimulationRunner.cpp
//  WorldGenerator
//

#include "SimulationRunner.hpp"

#include "World.hpp"
#include "Defines.h"
#include "Generator.hpp"

#include <iostream>

namespace WorldBuilder {
    void SimulationRunner::Run(){
        // check if we've generated the base yet
        if (this->haveGenerated == false) {
            this->theGenerator->Generate(this->theWorld);
            this->haveGenerated = true;
        }
        
        // copied from old, define input vars
        float minTimestep = this->runMinTimestep;
        uint_fast32_t stepCount = this->runTimesteps;
        
        std::vector<WorldUpdateTask> tasks;
        for (uint_fast32_t i = 0; i < stepCount; i++) {
            tasks.push_back(this->theWorld->progressByTimestep(minTimestep));
        }
        if (this->shouldLogRunTiming == true) {
            std::cout.precision(5);
            std::cout << std::scientific;
            // log our timings
            // movement
            double min, max, average, durationSeconds;
            min = tasks[0].move.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].move.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Movement took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // casting
            min = tasks[0].cast.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].cast.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Cell casting took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // homeostasis
            min = tasks[0].homeostasis.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].homeostasis.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Homeostasis took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // Collision phase
            min = tasks[0].collision.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].collision.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Collision phase took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // erosion
            min = tasks[0].erode.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].erode.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Erosion took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // super cycle
            min = tasks[0].superCycle.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].superCycle.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Supercontinent Cycle took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            min = tasks[0].timestepUsed;
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].timestepUsed;
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Averaged timestep used was: " << average << " on average, with min: " << min << " max: " << max << "\n";
        }
    }
}