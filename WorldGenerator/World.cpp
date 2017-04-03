//
//  World.cpp
//  WorldGenerator
//
//

#include "World.hpp"

#include <limits>
#include <thread>

namespace WorldBuilder {
    
    const wb_float erosionRate = 0.05; // per million years
    
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
        
        // update plate indices?
        
        // find a reasonable timestep (currently broken)
        // should get fastest relative speeds
        wb_float fastestPlateSpeed = 0; // fastest plate speeds
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            if (fastestPlateSpeed < std::abs(plate->angularSpeed)) {
                fastestPlateSpeed = std::abs(plate->angularSpeed);
            }
        }
        // since we are on the unit sphere (assumed)
        timestep = (this->cellSmallAngle / fastestPlateSpeed ) / 3;
        if (timestep < minTimestep) {
            timestep = minTimestep;
        } else if (timestep > 10) {
            timestep = 10;
        }
        
        updateTask.timestepUsed = timestep;
        
        this->age = this->age + timestep;

        
        // movement phase
        updateTask.movement.start();
        this->columnMovementPhase(timestep);
        updateTask.movement.end();
        
        // transistion phase
        updateTask.transition.start();
        this->transitionPhase(timestep);
        updateTask.transition.end();
        
        // modification phase
        updateTask.modification.start();
        this->columnModificationPhase(timestep);
        updateTask.modification.end();
        
        return updateTask;
    }
    
    void World::columnMovementPhase(wb_float timestep){
        // move our plates
        this->movePlates(timestep);
        
        
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
    
    void World::transitionPhase(wb_float timestep) {
        // normalize plate grid
        this->renormalizeAllPlates();
        
        // split in supercontinent if needed
        this->supercontinentCycle();
        
        // update edges
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            this->updatePlateEdges(plateIt->second);
        }
        
        // knit back together
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            this->knitPlates(plateIt->second);
        }
        
        // float it!
        this->homeostasis(timestep);
    }
    
    void World::columnModificationPhase(wb_float timestep){
        // thermal first
        this->erodeThermalSmoothing(timestep);
        
        this->erodeSedimentTransport(timestep);
    }
    
