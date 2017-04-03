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
            std::cout << "Starting world initial generation" << std::endl;
            
            TaskTracker generationTracker;
            generationTracker.start();
            this->theGenerator->Generate(this->theWorld);
            generationTracker.end();
            
            std::cout << "Intitial Generation took " << generationTracker.duration().count() << " seconds." << std::endl;
            
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
            min = tasks[0].movement.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].movement.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Movement phase took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // transition
            min = tasks[0].transition.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].transition.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Transition phase took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            // movement
            min = tasks[0].modification.duration().count();
            max = min;
            average = min;
            for (uint_fast32_t i = 1; i < stepCount; i++) {
                durationSeconds = tasks[i].modification.duration().count();
                average = average + durationSeconds;
                if (min > durationSeconds) {
                    min = durationSeconds;
                }
                if (max < durationSeconds) {
                    max = durationSeconds;
                }
            }
            average = average / stepCount;
            std::cout << "Modification phase took " << average << " on average, with min: " << min << " max: " << max << "\n";
            
            
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