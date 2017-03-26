//
//  World.cpp
//  WorldGenerator
//
//

#include "World.hpp"

namespace WorldBuilder {
    
    const wb_float erosionRate = 0.05; // per million years
    
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
        timestep = (this->cellSmallAngle / fastestPlateSpeed );
        if (timestep < minTimestep) {
            timestep = minTimestep;
        } else if (timestep > 10) {
            timestep = 10;
        }
        
        updateTask.timestepUsed = timestep;
        
        this->age = this->age + timestep;

        
        // movement phase
        
        // transistion phase
        this->transitionPhase(timestep);
        
        // modification phase
        
        return updateTask;
    }
    
    // wants somewhere to go
    wb_float World::randomPlateDensityOffset() {
        return this->randomSource->randomNormal(0.0, 1.0);
    }
    
/****************************** Transistion ******************************/
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
        
        // add center to plate
        plate->center = math::normalize3Vector(center);
        
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
    
    void World::knitPlates(std::shared_ptr<Plate> targetPlate) {
        
        Matrix3x3 targetToWorld = math::transpose(targetPlate->rotationMatrix);
        
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++) {
            std::shared_ptr<Plate> testPlate = plateIt->second;
            // ignore self
            if (testPlate != targetPlate) {
                // test interacability between plates
                Matrix3x3 targetToTest = math::matrixMul(targetPlate->rotationMatrix, targetToWorld);
                // if the plates max extent overlap, test edges
                Vec3 testCenter = math::affineRotaionMulVec(targetToTest, targetPlate->center);
                wb_float testAngle = math::angleBetweenUnitVectors(targetPlate->center, testCenter);
                if (testAngle < targetPlate->maxEdgeAngle + testPlate->maxEdgeAngle) {
                    // some cells may interact
                    for (auto edgeIt = targetPlate->edgeCells.begin(); edgeIt != targetPlate->edgeCells.end(); edgeIt++) {
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
                                edgeCell->edgeInfo->otherPlateNeighbors[neighborKey] = neighborDistance;
                            }
                            
                            // loop over neighbors
                            for (auto neighborIt = this->worldGrid->get_vertices()[nearestGridIndex].get_neighbors().begin(); neighborIt != this->worldGrid->get_vertices()[nearestGridIndex].get_neighbors().end(); neighborIt++)
                            {
                                uint32_t index = (*neighborIt)->get_index();
                                testEdgeIt = testPlate->edgeCells.find(index);
                                if (testEdgeIt != testPlate->edgeCells.end()) {
                                    std::shared_ptr<PlateCell> testEdge = testEdgeIt->second;
                                    uint64_t neighborKey;
                                    neighborKey = ((uint64_t)testPlate->id << 32) + nearestGridIndex;
                                    wb_float neighborDistance = math::distanceBetween3Points(targetEdgeInTest, testEdge->get_vertex()->get_vector());
                                    edgeCell->edgeInfo->otherPlateNeighbors[neighborKey] = neighborDistance;
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
            std::vector<std::shared_ptr<PlateCell>> cellsToDelete;
            for (auto cellIt = plateIt->second->edgeCells.begin(); cellIt != plateIt->second->edgeCells.end(); cellIt++) {
                std::shared_ptr<PlateCell> cell = cellIt->second;
                if (cell->displacement != nullptr && cell->displacement->deleteTarget != nullptr) {
                    cellsToDelete.push_back(cell);
                    cell->displacement->deleteTarget->rock = accreteColumns(cell->displacement->deleteTarget->rock, cell->rock);
                }
            }
            for (auto deleteIt = cellsToDelete.begin(); deleteIt != cellsToDelete.end(); deleteIt++) {
                // skip erasing from edges, as that map gets cleared next
                plateIt->second->cells.erase((*deleteIt)->get_vertex()->get_index());
            }
        }
    }
    
    
    void World::renormalizePlate(std::shared_ptr<Plate> plate) {
        // if a cell has been moved, the rock needs to be copied to the displaced info section
        for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            if (cell->displacement != nullptr) {
                cell->displacement->displacedRock = cell->rock;
                RockSegment zeroSegment;
                zeroSegment.density = 1;
                zeroSegment.thickness = 0;
                cell->rock.sediment = zeroSegment;
                cell->rock.continental = zeroSegment;
                cell->rock.oceanic = zeroSegment;
                cell->rock.root = zeroSegment;
            }
        }
        
        // move rock from each displaced cell based on weighted overlap
        std::vector<std::pair<std::shared_ptr<PlateCell>, wb_float>> weights;
        wb_float totalWeight;
        for (auto cellIt = plate->cells.begin(); cellIt != plate->cells.end(); cellIt++) {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            if (cell->displacement != nullptr) {
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
                    moveColumn.sediment.density = cell->displacement->displacedRock.sediment.density;
                    moveColumn.continental.density = cell->displacement->displacedRock.continental.density;
                    moveColumn.oceanic.density = cell->displacement->displacedRock.oceanic.density;
                    moveColumn.root.density = cell->displacement->displacedRock.root.density;
                    
                    // set thicknesses
                    moveColumn.sediment.thickness = cell->displacement->displacedRock.sediment.thickness * (destinationWeight / totalWeight);
                    moveColumn.continental.thickness = cell->displacement->displacedRock.continental.thickness * (destinationWeight / totalWeight);
                    moveColumn.oceanic.thickness = cell->displacement->displacedRock.oceanic.thickness * (destinationWeight / totalWeight);
                    moveColumn.root.thickness = cell->displacement->displacedRock.root.thickness * (destinationWeight / totalWeight);
                    
                    // combine with destination
                    destinationCell->rock = accreteColumns(destinationCell->rock, moveColumn);
                }
            }
        }
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
                    size_t plateSize = plate->surfaceSize();
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
                
                std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> newPlates = this->splitPlate(plateToSplit);
                this->deletePlate(plateToSplit); // TODO potential memory leak
                this->plates.insert({newPlates.first->id, newPlates.first});
                this->plates.insert({newPlates.second->id, newPlates.second});
                
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
        uint32_t index = 0;
        for (auto cellIt = plateToSplit->cells.begin(); cellIt != plateToSplit->cells.end(); cellIt++, index++)
        {
            std::shared_ptr<PlateCell> cell = cellIt->second;
            
            distanceSmall = math::squareDistanceBetween3Points(smallCenter, cell->get_vertex()->get_vector());
            // could test distanceSmall in circle known to be closer
            distanceLarge1 = math::squareDistanceBetween3Points(largeCenter1, cell->get_vertex()->get_vector());
            if (distanceSmall > distanceLarge1) {
                // copy rock, subduction
                newPlates.first->cells[index]->rock = cell->rock;
                newPlates.first->cells[index]->bIsSubducted = cell->bIsSubducted;
            } else {
                distanceLarge2 = math::squareDistanceBetween3Points(largeCenter2, cell->get_vertex()->get_vector());
                if (distanceSmall > distanceLarge2) {
                    // copy rock, subduction
                    newPlates.first->cells[index]->rock = cell->rock;
                    newPlates.first->cells[index]->bIsSubducted = cell->bIsSubducted;
                } else {
                    // closer to small
                    // copy rock, subduction
                    newPlates.second->cells[index]->rock = cell->rock;
                    newPlates.second->cells[index]->bIsSubducted = cell->bIsSubducted;
                }
            }
        }
        
        
        return newPlates;
    }
    
    std::tuple<Vec3, Vec3, Vec3> World::getSplitPoints(std::shared_ptr<Plate> plateToSplit){
        const GridVertex* firstVertex = this->getRandomContinentalVertex(plateToSplit);
        
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
        std::vector<size_t> continentalIndicies;
        std::vector<size_t> oceanicIndicies;
        uint32_t index = 0;
        for (auto plateCellIt = plateToSplit->cells.begin();
             plateCellIt != plateToSplit->cells.end();
             plateCellIt++, index++)
        {
            std::shared_ptr<PlateCell> plateCell = plateCellIt->second;
            if (plateCell->isContinental()) {
                continentalIndicies.push_back(index);
            } else if (!plateCell->isSubducted() && !plateCell->rock.isEmpty()){
                oceanicIndicies.push_back(index);
            }
        }
        
        if (continentalIndicies.size() > 0) {
            return plateToSplit->cells[continentalIndicies[this->randomSource->uniformIndex(0, continentalIndicies.size() - 1)]]->get_vertex();
        } else if (oceanicIndicies.size() > 0) {
            return plateToSplit->cells[oceanicIndicies[this->randomSource->uniformIndex(0, oceanicIndicies.size() - 1)]]->get_vertex();
        } else {
            // plate has no cells?????
            return plateToSplit->cells[this->randomSource->uniformIndex(0, plateToSplit->cells.size() - 1)]->get_vertex();
        }
    }
    
    void World::deletePlate(std::shared_ptr<Plate> plate) {
        this->plates.erase(plate->id);
    }
    
    /*************** Erosion ***************/
    // TODO need to add timestep, at least to suspension amounts
//    std::unique_ptr<MaterialFlowGraph> World::createFlowGraph(){
//        MaterialFlowGraph* graph = new MaterialFlowGraph(this->cells.size());
//        
//        size_t index = 0;
//        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
//             cell != this->cells.end();
//             cell++, index++)
//        {
//            // set node material height and elevation offset
//            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
//            if (activePlateIndex >= 0) {
//                graph->nodes[index].materialHeight = cell->surfaceCells[activePlateIndex][0]->rock.sediment.thickness;
//                graph->nodes[index].offsetHeight = cell->surfaceCells[activePlateIndex][0]->get_elevation() - graph->nodes[index].materialHeight;
//            } else {
//                graph->nodes[index].materialHeight = cell->strandedSediment.thickness;
//                graph->nodes[index].offsetHeight = cell->strandedElevation;
//            }
//        }
//        
//        // build from node heights
//        std::vector<MaterialFlowNode*> outflowCandidates;
//        std::vector<float> outflowCandidateSquareElevationDifferences;
//        // find outflow only, inflow set from outflow nodes
//        index = 0;
//        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
//             cell != this->cells.end();
//             cell++, index++)
//        {
//            float elevation =  graph->nodes[index].materialHeight + graph->nodes[index].offsetHeight;
//            // only nodes above -300 meters can have outflow nodes
//            // TODO replace with different erosion model for oceans
//            if (elevation > this->attributes.sealevel - 300) {
//                float largestHeightDifference = 0; // determines suspended material
//                float totalSquareElevationOut = 0;
//                // get node index from grid for each neighbor
//                for (std::vector<GridVertex*>::const_iterator vertex = cell->vertex->get_neighbors().begin();
//                     vertex != cell->vertex->get_neighbors().end();
//                     vertex++)
//                {
//                    MaterialFlowNode* candidate = &(graph->nodes[(*vertex)->get_index()]);
//                    float neighborElevation = candidate->materialHeight + candidate->offsetHeight;
//                    float heightDifference = elevation - neighborElevation;
//                    if (heightDifference > 0) {
//                        // downhill node found, take note
//                        outflowCandidates.push_back(candidate);
//                        outflowCandidateSquareElevationDifferences.push_back(heightDifference*heightDifference);
//                        if (heightDifference > largestHeightDifference) {
//                            largestHeightDifference = heightDifference;
//                        }
//                        totalSquareElevationOut += heightDifference*heightDifference;
//                    }
//                }
//                
//                // determine ouflow weights, and material suspended in this cell
//                if (totalSquareElevationOut > 0) {
//                    float x = largestHeightDifference / this->cellDistanceMeters;
//                    if (x < 0){
//                        x = 0;
//                        std::printf("negative grade!");
//                    }
//                    float y = logf(x+1);
//                    float suspended = 15237.4 * y + 170378.0 * y * y;
//                    if (suspended >= graph->nodes[index].materialHeight) {
//                        graph->nodes[index].suspendedMaterialHeight = graph->nodes[index].materialHeight;
//                    } else {
//                        graph->nodes[index].suspendedMaterialHeight = suspended;
//                    }
//                    // determine weights
//                    size_t outflowIndex = 0;
//                    for (std::vector<MaterialFlowNode*>::iterator outflowCandidate = outflowCandidates.begin();
//                         outflowCandidate != outflowCandidates.end();
//                         outflowCandidate++, outflowIndex++)
//                    {
//                        std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
//                        edge->destination = *outflowCandidate;
//                        edge->source = &(graph->nodes[index]);
//                        edge->materialHeight = 0; // none moved yet
//                        edge->weight = outflowCandidateSquareElevationDifferences[outflowIndex] / totalSquareElevationOut;
//                        // add as outflow to this node
//                        graph->nodes[index].outflowTargets.push_back(edge);
//                        // add as inflow node to candidate
//                        (*outflowCandidate)->inflowTargets.push_back(edge);
//                    }
//                }
//                // TODO verify weighting sums to one
//                
//                // clear for next cell
//                outflowCandidates.clear();
//                outflowCandidateSquareElevationDifferences.clear();
//            } else /* elevation < sealevel - 300 */{
//                // weight linearly. Less directed water flow??
//                float largestHeightDifference = 0; // determines suspended material
//                float totalSquareElevationOut = 0;
//                // get node index from grid for each neighbor
//                for (std::vector<GridVertex*>::const_iterator vertex = cell->vertex->get_neighbors().begin();
//                     vertex != cell->vertex->get_neighbors().end();
//                     vertex++)
//                {
//                    MaterialFlowNode* candidate = &(graph->nodes[(*vertex)->get_index()]);
//                    float neighborElevation = candidate->materialHeight + candidate->offsetHeight;
//                    float heightDifference = elevation - neighborElevation;
//                    if (heightDifference > 0) {
//                        // downhill node found, take note
//                        outflowCandidates.push_back(candidate);
//                        outflowCandidateSquareElevationDifferences.push_back(heightDifference);
//                        if (heightDifference > largestHeightDifference) {
//                            largestHeightDifference = heightDifference;
//                        }
//                        totalSquareElevationOut += heightDifference;
//                    }
//                }
//                
//                // determine ouflow weights, and material suspended in this cell
//                if (totalSquareElevationOut > 0) {
//                    float x = largestHeightDifference / this->cellDistanceMeters;
//                    if (x < 0){
//                        x = 0;
//                        std::printf("negative grade!");
//                    }
//                    float y = logf(x+1);
//                    // TODO modify target slope for under water
//                    float suspended = 15237.4 * y + 170378.0 * y * y;
//                    // hack lower
//                    suspended = suspended*0.25;
//                    if (suspended >= graph->nodes[index].materialHeight) {
//                        graph->nodes[index].suspendedMaterialHeight = graph->nodes[index].materialHeight;
//                    } else {
//                        graph->nodes[index].suspendedMaterialHeight = suspended;
//                    }
//                    // determine weights
//                    size_t outflowIndex = 0;
//                    for (std::vector<MaterialFlowNode*>::iterator outflowCandidate = outflowCandidates.begin();
//                         outflowCandidate != outflowCandidates.end();
//                         outflowCandidate++, outflowIndex++)
//                    {
//                        std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
//                        edge->destination = *outflowCandidate;
//                        edge->source = &(graph->nodes[index]);
//                        edge->materialHeight = 0; // none moved yet
//                        edge->weight = outflowCandidateSquareElevationDifferences[outflowIndex] / totalSquareElevationOut;
//                        // add as outflow to this node
//                        graph->nodes[index].outflowTargets.push_back(edge);
//                        // add as inflow node to candidate
//                        (*outflowCandidate)->inflowTargets.push_back(edge);
//                    }
//                }
//                // TODO verify weighting sums to one
//                
//                // clear for next cell
//                outflowCandidates.clear();
//                outflowCandidateSquareElevationDifferences.clear();
//                
//            }
//        }
//        
//        return std::unique_ptr<MaterialFlowGraph>(graph);
//    }
//    
//    void World::erode(float timestep){
//        // sediment flow, does this want to be first???
//        std::unique_ptr<MaterialFlowGraph> flowGraph = this->createFlowGraph(); // TODO don't create each phase, reuse the memory!
//        flowGraph->flowAll();
//        
//        // set sediment for each world cell
//        size_t index = 0;
//        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
//             cell != this->cells.end();
//             cell++, index++)
//        {
//            RockSegment sediment;
//            sediment.thickness = flowGraph->nodes[index].materialHeight;
//            sediment.density = 2700; // TODO calculate some actual density
//            
//            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
//            if (activePlateIndex >= 0){
//                cell->surfaceCells[activePlateIndex][0]->rock.sediment = sediment;
//            } else {
//                cell->strandedSediment = sediment;
//            }
//        }
//        
//        // bedrock phase
//        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
//             cell != this->cells.end();
//             cell++)
//        {
//            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
//            if (activePlateIndex >= 0) {
//                PlateCell* activePlateCell = cell->surfaceCells[activePlateIndex][0];
//                float activeElevation = activePlateCell->get_elevation();
//                if (activeElevation > this->attributes.sealevel - 300) {
//                    // move some stuff to lower elevation cells
//                    float erosionFactor = (activeElevation - 9260) / (4000);
//                    float erosionHeight = 0;
//                    if (erosionFactor > 0) {
//                        // add erosion to self as sediment
//                        erosionFactor = erosionFactor*erosionFactor*erosionFactor/400;
//                        // limit to .1
//                        if (erosionFactor > 0.5) {
//                            erosionFactor = 0.5;
//                        }
//                        erosionHeight = erosionFactor * timestep * (activeElevation - 9260);
//                        RockSegment erodedSegment = activePlateCell->erodeThickness(erosionHeight);
//                        activePlateCell->rock.sediment = combineSegments(activePlateCell->rock.sediment, erodedSegment);
//                    }
//                    
//                    float neighborCount = cell->get_vertex()->get_neighbors().size();
//                    for (std::vector<GridVertex*>::const_iterator neighborVertex = cell->get_vertex()->get_neighbors().begin();
//                         neighborVertex != cell->get_vertex()->get_neighbors().end();
//                         neighborVertex++)
//                    {
//                        
//                        int_fast32_t activeNeighborIndex = this->activePlateIndexForCell(&(this->cells[(*neighborVertex)->get_index()]));
//                        float neighborElevation = this->cells[(*neighborVertex)->get_index()].get_elevation(activeNeighborIndex);
//                        if (activeElevation > neighborElevation){
//                            erosionHeight = (activeElevation - neighborElevation) * erosionRate * timestep / neighborCount;
//                            
//                            RockSegment erodedSegment = activePlateCell->erodeThickness(erosionHeight);
//                            if (activeNeighborIndex >=0){
//                                this->cells[(*neighborVertex)->get_index()].surfaceCells[activeNeighborIndex][0]->rock.sediment = combineSegments(erodedSegment, this->cells[(*neighborVertex)->get_index()].surfaceCells[activeNeighborIndex][0]->rock.sediment);
//                            } else {
//                                cell->strandedSediment = combineSegments(cell->strandedSediment, erodedSegment);
//                            }//*/
//                        }
//                    }
//                }
//            }
//        }
//    }
    
    /*************** Plate Movement ***************/
    void World::movePlates(wb_float timestep){
        for (auto plateIt = this->plates.begin(); plateIt != this->plates.end(); plateIt++)
        {
            std::shared_ptr<Plate> plate = plateIt->second;
            plate->move(timestep);
        }
    }
    float World::randomPlateSpeed(){
        return randomSource->randomNormal(0.0095, 0.0038); // mean, stdev
    }
    
    /*************** Constructors ***************/
    World::World(Grid *theWorldGrid, std::shared_ptr<Random> random) : worldGrid(theWorldGrid), plates(10), randomSource(random), _nextPlateId(0){
        // set default rock column
        this->divergentOceanicColumn.root.density = 3200.0;
        this->divergentOceanicColumn.root.thickness = 84000.0;
        this->divergentOceanicColumn.oceanic.density = 2890.0;
        this->divergentOceanicColumn.oceanic.thickness = 6000;
        
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
        std::shared_ptr<Plate> firstPlate = std::make_shared<Plate>(this->nextPlateId(), theWorldGrid->verts_size());
        firstPlate->pole = this->randomSource->getRandomPointUnitSphere();
        firstPlate->angularSpeed = this->randomPlateSpeed();
        firstPlate->densityOffset = this->randomPlateDensityOffset();
        // add the initial cells to the plate
        for (uint32_t index = 0; index < theWorldGrid->verts_size(); index++) {
            std::shared_ptr<PlateCell> cell = std::make_shared<PlateCell>(&(theWorldGrid->get_vertices()[index]));
            
        }
        
        this->plates.insert({firstPlate->id,firstPlate});
        
    } // World(Grid, Random)
}