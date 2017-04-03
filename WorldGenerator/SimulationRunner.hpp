// --
//  SimulationRunner.hpp
//  WorldGenerator
//

#ifndef SimulationRunner_hpp
#define SimulationRunner_hpp

#include <memory>

namespace WorldBuilder {
    class Generator;
    class World;
/***************  Base Simulation Runner ***************/
/*  Responsible for running simulation to a certain endpoint
 *  This base runner simply runs for a number of timesteps
 */
    class SimulationRunner {
    private:
        std::shared_ptr<World> theWorld;
        std::shared_ptr<Generator> theGenerator;
        
        
        bool haveGenerated;

        // should be placed in specific child class
        uint_fast32_t runTimesteps;
        float runMinTimestep;
    public:
        
        bool shouldLogRunTiming;

        // requires World and Generator at construction
        // takes ownership of both
        SimulationRunner(Generator *generator, World* world) : theWorld(world), theGenerator(generator), haveGenerated(false), shouldLogRunTiming(true){};
        
        void Run();
        
    /*************** Getters ***************/
        std::shared_ptr<World> get_world() const{
            return this->theWorld;
        }
        uint_fast32_t get_runTimesteps() const {
            return this->runTimesteps;
        }
        float get_runMinTimestep() const {
            return this->runMinTimestep;
        }
        
    /*************** Setters ***************/
        void set_runTimesteps(uint_fast32_t timesteps){
            this->runTimesteps = timesteps;
        }
        void set_runMinTimestep(float minTimestep){
            this->runMinTimestep = minTimestep;
        }
    }; // class SimulationRunner
} // namespace WorldBuilder

#endif /* SimulationRunner_hpp */
