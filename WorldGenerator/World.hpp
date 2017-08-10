// --
//  World.hpp
//  WorldGenerator
//


#ifndef World_hpp
#define World_hpp

#include <unordered_map>
#include <limits>

#include "RockColumn.hpp"
#include "Plate.hpp"
#include "Random.hpp"
#include "Defines.h"
#include "ErosionFlowGraph.hpp"
#include "MomentumTracker.hpp"

namespace WorldBuilder {
    struct LocationInfo {
        wb_float elevation;
        wb_float sediment;
        wb_float tempurature;
        wb_float precipitation;
        uint32_t plateId;
        
        LocationInfo() : elevation(0), sediment(0), tempurature(0), precipitation(0), plateId(std::numeric_limits<uint32_t>::max()){};
    };
    /*************** Base World ***************/
    /*  Responsible for running the world forward through time
     *  This base implements common functions such as erosion and plate movement
     *  All modifications after initial generation should be done within this class,
     *  only leaving basic housekeeping to plate and cell classes
     */
    class World {
    private:
        std::shared_ptr<Grid> worldGrid;
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
        
        std::shared_ptr<AngularMomentumTracker> momentumTracker;
        
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
        
        void balanceInternalPlateForce(std::shared_ptr<Plate> plate, wb_float timestep);
        
        void computeEdgeInteraction(wb_float timestep);
        
        void movePlates(wb_float timestep);
        
        /*************** Movement Aux ***************/
        wb_float randomPlateSpeed();
        
        
        
        void clean();
        
        /*************** Transistion ***************/
        void transitionPhase(wb_float timestep);
        
        void updatePlateEdges(std::shared_ptr<Plate> plate);
        void knitPlates(std::shared_ptr<Plate> targetPlate);
        
        void renormalizeAllPlates();
        void renormalizePlate(std::shared_ptr<Plate> thePlate);
        
        std::vector<std::shared_ptr<PlateCell>> riftPlate(std::shared_ptr<Plate> plate);
        
        void homeostasis(wb_float timestep);
        void updateTempurature();
        void updatePrecipitation();
        void supercontinentCycle();
        
        /*************** Transistion Aux ***************/
        std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> splitPlate(std::shared_ptr<Plate> plateToSplit);
        std::tuple<Vec3, Vec3, Vec3> getSplitPoints(std::shared_ptr<Plate> plate);
        const GridVertex* getRandomContinentalVertex(std::shared_ptr<Plate> plateToSplit);
        
        uint32_t getNearestGridIndex(Vec3 location, uint32_t hint);
        
        
        
        
        
        
        /*************** Modification ***************/
        void columnModificationPhase(wb_float timestep);
        
        void erodeThermalSmoothing(wb_float timestep);
        void erodeSedimentTransport(wb_float timestep);
        
        /*************** Modification Aux ***************/
        std::shared_ptr<MaterialFlowGraph> createFlowGraph();
        
        
        /*************** Getters ***************/
        const std::unordered_map<uint32_t, std::shared_ptr<Plate>>& get_plates() {
            return this->plates;
        }
        RockColumn get_divergentOceanicColumn() {
            return this->divergentOceanicColumn;
        }
        
        wb_float get_age() const {
            return this->age;
        }
        
        WorldAttributes get_attributes() const {
            return this->attributes;
        }
        wb_float get_cellDistanceMeters() const {
            return this->cellDistanceMeters;
        }
        
        LocationInfo get_locationInfo(Vec3 location);
        
        bool validate();
        
        RockColumn netRock();
        
    }; // class World
    
}; // namespace WorldBuilder

#endif /* World_hpp */
