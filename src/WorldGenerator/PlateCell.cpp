// --
//  PlateCell.cpp
//  WorldGenerator
//


#include "PlateCell.hpp"
#include<algorithm>

namespace WorldBuilder {
    // Removes thickness and combines into a single rock segement representing the resulting sediment
    RockSegment PlateCell::erodeThickness(wb_float thickness){
        RockColumn erodedRock = this->rock.removeThickness(thickness);
        return combineSegments(erodedRock.sediment, combineSegments(erodedRock.continental, combineSegments(erodedRock.oceanic, erodedRock.root)));
    }
    
    void PlateCell::homeostasis(const WorldAttributes worldAttributes, wb_float timestep){
        // current water overhead, before modification
        wb_float waterMass = 0;
        if (this->get_elevation() < worldAttributes.sealevel) {
            waterMass = (worldAttributes.sealevel - this->get_elevation()) * 1000;
        }

        this->age += timestep; // certain effects depend on cell age
        
        // crush any continental or oceanic crust above max thickness
// KILLS ALL THE CONTINENT GROWTH!
//        if (this->rock.continental.get_thickness() > 70000) {
//            wb_float thicknessToHarden = this->rock.continental.get_thickness() - 65000; // buffer so we don't compute every step
//            this->rock.continental.set_thickness(65000);
//            
//            wb_float thicknessInRoot = thicknessToHarden * this->rock.continental.get_density() / this->rock.root.get_density();
//            this->rock.root.set_thickness(this->rock.root.get_thickness() + thicknessInRoot);
//        }
        if (this->rock.oceanic.get_thickness() > 10000) {
            wb_float thicknessToHarden = this->rock.oceanic.get_thickness() - 9000; // buffer so we don't compute every step
            this->rock.oceanic.set_thickness(9000);
            
            wb_float thicknessInRoot = thicknessToHarden * this->rock.oceanic.get_density() / this->rock.root.get_density();
            this->rock.root.set_thickness(this->rock.root.get_thickness() + thicknessInRoot);
        } else if (this->rock.continental.get_thickness() > 10000) {
            // WHAT'S THIS FOR?
            // harden all oceanic to root
            wb_float thicknessToHarden = this->rock.oceanic.get_thickness();
            this->rock.oceanic.set_thickness(0);
            
            wb_float thicknessInRoot = thicknessToHarden * this->rock.oceanic.get_density() / this->rock.root.get_density();
            this->rock.root.set_thickness(this->rock.root.get_thickness() + thicknessInRoot);
        }
        
        // melt root thickness if too much
        if (this->rock.root.get_thickness() > 210000) {
            this->rock.root.set_thickness(205000);
        }
        
        // basic oceanic densification
        // increases by ~16kg/m^3/m.y. from "Densities and porosities in the oceanic crust and their variations with depth and age"
//        if (this->rock.oceanic.get_thickness() > 0 && this->rock.oceanic.get_density() < 3500) {
//            wb_float oceanicMass = this->rock.oceanic.mass();
//            this->rock.oceanic.set_density(this->rock.oceanic.get_density() + 16);
//            this->rock.oceanic.set_thickness(oceanicMass / this->rock.oceanic.get_density());
//        }
        

        this->baseOffset = -1 * (this->rock.mass() + waterMass) / worldAttributes.mantleDensity;
        
        // harden any sediment over 3k meters
        // TODO add timestep depencency
        if (this->rock.sediment.get_thickness() > 3000) {
            wb_float timeFactor = std::min(1.0, 0.25*timestep);
            wb_float thicknessToHarden = (this->rock.sediment.get_thickness() - 3000)*timeFactor;
            //float massHardened = this->rock.sediment.density * thicknessToHarden;
            this->rock.sediment.set_thickness(this->rock.sediment.get_thickness() - thicknessToHarden);
            // eventually harden to continental density
            // TODO make sure continental density is initialized to appropriate value on all rock
            RockSegment hardenedSediment(thicknessToHarden, this->rock.sediment.get_density());
            this->rock.continental = combineSegments(this->rock.continental, hardenedSediment);
        }
    }
    
    PlateCell::PlateCell(const GridVertex *ourVertex) : bIsSubducted(false), vertex(ourVertex), edgeInfo(nullptr), displacement(nullptr), age(0), tempurature(0), precipitation(0) {
    }
}