/****************************** Transistion ******************************/
    void World::updatePlateEdges(std::shared_ptr<Plate> plate) {
        // clear the edgeCells
        plate->edgeCells.clear();
        
        // TODO replace with a smallest bounding circle implementation
        
        // find the center of the plate, hope it's close tot where the smallest bounding circle's would be
        Vec3 center;
        for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            
            
            
            Vec3 cellVec = cell->get_vertex()->get_vector();
            center = center + cellVec;
            
            // determine edges
            bool isEdge = false;
            for (auto neighborIt = cell->get_vertex()->get_neighbors().begin(); neighborIt != cell->get_vertex()->get_neighbors().end(); neighborIt++) {
                uint32_t index = (*neighborIt)->get_index();
                
                // test if index is in plate
                auto neighborCellIt = plate->cells.find(index);
                if (neighborCellIt == plate->cells.end()) {
                    isEdge = true;
                    break; // found at least one missing cell
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
        for (auto edgeIt = plate->edgeCells.begin(); edgeIt != plate->edgeCells.end(); edgeIt++) {
            std::shared_ptr<PlateCell> edge = edgeIt->second;
            wb_float testDistance = math::angleBetweenUnitVectors(plate->center, edge->get_vertex()->get_vector());
            if (testDistance > plate->maxEdgeAngle) {
                plate->maxEdgeAngle = testDistance;
            }
        }
    }
    
    void World::knitPlates(std::shared_ptr<Plate> plate) {
        
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> testPlate = plateIt->second;
            // ignore self
            if (testPlate != plate) {
                // test interacability between plates
                Matrix3x3 targetToTest = math::matrixMul(math::transpose(testPlate->rotationMatrix), plate->rotationMatrix);
                // if the plates max extent overlap, test edges
                Vec3 testCenter = math::affineRotaionMulVec(targetToTest, plate->center);
                wb_float testAngle = math::angleBetweenUnitVectors(plate->center, testCenter);
                if (testAngle < plate->maxEdgeAngle + testPlate->maxEdgeAngle) {
                    // some cells may interact
                    for (auto edgeIt = plate->edgeCells.begin(); edgeIt != plate->edgeCells.end(); edgeIt++) {
                        std::shared_ptr<PlateCell> edgeCell = edgeIt->second;
                        Vec3 targetEdgeInTest = math::affineRotaionMulVec(targetToTest, edgeCell->get_vertex()->get_vector());
                        wb_float angleToCenter = math::angleBetweenUnitVectors(testPlate->center, targetEdgeInTest);
                        if (angleToCenter < testPlate->maxEdgeAngle) {
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
                            auto testEdgeIt = testPlate->edgeCells.find(nearestGridIndex);
                            if (testEdgeIt != testPlate->edgeCells.end()) {
                                std::shared_ptr<PlateCell> testEdge = testEdgeIt->second;
                                uint64_t neighborKey;
                                neighborKey = ((uint64_t)testPlate->id << 32) + nearestGridIndex;
                                wb_float neighborDistance = math::distanceBetween3Points(targetEdgeInTest, testEdge->get_vertex()->get_vector());
                                EdgeNeighbor edgeData;
                                edgeData.plateIndex = testPlate->id;
                                edgeData.cellIndex = nearestGridIndex;
                                edgeData.distance = neighborDistance;
                                edgeCell->edgeInfo->otherPlateNeighbors[neighborKey] = edgeData; //neighborDistance;
                            }
                            
                            // loop over neighbors
                            for (auto neighborIt = this->worldGrid->get_vertices()[nearestGridIndex].get_neighbors().begin(); neighborIt != this->worldGrid->get_vertices()[nearestGridIndex].get_neighbors().end(); neighborIt++)
                            {
                                uint32_t index = (*neighborIt)->get_index();
                                testEdgeIt = testPlate->edgeCells.find(index);
                                if (testEdgeIt != testPlate->edgeCells.end()) {
                                    std::shared_ptr<PlateCell> testEdge = testEdgeIt->second;
                                    uint64_t neighborKey;
                                    neighborKey = ((uint64_t)testPlate->id << 32) + index;
                                    wb_float neighborDistance = math::distanceBetween3Points(targetEdgeInTest, testEdge->get_vertex()->get_vector());
                                    EdgeNeighbor edgeData;
                                    edgeData.plateIndex = testPlate->id;
                                    edgeData.cellIndex = index;
                                    edgeData.distance = neighborDistance;
                                    edgeCell->edgeInfo->otherPlateNeighbors[neighborKey] = edgeData;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
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
                    cell->displacement->deleteTarget->rock = accreteColumns(cell->displacement->deleteTarget->rock, cell->rock);
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
        
// DEBUG RANDOM DELETE!
//        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
//            std::shared_ptr<Plate> plate = plateIt->second;
//            std::vector<std::shared_ptr<PlateCell>> cellsToDelete;
//            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
//                std::shared_ptr<PlateCell> cell = cellIt->second;
//                if (this->randomSource->randomUniform(0, 1000) == 0) {
//                    cellsToDelete.push_back(cell);
//                    //cell->displacement->deleteTarget->rock = accreteColumns(cell->displacement->deleteTarget->rock, cell->rock);
//                }
//            }
//            int deleteCount = 0;
//            for (auto deleteIt = cellsToDelete.begin(); deleteIt != cellsToDelete.end(); deleteIt++) {
//                (*deleteIt)->displacement = nullptr;
//                // skip erasing from edges, as that map gets cleared next
//                plate->cells.erase((*deleteIt)->get_vertex()->get_index());
//                
//                deleteCount++;
//            }
//            std::cout << "Randomly deleted " << deleteCount << " cells for plate " << plateIt->second->id << "." << std::endl;
//        }
        
        // clear displaced?
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                cell->displacement = nullptr;
            }
        }
    }
    
    
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
                
                // find the nearest index
                uint32_t nearestIndex = this->getNearestGridIndex(cell->displacement->displacementLocation, cell->get_vertex()->get_index());
                const GridVertex* nearestVertex = &this->worldGrid->get_vertices()[nearestIndex];
                
                // find weights for nearest and each neighbors
                // can't trust the world cell size estimate until more uniform grid is created, but radius should be roughly the same for nearby cells
                wb_float cellRadius = math::distanceBetween3Points(nearestVertex->get_vector(), nearestVertex->get_neighbors()[0]->get_vector()) / 2;
                // check the nearest is in the plate
                auto targetCellIt = plate->cells.find(nearestIndex);
                if (targetCellIt != plate->cells.end()) {
                    std::shared_ptr<PlateCell> targetCell = targetCellIt->second;
                    // weight with nearest
                    wb_float weight = math::circleIntersectionArea(math::distanceBetween3Points(targetCell->get_vertex()->get_vector(), cell->displacement->displacementLocation), cellRadius);
                    totalWeight += weight;
                    weights.push_back(std::make_pair(targetCell, weight));
                }
                // each neighbor
                for (auto neighborIt = nearestVertex->get_neighbors().begin(); neighborIt != nearestVertex->get_neighbors().end(); neighborIt++) {
                    targetCellIt = plate->cells.find((*neighborIt)->get_index());
                    if (targetCellIt != plate->cells.end()) {
                        std::shared_ptr<PlateCell> targetCell = targetCellIt->second;
                        // weight with neighbor
                        wb_float weight = math::circleIntersectionArea(math::distanceBetween3Points(targetCell->get_vertex()->get_vector(), cell->displacement->displacementLocation), cellRadius);
                        totalWeight += weight;
                        weights.push_back(std::make_pair(targetCell, weight));
                    }
                }
                
                // move rock based on weights
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
    
    /*************** Supercontinent Cycle ***************/
    void World::supercontinentCycle(){
        // for cycle if plates have eaten eachother (want more than two)
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
                uint_fast32_t plateIndexToSplit = this->randomSource->randomUniform(0, totalPlateSize - 1);
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
    void World::erodeThermalSmoothing(wb_float timestep) {
        
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                
                // create sediement
                float activeElevation = cell->get_elevation();
                // move some stuff to lower elevation cells
                float erosionFactor = (activeElevation - 9260) / (4000);
                float erosionHeight = 0;
                if (erosionFactor > 0) {
                    // add erosion to self as sediment
                    erosionFactor = erosionFactor*erosionFactor*erosionFactor/400;
                    // limit to .1
                    if (erosionFactor > 0.5) {
                        erosionFactor = 0.5;
                    }
                    erosionHeight = erosionFactor * timestep * (activeElevation - 9260);
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
                            erosionHeight = (activeElevation - neighborElevation) * erosionRate * timestep / neighborCount;
                            
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
                                    erosionHeight = (activeElevation - neighborElevation) * erosionRate * timestep / neighborCount;
                                    
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
    
    void World::erodeSedimentTransport(wb_float timestep){
        // sediment flow, does this want to be first???
        std::shared_ptr<MaterialFlowGraph> flowGraph = this->createFlowGraph(); // TODO don't create each phase, reuse the memory!
        wb_float beforeTotal = flowGraph->totalMaterial();
        bool validWeights = flowGraph->checkWeights();
        flowGraph->flowAll();
        wb_float afterTotal = flowGraph->totalMaterial();
        
        std::cout << "After total is " << afterTotal / beforeTotal << " larger than before." << std::endl;
        
        // update sediment for each plate cell
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                RockSegment sediment(cell->flowNode->get_materialHeight(), 2700);
                cell->rock.sediment = sediment; // all sediment is tracked in the flow graph, not just what moved
            }
        }
    }
    
    // TODO need to add timestep, at least to suspension amounts
    std::shared_ptr<MaterialFlowGraph> World::createFlowGraph(){
        size_t cellCount = 0;
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            cellCount += plateIt->second->cells.size();
        }
        
        std::shared_ptr<MaterialFlowGraph> graph = std::make_shared<MaterialFlowGraph>(cellCount);
        
        // set initial elevations
        size_t index = 0;
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            for (auto cellIt = plateIt->second->cells.begin(); cellIt != plateIt->second->cells.end(); cellIt++, index++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                graph->nodes[index].set_materialHeight(cell->rock.sediment.get_thickness());
                graph->nodes[index].offsetHeight = cell->get_elevation() - graph->nodes[index].get_materialHeight();
                cell->flowNode = &(graph->nodes[index]);
            }
        }
        
        // build from node heights
        std::vector<std::pair<MaterialFlowNode*, wb_float>> outflowCandidates;
        // find outflow only, inflow set from outflow nodes
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                // we grab the elevations from the flow graph, incase a different module wants to modify the elevatsion while sediment transport is in progress
                wb_float elevation = cell->flowNode->get_materialHeight() + cell->flowNode->offsetHeight;
                
                // find outflow candidates
                wb_float largestHeightDifference = 0; // determines suspended material
                bool hasOutflow = false;
                // candidates from within the plate
                for (auto neighborIndexIt = cell->get_vertex()->get_neighbors().begin(); neighborIndexIt != cell->get_vertex()->get_neighbors().end(); neighborIndexIt++) {
                    auto neighborIt = plate->cells.find((*neighborIndexIt)->get_index());
                    if (neighborIt != plate->cells.end()) {
                        std::shared_ptr<PlateCell> neighborCell = neighborIt->second;
                        wb_float heightDifference = elevation - (neighborCell->flowNode->get_materialHeight() + neighborCell->flowNode->offsetHeight);
                        if (heightDifference > 0) {
                            // downhill node found, take note
                            outflowCandidates.push_back(std::make_pair(neighborCell->flowNode, heightDifference));
                            if (heightDifference > largestHeightDifference) {
                                largestHeightDifference = heightDifference;
                            }
                            hasOutflow = true;
                        }
                    }
                }
                if (cell->edgeInfo != nullptr) {
                    // we are an edge, check other plate neighbors
                    for (auto neighborIndexIt = cell->edgeInfo->otherPlateNeighbors.begin(); neighborIndexIt != cell->edgeInfo->otherPlateNeighbors.end(); neighborIndexIt++) {
                        auto neighborPlateIt = this->plates.find(neighborIndexIt->second.plateIndex);
                        if (neighborPlateIt != this->plates.end()){
                            std::shared_ptr<Plate> neighborPlate = neighborPlateIt->second;
                            auto neighborIt = neighborPlate->cells.find(neighborIndexIt->second.cellIndex);
                            if (neighborIt != neighborPlate->cells.end()) {
                                std::shared_ptr<PlateCell> neighborCell = neighborIt->second;
                                wb_float heightDifference = elevation - (neighborCell->flowNode->get_materialHeight() + neighborCell->flowNode->offsetHeight);
                                if (heightDifference > 0) {
                                    // downhill node found, take note
                                    outflowCandidates.push_back(std::make_pair(neighborCell->flowNode, heightDifference));
                                    if (heightDifference > largestHeightDifference) {
                                        largestHeightDifference = heightDifference;
                                    }
                                    hasOutflow = true;
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
                        
                        if (suspended >= cell->flowNode->get_materialHeight()) {
                            cell->flowNode->suspendMaterial(graph->nodes[index].get_materialHeight());
                        } else {
                            cell->flowNode->suspendMaterial(suspended);
                        }
                        
                        //
                        for (auto candidateIt = outflowCandidates.begin(); candidateIt != outflowCandidates.end(); candidateIt++) {
                            wb_float heightDifference = candidateIt->second;
                            std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
                            edge->destination = candidateIt->first;
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
                        if (suspended >= cell->flowNode->get_materialHeight()) {
                            cell->flowNode->suspendMaterial(graph->nodes[index].get_materialHeight());
                        } else {
                            cell->flowNode->suspendMaterial(suspended);
                        }
                        
                        //
                        for (auto candidateIt = outflowCandidates.begin(); candidateIt != outflowCandidates.end(); candidateIt++) {
                            wb_float heightDifference = candidateIt->second;
                            std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
                            edge->destination = candidateIt->first;
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
            }
        }
        return graph;
    }
    
/****************************** Movement ******************************/
    
    void World::computeEdgeInteraction(wb_float timestep){
        // determine edge cell displacements, currently in the direction perpendicular to the closest triangle edge along the plate edge
        std::unordered_map<uint32_t, Matrix3x3> testPlateTransforms;
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            // WRONG AGAIN!, keep incase I'm wrong wrong
            //Matrix3x3 toWorldTransform = math::transpose(plate->rotationMatrix);
            testPlateTransforms.clear();
            for (auto edgeCellIt = plate->edgeCells.begin(); edgeCellIt != plate->edgeCells.end(); edgeCellIt++) {
                std::shared_ptr<PlateCell> edgeCell = edgeCellIt->second;
                std::pair<std::shared_ptr<PlateCell>, wb_float> deleteTarget = std::make_pair(nullptr, plate->densityOffset); // only plates less dense than this one
                
                for (auto lastNearestIt = edgeCell->edgeInfo->otherPlateLastNearest.begin(); lastNearestIt != edgeCell->edgeInfo->otherPlateLastNearest.end(); lastNearestIt++) {
                    // check the plate exists
                    auto testPlateIt = this->plates.find(lastNearestIt->first);
                    if (testPlateIt != this->plates.end()) {
                        std::shared_ptr<Plate> testPlate = testPlateIt->second;
                        // try to get matrix
                        Matrix3x3 toTestTransform;
                        auto transformIt = testPlateTransforms.find(lastNearestIt->first);
                        if (transformIt != testPlateTransforms.end()) {
                            toTestTransform = transformIt->second;
                        } else {
                            // create the transform
                            toTestTransform = math::matrixMul(math::transpose(testPlate->rotationMatrix), plate->rotationMatrix);
                            testPlateTransforms[lastNearestIt->first] = toTestTransform;
                        }
                        Vec3 cellInTest = math::affineRotaionMulVec(toTestTransform, edgeCell->get_vertex()->get_vector());
                        // check nearest
                        uint32_t nearestCellIndex = this->getNearestGridIndex(cellInTest, lastNearestIt->second);
                        
                        // test if nearest is in target plate
                        auto nearestCellIt = testPlate->cells.find(nearestCellIndex);
                        if (nearestCellIt != testPlate->cells.end()) {
                            std::shared_ptr<PlateCell> nearestCell = nearestCellIt->second;
                            // check delete target, any rock that ends up back at this cell should be moved to the nearest least dense plate
                            if (testPlate->densityOffset < deleteTarget.second) {
                                deleteTarget.first = nearestCell;
                                deleteTarget.second = testPlate->densityOffset;
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
                    //  MODIFIED FOR DEBUG!
                    //  Not currently
                    //
                    //
                    edgeCell->displacement->displacementLocation = math::normalize3Vector(edgeCell->get_vertex()->get_vector() + displacement);
                    
                    // set delete target, will be nullptr if none found
                    edgeCell->displacement->deleteTarget = deleteTarget.first;
                }
            } // end for each edge cell
        } // end for each plate
    }
    
    
    // currently a very basic displacement balancer, not forces
    void World::balanceInternalPlateForce(std::shared_ptr<Plate> plate, wb_float timestep) {
        const wb_float minDisplacment = this->cellSmallAngle / 100;
        int i;
        bool hadMovement = true;
        for (i = 0; i < 100 && hadMovement; i++) {
            hadMovement = false;
            for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                if (cell->edgeInfo == nullptr) {
                    // find center of current neighbors, move to that minus min displacement distance
                    Vec3 neighborCenter;
                    Vec3 expectedCenter;
                    Vec3 currentLocation;
                    if (cell->displacement != nullptr) {
                        currentLocation = cell->displacement->displacementLocation;
                    } else {
                        currentLocation = cell->get_vertex()->get_vector();
                    }
                    for (auto neighborIndexIt = cell->get_vertex()->get_neighbors().begin(); neighborIndexIt != cell->get_vertex()->get_neighbors().end(); neighborIndexIt++) {
                        auto neighborIt = plate->cells.find((*neighborIndexIt)->get_index());
                        if (neighborIt != plate->cells.end()) {
                            std::shared_ptr<PlateCell> neighborCell = neighborIt->second;
                            Vec3 neighborVector;
                            if (neighborCell->displacement != nullptr) {
                                neighborVector = neighborCell->displacement->displacementLocation;
                            } else {
                                neighborVector = neighborCell->get_vertex()->get_vector();
                            }
                            neighborCenter = neighborCenter + neighborVector;
                        } else {
                            throw std::logic_error("something's fishy with the edge finding, hole in middle of plate");
                        }
                    } // end for each neighbor index
                    // normalize center
                    neighborCenter = math::normalize3Vector(neighborCenter);
                    Vec3 displacementToCenter = (neighborCenter + cell->get_vertex()->displacementFromCenter()) - currentLocation;
                    if (displacementToCenter.length() > minDisplacment) {
                        displacementToCenter = displacementToCenter * ((displacementToCenter.length() - minDisplacment) * 0.5 / displacementToCenter.length()); // only travel half the distance
                        if (cell->displacement == nullptr) {
                            cell->displacement = std::make_shared<DisplacementInfo>();
                        }
                        cell->displacement->displacementLocation = math::normalize3Vector(currentLocation + displacementToCenter);
                        hadMovement = true;
                    }
                }
            }
        }
        
        std::cout << "Finished balacing plate " << plate->id << " in " << i << " steps." << std::endl;
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
    
    wb_float World::get_elevation(Vec3 location) {
        
        // TODO linear interpolation
        // simplest implementation, get the first closest cell we can find
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> plate = plateIt->second;
            
            // check if we can interact
            Vec3 locationInLocal = math::affineRotaionMulVec(math::transpose(plate->rotationMatrix), location);
            if (plate->maxEdgeAngle == 0 || math::angleBetweenUnitVectors(locationInLocal, plate->center) < plate->maxEdgeAngle) {
                uint32_t hint = 0;
                if (plate->centerVertex != nullptr) {
                    hint = plate->centerVertex->get_index();
                }
                uint32_t nearestIndex = this->getNearestGridIndex(locationInLocal, hint);
                auto nearestIt = plate->cells.find(nearestIndex);
                if (nearestIt != plate->cells.end()) {
                    return nearestIt->second->get_elevation();
                }
            }
        }
        
        return 0;
    }
    
    /*************** Casting ***************/
    
    uint32_t World::getNearestGridIndex(Vec3 location, uint32_t hint) {
        // check hint is valid
        if (hint >= this->worldGrid->verts_size()) {
            return UINT32_MAX;
        }
        const GridVertex* currentNearest = &this->worldGrid->get_vertices()[hint];
        const GridVertex* testSetNearest = currentNearest;
        float smallestSquareDistance = math::squareDistanceBetween3Points(location, currentNearest->get_vector());
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
    World::World(Grid *theWorldGrid, std::shared_ptr<Random> random) : worldGrid(theWorldGrid), plates(10), randomSource(random), _nextPlateId(0){
        // set default rock column
        this->divergentOceanicColumn.root = RockSegment(84000.0, 3200.0);
        this->divergentOceanicColumn.oceanic = RockSegment(6000.0, 2890.0);
        
        // set attributes
        this->attributes.mantleDensity = 3400;
        this->attributes.radius = 6367;
        this->attributes.sealevel = 9620;
        this->attributes.waterDensity = 1026;
        
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
        this->desiredPlateCount = 6;
        
        // set cell small angle
        // ~ distance on unit sphere
        this->cellSmallAngle = 0.45 * math::distanceBetween3Points(this->worldGrid->get_vertices()[0].get_vector(), this->worldGrid->get_vertices()[0].get_neighbors()[0]->get_vector());
        
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