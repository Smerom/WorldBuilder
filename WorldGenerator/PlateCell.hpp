// --
//  PlateCell.hpp
//  WorldGenerator
//

#ifndef PlateCell_hpp
#define PlateCell_hpp

#include "RockColumn.hpp"
#include "Grid.hpp"
#include "Defines.h"
#include "WorldCell.hpp"

namespace WorldBuilder {
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
        float baseOffset;
        bool bIsSubducted;
        float poleRadius;
    public:
        RockColumn rock;
        const GridVertex* vertex;
        WorldCell* lastNearest;
        
        PlateCell();
        
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
