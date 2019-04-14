//
//  World.cpp
//  WorldGenerator
//
//

#include "World.hpp"

#include <iostream>
#include <limits>
#include <thread>
#include <algorithm>

namespace WorldBuilder {
    
    // per million years
    wb_float erosionRate(wb_float elevationAboveSealevel) {
        if (elevationAboveSealevel < 1000) {
            return 0.075;
        } else if (elevationAboveSealevel < 4000) {
            return 0.15;
        } else {
            return 0.30;
        }
    }
    
    // wants somewhere to go
    wb_float World::randomPlateDensityOffset() {
        return this->randomSource->randomNormal(0.0, 1.0);
    }
    
/****************************** Top Level Phases ******************************/
    WorldUpdateTask World::progressByTimestep(wb_float minTimestep) {
        WorldUpdateTask updateTask;
        wb_float timestep = minTimestep;
        
        if (this->plates.size() == 0) {
            return updateTask;
        }
        
        this->validate();
        
        // find a reasonable timestep based on fastest relative speeds
        wb_float fastestRelativePlateSpeed = 0; // fastest plate speeds
        for (auto plateIt = this->plates.begin(), end = this->plates.end(); plateIt != end; plateIt++) {
            std::shared_ptr<Plate>& plate = plateIt->second;
            auto testPlateIt = plateIt;
            testPlateIt++;
            for (; testPlateIt != end; testPlateIt++) {
                std::shared_ptr<Plate>& testPlate = testPlateIt->second;
                wb_float relativeSpeed = ((plate->pole * plate->angularSpeed) - (testPlate->pole * testPlate->angularSpeed)).length();
                if (fastestRelativePlateSpeed < relativeSpeed) {
                    fastestRelativePlateSpeed = relativeSpeed;
                }
            }
        }
        
        // timestep such that the fastest cells will overlap by no more than one full cell angle
        timestep = this->cellSmallAngle / fastestRelativePlateSpeed;
        // clamp between reasonable values
        if (timestep < minTimestep) {
            timestep = minTimestep;
        } else if (timestep > 10) {
            timestep = 10;
        }
        
        updateTask.timestepUsed = timestep;
        
        this->age = this->age + timestep;

        std::cout << "Advancing by: " << timestep << " to age: " << this->age << std::endl; 

        
        // movement phase
        RockColumn initial, final;
        initial = this->netRock();
        updateTask.movement.start();
        this->columnMovementPhase(timestep);
        updateTask.movement.end();
        final = this->netRock();
        // std::cout << "Rock change after movement:" << std::endl;
        // logColumnChange(initial, final, false, false);
        initial = final;
        
        // transistion phase
        updateTask.transition.start();
        this->transitionPhase(timestep);
        updateTask.transition.end();
        final = this->netRock();
        // std::cout << "Rock change after transition:" << std::endl;
        // logColumnChange(initial, final, false, false);
        initial = final;
        
        // modification phase
        updateTask.modification.start();
        this->columnModificationPhase(timestep);
        updateTask.modification.end();
        final = this->netRock();
        // std::cout << "Rock change after modification:" << std::endl;
        // logColumnChange(initial, final, false, false);
        
        return updateTask;
    }
    
    // top level movement phase
    void World::columnMovementPhase(wb_float timestep){
        // move our plates
        this->movePlates(timestep);
        
        // set up momentum
        this->momentumTracker = std::make_shared<AngularMomentumTracker>(this->plates);
        
        this->computeEdgeInteraction(timestep);
        
        
        // move them cells around a bunch!
        std::vector<std::thread> threads;
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            threads.push_back(std::thread(&World::balanceInternalPlateForce, this, plateIt->second, timestep));
        }
        for (auto threadIt = threads.begin(); threadIt != threads.end(); threadIt++) {
            threadIt->join();
        }
        
    }
    
    // top level transition phase
    void World::transitionPhase(wb_float timestep) {
        // normalize plate grid
        this->renormalizeAllPlates();
        
        // rifting
        std::vector<std::pair<std::shared_ptr<Plate>, std::vector<std::shared_ptr<PlateCell>>>> cellsToAddToPlates;
        for (auto&& plateIt : this->plates) {
            cellsToAddToPlates.push_back(std::make_pair(plateIt.second, this->riftPlate(plateIt.second)));
        }
        for (auto&& pairIt : cellsToAddToPlates) {
            for (auto&& cellAddIt : pairIt.second) {
                pairIt.first->cells.insert({cellAddIt->get_vertex()->get_index(), cellAddIt});
            }
        }
        
        // update momentum
        this->momentumTracker->commitTransfer();
        
        // delete plates that no longer exist
        std::vector<uint32_t> platesToDelete;
        for (auto&& plateIt : this->plates) {
            if (plateIt.second->cells.size() == 0) {
                platesToDelete.push_back(plateIt.first);
            }
        }
        for (auto&& plateIndex : platesToDelete) {
            this->plates.erase(plateIndex);
        }
        
        // split in supercontinent if needed
        this->supercontinentCycle();
        
        // update edges
        for (auto&& plateIt : this->plates) {
            this->updatePlateEdges(plateIt.second);
        }
        
        // knit back together
        for (auto&& plateIt : this->plates) {
            this->knitPlates(plateIt.second);
        }
        
        // float it!
        this->homeostasis(timestep);

        // update sealevel!
        this->updateSealevel();
        
        // update attributes for cells
        this->updatePrecipitation();
        this->updateTempurature();
    }
    
    // top level modification phase
    void World::columnModificationPhase(wb_float timestep){
        
        this->processAllHotspots(timestep);
        
        // thermal first
        // RockColumn initial, final;
        // initial = this->netRock();
        this->erodeThermalSmoothing(timestep);
        // final = this->netRock();
        // std::cout << " Change after thermal erosion: " << std::endl;
        // logColumnChange(initial, final, false, false);
        
        // initial = final;
        this->erodeSedimentTransport(timestep);
        // final = this->netRock();
        // std::cout << "Change after sediment transport: " << std::endl;
        // logColumnChange(initial, final, false, false);
    }
    
