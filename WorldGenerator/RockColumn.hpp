// --
//  RockColumn.hpp
//  WorldGenerator
//

#ifndef RockColumn_hpp
#define RockColumn_hpp

#include "Defines.h"

namespace WorldBuilder {
    // section of rock with specific properties
    struct RockSegment {
        wb_float density;
        wb_float thickness;
        
        wb_float mass() const {
            return density*thickness;
        }
    };
    RockSegment combineSegments(RockSegment one, RockSegment two);
    // Bunch of rock
    struct RockColumn {
        RockSegment sediment;
        RockSegment continental;
        RockSegment oceanic;
        RockSegment root;
        
        wb_float mass() const{
            return sediment.mass() + continental.mass() + oceanic.mass() + root.mass();
        }
        wb_float thickness() const {
            return sediment.thickness + continental.thickness + oceanic.thickness + root.thickness;
        }
        bool isEmpty() const{
            return (this->thickness() < float_epsilon);
        }
        bool isContinental() const {
            if (continental.thickness > 1000){
                return true;
            }
            return false;
        }
        RockColumn removeThickness(float thickness);
    };
    RockColumn accreteColumns(RockColumn one, RockColumn two);
}

#endif /* RockColumn_hpp */
