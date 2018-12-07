// --
//  RockColumn.hpp
//  WorldGenerator
//
//  Data structures representing a column of rock
//  May describe: a section of the lithosphere, a section of rock being transfered between cells, net rock within a plate, ect

#ifndef RockColumn_hpp
#define RockColumn_hpp

#include "Defines.h"
#include <cmath>

namespace WorldBuilder {
    
    // A section of rock with specific properties based roughly on rock type (currently estimates of continental, oceanic crust, ect)
    class RockSegment {
        wb_float density;
        wb_float thickness;
        
    public:
        RockSegment(wb_float initialThickness, wb_float initialDensity) {
            this->set_density(initialDensity);
            this->set_thickness(initialThickness);
        }
        
        wb_float get_density() const {return density;};
        wb_float get_thickness() const {return thickness;};
        void set_density(wb_float newDensity){
            if (!std::isnormal(newDensity) || newDensity < 0) {
                throw std::invalid_argument("Non-normal density");
            }
            density = newDensity;
        }
        void set_thickness(wb_float newThickness){
            if (!std::isfinite(newThickness) || newThickness < 0) {
                throw std::invalid_argument("Invalid Thickness, must be >=0 and finite");
            }
            thickness = newThickness;
        }
        
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
        
        RockColumn() : sediment(0,1), continental(0,1), oceanic(0,1), root(0,1){};
        
        wb_float mass() const{
            return sediment.mass() + continental.mass() + oceanic.mass() + root.mass();
        }
        wb_float thickness() const {
            return sediment.get_thickness() + continental.get_thickness() + oceanic.get_thickness() + root.get_thickness();
        }
        bool isEmpty() const{
            return (this->thickness() < float_epsilon);
        }
        bool isContinental() const {
            if (continental.get_thickness() > 1000){
                return true;
            }
            return false;
        }
        
        // removes rock from the top of the column until thickness has been removed
        RockColumn removeThickness(wb_float thickness);
    };
    // combines rock columns, may want to be renamed to combineColumns
    RockColumn accreteColumns(RockColumn one, RockColumn two);
    
    // Helper function for logging changes in rock columns
    void logColumnChange(RockColumn initial, RockColumn final, bool logSedCont, bool logNet);
}

#endif /* RockColumn_hpp */
