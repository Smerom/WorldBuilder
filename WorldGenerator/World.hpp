// --
//  World.hpp
//  WorldGenerator
//


#ifndef World_hpp
#define World_hpp

#include <unordered_map>

#include "RockColumn.hpp"
#include "Plate.hpp"
#include "Random.hpp"
#include "Defines.h"
#include "MomentumTracker.hpp"
#include "ErosionFlowGraph.hpp"

namespace WorldBuilder {
    /*************** Base World ***************/
    /*  Responsible for running the world forward through time
     *  This base implements common functions such as erosion and plate movement
     *  All modifications after initial generation should be done within this class,
     *  only leaving basic housekeeping to plate and cell classes
     */
    class World {
    private:
        std::unique_ptr<Grid> worldGrid;
        std::shared_ptr<Random> randomSource;
        
        std::unordered_map<uint32_t, std::shared_ptr<Plate>> plates;
        uint32_t _nextPlateId;
        uint32_t nextPlateId() {
            return _nextPlateId++;
        }
        
        RockColumn divergentOceanicColumn;
        WorldAttributes attributes;
        
        wb_float cellSmallAngle;
        
        wb_float age;
        
        wb_float supercontinentCycleStartAge;
        wb_float supercontinentCycleDuration;
        uint32_t desiredPlateCount;
        
        //AngularMomentumTracker *momentumTransfer;
        
        wb_float cellDistanceMeters;
        
        //std::vector<std::shared_ptr<Plate>> deletedPlates; // TODO, find out why plates deleted from supercontinent break things
        void deletePlate(std::shared_ptr<Plate> plateToRemove);
        
    public:
        // takes ownership of the grid
        World(Grid* theWorldGrid, std::shared_ptr<Random> randomSource);
        WorldUpdateTask progressByTimestep(wb_float minTimestep);
        wb_float randomPlateDensityOffset();
        
        
        
        
        /*************** Movement ***************/
        void columnMovementPhase(wb_float timestep);
        
        void balanceInternalPlateForce(std::shared_ptr<PlateCell>, wb_float timestep);
        
        void movePlates(wb_float timestep);
        
        /*************** Movement Aux ***************/
        float randomPlateSpeed();
        
        
        
        
        
        /*************** Transistion ***************/
        void transitionPhase(wb_float timestep);
        
        void updatePlateEdges(std::shared_ptr<Plate> plate);
        void knitPlates(std::shared_ptr<Plate> targetPlate);
        
        void renormalizeAllPlates();
        void renormalizePlate(std::shared_ptr<Plate> thePlate);
        
        void homeostasis(wb_float timestep);
        void supercontinentCycle();
        
        /*************** Transistion Aux ***************/
        std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> splitPlate(std::shared_ptr<Plate> plateToSplit);
        std::tuple<Vec3, Vec3, Vec3> getSplitPoints(std::shared_ptr<Plate> plate);
        const GridVertex* getRandomContinentalVertex(std::shared_ptr<Plate> plateToSplit);
        
        uint32_t getNearestGridIndex(Vec3 location, uint32_t hint);
        
        
        
        
        
        
        /*************** Modification ***************/
        void columnModificationPhase();
        
        void erodeThermalSmothing(wb_float timestep);
        void erodeSedimentTransport(wb_float timestep);
        
        /*************** Modification Aux ***************/
        void createFlowGraph();
        
        
        /*************** Getters ***************/
        const std::unordered_map<uint32_t, std::shared_ptr<Plate>>& get_plates() {
            return this->plates;
        }
        RockColumn get_divergentOceanicColumn() {
            return this->divergentOceanicColumn;
        }
        
        float get_age() const {
            return this->age;
        }
        
        WorldAttributes get_attributes() const {
            return this->attributes;
        }
        float get_cellDistanceMeters() const {
            return this->cellDistanceMeters;
        }
        
    }; // class World
    
}; // namespace WorldBuilder

#endif /* World_hpp */
