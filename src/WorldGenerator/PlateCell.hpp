// --
//  PlateCell.hpp
//  WorldGenerator
//
//  Data structures representing a Plate Cell
//  Primarily record keeping, minimal logic relating to timestep updates

#ifndef PlateCell_hpp
#define PlateCell_hpp

#include <unordered_map>

#include "RockColumn.hpp"
#include "Grid.hpp"
#include "Defines.h"
#include "ErosionFlowGraph.hpp"

namespace WorldBuilder {
    
    class PlateCell;
    /***************  Edge Cell Info ***************/
    /*  Additional info required for Plate Cells on the edge of a plate
     *
     *
     * Record keeping only
     */
    struct EdgeNeighbor {
        uint32_t plateIndex;
        uint32_t cellIndex;
        wb_float distance;
    };
    class EdgeCellInfo {
    public:
        // EdgeNeighbor currently duplicates information held in the key
        std::unordered_map<uint64_t, EdgeNeighbor> otherPlateNeighbors; // lower 32 bits are cell index, higher are plate index
        std::unordered_map<uint32_t, uint32_t> otherPlateLastNearest; // plate -> cell index
    };
    
    
    /***************  Displacement Info ***************/
    /*  Represents the displacement of a Plate Cell from its origional location
     *  Used when a plate is deformed by any geologic process (currently convergent boundaries only)
     *
     *  Record keeping only
     */
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
     *
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
        
        wb_float age;
        wb_float tempurature;
        wb_float precipitation;
        
        
        PlateCell(const GridVertex* vertex);
        
        
        // simple 
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