/****************************** Transistion ******************************/
    
    // adds new oceanic cells along plate boundaries where appropriate
    std::vector<std::shared_ptr<PlateCell>> World::riftPlate(std::shared_ptr<Plate> plate) {
        std::vector<std::shared_ptr<PlateCell>> cellsToAdd;
        
        std::vector<std::shared_ptr<Plate>> interactablePlates;
        for (auto&& plateIt : this->plates) {
            std::shared_ptr<Plate>& testPlate = plateIt.second;
            // ignore self
            if (testPlate != plate) {
                // test interacability between plates
                Matrix3x3 targetToTest = math::matrixMul(math::transpose(testPlate->rotationMatrix), plate->rotationMatrix);
                // if the plates max extent overlap, test edges
                Vec3 testCenter = math::affineRotaionMulVec(targetToTest, plate->center);
                wb_float testAngle = math::angleBetweenUnitVectors(testPlate->center, testCenter);
                if (testAngle < plate->maxEdgeAngle + testPlate->maxEdgeAngle || std::isnan(testAngle)) {
                    // some cells may interact, add to interactables
                    interactablePlates.push_back(testPlate);
                }
            }
        }
        
        // for each rifting target
        for(auto&& riftIndex : plate->riftingTargets) {
            bool cellFound = false;
            for (auto&& testPlate : interactablePlates) {
                // test interacability between plates
                Matrix3x3 targetToTest = math::matrixMul(math::transpose(testPlate->rotationMatrix), plate->rotationMatrix);
                // if the plates max extent overlap, test edges
                Vec3 riftInTest = math::affineRotaionMulVec(targetToTest, this->worldGrid->get_vertices()[riftIndex].get_vector());
                wb_float testAngle = math::angleBetweenUnitVectors(testPlate->center, riftInTest);
                if (testAngle < testPlate->maxEdgeAngle || std::isnan(testAngle)) {
                    // may interact
                    // could grab better hint from neighbor
                    uint32_t hint = 0;
                    if (testPlate->centerVertex != nullptr) {
                        hint = testPlate->centerVertex->get_index();
                    }
                    uint32_t indexInTest = this->getNearestGridIndex(riftInTest, hint);
                    
                    // check in cells
                    auto cellTestIt = testPlate->cells.find(indexInTest);
                    if (cellTestIt != testPlate->cells.end()) {
                        cellFound = true;
                    } else {
                        auto testRift = testPlate->riftingTargets.find(indexInTest);
                        if (testRift != testPlate->riftingTargets.end()) {
                            cellFound = true;
                        } else {
                            // check all neighbors
                            for(auto&& neighborIt : this->worldGrid->get_vertices()[indexInTest].get_neighbors()) {
                                uint32_t neighborIndex = neighborIt->get_index();
                                // only need to check rifting targets, as the nearest index would have been in rifting at least otherwise
                                auto testNeighborRift = testPlate->riftingTargets.find(neighborIndex);
                                if (testNeighborRift != testPlate->riftingTargets.end()) {
                                    cellFound = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (cellFound) {
                    break;
                }
            }
            if (!cellFound) {
                // create oceanic and add to plate
                std::shared_ptr<PlateCell> riftedCell = std::make_shared<PlateCell>(&this->worldGrid->get_vertices()[riftIndex]);
                riftedCell->rock = this->divergentOceanicColumn;
                
                cellsToAdd.push_back(riftedCell);
            }
        }
        return cellsToAdd;
    }
    
    // recalculates which plate cells are on the edge of the plate
    // updates a plate's center estimate
    // could be moved to the Plate class
    void World::updatePlateEdges(std::shared_ptr<Plate> plate) {
        // clear the edgeCells
        plate->edgeCells.clear();
        plate->riftingTargets.clear();
        
        // TODO replace with a smallest bounding circle implementation
        
        // find the center of the plate, hope it's close tot where the smallest bounding circle's would be
        Vec3 center;
        for (auto&& cellIt : plate->cells) {
            std::shared_ptr<PlateCell> cell = cellIt.second;
            
            
            
            Vec3 cellVec = cell->get_vertex()->get_vector();
            center = center + cellVec;
            
            // determine edges
            bool isEdge = false;
            for (auto&& neighborIt : cell->get_vertex()->get_neighbors()) {
                uint32_t index = neighborIt->get_index();
                
                // test if index is in plate
                auto neighborCellIt = plate->cells.find(index);
                if (neighborCellIt == plate->cells.end()) {
                    isEdge = true;
                    // add to rifting targets, keep looping to get the remaining rifting targest
                    plate->riftingTargets.insert(index);
                }
            }
            if (isEdge) {
                // create our EdgeCellInfo if needed
                if (cell->edgeInfo == nullptr) {
                    cell->edgeInfo = std::make_shared<EdgeCellInfo>();
                } else {
                    // clear neighbors, they need to be updated when plates are knit
                    cell->edgeInfo->otherPlateNeighbors.clear();
                }
                // add to the plate edge container
                plate->edgeCells.insert({cell->get_vertex()->get_index(), cell});
            } else {
                // delete edge data
                cell->edgeInfo = nullptr;
            }
        }
        
        //std::cout << "Edge cell count of " << plate->edgeCells.size() << " for plate with " << plate->cells.size() << " cells." << std::endl;
        
        // add center to plate
        plate->center = math::normalize3Vector(center);
        
        // find nearest index
        uint32_t hint = 0;
        if (plate->centerVertex != nullptr) {
            hint = plate->centerVertex->get_index();
        }
        plate->centerVertex = &this->worldGrid->get_vertices()[this->getNearestGridIndex(plate->center, hint)];
        
        // find max distance
        plate->maxEdgeAngle = 0; // reset max distance
        for (auto&& edgeIt : plate->edgeCells) {
            std::shared_ptr<PlateCell> edge = edgeIt.second;
            wb_float testDistance = math::angleBetweenUnitVectors(plate->center, edge->get_vertex()->get_vector());
            if (testDistance > plate->maxEdgeAngle || std::isnan(testDistance)) {
                plate->maxEdgeAngle = testDistance;
            }
        }
        // add a bit to the max edge angle to capture cells withing one breadth of the rifing targets
        plate->maxEdgeAngle = plate->maxEdgeAngle + 2 * this->cellSmallAngle;
    }
    
    // knits the edges of plates together so the edge cells can interact
    void World::knitPlates(std::shared_ptr<Plate> plate) {

        // logging vars
        uint32_t offEdgeCount, connections;
        
        for (auto&& plateIt : this->plates) {
            std::shared_ptr<Plate> testPlate = plateIt.second;
            // ignore self
            if (testPlate != plate) {
                // test interacability between plates
                Matrix3x3 targetToTest = math::matrixMul(math::transpose(testPlate->rotationMatrix), plate->rotationMatrix);
                // if the plates max extent overlap, test edges
                Vec3 testCenter = math::affineRotaionMulVec(targetToTest, plate->center);
                wb_float testAngle = math::angleBetweenUnitVectors(testPlate->center, testCenter);
                if (testAngle < plate->maxEdgeAngle + testPlate->maxEdgeAngle || std::isnan(testAngle)) {
                    // some cells may interact
                    for (auto&& edgeIt : plate->edgeCells) {
                        std::shared_ptr<PlateCell> edgeCell = edgeIt.second;
                        Vec3 targetEdgeInTest = math::affineRotaionMulVec(targetToTest, edgeCell->get_vertex()->get_vector());
                        wb_float angleToCenter = math::angleBetweenUnitVectors(testPlate->center, targetEdgeInTest);
                        if (angleToCenter < testPlate->maxEdgeAngle || std::isnan(angleToCenter)) {
                            // edge cell may be close enough to interact
                            uint32_t lastNearestForPlate = 0;
                            auto lastNearestIt = edgeCell->edgeInfo->otherPlateLastNearest.find(testPlate->id);
                            if (lastNearestIt != edgeCell->edgeInfo->otherPlateLastNearest.end()) {
                                // set hint to last known
                                lastNearestForPlate = lastNearestIt->second;
                            }
                            
                            uint32_t nearestGridIndex = getNearestGridIndex(targetEdgeInTest, lastNearestForPlate);
                            // set our last nearest, even if it isn't an edge
                            edgeCell->edgeInfo->otherPlateLastNearest[testPlate->id] = nearestGridIndex;
                            
                            // check if it is an edge, and neighbors
                            auto testEdgeIt = testPlate->cells.find(nearestGridIndex);
                            if (testEdgeIt != testPlate->cells.end()) {
                                std::shared_ptr<PlateCell> testEdge = testEdgeIt->second;
                                uint64_t neighborKey;
                                neighborKey = ((uint64_t)testPlate->id << 32) + nearestGridIndex;
                                wb_float neighborDistance = math::distanceBetween3Points(targetEdgeInTest, testEdge->get_vertex()->get_vector());
                                EdgeNeighbor edgeData;
                                edgeData.plateIndex = testPlate->id;
                                edgeData.cellIndex = nearestGridIndex;
                                edgeData.distance = neighborDistance;
                                edgeCell->edgeInfo->otherPlateNeighbors[neighborKey] = edgeData; //neighborDistance;

                                connections++;
                            }
                            
                            // loop over neighbors
                            for (auto neighborIt = this->worldGrid->get_vertices()[nearestGridIndex].get_neighbors().begin(); neighborIt != this->worldGrid->get_vertices()[nearestGridIndex].get_neighbors().end(); neighborIt++)
                            {
                                uint32_t index = (*neighborIt)->get_index();
                                testEdgeIt = testPlate->cells.find(index);
                                if (testEdgeIt != testPlate->cells.end()) {
                                    std::shared_ptr<PlateCell> testEdge = testEdgeIt->second;
                                    uint64_t neighborKey;
                                    neighborKey = ((uint64_t)testPlate->id << 32) + index;
                                    wb_float neighborDistance = math::distanceBetween3Points(targetEdgeInTest, testEdge->get_vertex()->get_vector());
                                    EdgeNeighbor edgeData;
                                    edgeData.plateIndex = testPlate->id;
                                    edgeData.cellIndex = index;
                                    edgeData.distance = neighborDistance;
                                    edgeCell->edgeInfo->otherPlateNeighbors[neighborKey] = edgeData;

                                    connections++;
                                }
                            }
                        }
                    }
                }
            }
        }
        // calculate off edge cells
        for (auto&& edgeIt : plate->edgeCells) {
            std::shared_ptr<PlateCell> edgeCell = edgeIt.second;
            for (auto& neighborIt : this->worldGrid->get_vertices()[edgeCell->vertex->get_index()].get_neighbors()) {
                auto testEdgeIt = plate->cells.find(neighborIt->get_index());
                if (testEdgeIt == plate->cells.end()) {
                    offEdgeCount++;
                }
            }
        }

        // print knit stats for plate
        std::cout << "Missing edges: " << offEdgeCount << " with other plate connections: " << connections << std::endl;
    }
    
    // renormalize plates and transfer rock from cells that are too thin
    void World::renormalizeAllPlates() {
        // renormalize all plates
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            this->renormalizePlate(plateIt->second);
        }
        
        // move rock from destroyed cells to targets, rock will now be on the plate itself (no longer in the displacementinfo
        // currently assumed only edge cells could be deleted
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            std::vector<std::shared_ptr<PlateCell>> cellsToDelete;
            for (auto cellIt = plate->edgeCells.begin(); cellIt != plate->edgeCells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                if (cell->displacement != nullptr && cell->displacement->deleteTarget != nullptr) {
                    cellsToDelete.push_back(cell);
                    // only move sediment if age is less than min
                    if (cell->age < min_interaction_age) {
                        cell->displacement->deleteTarget->rock.sediment = combineSegments(cell->displacement->deleteTarget->rock.sediment, cell->rock.sediment);
                        cell->displacement->deleteTarget->rock.continental = combineSegments(cell->displacement->deleteTarget->rock.continental, cell->rock.continental);
                    } else {
                        cell->displacement->deleteTarget->rock = accreteColumns(cell->displacement->deleteTarget->rock, cell->rock);
                    }
                }
            }
            int deleteCount = 0;
            for (auto deleteIt = cellsToDelete.begin(); deleteIt != cellsToDelete.end(); deleteIt++) {
                (*deleteIt)->displacement = nullptr;
                // skip erasing from edges, as that map gets cleared next
                plate->cells.erase((*deleteIt)->get_vertex()->get_index());
                
                deleteCount++;
            }
            //std::cout << "Deleted " << deleteCount << " cells for plate " << plateIt->second->id << "." << std::endl;
        }
        
        // clear displaced?
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                cell->displacement = nullptr;
            }
        }
    }
    
    // redistributes rock such that cells once again lay along the origional grid
    // could be moved to the Plate class
    void World::renormalizePlate(std::shared_ptr<Plate> plate) {
        // if a cell has been moved, the rock needs to be copied to the displaced info section
        for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            if (cell->displacement != nullptr) {
                cell->displacement->displacedRock = cell->rock;
                RockSegment zeroSegment(0,1);
                cell->rock.sediment = zeroSegment;
                cell->rock.continental = zeroSegment;
                cell->rock.oceanic = zeroSegment;
                cell->rock.root = zeroSegment;
            }
        }
        
        // move rock from each displaced cell based on weighted overlap
        std::vector<std::pair<std::shared_ptr<PlateCell>, wb_float>> weights;
        wb_float totalWeight;
        int totalDisplaced = 0;
        for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            if (cell->displacement != nullptr) {
                totalDisplaced++;
                // find the weights of each overlapping cell
                weights.clear();
                totalWeight = 0;
                
                // find the normalized new location
                Vec3 cellLocation = math::normalize3Vector(cell->get_vertex()->get_vector() + cell->displacement->displacementLocation);
                
                // find the nearest index
                uint32_t nearestIndex = this->getNearestGridIndex(cellLocation, cell->get_vertex()->get_index());
                const GridVertex* nearestVertex = &this->worldGrid->get_vertices()[nearestIndex];
                
                // find weights for nearest and each neighbors
                // can't trust the world cell size estimate until more uniform grid is created, but radius should be roughly the same for nearby cells
                wb_float cellRadius = math::distanceBetween3Points(nearestVertex->get_vector(), nearestVertex->get_neighbors()[0]->get_vector()) / 2;
                // check the nearest is in the plate
                auto targetCellIt = plate->cells.find(nearestIndex);
                if (targetCellIt != plate->cells.end()) {
                    std::shared_ptr<PlateCell> targetCell = targetCellIt->second;
                    // weight with nearest
                    wb_float weight = math::circleIntersectionArea(math::distanceBetween3Points(targetCell->get_vertex()->get_vector(), cellLocation), cellRadius);
                    totalWeight += weight;
                    weights.push_back(std::make_pair(targetCell, weight));
                }
                // each neighbor
                for (auto neighborIt = nearestVertex->get_neighbors().begin(); neighborIt != nearestVertex->get_neighbors().end(); neighborIt++) {
                    targetCellIt = plate->cells.find((*neighborIt)->get_index());
                    if (targetCellIt != plate->cells.end()) {
                        std::shared_ptr<PlateCell> targetCell = targetCellIt->second;
                        // weight with neighbor
                        wb_float weight = math::circleIntersectionArea(math::distanceBetween3Points(targetCell->get_vertex()->get_vector(), cellLocation), cellRadius);
                        totalWeight += weight;
                        weights.push_back(std::make_pair(targetCell, weight));
                    }
                }
                
                // move rock based on weights
                if (totalWeight <= 0) {
                    throw std::logic_error("Cell moved to invalid location (likely outside of edge border).");
                }
                for (auto destinationIt = weights.begin(); destinationIt != weights.end(); destinationIt++) {
                    std::shared_ptr<PlateCell> destinationCell = destinationIt->first;
                    wb_float destinationWeight = destinationIt->second;
                    RockColumn moveColumn;
                    // set densities
                    moveColumn.sediment.set_density(cell->displacement->displacedRock.sediment.get_density());
                    moveColumn.continental.set_density(cell->displacement->displacedRock.continental.get_density());
                    moveColumn.oceanic.set_density(cell->displacement->displacedRock.oceanic.get_density());
                    moveColumn.root.set_density(cell->displacement->displacedRock.root.get_density());
                    
                    // set thicknesses
                    moveColumn.sediment.set_thickness(cell->displacement->displacedRock.sediment.get_thickness() * (destinationWeight / totalWeight));
                    moveColumn.continental.set_thickness(cell->displacement->displacedRock.continental.get_thickness() * (destinationWeight / totalWeight));
                    moveColumn.oceanic.set_thickness(cell->displacement->displacedRock.oceanic.get_thickness() * (destinationWeight / totalWeight));
                    moveColumn.root.set_thickness(cell->displacement->displacedRock.root.get_thickness() * (destinationWeight / totalWeight));
                    
                    // combine with destination
                    destinationCell->rock = accreteColumns(destinationCell->rock, moveColumn);
                }
            }
        }
        
        //std::cout << "Displaced " << totalDisplaced << " cells out of " << plate->cells.size() << std::endl;
    }
    
    /*************** Homeostasis ***************/
    void World::homeostasis(wb_float timestep){
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++)
        {
            std::shared_ptr<Plate> plate = plateIt->second;
            plate->homeostasis(this->attributes, timestep);
        }
    }

    void World::updateSealevel() {
        std::vector<std::shared_ptr<PlateCell>> cells;
        // compute size
        unsigned long size = 0;
        for (auto&& plateIt : this->plates) {
            auto plate = plateIt.second;
            size += plate->cells.size();
        }
        cells.reserve(size);

        // add cells to vector
        for (auto&& plateIt : this->plates) {
            auto plate = plateIt.second;
            for (auto&& cellIt : plate->cells) {
                cells.push_back(cellIt.second);
            }
        }

        // sort the thing
        std::sort(cells.begin(), cells.end(), 
            [](const std::shared_ptr<PlateCell> a, const std::shared_ptr<PlateCell> b) -> bool
        {
            return a->get_elevation() < b->get_elevation();
        });

        // loop through until we've used up all the water
        wb_float currentWater = 0;
        wb_float currentElevation = cells[0]->get_elevation();
        //std::cout << "Starting fill elevation: " << currentElevation << std::endl;
        unsigned long seaCells = 0;
        for (auto&& cell : cells) {
            wb_float nextElevation = cell->get_elevation();
            wb_float nextWater = currentWater + (seaCells * (nextElevation - currentElevation));
            if (nextWater > this->attributes.totalSeaDepth) {
                // compute exact level
                wb_float remainingDepth = this->attributes.totalSeaDepth - currentWater;
                currentElevation = currentElevation + remainingDepth / seaCells;
                break;
            }
            currentElevation = nextElevation;
            currentWater = nextWater;
            seaCells++;
        }

        //std::cout << "Ending fill elevation: " << currentElevation << std::endl;

        // set the sea level
        this->attributes.sealevel = currentElevation;
        //std::cout << "Current water: " << currentWater << " Total Water: " << this->attributes.totalSeaDepth << std::endl;
    }
    
    void World::updateTempurature() {
        Vec3 northPole;
        northPole.coords[2] = 1;
        
        // loop through all plate cells
        for (auto&& plateIt : this->plates) {
            auto plate = plateIt.second;
            for (auto&& cellIt : plate->cells) {
                auto cell = cellIt.second;
                wb_float latitude = std::abs(math::piOverTwo - math::angleBetweenUnitVectors(plate->localToWorld(cell->get_vertex()->get_vector()), northPole));
                wb_float elevation = cell->get_elevation() - this->attributes.sealevel;
                if (elevation < 0) {
                    elevation = 0;
                }
                // 25*(cos(2*latitude) + 0.4)
                // lapse rate estimate of 5C/1000 meters above sealevel
                cell->tempurature = 25*(std::cos(2*latitude) + 0.4) - 5 * elevation / 1000;
            }
        }
        
    } // World::updateTempurature
    
    void World::updatePrecipitation() {
        Vec3 northPole;
        northPole.coords[2] = 1;
        
        // loop through all plate cells
        for (auto&& plateIt : this->plates) {
            auto plate = plateIt.second;
            for (auto&& cellIt : plate->cells) {
                auto cell = cellIt.second;
                wb_float latitude = std::abs(math::piOverTwo - math::angleBetweenUnitVectors(plate->localToWorld(cell->get_vertex()->get_vector()), northPole));
                // e^(-(x)^2/(2 *0.6^2))/(sqrt(2*π) * 0.6) + 1/20*e^(-(x - 5/9*pi/2)^2/(2 * 0.1^2))/(sqrt(2*π) * 0.1)
                // simplifies to 0.199471 e^(-50. (0.872665 - x)^2) + 0.664904 e^(-1.38889 x^2)
                cell->precipitation = 6.5*(0.199471 * std::exp(-50.0 * (0.872665 - latitude) * (0.872665 - latitude)) + 0.664904 * std::exp(-1.38889 * latitude * latitude));
            }
        }
    } // World::updatePrecipitation
    
    
    /*************** Supercontinent Cycle ***************/
    // for splitting of large plates
    void World::supercontinentCycle(){
        // for cycle if plates have eaten each other (want more than two)
        if (this->age > this->supercontinentCycleDuration + this->supercontinentCycleStartAge || this->plates.size() <= 2) {
            this->supercontinentCycleStartAge = this->age;
            this->supercontinentCycleDuration = this->randomSource->randomNormal(300, 200);
            
            
            std::vector<size_t> plateSizes;
            while (this->plates.size() < this->desiredPlateCount) {
                size_t totalPlateSize = 0;
                // get total plate sizes, don't create random number yet, as total size may not
                // equal the size of world cells, but should be close
                for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++)
                {
                    std::shared_ptr<Plate> plate = plateIt->second;
                    size_t plateSize = plate->cells.size();
                    totalPlateSize += plateSize;
                    plateSizes.push_back(plateSize);
                }
                // get random index of plate to split
                uint32_t plateIndexToSplit = this->randomSource->randomUniform(0, totalPlateSize - 1);
                std::shared_ptr<Plate> plateToSplit;
                size_t currentTotalSize = 0;
                size_t index = 0;
                bool plateFound = false;
                for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++, index++)
                {
                    std::shared_ptr<Plate> plate = plateIt->second;
                    currentTotalSize += plateSizes[index];
                    if (currentTotalSize > plateIndexToSplit) {
                        plateFound = true;
                        plateToSplit = plate;
                    }
                }
                
                if (plateFound) {
                    std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> newPlates = this->splitPlate(plateToSplit);
                    this->deletePlate(plateToSplit); // TODO potential memory leak
                    this->plates.insert({newPlates.first->id, newPlates.first});
                    this->plates.insert({newPlates.second->id, newPlates.second});
                } else {
                    throw "no plates to split?!?!";
                }
                
                // clear plate sizes for next run
                plateSizes.clear();
            }
        }
    }
    
    // splits a plate along a triple point such that the smaller angle around the split vertex is 2/3PI
    // could be moved to Plate class
    std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> World::splitPlate(std::shared_ptr<Plate> plateToSplit){
        std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> newPlates;
        newPlates.first = std::make_shared<Plate>(0, this->nextPlateId()); // large
        newPlates.second = std::make_shared<Plate>(0, this->nextPlateId()); // small
        
        newPlates.first->rotationMatrix = plateToSplit->rotationMatrix;
        newPlates.second->rotationMatrix = plateToSplit->rotationMatrix;
        
        // create poles and speeds
        newPlates.first->pole = this->randomSource->getRandomPointUnitSphere();
        newPlates.second->pole = this->randomSource->getRandomPointUnitSphere();
        
        newPlates.first->angularSpeed = this->randomPlateSpeed();
        newPlates.second->angularSpeed = this->randomPlateSpeed();
        
        newPlates.first->densityOffset = this->randomPlateDensityOffset();
        newPlates.second->densityOffset = this->randomPlateDensityOffset();
        
        Vec3 smallCenter, largeCenter1, largeCenter2;
        std::tie(smallCenter, largeCenter1, largeCenter2) = this->getSplitPoints(plateToSplit);
        
        
        // check distances
        float distanceSmall, distanceLarge1, distanceLarge2;
        for (auto cellIt = plateToSplit->cells.begin(); cellIt != plateToSplit->cells.end(); cellIt++)
        {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            uint32_t index = cellIt->first;
            
            distanceSmall = math::squareDistanceBetween3Points(smallCenter, cell->get_vertex()->get_vector());
            // could test distanceSmall in circle known to be closer
            distanceLarge1 = math::squareDistanceBetween3Points(largeCenter1, cell->get_vertex()->get_vector());
            if (distanceSmall > distanceLarge1) {
                // copy rock, subduction
                newPlates.first->cells[index] = cell;
            } else {
                distanceLarge2 = math::squareDistanceBetween3Points(largeCenter2, cell->get_vertex()->get_vector());
                if (distanceSmall > distanceLarge2) {
                    // copy rock, subduction
                    newPlates.first->cells[index] = cell;
                } else {
                    // closer to small
                    // copy rock, subduction
                    newPlates.second->cells[index] = cell;
                }
            }
        }
        
        
        return newPlates;
    }
    
    // finds a viable triple point for the Plate, prefering continental cells
    std::tuple<Vec3, Vec3, Vec3> World::getSplitPoints(std::shared_ptr<Plate> plateToSplit){
        const GridVertex* firstVertex = this->getRandomContinentalVertex(plateToSplit);
        if (firstVertex == nullptr) {
            throw "No valid cells in plate!";
        }
        
        // get a random neighbor
        const GridVertex* secondVertex = firstVertex->get_neighbors()[this->randomSource->randomUniform(0, firstVertex->get_neighbors().size() - 1)];
        
        // find shared neighbors
        std::vector<const GridVertex*> sharedNeighbors;
        // TODO, could be opitmized to break inner once match found
        for (std::vector<const GridVertex*>::const_iterator testOne = firstVertex->get_neighbors().begin();
             testOne != firstVertex->get_neighbors().end();
             testOne++)
        {
            for (std::vector<const GridVertex*>::const_iterator testTwo = secondVertex->get_neighbors().begin();
                 testTwo != secondVertex->get_neighbors().end();
                 testTwo++)
            {
                if (*testOne == *testTwo) {
                    sharedNeighbors.push_back(*testOne);
                }
            }
        }
        
#ifdef DEBUG
        if (sharedNeighbors.size() == 0) {
            throw "no shared neighbors!!!!!";
        }
#endif
        const GridVertex* thirdVertex = sharedNeighbors[this->randomSource->randomUniform(0, sharedNeighbors.size() - 1)];
        
        return std::make_tuple(firstVertex->get_vector(), secondVertex->get_vector(), thirdVertex->get_vector());
    }
    
    // for splitting only
    // should be renamed to reflect that oceanic cells can be returned if no continental available
    const GridVertex* World::getRandomContinentalVertex(std::shared_ptr<Plate> plateToSplit){
        std::vector<std::shared_ptr<PlateCell>> continentalCells;
        std::vector<std::shared_ptr<PlateCell>> oceanicCells;
        for (auto plateCellIt = plateToSplit->cells.begin();
             plateCellIt != plateToSplit->cells.end();
             plateCellIt++)
        {
            std::shared_ptr<PlateCell> cell = plateCellIt->second;
            if (cell->isContinental()) {
                continentalCells.push_back(cell);
            } else if (!cell->isSubducted() && !cell->rock.isEmpty()){
                oceanicCells.push_back(cell);
            }
        }
        
        if (continentalCells.size() > 0) {
            return continentalCells[this->randomSource->uniformIndex(0, continentalCells.size() - 1)]->get_vertex();
        } else if (oceanicCells.size() > 0) {
            return oceanicCells[this->randomSource->uniformIndex(0, oceanicCells.size() - 1)]->get_vertex();
        } else {
            // plate has no cells?????
            return nullptr;
        }
    }
    
    void World::deletePlate(std::shared_ptr<Plate> plate) {
        this->plates.erase(plate->id);
    }
    
