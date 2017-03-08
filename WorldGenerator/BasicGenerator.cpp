// --
//  BasicGenerator.cpp
//  WorldGenerator
//

#include "BasicGenerator.hpp"

#include "math.hpp"
#include "RockColumn.hpp"
#include "Plate.hpp"
#include "World.hpp"

namespace WorldBuilder {
    void BasicGenerator::Generate(std::shared_ptr<World> theWorld){
        const float landRadius = 1.0f;
        
        Vec3 randomPoint = this->randomSource->getRandomPointUnitSphere();
        
        for (std::vector<PlateCell>::iterator cell = theWorld->get_plates()[0]->cells.begin();
             cell != theWorld->get_plates()[0]->cells.end();
             cell++) {
            // could to distance square comparisions here
            float distance = math::distanceBetween3Points(randomPoint, cell->get_vertex()->get_vector());
            if (distance < landRadius) {
                // basic land column
                cell->rock.continental.density = 2700;
                cell->rock.continental.thickness = 15000 + 10000*(landRadius-distance)/landRadius;
                cell->rock.root.density = 3200;
                cell->rock.root.thickness = 135000;
            } else {
                // create the world default column for newly divergent ocean crust
                cell->rock = theWorld->get_divergentOceanicColumn();
            }
        }
    }
}