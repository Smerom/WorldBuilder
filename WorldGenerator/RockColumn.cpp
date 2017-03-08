//  --
//  RockColumn.cpp
//  WorldGenerator
//

#include "RockColumn.hpp"

#include <math.h>

namespace WorldBuilder {
    RockSegment combineSegments(RockSegment one, RockSegment two){
        RockSegment result;
        result.thickness = one.thickness + two.thickness;
        if (fabsf(result.thickness) > float_epsilon) {
            result.density = fabsf((one.density*one.thickness + two.density*two.thickness) / (result.thickness));
        } else {
            result.thickness = 0;
            result.density = one.density;
        }
        return result;
    }
    
    RockColumn accreteColumns(RockColumn one, RockColumn two){
        RockColumn result;
        result.sediment = combineSegments(one.sediment, two.sediment);
        result.continental = combineSegments(one.continental, two.continental);
        result.oceanic = combineSegments(one.oceanic, two.oceanic);
        result.root = combineSegments(one.root, two.root);
        return result;
    }
    
    RockColumn RockColumn::removeThickness(float thickness){
        float remainingThickness = thickness;
        
        RockColumn removedColumn;
        // sediment
        if (remainingThickness - this->sediment.thickness > 0) {
            remainingThickness -= this->sediment.thickness;
            removedColumn.sediment = this->sediment;
            this->sediment.thickness = 0;
        } else {
            this->sediment.thickness -= remainingThickness;
            
            removedColumn.sediment.thickness = remainingThickness;
            removedColumn.sediment.density = this->sediment.density;
            
            return removedColumn;
        }
        // continental
        if (remainingThickness - this->continental.thickness > 0) {
            remainingThickness -= this->continental.thickness;
            removedColumn.continental = this->continental;
            this->continental.thickness = 0;
        } else {
            this->continental.thickness -= remainingThickness;
            
            removedColumn.continental.thickness = remainingThickness;
            removedColumn.continental.density = this->continental.density;
            
            return removedColumn;
        }
        // oceanic
        if (remainingThickness - this->oceanic.thickness > 0) {
            remainingThickness -= this->oceanic.thickness;
            removedColumn.oceanic = this->oceanic;
            this->oceanic.thickness = 0;
        } else {
            this->oceanic.thickness -= remainingThickness;
            
            removedColumn.oceanic.thickness = remainingThickness;
            removedColumn.oceanic.density = this->oceanic.density;
            
            return removedColumn;
        }
        // root
        
        if (remainingThickness - this->root.thickness > 0) {
            remainingThickness -= this->root.thickness;
            removedColumn.root = this->root;
            this->root.thickness = 0;
        } else {
            this->root.thickness -= remainingThickness;
            
            removedColumn.root.thickness = remainingThickness;
            removedColumn.root.density = this->root.density;
            
            return removedColumn;
        }
        // hopefully we should have some left by now...
        return removedColumn;
    }
}

