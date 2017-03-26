// --
//  BombardmentGenerator.cpp
//  WorldGenerator
//

#include "BombardmentGenerator.hpp"

#include "math.hpp"
#include "RockColumn.hpp"
#include "Plate.hpp"
#include "World.hpp"

namespace WorldBuilder {
    
    // TODO, modify to take cell size into account
    void BombardmentGenerator::Generate(std::shared_ptr<World> theWorld){
        
        
        float prehistoricRootThickness = 120000 + 100000; // some continental, plus root to melt
        
        // create a nice smooth world
        auto plateIt = theWorld->get_plates().find(0);
        if (plateIt != theWorld->get_plates().end()) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++)
            {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                cell->rock.root.density = 3200;
                cell->rock.root.thickness = prehistoricRootThickness;
            }
            // create our impact crater vector, crater radii
            std::vector<float> impacts;
            // big boys
            impacts.push_back(2500 / theWorld->get_attributes().radius);
            impacts.push_back(2000 / theWorld->get_attributes().radius);
            impacts.push_back(1500 / theWorld->get_attributes().radius);
            impacts.push_back(1000 / theWorld->get_attributes().radius);
            // heavy ~500km diameter LHB had ~40 for earth
            for (size_t count = 0; count < 40; count++) {
                // TODO, limit size so we don't randomly destroy everying
                float radius = this->randomSource->randomNormal(500, 100);
                if (radius > 800) {
                    radius = 800;
                } else if (radius < 200) {
                    radius = 200;
                }
                impacts.push_back(radius / theWorld->get_attributes().radius);
            }
            // large number of smaller impacts
            for (size_t count = 0; count < 5000; count++) {
                // TODO Try with lognormal distribution or gamma distribution
                float radius = this->randomSource->randomNormal(200, 50);
                if (radius > 500) {
                    radius = 500;
                } else if (radius < 50){
                    radius = 50;
                }
                impacts.push_back(radius / theWorld->get_attributes().radius);
            }
            
            std::vector<float> materialValue;
            std::vector<PlateCell*> ejectaCells;
            for (std::vector<float>::iterator impactRadius = impacts.begin();
                 impactRadius != impacts.end();
                 impactRadius++)
            {
                Vec3 randomPoint = this->randomSource->getRandomPointUnitSphere();
                float craterRadius = (*impactRadius) * theWorld->get_attributes().radius*1000;
                
                RockColumn materialEjected;
                materialEjected.sediment.thickness = 0;
                materialEjected.continental.thickness = 0;
                materialEjected.oceanic.thickness = 0;
                materialEjected.root.thickness = 0;
                materialEjected.sediment.density = 1;
                materialEjected.continental.density = 1;
                materialEjected.oceanic.density = 1;
                materialEjected.root.density = 1;
                
                // TODO, MAKE BETER!
                size_t tilesWithin = 0;
                size_t tilesWithout = 0;
                for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++)
                {
                    std::shared_ptr<PlateCell> cell = cellIt->second;
                    float distance = math::distanceBetween3Points(randomPoint, cell->get_vertex()->get_vector());
                    // for now just remove material in a symetrical manner
                    if (distance < (*impactRadius)) {
                        tilesWithin++;
                        distance = distance * theWorld->get_attributes().radius * 1000; // to meters
                        // 10^6 for km^2 to m^2
                        float materialChange = -1*(distance*distance * 0.05 / craterRadius - (craterRadius * 0.05));
                        if (materialChange > cell->rock.thickness()) {
                            materialChange = cell->rock.thickness();
                        } else if (materialChange < 0){
                            std::printf("UH OH!!!");
                        }
                        materialEjected = accreteColumns(materialEjected, cell->rock.removeThickness(materialChange));
                    } else if (distance < 2*(*impactRadius)){
                        ejectaCells.push_back(&*cell);
                        tilesWithout++;
                    }
                }
                // place material (1/4 as much per tile as removed?)
                RockSegment continentalConversion;
                continentalConversion.thickness = (materialEjected.root.thickness / 2) * materialEjected.root.density / 2700;
                continentalConversion.density = 2700;
                
