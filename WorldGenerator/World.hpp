// --
//  World.hpp
//  WorldGenerator
//


#ifndef World_hpp
#define World_hpp

#include <vector>

#include "RockColumn.hpp"
#include "Plate.hpp"
#include "Random.hpp"
#include "Defines.h"
#include "WorldCell.hpp"
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
        std::vector<std::shared_ptr<Plate>> plates;
        std::vector<WorldCell> cells;
        std::shared_ptr<Random> randomSource;
        
        RockColumn divergentOceanicColumn;
        WorldAttributes attributes;
        
        float cellSmallAngle;
        
        float age;
        
        float supercontinentCycleStartAge;
        float supercontinentCycleDuration;
        uint_fast32_t desiredPlateCount;
        
        AngularMomentumTracker *momentumTransfer;
        
        float cellDistanceMeters;
        
        //std::vector<std::shared_ptr<Plate>> deletedPlates; // TODO, find out why plates deleted from supercontinent break things
        void deletePlate(std::shared_ptr<Plate> plateToRemove);
        
    public:
        // takes ownership of the grid
        World(Grid* theWorldGrid, std::shared_ptr<Random> randomSource);
        
        WorldUpdateTask progressByTimestep(float minTimestep);
        
        float randomPlateDensityOffset();
        
    /*************** Casting ***************/
        WorldCell* getNearestWorldCell(Vec3 absoluteCellPosition, WorldCell* startCellHint);
        void castPlateCellsToWorld();
        
    /*************** Homeostasis ***************/
        void homeostasis(float timestep);
        
    /*************** Plate Movement ***************/
        void movePlates(float timestep);
        float randomPlateSpeed();
        
    /*************** Collision ***************/
        virtual void collision(float timestep);
        int_fast32_t activePlateIndexForCell(const WorldCell* cell);
        
    /*************** Erosion ***************/
        void patch();
        std::unique_ptr<MaterialFlowGraph> createFlowGraph();
        void erode(float timestep);
        
    /*************** Supercontinent Cycle ***************/
        void supercontinentCycle();
        std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> splitPlate(std::shared_ptr<Plate> plateToSplit);
        std::tuple<Vec3, Vec3, Vec3> getSplitPoints(std::shared_ptr<Plate> plate);
        const GridVertex* getRandomContinentalVertex(std::shared_ptr<Plate> plateToSplit);
        
    /*************** Getters ***************/
        const std::vector<WorldCell>& get_cells() {
            return this->cells;
        }
        const std::vector<std::shared_ptr<Plate>>& get_plates() {
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
