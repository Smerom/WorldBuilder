//  --
//  WorldCell.hpp
//  WorldGenerator
//


#ifndef WorldCell_hpp
#define WorldCell_hpp

#include "Grid.hpp"
#include "RockColumn.hpp"

#include <utility>

namespace WorldBuilder {
    class PlateCell;
/***************  World Cell ***************/
/*  Represents a cell in world space
 *  Used for determining plate cell collisions
 *  And for world level effects
 */
    class WorldCell {
        friend class World;
    private:
        const GridVertex* vertex;
        std::vector<std::vector<PlateCell*>> surfaceCells;
        std::vector<std::vector<PlateCell*>> subductedCells;
        std::vector<std::vector<PlateCell*>> emptyCells;
        float strandedElevation;
        RockSegment strandedSediment;
        
        // should have been cleared to the right plate count by now
        // only used in plate casting from World
        void addSurfaceCell(PlateCell* cell, size_t plateIndex){
            surfaceCells[plateIndex].push_back(cell);
        };
        void addSubductedCell(PlateCell* cell, size_t plateIndex){
            subductedCells[plateIndex].push_back(cell);
        };
        void addEmptyCell(PlateCell* cell, size_t plateIndex){
            emptyCells[plateIndex].push_back(cell);
        };
        
    public:
        
        void clearPlateCells(size_t plateCount);
        
        // doesn't clear sediment, as that wants to be transfered
        void resetStrandedElevation(){
            this->strandedElevation = -1*std::numeric_limits<float>::max();
        }
        
        float get_elevation(int_fast32_t activePlateIndex) const;
        
        const GridVertex* get_vertex(){
            return vertex;
        }
        
        const std::vector<std::vector<PlateCell*>> get_surfaceCells() const{
            return this->surfaceCells;
        }
        
        RockSegment get_strandedSegment() const {
            return this->strandedSediment;
        }
        

    }; // class WorldCell
} // namespace WorldBuilder
#endif /* WorldCell_hpp */