                materialEjected.continental = combineSegments(materialEjected.continental, continentalConversion);
                materialEjected.root.thickness = materialEjected.root.thickness / 2;
                
                float totalPercent = 0;
                
                for (std::vector<PlateCell*>::iterator cell = ejectaCells.begin();
                     cell != ejectaCells.end();
                     cell++)
                {
                    // from r to 2r x*( 3/(r*sqrt(2*pi))*( e^-((x-r)^2/(2*r^2/9)) ) )^2 integrates to 0.50270049114536518
                    // at a rotation about the y axis
                    //const float ejectaFactor = 1 / 0.50270049114536518;
                    float distance = math::distanceBetween3Points(randomPoint, (*cell)->get_vertex()->get_vector());
                    distance = distance * theWorld->get_attributes().radius * 1000; //to meters
                    
                    // exponential, starts with cliff edge however
                    float norm = (expf(-1*((distance - craterRadius)*(distance - craterRadius)/(2*craterRadius*craterRadius/9))));
                    // gamma, looks funny
                    //float k = 2.0;
                    //float omega = (craterRadius)/9;
                    //float norm = 1/(powf(omega, k))*powf(distance - craterRadius, k - 1)*expf(-1*(distance - craterRadius)/omega);
                    /*float distributionFactor = (3 / (craterRadius*sqrtf(2*M_PI))*norm*norm);
                     float areaFactor = (M_PI*(2*craterRadius)*(2*craterRadius) - M_PI*craterRadius*craterRadius) / (theWorld->get_cellDistanceMeters() * theWorld->get_cellDistanceMeters());
                     float materialPercentage = distributionFactor * ejectaFactor * tilesWithout;*/
                    materialValue.push_back(norm);
                    totalPercent += norm;
                }
                float thicknessAdded = 0;
                size_t index = 0;
                for (std::vector<PlateCell*>::iterator cell = ejectaCells.begin();
                     cell != ejectaCells.end();
                     cell++, index++)
                {
                    float materialPercentage = materialValue[index] / totalPercent;
                    
                    RockColumn addedRock;
                    //addedRock.sediment.density = materialEjected.sediment.density;
                    //addedRock.sediment.thickness = materialEjected.sediment.thickness * materialPercentage;
                    addedRock.continental.density = materialEjected.continental.density;
                    addedRock.continental.thickness = materialEjected.continental.thickness * materialPercentage;
                    //addedRock.oceanic.density = materialEjected.oceanic.density;
                    //addedRock.oceanic.thickness = materialEjected.oceanic.thickness * materialPercentage;
                    addedRock.root.density = materialEjected.root.density;
                    addedRock.root.thickness = materialEjected.root.thickness * materialPercentage;
                    
                    
                    (*cell)->rock = accreteColumns((*cell)->rock, addedRock);
                    
                    thicknessAdded += addedRock.continental.thickness+addedRock.root.thickness+addedRock.oceanic.thickness+addedRock.sediment.thickness;
                }
                
                //std::printf("Added to ejected: %e\n", thicknessAdded / materialEjected.thickness());
                ejectaCells.clear();
                materialValue.clear();
            }
            
            // flood low stuff with lava
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++)
            {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                cell->rock.root.thickness = cell->rock.root.thickness - 100000; // melt extra
                // check to see if root is too small, may want to melt from other cells
                if (cell->rock.root.thickness < theWorld->get_divergentOceanicColumn().root.thickness) {
                    cell->rock.root.thickness = theWorld->get_divergentOceanicColumn().root.thickness;
                }
                
                // fill in with laaava
                if (cell->rock.thickness() < theWorld->get_divergentOceanicColumn().thickness()) {
                    cell->rock = theWorld->get_divergentOceanicColumn();
                }
            }
        } else { // plate zero not found
            // something's fishy!
        }
    } // Generate
}