// --
//  PlateCell.cpp
//  WorldGenerator
//


#include "PlateCell.hpp"

namespace WorldBuilder {
    
    float PlateCell::get_elevation() const {
        return this->baseOffset + this->rock.thickness();
    }
    
    RockSegment PlateCell::erodeThickness(float thickness){
        float remainingThickness = thickness;
        float mass = 0; // eroded mass
        
        RockSegment erodedSegment;
        erodedSegment.density = 1;
        erodedSegment.thickness = 0;

        // TODO check for negative thickness
        if (thickness <= 0) {
            return erodedSegment;
        }
        // sediment
        if (remainingThickness - this->rock.sediment.thickness > 0) {
            remainingThickness -= this->rock.sediment.thickness;
            mass += this->rock.sediment.mass();
            this->rock.sediment.thickness = 0;
        } else {
            mass += remainingThickness * this->rock.sediment.density;
            this->rock.sediment.thickness -= remainingThickness;
            
            erodedSegment.thickness = thickness;
            erodedSegment.density = mass / thickness;
            
            return erodedSegment;
        }
        // continental
        if (remainingThickness - this->rock.continental.thickness > 0) {
            remainingThickness -= this->rock.continental.thickness;
            mass += this->rock.continental.mass();
            this->rock.continental.thickness = 0;
        } else {
            mass += remainingThickness * this->rock.continental.density;
            this->rock.continental.thickness -= remainingThickness;
            
            erodedSegment.thickness = thickness;
            erodedSegment.density = mass / thickness;
            
            return erodedSegment;
        }
        // oceanic
        if (remainingThickness - this->rock.oceanic.thickness > 0) {
            remainingThickness -= this->rock.oceanic.thickness;
            mass += this->rock.oceanic.mass();
            this->rock.oceanic.thickness = 0;
        } else {
            mass += remainingThickness * this->rock.oceanic.density;
            this->rock.oceanic.thickness -= remainingThickness;
            
            erodedSegment.thickness = thickness;
            erodedSegment.density = mass / thickness;
            
            return erodedSegment;
        }
        // still more remaining?!?!?!
        // stop eroding...
        erodedSegment.thickness = thickness - remainingThickness;
        erodedSegment.density = mass / (thickness - remainingThickness);
        return erodedSegment;
    }
    
    void PlateCell::homeostasis(const WorldAttributes worldAttributes, float timestep){
        this->baseOffset = -1 * this->rock.mass() / worldAttributes.mantleDensity;
        
        // harden any sediment over 3k meters
        // TODO add timestep
        if (this->rock.sediment.thickness > 3000) {
            float thicknessToHarden = (this->rock.sediment.thickness - 3000)*0.25*timestep;
            //float massHardened = this->rock.sediment.density * thicknessToHarden;
            this->rock.sediment.thickness = this->rock.sediment.thickness - thicknessToHarden;
            // eventually harden to continental density
            // TODO make sure continental density is initialized to appropriate value on all rock
            RockSegment hardenedSediment;
            hardenedSediment.thickness = thicknessToHarden;
            hardenedSediment.density = this->rock.sediment.density;
            this->rock.continental = combineSegments(this->rock.continental, hardenedSediment);
        }
    }
    
    PlateCell::PlateCell(const GridVertex *ourVertex) : bIsSubducted(false), vertex(ourVertex), edgeInfo(nullptr), displacement(nullptr) {
        // valid (if not realistic) rock values;
        this->rock.sediment.thickness = 0;
        this->rock.sediment.density = 1;
        
        this->rock.continental.thickness = 0;
        this->rock.continental.density = 1;
        
        this->rock.oceanic.thickness = 0;
        this->rock.oceanic.density = 1;
        
        this->rock.root.thickness = 0;
        this->rock.root.density = 1;
    }
}