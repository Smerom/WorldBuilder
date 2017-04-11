//  --
//  RockColumn.cpp
//  WorldGenerator
//

#include "RockColumn.hpp"

#include <iostream>

namespace WorldBuilder {
    RockSegment combineSegments(RockSegment one, RockSegment two){
        wb_float thickness;
        wb_float density;
        thickness = one.get_thickness() + two.get_thickness();
        if (thickness > float_epsilon) {
            density = std::abs((one.get_density()*one.get_thickness() + two.get_density()*two.get_thickness()) / thickness);
        } else {
            density = one.get_density();
        }
        return RockSegment(thickness, density);
    }
    
    RockColumn accreteColumns(RockColumn one, RockColumn two){
        RockColumn result;
        result.sediment = combineSegments(one.sediment, two.sediment);
        result.continental = combineSegments(one.continental, two.continental);
        result.oceanic = combineSegments(one.oceanic, two.oceanic);
        result.root = combineSegments(one.root, two.root);
        return result;
    }
    
    RockColumn RockColumn::removeThickness(wb_float thickness){
        wb_float remainingThickness = thickness;
        
        RockColumn removedColumn;
        // sediment
        if (remainingThickness - this->sediment.get_thickness() > 0) {
            remainingThickness -= this->sediment.get_thickness();
            removedColumn.sediment = this->sediment;
            this->sediment.set_thickness(0);
        } else {
            this->sediment.set_thickness(this->sediment.get_thickness() - remainingThickness);
            
            removedColumn.sediment.set_thickness(remainingThickness);
            removedColumn.sediment.set_density(this->sediment.get_density());
            
            return removedColumn;
        }
        // continental
        if (remainingThickness - this->continental.get_thickness() > 0) {
            remainingThickness -= this->continental.get_thickness();
            removedColumn.continental = this->continental;
            this->continental.set_thickness(0);
        } else {
            this->continental.set_thickness(this->continental.get_thickness() - remainingThickness);
            
            removedColumn.continental.set_thickness(remainingThickness);
            removedColumn.continental.set_density(this->continental.get_density());
            
            return removedColumn;
        }
        // oceanic
        if (remainingThickness - this->oceanic.get_thickness() > 0) {
            remainingThickness -= this->oceanic.get_thickness();
            removedColumn.oceanic = this->oceanic;
            this->oceanic.set_thickness(0);
        } else {
            this->oceanic.set_thickness(this->oceanic.get_thickness() - remainingThickness);
            
            removedColumn.oceanic.set_thickness(remainingThickness);
            removedColumn.oceanic.set_density(this->oceanic.get_density());
            
            return removedColumn;
        }

        // root
        if (remainingThickness - this->root.get_thickness() > 0) {
            remainingThickness -= this->root.get_thickness();
            removedColumn.root = this->root;
            this->root.set_thickness(0);
        } else {
            this->root.set_thickness(this->root.get_thickness() - remainingThickness);
            
            removedColumn.root.set_thickness(remainingThickness);
            removedColumn.root.set_density(this->root.get_density());
            
            return removedColumn;
        }

        // hopefully we should have some left by now...
        return removedColumn;
    }
    
    void logColumnChange(RockColumn initial, RockColumn final, bool logSedCont, bool logNet){
        std::cout.precision(6);
        if (logNet) {
            if (logSedCont) {
                std::cout << "Sed+Cont is    " << std::scientific << final.sediment.get_thickness() + final.continental.get_thickness() << " or " << std::fixed << (final.sediment.get_thickness() + final.continental.get_thickness()) / initial.continental.get_thickness() << " of Origional" << std::endl;
            } else {
                std::cout << "Sediment is    " << std::scientific << final.sediment.get_thickness() << " or " << std::fixed << final.sediment.get_thickness() / initial.sediment.get_thickness() << " of Origional" << std::endl;
            }
            std::cout << "Continental is " << std::scientific << final.continental.get_thickness() << " or " << std::fixed << final.continental.get_thickness() / initial.continental.get_thickness() << " of Origional" << std::endl;
            std::cout << "Oceanic is     " << std::scientific << final.oceanic.get_thickness() << " or " << std::fixed << final.oceanic.get_thickness() / initial.oceanic.get_thickness() << " of Origional" << std::endl;
            std::cout << "Root is        " << std::scientific << final.root.get_thickness() << " or " << std::fixed << final.root.get_thickness() / initial.root.get_thickness() << " of Origional" << std::endl;
        } else {
            if (logSedCont) {
                std::cout << "Sed+Cont change is    " << std::scientific << final.sediment.get_thickness() + final.continental.get_thickness() - initial.sediment.get_thickness() - initial.continental.get_thickness() << " or " << std::fixed << (final.sediment.get_thickness() + final.continental.get_thickness()) / initial.continental.get_thickness() << " of Origional" << std::endl;
            } else {
                std::cout << "Sediment change is    " << std::scientific << final.sediment.get_thickness() - initial.sediment.get_thickness() << " or " << std::fixed << final.sediment.get_thickness() / initial.sediment.get_thickness() << " of Origional" << std::endl;
            }
            std::cout << "Continental change is " << std::scientific << final.continental.get_thickness() - initial.continental.get_thickness() << " or " << std::fixed << final.continental.get_thickness() / initial.continental.get_thickness() << " of Origional" << std::endl;
            std::cout << "Oceanic change is     " << std::scientific << final.oceanic.get_thickness() - initial.oceanic.get_thickness() << " or " << std::fixed << final.oceanic.get_thickness() / initial.oceanic.get_thickness() << " of Origional" << std::endl;
            std::cout << "Root change is        " << std::scientific << final.root.get_thickness() - initial.root.get_thickness() << " or " << std::fixed << final.root.get_thickness() / initial.root.get_thickness() << " of Origional" << std::endl;
        }
    }
}

