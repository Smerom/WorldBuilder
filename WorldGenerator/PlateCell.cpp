// --
//  PlateCell.cpp
//  WorldGenerator
//


#include "PlateCell.hpp"

namespace WorldBuilder {
    
    wb_float PlateCell::get_elevation() const {
        return this->baseOffset + this->rock.thickness();
    }
    
    RockSegment PlateCell::erodeThickness(wb_float thickness){
        RockColumn erodedRock = this->rock.removeThickness(thickness);
        return combineSegments(erodedRock.sediment, combineSegments(erodedRock.continental, combineSegments(erodedRock.oceanic, erodedRock.root)));
    }
    
    void PlateCell::homeostasis(const WorldAttributes worldAttributes, wb_float timestep){
        this->age += timestep;
        
        // crush any continental or oceanic crust above max thickness
        // KILLS ALL THE CONTINENT GROWTH!
//        if (this->rock.continental.get_thickness() > 70000) {
//            wb_float thicknessToHarden = this->rock.continental.get_thickness() - 65000; // buffer so we don't compute every step
//            this->rock.continental.set_thickness(65000);
//            
//            wb_float thicknessInRoot = thicknessToHarden * this->rock.continental.get_density() / this->rock.root.get_density();
//            this->rock.root.set_thickness(this->rock.root.get_thickness() + thicknessInRoot);
//        }
//        if (this->rock.oceanic.get_thickness() > 10000) {
//            wb_float thicknessToHarden = this->rock.oceanic.get_thickness() - 9000; // buffer so we don't compute every step
//            this->rock.oceanic.set_thickness(9000);
//            
//            wb_float thicknessInRoot = thicknessToHarden * this->rock.oceanic.get_density() / this->rock.root.get_density();
//            this->rock.root.set_thickness(this->rock.root.get_thickness() + thicknessInRoot);
//        }
        
        // melt root thickness if too much
        if (this->rock.root.get_thickness() > 210000) {
            this->rock.root.set_thickness(205000);
        }
        
        
        this->baseOffset = -1 * this->rock.mass() / worldAttributes.mantleDensity;
        
        // harden any sediment over 3k meters
        // TODO add timestep
        if (this->rock.sediment.get_thickness() > 3000) {
            wb_float thicknessToHarden = (this->rock.sediment.get_thickness() - 3000)*0.25*timestep;
            //float massHardened = this->rock.sediment.density * thicknessToHarden;
            this->rock.sediment.set_thickness(this->rock.sediment.get_thickness() - thicknessToHarden);
            // eventually harden to continental density
            // TODO make sure continental density is initialized to appropriate value on all rock
            RockSegment hardenedSediment(thicknessToHarden, this->rock.sediment.get_density());
            this->rock.continental = combineSegments(this->rock.continental, hardenedSediment);
        }
    }
    
    PlateCell::PlateCell(const GridVertex *ourVertex) : bIsSubducted(false), vertex(ourVertex), edgeInfo(nullptr), displacement(nullptr), flowNode(nullptr), age(0) {
    }
}