// --
//  PlateCell.hpp
//  WorldGenerator
//

#ifndef PlateCell_hpp
#define PlateCell_hpp

#include <map>

#include "RockColumn.hpp"
#include "Grid.hpp"
#include "Defines.h"
#include "ErosionFlowGraph.hpp"

namespace WorldBuilder {
    
    class PlateCell;
    /***************  Edge Cell Info ***************/
    /*
     *
     *
     *
     */
    struct EdgeNeighbor {
        uint32_t plateIndex;
        uint32_t cellIndex;
        wb_float distance;
    };
    class EdgeCellInfo {
    public:
        std::map<uint64_t, EdgeNeighbor> otherPlateNeighbors; // lower 32 bits are cell index, higher are plate index
        std::map<uint32_t, uint32_t> otherPlateLastNearest; // plate -> cell index
    };
    
    class DisplacementInfo {
    public:
        Vec3 displacementLocation;
        Vec3 nextDisplacementLocation;
        //bool touched;
        //bool touchedNextRound;
        RockColumn displacedRock;
        std::shared_ptr<PlateCell> deleteTarget;
        
        //DisplacementInfo() : touched(false), touchedNextRound(false){};
    };
    
    /***************  Plate Cell ***************/
    /*  Represents a cell in plate space
     *  Used for tracking rock
     *  And effects between Plates
     *
     *  Methods should only be record keeping
     */
    class PlateCell {
        friend class World;
        friend class Plate;
        friend class AngularMomentumTracker;
    private:
        wb_float baseOffset;
        bool bIsSubducted;
        wb_float poleRadius;
    public:
        RockColumn rock;
        const GridVertex* vertex;
        
        std::shared_ptr<EdgeCellInfo> edgeInfo; // shared with edge list
        std::shared_ptr<DisplacementInfo> displacement;
        
        MaterialFlowNode* flowNode;
        
        
        PlateCell(const GridVertex* vertex);
        
        void homeostasis(const WorldAttributes, wb_float timestep);
        
        wb_float get_elevation() const;
        
        const GridVertex* get_vertex() const {
            return this->vertex;
        }
        bool isSubducted() const {
            return this->bIsSubducted;
        }
        
        bool isContinental() const {
            return (!this->bIsSubducted && this->rock.isContinental());
        }
        
        RockSegment erodeThickness(wb_float thickness);
        
    }; // class PlateCell
} // namespace WorldBuilder

#endif /* PlateCell_hpp */
