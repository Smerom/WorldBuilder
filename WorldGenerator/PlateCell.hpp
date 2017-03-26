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
    class EdgeCellInfo {
    public:
        std::map<uint64_t, wb_float> otherPlateNeighbors; // lower 32 bits are cell index, higher are plate index
        std::map<uint32_t, uint32_t> otherPlateLastNearest; // plate -> cell index
    };
    
    class DisplacementInfo {
    public:
        Vec3 displacementLocation;
        RockColumn displacedRock;
        std::shared_ptr<PlateCell> deleteTarget;
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
        
        std::shared_ptr<MaterialFlowNode> flowNode;
        
        
        PlateCell(const GridVertex* vertex);
        
        void homeostasis(const WorldAttributes, float timestep);
        
        float get_elevation() const;
        
        const GridVertex* get_vertex() const {
            return this->vertex;
        }
        bool isSubducted() const {
            return this->bIsSubducted;
        }
        
        bool isContinental() const {
            return (!this->bIsSubducted && this->rock.isContinental());
        }
        
        RockSegment erodeThickness(float thickness);
        
    }; // class PlateCell
} // namespace WorldBuilder

#endif /* PlateCell_hpp */