/****************************** Modification ******************************/
    /*************** Erosion ***************/
    // simple smoothing erosion, rock moved to neighbors based on height difference
    void World::erodeThermalSmoothing(wb_float timestep) {
        
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                
                // create sediement
                wb_float activeElevation = cell->get_elevation();
                wb_float elevationAboveSealevel = activeElevation - this->attributes.sealevel;
                // move some stuff to lower elevation cells
                wb_float erosionFactor = elevationAboveSealevel / (4000);
                wb_float erosionHeight = 0;
                if (erosionFactor > 0) {
                    // add erosion to self as sediment
                    erosionFactor = erosionFactor*erosionFactor*erosionFactor/400;
                    // limit to .1
                    if (erosionFactor > 0.5) {
                        erosionFactor = 0.5;
                    }
                    erosionHeight = erosionFactor * timestep * (activeElevation - this->attributes.sealevel);
                    RockSegment erodedSegment = cell->erodeThickness(erosionHeight);
                    cell->rock.sediment = combineSegments(cell->rock.sediment, erodedSegment);
                }
                
                // caluclate neighbor count so we know what fraction to move to each
                wb_float neighborCount = 0;
                for (auto neighborIndexIt = cell->get_vertex()->get_neighbors().begin(); neighborIndexIt != cell->get_vertex()->get_neighbors().end(); neighborIndexIt++) {
                    auto neighborIt = plate->cells.find((*neighborIndexIt)->get_index());
                    if (neighborIt != plate->cells.end()) {
                        neighborCount++;
                    }
                }
                if (cell->edgeInfo != nullptr){
                    neighborCount += cell->edgeInfo->otherPlateNeighbors.size();
                }
                
                // move to neighbors
                for (auto neighborIndexIt = cell->get_vertex()->get_neighbors().begin(); neighborIndexIt != cell->get_vertex()->get_neighbors().end(); neighborIndexIt++) {
                    auto neighborIt = plate->cells.find((*neighborIndexIt)->get_index());
                    if (neighborIt != plate->cells.end()) {
                        std::shared_ptr<PlateCell> neighborCell = neighborIt->second;
                        wb_float neighborElevation = neighborCell->get_elevation();
                        if (activeElevation > neighborElevation) {
                            erosionHeight = (activeElevation - neighborElevation) * erosionRate(elevationAboveSealevel) * timestep / neighborCount;
                            
                            // erode from this cell
                            RockSegment erodedSegment = cell->erodeThickness(erosionHeight);
                            
                            // add to neighbor
                            neighborCell->rock.sediment = combineSegments(neighborCell->rock.sediment, erodedSegment);
                        }
                    }
                }
                // and to edge neighbors
                if (cell->edgeInfo != nullptr){
                    for (auto neighborIndexIt = cell->edgeInfo->otherPlateNeighbors.begin(); neighborIndexIt != cell->edgeInfo->otherPlateNeighbors.end(); neighborIndexIt++) {
                        auto neighborPlateIt = this->plates.find(neighborIndexIt->second.plateIndex);
                        if (neighborPlateIt != this->plates.end()){
                            std::shared_ptr<Plate> neighborPlate = neighborPlateIt->second;
                            auto neighborIt = neighborPlate->cells.find(neighborIndexIt->second.cellIndex);
                            if (neighborIt != neighborPlate->cells.end()) {
                                std::shared_ptr<PlateCell> neighborCell = neighborIt->second;
                                wb_float neighborElevation = neighborCell->get_elevation();
                                if (activeElevation > neighborElevation) {
                                    erosionHeight = (activeElevation - neighborElevation) * erosionRate(elevationAboveSealevel) * timestep / neighborCount;
                                    
                                    // erode from this cell
                                    RockSegment erodedSegment = cell->erodeThickness(erosionHeight);
                                    
                                    // add to neighbor
                                    neighborCell->rock.sediment = combineSegments(neighborCell->rock.sediment, erodedSegment);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // flow graph water erosion with basin filling
    void World::erodeSedimentTransport(wb_float timestep){
        // sediment flow, does this want to be first???
        std::shared_ptr<MaterialFlowGraph> flowGraph = this->createFlowGraph(); // TODO don't create each phase, reuse the memory!
        wb_float beforeTotal = flowGraph->totalMaterial();
        bool validWeights = flowGraph->checkWeights();
        flowGraph->flowAll();
        flowGraph->fillBasins();
        wb_float afterTotal = flowGraph->totalMaterial();
        
        //std::cout << "Graph volume changed by " << std::scientific << afterTotal - beforeTotal << " and is " << afterTotal / beforeTotal << " of origional." << std::endl;
        
        //std::cout << "Orphaned material Out: " << std::scientific << flowGraph->inTransitOutMaterial() << " In: " << std::scientific << flowGraph->inTransitInMaterial() << std::endl;
        
        // update sediment for each plate cell
        for (auto&& plateIt : this->plates) {
            std::shared_ptr<Plate>& plate = plateIt.second;
            for (auto&& cellIt : plate->cells) {
                std::shared_ptr<PlateCell>& cell = cellIt.second;
                RockSegment sediment(cell->flowNode->get_materialHeight(), 2700);
                cell->rock.sediment = sediment; // all sediment is tracked in the flow graph, not just what moved
            }
        }
    }
    
    // Creates a unified flow graph from the knit plates
    // TODO need to add timestep, at least to suspension amounts
    std::shared_ptr<MaterialFlowGraph> World::createFlowGraph(){
        size_t cellCount = 0;
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            cellCount += plateIt->second->cells.size();
        }
        
        std::shared_ptr<MaterialFlowGraph> graph = std::make_shared<MaterialFlowGraph>(cellCount);
        
        // set initial elevations
        size_t nodeIndex = 0;
        for (auto&& plateIt : this->plates) {
            for (auto&& cellIt : plateIt.second->cells) {
                std::shared_ptr<PlateCell>& cell = cellIt.second;
                graph->nodes[nodeIndex].set_materialHeight(cell->rock.sediment.get_thickness());
                graph->nodes[nodeIndex].offsetHeight = cell->get_elevation() - graph->nodes[nodeIndex].get_materialHeight();
                cell->flowNode = &graph->nodes[nodeIndex];
                nodeIndex++;
            }
        }
        
        //std::cout << "Cell count: " << cellCount << " node Count: " << nodeIndex << std::endl;
        
        // build from node heights
        std::vector<std::pair<MaterialFlowNode*, wb_float>> outflowCandidates;
        // find outflow only, inflow set from outflow nodes
        size_t plateEdges = 0;
        size_t edgeCellCount = 0;
        for (auto&& plateIt : this->plates) {
            std::shared_ptr<Plate>& plate = plateIt.second;
            for (auto&& cellIt : plate->cells) {
                std::shared_ptr<PlateCell>& cell = cellIt.second;
                // we grab the elevations from the flow graph, incase a different module wants to modify the elevatsion while sediment transport is in progress
                wb_float elevation = cell->flowNode->elevation();
                
                // find outflow candidates
                wb_float largestHeightDifference = 0; // determines suspended material
                bool hasOutflow = false;
                // candidates from within the plate
                for (auto&& neighborIndexIt : cell->get_vertex()->get_neighbors()) {
                    auto neighborIt = plate->cells.find(neighborIndexIt->get_index());
                    if (neighborIt != plate->cells.end()) {
                        std::shared_ptr<PlateCell>& neighborCell = neighborIt->second;
                        wb_float heightDifference = elevation - (neighborCell->flowNode->elevation());
                        if (heightDifference > float_epsilon) {
                            // downhill node found, take note
                            outflowCandidates.push_back(std::make_pair(neighborCell->flowNode, heightDifference));
                            if (heightDifference > largestHeightDifference) {
                                largestHeightDifference = heightDifference;
                            }
                            hasOutflow = true;
                        } else if (std::abs(heightDifference) <= float_epsilon) {
                            cell->flowNode->equalNodes.insert(neighborCell->flowNode);
                        }
                    }
                }
                if (cell->edgeInfo != nullptr) {
                    edgeCellCount++;
                    // we are an edge, check other plate neighbors
                    for (auto&& neighborIndexIt : cell->edgeInfo->otherPlateNeighbors) {
                        auto neighborPlateIt = this->plates.find(neighborIndexIt.second.plateIndex);
                        if (neighborPlateIt != this->plates.end()){
                            std::shared_ptr<Plate>& neighborPlate = neighborPlateIt->second;
                            auto neighborIt = neighborPlate->cells.find(neighborIndexIt.second.cellIndex);
                            if (neighborIt != neighborPlate->cells.end()) {
                                std::shared_ptr<PlateCell>& neighborCell = neighborIt->second;
                                wb_float heightDifference = elevation - (neighborCell->flowNode->elevation());
                                if (heightDifference > float_epsilon) {
                                    plateEdges++;
                                    // downhill node found, take note
                                    outflowCandidates.push_back(std::make_pair(neighborCell->flowNode, heightDifference));
                                    if (heightDifference > largestHeightDifference) {
                                        largestHeightDifference = heightDifference;
                                    }
                                    hasOutflow = true;
                                } else if (std::abs(heightDifference) <= float_epsilon) {
                                    cell->flowNode->equalNodes.insert(neighborCell->flowNode);
                                }
                            }
                        }
                    }
                }
                
                // set up outflow
                if (hasOutflow) {
                    // above sea shelf (currently 300 meters below sea level)
                    if (elevation > this->attributes.sealevel - 300) {
                        wb_float totalSquareElevationOut = 0;
                        for (auto candidateIt = outflowCandidates.begin(); candidateIt != outflowCandidates.end(); candidateIt++) {
                            // squared weighting
                            totalSquareElevationOut += candidateIt->second*candidateIt->second;
                        }
                        
                        // determine suspended sediment
                        wb_float x = largestHeightDifference / this->cellDistanceMeters;
                        if (x < 0){
                            x = 0;
                            std::printf("negative grade!");
                        }
                        wb_float y = log(x+1);
                        wb_float suspended = 15237.4 * y + 170378.0 * y * y;
                        
                        // TODO make not hacky! (ie, more smooth)
                        if (suspended > largestHeightDifference) {
                            suspended = largestHeightDifference;
                        }
                        
                        // will only suspend up to the available material
                        cell->flowNode->suspendMaterial(suspended);
                        
                        //
                        for (auto&& candidateIt : outflowCandidates) {
                            if (candidateIt.first == nullptr) {
                                throw "Null destination";
                            } else if(candidateIt.first == cell->flowNode) {
                                throw "Source and cell are the same";
                            }
                            wb_float heightDifference = candidateIt.second;
                            std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
                            edge->destination = candidateIt.first;
                            edge->source = cell->flowNode;
                            edge->materialHeight = 0; // none moved yet
                            edge->weight = heightDifference*heightDifference / totalSquareElevationOut; // squared weighting on height
                            // add as outflow to this node
                            edge->source->outflowTargets.push_back(edge);
                            // add as inflow node to candidate
                            edge->destination->inflowTargets.push_back(edge);
                        }
                    } else { // below sea shelf
                        wb_float totalElevationOut = 0;
                        for (auto candidateIt = outflowCandidates.begin(); candidateIt != outflowCandidates.end(); candidateIt++) {
                            // linear weighting
                            totalElevationOut += candidateIt->second;
                        }
                        
                        // determine suspended sediment
                        wb_float x = largestHeightDifference / this->cellDistanceMeters;
                        if (x < 0){
                            x = 0;
                            std::printf("negative grade!");
                        }
                        wb_float y = log(x+1);
                        // TODO modify target slope for under water
                        wb_float suspended = 15237.4 * y + 170378.0 * y * y;
                        
                        // TODO make not hacky! (ie, more smooth)
                        if (suspended > largestHeightDifference) {
                            suspended = largestHeightDifference;
                        }
                        
                        // hack lower
                        suspended = suspended*0.25;
                        cell->flowNode->suspendMaterial(suspended);
                        
                        //
                        for (auto&& candidateIt : outflowCandidates) {
                            if (candidateIt.first == nullptr) {
                                throw "Null destination";
                            } else if(candidateIt.first == cell->flowNode) {
                                throw "Source and cell are the same";
                            }
                            wb_float heightDifference = candidateIt.second;
                            std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
                            edge->destination = candidateIt.first;
                            edge->source = cell->flowNode;
                            edge->materialHeight = 0; // none moved yet
                            edge->weight = heightDifference / totalElevationOut; // linear weighting on height
                            // add as outflow to this node
                            edge->source->outflowTargets.push_back(edge);
                            // add as inflow node to candidate
                            edge->destination->inflowTargets.push_back(edge);
                        }
                    }
                }
                outflowCandidates.clear();
            } // end for each cell in plate
        } // end for each plate

        // log number of edges crossing plate boundaries
        //std::cout << "Number of cross boundary edges: " << plateEdges << std::endl;
        //std::cout << "Number of boundary cells: " << edgeCellCount << std::endl;

        return graph;
    }
    
    /*************** Volcanism ***************/
    void World::processAllHotspots(wb_float timestep) {
        
        
        // add thickness (currently roughly bassed on hawaii seamount chain)
        // at 8800 km^3 per million years on average
        // hawaii volume km * years * target num of hotspots * convert to meters / area in km ^2
        this->availableHotspotThickness += 8500*timestep * 10 * 1000 / this->attributes.cellArea;
        
        wb_float usedThickness = 0;
        for(auto&& hotspot : this->hotspots) {
            usedThickness += processHotspot(hotspot, timestep);
        }
        
        this->availableHotspotThickness -= usedThickness;
        
        // create a hotspot if we don't have enough
        if (this->hotspots.size() < 10) {
            std::shared_ptr<VolcanicHotspot> theHotspot = std::make_shared<VolcanicHotspot>();
            theHotspot->weight = std::abs(this->randomSource->randomNormal(10, 4));
            theHotspot->worldLocation = this->randomSource->getRandomPointUnitSphere();
            
            this->hotspots.insert(theHotspot);

            // update nomalized weights
            wb_float totalWeight = 0;
            for(auto&& hotspot : this->hotspots) {
                totalWeight += hotspot->weight;
            }
            for(auto&& hotspot : this->hotspots) {
                hotspot->normWeight = hotspot->weight / totalWeight;
            }
        }        
    }
    
    wb_float World::processHotspot(std::shared_ptr<VolcanicHotspot> hotspot, wb_float timestep) {

        // make sticky hotspot
        // TODO, make configurable
        bool validOutflow = false;
        std::vector<std::shared_ptr<PlateCell>> validCells;
        if (auto plate = hotspot->lastPlate.lock()) { 
            if (auto cell = hotspot->lastCell.lock()) {
                Vec3 locationInLocal = math::affineRotaionMulVec(math::transpose(plate->rotationMatrix), hotspot->worldLocation);
                wb_float dist = math::distanceBetween3Points(locationInLocal, plate->center) * this->attributes.radius;
                // make config! (in km)
                if (dist < 150){
                    validOutflow = true;
                    validCells.push_back(cell);
                }
            }
        }
        
        // find the relavent plate cell
        if (!validOutflow) {
            for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
                std::shared_ptr<Plate> plate = plateIt->second;
                
                // check if we can interact
                Vec3 locationInLocal = math::affineRotaionMulVec(math::transpose(plate->rotationMatrix), hotspot->worldLocation);
                wb_float testAngle = math::angleBetweenUnitVectors(locationInLocal, plate->center);
                if (plate->maxEdgeAngle == 0 || testAngle < plate->maxEdgeAngle || std::isnan(testAngle)) {
                    uint32_t hint = 0;
                    // get our last closest cell index for this plate
                    if (hotspot->closestPlateCellIndex.find(plate->id) != hotspot->closestPlateCellIndex.end()) {
                        hint = hotspot->closestPlateCellIndex.find(plate->id)->second;
                    }
                    uint32_t nearestIndex = this->getNearestGridIndex(locationInLocal, hint);
                    // update neareset
                    hotspot->closestPlateCellIndex[plate->id] = nearestIndex;
                    
                    // check if in plate
                    auto nearestIt = plate->cells.find(nearestIndex);
                    if (nearestIt != plate->cells.end()) {
                        validCells.push_back(nearestIt->second);
                        // TODO, make less hacky in setting last values for hotspot
                        hotspot->lastPlate = plate;
                        hotspot->lastCell = nearestIt->second;
                    }
                }
            }
        }
        
        
        wb_float usedThickness = 0;
        if (validCells.size() > 0) {
            // add the weighted thickness
            wb_float usedThickness = this->availableHotspotThickness * hotspot->normWeight;
            
            // add percent to each valid cell
            for(auto && cell : validCells) {
                cell->rock.continental.set_thickness(cell->rock.continental.get_thickness() + usedThickness/validCells.size());
            }
        }
        
        return usedThickness;
    }
    
/****************************** Movement ******************************/
    
    struct CellDeleteTarget {
        std::shared_ptr<Plate> plate;
        std::shared_ptr<PlateCell> cell;
        
        CellDeleteTarget(std::shared_ptr<Plate> initialPlate) : plate(initialPlate){};
    };
    
#warning "Do it!"
    // TODO make displacement always appear on an edge cell, so delete target is properly set
    void World::computeEdgeInteraction(wb_float timestep){
        // determine edge cell displacements, currently in the direction perpendicular to the closest triangle edge along the plate edge
        std::unordered_map<uint32_t, Matrix3x3> testPlateTransforms;
        for (auto&& plateIt : this->plates) {
            std::shared_ptr<Plate> plate = plateIt.second;
            testPlateTransforms.clear();
            for (auto&& edgeCellIt : plate->edgeCells) {
                std::shared_ptr<PlateCell> edgeCell = edgeCellIt.second;
                CellDeleteTarget deleteTarget(plate); // only plates less dense than this one
                
                for (auto&& lastNearestIt : edgeCell->edgeInfo->otherPlateLastNearest) {
                    // check the plate exists
                    auto testPlateIt = this->plates.find(lastNearestIt.first);
                    if (testPlateIt != this->plates.end()) {
                        std::shared_ptr<Plate> testPlate = testPlateIt->second;
                        // try to get matrix
                        Matrix3x3 toTestTransform;
                        auto transformIt = testPlateTransforms.find(lastNearestIt.first);
                        if (transformIt != testPlateTransforms.end()) {
                            toTestTransform = transformIt->second;
                        } else {
                            // create the transform
                            toTestTransform = math::matrixMul(math::transpose(testPlate->rotationMatrix), plate->rotationMatrix);
                            testPlateTransforms[lastNearestIt.first] = toTestTransform;
                        }
                        Vec3 cellInTest = math::affineRotaionMulVec(toTestTransform, edgeCell->get_vertex()->get_vector());
                        // check nearest
                        uint32_t nearestCellIndex = this->getNearestGridIndex(cellInTest, lastNearestIt.second);
                        
                        // test if nearest is in target plate
                        auto nearestCellIt = testPlate->cells.find(nearestCellIndex);
                        if (nearestCellIt != testPlate->cells.end()) {
                            std::shared_ptr<PlateCell> nearestCell = nearestCellIt->second;
                            // check if this cell is too young
#warning "Not stable checking, allows for rifting between colliding plates to advance the least dense plate"
                            if (deleteTarget.cell == nullptr && edgeCell->age < min_interaction_age && nearestCell->age < min_interaction_age && testPlate->densityOffset < deleteTarget.plate->densityOffset) {
                                deleteTarget.cell = nearestCell;
                                deleteTarget.plate = testPlate;
                            } else
                            // check delete target, any rock that ends up back at this cell should be moved to the nearest least dense plate
                            if (testPlate->densityOffset < deleteTarget.plate->densityOffset && nearestCell->age > min_interaction_age) {
                                deleteTarget.cell = nearestCell;
                                deleteTarget.plate = testPlate;
                            }
                            const GridVertex* nearestVertex = nearestCell->get_vertex();
                            // need to find the two nearest neighbors
                            std::pair<const GridVertex*, wb_float> closestNeighbor = std::make_pair(nullptr, std::numeric_limits<wb_float>::infinity());
                            std::pair<const GridVertex*, wb_float> secondClosestNeighbor = std::make_pair(nullptr, std::numeric_limits<wb_float>::infinity());
                            for (auto neighborVertexIt = nearestVertex->get_neighbors().begin(); neighborVertexIt != nearestVertex->get_neighbors().end(); neighborVertexIt++) {
                                wb_float testDistance = math::distanceBetween3Points((*neighborVertexIt)->get_vector(), nearestVertex->get_vector());
                                if (testDistance < closestNeighbor.second) {
                                    secondClosestNeighbor = closestNeighbor;
                                    closestNeighbor.first = (*neighborVertexIt);
                                    closestNeighbor.second = testDistance;
                                } else if (testDistance < secondClosestNeighbor.second) {
                                    secondClosestNeighbor.first = (*neighborVertexIt);
                                    secondClosestNeighbor.second = testDistance;
                                }
                            }
                            // find the edge in the direction of plate movement
                            // angular cross position scaled to distance for timestep
                            Vec3 pushVector = testPlate->pole.cross(cellInTest) * (testPlate->angularSpeed * timestep);
                            // check the three possibilities
                            
                            const uint maxDepth = 2; // max loop depth
                            bool exitFound = false;
                            uint depth = 0;
                            const GridVertex* pointA, * pointB, * pointC;
                            pointA = nearestVertex;
                            pointB = closestNeighbor.first;
                            pointC = secondClosestNeighbor.first;
                            
                            Vec3 testPoint = cellInTest;
                            wb_float netVCount = 0; // net number of pushVector vectors between origional cellInTest and the edge
                            // closest and first neighbor
                            
                            
                            bool checkPQ = true;
                            
                            Vec3 displacement; // displacement
                            while (!exitFound && depth < maxDepth) {
                                depth++;
                                
                                Vec3 intersectionPoint;
                                wb_float vCount;
                                bool validIntersection;
                                uint8_t intersectingEdge;
                                std::tie(intersectionPoint, vCount, validIntersection, intersectingEdge) = math::triangleIntersection(pointA->get_vector(), pointB->get_vector(), pointC->get_vector(), testPoint, pushVector, checkPQ);
                                // only check first PQ
                                checkPQ = false;
                                testPoint = intersectionPoint;
                                if (validIntersection) {
                                    // swap based on insersecting edge
                                    if (intersectingEdge == 1 << 1) {
                                        // intersecting on A - C
                                        const GridVertex *temp = pointB;
                                        pointB = pointC;
                                        pointC = temp;
                                    } else if (intersectingEdge == 1 << 2) {
                                        // intersectin on B - C
                                        const GridVertex* temp = pointA;
                                        pointA = pointC;
                                        pointC = temp;
                                    }
                                    
                                    // increase net distance
                                    netVCount += vCount;
                                } else {
                                    // something's funky!
                                    //throw std::logic_error("triangle edge intersection not found");
                                    displacement = pushVector * (netVCount + 0.333333); // push 1/3 of the movement outside of the test plate boundary
                                    break;
                                }
                                
                                // Intersection is now between A and B, need to check if they are on the edge
                                auto aCheckIt = testPlate->edgeCells.find(pointA->get_index());
                                if (aCheckIt != testPlate->edgeCells.end()) {
                                    auto bCheckIt = testPlate->edgeCells.find(pointB->get_index());
                                    if (bCheckIt != testPlate->edgeCells.end()) {
                                        // we found an edge edge
                                        exitFound = true;
                                        
                                        displacement = pushVector * (netVCount + 0.333333); // push 1/3 of the movement outside of the test plate boundary
                                    }
                                }
                                if (!exitFound) {
                                    // not on edge, need to find the next pair to check
                                    size_t index = 0;
                                    for (auto neighborIt = pointA->get_neighbors().begin(); neighborIt != pointA->get_neighbors().end(); neighborIt++, index++) {
                                        if ((*neighborIt)->get_index() == pointB->get_index()) {
                                            // must be the next or previous, whichever isn't the old pointC
                                            if (pointA->get_neighbors()[(index + 1) % pointA->get_neighbors().size()]->get_index() == pointC->get_index()) {
                                                pointC = pointA->get_neighbors()[(index - 1) % pointA->get_neighbors().size()];
                                            } else {
                                                pointC = pointA->get_neighbors()[(index + 1) % pointA->get_neighbors().size()];
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                            if (netVCount != 0) {
                                // add collision
                                this->momentumTracker->addCollision(plate, testPlate);
                                
                                // store
                                Vec3 displacementInSelf = math::affineRotaionMulVec(math::transpose(toTestTransform), displacement); // rotate back to self
                                
                                if (edgeCell->displacement == nullptr) {
                                    edgeCell->displacement = std::make_shared<DisplacementInfo>();
                                    //edgeCell->displacement->displacementLocation = edgeCell->get_vertex()->get_vector();
                                }
                                edgeCell->displacement->displacementLocation = edgeCell->displacement->displacementLocation + displacementInSelf;
                            }
                        } // end if nearest index is in test plate
                    } // end if test plate exists
                } // end for each last nearest on edge cell
                
                // move our cell
                if (edgeCell->displacement != nullptr) {
                    // check displacement length, if don't want to push past a neighbor cell
                    Vec3 displacement = edgeCell->displacement->displacementLocation;
                    if (displacement.length() > this->cellSmallAngle / 2) {
                        displacement = displacement * (this->cellSmallAngle / 2 / displacement.length());
                    }
                    edgeCell->displacement->displacementLocation = displacement;
                    //edgeCell->displacement->touched = true;
                    
                    // set delete target, will be nullptr if none found
                    edgeCell->displacement->deleteTarget = deleteTarget.cell;
                } else {
                    edgeCell->displacement = std::make_shared<DisplacementInfo>();
                    
                    // set delete target, will be nullptr if none found
                    edgeCell->displacement->deleteTarget = deleteTarget.cell;
                }
                
                // add momentum tranfer for cells that will delete
                if (deleteTarget.cell != nullptr) {
                    this->momentumTracker->transferMomentumOfCell(plate, deleteTarget.plate, edgeCell);
                }
                
            } // end for each edge cell
        } // end for each plate
    }
    
    
    // currently a very basic displacement balancer, not forces
    void World::balanceInternalPlateForce(std::shared_ptr<Plate> plate, wb_float timestep) {
        const wb_float decayFactor = exp(-0.051293*timestep);
        const wb_float minDisplacement = this->cellSmallAngle / 10;
        int i;
        bool hadMovement = true;
        for (i = 0; i < 100 && hadMovement; i++) {
            hadMovement = false;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell>& cell = cellIt->second;
                if (cell->edgeInfo == nullptr) {
                    // find center of current neighbors, move to that minus min displacement distance
                    
                    Vec3 desiredDisplacement;
                    bool displaced = false;
                    for (auto&& neighborVertex : cell->get_vertex()->get_neighbors()) {
                        auto neighborIt = plate->cells.find(neighborVertex->get_index());
                        if (neighborIt != plate->cells.end()) {
                            std::shared_ptr<PlateCell>& neighborCell = neighborIt->second;
                            if (neighborCell->displacement != nullptr) {
                                Vec3 normalizedDisplacement = math::normalize3Vector(neighborCell->displacement->displacementLocation);
                                wb_float angle = math::angleBetweenUnitVectors(math::normalize3Vector(cell->get_vertex()->get_vector() - neighborCell->get_vertex()->get_vector()), normalizedDisplacement);
                                // Nan's will be skipped should they arise
                                if (angle < math::piOverTwo) {
                                    wb_float weight = 0;
                                    //  could skip angle computation for this vertex, also normalized vectors could be precomputed for ~ 6*8*3*vertexCount bytes
                                    for (auto&& testVertex : cell->get_vertex()->get_neighbors()) {
                                        wb_float testAngle = math::angleBetweenUnitVectors(math::normalize3Vector(cell->get_vertex()->get_vector() - testVertex->get_vector()), normalizedDisplacement);
                                        if (testAngle < math::piOverTwo) {
                                            weight+= cos(testAngle);
                                        }
                                    }
                                    desiredDisplacement = desiredDisplacement + neighborCell->displacement->displacementLocation * (cos(angle) / weight);
                                    displaced = true;
                                }
                            }
                        }
                    }
                    if (displaced) {
                        if (cell->displacement == nullptr) {
                            cell->displacement = std::make_shared<DisplacementInfo>();
                        }
                        
                        // remove the normal component so it's purpendicular to the sphere
                        desiredDisplacement = desiredDisplacement - cell->get_vertex()->get_vector() * cell->get_vertex()->get_vector().dot(desiredDisplacement);
                        Vec3 change = desiredDisplacement - cell->displacement->displacementLocation;
                        if(change.length() > minDisplacement) {
                            cell->displacement->nextDisplacementLocation = desiredDisplacement * decayFactor;
                            hadMovement = true;
                        }
                    }
                }
            }
            
            // update for next round;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell>& cell = cellIt->second;
                if (cell->displacement != nullptr) {
                    if (cell->edgeInfo == nullptr) {
                        cell->displacement->displacementLocation = cell->displacement->nextDisplacementLocation;

                    }
                }
            }
        }
        
//        wb_float averageDisplacement = 0;
//        wb_float cellCount = plate->cells.size();
//        for(auto&& cellIt: plate->cells) {
//            std::shared_ptr<PlateCell> cell = cellIt.second;
//            if (cell->displacement != nullptr) {
//                averageDisplacement = cell->displacement->displacementLocation.length() / cellCount;
//            }
//        }
        
        //std::cout << "Finished balacing plate " << plate->id << " in " << i << " steps."/* Average displacement of " << averageDisplacement / this->cellSmallAngle << "x smallest angle."*/ << std::endl;
    }
    
    
    /*************** Plate Movement ***************/
    void World::movePlates(wb_float timestep){
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++)
        {
            std::shared_ptr<Plate> plate = plateIt->second;
            plate->move(timestep);
        }
    }
    wb_float World::randomPlateSpeed(){
        return randomSource->randomNormal(0.0095, 0.0038); // mean, stdev
    }
    
    
/****************************** Info Getters ******************************/
    
    LocationInfo World::get_locationInfo(Vec3 location) {
        LocationInfo info;
        
        // TODO linear interpolation
        // simplest implementation, get the first closest cell we can find
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            
            // check if we can interact
            Vec3 locationInLocal = math::affineRotaionMulVec(math::transpose(plate->rotationMatrix), location);
            wb_float testAngle = math::angleBetweenUnitVectors(locationInLocal, plate->center);
            if (plate->maxEdgeAngle == 0 || testAngle < plate->maxEdgeAngle || std::isnan(testAngle)) {
                uint32_t hint = 0;
                if (plate->centerVertex != nullptr) {
                    hint = plate->centerVertex->get_index();
                }
                uint32_t nearestIndex = this->getNearestGridIndex(locationInLocal, hint);
                auto nearestIt = plate->cells.find(nearestIndex);
                if (nearestIt != plate->cells.end()) {
                    info.elevation = nearestIt->second->get_elevation();
                    info.sediment = nearestIt->second->rock.sediment.get_thickness();
                    info.tempurature = nearestIt->second->tempurature;
                    info.precipitation = nearestIt->second->precipitation;
                    info.plateId = plate->id;
                    break;
                }
            }
        }
        
        return info;
    }
    
    RockColumn World::netRock() {
        RockColumn result;
        
        for(auto&& plateIt : this->plates) {
            for (auto&& cellIt : plateIt.second->cells) {
                result = accreteColumns(result, cellIt.second->rock);
            }
        }
        return result;
    }
    
    /*************** Validation  ***************/
    bool World::validate(){
        for (auto&& plateIt : this->plates) {
            for (auto&& cellIt : plateIt.second->cells) {
                if (cellIt.first != cellIt.second->get_vertex()->get_index()) {
                    throw std::logic_error("Incorrect Id for cell");
                }
            }
        }
        return true;
    }
    
    /*************** Casting ***************/
    
    uint32_t World::getNearestGridIndex(Vec3 location, uint32_t hint) {
        // check hint is valid
        if (hint >= this->worldGrid->verts_size()) {
            return std::numeric_limits<uint32_t>::max();
        }
        const GridVertex* currentNearest = &this->worldGrid->get_vertices()[hint];
        const GridVertex* testSetNearest = currentNearest;
        wb_float smallestSquareDistance = math::squareDistanceBetween3Points(location, currentNearest->get_vector());
        // could loop for a very long time...
        do {
            float testDistance;
            currentNearest = testSetNearest;
            // loop neighbors
            // bool closestFound = false;
            for (std::vector<GridVertex*>::const_iterator vertexIt = currentNearest->get_neighbors().begin();
                 vertexIt != currentNearest->get_neighbors().end();
                 vertexIt++)
            {
                const GridVertex* vertex = (*vertexIt);
                testDistance = math::squareDistanceBetween3Points(location, vertex->get_vector());
                if (testDistance < smallestSquareDistance) {
                    smallestSquareDistance = testDistance;
                    testSetNearest = vertex;
                }
            }
        } while (currentNearest != testSetNearest);
        
        currentNearest = testSetNearest;
        
        return currentNearest->get_index();
    }
    
    /*************** Constructors ***************/
    World::World(Grid *theWorldGrid, std::shared_ptr<Random> random, WorldConfig config) : worldGrid(theWorldGrid), plates(10), randomSource(random), _nextPlateId(0), availableHotspotThickness(0){
        // set default rock column
        this->divergentOceanicColumn.root = RockSegment(84000.0, 3200.0);
        this->divergentOceanicColumn.oceanic = RockSegment(6000.0, 2890.0);
        
        // set attributes
        this->attributes.mantleDensity = 3400;
        this->attributes.radius = 6367;
        this->attributes.totalSeaDepth = wb_float(theWorldGrid->verts_size()) * config.waterDepth; // average depth for Earth is 2510
        // TODO, calculate on initial run
        this->attributes.sealevel = 9620; // very rough start, dynamic after first run?
        this->attributes.waterDensity = 1026;

        this->attributes.cellArea = 8 * math::piOverTwo * this->attributes.radius * this->attributes.radius / theWorldGrid->verts_size();
        
        // null transfer
        //this->momentumTransfer = nullptr;
        
        // age = 0
        this->age = 0;
        
        // distance between cells in simulation
        // curently a multiple of cell small size
        this->cellDistanceMeters = math::distanceBetween3Points(this->worldGrid->get_vertices()[0].get_vector(), this->worldGrid->get_vertices()[0].get_neighbors()[0]->get_vector()) * this->attributes.radius * 1000;
        
        // supercontinent stuff
        this->supercontinentCycleDuration = 0;
        this->supercontinentCycleStartAge = 0;
        this->desiredPlateCount = 10;
        
        // set cell small angle
        this->cellSmallAngle = std::numeric_limits<wb_float>::max();
        for (auto&& vertex : theWorldGrid->get_vertices()) {
            for (auto&& testVertex : vertex.get_neighbors()) {
                wb_float testAngle = math::distanceBetween3Points(vertex.get_vector(), testVertex->get_vector());
                if (testAngle < cellSmallAngle) {
                    this->cellSmallAngle = testAngle;
                }
            }
        }
        // create and add plates
        std::shared_ptr<Plate> firstPlate = std::make_shared<Plate>(theWorldGrid->verts_size(), this->nextPlateId());
        firstPlate->pole = this->randomSource->getRandomPointUnitSphere();
        firstPlate->angularSpeed = this->randomPlateSpeed();
        firstPlate->densityOffset = this->randomPlateDensityOffset();
        // add the initial cells to the plate
        for (uint32_t index = 0; index < theWorldGrid->verts_size(); index++) {
            std::shared_ptr<PlateCell> cell = std::make_shared<PlateCell>(&(theWorldGrid->get_vertices()[index]));
            firstPlate->cells[index] = cell;
        }
        
        this->plates.insert({firstPlate->id,firstPlate});
        
    } // World(Grid, Random)
}